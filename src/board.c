#include "board.h"
#include "board_config.h"
#include "board_usb.h"
#include "board_usb_descriptors.h"
#include "board_usb_endpoints.h"
#include "interrupt.h"
#include "drivers/dma.h"
#include "drivers/dmamux.h"
#include "drivers/gpio.h"
#include "drivers/nvic.h"
#include "drivers/rcc.h"
#include "drivers/spi.h"
#include "drivers/usart.h"
#include "drivers/usb.h"
#include "drivers/usb_protocol.h"
#include "util/strformat.h"

#ifdef NUCLEO64
constexpr auto led0_port = GPIO_PORTA;
constexpr auto led1_port = GPIO_PORTC;
constexpr unsigned led0 = 5;
constexpr unsigned led1 = 9;
#else
constexpr auto led_port = GPIO_PORTD;
constexpr unsigned led0 = 0;
constexpr unsigned led1 = 1;
#endif // NUCLEO64

constexpr auto uart_port = GPIO_PORTA;
constexpr unsigned uart_tx = 2;
constexpr unsigned uart_rx = 3;
constexpr unsigned uart_usart_id = 2;

constexpr auto usb_port = GPIO_PORTA;
constexpr unsigned usbdm = 11;
constexpr unsigned usbdp = 12;
constexpr unsigned nvic_usb = 8;

constexpr auto i2s_port = GPIO_PORTA;
constexpr unsigned i2s_ws = 4;
constexpr unsigned i2s_bck = 5;
constexpr unsigned i2s_sd = 7;

constexpr size_t dma_i2s_extraction_chan = 1;

// USBSRAM doesn't support 16b accesses, so a 16b-addressable buffer must be
// allocated. This prevents a direct DMA copy from I2S to populate the next USB
// packet to be sent. To allow DMA for running freely while the USB handler populates
// each of the buffers involved in double-buffered isochronous packets, allocate two
// buffers in a continuous address range (i.e., allocate 2ms worth of samples)
static constexpr size_t i2s_double_buffer_length =
    config0_ep3_tx_size * 2 / sizeof(uint32_t);
static volatile uint32_t i2s_soft_buffer[i2s_double_buffer_length];
static const volatile uint32_t * const i2s_soft_buffer_end =
    &i2s_soft_buffer[i2s_double_buffer_length];
static const volatile uint32_t *i2s_soft_buffer_head = &i2s_soft_buffer[0];

static void uart_strformat(char c)
{
    usart_uart_putc(uart_usart_id, c);
}

static void usb_cdc_acm_strformat(char c)
{
    board_usb_cdc_acm_putc(c);
}

void board_kprintformat(const char *s, ...)
{
    va_list args;
    va_start(args, s);

    vstrformat(uart_strformat, s, args);

    va_end(args);
}

void board_printformat(const char *s, ...)
{
    va_list args;
    va_start(args, s);

    if (board_usb_cdc_acm_is_active())
        vstrformat(usb_cdc_acm_strformat, s, args);
    else
        vstrformat(uart_strformat, s, args);

    va_end(args);
}

void board_usb_reset(uint32_t device_address)
{
    board_usb_init_ep0();
    usb_configure_device(true, device_address);
}

static inline void board_init_usb(uint32_t device_address)
{
    board_kprintformat("USB init\r\n");

    rcc_set_clock(RCC_CK_APB_USB, true);

    // Assumes SYSCFG registers do not remap pins
    // STM32C071 datasheet Table 12, USB_DM/USB_DP are "additional functions"
    // RM0490 Section 8.3.1, it is recommended to set MODER to analog mode for additional
    // functions
    // RM0490 Section 8.3.2, although USB is not explicitly listed, configuring the
    // peripheral should take over the gpio function
    gpio_init(usb_port, usbdm, GPIO_MODE_ANALOG, GPIO_RESISTOR_NONE, GPIO_PIN_NA, 3);
    gpio_init(usb_port, usbdp, GPIO_MODE_ANALOG, GPIO_RESISTOR_NONE, GPIO_PIN_NA, 3);

    usb_reset();
    usb_start();

    // Interrupts
    constexpr struct usb_interrupt_mask_t mask = {
        .l1reqm = false,
        .esofm = false,
        .sofm = false,
        .rst_dconm = true,
        .suspm = true,
        .wkupm = true,
        .errm = false,
        .pmaovrm = true,
        .ctrm = true,
        .thr512m = false,
        .ddiscm = false
    };
    usb_init(false, mask, 0x0); // Device mode

    board_usb_reset(device_address);
}

static inline void board_init_dma(void)
{
    rcc_set_clock(RCC_CK_AHB_DMA_DMAMUX, true);

    // DMA channel 1: I2S to software staging buffer (16b copies)
    // Channel 1 will be respectively initialized and freed on audio stream start and
    // stop. So configure DMAMUX for channel 1 here only
    constexpr struct dmamux_event_config_t event_generation = {
        .n_requests = 1,
        .generation_active = false
    };
    dmamux_init(dma_i2s_extraction_chan, DMAMUX_INPUT_I2S_RX, event_generation,
        NULL, NULL);
}

void board_init(void)
{
    interrupt_deactivate();

    // General clocks
    rcc_hsiusb_set(true);
    rcc_hsiusb_wait(); // TODO: Maybe later, maybe with interrupt/timeout
    rcc_select_sysclk_source();

    // All ports are used
    rcc_io_set(RCC_PORTA, true);
    rcc_io_set(RCC_PORTB, true);
    rcc_io_set(RCC_PORTC, true);
    rcc_io_set(RCC_PORTD, true);
    rcc_io_set(RCC_PORTF, true);

    // LEDs
    gpio_init(led0_port, led0, GPIO_MODE_OUTPUT,
#ifdef NUCLEO64
        GPIO_RESISTOR_PULLDOWN,
#else
        GPIO_RESISTOR_PULLUP,
#endif // NUCLEO64
        GPIO_PIN_PUSHPULL, 0);
    gpio_init(led1_port, led1, GPIO_MODE_OUTPUT, GPIO_RESISTOR_PULLUP, GPIO_PIN_PUSHPULL, 0);
    // Light both LEDs on, just in case this function doesn't complete
    board_set_led(0, true);
    board_set_led(1, true);

    // UART
    rcc_set_clock(RCC_CK_APB_USART2, true);
    gpio_init(uart_port, uart_tx, GPIO_MODE_AF, GPIO_RESISTOR_NONE, GPIO_PIN_PUSHPULL, 0);
    gpio_init(uart_port, uart_rx, GPIO_MODE_AF, GPIO_RESISTOR_PULLUP, GPIO_PIN_NA, 0);
    gpio_select_alternate_function(uart_port, uart_tx, 1);
    gpio_select_alternate_function(uart_port, uart_rx, 1);

    usart_config_uart(uart_usart_id, 912600);

    // USB
    // Always listen on address 0 before getting a SET_ADDRESS command
    board_init_usb(0x0);
    constexpr struct usb_interrupt_mask_t interrupt_mask = {
        .l1reqm = true,
        .esofm = true,
        .sofm = true,
        .rst_dconm = true,
        .suspm = true,
        .wkupm = true,
        .errm = true,
        .pmaovrm = true,
        .ctrm = false, // This bit is read-only
        .thr512m = true,
        .ddiscm = true
    };
    usb_interrupt_ack(interrupt_mask);

    // DMA
    board_init_dma();

    // I2S
    // 27.7.4, regarding RCC/APB configuration
    // It is mandatory to keep the I2SxCLK frequency higher or equal to the APB clock
    // used by the SPI/I2S block
    rcc_set_clock(RCC_CK_APB_SPI1, true);
    gpio_init(i2s_port, i2s_ws, GPIO_MODE_AF, GPIO_RESISTOR_NONE, GPIO_PIN_PUSHPULL, 3);
    gpio_init(i2s_port, i2s_bck, GPIO_MODE_AF, GPIO_RESISTOR_NONE, GPIO_PIN_PUSHPULL, 3);
    gpio_init(i2s_port, i2s_sd, GPIO_MODE_AF, GPIO_RESISTOR_NONE, GPIO_PIN_NA, 3);
    gpio_select_alternate_function(i2s_port, i2s_ws, 0);
    gpio_select_alternate_function(i2s_port, i2s_bck, 0);
    gpio_select_alternate_function(i2s_port, i2s_sd, 0);

    auto const actual_sampling_frequency = i2s_init(I2S_CFG_SUP_RX, I2S_STD_PHILIPS,
        32, 24, 48000000, AUDIO_SAMPLING_FREQUENCY_HZ, false, false);
    board_kprintformat("I2S sampling at %d Hz\r\n", actual_sampling_frequency);

    board_set_led(0, false);
    board_set_led(1, false);

    // NVIC
    nvic_setup(nvic_usb, true);
    nvic_clear_all_pending();

    interrupt_activate();
}

void board_set_led(unsigned index, bool on)
{
    index &= 0x1;
#ifdef NUCLEO64
    // Only LD2 (index == 1) is reversed
    if (index == 0)
        gpio_out(led0_port, led0, on);
    else
        gpio_out(led1_port, led1, !on);
#else
    gpio_out(led_port, index == 0 ? led0 : led1, !on); // LEDs have reverse polarity
#endif
}

bool board_audio_is_active(void)
{
    return board_usb_audio_is_active();
}

void board_start_sampling(void)
{
    constexpr struct dma_interrupt_t interrupts = {
        .tci = false,
        .hti = false,
        .tei = false // Hardware stops channel on TEI interrupt, so software needs to
                     // set it back up. For the moment, the corresponding interrupt
                     // handler is not supported. TODO
    };

    // Dst = i2s_soft_buffer
    const struct dma_endpoint_t dst_dma_endpoint = {
        .address = (uint32_t) &i2s_soft_buffer[0], // Producer starts on the first half
        .width = DMA_WIDTH_16BIT,
        .increment = true
    };

    // Src = I2S (SPI1 DR)
    struct dma_endpoint_t src_dma_endpoint;
    src_dma_endpoint.address = (uint32_t) i2s_get_data_pointer();
    src_dma_endpoint.width = DMA_WIDTH_16BIT;
    src_dma_endpoint.increment = false;

    // Consumer starts on the second half
    i2s_soft_buffer_head = &i2s_soft_buffer[i2s_double_buffer_length / 2];

    dma_activate_channel(dma_i2s_extraction_chan, DMA_TRANSFER_PERIPHERAL_TO_MEMORY,
        &dst_dma_endpoint, &src_dma_endpoint, sizeof(i2s_soft_buffer),
        DMA_PRIO_VHIGH, true, interrupts);

    dma_start(dma_i2s_extraction_chan);
    i2s_flush_rx();
    i2s_configure_dma(false, true); // RXDMAEN=1
    i2s_start();
}

void board_stop_sampling(void)
{
    i2s_stop();
    i2s_configure_dma(false, false); // RXDMAEN=0
    dma_stop(dma_i2s_extraction_chan);
}

size_t board_on_audio_buffers_swapped(volatile void *new_buffer)
{
    volatile uint32_t *dst = new_buffer;
    static size_t n_swaps = 0;

    // Add extra frame for sampling frequencies that are not multiples of 1000Hz so
    // that the average bandwidth is met. For instance, 48kHz only has "short" packets.
    // However, 44.1kHz would require 9 buffers of 352 bytes followed by one buffer of
    // 360 bytes to make an average bandwidth of 352.8B/ms required for 2 channels of
    // 32b sampled at 44.1kHz.
    const size_t buffer_length = (config0_ep3_tx_size - 8) / sizeof(uint32_t)
        + (((n_swaps++) % (AUDIO_AMOUNT_SHORT_PACKETS + AUDIO_AMOUNT_LONG_PACKETS))
            >= AUDIO_AMOUNT_SHORT_PACKETS ? 2 : 0);
    const volatile uint32_t *src = i2s_soft_buffer_head;
    for(size_t i = buffer_length; i--;)
    {
        const uint32_t tmp = *src;
#ifdef PCM24
        // RM0490 Figure 299: 0x8eaa33 is received, via two consecutive DR reads, as:
        // * u16[0] = 0x8eaa
        // * u16[1] = 0x33xx
        // LE representation: [0xaa, 0x8e, 0xxx, 0x33]
        // u32 representation: 0x33xx8eaa
        // Rotation is required to reconstruct the initial value:
        // (tmp << 8) | (tmp >> 24) = 0xxx8eaa33
        // USB Audio formats 1.0 Section 2.2.6.1: data is padded with trailing zeros to
        // fill the frame => PCM 24b inside 32b frames requires the data to be
        // right-shifted by 8b, which also gets rid of the xx byte.
        // DMA does not convert data, therefore the hardware cannot be configured to
        // operate in an autonomous manner (e.g., using chained DMA channels to extract
        // I2S data and copy it back into the free USB packet data)
        const uint32_t value = (((tmp << 8) | (tmp >> 24)) << 8);
#else
        // Similarly, in PCM32 mode, both half-words are swapped due to the SPI hardware
        // controller reading the most significant half-word first and thus making the
        // DMA copy its value into the lowest 16b-aligned address of the corresponding
        // u32 data
        const uint32_t value = (tmp << 16) | (tmp >> 16);
#endif // PCM24
        *dst++ = value;

        if (++src == i2s_soft_buffer_end)
            src = &i2s_soft_buffer[0];
#ifdef MONO_TO_STEREO
        // Duplicate value onto the other channel. Discard the next source data
        *dst++ = value;
        --i;
        if (++src == i2s_soft_buffer_end)
            src = &i2s_soft_buffer[0];
#endif // MONO_TO_STEREO
    }
    i2s_soft_buffer_head = src;
    return buffer_length * sizeof(uint32_t);
}
