#ifndef DRIVERS_GPIO_H
#define DRIVERS_GPIO_H

#include <stdint.h>
#include <stdbool.h>

enum gpio_port_e
{
    GPIO_PORTA = 0,
    GPIO_PORTB = 1,
    GPIO_PORTC = 2,
    GPIO_PORTD = 3,
    GPIO_PORTF = 5
};

enum gpio_mode_e: uint8_t
{
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT = 1,
    GPIO_MODE_AF = 2,
    GPIO_MODE_ANALOG = 3
};

enum gpio_resistor_e: uint8_t
{
    GPIO_RESISTOR_NONE = 0,
    GPIO_RESISTOR_PULLUP = 1,
    GPIO_RESISTOR_PULLDOWN = 2
};

enum gpio_pin_e: uint8_t
{
    GPIO_PIN_NA = 0,
    GPIO_PIN_PUSHPULL = 0,
    GPIO_PIN_OPENDRAIN = 1
};

void gpio_init(enum gpio_port_e port, unsigned index, enum gpio_mode_e mode,
    enum gpio_resistor_e resistor, enum gpio_pin_e pin, uint8_t speed);
void gpio_select_alternate_function(enum gpio_port_e port, unsigned index,
    uint32_t function);

void gpio_out(enum gpio_port_e port, unsigned index, bool on);

#endif // DRIVERS_GPIO_H
