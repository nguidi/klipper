// AVR serial port code.
//
// Copyright (C) 2016-2018  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h> // uint32_t
#include <avr/interrupt.h> // USART_RX_vect
#include "autoconf.h" // CONFIG_SERIAL_BAUD_U2X
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h" // DECL_ENUMERATION
#include "gpio.h" // uart_setup
#include "sched.h" // sched_shutdown

struct bus_info {
    uint8_t id;
    volatile uint8_t *ucsr_a, *ucsr_b, *ucsr_c;
    volatile uint16_t *ubrr;
    uint8_t u2x, ucsz_0, ucsz_1, rxen, txen, rxcie, udrie;
};

#if CONFIG_MACH_atmega168 || CONFIG_MACH_atmega328 || CONFIG_MACH_atmega328p

DECL_ENUMERATION("uart_bus", "usart0", 0);
DECL_CONSTANT_STR("BUS_PINS_usart0", "[_],PD0,PD1");

static const struct bus_info bus_data[] = {
    { 0, &UCSR0A, &UCSR0B, &UCSR0C, &UBRR0,
      U2X0, UCSZ00, UCSZ01, RXEN0, TXEN0, RXCIE0, UDRIE0 },
};

ISR(USART_RX_vect)
{
    serial_rx_byte(0, UDR0);
}

ISR(USART_UDRE_vect)
{
    uint8_t data;
    int ret = serial_get_tx_byte(0, &data);
    if (ret)
        UCSR0B &= ~(1 << UDRIE0);
    else
        UDR0 = data;
}

#elif CONFIG_MACH_at90usb1286 || CONFIG_MACH_at90usb646 \
      || CONFIG_MACH_atmega32u4

DECL_ENUMERATION("uart_bus", "usart1", 0);
DECL_CONSTANT_STR("BUS_PINS_usart1", "[_],PD2,PD3");

static const struct bus_info bus_data[] = {
    { 1, &UCSR1A, &UCSR1B, &UCSR1C, &UBRR1,
      U2X1, UCSZ10, UCSZ11, RXEN1, TXEN1, RXCIE1, UDRIE1 },
};

ISR(USART1_RX_vect)
{
    serial_rx_byte(1, UDR1);
}

ISR(USART1_UDRE_vect)
{
    uint8_t data;
    int ret = serial_get_tx_byte(1, &data);
    if (ret)
        UCSR1B &= ~(1 << UDRIE1);
    else
        UDR1 = data;
}

#else

DECL_ENUMERATION_RANGE("uart_bus", "usart0", 0, 2);

#if CONFIG_MACH_atmega1280 || CONFIG_MACH_atmega2560
DECL_CONSTANT_STR("BUS_PINS_usart0", "[_],PE0,PE1");
#else
DECL_CONSTANT_STR("BUS_PINS_usart0", "[_],PD0,PD1");
#endif

DECL_CONSTANT_STR("BUS_PINS_usart1", "[_],PD2,PD3");

ISR(USART0_RX_vect)
{
    serial_rx_byte(0, UDR0);
}

ISR(USART0_UDRE_vect)
{
    uint8_t data;
    int ret = serial_get_tx_byte(0, &data);
    if (ret)
        UCSR0B &= ~(1 << UDRIE0);
    else
        UDR0 = data;
}

ISR(USART1_RX_vect)
{
    serial_rx_byte(1, UDR1);
}

ISR(USART1_UDRE_vect)
{
    uint8_t data;
    int ret = serial_get_tx_byte(1, &data);
    if (ret)
        UCSR1B &= ~(1 << UDRIE1);
    else
        UDR1 = data;
}

#if CONFIG_MACH_atmega1280 || CONFIG_MACH_atmega2560

DECL_ENUMERATION_RANGE("uart_bus", "usart2", 2, 2);
DECL_CONSTANT_STR("BUS_PINS_usart2", "[_],PH0,PH1");
DECL_CONSTANT_STR("BUS_PINS_usart3", "[_],PJ0,PJ1");

ISR(USART2_RX_vect)
{
    serial_rx_byte(2, UDR2);
}

ISR(USART2_UDRE_vect)
{
    uint8_t data;
    int ret = serial_get_tx_byte(2, &data);
    if (ret)
        UCSR2B &= ~(1 << UDRIE2);
    else
        UDR2 = data;
}

ISR(USART3_RX_vect)
{
    serial_rx_byte(3, UDR3);
}

ISR(USART3_UDRE_vect)
{
    uint8_t data;
    int ret = serial_get_tx_byte(3, &data);
    if (ret)
        UCSR3B &= ~(1 << UDRIE3);
    else
        UDR3 = data;
}

#endif // CONFIG_MACH_atmega1280 || CONFIG_MACH_atmega2560

static const struct bus_info bus_data[] = {
    { 0, &UCSR0A, &UCSR0B, &UCSR0C, &UBRR0,
      U2X0, UCSZ00, UCSZ01, RXEN0, TXEN0, RXCIE0, UDRIE0 },
    { 1, &UCSR1A, &UCSR1B, &UCSR1C, &UBRR1,
      U2X1, UCSZ10, UCSZ11, RXEN1, TXEN1, RXCIE1, UDRIE1 },
  #if CONFIG_MACH_atmega1280 || CONFIG_MACH_atmega2560
    { 2, &UCSR2A, &UCSR2B, &UCSR2C, &UBRR2,
      U2X2, UCSZ20, UCSZ21, RXEN2, TXEN2, RXCIE2, UDRIE2 },
    { 3, &UCSR3A, &UCSR3B, &UCSR3C, &UBRR3,
      U2X3, UCSZ30, UCSZ31, RXEN3, TXEN3, RXCIE3, UDRIE3 },
  #endif
};

#endif

struct uart_config
uart_setup(uint8_t bus, uint32_t baud, uint8_t *id, uint32_t priority)
{
    if (bus >= ARRAY_SIZE(bus_data))
        shutdown("Invalid UART config");
    const struct bus_info *bi = &bus_data[bus];

    *(bi->ucsr_a) = CONFIG_SERIAL_BAUD_U2X ? (1 << bi->u2x) : 0;
    uint32_t cm = CONFIG_SERIAL_BAUD_U2X ? 8 : 16;
    *(bi->ubrr) = DIV_ROUND_CLOSEST(CONFIG_CLOCK_FREQ, cm * baud) - 1UL;
    *(bi->ucsr_c) = (1 << bi->ucsz_1) | (1 << bi->ucsz_0);
    *(bi->ucsr_b) = ((1 << bi->rxen) | (1 << bi->txen)
                     | (1 << bi->rxcie) | (1 << bi->udrie));

    *id = bi->id;

    return (struct uart_config){ .ucsr_b=bi->ucsr_b, .udrie=bi->udrie };
}

void
uart_enable_tx_irq(struct uart_config config)
{
    volatile uint8_t *ucsr_b = config.ucsr_b;
    *ucsr_b |= 1 << config.udrie;
}
