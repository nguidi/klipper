#ifndef __GENERIC_SERIAL_IRQ_H
#define __GENERIC_SERIAL_IRQ_H

#include <stdarg.h> // va_list
#include <stdint.h> // uint32_t
#include "command.h" // command_encoder

typedef int_fast8_t (*serial_receive_cb)(uint8_t id, uint8_t* data
                                         , uint_fast8_t len
                                         , uint_fast8_t *pop_count);

uint8_t serial_prepare(uint8_t bus, uint32_t baud
                       , uint8_t rx_buf, uint8_t tx_buf
                       , uint16_t rx_interval
                       , serial_receive_cb receive_cb);
uint_fast8_t serial_send(uint8_t id, uint8_t *data, uint8_t len);
uint_fast8_t serial_send_command(uint8_t id, const struct command_encoder *ce
                                 , va_list args);

void serial_rx_byte(uint8_t id, uint_fast8_t data);
int serial_get_tx_byte(uint8_t id, uint8_t *pdata);

#endif // serial_irq.h
