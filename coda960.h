#ifndef INCLUDE_CODA960_H
#define INCLUDE_CODA960_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "common.h"

#define NV12_REQBUF_CNT  1
#define H264_REQBUF_CNT  2


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
    int              framerate;
    int              bitrate;
    int              num_bframes;
};



int coda_open(struct Coda_inst *i);
void coda_close(struct Coda_inst *i);

int coda_init_nv12(struct Coda_inst *i);
int coda_init_h264(struct Coda_inst *i);
int coda_set_control(struct Coda_inst *i);

int coda_queue_buf_h264(struct Coda_inst *i, unsigned int index);
int coda_queue_buf_nv12(struct Coda_inst *i, unsigned int index);

int coda_stream_act(struct Coda_inst *i, unsigned int type, unsigned int action);

int coda_dequeue_nv12(struct Coda_inst *i, unsigned int *indx);
int coda_dequeue_h264(struct Coda_inst *i, unsigned int *indx,
                      unsigned int *finished, unsigned int *bytesused,
                      unsigned int *buf_flags);

#endif /* INCLUDE_CODA960_H */