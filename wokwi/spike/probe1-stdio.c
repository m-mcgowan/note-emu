#include "wokwi-api.h"
#include <stdio.h>

void chip_init(void) {
    printf("[probe-1] WASI stdio works\n");
    fprintf(stderr, "[probe-1] stderr works\n");
}
