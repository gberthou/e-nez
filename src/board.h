#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

void board_init(void);
void board_usb_reset(uint32_t device_address);
void board_set_led(unsigned index, bool on);
void board_printformat(const char *s, ...);

bool board_audio_is_active(void);
volatile int32_t *board_audio_get_pcm_buffer(void);

#endif // BOARD_H
