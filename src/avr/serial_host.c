// Host serial communication initialization
//
// Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_SERIAL_BAUD
#include "board/serial_irq.h" // serial_prepare
#include "sched.h" // DECL_INIT
#include "command.h" // DECL_CONSTANT

#define RX_BUFFER_SIZE 192

DECL_CONSTANT("SERIAL_BAUD", CONFIG_SERIAL_BAUD);
DECL_CONSTANT("RECEIVE_WINDOW", RX_BUFFER_SIZE);

#if CONFIG_MACH_atmega168 || CONFIG_MACH_atmega328 || CONFIG_MACH_atmega328p
  #define CONSOLE_UART_BUS 0
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart0],PD0,PD1");
#elif CONFIG_MACH_at90usb1286 || CONFIG_MACH_at90usb646 \
      || CONFIG_MACH_atmega32u4
  #define CONSOLE_UART_BUS 0
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart1],PD2,PD3");
#elif CONFIG_MACH_atmega1280 || CONFIG_MACH_atmega2560
  #if CONFIG_SERIAL_PORT == 0
    #define CONSOLE_UART_BUS 0
    DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart0],PE0,PE1");
  #elif CONFIG_SERIAL_PORT == 1
    #define CONSOLE_UART_BUS 1
    DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart1],PD2,PD3");
  #elif CONFIG_SERIAL_PORT == 2
    #define CONSOLE_UART_BUS 2
    DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart2],PH0,PH1");
  #else
    #define CONSOLE_UART_BUS 3
    DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart3],PJ0,PJ1");
  #endif
#else
  #if CONFIG_SERIAL_PORT == 0
    #define CONSOLE_UART_BUS 0
    DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart0],PD0,PD1");
  #else
    #define CONSOLE_UART_BUS 1
    DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart1],PD2,PD3");
  #endif
#endif

static uint8_t serial_host_id;

void
serial_init(void)
{
    serial_host_id = serial_prepare(CONSOLE_UART_BUS, CONFIG_SERIAL_BAUD,
                                    RX_BUFFER_SIZE, 96, 0, NULL);
}
DECL_INIT(serial_init);

uint_fast8_t
console_sendf(const struct command_encoder *ce, va_list args)
{
    return serial_send_command(serial_host_id, ce, args);
}
