#ifndef __FFT_H__
#define __FFT_H__

typedef struct complex //复数类型
{
	float real;   //实部
	float imag;   //虚部
} complex;

#define PI 3.1415926535897932384626433832795028841971
#define  FFT_N  512                                                      //定义福利叶变换的点数

void create_sin_tab(void);

///////////////////////////////////////////

int myfft(float *x, float *y, int n);//傅立叶变换 输出也存在数组f中
void ifft(int N, float *real, float *imag); // 傅里叶逆变换
void c_abs(complex f[], float out[], int n);//复数数组取模
////////////////////////////////////////////
#endif
