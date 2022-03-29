// samd21 serial port
//
// Copyright (C) 2018-2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h> // uint32_t
#include "autoconf.h" // CONFIG_MACH_*
#include "board/armcm_boot.h" // DECL_ARMCM_IRQ
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h" // DECL_ENUMERATION
#include "gpio.h" // uart_setup
#include "internal.h" // enable_pclock
#include "sched.h" // sched_shutdown

#if CONFIG_MACH_SAMD21
  #define SAMD_IRQn(sercom)  SERCOM ## sercom ## _IRQn
#elif CONFIG_MACH_SAMD51
  #define SAMD_x_IRQn(sercom, x)  SERCOM ## sercom ## _ ## x ## _IRQn
  #define SAMD_IRQn(sercom)  SAMD_x_IRQn(sercom, 0), SAMD_x_IRQn(sercom, 1),  \
      SAMD_x_IRQn(sercom, 2), SAMD_x_IRQn(sercom, 3)
#endif

struct bus_info {
    uint8_t id;
    Sercom *sercom;
#if CONFIG_MACH_SAMD21
    IRQn_Type irqn;
#elif CONFIG_MACH_SAMD51
    IRQn_Type irqn0, irqn1, irqn2, irqn3;
#endif
};

static const struct bus_info bus_data[] = {
    { 0, SERCOM0, SAMD_IRQn(0) },
    { 1, SERCOM1, SAMD_IRQn(1) },
    { 2, SERCOM2, SAMD_IRQn(2) },
    { 3, SERCOM3, SAMD_IRQn(3) },
#ifdef SERCOM4
    { 4, SERCOM4, SAMD_IRQn(4) },
    { 5, SERCOM5, SAMD_IRQn(5) },
  #ifdef SERCOM6
    { 6, SERCOM6, SAMD_IRQn(6) },
    { 7, SERCOM7, SAMD_IRQn(7) },
  #endif
#endif
};

void
SERCOM0_IRQHandler(void)
{
    uint32_t status = SERCOM0->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(0, SERCOM0->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(0, &data);
        if (ret)
            SERCOM0->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM0->USART.DATA.reg = data;
    }
}
#if CONFIG_MACH_SAMD21
DECL_ARMCM_IRQ(SERCOM0_IRQHandler, SERCOM0_IRQn);
#elif CONFIG_MACH_SAMD51
DECL_ARMCM_IRQ(SERCOM0_IRQHandler, SERCOM0_0_IRQn);
DECL_ARMCM_IRQ(SERCOM0_IRQHandler, SERCOM0_1_IRQn);
DECL_ARMCM_IRQ(SERCOM0_IRQHandler, SERCOM0_2_IRQn);
DECL_ARMCM_IRQ(SERCOM0_IRQHandler, SERCOM0_3_IRQn);
#endif

void
SERCOM1_IRQHandler(void)
{
    uint32_t status = SERCOM1->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(1, SERCOM1->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(1, &data);
        if (ret)
            SERCOM1->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM1->USART.DATA.reg = data;
    }
}
#if CONFIG_MACH_SAMD21
DECL_ARMCM_IRQ(SERCOM1_IRQHandler, SERCOM1_IRQn);
#elif CONFIG_MACH_SAMD51
DECL_ARMCM_IRQ(SERCOM1_IRQHandler, SERCOM1_0_IRQn);
DECL_ARMCM_IRQ(SERCOM1_IRQHandler, SERCOM1_1_IRQn);
DECL_ARMCM_IRQ(SERCOM1_IRQHandler, SERCOM1_2_IRQn);
DECL_ARMCM_IRQ(SERCOM1_IRQHandler, SERCOM1_3_IRQn);
#endif

void
SERCOM2_IRQHandler(void)
{
    uint32_t status = SERCOM2->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(2, SERCOM2->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(2, &data);
        if (ret)
            SERCOM2->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM2->USART.DATA.reg = data;
    }
}
#if CONFIG_MACH_SAMD21
DECL_ARMCM_IRQ(SERCOM2_IRQHandler, SERCOM2_IRQn);
#elif CONFIG_MACH_SAMD51
DECL_ARMCM_IRQ(SERCOM2_IRQHandler, SERCOM2_0_IRQn);
DECL_ARMCM_IRQ(SERCOM2_IRQHandler, SERCOM2_1_IRQn);
DECL_ARMCM_IRQ(SERCOM2_IRQHandler, SERCOM2_2_IRQn);
DECL_ARMCM_IRQ(SERCOM2_IRQHandler, SERCOM2_3_IRQn);
#endif

void
SERCOM3_IRQHandler(void)
{
    uint32_t status = SERCOM3->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(3, SERCOM3->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(3, &data);
        if (ret)
            SERCOM3->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM3->USART.DATA.reg = data;
    }
}
#if CONFIG_MACH_SAMD21
DECL_ARMCM_IRQ(SERCOM3_IRQHandler, SERCOM3_IRQn);
#elif CONFIG_MACH_SAMD51
DECL_ARMCM_IRQ(SERCOM3_IRQHandler, SERCOM3_0_IRQn);
DECL_ARMCM_IRQ(SERCOM3_IRQHandler, SERCOM3_1_IRQn);
DECL_ARMCM_IRQ(SERCOM3_IRQHandler, SERCOM3_2_IRQn);
DECL_ARMCM_IRQ(SERCOM3_IRQHandler, SERCOM3_3_IRQn);
#endif

#ifdef SERCOM4

void
SERCOM4_IRQHandler(void)
{
    uint32_t status = SERCOM4->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(4, SERCOM4->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(4, &data);
        if (ret)
            SERCOM4->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM4->USART.DATA.reg = data;
    }
}
#if CONFIG_MACH_SAMD21
DECL_ARMCM_IRQ(SERCOM4_IRQHandler, SERCOM4_IRQn);
#elif CONFIG_MACH_SAMD51
DECL_ARMCM_IRQ(SERCOM4_IRQHandler, SERCOM4_0_IRQn);
DECL_ARMCM_IRQ(SERCOM4_IRQHandler, SERCOM4_1_IRQn);
DECL_ARMCM_IRQ(SERCOM4_IRQHandler, SERCOM4_2_IRQn);
DECL_ARMCM_IRQ(SERCOM4_IRQHandler, SERCOM4_3_IRQn);
#endif

void
SERCOM5_IRQHandler(void)
{
    uint32_t status = SERCOM5->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(5, SERCOM5->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(5, &data);
        if (ret)
            SERCOM5->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM5->USART.DATA.reg = data;
    }
}
#if CONFIG_MACH_SAMD21
DECL_ARMCM_IRQ(SERCOM5_IRQHandler, SERCOM5_IRQn);
#elif CONFIG_MACH_SAMD51
DECL_ARMCM_IRQ(SERCOM5_IRQHandler, SERCOM5_0_IRQn);
DECL_ARMCM_IRQ(SERCOM5_IRQHandler, SERCOM5_1_IRQn);
DECL_ARMCM_IRQ(SERCOM5_IRQHandler, SERCOM5_2_IRQn);
DECL_ARMCM_IRQ(SERCOM5_IRQHandler, SERCOM5_3_IRQn);
#endif

#ifdef SERCOM6

void
SERCOM6_IRQHandler(void)
{
    uint32_t status = SERCOM6->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(6, SERCOM6->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(6, &data);
        if (ret)
            SERCOM6->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM6->USART.DATA.reg = data;
    }
}
DECL_ARMCM_IRQ(SERCOM6_IRQHandler, SERCOM6_0_IRQn);
DECL_ARMCM_IRQ(SERCOM6_IRQHandler, SERCOM6_1_IRQn);
DECL_ARMCM_IRQ(SERCOM6_IRQHandler, SERCOM6_2_IRQn);
DECL_ARMCM_IRQ(SERCOM6_IRQHandler, SERCOM6_3_IRQn);

void
SERCOM7_IRQHandler(void)
{
    uint32_t status = SERCOM7->USART.INTFLAG.reg;
    if (status & SERCOM_USART_INTFLAG_RXC)
        serial_rx_byte(7, SERCOM7->USART.DATA.reg);
    if (status & SERCOM_USART_INTFLAG_DRE) {
        uint8_t data;
        int ret = serial_get_tx_byte(7, &data);
        if (ret)
            SERCOM7->USART.INTENCLR.reg = SERCOM_USART_INTENSET_DRE;
        else
            SERCOM7->USART.DATA.reg = data;
    }
}
DECL_ARMCM_IRQ(SERCOM7_IRQHandler, SERCOM7_0_IRQn);
DECL_ARMCM_IRQ(SERCOM7_IRQHandler, SERCOM7_1_IRQn);
DECL_ARMCM_IRQ(SERCOM7_IRQHandler, SERCOM7_2_IRQn);
DECL_ARMCM_IRQ(SERCOM7_IRQHandler, SERCOM7_3_IRQn);

#endif // SERCOM6

#endif // SERCOM4

const struct bus_info *
lookup_bus_info(Sercom *sercom)
{
    const struct bus_info *bi = bus_data;
    for (; ; bi++) {
        if (bi >= &bus_data[ARRAY_SIZE(bus_data)])
            shutdown("Invalid UART config");
        if (bi->sercom == sercom)
            return bi;
    }
}

struct uart_config
uart_setup(uint8_t bus, uint32_t baud, uint8_t *id, uint32_t priority)
{
    Sercom *sercom = sercom_enable_pclock(bus);
    const struct bus_info *bi = lookup_bus_info(sercom);

    SercomUsart* usart = &sercom->USART;
    uint32_t pinout = sercom_usart_pins(bus);

    // Configure USART
    usart->CTRLA.reg = 0;
    uint32_t areg = (SERCOM_USART_CTRLA_MODE(1)
                     | SERCOM_USART_CTRLA_DORD
                     | SERCOM_USART_CTRLA_SAMPR(1)
                     | pinout);
    usart->CTRLA.reg = areg;
    usart->CTRLB.reg = SERCOM_USART_CTRLB_RXEN | SERCOM_USART_CTRLB_TXEN;
    uint32_t baud8 = sercom_get_pclock_frequency(bus) / (2 * baud);
    usart->BAUD.reg = (SERCOM_USART_BAUD_FRAC_BAUD(baud8 / 8)
                       | SERCOM_USART_BAUD_FRAC_FP(baud8 % 8));
    // enable irqs
    usart->INTENSET.reg = SERCOM_USART_INTENSET_RXC;
    usart->CTRLA.reg = areg | SERCOM_USART_CTRLA_ENABLE;
#if CONFIG_MACH_SAMD21
    NVIC_SetPriority(bi->irqn, priority);
    NVIC_EnableIRQ(bi->irqn);
#elif CONFIG_MACH_SAMD51
    NVIC_SetPriority(bi->irqn0, priority);
    NVIC_EnableIRQ(bi->irqn0);
    NVIC_SetPriority(bi->irqn1, priority);
    NVIC_EnableIRQ(bi->irqn1);
    NVIC_SetPriority(bi->irqn2, priority);
    NVIC_EnableIRQ(bi->irqn2);
    NVIC_SetPriority(bi->irqn3, priority);
    NVIC_EnableIRQ(bi->irqn3);
#endif

    *id = bi->id;

    return (struct uart_config){ .usart=usart };
}

void
uart_enable_tx_irq(struct uart_config config)
{
    SercomUsart* usart = config.usart;
    usart->INTENSET.reg = SERCOM_USART_INTENSET_DRE;
}
