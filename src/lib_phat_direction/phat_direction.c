#include <stdio.h>
#include <stdlib.h>
#include "sysctl.h"
#include "plic.h"
#include "dmac.h"
#include "uarths.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "i2s_bf.h"
#include <unistd.h>
#include "mic_def.h"
#include <printf.h>

#define PLL0_OUTPUT_FREQ 800000000UL
#define PLL1_OUTPUT_FREQ 300000000UL
#define PLL2_OUTPUT_FREQ ((SAMPLE_RATE*64*16)) // 45158400

int32_t pass_start_idx=1, pass_end_idx=256;



int system_run(void cb(int angle, int theta, int64_t val))
{
    sysctl_set_power_mode(SYSCTL_POWER_BANK0, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK2, SYSCTL_POWER_V18);

    sysctl_pll_set_freq(SYSCTL_PLL0, PLL0_OUTPUT_FREQ);
//     sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);
    sysctl_pll_set_freq(SYSCTL_PLL2, PLL2_OUTPUT_FREQ);
    uarths_init();
    uarths_config(115200, 0);

    dmac_init();
    plic_init();

    make_mic_array_circle(0.03, 7, 0);
    make_delay_table();

    audio_init_all();
    audio_callback = cb;

    sysctl_enable_irq();
    printf("main end\n");
    static char buff[1024]= {0};
    uint32_t buff_end = 0;


//     uarths_receive_data(buff, 8); //read diry data
    while (1)
    {

//         printk("[debug]%lu %lu\n", pass_start_idx*SAMPLE_RATE/512, pass_end_idx*SAMPLE_RATE/512);
/*
        if(uarths_receive_data(buff+buff_end, 1) != 1) {
            continue;
        }
//         printk("[debug] got char %x\n", buff[buff_end]);
        buff_end = (buff_end+1 % 1000);
        if(buff[buff_end-1] == '\n') {
            buff[buff_end] = '\0';
            int32_t pass_start, pass_end;
            sscanf(buff, "%d %d\n", &pass_start, &pass_end);
            buff_end = 0;
            pass_start_idx = (512*pass_start)/SAMPLE_RATE;
            pass_end_idx = (512*pass_end)/SAMPLE_RATE;
            if(pass_start_idx<1) {
                pass_start_idx=1;
            }
            if(pass_end_idx>256) {
                pass_end_idx=256;
            }
        }*/
    }

    return 0;
}
