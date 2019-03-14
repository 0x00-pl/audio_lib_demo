//任意点数FFT 
#ifndef __FFT_H__ 
#define __FFT_H__ 
#include<math.h> 
 
#ifdef FFT_GLOBALS 
	#define FFT_EXT 
#else 
	#define FFT_EXT extern 
#endif 
 
#define  PI 3.14159265 
#define  FFT_POINT    9     //设置点数(此值变，下面的也要变)(0~11) 
#define  SAMPLE_NUM  512	//count[n] 
                            //count[]={1,2,4,8,16,32,64,128,256,512,1024,2048} 
 
FFT_EXT float dataR[SAMPLE_NUM]; 
FFT_EXT float dataI[SAMPLE_NUM];  
#endif 

