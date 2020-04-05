#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

#include "common.h"
#include "args.h"
#include "webcam.h"
#include "server.h"
#include "coda960.h"



int mainloop(struct Webcam_inst* wcam_i,
        struct Srv_inst* srv_i, struct Coda_inst* coda_i)
{
    struct timeval tv;

    int iter;
    int ret;
    int fds_max = 0;


    for( iter = 0; iter < wcam_i->frame_count; iter++) {
        if( wcam_i->frame_count == 0 )
            iter--;

        fd_set read_fds;
        FD_ZERO(&read_fds);

        FD_SET(wcam_i->wcam_fd, &read_fds);
        if( fds_max < wcam_i->wcam_fd )
            fds_max = wcam_i->wcam_fd;

        FD_SET(srv_i->peer_fd, &read_fds);
        if( fds_max < srv_i->peer_fd )
            fds_max = srv_i->peer_fd;


        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        ret = select(fds_max + 1, &read_fds, NULL, NULL, &tv);
        if( ret == -1 ) {
            err("select()");
            return -1;
        } else if( ret == 0 ) {
            err("select() timeout to get one more frame from Webcam");
            return -1;
        }

        // Read data from client
        if( FD_ISSET(srv_i->peer_fd, &read_fds) ) {
            int data_len = srv_get_data(srv_i);
            if( data_len == -1 ) {
                err("Some error with client [%m]");
                return -1;
            } else if( data_len == 0 ) {
                info("Client closed connection");
                return 0;
            }
        }

        // Read data from webcam
        if( FD_ISSET(wcam_i->wcam_fd, &read_fds) ) {
            ret = wcam_process_new_frame(wcam_i);
            if (ret == -1)
                return -1;

            ret = srv_send_data(srv_i, wcam_i->nv12_buff.start, wcam_i->nv12_buff.length);
            if (ret == -1)
                return -1;
        }
    }

    return 0;
}


int main(int argc, char **argv) {
    struct Webcam_inst wcam_inst;
    MEMZERO(wcam_inst);
    struct Srv_inst srv_inst;
    MEMZERO(srv_inst);
    struct Coda_inst coda_inst;
    MEMZERO(coda_inst);

    int ret;

    ret = pars_args(argc, argv, &wcam_inst, &srv_inst, &coda_inst);
    if( ret != 0 )
        goto err;

    ret = srv_tcp_start(&srv_inst);
    if( ret != 0 )
        goto err;

    ret = wcam_open(&wcam_inst);
    if( ret != 0 )
        goto err;

    ret = wcam_init(&wcam_inst);
    if( ret != 0 )
        goto err;

    ret = wcam_start_capturing(&wcam_inst);
    if( ret != 0 )
        goto err;

    // Main loop start here!!!
    ret = mainloop(&wcam_inst, &srv_inst, &coda_inst);
    if( ret != 0 )
        goto err;


    printf("\n");
    wcam_stop_capturing(&wcam_inst);
    wcam_uninit(&wcam_inst);
    wcam_close(&wcam_inst);
    srv_stop(&srv_inst);

    return 0;
err:
    printf("\n");
    wcam_stop_capturing(&wcam_inst);
    wcam_uninit(&wcam_inst);
    wcam_close(&wcam_inst);
    srv_stop(&srv_inst);

    return -1;
}