#include "fm1_output.h"
#include <stdio.h>
#include <stdlib.h>
static int original(int sample) {
    double value=sample/32767.0;
    if(value < -1.0)value=1.0;
    else if(value > 1.0)value=1.0;
    else value=value-value*value*value/3;
    return (int)(value*32767);
}
int main(void) {
    int x,maximum=0;
    for(x=-65536;x<=65536;x++) {
        int delta=fm1_opm_output(x)-original(x);
        if(abs(delta)>maximum)maximum=abs(delta);
        if(abs(delta)>1){fprintf(stderr,"Output mismatch x=%d delta=%d\n",x,delta);return 1;}
    }
    printf("PASS integer cubic output exhaustive comparison; maximum difference=%d LSB\n",maximum);
    return 0;
}
