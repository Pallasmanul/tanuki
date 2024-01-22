#ifndef TANUKI_BOARD_H
#define TANUKI_BOARD_H

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/usart.h>

//-- use uart default gpio pin
#define LOG_USART USART1




void board_init(void);

#endif //TANUKI_BOARD_H
