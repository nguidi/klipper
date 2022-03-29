// Host serial communication initialization
//
// Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_SERIAL_BAUD
#include "board/serial_irq.h" // serial_prepare
#include "internal.h" // sercom_set_pin
#include "sched.h" // DECL_INIT
#include "command.h" // DECL_CONSTANT

#define RX_BUFFER_SIZE 192

DECL_CONSTANT("SERIAL_BAUD", CONFIG_SERIAL_BAUD);
DECL_CONSTANT("RECEIVE_WINDOW", RX_BUFFER_SIZE);

#define CONSOLE_UART_BUS 0

#define GPIO_Rx GPIO('A', 11)
#define GPIO_Tx GPIO('A', 10)

DECL_CONSTANT_STR("RESERVE_PINS_serial", "[sercom0],PA11,PA10");

static uint8_t serial_host_id;

void
serial_init(void)
{
    sercom_set_pin(CONSOLE_UART_BUS, SERCOM_RX_PIN, GPIO_Rx);
    sercom_set_pin(CONSOLE_UART_BUS, SERCOM_TX_PIN, GPIO_Tx);
    serial_host_id = serial_prepare(CONSOLE_UART_BUS, CONFIG_SERIAL_BAUD,
                                    RX_BUFFER_SIZE, 96, 0, NULL);
}
DECL_INIT(serial_init);

uint_fast8_t
console_sendf(const struct command_encoder *ce, va_list args)
{
    return serial_send_command(serial_host_id, ce, args);
}
