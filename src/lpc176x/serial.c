// lpc176x serial port
//
// Copyright (C) 2018-2021  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h> // uint32_t
#include "board/armcm_boot.h" // DECL_ARMCM_IRQ
#include "board/irq.h" // irq_save
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h" // DECL_ENUMERATION
#include "gpio.h" // uart_setup
#include "internal.h" // gpio_peripheral
#include "sched.h" // sched_shutdown

struct bus_info {
    uint8_t id;
    LPC_UART_TypeDef *uart;
    uint32_t pclk;
    IRQn_Type irqn;
    uint8_t rx_pin, tx_pin, function;
};

DECL_ENUMERATION_RANGE("uart_bus", "uart0", 0, 4);
DECL_ENUMERATION("uart_bus", "uart1a", 4);
DECL_ENUMERATION("uart_bus", "uart2a", 5);
DECL_ENUMERATION("uart_bus", "uart3a", 6);
DECL_ENUMERATION("uart_bus", "uart3b", 7);
DECL_CONSTANT_STR("BUS_PINS_uart0", "[_],P0.3,P0.2");
DECL_CONSTANT_STR("BUS_PINS_uart1", "[_],P0.16,P0.15");
DECL_CONSTANT_STR("BUS_PINS_uart2", "[_],P0.11,P0.10");
DECL_CONSTANT_STR("BUS_PINS_uart3", "[_],P4.29,P4.28");
DECL_CONSTANT_STR("BUS_PINS_uart1a", "[uart1],P2.1,P2.0");
DECL_CONSTANT_STR("BUS_PINS_uart2a", "[uart2],P2.9,P2.8");
DECL_CONSTANT_STR("BUS_PINS_uart3a", "[uart3],P0.1,P0.0");
DECL_CONSTANT_STR("BUS_PINS_uart3b", "[uart3],P0.26,P0.25");

static const struct bus_info bus_data[] = {
    { 0, (LPC_UART_TypeDef*)LPC_UART0, PCLK_UART0, UART0_IRQn,
      GPIO(0, 3), GPIO(0, 2), 1 },
    { 1, (LPC_UART_TypeDef*)LPC_UART1, PCLK_UART1, UART1_IRQn,
      GPIO(0, 16), GPIO(0, 15), 1 },
    { 2, LPC_UART2, PCLK_UART2, UART2_IRQn,
      GPIO(0, 11), GPIO(0, 10), 1 },
    { 3, LPC_UART3, PCLK_UART3, UART3_IRQn,
      GPIO(4, 29), GPIO(4, 28), 3 },
    { 1, (LPC_UART_TypeDef*)LPC_UART1, PCLK_UART1, UART1_IRQn,
      GPIO(2, 1), GPIO(2, 0), 2 },
    { 2, LPC_UART2, PCLK_UART2, UART2_IRQn,
      GPIO(2, 9), GPIO(2, 8), 2 },
    { 3, LPC_UART3, PCLK_UART3, UART3_IRQn,
      GPIO(0, 1), GPIO(0, 0), 2 },
    { 3, LPC_UART3, PCLK_UART3, UART3_IRQn,
      GPIO(0, 26), GPIO(0, 25), 3 },
};

// Write tx bytes to the serial port
static void
kick_tx(uint8_t id, LPC_UART_TypeDef *uart)
{
    for (;;) {
        if (!(uart->LSR & (1<<5))) {
            // Output fifo full - enable tx irq
            uart->IER = 0x03;
            break;
        }
        uint8_t data;
        int ret = serial_get_tx_byte(id, &data);
        if (ret) {
            // No more data to send - disable tx irq
            uart->IER = 0x01;
            break;
        }
        uart->THR = data;
    }
}

void
UART0_IRQHandler(void)
{
    uint32_t iir = LPC_UART0->IIR, status = iir & 0x0f;
    if (status == 0x04)
        serial_rx_byte(0, LPC_UART0->RBR);
    else if (status == 0x02)
        kick_tx(0, (LPC_UART_TypeDef*)LPC_UART0);
}
DECL_ARMCM_IRQ(UART0_IRQHandler, UART0_IRQn);

void
UART1_IRQHandler(void)
{
    uint32_t iir = LPC_UART1->IIR, status = iir & 0x0f;
    if (status == 0x04)
        serial_rx_byte(1, LPC_UART1->RBR);
    else if (status == 0x02)
        kick_tx(1, (LPC_UART_TypeDef*)LPC_UART1);
}
DECL_ARMCM_IRQ(UART1_IRQHandler, UART1_IRQn);

void
UART2_IRQHandler(void)
{
    uint32_t iir = LPC_UART2->IIR, status = iir & 0x0f;
    if (status == 0x04)
        serial_rx_byte(2, LPC_UART2->RBR);
    else if (status == 0x02)
        kick_tx(2, LPC_UART2);
}
DECL_ARMCM_IRQ(UART2_IRQHandler, UART2_IRQn);

void
UART3_IRQHandler(void)
{
    uint32_t iir = LPC_UART3->IIR, status = iir & 0x0f;
    if (status == 0x04)
        serial_rx_byte(3, LPC_UART3->RBR);
    else if (status == 0x02)
        kick_tx(3, LPC_UART3);
}
DECL_ARMCM_IRQ(UART3_IRQHandler, UART3_IRQn);

struct uart_config
uart_setup(uint8_t bus, uint32_t baud, uint8_t *id, uint32_t priority)
{
    if (bus >= ARRAY_SIZE(bus_data))
        shutdown("Invalid UART config");
    const struct bus_info *bi = &bus_data[bus];
    LPC_UART_TypeDef *uart = bi->uart;

    // Setup baud
    enable_pclock(bi->pclk);
    uart->LCR = (1<<7); // set DLAB bit
    uint32_t pclk = get_pclock_frequency(bi->pclk);
    uint32_t div = pclk / (baud * 16);
    uart->DLL = div & 0xff;
    uart->DLM = (div >> 8) & 0xff;
    uart->FDR = 0x10;
    uart->LCR = 3; // 8N1 ; clear DLAB bit

    // Enable fifo
    uart->FCR = 0x01;

    // Setup pins
    gpio_peripheral(bi->rx_pin, bi->function, 0);
    gpio_peripheral(bi->tx_pin, bi->function, 0);

    // Enable receive irq
    NVIC_SetPriority(bi->irqn, priority);
    NVIC_EnableIRQ(bi->irqn);
    uart->IER = 0x01;

    *id = bi->id;

    return (struct uart_config){ .uart=uart, .id=bi->id };
}

void
uart_enable_tx_irq(struct uart_config config)
{
    LPC_UART_TypeDef *uart = config.uart;
    if (uart->LSR & (1<<5)) {
        irqstatus_t flag = irq_save();
        kick_tx(config.id, uart);
        irq_restore(flag);
    }
}
