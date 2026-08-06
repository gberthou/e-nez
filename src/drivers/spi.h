#ifndef DRIVERS_SPI_H
#define DRIVERS_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum i2s_config_e: uint32_t
{
    I2S_CFG_SUB_TX = 0,
    I2S_CFG_SUB_RX = 1,
    I2S_CFG_SUP_TX = 2,
    I2S_CFG_SUP_RX = 3
};

enum i2s_format_e: uint32_t
{
    I2S_STD_PHILIPS = 0,
    I2S_STD_LJUSTIFIED = 1,
    I2S_STD_RJUSTIFIED = 2,
    I2S_STD_PCM_SHORT = 3,
    I2S_STD_PCM_LONG = 7
};

// Returns the (floored) actual sampling frequency
uint32_t i2s_init(
    // Generic
    enum i2s_config_e config,
    // Format
    enum i2s_format_e format, unsigned frame_length_bits, unsigned data_length_bits,
    // Protocol
    unsigned source_clock_frequency, unsigned sampling_frequency, bool clock_active_low,
    // Physical driver
    bool drive_mck);
void i2s_configure_dma(bool tx, bool activate);
void i2s_start(void);
void i2s_stop(void);

// Assumes I2S is stopped
void i2s_flush_rx(void);

volatile void *i2s_get_data_pointer(void);

#endif // DRIVERS_SPI_H
