#include "spi.h"

struct __attribute__((packed)) spi_t
{
    uint32_t cr1;
    uint32_t cr2;
    uint32_t sr;
    uint32_t dr;
    uint32_t crcpr;
    uint32_t rxcrcr;
    uint32_t txcrcr;
    uint32_t i2scfgr;
    uint32_t i2spr;
};

static volatile struct spi_t * const spi1 = (volatile struct spi_t*) 0x40013000;
//static volatile struct spi_t * const spi2 = (volatile struct spi_t*) 0x40003800;

uint32_t i2s_init(enum i2s_config_e config, enum i2s_format_e format,
    unsigned frame_length_bits, unsigned data_length_bits,
    unsigned source_clock_frequency, unsigned sampling_frequency, bool clock_active_low,
    bool drive_mck)
{
    uint32_t chlen = 1; // 32b
    uint32_t datlen = 0; // 16b
    if (data_length_bits <= 16)
        chlen = (frame_length_bits > 16 ? 1 : 0);
    else if (data_length_bits > 24)
        datlen = 2; // 32b
    else
        datlen = 1; // 24b

    const bool is_i2s_format = (format < I2S_STD_PCM_SHORT);
    uint32_t divider = 4;
    uint32_t actual_frequency = 0u;
    {
        const uint32_t nbits = (drive_mck ? 7u : 4u + chlen)
            + (is_i2s_format ? 1u : 0u);
        const uint32_t ratio = source_clock_frequency / sampling_frequency
            + (1u << (nbits - 1)); // Ceil
        divider = (ratio >> nbits); // divider = 2 * I2SDIV + ODD
        // Since I2SDIV > 1, then divider >= 4
        if (divider < 4u)
            divider = 4u;
        // I2SDIV is on 8b, so divider is on 9b
        else if (divider > 0x1ffu)
            divider = 0x1ffu;
        actual_frequency = (source_clock_frequency >> nbits) / divider;
    }

    spi1->cr1 = 0x0; // Not used in I2S mode
    spi1->cr2 = 0x0;
    spi1->i2scfgr = chlen
                  | (datlen << 1)
                  | (clock_active_low ? (1u << 3) : 0u) // CKPOL
                  | (format << 4) // I2SSTD + PCMSYNC
                  | (config << 8) // I2SCFG
                  | (1u << 11) // I2SMOD
                  ;
    spi1->i2spr = (divider >> 1) // I2SDIV
                | ((divider & 0x1u) << 8) // ODD
                | (drive_mck ? (1u << 9) : 0u) // MCKOE
                ;

    return actual_frequency;
}

void i2s_configure_dma(bool tx, bool activate)
{
    const uint32_t mask = (1u << (tx ? 1 : 0)); // RXDMAEN or TXDMAEN
    if (activate)
        spi1->cr2 |= mask;
    else
        spi1->cr2 &= ~mask;
}

void i2s_start(void)
{
    spi1->i2scfgr |= (1u << 10); // I2SE
}

void i2s_stop(void)
{
    spi1->i2scfgr &= ~(1u << 10); // I2SE
}

void i2s_flush_rx(void)
{
    // FIFOs can only hold two 16b half-words
    // Sample the RXNE bit
    for (size_t i = 2; i-- && (spi1->sr & 0x1u);)
    {
        const volatile uint16_t x = spi1->dr;
        (void) x;
    }
}

volatile void *i2s_get_data_pointer(void)
{
    return &spi1->dr;
}
