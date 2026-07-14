#include <stddef.h>
#include <stdint.h>

#include "strformat.h"

// IDLE --> IDLE if not '%'
// IDLE --> FORMAT_OPEN if '%'
// OPEN --> IDLE if '%', 'b', 'd', 's' or 'x' (normal) or neither them nor [0-9] (error)
// OPEN --> FORMAT_QUANTITY if [1-9]
// QUANTITY --> IDLE if 'b' (normal) or neither 'b' nor [0-9] (error)
enum format_state_e
{
    FORMAT_STATE_IDLE,
    FORMAT_STATE_OPEN,
    FORMAT_STATE_QUANTITY
};

static inline void hex_digit(strformat_cb_t cb, uint32_t digit)
{
    digit &= 0xfu;
    if (digit < 10)
        cb('0' + digit);
    else
        cb ('a' - 10 + digit);
}

static void process_int32(strformat_cb_t cb, int32_t x)
{
    if (x == 0)
    {
        cb('0');
        return;
    }

    if (x < 0)
    {
        cb('-');
        x = -x;
    }

    // Optimize for memory, not speed: don't create a reverse buffer
    for (int32_t divider = 1000000000u; // Biggest power of 10 on 32b
        divider;
        divider /= 10)
    {
        const int32_t unit = x / divider;
        if (unit != 0)
            cb('0' + (unit % 10));
    }
}

static void process_raw_binary(strformat_cb_t cb, const uint8_t *ptr, size_t size)
{
    for (size_t size_processed = 0; size--; ++size_processed)
    {
        const uint8_t x = *ptr++;
        hex_digit(cb, x >> 4);
        hex_digit(cb, x);
        if (size)
        {
            if ((size_processed % 8) == 7)
            {
                cb('\r');
                cb('\n');
            }
            else
                cb(' ');
        }
    }
}

static inline enum format_state_e vstrformat_idle(
    strformat_cb_t cb, char c, va_list *pargs, size_t *pquantity)
{
    (void) pargs;
    (void) pquantity;

    if (c == '%')
        return FORMAT_STATE_OPEN;
    cb(c);
    return FORMAT_STATE_IDLE;
}

static inline enum format_state_e vstrformat_open(
    strformat_cb_t cb, char c, va_list *pargs, size_t *pquantity)
{
    if (c == '%') // Just print %
    {
        cb(c);
        return FORMAT_STATE_IDLE;
    }

    if (c == 'b') // Raw binary, pointer in first arg and size in second arg
    {
        const uint8_t *ptr = va_arg(*pargs, const uint8_t *);
        const size_t size = va_arg(*pargs, size_t);
        process_raw_binary(cb, ptr, size);
        return FORMAT_STATE_IDLE;
    }

    if (c == 'd') // Signed integer
    {
        int32_t x = va_arg(*pargs, int32_t);
        process_int32(cb, x);
        return FORMAT_STATE_IDLE;
    }

    if (c == 's') // String
    {
        const char *s = va_arg(*pargs, const char *);
        for (char inner_c = *s++; inner_c; inner_c = *s++)
            cb(inner_c);
        return FORMAT_STATE_IDLE;
    }

    if (c == 'x') // Unsigned hex; size adapts to first 1
    {
        uint32_t x = va_arg(*pargs, uint32_t);
        if (x == 0)
            cb('0');
        else
        {
            // Optimize for memory, not speed: don't create a reverse buffer
            // Shift for biggest power of 16 on 32b
            for (size_t shift = 28; ; shift -= 4)
            {
                const uint32_t unit = x >> shift;
                if (unit != 0)
                    hex_digit(cb, unit);

                if (shift == 0)
                    break;
            }
        }
        return FORMAT_STATE_IDLE;
    }

    if(c >= '1' && c <= '9')
    {
        *pquantity = c - '0';
        return FORMAT_STATE_QUANTITY;
    }

    // Error, just print the consumed % and the current character
    cb('%'); // Consumed in IDLE mode
    cb(c);
    return FORMAT_STATE_IDLE;
}

static inline enum format_state_e vstrformat_quantity(
    strformat_cb_t cb, char c, va_list *pargs, size_t *pquantity)
{
    if (c >= '0' && c <= '9')
    {
        *pquantity = (*pquantity * 10) + c - '0';
        return FORMAT_STATE_QUANTITY;
    }

    if (c == 'b') // Raw binary, pointer in first arg and size is read from pquantity
    {
        const uint8_t *ptr = va_arg(*pargs, const uint8_t *);
        process_raw_binary(cb, ptr, *pquantity);
        return FORMAT_STATE_IDLE;
    }

    // Error, just print the consumed %, quantity and current character
    cb('%');
    process_int32(cb, *pquantity);
    cb(c);
    return FORMAT_STATE_IDLE;
}

/* Supported formats:
 - %% displays %
 - %d pops arg. and displays it as int32_t
 - %s pops arg. and displays it as string
 - %x pops arg. as uint32_t and displays it as hex (auto size)
 - %b pops arg. as uint8_t* and pops arg. as size_t (N bytes)
 - %[0-9]+b pops arg. and displays it as N bytes in hex format (space-separated between
   bytes, newline-separated between sets of 8 bytes). Recommended on its own line
*/
void vstrformat(strformat_cb_t cb, const char *fmt, va_list args)
{
    enum format_state_e state = FORMAT_STATE_IDLE;
    size_t quantity = 0;
    for (char c = *fmt++; c; c = *fmt++)
    {
        switch (state)
        {
            case FORMAT_STATE_OPEN:
                state = vstrformat_open(cb, c, &args, &quantity);
                break;

            case FORMAT_STATE_QUANTITY:
                state = vstrformat_quantity(cb, c, &args, &quantity);
                break;

            default: // IDLE
                state = vstrformat_idle(cb, c, &args, &quantity);
                break;
        }
    }
}
