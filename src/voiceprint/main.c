#include "config.h"
#include "voiceprint_system.h"
#include <stdint.h>
#include <stdio.h>

static void print_vector_128(float* vec){
    static int reg_num = 0;
    printf("\r\nvector begin %d--------------------------------------------------------\r\n",reg_num++);
    for(uint32_t i=0; i<128; i++){
        if(i%8==0) printf("\r\n");
        printf("%f, ", vec[i]);
    }
    printf("\r\nvector end--------------------------------------------------------------------\r\n");
}

static int process_features(float features[128]){
    print_vector_128(features);
    return 0;
}

int main(void)
{
    system_init();

    while (1)
    {
        cpu_regcog(process_features);
    }
    return 0;
}
