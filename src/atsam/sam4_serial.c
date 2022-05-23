// Hardware interface to SAM4 USART/UART
//
// Copyright (C) 2022  Desuuuu <contact@desuuuu.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h>           // uint32_t
#include "autoconf.h"         // CONFIG_MACH_SAM4S
#include "board/armcm_boot.h" // DECL_ARMCM_IRQ
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h"          // DECL_ENUMERATION
#include "gpio.h"             // uart_setup
#include "internal.h"         // gpio_peripheral
#include "sched.h"            // sched_shutdown

struct bus_info
{
    uint8_t id, is_usart;
    void *dev;
    uint32_t dev_id;
    IRQn_Type irqn;
    uint8_t rx_pin, tx_pin, func;
};

DECL_ENUMERATION_RANGE("uart_bus", "uart0", 0, 2);
DECL_ENUMERATION_RANGE("uart_bus", "usart0", 2, 2);
DECL_CONSTANT_STR("BUS_PINS_uart0", "[_],PA9,PA10");
#if CONFIG_MACH_SAM4S
DECL_CONSTANT_STR("BUS_PINS_uart1", "[_],PB2,PB3");
DECL_CONSTANT_STR("BUS_PINS_usart0", "[_],PA5,PA6");
#else
DECL_CONSTANT_STR("BUS_PINS_uart1", "[_],PA5,PA6");
DECL_CONSTANT_STR("BUS_PINS_usart0", "[_],PB0,PB1");
#endif
DECL_CONSTANT_STR("BUS_PINS_usart1", "[_],PA21,PA22");

static const struct bus_info bus_data[] = {
    { 0, 0, UART0, ID_UART0, UART0_IRQn,
      GPIO('A', 9), GPIO('A', 10), 'A' },
#if CONFIG_MACH_SAM4S
    { 1, 0, UART1, ID_UART1, UART1_IRQn,
      GPIO('B', 2), GPIO('B', 3), 'A' },
    { 2, 1, USART0, ID_USART0, USART0_IRQn,
      GPIO('A', 5), GPIO('A', 6), 'A' },
#else
    { 1, 0, UART1, ID_UART1, UART1_IRQn,
      GPIO('A', 5), GPIO('A', 6), 'C' },
    { 2, 1, USART0, ID_USART0, USART0_IRQn,
      GPIO('B', 0), GPIO('B', 1), 'C' },
#endif
    { 3, 1, USART1, ID_USART1, USART1_IRQn,
      GPIO('A', 21), GPIO('A', 22), 'A' },
};

void UART0_IRQHandler(void)
{
    uint32_t status = UART0->UART_SR;
    if (status & UART_SR_RXRDY)
        serial_rx_byte(0, UART0->UART_RHR);
    if (status & UART_SR_TXRDY)
    {
        uint8_t data;
        int ret = serial_get_tx_byte(0, &data);
        if (ret)
            UART0->UART_IDR = UART_IDR_TXRDY;
        else
            UART0->UART_THR = data;
    }
}
DECL_ARMCM_IRQ(UART0_IRQHandler, UART0_IRQn);

void UART1_IRQHandler(void)
{
    uint32_t status = UART1->UART_SR;
    if (status & UART_SR_RXRDY)
        serial_rx_byte(1, UART1->UART_RHR);
    if (status & UART_SR_TXRDY)
    {
        uint8_t data;
        int ret = serial_get_tx_byte(1, &data);
        if (ret)
            UART1->UART_IDR = UART_IDR_TXRDY;
        else
            UART1->UART_THR = data;
    }
}
DECL_ARMCM_IRQ(UART1_IRQHandler, UART1_IRQn);

void USART0_IRQHandler(void)
{
    uint32_t status = USART0->US_CSR;
    if (status & US_CSR_RXRDY)
        serial_rx_byte(2, USART0->US_RHR);
    if (status & US_CSR_TXRDY)
    {
        uint8_t data;
        int ret = serial_get_tx_byte(2, &data);
        if (ret)
            USART0->US_IDR = US_IDR_TXRDY;
        else
            USART0->US_THR = data;
    }
}
DECL_ARMCM_IRQ(USART0_IRQHandler, USART0_IRQn);

void USART1_IRQHandler(void)
{
    uint32_t status = USART1->US_CSR;
    if (status & US_CSR_RXRDY)
        serial_rx_byte(3, USART1->US_RHR);
    if (status & US_CSR_TXRDY)
    {
        uint8_t data;
        int ret = serial_get_tx_byte(3, &data);
        if (ret)
            USART1->US_IDR = US_IDR_TXRDY;
        else
            USART1->US_THR = data;
    }
}
DECL_ARMCM_IRQ(USART1_IRQHandler, USART1_IRQn);

struct uart_config
setup_dev_uart(const struct bus_info *bi, uint32_t baud, uint8_t *id
               , uint32_t priority)
{
    Uart *uart = bi->dev;

    gpio_peripheral(bi->rx_pin, bi->func, 1);
    gpio_peripheral(bi->tx_pin, bi->func, 0);

    // Reset uart
    enable_pclock(bi->dev_id);
    uart->UART_CR = (UART_CR_RSTRX | UART_CR_RSTTX
                     | UART_CR_RXDIS | UART_CR_TXDIS);
    uart->UART_IDR = 0xFFFFFFFF;

    // Enable uart
    uart->UART_MR = (UART_MR_PAR_NO | UART_MR_CHMODE_NORMAL);
    uart->UART_BRGR = get_pclock_frequency(bi->dev_id) / (16 * baud);
    uart->UART_IER = UART_IER_RXRDY;
    NVIC_SetPriority(bi->irqn, priority);
    NVIC_EnableIRQ(bi->irqn);
    uart->UART_CR = UART_CR_RXEN | UART_CR_TXEN;

    *id = bi->id;

    return (struct uart_config){ .dev=uart, .is_usart=0 };
}

struct uart_config
setup_dev_usart(const struct bus_info *bi, uint32_t baud, uint8_t *id
                , uint32_t priority)
{
    Usart *usart = bi->dev;

    gpio_peripheral(bi->rx_pin, bi->func, 1);
    gpio_peripheral(bi->tx_pin, bi->func, 0);

    // Reset usart
    enable_pclock(bi->dev_id);
    usart->US_CR = (US_CR_RSTRX | US_CR_RSTTX
                    | US_CR_RXDIS | US_CR_TXDIS);
    usart->US_IDR = 0xFFFFFFFF;

    // Enable usart
    usart->US_MR = (US_MR_CHRL_8_BIT | US_MR_NBSTOP_1_BIT | US_MR_PAR_NO
                    | US_MR_CHMODE_NORMAL);
    usart->US_BRGR = get_pclock_frequency(bi->dev_id) / (16 * baud);
    usart->US_IER = US_IER_RXRDY;
    NVIC_SetPriority(bi->irqn, priority);
    NVIC_EnableIRQ(bi->irqn);
    usart->US_CR = US_CR_RXEN | US_CR_TXEN;

    *id = bi->id;

    return (struct uart_config){ .dev=usart, .is_usart=1 };
}

struct uart_config
uart_setup(uint8_t bus, uint32_t baud, uint8_t *id, uint32_t priority)
{
    if (bus >= ARRAY_SIZE(bus_data))
        shutdown("Invalid UART config");

    if (bus_data[bus].is_usart) {
        return setup_dev_usart(&bus_data[bus], baud, id, priority);
    }

    return setup_dev_uart(&bus_data[bus], baud, id, priority);
}

void
uart_enable_tx_irq(struct uart_config config)
{
    if (config.is_usart) {
        Usart *usart = config.dev;
        usart->US_IER = US_IER_TXRDY;
    } else {
        Uart *uart = config.dev;
        uart->UART_IER = UART_IER_TXRDY;
    }
}
