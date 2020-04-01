#ifndef INCLUDE_WEBCAM_H
#define INCLUDE_WEBCAM_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

enum io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct buffer {
    void   *start;
    size_t  length;
};


struct Webcam_inst {
    char             wcam_name[128];
    int              wcam_fd;
    int              get_info;
    struct buffer   *buffers;
    uint8_t          n_buffers;

    int              width;
    int              height;
    int              frame_rate;
    int              frame_count;

    //    enum io_method   io;
};



#define MACROPIX       8
#define MPIX444_SZ    24
#define MPIX422_SZ    16
#define MPIX420_SZ    12


#endif /* INCLUDE_WEBCAM_H */