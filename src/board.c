#include "board.h"
#include "board_config.h"
#include "board_usb.h"
#include "interrupt.h"
#include "drivers/gpio.h"
#include "drivers/nvic.h"
#include "drivers/rcc.h"
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
