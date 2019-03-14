#include "math.h"
#include "fft.h"
//精度0.0001弧度
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define NULL ((void *)0)

float SIN_TAB[FFT_N + FFT_N / 4];
int BITREV[FFT_N];

void make_sintbl(int n, float *sintbl)
{
	int i;
	static float pi = (float) M_PI;

	int n2 = n / 2;
	int n4 = n / 4;
	int n8 = n / 8;
	float t = sinf(pi / n);
	float dc = 2 * t * t;
	float ds = sqrt(dc * (2 - dc));

	t = 2 * dc;
	float c = sintbl[n4] = 1;
	float s = sintbl[0] = 0;

	for (i = 1; i < n8; i++) {
		c -= dc;
		dc += t * c;
		s += ds;
		ds -= t * s;
		sintbl[i] = s;
		sintbl[n4 - i] = c;
	}
	if (n8 != 0)
		sintbl[n8] = sqrtf(0.5f);
	for (i = 0; i < n4; i++)
		sintbl[n2 - i] = sintbl[i];
	for (i = 0; i < n2 + n4; i++)
		sintbl[i + n2] = -sintbl[i];
}

void make_bitrev(int n, int *bitrev)
{
	int j = 0;
	int n2 = n / 2;
	int i = 0;
	int k;

	for (;;) {
		bitrev[i] = j;
		if (++i >= n)
			break;
		k = n2;
		while (k <= j) {
			j -= k;
			k /= 2;
		}
		j += k;
	}
}

int myfft(float *x, float *y, int n)
{
	static int last_n; /* previous n */
	static int *bitrev; /* bit reversal table */
	static float *sintbl; /* trigonometric function table */
	int i, j, k2, inverse;
	float s;
	int n4 = n / 4;
	float t, c, dx, dy;
	int k, h, d, ik;

	/* preparation */
	if (n < 0) {
		n = -n;
		inverse = 1; /* inverse transform */
	} else
		inverse = 0;
	if (n != last_n || n == 0) {
		last_n = n;
		if (sintbl != NULL)
			return 1;
		if (bitrev != NULL)
			return 1;
		if (n == 0)
			return 0; /* free the memory */
		sintbl = SIN_TAB;
		bitrev = BITREV;
		if (sintbl == NULL || bitrev == NULL) {
			//Error("%s in %f(%d): out of memory\n", __func__, __FILE__, __LINE__);
			return 1;
		}
		make_sintbl(n, sintbl);
		make_bitrev(n, bitrev);
	}
	/* bit reversal */
	for (i = 0; i < n; i++) {
		j = bitrev[i];
		if (i < j) {
			t = x[i];
			x[i] = x[j];
			x[j] = t;
			t = y[i];
			y[i] = y[j];
			y[j] = t;
		}
	}
	/* transformation */
	for (k = 1; k < n; k = k2) {
		h = 0;
		k2 = k + k;
		d = n / k2;
		for (j = 0; j < k; j++) {
			c = sintbl[h + n4];
			if (inverse)
				s = -sintbl[h];
			else
				s = sintbl[h];
			for (i = j; i < n; i += k2) {
				ik = i + k;
				dx = s * y[ik] + c * x[ik];
				dy = c * y[ik] - s * x[ik];
				x[ik] = x[i] - dx;
				x[i] += dx;
				y[ik] = y[i] - dy;
				y[i] += dy;
			}
			h += d;
		}
	}
	if (inverse) {
		/* divide by n in case of the inverse transformation */
		for (i = 0; i < n; i++) {
			x[i] /= n;
			y[i] /= n;
		}
	}
	return 0; /* finished successfully */
}

//傅里叶逆变换
void ifft(int N, float *real, float *imag)
{
	int i = 0;

	for (i = 0; i < N; i++) {
		imag[i] = -imag[i];
		real[i] = real[i];
	}

	myfft(real, imag, N);

	for (i = 0; i < N; i++) {
		imag[i] = -imag[i];
		real[i] = real[i];
	}

	for (i = 0; i < N; i++) {
		imag[i] = -imag[i] / N;
		real[i] = real[i] / N;
	}
}
