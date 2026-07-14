#include <stddef.h>
#include <stdint.h>

#include "board.h"

int main(void)
{
    board_init();

    board_printformat("hello\r\n");
    board_set_led(1, true);
    for (;;)
    {
        board_printformat("ping\r\n");

        board_set_led(0, false);
        for (volatile unsigned x = (1 << 22); x--;);
        board_set_led(0, true);
        for (volatile unsigned x = (1 << 22); x--;);
    }
    return 0;
}
