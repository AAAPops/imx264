#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/wait.h>

#include "../log.h"

#define PipeReadFd  0
#define PipeWriteFd 1


int main(int argc, char **argv)
{
    struct pollfd pfds[2];
    int ret;

    int pipeFD[2];   /* two file descriptors */
    char pipe_buf[500];


    if (pipe(pipeFD) < 0) {
        log_error("Pipe create");
        goto err;
    }

    log_info("......Get DATA loop here.....");    // 1sec = 1000000 usec

    if (pipe(pipeFD) < 0) {
        log_error("Pipe create");
        goto err;
    }

    pid_t child_pid = fork();


    switch (child_pid)
    {
        case -1: /* An error has occurred */
            log_error("Fork Error");
            goto err;
            break;

        case 0: // Child process ----------------------------------------------------------
            log_info("Child: job start");
            close(pipeFD[PipeReadFd]);      /* close the ReadEnd: all done */

            const char *msg = "knock-knock-1111111111111111111111112222222222222222222222111111111111111\n";
            while (1) {
                write(pipeFD[PipeWriteFd], msg, strlen(msg));
                usleep(1000000);
            }

            log_debug("Child: job finish");
            close(pipeFD[PipeWriteFd]);     /* close the ReadEnd: all done */
            exit(0);
            break;

        default: // Parent process ---------------------------------------------------------
            log_info("Parent process: get data from server");
            close(pipeFD[PipeWriteFd]);

            pfds[0].fd = pipeFD[PipeReadFd];
            pfds[0].events = POLLIN;
            pfds[0].revents = 0;

            while (1) {
                ret = poll(pfds, 1, 5*1000);
                if (ret == -1) {
                    log_fatal("poll: [%m]");
                    goto err;

                } else if (ret == 0) {
                    log_fatal("poll: Time out");
                    goto err;
                }

                if (pfds[0].revents & POLLIN) {     // PipeReadEnd
                    if( read(pipeFD[PipeReadFd], &pipe_buf, sizeof(pipe_buf)) > 0 )
                        log_debug("Knock-knock from child: Time to write out first h264 frame from buffer");

                } else { // POLLERR | POLLHUP
                    log_info("PipeReadEnd closed connection");
                    goto err;
                }
            }
    } // Switch end

    err:
    close(pipeFD[PipeReadFd]);
    close(pipeFD[PipeWriteFd]);
    return 0;
}