#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void board_init(void);
void board_usb_reset(uint32_t device_address);
void board_set_led(unsigned index, bool on);
void board_printformat(const char *s, ...);

bool board_audio_is_active(void);

void board_start_sampling(void);
void board_stop_sampling(void);

// Returns the computed buffer size in bytes
size_t board_on_audio_buffers_swapped(volatile void *new_buffer);

#endif // BOARD_H
