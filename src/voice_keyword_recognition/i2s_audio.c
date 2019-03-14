#include "i2s_audio.h"
#include <stddef.h>
#include <stdio.h>
#include <printf.h>
#include <fft.h>
#include <atomic.h>
#include "string.h"
#include "bsp.h"

void (*on_direction_calced)(int x, int y, int64_t val);
void (*on_i2s_data)(int32_t *data);

static volatile int worker_status = 0;

#define I2S_INPUT_SHIFT_FACTOR 8

#define FFT_POINT FFT_512


#define FFT_IN_MODE_RIRI 0
#define FFT_IN_MODE_RRRR 1
#define FFT_IN_MODE_RRRRIIII 2
#define FFT_INPUT_DATA_MODE_64Bit 0
#define FFT_INPUT_DATA_MODE_32Bit 1
#define FFT_NOT_USING_DMA 0
#define FFT_USING_DMA 1
#define FFT_SHIFT_FACTOR 0x00

#define FFT_RESULT_ADDR (0x42100000 + 0x2000 + (FFT_POINT % 2 == 0 ? 0x800 : 0))
volatile uint64_t* fft_result1 = (volatile uint64_t *)FFT_RESULT_ADDR;

static int32_t buffer_raw[GCCPHAT_SIZE][2];
static int32_t buffer[GCCPHAT_SIZE][2];

int int_i2s(void* args)
{
//     int current_status = atomic_read(&worker_status);
//     if (current_status == 0)
//     {
//         memcpy(buffer, buffer_raw, 2 * GCCPHAT_SIZE * 4);
//         atomic_set(&worker_status, -1);
//     }
    memcpy(buffer, buffer_raw, 2 * GCCPHAT_SIZE * 4);
    i2s_receive_data_dma(I2S_DEVICE_0, (uint32_t *)buffer_raw, 2 * GCCPHAT_SIZE, I2S_DMA_CHANNEL);

    on_i2s_data((void*)buffer);
    return 0;
}

static inline void i2s_set_sign_expand_en(i2s_device_number_t device_num, uint32_t enable)
{
    ccr_t u_ccr;
    u_ccr.reg_data           = readl(&i2s[device_num]->ccr);
    u_ccr.ccr.sign_expand_en = enable;
    writel(u_ccr.reg_data, &i2s[device_num]->ccr);
}

void audio_init_all(void)
{
    i2s_init(I2S_DEVICE_0, I2S_RECEIVER, 0x3);
    i2s_set_sample_rate(I2S_DEVICE_0,16000);

    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_0,
        RESOLUTION_16_BIT, SCLK_CYCLES_32,
        TRIGGER_LEVEL_4, STANDARD_MODE);

    dmac_set_irq(I2S_DMA_CHANNEL, int_i2s, NULL, 1);
    i2s_set_sign_expand_en(I2S_DEVICE_0, 1);
    i2s_receive_data_dma(I2S_DEVICE_0, (uint32_t *)buffer_raw, 2 * GCCPHAT_SIZE, I2S_DMA_CHANNEL);
}

static int core_func(void* ctx)
{
    while (1)
    {
        int current_status = atomic_read(&worker_status);
        if (current_status == 0)
            continue;
        atomic_set(&worker_status, 0);
    }
    return 0;
}

void do_direction_scan(void)
{
    register_core1(core_func, 0);
}
