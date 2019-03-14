#ifndef _I2S_CAPTURE_H_
#define _I2S_CAPTURE_H_
#include "stdbool.h"
#include "config.h"

void i2s_capture(int16_t *i2s_one_frame,uint32_t i2s_one_frame_len);

#endif