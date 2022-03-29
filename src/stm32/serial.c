// STM32 serial
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h> // uint32_t
#include "autoconf.h" // CONFIG_*
#include "board/armcm_boot.h" // DECL_ARMCM_IRQ
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h" // DECL_ENUMERATION
#include "gpio.h" // uart_setup
#include "internal.h" // gpio_peripheral
#include "sched.h" // sched_shutdown

struct bus_info {
    uint8_t id;
    USART_TypeDef *usart;
    IRQn_Type irqn;
    uint8_t rx_pin, tx_pin, function;
};

#if CONFIG_MACH_STM32F0
  #define USART_FUNCTION(fn) GPIO_FUNCTION(fn)

  #define USART_SR(usart)  (usart)->ISR
  #define USART_RDR(usart) (usart)->RDR
  #define USART_TDR(usart) (usart)->TDR

  #define USART_SR_RXNE USART_ISR_RXNE
  #define USART_SR_ORE  USART_ISR_ORE
  #define USART_SR_TXE  USART_ISR_TXE

  #define USART_BRR_DIV_Mantissa_Pos USART_BRR_DIV_MANTISSA_Pos
  #define USART_BRR_DIV_Fraction_Pos USART_BRR_DIV_FRACTION_Pos
#elif CONFIG_MACH_STM32G0
  #define USART_FUNCTION(fn) GPIO_FUNCTION(fn)

  #define USART_SR(usart)  (usart)->ISR
  #define USART_RDR(usart) (usart)->RDR
  #define USART_TDR(usart) (usart)->TDR

  #define USART2_IRQn      USART2_LPUART2_IRQn
  #define USART_CR1_RXNEIE USART_CR1_RXNEIE_RXFNEIE
  #define USART_CR1_TXEIE  USART_CR1_TXEIE_TXFNFIE
  #define USART_SR_RXNE    USART_ISR_RXNE_RXFNE
  #define USART_SR_ORE     USART_ISR_ORE
  #define USART_SR_TXE     USART_ISR_TXE_TXFNF

  #define USART_BRR_DIV_Mantissa_Pos 4
  #define USART_BRR_DIV_Fraction_Pos 0
#elif CONFIG_MACH_STM32H7
  #define USART_FUNCTION(fn) GPIO_FUNCTION(7)

  #define USART_SR(usart)  (usart)->ISR
  #define USART_RDR(usart) (usart)->RDR
  #define USART_TDR(usart) (usart)->TDR

  #define USART_SR_RXNE    USART_ISR_RXNE_RXFNE
  #define USART_SR_ORE     USART_ISR_ORE
  #define USART_SR_TXE     USART_ISR_TXE_TXFNF

  #define USART_BRR_DIV_Mantissa_Pos USART_BRR_DIV_MANTISSA_Pos
  #define USART_BRR_DIV_Fraction_Pos USART_BRR_DIV_FRACTION_Pos
#else
  #define USART_FUNCTION(fn) GPIO_FUNCTION(7)

  #define USART_SR(usart)  (usart)->SR
  #define USART_RDR(usart) (usart)->DR
  #define USART_TDR(usart) (usart)->DR
#endif

DECL_ENUMERATION_RANGE("uart_bus", "usart1", 0, 2);
DECL_ENUMERATION("uart_bus", "usart1a", 2);
DECL_ENUMERATION("uart_bus", "usart2a", 3);
DECL_CONSTANT_STR("BUS_PINS_usart1", "[_],PA10,PA9");
DECL_CONSTANT_STR("BUS_PINS_usart2", "[_],PA3,PA2");
DECL_CONSTANT_STR("BUS_PINS_usart1a", "[usart1],PB7,PB6");

#if CONFIG_MACH_STM32F0 || CONFIG_MACH_STM32G0
  DECL_CONSTANT_STR("BUS_PINS_usart2a", "[usart2],PA15,PA14");
#else
  DECL_CONSTANT_STR("BUS_PINS_usart2a", "[usart2],PD6,PD5");

  #ifdef USART3
    DECL_ENUMERATION("uart_bus", "usart3", 4);
    DECL_ENUMERATION("uart_bus", "usart3a", 5);
    DECL_CONSTANT_STR("BUS_PINS_usart3", "[_],PB11,PB10");
    DECL_CONSTANT_STR("BUS_PINS_usart3a", "[usart3],PD9,PD8");

    #if CONFIG_MACH_STM32H7
        DECL_ENUMERATION("uart_bus", "uart4", 6);
        DECL_CONSTANT_STR("BUS_PINS_uart4", "[_],PA1,PA0");
    #endif
  #endif
#endif

static const struct bus_info bus_data[] = {
    { 1, USART1, USART1_IRQn, GPIO('A', 10), GPIO('A', 9), USART_FUNCTION(1) },
    { 2, USART2, USART2_IRQn, GPIO('A', 3), GPIO('A', 2), USART_FUNCTION(1) },
    { 1, USART1, USART1_IRQn, GPIO('B', 7), GPIO('B', 6), USART_FUNCTION(0) },
#if CONFIG_MACH_STM32F0 || CONFIG_MACH_STM32G0
    { 2, USART2, USART2_IRQn, GPIO('A', 15), GPIO('A', 14), USART_FUNCTION(1) },
#else
    { 2, USART2, USART2_IRQn, GPIO('D', 6), GPIO('D', 5), USART_FUNCTION(1) },
  #ifdef USART3
    { 3, USART3, USART3_IRQn, GPIO('B', 11), GPIO('B', 10), USART_FUNCTION(7) },
    { 3, USART3, USART3_IRQn, GPIO('D', 9), GPIO('D', 8), USART_FUNCTION(7) },
    #if CONFIG_MACH_STM32H7
    { 4, UART4, UART4_IRQn, GPIO('A', 1), GPIO('A', 0), GPIO_FUNCTION(8) },
    #endif
  #endif
#endif
};

#define CR1_FLAGS (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE   \
                   | USART_CR1_RXNEIE)

void
USART1_IRQHandler(void)
{
    uint32_t sr = USART_SR(USART1);
    if (sr & (USART_SR_RXNE | USART_SR_ORE))
        serial_rx_byte(1, USART_RDR(USART1));
    if (sr & USART_SR_TXE && USART1->CR1 & USART_CR1_TXEIE) {
        uint8_t data;
        int ret = serial_get_tx_byte(1, &data);
        if (ret)
            USART1->CR1 = CR1_FLAGS;
        else
            USART_TDR(USART1) = data;
    }
}
DECL_ARMCM_IRQ(USART1_IRQHandler, USART1_IRQn);

void
USART2_IRQHandler(void)
{
    uint32_t sr = USART_SR(USART2);
    if (sr & (USART_SR_RXNE | USART_SR_ORE))
        serial_rx_byte(2, USART_RDR(USART2));
    if (sr & USART_SR_TXE && USART2->CR1 & USART_CR1_TXEIE) {
        uint8_t data;
        int ret = serial_get_tx_byte(2, &data);
        if (ret)
            USART2->CR1 = CR1_FLAGS;
        else
            USART_TDR(USART2) = data;
    }
}
DECL_ARMCM_IRQ(USART2_IRQHandler, USART2_IRQn);

#if !CONFIG_MACH_STM32F0 && !CONFIG_MACH_STM32G0 && defined(USART3)

void
USART3_IRQHandler(void)
{
    uint32_t sr = USART_SR(USART3);
    if (sr & (USART_SR_RXNE | USART_SR_ORE))
        serial_rx_byte(3, USART_RDR(USART3));
    if (sr & USART_SR_TXE && USART3->CR1 & USART_CR1_TXEIE) {
        uint8_t data;
        int ret = serial_get_tx_byte(3, &data);
        if (ret)
            USART3->CR1 = CR1_FLAGS;
        else
            USART_TDR(USART3) = data;
    }
}
DECL_ARMCM_IRQ(USART3_IRQHandler, USART3_IRQn);

#endif // USART3

struct uart_config
uart_setup(uint8_t bus, uint32_t baud, uint8_t *id, uint32_t priority)
{
    if (bus >= ARRAY_SIZE(bus_data))
        shutdown("Invalid UART config");
    const struct bus_info *bi = &bus_data[bus];
    USART_TypeDef *usart = bi->usart;

    enable_pclock((uint32_t)usart);

    uint32_t pclk = get_pclock_frequency((uint32_t)usart);
    uint32_t div = DIV_ROUND_CLOSEST(pclk, baud);
    usart->BRR = (((div / 16) << USART_BRR_DIV_Mantissa_Pos)
                   | ((div % 16) << USART_BRR_DIV_Fraction_Pos));
    usart->CR1 = CR1_FLAGS;
    NVIC_SetPriority(bi->irqn, priority);
    NVIC_EnableIRQ(bi->irqn);

    gpio_peripheral(bi->rx_pin, bi->function, 1);
    gpio_peripheral(bi->tx_pin, bi->function, 0);

    *id = bi->id;

    return (struct uart_config){ .usart=usart };
}

void
uart_enable_tx_irq(struct uart_config config)
{
    USART_TypeDef *usart = config.usart;
    usart->CR1 = CR1_FLAGS | USART_CR1_TXEIE;
}
