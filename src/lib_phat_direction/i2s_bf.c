#include "i2s_bf.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <printf.h>
#include <fft.h>
#include "mic_def.h"
static volatile fft_t *const fft = (volatile fft_t *)FFT_BASE_ADDR;
void (*audio_callback)(int x,int y,int64_t val);
static int32_t full_srp(int64_t *value_out);

#define GCCPHAT_SIZE 512
#define I2S_INPUT_SHIFT_FACTOR 8

#if GCCPHAT_SIZE == 64
#define FFT_POINT FFT_64
#elif GCCPHAT_SIZE == 128
#define FFT_POINT FFT_128
#elif GCCPHAT_SIZE == 256
#define FFT_POINT FFT_256
#elif GCCPHAT_SIZE == 512
#define FFT_POINT FFT_512
#endif

#define FFT_IN_MODE_RIRI 0
#define FFT_IN_MODE_RRRR 1
#define FFT_IN_MODE_RRRRIIII 2
#define FFT_INPUT_DATA_MODE_64Bit 0
#define FFT_INPUT_DATA_MODE_32Bit 1
#define FFT_NOT_USING_DMA 0
#define FFT_USING_DMA 1
#define FFT_SHIFT_FACTOR 0x00

// evil code
#define FFT_RESULT_ADDR (0x42100000 + 0x2000 + (FFT_POINT % 2 == 0 ? 0x800 : 0))
volatile uint64_t *fft_result1 = (volatile uint64_t *)FFT_RESULT_ADDR;

static int32_t buffer_a[GCCPHAT_SIZE][8];
static int32_t buffer_b[GCCPHAT_SIZE][8];
typedef int32_t buffer_t[GCCPHAT_SIZE][8];
static buffer_t* buffer = &buffer_a;
static buffer_t* buffer_dma = &buffer_b;

static void init_fpioa(void)
{
    printk("init fpioa.\n");
    // fpioa_set_function(FPIOA_GPIO4, FUNC_GPIOHS4);
    fpioa_set_function(FPIOA_I2S0_IN_D0, FUNC_I2S0_IN_D0);
    fpioa_set_function(FPIOA_I2S0_IN_D1, FUNC_I2S0_IN_D1);
    fpioa_set_function(FPIOA_I2S0_IN_D2, FUNC_I2S0_IN_D2);
    fpioa_set_function(FPIOA_I2S0_IN_D3, FUNC_I2S0_IN_D3);
    fpioa_set_function(FPIOA_I2S0_WS, FUNC_I2S0_WS);
    fpioa_set_function(FPIOA_I2S0_SCLK, FUNC_I2S0_SCLK);
}


static void init_i2s(void)
{
    printk("init i2s.\n");

    i2s_init(I2S_DEVICE_0, I2S_RECEIVER, 0xFF);

    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_0, RESOLUTION_24_BIT,
                          SCLK_CYCLES_32, TRIGGER_LEVEL_4,
                          STANDARD_MODE);
    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_1, RESOLUTION_24_BIT,
                          SCLK_CYCLES_32, TRIGGER_LEVEL_4,
                          STANDARD_MODE);
    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_2, RESOLUTION_24_BIT,
                          SCLK_CYCLES_32, TRIGGER_LEVEL_4,
                          STANDARD_MODE);
    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_3, RESOLUTION_24_BIT,
                          SCLK_CYCLES_32, TRIGGER_LEVEL_4,
                          STANDARD_MODE);

    sysctl_clock_set_threshold(SYSCTL_THRESHOLD_I2S0, 0x7);
}

int int_dma(void *args)
{
//     float eng[8] = {0};
//     for(int32_t i=0; i<GCCPHAT_SIZE; i++){
//         eng[0] += (float)buffer[i][0]*(float)buffer[i][0];
//         eng[1] += (float)buffer[i][1]*(float)buffer[i][1];
//         eng[2] += (float)buffer[i][2]*(float)buffer[i][2];
//         eng[3] += (float)buffer[i][3]*(float)buffer[i][3];
//         eng[4] += (float)buffer[i][4]*(float)buffer[i][4];
//         eng[5] += (float)buffer[i][5]*(float)buffer[i][5];
//         eng[6] += (float)buffer[i][6]*(float)buffer[i][6];
//         eng[7] += (float)buffer[i][7]*(float)buffer[i][7];
//     }
//     printf("eng: %f %f %f %f %f %f %f %f\n", eng[0], eng[1], eng[2], eng[3], eng[4], eng[5], eng[6], eng[7]);

    static uint64_t last_cycle = 0;
    uint64_t start_cycle = read_csr(mcycle);
//     memcpy(buffer, buffer_dma, sizeof(buffer));
    buffer_t* t=buffer;
    buffer = buffer_dma;
    buffer_dma = t;
    i2s_receive_data_dma(I2S_DEVICE_0, (uint32_t *)*buffer_dma, 8 * GCCPHAT_SIZE, I2S_DMA_CHANNEL);
    int64_t volume;
    full_srp(&volume);
    uint64_t end_cycle = read_csr(mcycle);
    if(start_cycle-last_cycle > 4800000){
        printk("[busy] cycle: %ld/%ld\n", end_cycle-start_cycle, start_cycle-last_cycle);
    }
    last_cycle = start_cycle;
    return 0;
}

static inline void i2s_set_sign_expand_en(i2s_device_number_t device_num, uint32_t enable)
{
    ccr_t u_ccr;
    u_ccr.reg_data = readl(&i2s[device_num]->ccr);
    u_ccr.ccr.sign_expand_en = enable;
    writel(u_ccr.reg_data, &i2s[device_num]->ccr);
}

void audio_init_all(void)
{
    init_fpioa();
    init_i2s();

    //dma
    dmac_set_irq(I2S_DMA_CHANNEL, int_dma, NULL, 4);
    i2s_set_sign_expand_en(I2S_DEVICE_0, 1);
    i2s_receive_data_dma(I2S_DEVICE_0, (uint32_t *)*buffer_dma, 8 * GCCPHAT_SIZE, I2S_DMA_CHANNEL);
    printf("inited\n");
}

#define LOW16(x) ((int16_t)((x)&0xffff))
#define HEI16(x) ((int16_t)(((x) >> 16) & 0xffff))


static double invSqrt(double y)
{
    double xhalf = (double)0.5 * y;
    long long i = *(long long *)(&y);
    i = 0x5FE6EB50C7B537A9LL - (i >> 1); //LL suffix for (long long) type for GCC
    y = *(double *)(&i);
    y = y * ((double)1.5 - xhalf * y * y);

    return y;
}

static uint8_t fft_get_finish_flag(void)
{
    // wait for fft finished,
    // fft finished and read fft_done_status in same cycle will boom.
    uint64_t cycle_end = read_csr(mcycle) + 2400;

    while (read_csr(mcycle) <= cycle_end)
    {
        /* Wait for mcycle reaches cycle_end */
        asm volatile("nop");
    }

    return (uint8_t)fft->fft_status.fft_done_status & 0x01;
}

static void fft_init(uint8_t point, uint8_t mode, uint16_t shift, uint8_t is_dma, uint8_t input_mode, uint8_t data_mode)
{
    fft->fft_ctrl.fft_point = point;
    fft->fft_ctrl.fft_mode = mode;
    fft->fft_ctrl.fft_shift = shift;
    fft->fft_ctrl.dma_send = is_dma;
    fft->fft_ctrl.fft_enable = 1;
    fft->fft_ctrl.fft_input_mode = input_mode;
    fft->fft_ctrl.fft_data_mode = data_mode;
}

static int do_fft(size_t ch, int32_t *result)
{
    sysctl_reset(SYSCTL_RESET_FFT);

    fft_init(FFT_POINT, FFT_DIR_FORWARD,
             FFT_SHIFT_FACTOR, FFT_NOT_USING_DMA,
             FFT_IN_MODE_RIRI, FFT_INPUT_DATA_MODE_64Bit);

    uint64_t data;
    for (size_t i = 0; i < GCCPHAT_SIZE / 2; i++)
    {
        uint64_t d0 = (uint16_t)LOW16((*buffer)[i * 2][ch] >> I2S_INPUT_SHIFT_FACTOR);
        uint64_t d1 = (uint16_t)LOW16((*buffer)[i * 2 + 1][ch] >> I2S_INPUT_SHIFT_FACTOR);
        data = (d1 << 48) | (d0 << 16);

        fft->fft_input_fifo.fft_input_fifo = data;
    }
    while (!fft_get_finish_flag())
        ;

    for (size_t i = 0; i < GCCPHAT_SIZE / 2; i++)
    {
        uint64_t data;
        data = fft_result1[i]; //fft->fft_output_fifo.fft_output_fifo;
        result[i * 2] = data & 0xffffffff;
        result[i * 2 + 1] = (data >> 32) & 0xffffffff;
    }
    return 0;
}

static int do_ifft(int32_t *input, int32_t *result)
{
    sysctl_reset(SYSCTL_RESET_FFT);
    fft_init(FFT_POINT, FFT_DIR_BACKWARD,
             FFT_SHIFT_FACTOR, FFT_NOT_USING_DMA,
             FFT_IN_MODE_RIRI, FFT_INPUT_DATA_MODE_64Bit);

    for (size_t i = 0; i < GCCPHAT_SIZE / 2; i++)
    {
        uint64_t d0 = (uint32_t)input[i * 2];
        uint64_t d1 = (uint32_t)input[i * 2 + 1];
        uint64_t data = (d1 << 32) | d0;
        fft->fft_input_fifo.fft_input_fifo = data;
    }
    while (!fft_get_finish_flag())
        ;

    for (size_t i = 0; i < GCCPHAT_SIZE / 2; i++)
    {
        uint64_t data;
        data = fft_result1[i]; //fft->fft_output_fifo.fft_output_fifo;
        result[i * 2] = data & 0xffffffff;
        result[i * 2 + 1] = (data >> 32) & 0xffffffff;
    }
    return 0;
}

static int mult_conj_norm(int32_t *x, int32_t *x_ref, int32_t *result)
{
    for (size_t i = 0; i < GCCPHAT_SIZE; i++)
    {
        int32_t a = LOW16(x[i] >> 16);
        int32_t b = LOW16(x[i]);
        int32_t c = LOW16(x_ref[i] >> 16);
        int32_t d = -LOW16(x_ref[i]); // conj

        // mult
        double real = a * c - b * d;
        double imag = a * d + b * c;

        // norm
        //      double inv_abs = 256 / (sqrt(real*real+imag*imag)+0.001); // our sqrt is very fast, haha
        double inv_abs = 256 * invSqrt(real * real + imag * imag + 0.001);
        int16_t real_norm = real * inv_abs;
        int16_t imag_norm = imag * inv_abs;
        uint32_t tmp = real_norm & 0x0000ffff;
        result[i] = (tmp << 16) | (imag_norm & 0x0000ffff);
    }
    return 0;
}


static int lowpass_filter_freq(int32_t *fa)
{
    for (uint32_t i = 0; i < GCCPHAT_SIZE; i++)
    {
        if (i > 17 && i <= 512 - 17)
        {
            fa[i] = 0;
        }
        if (i <= 3 && i > 512 - 3)
        {
            fa[i] = 0;
        }
    }
    return 0;
}

static int hann(int32_t ch)
{
    const static int16_t win_hann_tab[GCCPHAT_SIZE] = {
        0, 1, 5, 11, 20, 31, 45, 61,
        79, 100, 124, 150, 178, 209, 242, 278,
        316, 357, 400, 445, 493, 543, 596, 651,
        708, 768, 830, 895, 961, 1031, 1102, 1176,
        1252, 1330, 1411, 1494, 1579, 1667, 1756, 1848,
        1942, 2038, 2137, 2237, 2340, 2445, 2552, 2661,
        2772, 2885, 3000, 3117, 3236, 3358, 3481, 3606,
        3733, 3862, 3993, 4126, 4260, 4397, 4535, 4675,
        4817, 4960, 5105, 5252, 5401, 5551, 5703, 5857,
        6012, 6169, 6327, 6487, 6648, 6811, 6975, 7141,
        7308, 7476, 7646, 7817, 7989, 8163, 8338, 8514,
        8691, 8870, 9049, 9230, 9412, 9595, 9778, 9963,
        10149, 10336, 10523, 10712, 10901, 11092, 11283, 11475,
        11667, 11860, 12054, 12249, 12444, 12640, 12836, 13033,
        13231, 13429, 13627, 13826, 14025, 14225, 14425, 14625,
        14825, 15026, 15227, 15428, 15629, 15830, 16031, 16233,
        16434, 16636, 16837, 17039, 17240, 17441, 17642, 17843,
        18043, 18243, 18443, 18643, 18843, 19041, 19240, 19438,
        19636, 19833, 20030, 20226, 20421, 20616, 20811, 21004,
        21197, 21389, 21581, 21772, 21961, 22150, 22338, 22526,
        22712, 22897, 23082, 23265, 23447, 23629, 23809, 23988,
        24166, 24342, 24518, 24692, 24865, 25037, 25207, 25376,
        25544, 25710, 25875, 26039, 26201, 26361, 26520, 26678,
        26834, 26988, 27141, 27292, 27442, 27589, 27735, 27880,
        28023, 28163, 28303, 28440, 28575, 28709, 28841, 28971,
        29099, 29225, 29349, 29471, 29591, 29710, 29826, 29940,
        30052, 30162, 30270, 30376, 30480, 30581, 30681, 30778,
        30873, 30966, 31057, 31145, 31232, 31316, 31398, 31477,
        31554, 31629, 31702, 31772, 31840, 31906, 31969, 32030,
        32089, 32145, 32199, 32250, 32299, 32346, 32390, 32432,
        32471, 32508, 32543, 32575, 32604, 32632, 32656, 32679,
        32698, 32716, 32731, 32743, 32753, 32760, 32765, 32767,
        32767, 32765, 32760, 32753, 32743, 32731, 32716, 32698,
        32679, 32656, 32632, 32604, 32575, 32543, 32508, 32471,
        32432, 32390, 32346, 32299, 32250, 32199, 32145, 32089,
        32030, 31969, 31906, 31840, 31772, 31702, 31629, 31554,
        31477, 31398, 31316, 31232, 31145, 31057, 30966, 30873,
        30778, 30681, 30581, 30480, 30376, 30270, 30162, 30052,
        29940, 29826, 29710, 29591, 29471, 29349, 29225, 29099,
        28971, 28841, 28709, 28575, 28440, 28303, 28163, 28023,
        27880, 27735, 27589, 27442, 27292, 27141, 26988, 26834,
        26678, 26520, 26361, 26201, 26039, 25875, 25710, 25544,
        25376, 25207, 25037, 24865, 24692, 24518, 24342, 24166,
        23988, 23809, 23629, 23447, 23265, 23082, 22897, 22712,
        22526, 22338, 22150, 21961, 21772, 21581, 21389, 21197,
        21004, 20811, 20616, 20421, 20226, 20030, 19833, 19636,
        19438, 19240, 19041, 18843, 18643, 18443, 18243, 18043,
        17843, 17642, 17441, 17240, 17039, 16837, 16636, 16434,
        16233, 16031, 15830, 15629, 15428, 15227, 15026, 14825,
        14625, 14425, 14225, 14025, 13826, 13627, 13429, 13231,
        13033, 12836, 12640, 12444, 12249, 12054, 11860, 11667,
        11475, 11283, 11092, 10901, 10712, 10523, 10336, 10149,
        9963, 9778, 9595, 9412, 9230, 9049, 8870, 8691,
        8514, 8338, 8163, 7989, 7817, 7646, 7476, 7308,
        7141, 6975, 6811, 6648, 6487, 6327, 6169, 6012,
        5857, 5703, 5551, 5401, 5252, 5105, 4960, 4817,
        4675, 4535, 4397, 4260, 4126, 3993, 3862, 3733,
        3606, 3481, 3358, 3236, 3117, 3000, 2885, 2772,
        2661, 2552, 2445, 2340, 2237, 2137, 2038, 1942,
        1848, 1756, 1667, 1579, 1494, 1411, 1330, 1252,
        1176, 1102, 1031, 961, 895, 830, 768, 708,
        651, 596, 543, 493, 445, 400, 357, 316,
        278, 242, 209, 178, 150, 124, 100, 79,
        61, 45, 31, 20, 11, 5, 1, 0};
    for (uint32_t i = 0; i < GCCPHAT_SIZE; i++)
    {
        int64_t tmp = (int64_t)(*buffer)[i][ch] * win_hann_tab[i];
        (*buffer)[i][ch] = tmp >> 16;
    }
    return 0;
}

static float fmax32(float a, float b){
    return a>b?a:b;
}

static int32_t full_srp(int64_t *value_out)
{
    int32_t m0[GCCPHAT_SIZE];
    int32_t m1[GCCPHAT_SIZE];
    int32_t m2[GCCPHAT_SIZE];
    int32_t m3[GCCPHAT_SIZE];
    int32_t m4[GCCPHAT_SIZE];
    int32_t m5[GCCPHAT_SIZE];
    int32_t m6[GCCPHAT_SIZE];
    int32_t m7[GCCPHAT_SIZE];

    int32_t *m[8] = {m0,
                     m1,
                     m2,
                     m3,
                     m4,
                     m5,
                     m6,
                     m7};

    for (size_t i = 0; i < 8; ++i)
    {
        if (!MIC_EN[i])
            continue;

        hann(i);
        do_fft(i, m[i]);
    }

    static int64_t result[DIRECTION_RES][DIRECTION_RES];
    int32_t tmp[GCCPHAT_SIZE];
    int32_t phat[GCCPHAT_SIZE];

    for (size_t i = 0; i < 8; ++i)
    {
        if (!MIC_EN[i])
            continue;

        for (size_t j = i + 1; j < 8; ++j)
        {
            if (!MIC_EN[j])
                continue;

            mult_conj_norm(m[i], m[j], tmp);
            lowpass_filter_freq(tmp);
            do_ifft(tmp, phat);
            phat[0] = phat[1];
            for (uint32_t x = 0; x < DIRECTION_RES; x++)
            {
                for (uint32_t y = 0; y < DIRECTION_RES; y++)
                {
                    int64_t value = 0;
                    value += HEI16(phat[(*(delay_table[i][j] + x * DIRECTION_RES + y) + GCCPHAT_SIZE) % GCCPHAT_SIZE]);

                    if (value < 1)
                    {
                        value = 1;
                    }
                    result[x][y] += value ;
                }
            }
        }
    }

    float eng[8] = {0};
    for(int32_t i=0; i<8; i++){
        if (!MIC_EN[i])
            continue;
        for(int32_t j=pass_start_idx; j<pass_end_idx; j++){
            float re = HEI16(m[i][j]);
            float im = LOW16(m[i][j]);
            eng[i] += re*re + im*im;
        }
    }
//     printf("eng: %f %f %f %f %f %f %f %f\n", eng[0], eng[1], eng[2], eng[3], eng[4], eng[5], eng[6], eng[7]);


    static uint32_t counter = 0;
    counter++;
    if (counter % 8 == 0)
    {
        counter = 0;

        int64_t maxv = 0;
        int max_x = 0;
        int max_y = 0;
        for (int x = 0; x < DIRECTION_RES; ++x)
        {
            for (int y = 0; y < DIRECTION_RES; ++y)
            {
                if (result[x][y] > maxv)
                {
                    maxv = result[x][y];
                    max_x = x;
                    max_y = y;
                }
                result[x][y] = 0;
            }
        }
        float max_eng = fmax32(fmax32(fmax32(eng[0], eng[1]), fmax32(eng[2],eng[3])), fmax32(eng[4],eng[5]));
        audio_callback(max_x, max_y, max_eng);
    }

    return 0;
}
