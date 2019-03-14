#include "mic_def.h"
#include <math.h>
#include <malloc.h>
#include <stdio.h>

#define M_PI 3.14159265358979323846264338327950288

double MIC_POS_X[8] = {0};
double MIC_POS_Y[8] = {0};
double MIC_POS_Z[8] = {0};

bool MIC_EN[8] = {0};

int8_t *delay_table[8][8] = {0};

static double dist3d(double x1, double y1, double z1, double x2, double y2, double z2)
{
    return sqrt((x1 - x2) * (x1 - x2) +
                (y1 - y2) * (y1 - y2) +
                (z1 - z2) * (z1 - z2));
}

void make_mic_array_circle(double radius, size_t count, bool center)
{
    if (center ? count > 7 : count > 8)
        return;

    for (size_t i = 0; i < count; ++i)
    {
        MIC_POS_X[i] = radius * cos(2 * M_PI / count * i);
        MIC_POS_Y[i] = radius * sin(2 * M_PI / count * i);
        MIC_POS_Z[i] = 0;
        MIC_EN[i] = true;
    }

    for (size_t i = count; i < 8; ++i)
    {
        MIC_POS_X[i] = 0;
        MIC_POS_Y[i] = 0;
        MIC_POS_Z[i] = 0;
        MIC_EN[i] = false;
    }

    if (center)
    {
        MIC_POS_X[count] = 0;
        MIC_POS_Y[count] = 0;
        MIC_POS_Z[count] = 0;
        MIC_EN[count] = true;
    }
}

void make_delay_table()
{
    for (size_t i = 0; i < 8; ++i)
    {
        for (size_t j = i + 1; j < 8; ++j)
        {
            // calc each plane
            int8_t *ptr = (int8_t *)malloc(sizeof(int8_t) * DIRECTION_RES * DIRECTION_RES);
            delay_table[i][j] = ptr;

            for (size_t x = 0; x < DIRECTION_RES; ++x)
            {
                for (size_t y = 0; y < DIRECTION_RES; ++y)
                {
                    volatile double pos_x = sin(2 * M_PI * x / DIRECTION_RES);
                    volatile double pos_y = cos(2 * M_PI * x / DIRECTION_RES);
                    volatile double pos_z = sin(M_PI / 2 * y / DIRECTION_RES);

                    volatile double dist1 = dist3d(pos_x, pos_y, pos_z, MIC_POS_X[i], MIC_POS_Y[i], MIC_POS_Z[i]);
                    volatile double dist2 = dist3d(pos_x, pos_y, pos_z, MIC_POS_X[j], MIC_POS_Y[j], MIC_POS_Z[j]);

                    volatile double dist = dist1 - dist2;

                    *(delay_table[i][j] + DIRECTION_RES * x + y) = (int8_t)(dist / SOUND_SPEED * I2S_FS);
                }
            }
        }
    }
}
