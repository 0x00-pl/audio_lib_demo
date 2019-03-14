#ifndef _CONFIG_H_
#define _CONFIG_H_

#undef INCLUDE_MIC_ARRAY         //麦克风阵列
#undef INCLUDE_WEBRTC_NS         //噪声抑制
#undef INCLUDE_WEBRTC_AGC        //AGC
#undef INCLUDE_WEBRTC_VAD        //VAD
#define INCLUDE_PLAYBACK

#define INCLUDE_SOFT_FFT        //software fft

#ifdef INCLUDE_MIC_ARRAY
#undef INCLUDE_MIC_SINGLE        //单麦克风
#else
#define INCLUDE_MIC_SINGLE
#endif

/*gpio pin func*/
#define CONFIG_GPIO_LED_0_PIN           24
#define CONFIG_GPIO_LED_1_PIN           25
#define CONFIG_GPIO_KEY_0_PIN           26

#define CONFIG_GPIO_SWITCH_0_PIN        23
#define CONFIG_GPIO_SWITCH_1_PIN        22
#define CONFIG_GPIO_SWITCH_2_PIN        21
#define CONFIG_GPIO_SWITCH_3_PIN        20

#define CONFIG_GPIO_SD_SPI_CLK_PIN      29
#define CONFIG_GPIO_SD_SPI_D0_PIN       30
#define CONFIG_GPIO_SD_SPI_D1_PIN       31
#define CONFIG_GPIO_SD_SPI_CS_PIN       32

#define CONFIG_GPIO_LCD_CS_PIN          6
#define CONFIG_GPIO_LCD_WR_PIN          7
#define CONFIG_GPIO_LCD_DC_PIN          8

#define CONFIG_GPIO_MIC_IN_D0_PIN       36
#define CONFIG_GPIO_MIC_WS_PIN          37
#define CONFIG_GPIO_MIC_SCLK_PIN        38

#define CONFIG_GPIO_MIC_ARRAY_D0_PIN    42
#define CONFIG_GPIO_MIC_ARRAY_D1_PIN    43
#define CONFIG_GPIO_MIC_ARRAY_D2_PIN    44
#define CONFIG_GPIO_MIC_ARRAY_D3_PIN    45
#define CONFIG_GPIO_MIC_ARRAY_WS_PIN    46
#define CONFIG_GPIO_MIC_ARRAY_SCLK_PIN  39


#define CONFIG_GPIO_PLAYBACK_D1_PIN     33
#define CONFIG_GPIO_PLAYBACK_SCLK_PIN   35
#define CONFIG_GPIO_PLAYBACK_WS_PIN     34

#define CONFIG_GPIO_LED_0_FUN           FUNC_GPIO4
#define CONFIG_GPIO_LED_1_FUN           FUNC_GPIO5
#define CONFIG_GPIO_KEY_0_FUN           FUNC_GPIO6

#define CONFIG_GPIO_SD_SPI_CLK_FUN      FUNC_SPI1_SCLK
#define CONFIG_GPIO_SD_SPI_D0_FUN       FUNC_SPI1_D0
#define CONFIG_GPIO_SD_SPI_D1_FUN       FUNC_SPI1_D1
#define CONFIG_GPIO_SD_SPI_CS_FUN       FUNC_GPIOHS7


#define CONFIG_GPIO_SWITCH_0_FUNC       FUNC_GPIOHS3
#define CONFIG_GPIO_SWITCH_1_FUNC       FUNC_GPIOHS5
#define CONFIG_GPIO_SWITCH_2_FUNC       FUNC_GPIOHS6
#define CONFIG_GPIO_SWITCH_3_FUNC       FUNC_GPIOHS8

#define CONFIG_GPIO_LCD_SPI_CLK_FUN     FUNC_SPI0_SCLK
#define CONFIG_GPIO_LCD_SPI_CS_FUN      FUNC_SPI0_SS3
#define CONFIG_GPIO_LCD_SPI_DC_FUN      FUNC_GPIOHS2

#define CONFIG_GPIO_MIC_IN_D0_FUN       FUNC_I2S0_IN_D0
#define CONFIG_GPIO_MIC_WS_FUN          FUNC_I2S0_WS
#define CONFIG_GPIO_MIC_SCLK_FUN        FUNC_I2S0_SCLK
                                        
#define CONFIG_GPIO_PLAYBACK_D1_FUN     FUNC_I2S2_OUT_D1
#define CONFIG_GPIO_PLAYBACK_SCLK_FUN   FUNC_I2S2_SCLK
#define CONFIG_GPIO_PLAYBACK_WS_FUN     FUNC_I2S2_WS

#define CONFIG_GPIO_MIC_ARRAY_IN_D0_FUN FUNC_I2S0_IN_D0
#define CONFIG_GPIO_MIC_ARRAY_IN_D1_FUN FUNC_I2S0_IN_D1
#define CONFIG_GPIO_MIC_ARRAY_IN_D2_FUN FUNC_I2S0_IN_D2
#define CONFIG_GPIO_MIC_ARRAY_IN_D3_FUN FUNC_I2S0_IN_D3
#define CONFIG_GPIO_MIC_ARRAY_WS_FUN    FUNC_I2S0_WS
#define CONFIG_GPIO_MIC_ARRAY_SCLK_FUN  FUNC_I2S0_SCLK


#define CONFIG_SINGLE_MIC_I2S_DEVICE    I2S_DEVICE_0
#define CONFIG_MIC_ARRAY_I2S_DEVICE     I2S_DEVICE_0
#define CONFIG_I2S_PLAYBACK_DEVICE      I2S_DEVICE_2


#define CONFIG_GPIOHS_SWITCH_INPUT_EN  ((1 << (CONFIG_GPIO_SWITCH_0_FUNC-FUNC_GPIOHS0)) | \
                                        (1 << (CONFIG_GPIO_SWITCH_1_FUNC-FUNC_GPIOHS0)) | \
                                        (1 << (CONFIG_GPIO_SWITCH_2_FUNC-FUNC_GPIOHS0)) | \
                                        (1 << (CONFIG_GPIO_SWITCH_3_FUNC-FUNC_GPIOHS0)))

#define CONFIG_READ_SWITCH_VALUE(x)    ((((x >> (CONFIG_GPIO_SWITCH_0_FUNC-FUNC_GPIOHS0)) & 0x1) << 0) | \
                                        (((x >> (CONFIG_GPIO_SWITCH_1_FUNC-FUNC_GPIOHS0)) & 0x1) << 1 )| \
                                        (((x >> (CONFIG_GPIO_SWITCH_2_FUNC-FUNC_GPIOHS0)) & 0x1) << 2 ) | \
                                        (((x >> (CONFIG_GPIO_SWITCH_3_FUNC-FUNC_GPIOHS0)) & 0x1) << 3 ))

/*spi*/
#define CONFIG_LCD_SPI_CHANNEL             SPI_DEVICE_0
#define CONFIG_LCD_SPI_SLAVE_SELECT        3

#define CONFIG_SD_SPI_CHANNEL              SPI_DEVICE_1

/*dma*/
#define AI_DMA    DMAC_CHANNEL0
#define FFT_DMA_1 DMAC_CHANNEL1
#define FFT_DMA_2 DMAC_CHANNEL2

#ifdef INCLUDE_MIC_SINGLE
#define I2S_SINGLE_MIC_DMA_CHANNEL DMAC_CHANNEL4
#else
#define AUDIO_BF_DIR_DMA_CHANNEL   DMAC_CHANNEL3
#define AUDIO_BF_VOC_DMA_CHANNEL   DMAC_CHANNEL4
#endif

#define LCD_DMA   DMAC_CHANNEL5
#define CONFIG_FEATURES_NUM 512
#define CONFIG_FEATURES_STEP 512
#define SDCARD_DAM_CHANNEL         DMAC_CHANNEL3

#ifdef INCLUDE_PLAYBACK
#define PLAYBACK_DAM_CHANNEL       DMAC_CHANNEL1 //share hardware fft dma
#endif


/*speech*/
#define CONFIG_I2S_SAMLPLE_RATE   16000 //采样频率
#define CONFIG_SMOOTH_STEP_SIZE   160 //滑窗大小

#define CONFIG_I2S_RCV_MS_TIME    5120 //5120ms
#define CONFIG_I2S_RCV_BUFFER_LEN (5120*CONFIG_I2S_SAMLPLE_RATE/1000)

#define CONFIG_PROCESS_FRAME_SIZE 400 //处理一帧的大小
#define CONFIG_MEL_FFT_SIZE       512 //梅尔FFT大小
#define CONFIG_MEL_DEMENSION      64  //Mel滤波维度
#define CONIFG_PREEMPHASIS_COEF   0.97 //预加重高通滤波器系数

#define CONFIG_VAD_SENSIBILITY_ADJUST_DURATION_TIMES       10 //连续10次检测到认为是调整音量
#define CONFIG_VAD_SENSIBILITY_VOLUME_MAX                   8   //mic最大音量
#define CONFIG_VAD_SENSIBILITY_VOLUME_MIN                  (-8) //mic最小音量

#ifdef INCLUDE_MIC_ARRAY //持续按下KEY1时间长度认为是按下
#define CONFIG_KEY_PRESS_DURATION_TIMES 3
#else
#define CONFIG_KEY_PRESS_DURATION_TIMES 10 
#endif

#ifdef INCLUDE_MIC_ARRAY
#define CONFIG_I2S_INTERRUPT_FRAME_LEN 512 //i2s中断数据为512采样点
#else
#define CONFIG_I2S_INTERRUPT_FRAME_LEN CONFIG_SMOOTH_STEP_SIZE //i2s中断数据为160采样点
#endif

#define CONFIG_KEY_KEEP_I2S_INTERRUPT_TIMES (CONFIG_I2S_RCV_BUFFER_LEN/CONFIG_I2S_INTERRUPT_FRAME_LEN)

#define VAD_FRAME_THREHOLD 10

/*sdcard file*/
#define FEATURE_DIR "features"
#define RECOGNITION_DIR "recognition"
#define FEATURE_NAME_SUFFIX ".feature.txt"
#define RECOGNITION_NAME_SUFFIX ".recognition.txt"

#define MAX_FEATURE_COUNT       1000
#define MAX_RECOGNITION_COUNT   1000

#define CONFIG_KPU_OUT_C            128
#define CONFIG_KPU_OUT_H            8
#define CONFIG_KPU_OUT_W            64

#define CONFIG_COS_VECTOR_DEMENSION 128

#define CONFIG_POOLING_SIZE         64
#define CONFIG_VECTOR_FLOAT_STRING_LEN 20

/*lcd show*/
#define CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER 12 //排序个数

#endif
