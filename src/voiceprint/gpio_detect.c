#include "stdio.h"
#include "stdbool.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "gpio.h"
#include "gpio_detect.h"
#include "config.h"

/*----------------------------------------------switch for mic volume--------------------------------------------------*/
#ifdef INCLUDE_WEBRTC_VAD
static int32_t g_current_vad_sensibility = 3;
#endif
void switch_init(void)
{
	fpioa_set_function(CONFIG_GPIO_SWITCH_0_PIN, CONFIG_GPIO_SWITCH_0_FUNC);
	fpioa_set_function(CONFIG_GPIO_SWITCH_1_PIN, CONFIG_GPIO_SWITCH_1_FUNC);
	fpioa_set_function(CONFIG_GPIO_SWITCH_2_PIN, CONFIG_GPIO_SWITCH_2_FUNC);
	fpioa_set_function(CONFIG_GPIO_SWITCH_3_PIN, CONFIG_GPIO_SWITCH_3_FUNC);
	gpiohs->input_en.u32[0] |= CONFIG_GPIOHS_SWITCH_INPUT_EN;
}

static uint8_t switch_read(void)
{
    uint8_t num = CONFIG_READ_SWITCH_VALUE(gpiohs->input_val.u32[0]);
	return num;
}

#ifdef INCLUDE_WEBRTC_VAD
int32_t get_vad_sensibility()
{
    return g_current_vad_sensibility;
}
#endif
/*--------------------------------------------------key led----------------------------------------------------------------------*/
//gpio led and key1
void gpio_key_init()
{
    gpio_init();
    gpio->interrupt_mask.u32[0] = 0;
    gpio->source.u32[0] = 0xff;

    fpioa_init();
    fpioa_set_function(CONFIG_GPIO_LED_0_PIN,CONFIG_GPIO_LED_0_FUN);
    fpioa_set_function(CONFIG_GPIO_LED_1_PIN,CONFIG_GPIO_LED_1_FUN);

    gpio->direction.u32[0] = 0;
    gpio->direction.u32[0] |= (1 << (CONFIG_GPIO_LED_0_FUN-FUNC_GPIO0)) | (1 << (CONFIG_GPIO_LED_1_FUN-FUNC_GPIO0));

    /*key detect*/
    
    fpioa_set_function(CONFIG_GPIO_KEY_0_PIN,CONFIG_GPIO_KEY_0_FUN);/*key*/
    gpio->direction.u32[0] &= ~(1 << (CONFIG_GPIO_KEY_0_FUN-FUNC_GPIO0));/*input*/

}


void led_onoff(int lednum,int OnOff)
{
    if(lednum == 0)
    {
        if(OnOff)
            gpio->data_output.u32[0] &= ~(1 << (CONFIG_GPIO_LED_0_FUN-FUNC_GPIO0));
        else
            gpio->data_output.u32[0] |= (1 << (CONFIG_GPIO_LED_0_FUN-FUNC_GPIO0));
    }
    else if(lednum == 1)
    {
        if(OnOff)
            gpio->data_output.u32[0] &= ~(1 << (CONFIG_GPIO_LED_1_FUN-FUNC_GPIO0));
        else
            gpio->data_output.u32[0] |= (1 << (CONFIG_GPIO_LED_1_FUN-FUNC_GPIO0));
    }
}

static SWITCH_KEY_STATUS g_current_switch_status = AUDIO_CAPTURE;
static bool g_is_main_processing_switch_status = false; //当前主程序在处理


//判断当前焦点,看选择值
SWITCH_KEY_STATUS get_switch_focus()
{
     return (g_current_switch_status > SWITCH_KEY_IDLE) ? SWITCH_KEY_IDLE:g_current_switch_status;
}


//当前选择生效,1:已经选择,2:key已经按下
SWITCH_KEY_STATUS get_switch_status_pressed()
{
    if(g_is_main_processing_switch_status == true)
    {
        return g_current_switch_status;
    }
    else
    {
        return SWITCH_KEY_IDLE;
    }
}

//其他任务处理完毕后,调用这个函数
void set_switch_status_finish()
{
    g_current_switch_status = SWITCH_KEY_IDLE;
    g_is_main_processing_switch_status = false;
}

//i2s每次中断里调用这个
void do_switch_status()
{
    static unsigned int key_press_count = 0;
    
    SWITCH_KEY_STATUS key_status = (SWITCH_KEY_STATUS)switch_read();

    //其他程序未处理完,不处理
    if(g_is_main_processing_switch_status == true)
    {
        key_press_count = 0;
        return;
    }

    //超过拨码开关定义,不处理
    if(key_status >= SWITCH_KEY_IDLE)
    {
        g_current_switch_status = SWITCH_KEY_IDLE;
        key_press_count = 0;
        return;
    }

    //拨码开关没有变,记录按下key的次数并处理
    if(g_current_switch_status == key_status)
    {
        if((gpio->data_input.u32[0] & (1 << 6)) == 0)
        {
            key_press_count ++;
        }
        else
        {
            key_press_count = 0;
            return;
        }

        //检测到了10次,并且只有第三次才做操作,大于10次的认为已经做过操作
        if(key_press_count == CONFIG_KEY_PRESS_DURATION_TIMES)
        {
            switch(key_status)
            {
                case AUDIO_CAPTURE:
                case AUDIO_RECOGNITION:
                case DELETE_LAST_AUDIO_FEATURE:
                case DELETE_LAST_AUDIO_RECOGNITION:
                {
                    g_is_main_processing_switch_status = true;
                    return;
                }
#ifdef INCLUDE_WEBRTC_VAD
                case VAD_SENSIBILITY_UP:
                {
                    g_current_vad_sensibility ++;
                    if(g_current_vad_sensibility > CONFIG_VAD_SENSIBILITY_VOLUME_MAX)
                        g_current_vad_sensibility = CONFIG_VAD_SENSIBILITY_VOLUME_MAX;
                    return;
                }
                case VAD_SENSIBILITY_DOWN:
                {
                    g_current_vad_sensibility --;
                    if(g_current_vad_sensibility < CONFIG_VAD_SENSIBILITY_VOLUME_MIN)
                        g_current_vad_sensibility = CONFIG_VAD_SENSIBILITY_VOLUME_MIN;
                    return;
                }
#endif
#ifdef INCLUDE_PLAYBACK
				case AUDIO_PLAYBACK:
				{
                    g_is_main_processing_switch_status = true;
                    return;
                }
#endif
                default: //前面程序已经判断过,正常不会到这里
                    return;
            }
        }
        else if(key_press_count > CONFIG_KEY_PRESS_DURATION_TIMES)
        {
            key_press_count = CONFIG_KEY_PRESS_DURATION_TIMES + 1;
        }
        else
        {
            /*do nothing, wait for 10 press*/
        }
    }
    else //拨码开关改变了
    {
        g_current_switch_status = key_status;
        key_press_count = 0;
    }
}
