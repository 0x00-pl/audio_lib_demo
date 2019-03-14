#ifndef _GPIO_DETECT_H_
#define _GPIO_DETECT_H_
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "gpio.h"
#include "fpioa.h"
#include "config.h"

typedef enum
{
    AUDIO_CAPTURE = 0,
    AUDIO_RECOGNITION = 1,
    DELETE_LAST_AUDIO_FEATURE = 2,
    DELETE_LAST_AUDIO_RECOGNITION = 3,

#ifdef INCLUDE_WEBRTC_VAD
    VAD_SENSIBILITY_UP = 4,
    VAD_SENSIBILITY_DOWN = 5,

#ifdef INCLUDE_PLAYBACK
    AUDIO_PLAYBACK = 6,
    SWITCH_KEY_IDLE = 7,
#else
    SWITCH_KEY_IDLE = 6,
#endif

#else //ndef INCLUDE_WEBRTC_VAD

#ifdef INCLUDE_PLAYBACK
    AUDIO_PLAYBACK = 4,
    SWITCH_KEY_IDLE = 5
#else
    SWITCH_KEY_IDLE = 4,
#endif
#endif
}SWITCH_KEY_STATUS;

void switch_init(void);
#ifdef INCLUDE_WEBRTC_VAD
int32_t get_vad_sensibility();
#endif
void gpio_key_init();
void led_onoff(int lednum,int OnOff);
SWITCH_KEY_STATUS get_switch_status_pressed();
SWITCH_KEY_STATUS get_switch_focus();
void set_switch_status_finish();
void do_switch_status();

#endif
