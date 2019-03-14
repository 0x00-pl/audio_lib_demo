#include "stdio.h"
#include "config.h"
#include "gpio.h"
#include "fpioa.h"
#include "gpio_detect.h"
#include "i2s_capture.h"
#include "speech_recog.h"
#include "config.h"

#ifdef INCLUDE_WEBRTC_NS
#include "noise_suppression.h"
#endif
#ifdef INCLUDE_WEBRTC_VAD
#include "vad.h"
#endif
#ifdef INCLUDE_WEBRTC_AGC
#include "agc.h"
#endif

#if defined(INCLUDE_MIC_ARRAY) || defined(INCLUDE_WEBRTC_VAD) || defined(INCLUDE_PLAYBACK)
static int16_t g_speech_i2s_rcv_buffer[CONFIG_I2S_RCV_BUFFER_LEN];
#endif

#ifdef INCLUDE_WEBRTC_VAD
volatile static bool g_i2s_5120ms_data_valid[CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE];

static void mark_valid_audio()
{
    int detect_begin = 0;
    int detect_true_cnt = 0;
#if 0
    for(int i=0;i < CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE;i++)
    {
        if(i % 16 == 0) printf("\r\n");

        printf("%04d:%s",i,(g_i2s_5120ms_data_valid[i] == true)?"true":"    ");
    }
    printf("\r\n");
#endif
    //先找出至少连续100ms的语音frame,并重新打上标记
    for(int i = 0;i < CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE; i++)
    {
        if(g_i2s_5120ms_data_valid[i]==true)//连续true
        {
            detect_true_cnt++;
        }
        else
        {
            if(detect_true_cnt < VAD_FRAME_THREHOLD)//有效语音frame
            {
                for(int j = detect_begin;j <= i; j++) 
                    g_i2s_5120ms_data_valid[j] = false;
            }
            detect_begin = i+1;
            detect_true_cnt = 0;
        }
    }

#if 1 //语音帧前后加上30ms 3*CONFIG_SMOOTH_STEP_SIZE的余量
    //防止填写越界,补前面
    for(int i = 3; i < CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE; i++)
    {
        if(g_i2s_5120ms_data_valid[i] == true)
        {
            for(int j = 1; j <= 3; j++)
            {
                g_i2s_5120ms_data_valid[i-j] = true;
            }
        }
    }
    //补后面
    for(int i = CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE-3; i >= 0; i--)
    {
        if(g_i2s_5120ms_data_valid[i] == true)
        {
            for(int j = 1; j <= 3; j++)
            {
                g_i2s_5120ms_data_valid[i+j] = true;
            }
        }
    }
#endif

#if 0 /*after vad*/
    printf("\r\nafter mark_valid_audio");
    for(int i=0;i < CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE;i++)
    {
        if(i % 16 == 0) printf("\r\n");

        printf("%04d:%s",i,(g_i2s_5120ms_data_valid[i] == true)?"true":"    ");
    }
#endif
}

//根据标记的frame进行数据合并
static void make_valid_audio()
{
    int i=0,j=0,valid_frame_index=0;

    for(i = 0; i < CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE; i++)
    {
        if(g_i2s_5120ms_data_valid[i] == true)
        {
            if(i != valid_frame_index)
            {
                for(j = 0; j < CONFIG_SMOOTH_STEP_SIZE;j ++)
                {
                    g_speech_i2s_rcv_buffer[valid_frame_index*CONFIG_SMOOTH_STEP_SIZE+j] = g_speech_i2s_rcv_buffer[i*CONFIG_SMOOTH_STEP_SIZE+j];
                }
            }
            valid_frame_index++;//true frame
        }
    }

    if(valid_frame_index == 0) 
    {
        printf("Invalid voiceprint!\r\n");
        for(i = 0;i < CONFIG_I2S_RCV_BUFFER_LEN;i++)
        {
            g_speech_i2s_rcv_buffer[i] = 0;
        }
    }
    else
    {
        //copy valid frame block
        for(i = 1; i <= CONFIG_I2S_RCV_BUFFER_LEN/(valid_frame_index*CONFIG_SMOOTH_STEP_SIZE); i++)
        {
            for(j = 0; j < valid_frame_index*CONFIG_SMOOTH_STEP_SIZE;j++)
            {
                if((i*valid_frame_index*CONFIG_SMOOTH_STEP_SIZE)+j < CONFIG_I2S_RCV_BUFFER_LEN) //防止越界
                    g_speech_i2s_rcv_buffer[(i*valid_frame_index*CONFIG_SMOOTH_STEP_SIZE) + j] = g_speech_i2s_rcv_buffer[j];
                else
                    return;
            }
        }
    }
}
#endif

#ifdef INCLUDE_PLAYBACK
#include "i2s.h"
static uint32_t playback[CONFIG_I2S_RCV_BUFFER_LEN*2];
void play_back()
{
    for(int i = 0; i < CONFIG_I2S_RCV_BUFFER_LEN; i++)
    {
        playback[i*2] = 0;
        playback[i*2+1] = g_speech_i2s_rcv_buffer[i]<<5;
    }
    i2s_send_data_dma(I2S_DEVICE_2, playback, CONFIG_I2S_RCV_BUFFER_LEN*2, PLAYBACK_DAM_CHANNEL);
}
#endif
/*-------------------------------------------call back--------------------------------------------*/
void i2s_capture(int16_t *i2s_one_frame,uint32_t i2s_one_frame_len)
{
    static int i2s_capture_frame_index = 0;

#ifdef INCLUDE_WEBRTC_VAD
    static uint32_t valid_audio_cnt = 0;
    static int vad_process_offset = 0;
    int32_t  vad_sensibility;
#endif

    do_switch_status(); //detect switch and key
#ifdef INCLUDE_WEBRTC_VAD
    vad_sensibility = get_vad_sensibility();
#endif
    if((get_switch_status_pressed() == AUDIO_CAPTURE) || (get_switch_status_pressed() == AUDIO_RECOGNITION)) //持续处理
    {
        if((i2s_capture_frame_index + i2s_one_frame_len) <= CONFIG_I2S_RCV_BUFFER_LEN) //process sample...
        {
            led_onoff(1, 1);
#ifdef INCLUDE_WEBRTC_VAD
            for(int i = 0; i < CONFIG_I2S_INTERRUPT_FRAME_LEN; i++)
            {
                if(vad_sensibility > 0)
                {
                    i2s_one_frame[i] = i2s_one_frame[i] << vad_sensibility;
                }
                else
                {
                    i2s_one_frame[i] = i2s_one_frame[i] >> abs(vad_sensibility);
                }
            }

            for(int i = 0; i < i2s_one_frame_len;i++)
            {
                g_speech_i2s_rcv_buffer[i+i2s_capture_frame_index] = i2s_one_frame[i];
            }
            i2s_capture_frame_index += i2s_one_frame_len;


            while((i2s_capture_frame_index - vad_process_offset) >= CONFIG_SMOOTH_STEP_SIZE)
            {
                if(vadProcess(&g_speech_i2s_rcv_buffer[vad_process_offset],CONFIG_I2S_SAMLPLE_RATE,CONFIG_SMOOTH_STEP_SIZE,3,10) > 0)
                {
                    valid_audio_cnt ++;
                    if(valid_audio_cnt >= VAD_FRAME_THREHOLD) 
                        led_onoff(0,1);
                }
                else
                {
                    led_onoff(0,0);
                    valid_audio_cnt = 0;
                }
                vad_process_offset += CONFIG_SMOOTH_STEP_SIZE;
            }
#else
#ifndef INCLUDE_MIC_SINGLE
            for(int i = 0; i < i2s_one_frame_len;i++)
            {
                g_speech_i2s_rcv_buffer[i+i2s_capture_frame_index] = i2s_one_frame[i];
            }
            i2s_capture_frame_index += i2s_one_frame_len;
/*没有定义VAD并且是单麦克风,加速处理*/
#else
#ifdef INCLUDE_PLAYBACK //记录录音数据
            for(int i = 0; i < i2s_one_frame_len;i++)
            {
                g_speech_i2s_rcv_buffer[i+i2s_capture_frame_index] = i2s_one_frame[i];
            }
#endif
            if(i2s_capture_frame_index == 0)
                speec_recog_frame(i2s_one_frame,true);
            else
                speec_recog_frame(i2s_one_frame,false);

            i2s_capture_frame_index += CONFIG_SMOOTH_STEP_SIZE;
            if(i2s_capture_frame_index == CONFIG_I2S_RCV_BUFFER_LEN) 
                i2s_capture_frame_index = CONFIG_I2S_RCV_BUFFER_LEN + 1; 
#endif

#endif
        }
        else if(i2s_capture_frame_index == CONFIG_I2S_RCV_BUFFER_LEN) //结束,需要处理
        {
            led_onoff(0, 0);
            led_onoff(1, 0);
#ifdef INCLUDE_WEBRTC_VAD
            valid_audio_cnt = 0;
            vad_process_offset = 0;

            for(int i = 0; i < CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE; i++)
            {
                if(vadProcess(&g_speech_i2s_rcv_buffer[i*CONFIG_SMOOTH_STEP_SIZE],CONFIG_I2S_SAMLPLE_RATE,CONFIG_SMOOTH_STEP_SIZE,3,10) > 0)
                {
                    g_i2s_5120ms_data_valid[i] = true;
                }
                else 
                {
                    g_i2s_5120ms_data_valid[i] = false;
                }
            }
            mark_valid_audio();
            make_valid_audio();
            /*restore voice*/
            for(int i = 0; i < CONFIG_I2S_RCV_BUFFER_LEN; i++)
            {
                if(vad_sensibility > 0)
                {
                    g_speech_i2s_rcv_buffer[i] = g_speech_i2s_rcv_buffer[i] >> vad_sensibility; //之前放大, 现在回退
                }
                else
                {
                    g_speech_i2s_rcv_buffer[i] = g_speech_i2s_rcv_buffer[i] << abs(vad_sensibility); //之前降低, 现在放大
                }
            }
#else

#endif

#ifdef INCLUDE_WEBRTC_NS
            nsProcess(g_speech_i2s_rcv_buffer, CONFIG_I2S_SAMLPLE_RATE, CONFIG_I2S_RCV_BUFFER_LEN,kModerate);
#endif

#ifdef INCLUDE_WEBRTC_AGC
            agcProcess(g_speech_i2s_rcv_buffer,CONFIG_I2S_SAMLPLE_RATE,CONFIG_I2S_RCV_BUFFER_LEN,kAgcModeAdaptiveDigital);
#endif

/*如果定义麦克风整列,或者VAD,则统一后处理*/
#if defined(INCLUDE_MIC_ARRAY) || defined(INCLUDE_WEBRTC_VAD)
            /*do recognize*/
            for(int i = 0; i < CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_SMOOTH_STEP_SIZE;i++)
            {
                if(i == 0)
                    speec_recog_frame(&g_speech_i2s_rcv_buffer[i*CONFIG_SMOOTH_STEP_SIZE],true);
                else
                    speec_recog_frame(&g_speech_i2s_rcv_buffer[i*CONFIG_SMOOTH_STEP_SIZE],false);
            }
            i2s_capture_frame_index = CONFIG_I2S_RCV_BUFFER_LEN + 1; //标志已经结束了本次处理
#endif
        }
        else //i2s_capture_frame_index == CONFIG_I2S_RCV_BUFFER_LEN + 1
        {
            led_onoff(0, 0);
            led_onoff(1, 0);
            /*do nothing*/
        }
    }
    else //状态机清零
    {
        led_onoff(0, 0);
        led_onoff(1, 0);
#ifdef INCLUDE_WEBRTC_VAD
        valid_audio_cnt = 0;
        vad_process_offset = 0;
#endif
        i2s_capture_frame_index = 0;
    }
}
