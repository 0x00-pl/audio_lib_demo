#ifndef SPEECH_RECOG_H
#define SPEECH_RECOG_H
#include <stdint.h>
#include "stdbool.h"
void speech_recog_init();
void speec_recog_frame(int16_t *buffer,bool firstframe);

#endif // SPEECH_RECOG_H
