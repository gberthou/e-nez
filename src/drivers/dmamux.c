#include "dma.h"
#include "dmamux.h"

struct __attribute__((packed)) dmamux_t
{
    uint32_t cr[n_dma_channels];
    uint32_t _reserved0[32 - n_dma_channels];
    uint32_t csr;
    uint32_t cfr;
    uint32_t _reserved1[30];
    uint32_t rgcr[4];
    uint32_t _reserved2[12];
    uint32_t rgsr;
    uint32_t rgcfr;
};

static volatile struct dmamux_t * const dmamux = (volatile struct dmamux_t*) 0x40020800;

constexpr size_t n_dmamux_generator_channels = 4;

void dmamux_init(size_t dma_channel, enum dmamux_input_e input,
    struct dmamux_event_config_t event_generation,
    const struct dmamux_sync_t *sync, const struct dmamux_generator_t *generator)
{
    auto const dma_channel_index = dma_channel_to_index(dma_channel);
    volatile uint32_t * const cr = &dmamux->cr[dma_channel_index];

    // Clear (at least) SE and EGE
    *cr = 0x0u;

    const uint32_t nbreq = (event_generation.n_requests - 1) & 0x1fu;
    uint32_t cr_value = input // DMAREQ_ID
                      | (event_generation.generation_active ? (1u << 9) : 0u) // EGE
                      | (nbreq << 19)
                      ;
    if (sync)
    {
        const uint32_t spol = sync->polarity;
        const uint32_t sync_id = sync->signal;
        cr_value |= (sync->overrun_interrupt ? (1u << 8) : 0u) // SOIE
                 | (1u << 16) // SE
                 | (spol << 17)
                 | (sync_id << 24)
                 ;
    }
    *cr = cr_value;

    volatile uint32_t * const rgcr = &dmamux->rgcr[dma_channel_index];

    // Clear (at least) GE
    *rgcr = 0x0u;

    // Clear interrupts
    dmamux->rgcfr = (1u << dma_channel_index);

    // Only the first 4 channels are capable of event generation
    if (generator && dma_channel_index < n_dmamux_generator_channels)
    {
        const uint32_t gpol = generator->polarity;
        const uint32_t gnbreq = (generator->n_requests - 1) & 0x1fu;
        *rgcr = generator->signal // SIG_ID
              | (generator->overrun_interrupt ? (1u << 8) : 0u) // OIE
              | (1u << 16) // GE
              | (gpol << 17)
              | (gnbreq << 19)
              ;
    }
}
