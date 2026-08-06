#ifndef DRIVERS_DMA_H
#define DRIVERS_DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dma.h"

enum dma_transfer_type_e
{
    DMA_TRANSFER_MEMORY_TO_MEMORY,
    DMA_TRANSFER_MEMORY_TO_PERIPHERAL,
    DMA_TRANSFER_PERIPHERAL_TO_MEMORY,
    DMA_TRANSFER_PERIPHERAL_TO_PERIPHERAL
};

enum dma_priority_e: uint32_t
{
    DMA_PRIO_LOW = 0,
    DMA_PRIO_MEDIUM = 1,
    DMA_PRIO_HIGH = 2,
    DMA_PRIO_VHIGH = 3
};

struct dma_interrupt_t
{
    bool tci: 1;
    bool hti: 1;
    bool tei: 1;
};
static_assert(sizeof(struct dma_interrupt_t) == sizeof(uint8_t));

struct dma_endpoint_t
{
    uint32_t address;
    enum
    {
        DMA_WIDTH_8BIT = 0,
        DMA_WIDTH_16BIT = 1,
        DMA_WIDTH_32BIT = 2
    } width: 2;
    bool increment: 1;
};

constexpr size_t n_dma_channels = 5; // STM32C071 specific as per RM0490 Table 43

static inline size_t dma_channel_to_index(size_t channel)
{
    return (channel - 1) % n_dma_channels;
}

void dma_activate_channel(size_t channel, enum dma_transfer_type_e type,
    const struct dma_endpoint_t * dst, const struct dma_endpoint_t * src,
    size_t source_size_bytes, enum dma_priority_e priority, bool circular,
    struct dma_interrupt_t interrupts);
void dma_start(size_t channel);
void dma_stop(size_t channel);

#endif // DRIVERS_DMA_H
