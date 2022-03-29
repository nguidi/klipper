// Generic interrupt based serial uart helper code
//
// Copyright (C) 2016-2018  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h> // memmove
#include "autoconf.h" // CONFIG_HAVE_GPIO_UART
#include "board/io.h" // readb
#include "board/irq.h" // irq_save
#include "board/misc.h" // timer_from_us
#include "board/pgm.h" // READP
#include "board/gpio.h" // uart_config
#include "basecmd.h" // alloc_chunk
#include "command.h" // shutdown
#include "sched.h" // sched_wake_task
#include "serial_irq.h" // serial_receive_cb

struct serial_data {
    struct uart_config uart_config;
    struct task_wake wake;
#if CONFIG_HAVE_GPIO_UART
    struct timer timer;
    serial_receive_cb receive_cb;
    uint32_t rx_interval;
    uint8_t ovfl;
#endif
    uint8_t rx_buf_len, tx_buf_len;
    uint8_t rpos, tpos, tmax;
    uint8_t buf[];
};

static struct serial_data *serial_data[GPIO_UART_MAX_ID];

// Rx interrupt - store read data
void
serial_rx_byte(uint8_t id, uint_fast8_t data)
{
    if (id >= ARRAY_SIZE(serial_data) || !serial_data[id])
        return;
    struct serial_data *sd = serial_data[id];

#if CONFIG_HAVE_GPIO_UART
    if (sd->receive_cb) {
        uint8_t left = sd->rx_buf_len - sd->rpos;
        if (left < 8)
            sched_wake_task(&sd->wake);
        if (left == 0) {
            ++sd->ovfl;
            return;
        }
    } else {
#else
    {
#endif
        if (data == MESSAGE_SYNC)
            sched_wake_task(&sd->wake);
        if (sd->rpos >= sd->rx_buf_len)
            // Serial overflow - ignore it as crc error will force retransmit
            return;
    }
    sd->buf[sd->rpos++] = data;
}

// Tx interrupt - get next byte to transmit
int
serial_get_tx_byte(uint8_t id, uint8_t *pdata)
{
    if (id >= ARRAY_SIZE(serial_data) || !serial_data[id])
        return -1;
    struct serial_data *sd = serial_data[id];

    if (sd->tpos >= sd->tmax)
        return -1;
    *pdata = sd->buf[sd->rx_buf_len + sd->tpos++];
    return 0;
}

#if CONFIG_HAVE_GPIO_UART
static uint_fast8_t
serial_rx_event(struct timer *timer)
{
    struct serial_data *sd = container_of(timer, struct serial_data, timer);
    sched_wake_task(&sd->wake);
    sd->timer.waketime += sd->rx_interval;
    return SF_RESCHEDULE;
}
#endif

static void
serial_pop_input(struct serial_data *sd, uint_fast8_t len)
{
    uint_fast8_t copied = 0;
    for (;;) {
        uint_fast8_t rpos = readb(&sd->rpos);
        uint_fast8_t needcopy = rpos - len;
        if (needcopy) {
            memmove(sd->buf + copied, sd->buf + copied + len,
                    needcopy - copied);
            copied = needcopy;
            sched_wake_task(&sd->wake);
        }
        irqstatus_t flag = irq_save();
        if (rpos != readb(&sd->rpos)) {
            // Raced with irq handler - retry
            irq_restore(flag);
            continue;
        }
        sd->rpos = needcopy;
        irq_restore(flag);
        break;
    }
}

#if CONFIG_HAVE_GPIO_UART
static void
serial_process_rx(struct serial_data *sd, uint8_t id)
{
    uint_fast8_t rpos = readb(&sd->rpos), ovfl = readb(&sd->ovfl), pop_count;
    int_fast8_t ret;
    if (rpos) {
        ret = (sd->receive_cb)(id, sd->buf, rpos, &pop_count);
        if (!ret)
            sched_wake_task(&sd->wake);
        else if (pop_count)
            serial_pop_input(sd, pop_count);
    }
    if (ovfl) {
        ret = (sd->receive_cb)(id, NULL, 0, &pop_count);
        if (!ret)
            sched_wake_task(&sd->wake);
        else
            writeb(&sd->ovfl, 0);
    }
}
#endif

static void
console_process_rx(struct serial_data *sd)
{
    uint_fast8_t rpos = readb(&sd->rpos), pop_count;
    int_fast8_t ret = command_find_block(sd->buf, rpos, &pop_count);
    if (ret > 0)
        command_dispatch(sd->buf, pop_count);
    if (ret) {
        serial_pop_input(sd, pop_count);
        if (ret > 0)
            command_send_ack();
    }
}

// Process incoming data
void
serial_task(void)
{
    for (uint_fast8_t id = 0; id < ARRAY_SIZE(serial_data); ++id) {
        struct serial_data *sd = serial_data[id];
        if (sd && sched_check_wake(&sd->wake)) {
#if CONFIG_HAVE_GPIO_UART
            if (sd->receive_cb)
                serial_process_rx(sd, id);
            else
#endif
                console_process_rx(sd);
        }
    }
}
DECL_TASK(serial_task);

uint8_t
serial_prepare(uint8_t bus, uint32_t baud, uint8_t rx_buf, uint8_t tx_buf
               , uint16_t rx_interval, serial_receive_cb receive_cb)
{
    struct serial_data *sd = alloc_chunk(sizeof(*sd) + rx_buf + tx_buf);

    uint8_t id;
    sd->uart_config = uart_setup(bus, baud, &id, receive_cb ? 1 : 0);
    if (id >= ARRAY_SIZE(serial_data) || serial_data[id])
        shutdown("Invalid UART config");

    sd->rx_buf_len = rx_buf;
    sd->tx_buf_len = tx_buf;

#if CONFIG_HAVE_GPIO_UART
    sd->receive_cb = receive_cb;
    if (receive_cb && rx_interval > 0) {
        sd->rx_interval = timer_from_us(rx_interval * 1000L);
        sd->timer.func = serial_rx_event;
        sd->timer.waketime = timer_read_time() + sd->rx_interval;
        sched_add_timer(&sd->timer);
    }
#endif

    serial_data[id] = sd;

    return id;
}

uint_fast8_t
serial_send(uint8_t id, uint8_t *data, uint8_t len)
{
    if (id >= ARRAY_SIZE(serial_data) || !serial_data[id])
        return 1;
    struct serial_data *sd = serial_data[id];

    // Verify space for message
    uint_fast8_t tpos = readb(&sd->tpos), tmax = readb(&sd->tmax);
    if (tpos >= tmax) {
        tpos = tmax = 0;
        writeb(&sd->tmax, 0);
        writeb(&sd->tpos, 0);
    }
    if (tmax + len > sd->tx_buf_len) {
        if (tmax + len - tpos > sd->tx_buf_len)
            // Not enough space for message
            return len > sd->tx_buf_len;
        // Disable TX irq and move buffer
        writeb(&sd->tmax, 0);
        tpos = readb(&sd->tpos);
        tmax -= tpos;
        memmove(sd->buf + sd->rx_buf_len, sd->buf + sd->rx_buf_len + tpos,
                tmax);
        writeb(&sd->tpos, 0);
        writeb(&sd->tmax, tmax);
        uart_enable_tx_irq(sd->uart_config);
    }

    memcpy(sd->buf + sd->rx_buf_len + tmax, data, len);

    // Start message transmit
    writeb(&sd->tmax, tmax + len);
    uart_enable_tx_irq(sd->uart_config);
    return 1;
}

uint_fast8_t
serial_send_command(uint8_t id, const struct command_encoder *ce, va_list args)
{
    if (id >= ARRAY_SIZE(serial_data) || !serial_data[id])
        return 1;
    struct serial_data *sd = serial_data[id];

    // Verify space for message
    uint_fast8_t tpos = readb(&sd->tpos), tmax = readb(&sd->tmax);
    if (tpos >= tmax) {
        tpos = tmax = 0;
        writeb(&sd->tmax, 0);
        writeb(&sd->tpos, 0);
    }
    uint_fast8_t max_size = READP(ce->max_size);
    if (tmax + max_size > sd->tx_buf_len) {
        if (tmax + max_size - tpos > sd->tx_buf_len)
            // Not enough space for message
            return max_size > sd->tx_buf_len;
        // Disable TX irq and move buffer
        writeb(&sd->tmax, 0);
        tpos = readb(&sd->tpos);
        tmax -= tpos;
        memmove(sd->buf + sd->rx_buf_len, sd->buf + sd->rx_buf_len + tpos,
                tmax);
        writeb(&sd->tpos, 0);
        writeb(&sd->tmax, tmax);
        uart_enable_tx_irq(sd->uart_config);
    }

    // Generate message
    uint8_t *buf = sd->buf + sd->rx_buf_len + tmax;
    uint_fast8_t msglen = command_encode_and_frame(buf, ce, args);

    // Start message transmit
    writeb(&sd->tmax, tmax + msglen);
    uart_enable_tx_irq(sd->uart_config);
    return 1;
}
