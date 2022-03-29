// rp2040 serial
//
// Copyright (C) 2021  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h> // uint32_t
#include "board/armcm_boot.h" // DECL_ARMCM_IRQ
#include "board/irq.h" // irq_save
#include "board/serial_irq.h" // serial_rx_byte
#include "hardware/structs/resets.h" // RESETS_RESET_UART0_BITS
#include "hardware/structs/uart.h" // UART0_BASE
#include "command.h" // DECL_ENUMERATION
#include "gpio.h" // uart_setup
#include "internal.h" // gpio_peripheral
#include "sched.h" // sched_shutdown

struct bus_info {
    uint8_t id;
    uart_hw_t *uart;
    uint32_t reset_bit;
    IRQn_Type irqn;
    uint8_t rx_pin, tx_pin;
};

DECL_ENUMERATION_RANGE("uart_bus", "uart0", 0, 2);
DECL_ENUMERATION("uart_bus", "uart0a", 2);
DECL_ENUMERATION("uart_bus", "uart0b", 3);
DECL_ENUMERATION("uart_bus", "uart1a", 4);
DECL_CONSTANT_STR("BUS_PINS_uart0", "[_],gpio1,gpio0");
DECL_CONSTANT_STR("BUS_PINS_uart1", "[_],gpio5,gpio4");
DECL_CONSTANT_STR("BUS_PINS_uart0a", "[uart0],gpio13,gpio12");
DECL_CONSTANT_STR("BUS_PINS_uart0b", "[uart0],gpio17,gpio16");
DECL_CONSTANT_STR("BUS_PINS_uart1a", "[uart1],gpio9,gpio8");

static const struct bus_info bus_data[] = {
    { 0, uart0_hw, RESETS_RESET_UART0_BITS, UART0_IRQ_IRQn, 1, 0 },
    { 1, uart1_hw, RESETS_RESET_UART1_BITS, UART1_IRQ_IRQn, 5, 4 },
    { 0, uart0_hw, RESETS_RESET_UART0_BITS, UART0_IRQ_IRQn, 13, 12 },
    { 0, uart0_hw, RESETS_RESET_UART0_BITS, UART0_IRQ_IRQn, 17, 16 },
    { 1, uart1_hw, RESETS_RESET_UART1_BITS, UART1_IRQ_IRQn, 9, 8 },
};

// Write tx bytes to the serial port
static void
kick_tx(uint8_t id, uart_hw_t *uart)
{
    for (;;) {
        if (uart->fr & UART_UARTFR_TXFF_BITS) {
            // Output fifo full - enable tx irq
            uart->imsc = (UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS
                           | UART_UARTIMSC_TXIM_BITS);
            break;
        }
        uint8_t data;
        int ret = serial_get_tx_byte(id, &data);
        if (ret) {
            // No more data to send - disable tx irq
            uart->imsc = UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS;
            break;
        }
        uart->dr = data;
    }
}

void
UART0_IRQHandler(void)
{
    uint32_t mis = uart0_hw->mis;
    if (mis & (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS)) {
        do {
            serial_rx_byte(0, uart0_hw->dr);
        } while (!(uart0_hw->fr & UART_UARTFR_RXFE_BITS));
    } else if (mis & UART_UARTMIS_TXMIS_BITS) {
        kick_tx(0, uart0_hw);
    }
}
DECL_ARMCM_IRQ(UART0_IRQHandler, UART0_IRQ_IRQn);

void
UART1_IRQHandler(void)
{
    uint32_t mis = uart1_hw->mis;
    if (mis & (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS)) {
        do {
            serial_rx_byte(1, uart1_hw->dr);
        } while (!(uart1_hw->fr & UART_UARTFR_RXFE_BITS));
    } else if (mis & UART_UARTMIS_TXMIS_BITS) {
        kick_tx(1, uart1_hw);
    }
}
DECL_ARMCM_IRQ(UART1_IRQHandler, UART1_IRQ_IRQn);

struct uart_config
uart_setup(uint8_t bus, uint32_t baud, uint8_t *id, uint32_t priority)
{
    if (bus >= ARRAY_SIZE(bus_data))
        shutdown("Invalid UART config");
    const struct bus_info *bi = &bus_data[bus];
    uart_hw_t *uart = bi->uart;

    enable_pclock(bi->reset_bit);

    // Setup baud
    uint32_t pclk = get_pclock_frequency(bi->reset_bit);
    uint32_t div = DIV_ROUND_CLOSEST(pclk * 4, baud);
    uart->ibrd = div >> 6;
    uart->fbrd = div & 0x3f;

    // Enable fifo, set 8N1
    uart->lcr_h = UART_UARTLCR_H_FEN_BITS | UART_UARTLCR_H_WLEN_BITS;
    uart->ifls = 0;
    uart->cr = (UART_UARTCR_RXE_BITS | UART_UARTCR_TXE_BITS
                 | UART_UARTCR_UARTEN_BITS);

    // Setup pins
    gpio_peripheral(bi->rx_pin, 2, 1);
    gpio_peripheral(bi->tx_pin, 2, 0);

    // Enable receive irq
    NVIC_SetPriority(bi->irqn, priority);
    NVIC_EnableIRQ(bi->irqn);
    uart->imsc = UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS;

    *id = bi->id;

    return (struct uart_config){ .uart=uart, .id=bi->id };
}

void
uart_enable_tx_irq(struct uart_config config)
{
    uart_hw_t *uart = config.uart;
    if (!(uart->fr & UART_UARTFR_TXFF_BITS)) {
        irqstatus_t flag = irq_save();
        kick_tx(config.id, uart);
        irq_restore(flag);
    }
}
