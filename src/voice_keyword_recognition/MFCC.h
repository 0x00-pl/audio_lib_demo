#ifndef _MFCC_H
#define _MFCC_H
//#include "VAD.h"

#define mfcc_fft_point	512			//FFT点数
#define frq_max		(mfcc_fft_point/2)	//最大频率

#define tri_num		64				//三角滤波器个数
#define mfcc_num	64				//MFCC阶数

void get_one_mfcc(float *vc_dat, float *mfcc_p);
void normalize2(float *mfcc_p, u16 frm_num, u16 fbank_num);

#endif
