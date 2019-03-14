#pragma once
#include <stdint.h>
#include <stddef.h>
#include <plic.h>
#include <i2s.h>
#include <sysctl.h>
#include <dmac.h>
#include <fpioa.h>


#define SAMPLE_RATE 44100UL
#define I2S_DMA_CHANNEL DMAC_CHANNEL4
#define COPY_DATA_DMA_CHANNEL DMAC_CHANNEL3

#define FPIOA_GPIO4 47
#define FPIOA_I2S0_SCLK 46
#define FPIOA_I2S0_WS 45
#define FPIOA_I2S0_IN_D0 42
#define FPIOA_I2S0_IN_D1 43
#define FPIOA_I2S0_IN_D2 44
#define FPIOA_I2S0_IN_D3 47

void audio_init_all(void);
extern void (*audio_callback)(int x,int y,int64_t val);
extern int32_t pass_start_idx, pass_end_idx;
