#ifndef DRIVERS_DMAMUX_H
#define DRIVERS_DMAMUX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum dmamux_input_e
{
    DMAMUX_INPUT_GEN0 = 1,
    DMAMUX_INPUT_GEN1 = 2,
    DMAMUX_INPUT_GEN2 = 3,
    DMAMUX_INPUT_GEN3 = 4,
    DMAMUX_INPUT_I2S_RX = 16,
    DMAMUX_INPUT_I2S_TX = 17
};

enum dmamux_trigger_e: uint8_t
{
    DMAMUX_TRIGGER_EXTI0 = 0,
    DMAMUX_TRIGGER_EXTI1 = 1,
    DMAMUX_TRIGGER_EXTI2 = 2,
    DMAMUX_TRIGGER_EXTI3 = 3,
    DMAMUX_TRIGGER_EXTI4 = 4,
    DMAMUX_TRIGGER_EXTI5 = 5,
    DMAMUX_TRIGGER_EXTI6 = 6,
    DMAMUX_TRIGGER_EXTI7 = 7,
    DMAMUX_TRIGGER_EXTI8 = 8,
    DMAMUX_TRIGGER_EXTI9 = 9,
    DMAMUX_TRIGGER_EXTI10 = 10,
    DMAMUX_TRIGGER_EXTI11 = 11,
    DMAMUX_TRIGGER_EXTI12 = 12,
    DMAMUX_TRIGGER_EXTI13 = 13,
    DMAMUX_TRIGGER_EXTI14 = 14,
    DMAMUX_TRIGGER_EXTI15 = 15,
    DMAMUX_TRIGGER_EVT0 = 16,
    DMAMUX_TRIGGER_EVT1 = 17,
    DMAMUX_TRIGGER_EVT2 = 18,
    DMAMUX_TRIGGER_EVT3 = 19,
    DMAMUX_TRIGGER_TIM14 = 22
};

enum dmamux_generator_polarity_e: uint8_t
{
    DMAMUX_GEN_POL_RISING = 1,
    DMAMUX_GEN_POL_FALLING = 2,
    DMAMUX_GEN_POL_BOTH = 3
};

struct __attribute__((packed, aligned(2))) dmamux_event_config_t
{
    uint8_t n_requests; // >= 1
    bool generation_active;
};
static_assert(sizeof(struct dmamux_event_config_t) == sizeof(uint16_t));

struct dmamux_sync_t
{
    enum dmamux_trigger_e signal;
    enum dmamux_generator_polarity_e polarity;
    bool overrun_interrupt;
};

struct dmamux_generator_t
{
    enum dmamux_trigger_e signal;
    enum dmamux_generator_polarity_e polarity;
    uint8_t n_requests; // >= 1
    bool overrun_interrupt;
};

void dmamux_init(size_t dma_channel, enum dmamux_input_e input,
    struct dmamux_event_config_t event_generation,
    const struct dmamux_sync_t *sync, const struct dmamux_generator_t *generator);

#endif // DRIVERS_DMAMUX_H
