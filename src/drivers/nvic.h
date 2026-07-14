#ifndef DRIVERS_NVIC_H
#define DRIVERS_NVIC_H

#include <stdbool.h>

void nvic_setup(unsigned index, bool activate);
void nvic_ack(unsigned index);
void nvic_clear_all_pending(void);

#endif // DRIVERS_NVIC_H
