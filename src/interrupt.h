#ifndef INTERRUPT_H
#define INTERRUPT_H

#define interrupt_activate() __asm__ __volatile__("cpsie i")
#define interrupt_deactivate() __asm__ __volatile__("cpsid i")

#endif // INTERRUPT_H
