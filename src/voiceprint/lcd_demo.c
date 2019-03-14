/*for lcd*/
#include <stdio.h>
#include <string.h>
#include "stdbool.h"
#include "fpioa.h"
#include "lcd.h"
#include "sysctl.h"
#include "nt35310.h"
#include "gpio_detect.h"
#include "lcd_demo.h"
#include "sdcard.h"
#include "gpio_detect.h"
#include "config.h"

static void io_set_power(void)
{
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V18);
}

static void lcd_io_mux_init(void)
{
    fpioa_set_function(CONFIG_GPIO_LCD_DC_PIN, CONFIG_GPIO_LCD_SPI_DC_FUN);
    fpioa_set_function(CONFIG_GPIO_LCD_CS_PIN, CONFIG_GPIO_LCD_SPI_CS_FUN);
    fpioa_set_function(CONFIG_GPIO_LCD_WR_PIN, CONFIG_GPIO_LCD_SPI_CLK_FUN);
    sysctl_set_spi0_dvp_data(1);
}

void lcd_show_init()
{
    lcd_io_mux_init();
    io_set_power();
    lcd_init();
    lcd_set_direction(DIR_YX_RLUD);
    lcd_clear(WHITE);
}

#define LCD_MAX_LINE_NUM 12
#define LCD_STRING_MAX_LEN 100

static char g_lcd_last_show_string[LCD_MAX_LINE_NUM][LCD_STRING_MAX_LEN]={0};

//外部设置的告警信息
#define DEMO_OPRATION_MORMAL "Demo Operation Normal!"

static char g_lcd_set_warn_msg[LCD_STRING_MAX_LEN] = {DEMO_OPRATION_MORMAL};

void lcd_set_warning_message(char *warnmsg)
{
    if(NULL != warnmsg)
        strcpy(g_lcd_set_warn_msg,warnmsg);
    else
        strcpy(g_lcd_set_warn_msg,DEMO_OPRATION_MORMAL);
}

/*line 1*/
static char g_lcd_show_switch_mode_red[SWITCH_KEY_IDLE+1][LCD_STRING_MAX_LEN]={
                                             {"COLLECT FEATURE,press key to start!"},
                                             {"RECOGNITION,press key to start!"},
                                             {"DEL FEATURE,press key to delete!"},
                                             {"DEL RECOGNITION,press key to delete!"},
#ifdef INCLUDE_WEBRTC_VAD
                                             {"VAD UP,press key to adjust!"},
                                             {"VAD DOWN,press key to adjust!"},
#ifdef INCLUDE_PLAYBACK
                                             {"PLAYBACK,press key to start!"},
                                             {"UNKOWN, please switch to 0~6!"}
#else
                                             {"UNKOWN, please switch to 0~5!"}
#endif
#else
#ifdef INCLUDE_PLAYBACK
                                             {"PLAYBACK,press key to start!"},
                                             {"UNKOWN, please switch to 0~4!"}
#else

                                             {"UNKOWN, please switch to 0~3!"}
#endif
#endif
};

static char g_lcd_show_switch_mode_blue[SWITCH_KEY_IDLE+1][LCD_STRING_MAX_LEN]={
                                             {"COLLECT FEATURE,processing..."},
                                             {"RECOGNITION,processing..."},
                                             {"DEL FEATURE,processing..."},
                                             {"DEL RECOGNITION,processing..."},
#ifdef INCLUDE_WEBRTC_VAD
                                             {"VAD UP,processing..."},
                                             {"VAD DOWN,processing..."},
#ifdef INCLUDE_PLAYBACK
                                             {"PLAYBACK,processing..."},
                                             {"UNKOWN, please switch to 0~6!"}
#else
                                             {"UNKOWN, please switch to 0~5!"}
#endif
#else
#ifdef INCLUDE_PLAYBACK
                                             {"PLAYBACK,processing..."},
                                             {"UNKOWN, please switch to 0~4!"}
#else

                                             {"UNKOWN, please switch to 0~3!"}
#endif
#endif
};

#define X_START 16
#define Y_START 18

extern int32_t g_feature_file_total;
extern int32_t g_recognition_file_total;

extern bool get_current_recognition_cos_and_alfa_distance(float cos_alfa_array[CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER][3]);

void lcd_draw_voiceprint()
{
    char g_lcd_show_string[LCD_MAX_LINE_NUM][LCD_STRING_MAX_LEN];
    int switch_focus = (int)get_switch_focus();
    int switch_pressed = (int)get_switch_status_pressed();

    memset(g_lcd_show_string,0,LCD_STRING_MAX_LEN);
    /*line 1 infomation*/
    if(sd_get_init_result() != true)
    {
        sprintf(g_lcd_show_string[0],"Please insert FAT32 SD and reset!");
        if(strcmp(g_lcd_show_string[0],g_lcd_last_show_string[0]) != 0)
        {
            strcpy(g_lcd_last_show_string[0],g_lcd_show_string[0]);
            lcd_draw_string(X_START,Y_START,g_lcd_last_show_string[0],RED);
        }
        return;
    }
    else
    {
#ifdef INCLUDE_WEBRTC_VAD
        sprintf(g_lcd_show_string[0],"SD card:ON     VAD sens:%d",get_vad_sensibility());
#else
        sprintf(g_lcd_show_string[0],"SD card:ON     VAD sens:disable");
#endif
        if(strcmp(g_lcd_show_string[0],g_lcd_last_show_string[0]) != 0)
        {
            lcd_draw_string(X_START,Y_START,g_lcd_last_show_string[0],WHITE);
            strcpy(g_lcd_last_show_string[0],g_lcd_show_string[0]);
            lcd_draw_string(X_START,Y_START,g_lcd_last_show_string[0],BLUE);
        }
    }

    /*line 2 information*/
    if(strcmp(g_lcd_set_warn_msg,g_lcd_last_show_string[1]) != 0)
    {
        lcd_draw_string(X_START,Y_START*2,g_lcd_last_show_string[1],WHITE);
        strcpy(g_lcd_last_show_string[1],g_lcd_set_warn_msg);
        if(strcmp(g_lcd_last_show_string[1],DEMO_OPRATION_MORMAL) == 0)
        {
            lcd_draw_string(X_START,Y_START*2,g_lcd_last_show_string[1],BLUE);
        }
        else
        {
            lcd_draw_string(X_START,Y_START*2,g_lcd_last_show_string[1],RED);
        }
    }

    /*line 3 show current feature number and regoginition num*/
    sprintf(g_lcd_show_string[2],"feature num:%04d recognition num:%04d",g_feature_file_total,g_recognition_file_total);
    if(strcmp(g_lcd_show_string[2],g_lcd_last_show_string[2]) != 0)
    {
        lcd_draw_string(X_START,Y_START*3,g_lcd_last_show_string[2],WHITE);
        strcpy(g_lcd_last_show_string[2],g_lcd_show_string[2]);
        lcd_draw_string(X_START,Y_START*3,g_lcd_last_show_string[2],BLUE);
    }

    /*line 4 information*/
    /*indicate processing...*/
    if(switch_pressed != SWITCH_KEY_IDLE)
    {
        if(strcmp(g_lcd_show_switch_mode_blue[switch_pressed],g_lcd_last_show_string[3]) != 0)
        {
            lcd_draw_string(X_START,Y_START*4,g_lcd_last_show_string[3],WHITE);
            strcpy(g_lcd_last_show_string[3],g_lcd_show_switch_mode_blue[switch_pressed]);
            lcd_draw_string(X_START,Y_START*4,g_lcd_last_show_string[3],BLUE);
        }
    }
    else //only focus
    {
        if(strcmp(g_lcd_show_switch_mode_red[switch_focus],g_lcd_last_show_string[3]) != 0)
        {
            lcd_draw_string(X_START,Y_START*4,g_lcd_last_show_string[3],WHITE);
            strcpy(g_lcd_last_show_string[3],g_lcd_show_switch_mode_red[switch_focus]);
            lcd_draw_string(X_START,Y_START*4,g_lcd_last_show_string[3],RED);
        }
    }
    /*line 6 7 8 9 10 11*/
    float index_cos_alfa_array[CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER][3];
    if((g_feature_file_total > 0) && (g_recognition_file_total > 0) && (true == get_current_recognition_cos_and_alfa_distance(index_cos_alfa_array)))
    {
        //line 5 show max string
        sprintf(g_lcd_show_string[4],"cos distance match max items:");
        if(strcmp(g_lcd_show_string[4],g_lcd_last_show_string[4]) != 0)
        {
            lcd_draw_string(X_START,Y_START*5,g_lcd_last_show_string[4],WHITE);
            strcpy(g_lcd_last_show_string[4],g_lcd_show_string[4]);
            lcd_draw_string(X_START,Y_START*5,g_lcd_last_show_string[4],PURPLE);
        }
        //line 6, 7, 8
        for(int i = 0; i < 6; i+=2)
        {
            if(i == 0)
                sprintf(g_lcd_show_string[5+i/2],"1st:%f(%04d) 2nd:%f(%04d)",index_cos_alfa_array[i][1],(int)index_cos_alfa_array[i][0],
                                                                 index_cos_alfa_array[i+1][1],(int)index_cos_alfa_array[i+1][0]);
            else if(i == 2)
                sprintf(g_lcd_show_string[5+i/2],"3rd:%f(%04d) 4th:%f(%04d)",index_cos_alfa_array[i][1],(int)index_cos_alfa_array[i][0],
                                                                 index_cos_alfa_array[i+1][1],(int)index_cos_alfa_array[i+1][0]);
            else
                sprintf(g_lcd_show_string[5+i/2],"5th:%f(%04d) 6th:%f(%04d)",index_cos_alfa_array[i][1],(int)index_cos_alfa_array[i][0],
                                                                 index_cos_alfa_array[i+1][1],(int)index_cos_alfa_array[i+1][0]);
            if(strcmp(g_lcd_show_string[5+i/2],g_lcd_last_show_string[5+i/2]) != 0)
            {
                lcd_draw_string(X_START,Y_START*(6+i/2),g_lcd_last_show_string[5+i/2],WHITE);
                strcpy(g_lcd_last_show_string[5+i/2],g_lcd_show_string[5+i/2]);
                lcd_draw_string(X_START,Y_START*(6+i/2),g_lcd_last_show_string[5+i/2],BLUE);
            }
        }

        //line 9 show max string
        sprintf(g_lcd_show_string[8],"cos distance match min items:");
        if(strcmp(g_lcd_show_string[8],g_lcd_last_show_string[8]) != 0)
        {
            lcd_draw_string(X_START,Y_START*9,g_lcd_last_show_string[8],WHITE);
            strcpy(g_lcd_last_show_string[8],g_lcd_show_string[8]);
            lcd_draw_string(X_START,Y_START*9,g_lcd_last_show_string[8],PURPLE);
        }

        //line 10 ,11, 12
        for(int i = 0; i < 6; i+=2)
        {
            if(i == 0)
                sprintf(g_lcd_show_string[9+i/2],"6th:%f(%04d) 5th:%f(%04d)",index_cos_alfa_array[6+i][1],(int)index_cos_alfa_array[6+i][0],
                                                                 index_cos_alfa_array[6+i+1][1],(int)index_cos_alfa_array[6+i+1][0]);
            else if(i == 2)
                sprintf(g_lcd_show_string[9+i/2],"4th:%f(%04d) 3rd:%f(%04d)",index_cos_alfa_array[6+i][1],(int)index_cos_alfa_array[6+i][0],
                                                                 index_cos_alfa_array[6+i+1][1],(int)index_cos_alfa_array[6+i+1][0]);
            else
                sprintf(g_lcd_show_string[9+i/2],"2nd:%f(%04d) 1st:%f(%04d)",index_cos_alfa_array[6+i][1],(int)index_cos_alfa_array[6+i][0],
                                                                 index_cos_alfa_array[6+i+1][1],(int)index_cos_alfa_array[6+i+1][0]);
            if(strcmp(g_lcd_show_string[9+i/2],g_lcd_last_show_string[9+i/2]) != 0)
            {
                lcd_draw_string(X_START,Y_START*(10+i/2),g_lcd_last_show_string[9+i/2],WHITE);
                strcpy(g_lcd_last_show_string[9+i/2],g_lcd_show_string[9+i/2]);
                lcd_draw_string(X_START,Y_START*(10+i/2),g_lcd_last_show_string[9+i/2],BLUE);
            }
        }
    }
}
