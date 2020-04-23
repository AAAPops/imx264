/*
 * V4L2 Codec decoding example application
 * Kamil Debski <k.debski@samsung.com>
 *
 * Main file of the application
 *
 * Copyright 2012 Samsung Electronics Co., Ltd.
 * Copyright (c) 2015 Linaro Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * https://www.rtings.com/tv/learn/chroma-subsampling
 * https://ru.wikipedia.org/wiki/Цветовая_субдискретизация
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <arm_neon.h>

#include "common.h"

int clear_all(struct _instance *i) {

    if (i->in_file.ptr)
        fclose(i->in_file.ptr);

    if (i->out_file.ptr)
        fclose(i->out_file.ptr);

    if (i->in_buff_ptr)
        free(i->in_buff_ptr);

    if (i->out_buff_ptr)
        free(i->out_buff_ptr);

    return 0;
}


void print_usage(char *name)
{
    printf("ver.%s \n", VERSION);
    printf("Usage: %s  -f stream.yuy2 -d /output/dir -w width -h height [-n]\n", name);
    printf("\t-f  yuy2/yuyv file to convert \n");
    printf("\t-d  a directory to save file 'pic-WxH.nv12' in \n");
    printf("\t-w  frame width\n");
    printf("\t-h  frame height\n");
    printf("\t-n  number of frames to convert (default: all) \n");
    printf("\n");
}


int parse_args(struct _instance *inst, int argc, char **argv)
{
    int c;
    int f = -1;
    int d = -1;
    inst->n_frames_to_convert = 0;

    while ((c = getopt(argc, argv, "f:d:w:h:n:")) != -1) {
        switch (c) {
            case 'f':
                f = 1;
                strcpy(inst->in_file.name, optarg);
                break;
            case 'd':
                d = 1;
                strcpy(inst->out_file.name, optarg);
                break;
            case 'w':
                inst->width = strtol(optarg, NULL, 10);
                break;
            case 'h':
                inst->height = strtol(optarg, NULL, 10);
                break;
            case 'n':
                inst->n_frames_to_convert = strtol(optarg, NULL, 10);
                inst->n_frames_to_conv_str = optarg;
                break;
            default:
                err("Bad argument");
                return -1;
        }
    }

    if ( f != 1 ) {
        err("Set correct file name");
        return -1;
    } else if ( d != 1 ) {
        err("Set correct directory name");
        return -1;
    } else if (inst->width < 1) {
        err("Set correct frame width");
        return -1;
    } else if (inst->height < 1) {
        err("Set correct frame height");
        return -1;
    }

    if( d == 1)
        sprintf(inst->out_file.name, "%s/tmp-pic-%dx%d.nv12",
            inst->out_file.name, inst->width, inst->height);

    return 0;
}


int open_in_file(struct _instance *i) {

    i->in_file.ptr = fopen(i->in_file.name,"rb");
    if( !i->in_file.ptr )
        return -1;

    return 0;
}

int prepare_buffs(struct _instance *i) {
    // Размер картинки по ширине должен быть кратен 4
    // Размер картинки по высоте должен быть кратен 2

    uint32_t n_macro_pix = i->width * i->height / MACROPIX;

    uint32_t YCrCb_444 = n_macro_pix * MPIX444_SZ;
    info("YCrCb 4:4:4 picture size = %d bytes", YCrCb_444);

    uint32_t YCrCb_422 = n_macro_pix * MPIX422_SZ;
    info("YCrCb 4:2:2 picture size = %d bytes", YCrCb_422);

    uint32_t YCrCb_420 = n_macro_pix * MPIX420_SZ;
    info("YCrCb 4:2:0 picture size = %d bytes", YCrCb_420);

    i->in_buff_sz  = YCrCb_422;
    i->in_buff_ptr = (char *)calloc(i->in_buff_sz, sizeof(char));
    if( !i->in_buff_ptr ) {
        perror("calloc()");
        return -1;
    }


    i->out_buff_sz = YCrCb_420;
    i->out_buff_ptr = (char *)calloc(i->out_buff_sz, sizeof(char));
    if( !i->out_buff_ptr ) {
        perror("calloc()");
        return -1;
    }

    return 0;
}


int yuyv_to_nv12_neon(char *in_buff_ptr, size_t in_buff_sz,
                 char *out_buff_ptr, size_t out_buff_sz,
                 uint16_t width, uint16_t height)
{
    // Sanity checks first
    {
        if (width % 4 != 0) {
            err("Frame width must be a multiple of 4!");
            return -1;
        }
        if (height % 2 != 0) {
            err("Frame height must be a multiple of 2!");
            return -1;
        }

        u_int8_t macropix = 8;
        uint32_t n_macro_pix = width * height / macropix;

        uint8_t mpix422_sz = 16;
        uint32_t YCrCb_422_sz = n_macro_pix * mpix422_sz;
        if (in_buff_sz != YCrCb_422_sz) {
            err("Input buffer size must be exactly %d bytes", YCrCb_422_sz);
            return -1;
        }

        uint8_t mpix420_sz = 12;
        uint32_t YCrCb_420_sz = n_macro_pix * mpix420_sz;
        if (out_buff_sz < YCrCb_420_sz) {
            err("Output buffer size must be at least %d bytes", YCrCb_420_sz);
            return -1;
        }
    }

    // Main converter body
    int line_n, chunk_n;

    uint8_t *Y_plane_start = (uint8_t*)out_buff_ptr;
    size_t Y_offset = 0;
    uint8_t *CbCr_plane_start = (uint8_t*)(out_buff_ptr + width * height);
    size_t CbCr_offset = 0;

    size_t in_buff_offset = 0;

    int yuyv_line_len = (width / 2) * 4;
    uint8x16x2_t chunk_128x2;

    struct timeval  tv;
    gettimeofday(&tv, NULL);
    double time_begin = ((double)tv.tv_sec) * 1000 +
                        ((double)tv.tv_usec) / 1000;

    for( line_n = 0; line_n < height; line_n++ ) {
        for( chunk_n = 0; chunk_n < yuyv_line_len / 32 ; chunk_n++)
        {
            in_buff_offset = yuyv_line_len * line_n + 32 * chunk_n;
            chunk_128x2 = vld2q_u8( (uint8_t*)in_buff_ptr + in_buff_offset);

            vst1q_u8(Y_plane_start + Y_offset, chunk_128x2.val[0]);
            Y_offset += 16;

            if( line_n % 2 == 0 ) {
                vst1q_u8(CbCr_plane_start + CbCr_offset, chunk_128x2.val[1]);
                CbCr_offset += 16;
            }

        }

        if( yuyv_line_len % 32 != 0 ) {
            uint8_t bytes_left = yuyv_line_len % 32;
            in_buff_offset += 32;

            for( uint8_t iter = 0; iter < bytes_left; iter += 4 ) {

                Y_plane_start[Y_offset] = in_buff_ptr[in_buff_offset + 0];
                Y_offset++;

                Y_plane_start[Y_offset] = in_buff_ptr[in_buff_offset + 2];
                Y_offset++;

                if (line_n % 2 == 0) {
                    CbCr_plane_start[CbCr_offset] = in_buff_ptr[in_buff_offset + 1];
                    CbCr_offset++;
                    CbCr_plane_start[CbCr_offset] = in_buff_ptr[in_buff_offset + 3];
                    CbCr_offset++;
                }

                in_buff_offset += 4;
            }
        }

    }

    gettimeofday(&tv, NULL);
    double time_end = ((double)tv.tv_sec) * 1000 +
                      ((double)tv.tv_usec) / 1000 ;
    dbg("Execute time 'yuyv_to_nv12()' = %f(ms)", time_end - time_begin);

    dbg("Total lines in picture = %d", line_n);
    dbg("Y' plane start pos: 0, end pos: %d [0x%x]", Y_offset - 1, Y_offset - 1);
    dbg("CbCr plane start pos: %d, end pos: %d [0x%x]",
        width * height, width * height + CbCr_offset - 1,
        width * height + CbCr_offset - 1);

    return 0;
}


int yuyv_to_nv12(char *in_buff_ptr, size_t in_buff_sz,
        char *out_buff_ptr, size_t out_buff_sz,
        uint16_t width, uint16_t height)
{
    // Sanity checks first
    {
        if (width % 4 != 0) {
            err("Frame width must be a multiple of 4!");
            return -1;
        }
        if (height % 2 != 0) {
            err("Frame height must be a multiple of 2!");
            return -1;
        }

        u_int8_t macropix = 8;
        uint32_t n_macro_pix = width * height / macropix;

        uint8_t mpix422_sz = 16;
        uint32_t YCrCb_422_sz = n_macro_pix * mpix422_sz;
        if (in_buff_sz != YCrCb_422_sz) {
            err("Input buffer size must be exactly %d bytes", YCrCb_422_sz);
            return -1;
        }

        uint8_t mpix420_sz = 12;
        uint32_t YCrCb_420_sz = n_macro_pix * mpix420_sz;
        if (out_buff_sz < YCrCb_420_sz) {
            err("Output buffer size must be at least %d bytes", YCrCb_420_sz);
            return -1;
        }
    }


    size_t iter;

    uint16_t yuyv_line_len = (width / 2) * 4;
    uint16_t curr_line_n = 0;

    size_t Y_offset = 0;
    size_t CbCr_offset = width * height;

    struct timeval  tv;
    gettimeofday(&tv, NULL);
    double time_begin = ((double)tv.tv_sec) * 1000 +
                        ((double)tv.tv_usec) / 1000;

    for (iter = 0; iter < in_buff_sz; iter += 4) {
        out_buff_ptr[Y_offset++] = in_buff_ptr[iter + 0];
        out_buff_ptr[Y_offset++] = in_buff_ptr[iter + 2];

        if( iter % yuyv_line_len == 0)
            curr_line_n++;

        if (curr_line_n % 2 == 1) {
            out_buff_ptr[CbCr_offset++] = in_buff_ptr[iter + 1];
            out_buff_ptr[CbCr_offset++] = in_buff_ptr[iter + 3];
        }

    }
    gettimeofday(&tv, NULL);
    double time_end = ((double)tv.tv_sec) * 1000 +
                      ((double)tv.tv_usec) / 1000 ;
    dbg("Execute time 'yuyv_to_nv12()' = %f(ms)", time_end - time_begin);

    dbg("Total lines in picture = %d", curr_line_n);
    dbg("Y' plane start pos: 0, end pos: %ld", Y_offset - 1);
    dbg("CbCr plane start pos: %d, end pos: %ld",
            width * height, CbCr_offset - 1);

    return 0;
}



int read_data(struct _instance *i) {
    uint32_t n_read;

    n_read = fread(i->in_buff_ptr, i->in_buff_sz, 1, i->in_file.ptr);
    if( feof(i->in_file.ptr) != 0 ) {
        info("Get the End Of File");
        return 1;
    }
    if( n_read != 1 ) {
        err("fread() = %d. Error read of file", n_read);
        return -1;
    }

    dbg("Read from file: %ld bytes", i->in_buff_sz);

    return 0;
}


int save_to_file(struct _instance *i) {
    size_t n_write;

    if ( ! i->out_file.ptr ) {
        i->out_file.ptr = fopen(i->out_file.name, "wb");
        if (!i->out_file.ptr) {
            err("Can not create file '%s'", i->out_file.name);
            return -1;
        }
    }


    n_write = fwrite(i->out_buff_ptr, i->out_buff_sz, 1, i->out_file.ptr);
    if (n_write != 1) {
        err("Can not save data in file %s", i->out_file.name);
        return -1;
    }
    dbg("Write to file: %ld bytes", i->out_buff_sz);

    return 0;
}


int main_loop(struct _instance *inst)
{
    int iter;
    int ret;

    for(iter = 0; iter < 1000; iter++) {
        ret = read_data(inst);
        if( ret == -1 ) return -1;  // -1 Common error of file reading
        if( ret ==  1 ) return  0;  //  1 Get End of File

        ret = yuyv_to_nv12_neon(inst->in_buff_ptr, inst->in_buff_sz,
                inst->out_buff_ptr,  inst->out_buff_sz,
                inst->width, inst->height);
        if( ret != 0 ) return -1;

        ret = save_to_file(inst);
        if( ret != 0 ) return -1;
    }

    return 0;
}


int main(int argc, char **argv)
{
    struct _instance  inst;
    int ret;

	if( argc == 1) {
        print_usage(argv[0]);
        return 1;
	}

	memzero(inst);
	ret = parse_args(&inst, argc, argv);
	if( ret ) {
		print_usage(argv[0]);
		return -1;
	}
	info("input file: %s, %dx%d, frames: %s",
         inst.in_file.name,
         inst.width, inst.height,
         inst.n_frames_to_convert == 0 ? "All" : inst.n_frames_to_conv_str );
    info("out_file.name = %s", inst.out_file.name);

    ret = open_in_file(&inst);
    if( ret )
        goto err;

    ret = prepare_buffs(&inst);
    if( ret )
        goto err;


    ret = main_loop(&inst);
    if( ret )
        goto err;


    clear_all(&inst);
    info("Exit: success");
	return 0;
err:
    clear_all(&inst);
    info("Exit: error");
	return -1;
}
