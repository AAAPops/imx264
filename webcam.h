#ifndef INCLUDE_WEBCAM_H
#define INCLUDE_WEBCAM_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define REQ_BUFF  2

enum io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct Buffer {
    void   *start;
    size_t  length;
    size_t  bytesused;
};


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



#define MACROPIX       8
#define MPIX444_SZ    24
#define MPIX422_SZ    16
#define MPIX420_SZ    12

int wcam_open(struct Webcam_inst* wcam_i);
int wcam_init(struct Webcam_inst* wcam_i);

int wcam_start_capturing(struct Webcam_inst* i);
int wcam_mainloop(struct Webcam_inst* i, int peer_fd);

void wcam_stop_capturing(struct Webcam_inst* i);
void wcam_uninit(struct Webcam_inst* i);
void wcam_close(struct Webcam_inst* i);

#endif /* INCLUDE_WEBCAM_H */