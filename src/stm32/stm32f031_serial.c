// STM32F031 serial
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h> // uint32_t
#include "autoconf.h" // CONFIG_*
#include "board/armcm_boot.h" // armcm_enable_irq
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h" // DECL_CONSTANT_STR
#include "gpio.h" // uart_setup
#include "internal.h" // gpio_peripheral

#if CONFIG_STM32_SERIAL_USART1
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA10,PA9");
  #define GPIO_Rx GPIO('A', 10)
  #define GPIO_Tx GPIO('A', 9)
  #define USARTx_FUNCTION GPIO_FUNCTION(1)
#elif CONFIG_STM32_SERIAL_USART2
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA3,PA2");
  #define GPIO_Rx GPIO('A', 3)
  #define GPIO_Tx GPIO('A', 2)
  #define USARTx_FUNCTION GPIO_FUNCTION(1)
#elif CONFIG_STM32_SERIAL_USART1_ALT_PB7_PB6
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PB7,PB6");
  #define GPIO_Rx GPIO('B', 7)
  #define GPIO_Tx GPIO('B', 6)
  #define USARTx_FUNCTION GPIO_FUNCTION(0)
#elif CONFIG_STM32_SERIAL_USART2_ALT_PA15_PA14
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA15,PA14");
  #define GPIO_Rx GPIO('A', 15)
  #define GPIO_Tx GPIO('A', 14)
  #define USARTx_FUNCTION GPIO_FUNCTION(1)
#endif

#define CR1_FLAGS (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE   \
                   | USART_CR1_RXNEIE)

void
USART1_IRQHandler(void)
{
    uint32_t sr = USART1->ISR;
    if (sr & (USART_ISR_RXNE | USART_ISR_ORE))
        serial_rx_byte(1, USART1->RDR);
    if (sr & USART_ISR_TXE && USART1->CR1 & USART_CR1_TXEIE) {
        uint8_t data;
        int ret = serial_get_tx_byte(1, &data);
        if (ret)
            USART1->CR1 = CR1_FLAGS;
        else
            USART1->TDR = data;
    }
}

struct uart_config
uart_setup(uint8_t bus, uint32_t baud, uint8_t *id, uint32_t priority)
{
    enable_pclock((uint32_t)USART1);

    uint32_t pclk = get_pclock_frequency((uint32_t)USART1);
    uint32_t div = DIV_ROUND_CLOSEST(pclk, baud);
    USART1->BRR = (((div / 16) << USART_BRR_DIV_MANTISSA_Pos)
                   | ((div % 16) << USART_BRR_DIV_FRACTION_Pos));
    USART1->CR1 = CR1_FLAGS;
    armcm_enable_irq(USART1_IRQHandler, USART1_IRQn, priority);

    gpio_peripheral(GPIO_Rx, USARTx_FUNCTION, 1);
    gpio_peripheral(GPIO_Tx, USARTx_FUNCTION, 0);

    *id = 0;

    return (struct uart_config){  };
}

void
uart_enable_tx_irq(struct uart_config config)
{
    USART1->CR1 = CR1_FLAGS | USART_CR1_TXEIE;
}
