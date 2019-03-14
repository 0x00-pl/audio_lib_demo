#pragma once
#include <stdint.h>
#include <stddef.h>
#include <plic.h>
#include <i2s.h>
#include <sysctl.h>
#include <dmac.h>
#include <fpioa.h>


#define I2S_DMA_CHANNEL DMAC_CHANNEL4
#define GCCPHAT_SIZE 160

#define FPIOA_GPIO4 47
#define FPIOA_I2S0_SCLK 39
#define FPIOA_I2S0_WS 46
#define FPIOA_I2S0_IN_D0 42
#define FPIOA_I2S0_IN_D1 43
#define FPIOA_I2S0_IN_D2 44
#define FPIOA_I2S0_IN_D3 45

void audio_init_all(void);
extern void (*on_direction_calced)(int x,int y,int64_t val);
void do_direction_scan(void);
extern void (*on_i2s_data)(int32_t *data);
