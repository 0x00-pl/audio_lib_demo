
#include <stdio.h>
#include <stdlib.h>
#include "sysctl.h"
#include "plic.h"
#include "dmac.h"
#include "uarths.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "i2s_audio.h"
#include <unistd.h>
#include "speech_recog.h"
#include <atomic.h>

spinlock_t ai_lock = SPINLOCK_INIT;
spinlock_t fft_lock = SPINLOCK_INIT;

void on_audio_pos(int x, int y, int64_t val)
{
    /*do nothing*/
}

static int32_t buffer[512];
void on_audio_data(int32_t *data)
{
    for(int i = 0; i<160; ++i)
    {
        buffer[i] = data[2*i+1]; /*right channel*/
    }

    speec_recog_frame(buffer);
}

extern float cpu_regcog();

void io_mux_init(){

    fpioa_set_function(36, FUNC_I2S0_IN_D0);
    fpioa_set_function(37, FUNC_I2S0_WS);
    fpioa_set_function(38, FUNC_I2S0_SCLK);
}

extern void features_init();
extern void kpu_module_init();
extern void ws2812_init();


int system_init(){
    sysctl_set_power_mode(SYSCTL_POWER_BANK0, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK2, SYSCTL_POWER_V18);

    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 160000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL2, 32768000UL);
    dmac_init();
    ws2812_init();

    sysctl_clock_enable(SYSCTL_CLOCK_AI);

    uarths_init();

    io_mux_init();
    dmac_init();
    plic_init();

    speech_recog_init();
    features_init();
    kpu_module_init();
    audio_init_all();
    on_direction_calced = on_audio_pos;
    on_i2s_data = on_audio_data;
    do_direction_scan();

    sysctl_enable_irq();
    return 0;
}
