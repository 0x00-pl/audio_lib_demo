/*******   MFCC.C    *******/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "g_def.h"
#include <math.h>
//#include "ADC.h"
#include "MFCC.h"
//#include "VAD.h"
#include "MFCC_Arg.h"
#include <float.h>
#include "fft.h"



float fft_out[mfcc_fft_point];
float fft_in[mfcc_fft_point];
float fft_in_i[mfcc_fft_point];


float *mfcc_fft(float *dat_buf, u16 buf_len)
{
    u16 i;
    float real, imag;

    if (buf_len > mfcc_fft_point)
        return (void *)0;

    for (i = 0; i < buf_len; i++) {
        fft_in[i] =  *(dat_buf+i);//虚部高位 实部低位
    }
    for (; i < mfcc_fft_point; i++)
        fft_in[i] = 0;//超出部分用0填充
    for (i = 0; i < mfcc_fft_point; i++)
        fft_in_i[i] = 0;

    myfft(fft_in, fft_in_i, mfcc_fft_point);

    for(i=0;i<frq_max + 1;i++)
    {
        real=fft_in[i];
        imag=fft_in_i[i];
    //    printf("%f, %f ",real,imag);

    //    real=real*real+imag*imag;
        real=real*real-imag*imag;
    //    fft_out[i]=sqrtf((float)real);
        fft_out[i] = real;
    //    printf("%f\n",fft_out[i]);
    }
    return fft_out;
}



void get_one_mfcc(float *vc_dat, float *mfcc_p)
{
    float *frq_spct;          //频谱
    float energy = 0;
//    float vc_temp[frame_len]; //语音暂存区
    float pow_spct[tri_num];  //三角滤波器输出对数功率谱
    float pow_spct2[tri_num];
    float temp;
    u16 fc;
    u16 h, i;
    float  *dct_p;
    u16 count = 0;
    u8 try_num = 0;
#if 0
    for (i = 0; i < frame_len; i++) {
        temp = vc_dat[i];
    //    printf("%f \n",temp);
        //加汉明窗 并放大10倍
        vc_temp[i] = temp*hamming320[i];
    //    printf("%f\n",vc_temp[i]);
    }
#endif

//    for(i = 0; i < 512; i++)
//        printf("%f ",vc_dat[i]);

    frq_spct = mfcc_fft(vc_dat, 512);
    for (i = 0; i < frq_max + 1; i++) {
    //    frq_spct[i] *= frq_spct[i];//能量谱
        frq_spct[i] = frq_spct[i] / 2 / frq_max;
        energy += frq_spct[i];
    //    printf("%f ",frq_spct[i]);
    }
    if(0 == energy){
        energy = 0.000001;
    }
 //   printf("%f\n",energy);

    //加三角滤波器
    
    for (h = 0; h < tri_num; h++){
        pow_spct[h] = 0;
        fc = bin64[h];
        for (i = 0; i < bin64[h+2] - bin64[h]; i++){
            pow_spct[h] += (frq_spct[fc]*fbank64[count + i]);
            fc++;
        }
        count += (bin64[h+2] - bin64[h]);
        if(pow_spct[h] < 0.0001)
            pow_spct[h] = 0.0001;
    //    printf("%f ", pow_spct[h]);
    }

    //三角滤波器输出取对数
    for (h = 0; h < tri_num; h++) {
        //USART1_printf("pow_spct[%d]=%d ",h,pow_spct[h]);
        pow_spct2[h] = log(pow_spct[h]);//取对数后 乘100 提升数据有效位数
        try_num = 0;
        while(!(pow_spct2[h] == pow_spct2[h])){
            if(try_num > 10)
                break;
            pow_spct2[h] = log(pow_spct[h]);
            try_num++;
        }
    //    printf("%f ",pow_spct2[h]);
    }
    
    for (h = 0; h < mfcc_num; h++) {
        mfcc_p[h] = pow_spct2[h];
   //     printf("%f ",mfcc_p[h]);
    }
  // printf("\n");
}


float avg2(float *mfcc_p, u16 frm_num, u16 fnum)
{
    u16 i,j;
    float sum = 0.0f;

//    printf("frm_num = %d, (mfcc_num / 3) = %d, type = %d\n", frm_num, (mfcc_num / 3), type);
    for (i = 0; i < frm_num; i++)
        for (j = 0; j < fnum; ++j)
        {
            sum += mfcc_p[i * fnum + j];
        }
        
    return (float)(sum / (frm_num * fnum));
}

float stdev2(float *mfcc_p, float avg1, u16 frm_num, u16 fnum)
{
    u16 i,j;
    double sum = 0.0f;

    for (i = 0; i < frm_num; i++)
        for (j = 0; j < fnum; ++j)
        {
            sum += (mfcc_p[i * fnum + j] - avg1) * (mfcc_p[i * fnum + j] - avg1);
        }

    return (float)(sum / (frm_num * fnum));
}

void normalize2(float *mfcc_p, u16 frm_num, u16 fbank_num)
{
    u16 i, j;
    float avg1,stdev1;

    avg1 = avg2(mfcc_p, frm_num, fbank_num);
    stdev1 = stdev2(mfcc_p, avg1, frm_num, fbank_num);
    stdev1 = sqrtf(stdev1);
//    printf("avg1 = %f, stdev1 = %f", avg1, stdev1);
    if(stdev1 < 0.0001)
        stdev1 = 0.0001;
    for (i = 0; i < frm_num; i++){
        for (j = 0; j < fbank_num; j++)
        {
            mfcc_p[i * fbank_num + j] = (mfcc_p[i * fbank_num + j] - avg1) / stdev1;
 //           printf("%f,", mfcc_p[i * fbank_num  + j]);
        }
 //       printf("\n");
    }
}
