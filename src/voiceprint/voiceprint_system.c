#include "voiceprint_system.h"
#include <stdio.h>
#include <stdlib.h>
#include "sysctl.h"
#include "plic.h"
#include "dmac.h"
#include "uarths.h"
#include "fpioa.h"
#include "gpiohs.h"
#include <unistd.h>
#include "speech_recog.h"
#include <atomic.h>
#include "i2s_single_mic.h"
#include "mic_array.h"
#include "gpio_detect.h"
#include "i2s_capture.h"
#include "lcd_demo.h"
#include "sdcard.h"
spinlock_t ai_lock  = SPINLOCK_INIT;
spinlock_t fft_lock = SPINLOCK_INIT;

extern void features_init();
extern void kpu_module_init();


int system_init(){
    sysctl_set_power_mode(SYSCTL_POWER_BANK0, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK2, SYSCTL_POWER_V18);

    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 160000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL2, 32768000UL);
    dmac_init();
    sysctl_clock_enable(SYSCTL_CLOCK_AI);

    uarths_init();
    plic_init();

    speech_recog_init();
    features_init();
    /*for demo test*/
    gpio_key_init();
    switch_init();
    lcd_show_init();
    /*for demoe test*/
    kpu_module_init();
    #ifdef INCLUDE_MIC_ARRAY
    mic_array_init();
    #else
    single_mic_init();
    #endif
    sd_init_all();
    sysctl_enable_irq();
    return 0;
}
