#ifndef SPEECH_RECOG_H
#define SPEECH_RECOG_H
#include <stdint.h>

#define AI_DMA DMAC_CHANNEL0
#define FFT_DMA_1 DMAC_CHANNEL1
#define FFT_DMA_2 DMAC_CHANNEL2

void speech_recog_init();
void speec_recog_frame(int32_t *buffer);

extern void (*on_speech_recog)(int);

#endif // SPEECH_RECOG_H
