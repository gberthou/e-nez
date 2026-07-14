#include <stddef.h>

#include "gpio.h"

struct __attribute__((packed, aligned(4))) gpio_t
{
    uint32_t moder;
    uint32_t otyper;
    uint32_t ospeedr;
    uint32_t pupdr;
    uint32_t idr;
    uint32_t odr;
    uint32_t bsrr;
    uint32_t lckr;
    uint32_t afrl;
    uint32_t afrh;
    uint32_t brr;
};

static inline volatile struct gpio_t *get_port_ptr(enum gpio_port_e port)
{
    const size_t address = 0x50000000 + 0x400 * port;
    return (volatile struct gpio_t*) address;
}

void gpio_init(enum gpio_port_e port, unsigned index, enum gpio_mode_e mode,
    enum gpio_resistor_e resistor, enum gpio_pin_e pin, uint8_t speed)
{
    // RM0490 Table 38
    auto const gpio = get_port_ptr(port);
    index &= 0xfu;

    if (mode == GPIO_MODE_INPUT)
    {
        pin = GPIO_PIN_NA;
        speed = 0;
    }
    else if (mode == GPIO_MODE_ANALOG)
    {
        resistor = GPIO_RESISTOR_NONE;
        pin = GPIO_PIN_NA;
        speed = 0;
    }
    else
        speed &= 0x3u;

    // MODER / OSPEEDR / PUPDR
    {
        auto const shift = 2 * index;
        auto const mask = (0x3u << shift);

        auto moder = gpio->moder;
        moder &= ~mask;
        moder |= (mode << shift);
        gpio->moder = moder;

        auto ospeedr = gpio->ospeedr;
        ospeedr &= ~mask;
        ospeedr |= (speed << shift);
        gpio->ospeedr = ospeedr;

        auto pupdr = gpio->pupdr;
        pupdr &= ~mask;
        pupdr |= (resistor << shift);
        gpio->pupdr = pupdr;
    }

    // OTYPER
    {
        auto const mask = (0x1u << index);

        auto otyper = gpio->otyper;
        if (pin == GPIO_PIN_OPENDRAIN)
            otyper |= mask;
        else
            otyper &= ~mask;
        gpio->otyper = otyper;
    }
}

void gpio_select_alternate_function(enum gpio_port_e port, unsigned index, uint32_t function)
{
    index &= 0xfu;
    function &= 0xfu;

    const size_t bitshift = (4 * (index & 0x7u));
    auto const gpio = get_port_ptr(port);
    volatile uint32_t * const pafr = (index > 7 ? &gpio->afrh : &gpio->afrl);
    auto afr = *pafr;
    // Clear AFSEL[index]
    afr &= ~(0xfu << bitshift);
    afr |= (function << bitshift);
    *pafr = afr;
}

void gpio_out(enum gpio_port_e port, unsigned index, bool on)
{
    auto const gpio = get_port_ptr(port);
    auto const mask = (0x1u << index);

    index &= 0xfu;
    if (on)
        gpio->odr |= mask;
    else
        gpio->odr &= ~mask;
}
