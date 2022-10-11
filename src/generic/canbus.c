// Wrapper functions connecting canserial.c to low-level can hardware
//
// Copyright (C) 2022  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_CANBUS_FREQUENCY
#include "canbus.h" // canbus_send
#include "canserial.h" // canserial_send
#include "command.h" // DECL_CONSTANT

DECL_CONSTANT("CANBUS_FREQUENCY", CONFIG_CANBUS_FREQUENCY);

int
canserial_send(struct canbus_msg *msg)
{
    return canbus_send(msg);
}

void
canserial_set_filter(uint32_t id)
{
    canbus_set_filter(id);
}

void
canbus_notify_tx(void)
{
    canserial_notify_tx();
}

void
canbus_tx_task(void)
{
    if (!sched_check_wake(&CanData.tx_wake))
        return;
    uint32_t id = CanData.assigned_id;
    if (!id) {
        CanData.transmit_pos = CanData.transmit_max = 0;
        return;
    }
    struct canbus_msg msg;
    msg.id = id + 1;
    uint32_t tpos = CanData.transmit_pos, tmax = CanData.transmit_max;
    for (;;) {
        int avail = tmax - tpos, now = avail > 8 ? 8 : avail;
        if (avail <= 0)
            break;
        msg.dlc = now;
        memcpy(msg.data, &CanData.transmit_buf[tpos], now);
        int ret = canbus_send(&msg);
        if (ret <= 0)
            break;
        tpos += now;
    }
    CanData.transmit_pos = tpos;
}
<<<<<<< HEAD
DECL_TASK(canbus_tx_task);

// Encode and transmit a "response" message
uint_fast8_t
console_sendf(const struct command_encoder *ce, va_list args)
{
    // Verify space for message
    uint32_t tpos = CanData.transmit_pos, tmax = CanData.transmit_max;
    if (tpos >= tmax)
        CanData.transmit_pos = CanData.transmit_max = tpos = tmax = 0;
    uint32_t max_size = ce->max_size;
    if (tmax + max_size > sizeof(CanData.transmit_buf)) {
        if (tmax + max_size - tpos > sizeof(CanData.transmit_buf))
            // Not enough space for message
            return max_size > sizeof(transmit_buf);
        // Move buffer
        tmax -= tpos;
        memmove(&CanData.transmit_buf[0], &CanData.transmit_buf[tpos], tmax);
        CanData.transmit_pos = tpos = 0;
        CanData.transmit_max = tmax;
    }

    // Generate message
    uint32_t msglen = command_encode_and_frame(&CanData.transmit_buf[tmax]
                                               , ce, args);

    // Start message transmit
    CanData.transmit_max = tmax + msglen;
    canbus_notify_tx();
    return 1;
}


/****************************************************************
 * CAN "admin" command handling
 ****************************************************************/

// Available commands and responses
#define CANBUS_CMD_QUERY_UNASSIGNED 0x00
#define CANBUS_CMD_SET_KLIPPER_NODEID 0x01
#define CANBUS_CMD_REQUEST_BOOTLOADER 0x02
#define CANBUS_RESP_NEED_NODEID 0x20

// Helper to verify a UUID in a command matches this chip's UUID
static int
can_check_uuid(struct canbus_msg *msg)
{
    return (msg->dlc >= 7
            && memcmp(&msg->data[1], CanData.uuid, sizeof(CanData.uuid)) == 0);
}

// Helpers to encode/decode a CAN identifier to a 1-byte "nodeid"
static int
can_get_nodeid(void)
{
    if (!CanData.assigned_id)
        return 0;
    return (CanData.assigned_id - 0x100) >> 1;
}
static uint32_t
can_decode_nodeid(int nodeid)
{
    return (nodeid << 1) + 0x100;
}

static void
can_process_query_unassigned(struct canbus_msg *msg)
{
    if (CanData.assigned_id)
        return;
    struct canbus_msg send;
    send.id = CANBUS_ID_ADMIN_RESP;
    send.dlc = 8;
    send.data[0] = CANBUS_RESP_NEED_NODEID;
    memcpy(&send.data[1], CanData.uuid, sizeof(CanData.uuid));
    send.data[7] = CANBUS_CMD_SET_KLIPPER_NODEID;
    // Send with retry
    for (;;) {
        int ret = canbus_send(&send);
        if (ret >= 0)
            return;
    }
}

static void
can_id_conflict(void)
{
    CanData.assigned_id = 0;
    canbus_set_filter(CanData.assigned_id);
    shutdown("Another CAN node assigned this ID");
}

static void
can_process_set_klipper_nodeid(struct canbus_msg *msg)
{
    if (msg->dlc < 8)
        return;
    uint32_t newid = can_decode_nodeid(msg->data[7]);
    if (can_check_uuid(msg)) {
        if (newid != CanData.assigned_id) {
            CanData.assigned_id = newid;
            canbus_set_filter(CanData.assigned_id);
        }
    } else if (newid == CanData.assigned_id) {
        can_id_conflict();
    }
}

static void
can_process_request_bootloader(struct canbus_msg *msg)
{
    if (!can_check_uuid(msg))
        return;
    try_request_canboot();
}

// Handle an "admin" command
static void
can_process_admin(struct canbus_msg *msg)
{
    if (!msg->dlc)
        return;
    switch (msg->data[0]) {
    case CANBUS_CMD_QUERY_UNASSIGNED:
        can_process_query_unassigned(msg);
        break;
    case CANBUS_CMD_SET_KLIPPER_NODEID:
        can_process_set_klipper_nodeid(msg);
        break;
    case CANBUS_CMD_REQUEST_BOOTLOADER:
        can_process_request_bootloader(msg);
        break;
    }
}


/****************************************************************
 * CAN packet reading
 ****************************************************************/

static void
canbus_notify_rx(void)
{
    sched_wake_task(&CanData.rx_wake);
}

DECL_CONSTANT("RECEIVE_WINDOW", ARRAY_SIZE(CanData.receive_buf));

// Handle incoming data (called from IRQ handler)
void
canbus_process_data(struct canbus_msg *msg)
{
    canserial_process_data(msg);
}
=======
>>>>>>> 25110253ec39a887f1807beaa5c0a92b62057e37
