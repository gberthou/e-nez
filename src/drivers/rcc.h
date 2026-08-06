#ifndef DRIVERS_RCC_H
#define DRIVERS_RCC_H

#include <stdbool.h>

enum rcc_io_e
{
    RCC_PORTA = 0,
    RCC_PORTB = 1,
    RCC_PORTC = 2,
    RCC_PORTD = 3,
    RCC_PORTF = 5
};

enum rcc_clock_e
{
    RCC_CK_APB_TIM2 = 0,
    RCC_CK_APB_TIM3 = 1,
    RCC_CK_APB_RTC = 10,
    RCC_CK_APB_WWDG = 11,
    RCC_CK_APB_FDCAN1 = 12,
    RCC_CK_APB_USB = 13,
    RCC_CK_APB_SPI2 = 14,
    RCC_CK_APB_CRS = 16,
    RCC_CK_APB_USART2 = 17,
    RCC_CK_APB_USART3 = 18,
    RCC_CK_APB_USART4 = 19,
    RCC_CK_APB_I2C1 = 21,
    RCC_CK_APB_I2C2 = 22,
    RCC_CK_APB_DBG = 27,
    RCC_CK_APB_PWR = 28,

    RCC_CK_APB_SYSCFG = 32 + 0,
    RCC_CK_APB_TIM1 = 32 + 11,
    RCC_CK_APB_SPI1 = 32 + 12,
    RCC_CK_APB_USART1 = 32 + 14,
    RCC_CK_APB_TIM14 = 32 + 15,
    RCC_CK_APB_TIM15 = 32 + 16,
    RCC_CK_APB_TIM16 = 32 + 17,
    RCC_CK_APB_TIM17 = 32 + 18,
    RCC_CK_APB_ADC = 32 + 20
};

void rcc_select_sysclk_source(); // TODO: argument

void rcc_set_clock(enum rcc_clock_e clock, bool activate);

void rcc_io_set(enum rcc_io_e io, bool activate);

void rcc_hsiusb_set(bool activate);
void rcc_hsiusb_wait(void);

#endif // DRIVERS_RCC_H
