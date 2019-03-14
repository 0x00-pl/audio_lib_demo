#include <stdio.h>
#include <stdint.h>



void on_audio_pos(int angle, int theta, int64_t val)
{
    printf("angle:%04d eng:%ld\n", angle, val);
}


int system_run(void cb(int angle, int theta, int64_t val));
int main(){
    system_run(on_audio_pos);
    return 0;
}
