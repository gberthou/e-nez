#include <string.h>

#include "dma.h"

struct __attribute__((packed)) dma_channel_t
{
    uint32_t ccr;
    uint32_t cndtr;
    uint32_t cpar;
    uint32_t cmar;
    uint32_t _reserved;
};

struct __attribute__((packed)) dma_t
{
    uint32_t isr;
    uint32_t ifcr;
    struct dma_channel_t channels[n_dma_channels]; // Don't map unsupported channels
};

static volatile struct dma_t * const dma = (volatile struct dma_t*) 0x40020000;

static constexpr size_t ifcr_cgif_local_offset = 0;
static constexpr size_t ifcr_cteif_local_offset = 3;

static constexpr uint32_t ccr_en = 0x1u;

// channel_index == dma_channel_to_index(channel)
static inline size_t ifcr_bitshift(size_t channel_index, size_t local_offset)
{
    return (channel_index << 2) + local_offset;
}

void dma_activate_channel(size_t channel, enum dma_transfer_type_e type,
    const struct dma_endpoint_t * dst, const struct dma_endpoint_t * src,
    size_t source_size_bytes, enum dma_priority_e priority, bool circular,
    struct dma_interrupt_t interrupts)
{
    const struct dma_endpoint_t * peripheral_endpoint;
    const struct dma_endpoint_t * memory_endpoint;
    if (type == DMA_TRANSFER_MEMORY_TO_PERIPHERAL)
    {
        peripheral_endpoint = dst;
        memory_endpoint = src;
    }
    else
    {
        peripheral_endpoint = src;
        memory_endpoint = dst;
    }

    uint8_t interrupts_u8;
    memcpy(&interrupts_u8, &interrupts, sizeof(interrupts_u8));

    auto const channel_index = dma_channel_to_index(channel);
    volatile struct dma_channel_t * const chan = &dma->channels[channel_index];

    // Clear (at least) EN and MEM2MEM
    chan->ccr = 0x0;

    // Clear all interrupts for that channel
    dma->ifcr = (1u << ifcr_bitshift(channel_index, ifcr_cgif_local_offset));

    const uint32_t msize = memory_endpoint->width;
    const uint32_t psize = peripheral_endpoint->width;
    chan->ccr = (interrupts_u8 << 1) // TCIE, HTIE, TEIE
              | (type == DMA_TRANSFER_MEMORY_TO_PERIPHERAL ? (1u << 4) : 0u) // DIR
              | (type != DMA_TRANSFER_MEMORY_TO_MEMORY && circular ? (1u << 5)
                    : 0u) // CIRC
              | (peripheral_endpoint->increment ? (1u << 6) : 0u) // PINC
              | (memory_endpoint->increment ? (1u << 7) : 0u) // MINC
              | (psize << 8) // PSIZE
              | (msize << 10) // MSIZE
              | (priority << 12) // PL
              | (type == DMA_TRANSFER_MEMORY_TO_MEMORY ? (1u << 14): 0u) // MEM2MEM
              ;
    chan->cpar = peripheral_endpoint->address;
    chan->cmar = memory_endpoint->address;
    chan->cndtr = source_size_bytes >> src->width;
}

void dma_start(size_t channel)
{
    auto const index = dma_channel_to_index(channel);

    // Clear TEIF
    dma->ifcr = (1u << ifcr_bitshift(index, ifcr_cteif_local_offset));

    dma->channels[index].ccr |= ccr_en;
}

void dma_stop(size_t channel)
{
    auto const index = dma_channel_to_index(channel);
    dma->channels[index].ccr &= ~ccr_en;
}
