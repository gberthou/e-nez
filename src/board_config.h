#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Set NUCLEO64 to run on nucleo-c071rb; comment out to run on e-nez
#define NUCLEO64

// Uncomment to duplicate left channel into right channel
//#define MONO_TO_STEREO

// Uncomment for the USB audio to stream PCM24 data to the host instead of PCM32. This
// assumes that the I2S device outputs 24b data as well (commented out = PCM32)
//#define PCM24

#endif // BOARD_CONFIG_H
