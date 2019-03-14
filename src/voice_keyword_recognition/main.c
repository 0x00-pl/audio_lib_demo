#include "vkr_system.h"
#include <stdio.h>


int main(void)
{
    system_init();
    printf("init ok\n");
    while (1)
    {
        float ret = cpu_regcog();
        printf("dot val: %f\t %s\n",ret, (ret>2?"#####":(ret>1?"++++":(ret>0?("==="):(ret>-1?"..":(ret>-2?".":" "))))));
    }
    return 0;
}
