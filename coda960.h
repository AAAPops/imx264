#ifndef INCLUDE_CODA960_H
#define INCLUDE_CODA960_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "common.h"

#define NV12_BUF_COUNT  1


struct Coda_inst {
    char             coda_name[128];
    int              coda_fd;

    struct Buffer    buff_nv12[10];
    uint8_t          buff_nv12_n;
    int              nv_12_w;
    int              nv_12_h;
    int              nv_12_bpl;

    struct Buffer    buff_264[10];
    uint8_t          buff_264_n;

    int              width;
    int              height;
};



int coda_open(struct Coda_inst *i);
void coda_close(struct Coda_inst *i);

int coda_init_nv12(struct Coda_inst *i);

#endif /* INCLUDE_CODA960_H */