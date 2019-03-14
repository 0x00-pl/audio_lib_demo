#ifndef _MIC_DEF_H_
#define _MIC_DEF_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define DIRECTION_RES 16

#define I2S_FS 44100
#define SOUND_SPEED 340

extern double MIC_POS_X[8];
extern double MIC_POS_Y[8];
extern double MIC_POS_Z[8];

extern bool MIC_EN[8];

extern int8_t *delay_table[8][8];

void make_mic_array_circle(double radius,size_t count,bool center);
void make_delay_table();

#endif
