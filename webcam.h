#ifndef INCLUDE_WEBCAM_H
#define INCLUDE_WEBCAM_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "common.h"

#define REQ_BUFF  2

#define MACROPIX       8
#define MPIX444_SZ    24
#define MPIX422_SZ    16
#define MPIX420_SZ    12


struct Webcam_inst {
    char             wcam_name[128];
    int              wcam_fd;
    int              get_info;
    struct Buffer    buffers[10];
    uint8_t          buffers_n;

    struct Buffer    nv12_buff;

    int              width;
    int              height;
    int              frame_rate;
    int              frame_count;
};


int wcam_open(struct Webcam_inst* wcam_i);
int wcam_init(struct Webcam_inst* wcam_i);

int wcam_process_new_frame(struct Webcam_inst* i);

int wcam_start_capturing(struct Webcam_inst* i);
void wcam_stop_capturing(struct Webcam_inst* i);

int wcam_queue_buf(struct Webcam_inst *i, unsigned int index);
int wcam_dequeue_buf(struct Webcam_inst *i, unsigned int *index);

void wcam_uninit(struct Webcam_inst* i);
void wcam_close(struct Webcam_inst* i);

int yuyv_to_nv12_neon(char *in_buff_ptr, size_t in_buff_sz,
                      char *out_buff_ptr, size_t out_buff_sz,
                      uint16_t width, uint16_t height);

#endif /* INCLUDE_WEBCAM_H */