#include <stddef.h>
#include <stdint.h>

#include "board.h"

int main(void)
{
    int32_t y = 0;

    board_init();

    board_printformat("hello\r\n");
    board_set_led(1, true);

    for (;;)
    {
        if (!board_audio_is_active())
        {
            y = 0;
            board_printformat("ping\r\n");

            board_set_led(0, false);
            for (volatile unsigned x = (1 << 22); x--;);
            board_set_led(0, true);
            for (volatile unsigned x = (1 << 22); x--;);
        }
        else
        {
            constexpr size_t nframes = 48; // 48 frames in 1ms at 48kHz;
            constexpr size_t nchannels = 2;

            constexpr int32_t sampling_freq_hz = 48000;
            constexpr int32_t beep_freq_hz = 880;

            // Divide by 2 because sampling_freq_hz > 2 * beep_freq_hz, so use
            // 2 ** 31 as base instead of 2 ** 32 which cannot be represented on 32b.
            constexpr int32_t freq_ratio = sampling_freq_hz / (beep_freq_hz * 2);
            constexpr int32_t step = 0x7fffffff / freq_ratio;

            volatile int32_t *pcm = board_audio_get_pcm_buffer();
            auto const pcm_begin = pcm;
            bool abort = false;
            for (auto frame = nframes; frame-- && !abort;)
            {
                for (auto channel = nchannels; channel-- && !abort;)
                {
                    *pcm++ = y;
                    abort = !board_audio_is_active()
                        || pcm_begin != board_audio_get_pcm_buffer();
                }
                y += step;
            }

            // App finished in time, wait for buffer swap
            if (!abort)
            {
                // TODO: Maybe add SOF+ESOF interrupt timeouts to prevent starving if
                // the host decides to stop the communication
                while (pcm_begin == board_audio_get_pcm_buffer()
                    && board_audio_is_active())
                {
                    __asm__("wfi");
                }
            }
        }
    }
    return 0;
}
