#include "i2s_single_mic.h"
#include <stddef.h>
#include <stdio.h>
#include "bsp.h"
#include "config.h"
#include "i2s_capture.h"

#ifdef INCLUDE_MIC_SINGLE
//乒乓区
static int32_t single_mic_i2s_one_frame_buffer[2][CONFIG_SMOOTH_STEP_SIZE*2]; //left&&right channels
static int16_t single_mic_i2s_capture_buff[CONFIG_SMOOTH_STEP_SIZE];
static int single_mic_one_frame_buffer_flag = 0;


int single_mic_int_i2s(void* args)
{
    if(single_mic_one_frame_buffer_flag == 0)
    {
        i2s_receive_data_dma(CONFIG_SINGLE_MIC_I2S_DEVICE, (uint32_t *)single_mic_i2s_one_frame_buffer[1], CONFIG_SMOOTH_STEP_SIZE*2, I2S_SINGLE_MIC_DMA_CHANNEL);
        single_mic_one_frame_buffer_flag = 1;
        
        for(int i = 0; i < CONFIG_SMOOTH_STEP_SIZE; i++)
        {
            single_mic_i2s_capture_buff[i] = single_mic_i2s_one_frame_buffer[0][i*2+1];
        }
    }
    else
    {
        i2s_receive_data_dma(CONFIG_SINGLE_MIC_I2S_DEVICE, (uint32_t *)single_mic_i2s_one_frame_buffer[0], CONFIG_SMOOTH_STEP_SIZE*2, I2S_SINGLE_MIC_DMA_CHANNEL);
        single_mic_one_frame_buffer_flag = 0;

        for(int i = 0; i < CONFIG_SMOOTH_STEP_SIZE; i++)
        {
            single_mic_i2s_capture_buff[i] = single_mic_i2s_one_frame_buffer[1][i*2+1];
        }
    }

    i2s_capture(single_mic_i2s_capture_buff,CONFIG_SMOOTH_STEP_SIZE);
    return 0;
}

static inline void i2s_set_sign_expand_en(i2s_device_number_t device_num, uint32_t enable)
{
    ccr_t u_ccr;
    u_ccr.reg_data           = readl(&i2s[device_num]->ccr);
    u_ccr.ccr.sign_expand_en = enable;
    writel(u_ccr.reg_data, &i2s[device_num]->ccr);
}

static void single_mic_mux_init(){

    fpioa_set_function(CONFIG_GPIO_MIC_IN_D0_PIN, CONFIG_GPIO_MIC_IN_D0_FUN);
    fpioa_set_function(CONFIG_GPIO_MIC_WS_PIN, CONFIG_GPIO_MIC_WS_FUN);
    fpioa_set_function(CONFIG_GPIO_MIC_SCLK_PIN, CONFIG_GPIO_MIC_SCLK_FUN);
#ifdef INCLUDE_PLAYBACK
    fpioa_set_function(CONFIG_GPIO_PLAYBACK_D1_PIN, CONFIG_GPIO_PLAYBACK_D1_FUN);
    fpioa_set_function(CONFIG_GPIO_PLAYBACK_SCLK_PIN, CONFIG_GPIO_PLAYBACK_SCLK_FUN);
    fpioa_set_function(CONFIG_GPIO_PLAYBACK_WS_PIN, CONFIG_GPIO_PLAYBACK_WS_FUN);
#endif
}

void single_mic_init(void)
{
    single_mic_mux_init();
    i2s_init(CONFIG_SINGLE_MIC_I2S_DEVICE, I2S_RECEIVER, 0x3);
    i2s_set_sample_rate(CONFIG_SINGLE_MIC_I2S_DEVICE,CONFIG_I2S_SAMLPLE_RATE);

    i2s_rx_channel_config(CONFIG_SINGLE_MIC_I2S_DEVICE, I2S_CHANNEL_0,
        RESOLUTION_16_BIT, SCLK_CYCLES_32,
        TRIGGER_LEVEL_4, STANDARD_MODE);

    dmac_set_irq(I2S_SINGLE_MIC_DMA_CHANNEL, single_mic_int_i2s, NULL, 1);
    i2s_set_sign_expand_en(CONFIG_SINGLE_MIC_I2S_DEVICE, 1);
    single_mic_one_frame_buffer_flag = 0;
    i2s_receive_data_dma(CONFIG_SINGLE_MIC_I2S_DEVICE, (uint32_t *)single_mic_i2s_one_frame_buffer[0], 2 * CONFIG_SMOOTH_STEP_SIZE, I2S_SINGLE_MIC_DMA_CHANNEL);
#ifdef INCLUDE_PLAYBACK
    i2s_init(CONFIG_I2S_PLAYBACK_DEVICE, I2S_TRANSMITTER, 0xC);

    i2s_tx_channel_config(CONFIG_I2S_PLAYBACK_DEVICE, I2S_CHANNEL_1,
        RESOLUTION_16_BIT, SCLK_CYCLES_32,
        TRIGGER_LEVEL_4,
        RIGHT_JUSTIFYING_MODE
        );
    i2s_set_sample_rate(CONFIG_I2S_PLAYBACK_DEVICE, CONFIG_I2S_SAMLPLE_RATE);
#endif
}
#endif
