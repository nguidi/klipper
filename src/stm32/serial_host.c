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

#if CONFIG_MACH_STM32F031
  #define CONSOLE_UART_BUS 0
#elif CONFIG_STM32_SERIAL_USART1
  #define CONSOLE_UART_BUS 0
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart1],PA10,PA9");
#elif CONFIG_STM32_SERIAL_USART2
  #define CONSOLE_UART_BUS 1
    DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart2],PA3,PA2");
#elif CONFIG_STM32_SERIAL_USART1_ALT_PB7_PB6
  #define CONSOLE_UART_BUS 2
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart1],PB7,PB6");
#elif CONFIG_STM32_SERIAL_USART2_ALT_PA15_PA14
  #define CONSOLE_UART_BUS 3
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart2],PA15,PA14");
#elif CONFIG_STM32_SERIAL_USART2_ALT_PD6_PD5
  #define CONSOLE_UART_BUS 3
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart2],PD6,PD5");
#elif CONFIG_STM32_SERIAL_USART3
  #define CONSOLE_UART_BUS 4
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart3],PB11,PB10");
#elif CONFIG_STM32_SERIAL_USART3_ALT_PD9_PD8
  #define CONSOLE_UART_BUS 5
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[usart3],PD9,PD8");
#elif CONFIG_STM32_SERIAL_UART4
  #define CONSOLE_UART_BUS 6
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "[uart4],PA1,PA0");
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
