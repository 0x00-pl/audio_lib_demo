#include <dmac.h>
#include <fft.h>
#include <string.h>
#include <math.h>
#include <kpu.h>
#include <atomic.h>
#include "speech_recog.h"
#include <printf.h>
//#include <matrix.include>
#include "stdio.h"
#include "sysctl.h"
#include "post_args.h"
#include "lcd_demo.h"
#include "gpio_detect.h"
#include "sdcard.h"
#include "config.h"

/*noise*/
int16_t current_window[CONFIG_PROCESS_FRAME_SIZE+CONFIG_SMOOTH_STEP_SIZE]={0};

static float   feature_map[CONFIG_FEATURES_NUM+CONFIG_FEATURES_STEP][CONFIG_MEL_DEMENSION]={0};
static float   feature_map_norm[CONFIG_FEATURES_NUM][CONFIG_MEL_DEMENSION];
static uint8_t ai_input[CONFIG_MEL_DEMENSION][CONFIG_FEATURES_NUM];/*matrix.T*/
static uint8_t *ai_output;

static float avg_pool_result[CONFIG_KPU_OUT_C*CONFIG_KPU_OUT_H];
static float avg_pool_result_transposed[CONFIG_KPU_OUT_C*CONFIG_KPU_OUT_H];
static float current_audio_frame_feature[CONFIG_COS_VECTOR_DEMENSION];
static float fft_power[CONFIG_MEL_FFT_SIZE/2];
static float current_frame_mel_power[CONFIG_MEL_DEMENSION];

static fft_data_t current_frame[CONFIG_MEL_FFT_SIZE/2];

static kpu_task_t task;

extern spinlock_t ai_lock;
extern spinlock_t fft_lock;

static void fft_int_to_complex(fft_data_t* output, int16_t* start)
{
    for(int i = 0; i < CONFIG_MEL_FFT_SIZE/2; i++)
    {
        output[i].R1 = (*(start + i * 2));
        output[i].I1 = 0;
        output[i].R2 = (*(start + i * 2 + 1));
        output[i].I2 = 0;
    }
}

static void matrix_clip(float* features, uint32_t len, float min, float max, float multi)
{
    for(int j = 0; j < len; j++)
    {
        if(features[j] < min)
        {
            features[j] = min*multi;
        }
        else if(features[j] > max)
        {
            features[j] = max*multi;
        }
        else
        {
            features[j] = features[j]*multi;
        }
    }
    return;
}
unsigned int systick_current_millis()
{
    return (unsigned int)(read_csr(mcycle)/(sysctl_clock_get_freq(SYSCTL_CLOCK_CPU)/1000));
}

void delay_ms(unsigned int ms)
{
    unsigned int currentMs = systick_current_millis();
    while(1)
    {
        if(systick_current_millis()-currentMs > ms) return;
    }
}


extern float mel_bank[CONFIG_MEL_DEMENSION][CONFIG_MEL_FFT_SIZE/2+1];
extern void soft_calc_fft(float *input, float *power);
#include "stdlib.h"
static bool get_energy(fft_data_t* frame, float* output)
{
#ifndef INCLUDE_SOFT_FFT
    static fft_data_t fft_res[CONFIG_MEL_FFT_SIZE/2];
#endif

#ifndef INCLUDE_SOFT_FFT
    spinlock_lock(&fft_lock);
    fft_complex_uint16_dma(FFT_DMA_1, FFT_DMA_2, 0x155, FFT_DIR_FORWARD, (void*)frame, 512, (void*)fft_res);
    spinlock_unlock(&fft_lock);
    //to power, freq power
    for (int i = 0; i < CONFIG_MEL_FFT_SIZE/4; i++) /*0 ~ 255*/
    {
        float p1 = (float)fft_res[i].R1 * fft_res[i].R1 + (float)fft_res[i].I1 * fft_res[i].I1;
        float p2 = (float)fft_res[i].R2 * fft_res[i].R2 + (float)fft_res[i].I2 * fft_res[i].I2;
     #if 1
        if((abs(fft_res[i].R1) > 32760) || (abs(fft_res[i].R2) > 32760)
            || (abs(fft_res[i].I1) > 32760) || (abs(fft_res[i].I2) > 32760))
        {
            printf("overflow!!!!!\r\n");
            printf("%d %d %d %d\r\n",fft_res[i].R1,fft_res[i].R2,fft_res[i].I1,fft_res[i].I2);
//             while(1){};
        }
     #endif
        fft_power[i * 2]     = p1 * 0.001953125; /*1.0/512 * power*/
        fft_power[i * 2 + 1] = p2 * 0.001953125;
    }
#else //software fft
    float fft_soft_input[512];
    for(int i = 0; i < 256; i++)
    {
        fft_soft_input[2*i]   = frame[i].R1;
        fft_soft_input[2*i+1] = frame[i].R2;
    }
    soft_calc_fft(fft_soft_input,fft_power);
#endif
    for(int i = 0; i < CONFIG_MEL_DEMENSION; i++)/*mel filter demension*/
    {
        output[i] = 0;
    }
    /*multi mel filter*/
    for(int i = 0; i < CONFIG_MEL_DEMENSION;i++) /*power index for freq*/
    {
        for(int j = 0; j < CONFIG_MEL_FFT_SIZE/2; j++)
        {
            if(mel_bank[i][j] != 0)
                output[i] += mel_bank[i][j]*fft_power[j];
        }
    }
    for(int i = 0; i < CONFIG_MEL_DEMENSION;i++)
    {
        if(output[i]<=1e-10){
            output[i] = 1e-10;
        }
#ifndef INCLUDE_SOFT_FFT
        output[i] = log(output[i]*(1<<10)); //hardware shift
#else
        output[i] = log(output[i]);
#endif
    }
    return true;
}


void features_init()
{
    for(int i=0;i < CONFIG_FEATURES_NUM;i++)
    {
        for(int j=0;j<CONFIG_MEL_DEMENSION;j++)
        {
            feature_map[i][j] = 0;
        }
    }
}

float feature_mean(int start_index, int len, float feature_map[CONFIG_FEATURES_NUM][CONFIG_MEL_DEMENSION])
{
    volatile float feat_mean = 0;

    for(int i = start_index;i < start_index+len; i++)
    {
        for(int j = 0; j < CONFIG_MEL_DEMENSION; j++)
        {
            feat_mean += feature_map[i][j];
        }
    }
    feat_mean = feat_mean/(len*CONFIG_MEL_DEMENSION);
    return feat_mean;
}

float feature_std(int start_index, int len,float mean, float feature_map[CONFIG_FEATURES_NUM][CONFIG_MEL_DEMENSION])
{
    float sum = 0;
    volatile float sqrt_value=0;

    for(int i = start_index;i < start_index+len; i++)
    {
        for(int j = 0; j < CONFIG_MEL_DEMENSION;j++)
        {
            sum += (feature_map[i][j] - mean)*(feature_map[i][j] - mean);
        }
    }
    sum = sum/(len*CONFIG_MEL_DEMENSION);
    sqrt_value = sqrt(sum);
    return sqrt_value;
}

static void feature_Normalize(int start_index, int len,
                       float feature_map[CONFIG_FEATURES_NUM][CONFIG_MEL_DEMENSION],
                       float feature_map_norm[CONFIG_FEATURES_NUM][CONFIG_MEL_DEMENSION])
{
    volatile float feat_mean = feature_mean(start_index,len,feature_map);
    volatile float std = feature_std(start_index,len,feat_mean,feature_map);

    for(int i = start_index;i < start_index+len; i++)
    {
        for(int j = 0; j < CONFIG_MEL_DEMENSION;j++)
        {
            feature_map_norm[i][j] = (feature_map[i][j] - feat_mean)/std;
        }
        matrix_clip(feature_map_norm[i], CONFIG_MEL_DEMENSION, -8, 8, 16);
    }

    return;
}

static int feature_index = CONFIG_FEATURES_STEP;

static void add_feature(float feature_map[CONFIG_FEATURES_NUM+CONFIG_FEATURES_STEP][CONFIG_MEL_DEMENSION], float current_frame_mel_power[CONFIG_MEL_DEMENSION])
{
    for (int i = 0; i < CONFIG_MEL_DEMENSION; i++)
    {
        feature_map[feature_index][i] = current_frame_mel_power[i];/*add to end*/
    }

    feature_index++;
}
void speech_recog_init()
{
    memset(current_window, 0, sizeof(current_window));
}

volatile bool g_on_speech_ai_finish_flag = false;

int on_speech_ai_finish(void* args)
{
    spinlock_unlock(&ai_lock);
	ai_output = (uint8_t *)task.dst;
    g_on_speech_ai_finish_flag = true; //debug
    return 0;
}

void kpu_module_init()
{
	kpu_task_t* kpu_task_gencode_output_init(kpu_task_t* task);
	kpu_task_gencode_output_init(&task);
	task.dma_ch = AI_DMA;
	task.src = (uint64_t *)ai_input;

	task.callback = on_speech_ai_finish;
    kpu_task_gencode_output_init(&task);
}

float* cal_avg_pool_result(float *avg_pool_result, uint8_t *ai_output){
    float scale = task.output_scale;
    float bias = task.output_bias;
    float sum = 0;

    for(uint32_t i=0; i<CONFIG_KPU_OUT_C*CONFIG_KPU_OUT_H; i++){
        sum = 0;
        for(uint32_t j=0;j<CONFIG_POOLING_SIZE;j++){
            sum += (float)ai_output[j+i*CONFIG_POOLING_SIZE];
        }
        sum *=(scale*0.015625);
        sum +=bias;
        avg_pool_result[i] = sum;
    }
    return avg_pool_result;
}

float* cal_teanspose(float* avg_pool_result, float* avg_pool_src) {
    for(int i = 0; i < CONFIG_KPU_OUT_C;i ++)
    {
        for(int j = 0; j < CONFIG_KPU_OUT_H; j++)
        {
            avg_pool_result[j*CONFIG_KPU_OUT_C+i] = avg_pool_src[i*CONFIG_KPU_OUT_H+j];
        }
    }
    return avg_pool_result;
}

float cal_vector_dot_1024(float* vec1, float* vec2){
    float result = 0;
    for(uint32_t i=0; i<CONFIG_KPU_OUT_C*CONFIG_KPU_OUT_H; i++){
        result += vec1[i]*vec2[i];
        }
    return result;
}

float* cal_vector_mul_matrix_1024_128(float* result, float* vec, float* matrix){
    for(uint32_t res_i=0; res_i<CONFIG_KPU_OUT_C; res_i++){
        result[res_i] = cal_vector_dot_1024(vec, &matrix[res_i*CONFIG_KPU_OUT_C*CONFIG_KPU_OUT_H]);
    }
    return result;
}

float* cal_vector_add_128(float* result, float* vec1, float* vec2) {
    for(uint32_t i=0; i<CONFIG_COS_VECTOR_DEMENSION; i++) {
        result[i] = vec1[i] + vec2[i];
    }
    return result;
}


float cal_cos_distance_128(float* vec1, float* vec2){
    volatile float dot_sum = 0;
    volatile float asq_sum = 0;
    volatile float bsq_sum = 0;

    for(uint32_t i=0; i<CONFIG_COS_VECTOR_DEMENSION; i++)
    {
        dot_sum += vec1[i]*vec2[i];
        asq_sum += vec1[i]*vec1[i];
        bsq_sum += vec2[i]*vec2[i];
    }

    return dot_sum/sqrt(asq_sum*bsq_sum);
}

float cal_alfa_distance_128(float *vec1, float *vec2){
    volatile float sum = 0;
    for(int i = 0; i < CONFIG_COS_VECTOR_DEMENSION;i++)
    {
        sum += (vec1[i] - vec2[i]) * (vec1[i] - vec2[i]);
    }
    return sqrt(sum);
}

void print_vector_128(float* vec){
    static int reg_num = 0;
    printf("\r\nvector begin %d--------------------------------------------------------\r\n",reg_num++);
    for(uint32_t i=0; i<CONFIG_COS_VECTOR_DEMENSION; i++){
        if(i%8==0) printf("\r\n");
        printf("%f, ", vec[i]);
    }
    printf("\r\nvector end--------------------------------------------------------------------\r\n");
}


void print_relu_128()
{
    printf("relu:\r\n");
    for(int i = 0; i < CONFIG_COS_VECTOR_DEMENSION;i++)
    {
        if(i % 8 == 0) printf("\r\n");
        printf("%f, ",current_audio_frame_feature[i]);
    }
    printf("\r\nend of relu\r\n");
}

#ifdef INCLUDE_PLAYBACK
extern void play_back();
#endif
//将matrix转成string存盘, matrix长度256, 每个浮点预留20位
static char g_write_file_float_string_feature[CONFIG_COS_VECTOR_DEMENSION*CONFIG_VECTOR_FLOAT_STRING_LEN];
static char g_read_file_float_string_feature[CONFIG_COS_VECTOR_DEMENSION*CONFIG_VECTOR_FLOAT_STRING_LEN];

static void feature_float_array_to_fstring(float *featrue, char *fsting)
{
    char sbuf[CONFIG_VECTOR_FLOAT_STRING_LEN];

    memset(fsting,0,CONFIG_COS_VECTOR_DEMENSION*CONFIG_VECTOR_FLOAT_STRING_LEN);

    for(int i = 0; i < CONFIG_COS_VECTOR_DEMENSION; i++)
    {
        if((i !=0) && (i % 8) == 0)
        {
            strcat(fsting,"\r\n");
        }
        memset(sbuf,0,CONFIG_VECTOR_FLOAT_STRING_LEN);
        if(isnan(featrue[i]))
        {
            sprintf(sbuf,"%f, ",0.0);
        }
        else
        {
            sprintf(sbuf,"%f, ",featrue[i]);
        }
        strcat(fsting,sbuf);
    }
}

//从文件里读出来后,转成的数组
static float g_read_file_float_array_feature[CONFIG_COS_VECTOR_DEMENSION];
static void feature_float_string_to_float_array(char *fstring, float *feature)
{
    char one_float_char[CONFIG_VECTOR_FLOAT_STRING_LEN];
    uint32_t begin_offset = 0;
    uint32_t end_offset = 0;

    for(int i = 0; i < CONFIG_COS_VECTOR_DEMENSION; i++)
    {
        //丢掉无用字符
        while((fstring[begin_offset]==' ')  ||
              (fstring[begin_offset]=='\r') ||
              (fstring[begin_offset]=='\n') ||
              (fstring[begin_offset]==','))
        {
            begin_offset++;
            if(begin_offset >= CONFIG_COS_VECTOR_DEMENSION*CONFIG_VECTOR_FLOAT_STRING_LEN)
            {
                return;//已经到数组边界,退出
            }
        }

        if(fstring[begin_offset]==0)
        {
            return;//字符串结束符,退出
        }

        end_offset = begin_offset;

        //找有效浮点字符
        while((fstring[end_offset] != ' ')  &&
              (fstring[end_offset] != '\r') &&
              (fstring[end_offset] != '\n') &&
              (fstring[end_offset] != ','))
        {
            end_offset++;
            if(end_offset >= CONFIG_COS_VECTOR_DEMENSION*CONFIG_VECTOR_FLOAT_STRING_LEN)
            {
                return;//已经到数组边界,退出
            }
            if(fstring[begin_offset]==0)
            {
                return;//字符串结束符,退出
            }
        }

        memset(one_float_char,0,CONFIG_VECTOR_FLOAT_STRING_LEN);

        for(int j = 0; j < end_offset-begin_offset;j++) one_float_char[j] = fstring[begin_offset+j];
        feature[i] = atof(one_float_char);
        begin_offset = end_offset;
    }
}

static float g_cos_and_alfa_distance[MAX_FEATURE_COUNT][2]; //余弦距离和ALFA距离,最多计算MAX_FEATURE_COUNT次

static float g_cos_and_alfa_distance_bubble_sort[MAX_FEATURE_COUNT][2];//冒泡排序后的结果

static char  g_write_recognition_string[MAX_FEATURE_COUNT * 35]; //cos+alfa 限制在35字节

extern int32_t g_feature_file_total;
extern int32_t g_recognition_file_total;

static void bubble_sort(float src[MAX_FEATURE_COUNT][2], float dst[MAX_FEATURE_COUNT][2])
{
    float tmp;

    int32_t bubble_sort_num = (g_feature_file_total >= 12) ? g_feature_file_total:12;

    for(int i = 0; i < bubble_sort_num;i++)
    {
        dst[i][0] = src[i][0];
        dst[i][1] = src[i][1];
    }

    for(int i = 0; i < bubble_sort_num-1; i++)
    {
        for(int j = 0; j < bubble_sort_num -1 -i;j++)
        {
            if(dst[j][0] < dst[j+1][0]) //余弦距离从大到小
            {
                tmp = dst[j][0];
                dst[j][0] = dst[j+1][0];
                dst[j+1][0] = tmp;
            }
#if 0
            if(dst[j][1] > dst[j+1][1]) //alfa距离从小到大
            {
                tmp = dst[j][1];
                dst[j][1] = dst[j+1][1];
                dst[j+1][1] = tmp;
            }
#endif
        }
    }
}

static bool has_regcognition_flag = false;

//return valid number, cos_alfa_array0~2 is max, 3~5 is min
bool get_current_recognition_cos_and_alfa_distance(float cos_alfa_array[CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER][3]) //三个下标,第一个指出对比的是哪个feature, 第二是cos, 3 for alfa
{
    int32_t bubble_sort_num = (g_feature_file_total >= CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER) ? g_feature_file_total:CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER;

    for(int i = 0; i < CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            cos_alfa_array[i][j] = 0;
        }
    }

    if(has_regcognition_flag == false)
        return false;
    else
        has_regcognition_flag = false;//set flag false
    bubble_sort(g_cos_and_alfa_distance,g_cos_and_alfa_distance_bubble_sort);

    for(int i = 0; i < bubble_sort_num;i++)
    {
        for(int j = 0; j < CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER/2; j++)
        {
            if(g_cos_and_alfa_distance[i][0] == g_cos_and_alfa_distance_bubble_sort[j][0])
            {
                cos_alfa_array[j][0] = i+1;//计算下标
                cos_alfa_array[j][1] = g_cos_and_alfa_distance_bubble_sort[j][0];
                cos_alfa_array[j][2] = g_cos_and_alfa_distance_bubble_sort[j][1];
            }
        }
    }
#if 0
    printf("\r\n");
    for(int i =0; i<bubble_sort_num;i++)
        printf("g_cos_and_alfa_distance[%d][0] = %f\r\n",i,g_cos_and_alfa_distance[i][0]);
    printf("\r\n");

    printf("\r\n");
    for(int i =0; i<bubble_sort_num;i++)
        printf("g_cos_and_alfa_distance_bubble_sort[%d][0] = %f\r\n",i,g_cos_and_alfa_distance_bubble_sort[i][0]);
    printf("\r\n");
#endif
    for(int i = 0; i < bubble_sort_num;i++)
    {
        for(int j = CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER-1; j >= CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER/2; j--)
        {
            if(g_cos_and_alfa_distance[i][0] == g_cos_and_alfa_distance_bubble_sort[bubble_sort_num-1-(CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER-1-j)][0])//倒数是最小
            {
                cos_alfa_array[j][0] = i+1;
                cos_alfa_array[j][1] = g_cos_and_alfa_distance_bubble_sort[bubble_sort_num-1-(CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER-1-j)][0];
                cos_alfa_array[j][2] = g_cos_and_alfa_distance_bubble_sort[bubble_sort_num-1-(CONFIG_COS_AFLA_DISTANCE_SORT_NUMBER-1-j)][1];
            }
        }
    }
#if 0
    printf("\r\n");
    for(int i =0; i<12;i++)
        printf("cos_alfa_array[%d] = %d,%f\r\n",i,(int)cos_alfa_array[i][0],(float)cos_alfa_array[i][1]);
    printf("\r\n");
#endif
    return true;
}

void do_ai_finish(int cb(float[128]))
{

    cal_avg_pool_result(avg_pool_result,ai_output);//65536 ai_output 64*128*8
    cal_teanspose(avg_pool_result_transposed, avg_pool_result);
    cal_vector_mul_matrix_1024_128(current_audio_frame_feature, avg_pool_result_transposed,(float *)arg_dense_matrix);
    cal_vector_add_128(current_audio_frame_feature, current_audio_frame_feature, arg_dense_add);
//     print_vector_128(current_audio_frame_feature);
    cb(current_audio_frame_feature);

//     /*sdcard operation*/
//     if(get_switch_status_pressed() == AUDIO_CAPTURE)
//     {
//         int32_t before_write_file_cnt = sdcard_get_feature_dir_file_count();
//         int32_t after_write_file_cnt = 0;
//         /*save feature*/
//         feature_float_array_to_fstring(current_audio_frame_feature,g_write_file_float_string_feature);
//         sdcard_write_feature_file(g_write_file_float_string_feature,strlen(g_write_file_float_string_feature));
//
//         after_write_file_cnt = sdcard_get_feature_dir_file_count();
//         if(after_write_file_cnt == before_write_file_cnt+1) //已经写进去了
//         {
//             //回读比较
//             sdcard_read_feature_file(sdcard_get_feature_dir_file_count(),g_read_file_float_string_feature, CONFIG_COS_VECTOR_DEMENSION*CONFIG_VECTOR_FLOAT_STRING_LEN);
//             if(strcmp(g_write_file_float_string_feature,g_read_file_float_string_feature) != 0)
//             {
//                 lcd_set_warning_message("audio feature file write error!");
//                 sdcard_delete_last_feature_file();//比较错误，删掉文件，本次无效
//             }
//             else
//             {
//                 lcd_set_warning_message(NULL);//set normal
//             }
//         }
//         else
//         {
//             lcd_set_warning_message("audio feature file write error!");
//         }
//         set_switch_status_finish();
//          /*set continue flag*/
//     }
//     else if(get_switch_status_pressed() == AUDIO_RECOGNITION)
//     {
//         int feature_count = sdcard_get_feature_dir_file_count();
//
//         if(feature_count >= 1)
//         {
//             memset(g_write_recognition_string,0,MAX_FEATURE_COUNT*35);
//             for(int i = 0; i < MAX_FEATURE_COUNT; i++)
//             {
//                 g_cos_and_alfa_distance[i][0] = 0;
//                 g_cos_and_alfa_distance[i][1] = 0;
//             }
//
//             if(feature_count > MAX_FEATURE_COUNT) feature_count = MAX_FEATURE_COUNT;
//             /*save recognize result*/
//             for(int i = 0; i < feature_count;i++)
//             {
//                 char one_cos_alfa[35];
//                 if(0 == sdcard_read_feature_file(i+1,g_read_file_float_string_feature, CONFIG_COS_VECTOR_DEMENSION*CONFIG_VECTOR_FLOAT_STRING_LEN))
//                 {
//                     feature_float_string_to_float_array(g_read_file_float_string_feature,g_read_file_float_array_feature);
//                     g_cos_and_alfa_distance[i][0] = cal_cos_distance_128(g_read_file_float_array_feature,current_audio_frame_feature);
//                     g_cos_and_alfa_distance[i][1] = cal_alfa_distance_128(g_read_file_float_array_feature,current_audio_frame_feature);
//
//                     memset(one_cos_alfa,0,35);
//                     sprintf(one_cos_alfa,"%04d: %f, %f\r\n",i+1,g_cos_and_alfa_distance[i][0],g_cos_and_alfa_distance[i][1]);
//                     strcat(g_write_recognition_string,one_cos_alfa);
//                     lcd_set_warning_message(NULL);
//                 }
//                 else
//                 {
//                     lcd_set_warning_message("Read feature file error!");
//                     goto read_feature_fail;
//                 }
//             }
//             sdcard_write_recognition_file(g_write_recognition_string, strlen(g_write_recognition_string));
//             has_regcognition_flag = true;
//         }
//         else
//         {
//             lcd_set_warning_message("No feature file!");
//         }
//         //sdcard_write_recognition_file(char * feature, uint32_t len)
// read_feature_fail:
//         set_switch_status_finish();
//      }
//     //voiceprint_demo_process();
 }

void do_ai_unfinish()
{
    lcd_draw_voiceprint();
    if(get_switch_status_pressed() == DELETE_LAST_AUDIO_FEATURE)
    {
        if(0 != sdcard_delete_last_feature_file())
        {
            lcd_set_warning_message("Delete feature file error!");
        }
        else
        {
            lcd_set_warning_message(NULL);
        }
        set_switch_status_finish();
    }
    else if(get_switch_status_pressed() == DELETE_LAST_AUDIO_RECOGNITION)
    {
        if(0 != sdcard_delete_last_recognition_file())
        {
             lcd_set_warning_message("Delete recognition file error!");
        }
        else
        {
            lcd_set_warning_message(NULL);
        }
        set_switch_status_finish();
    }
#ifdef INCLUDE_PLAYBACK
    else if(get_switch_status_pressed() == AUDIO_PLAYBACK)
    {
        printf("start playback...\r\n");
        play_back();
        printf("end playback...\r\n");
        delay_ms(CONFIG_I2S_RCV_MS_TIME);
        set_switch_status_finish();
    }
#endif
}

void cpu_regcog(int cb(float[128]))
{
    while(g_on_speech_ai_finish_flag != true){
        do_ai_unfinish();
    }
    g_on_speech_ai_finish_flag = false;

    do_ai_finish(cb);
}

int8_t features_transpose[CONFIG_MEL_DEMENSION][CONFIG_FEATURES_NUM];
static void do_recog(float feature_map[CONFIG_FEATURES_NUM][CONFIG_MEL_DEMENSION])
{
    volatile float input_scale = 1.0;
    volatile float input_bias = -128.0;

    for(int i = 0 ; i < CONFIG_FEATURES_NUM; i++)
    {
        for(int j = 0; j < CONFIG_MEL_DEMENSION; j++)
        {
            ai_input[j][i] = (feature_map[i][j]-input_bias)/input_scale;
        }
    }

    spinlock_lock(&ai_lock);
    kpu_start(&task);
}

int16_t speech_frame_padding_out[CONFIG_MEL_FFT_SIZE];
static void frame_padding(int16_t *input, int len, int16_t *out)
{
    if(len > CONFIG_MEL_FFT_SIZE) return;

    for(int i = 0; i < len; i++)
    {
        out[i] = input[i];
    }

    if(len < CONFIG_MEL_FFT_SIZE)
    {
        for (int i = len; i < CONFIG_MEL_FFT_SIZE;i++)
        {
            out[i] = 0;
        }
    }
}

/*y(t)=x(t)−αx(t−1)*/
static int16_t PreEmphasise(int16_t *input, int len, int16_t last_value)
{
    for(int i = 0; i < len; i++)
    {
        int16_t t = input[i];
        input[i] = (float)(input[i]) - CONIFG_PREEMPHASIS_COEF * (float)last_value;
        last_value = t;
    }
    return last_value;
}

// each 160 sample
void speec_recog_frame(int16_t* new_input_step_frame,bool firstframe)
{
    static int16_t last_value = 0;

    /*reset PreEmphasise para*/
    if(firstframe == true)
    {
        last_value = 0;
    }

    last_value = PreEmphasise(new_input_step_frame, CONFIG_SMOOTH_STEP_SIZE, last_value);

    /*fill first frame*/
    if(firstframe == true)
    {
        for(int i = 0; i < CONFIG_SMOOTH_STEP_SIZE*3; i++)
        {
            if(i < (CONFIG_SMOOTH_STEP_SIZE + CONFIG_PROCESS_FRAME_SIZE))
                current_window[i] = new_input_step_frame[i%CONFIG_SMOOTH_STEP_SIZE];
            else
                break;
        }
    }

    memcpy(&current_window[CONFIG_PROCESS_FRAME_SIZE], new_input_step_frame, CONFIG_SMOOTH_STEP_SIZE * 2);

    for(int i = 0; i < CONFIG_PROCESS_FRAME_SIZE;i++) current_window[i] = current_window[CONFIG_SMOOTH_STEP_SIZE+i]; // shift current window

    frame_padding(current_window, CONFIG_PROCESS_FRAME_SIZE, speech_frame_padding_out);  //400==>512
    fft_int_to_complex(current_frame, speech_frame_padding_out);
    get_energy(current_frame, current_frame_mel_power);
    add_feature(feature_map, current_frame_mel_power);

    if(feature_index == CONFIG_FEATURES_NUM+CONFIG_FEATURES_STEP)
    {
        for(int i = 0; i < CONFIG_FEATURES_NUM; i++)
        {
            for(int j = 0; j < CONFIG_MEL_DEMENSION;j++)
            {
                feature_map[i][j] = feature_map[i+CONFIG_FEATURES_STEP][j];
            }
        }
        feature_index -= CONFIG_FEATURES_STEP;

        feature_Normalize(0, CONFIG_FEATURES_NUM, feature_map, feature_map_norm);
        do_recog(feature_map_norm);
    }
}
