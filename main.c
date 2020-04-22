#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "common.h"
#include "log.h"
#include "args.h"
#include "webcam.h"
#include "server.h"
#include "coda960.h"
#include "proto.h"


double stopwatch(char* label, double timebegin) {
    struct timeval tv;

    if( timebegin == 0 ) {
        if (label)
            fprintf(stderr, "%s: Start stopwatch... \n", label);
        else
            fprintf(stderr, "Start stopwatch... \n");


        gettimeofday(&tv, NULL);
        double time_begin = ((double) tv.tv_sec) * 1000 +
                            ((double) tv.tv_usec) / 1000;
        return time_begin;
    } else {
        gettimeofday(&tv, NULL);
        double time_end = ((double)tv.tv_sec) * 1000 +
                          ((double)tv.tv_usec) / 1000 ;

        fprintf(stderr, "%s: Execute time = %f(ms) \n",
                label, time_end - timebegin);

        return 0;
    }
}


int run_as_daemon(void) {
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if( pid < 0 ) {
        log_fatal("fork() [%m]");
        exit(EXIT_FAILURE);;
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) { // Parent process has finished executing
        log_debug("Parent process has finished executing");
        exit(0);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */
    log_fp = fopen(LOG_FILE_NAME, "w");
    if (log_fp) {
        log_set_fp(log_fp);
    } else {
        log_warn("fopen() [%m]");
        log_warn("Continue without logging into file '%s'", LOG_FILE_NAME);
    }


    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        log_fatal("setsid() [%m]");
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if( chdir("/") < 0 ) {
        log_fatal("chdir() [%m]");
        exit(EXIT_FAILURE);
    }

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Daemon-specific initialization goes here */
    log_set_quiet(BACKGROUND);

    return 0;
}



int mainloop(struct Webcam_inst* wcam_i,
             struct Srv_inst* srv_i,
             struct Coda_inst* coda_i,
             struct Proto_inst* proto_i)
{
    struct timeval tv;

    int iter;
    int ret;
    int fds_max = 0;
    int total_frames = 0;
    int frame_count;

    unsigned int yuy2_buf_indx;

    frame_count = (wcam_i->frame_count == 0) ?  10 : wcam_i->frame_count;

    while( frame_count-- ) {
        if( wcam_i->frame_count == 0 )
            frame_count = 10;


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
            log_fatal("select() [%m]");
            return -1;
        } else if( ret == 0 ) {
            log_fatal("select() timeout");
            return -1;
        }

        // Read data from client
        if( FD_ISSET(srv_i->peer_fd, &read_fds) ) {
            ret = get_peer_msg(srv_i, proto_i);
            if (ret)
                return -1;
        }

        //
        // Read data from webcam
        //
        if( FD_ISSET(wcam_i->wcam_fd, &read_fds) ) {
            //double time_start = stopwatch(NULL, 0);

            // 1. Извлекаю YUY2 буфер из Web-камеры
            ret = wcam_dequeue_buf(wcam_i, &yuy2_buf_indx);
            if (ret == -1)
                return -1;

            // 2. Конвертирую буфер Web-камеры в NV12 буфер Coda
            ret = yuyv_to_nv12_neon(wcam_i->buffers[yuy2_buf_indx].start,
                                    wcam_i->buffers[yuy2_buf_indx].length,
                                    coda_i->buff_nv12[0].start,
                                    coda_i->buff_nv12[0].length,
                                    wcam_i->width, wcam_i->height);
            if( ret == -1 )
                return -1;

            // 3. Ставлю входной буфер NV12 Coda в очередь на обработку
            ret = coda_queue_buf_nv12(coda_i, 0);
            if (ret == -1)
                return -1;

            // 4. Возвращаю буфер Web-камеры в очередь
            ret = wcam_queue_buf(wcam_i, yuy2_buf_indx);
            if (ret == -1)
                return -1;

            // 5. Извлекаю h264 буфер из Coda
            unsigned int h264_buf_indx;
            unsigned int h264_finished;
            unsigned int h264_bytesused;
            unsigned int h264_buf_flags;

            ret = coda_dequeue_h264(coda_i, &h264_buf_indx,
                    &h264_finished, &h264_bytesused,
                    &h264_buf_flags);
            if (ret == -1)
                return -1;

            // 5.1 Пересылаю h264 данные клиенту
            // Send h264 DATA
            memset(proto_i, 0, sizeof(struct Proto_inst));
            proto_i->cmd = PROTO_CMD_DATA;
            proto_i->status = PROTO_STS_OK;

            proto_i->data = coda_i->buff_264[h264_buf_indx].start;
            proto_i->data_len = h264_bytesused;
            ret = send_peer_msg(srv_i, proto_i);
            if (ret)
                return -1;


            // 6. Извлекаю NV12 буфер из очереди Coda
            unsigned int nv12_buf_indx;
            ret = coda_dequeue_nv12(coda_i, &nv12_buf_indx);
            if( ret == -1 )
                return -1;

            // 7. Возвращаю использованный h264 буфер обратно в очередь Coda
            ret = coda_queue_buf_h264(coda_i, h264_buf_indx);
            if (ret == -1)
                return -1;
        }

        if( srv_i->run_mode == FOREGROUND ) {
            fprintf(stdout, "%05d\b\b\b\b\b", ++total_frames);
            if (total_frames > 99999)
                total_frames = 0;
            fflush(stdout);
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
    struct Proto_inst proto_inst;
    MEMZERO(proto_inst);

    int ret;

    ret = pars_args(argc, argv, &wcam_inst, &srv_inst, &coda_inst);
    if( ret != 0 )
        goto err_1;

    if( srv_inst.run_mode == BACKGROUND) {
        ret = run_as_daemon();
        if (ret != 0)
            goto err_1;
    }

    ret = srv_srv_start(&srv_inst);
    if( ret != 0 )
        goto err_1;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while(1) {
        ret = srv_peer_accept(&srv_inst);
        if (ret)
            goto err;

        ret = proto_handshake(&srv_inst, &proto_inst, &wcam_inst);
        if (ret)
            goto err;

        {
            ret = wcam_open(&wcam_inst);
            if (ret != 0)
                goto err;

            ret = wcam_init(&wcam_inst);
            if (ret != 0)
                goto err;

            ret = wcam_start_capturing(&wcam_inst);
            if (ret != 0)
                goto err;
        }

        {
            coda_inst.width = wcam_inst.width;
            coda_inst.height = wcam_inst.height;
            coda_inst.framerate = wcam_inst.frame_rate;

            ret = coda_open(&coda_inst);
            if (ret != 0)
                goto err;

            ret = coda_init_nv12(&coda_inst);
            if (ret != 0)
                goto err;

            ret = coda_init_h264(&coda_inst);
            if (ret != 0)
                goto err;

            ret = coda_set_control(&coda_inst);
            if (ret != 0)
                goto err;

            int indx;
            for (indx = 0; indx < coda_inst.buff_264_n; indx++) {
                ret = coda_queue_buf_h264(&coda_inst, indx);
                if (ret != 0)
                    goto err;
            }
/*
            for( indx = 0; indx < coda_inst.buff_nv12_n; indx++ ) {
                ret = coda_queue_buf_nv12(&coda_inst, indx);
                if (ret != 0)
                    goto err;
            }
*/

            ret = coda_stream_act(&coda_inst, V4L2_BUF_TYPE_VIDEO_CAPTURE, VIDIOC_STREAMON);
            if (ret != 0)
                goto err;

            ret = coda_stream_act(&coda_inst, V4L2_BUF_TYPE_VIDEO_OUTPUT, VIDIOC_STREAMON);
            if (ret != 0)
                goto err;

        }

        // Main loop start here!!!
        ret = mainloop(&wcam_inst, &srv_inst, &coda_inst, &proto_inst);
        if (ret != 0)
            goto err;


        err:
        printf("\n");
        wcam_stop_capturing(&wcam_inst);
        wcam_uninit(&wcam_inst);
        wcam_close(&wcam_inst);

        coda_stream_act(&coda_inst, V4L2_BUF_TYPE_VIDEO_CAPTURE, VIDIOC_STREAMOFF);
        coda_stream_act(&coda_inst, V4L2_BUF_TYPE_VIDEO_OUTPUT, VIDIOC_STREAMOFF);
        coda_close(&coda_inst);

        srv_peer_stop(&srv_inst);
    }
#pragma clang diagnostic pop

err_1:
    printf("\n");
    wcam_stop_capturing(&wcam_inst);
    wcam_uninit(&wcam_inst);
    wcam_close(&wcam_inst);

    coda_stream_act(&coda_inst, V4L2_BUF_TYPE_VIDEO_OUTPUT, VIDIOC_STREAMOFF);
    coda_stream_act(&coda_inst, V4L2_BUF_TYPE_VIDEO_OUTPUT, VIDIOC_STREAMOFF);
    coda_close(&coda_inst);

    srv_srv_stop(&srv_inst);
    return -1;
}