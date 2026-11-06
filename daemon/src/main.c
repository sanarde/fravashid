/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "app.h"

static int open_signal_fd(void)
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0)
        return -1;
    return signalfd(-1, &mask, SFD_CLOEXEC);
}

static int parse_backend(const char *name, enum fravashid_backend *backend)
{
    if (strcmp(name, "auto") == 0)
        *backend = FRAVASHID_BACKEND_AUTO;
    else if (strcmp(name, "wayland") == 0)
        *backend = FRAVASHID_BACKEND_WAYLAND;
    else if (strcmp(name, "x11") == 0)
        *backend = FRAVASHID_BACKEND_X11;
    else
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    struct fravashid_source signal_source = {FRAVASHID_SRC_SIGNAL};
    enum fravashid_backend backend = FRAVASHID_BACKEND_AUTO;
    struct fravashid_app app;
    int sig_fd;
    int epfd;
    int status = 0;

    if (argc == 3 && strcmp(argv[1], "-b") == 0 && parse_backend(argv[2], &backend) == 0) {
    } else if (argc != 1) {
        fprintf(stderr, "usage: fravashid [-b auto|wayland|x11]\n");
        return 2;
    }

    sig_fd = open_signal_fd();
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (sig_fd < 0 || epfd < 0) {
        fprintf(stderr, "fravashid: %s\n", strerror(errno));
        return 1;
    }
    {
        struct epoll_event ev = {.events = EPOLLIN, .data.ptr = &signal_source};

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, sig_fd, &ev) != 0) {
            fprintf(stderr, "fravashid: epoll: %s\n", strerror(errno));
            return 1;
        }
    }

    if (fravashid_app_open(&app, epfd, backend) != 0) {
        fravashid_app_close(&app);
        return 1;
    }

    for (;;) {
        struct epoll_event event;
        struct fravashid_source *source;
        int n;

        if (fravashid_app_prepare(&app) != 0) {
            fprintf(stderr, "fravashid: lost the display connection\n");
            status = 1;
            break;
        }
        n = epoll_wait(epfd, &event, 1, -1);
        if (n < 0 && errno != EINTR) {
            fprintf(stderr, "fravashid: epoll_wait: %s\n", strerror(errno));
            status = 1;
            break;
        }
        source = n > 0 ? event.data.ptr : NULL;
        if (fravashid_app_finish(&app, source && source->kind == FRAVASHID_SRC_DISPLAY) != 0) {
            fprintf(stderr, "fravashid: lost the display connection\n");
            status = 1;
            break;
        }
        if (!source)
            continue;

        if (source->kind == FRAVASHID_SRC_SIGNAL) {
            struct signalfd_siginfo info;

            if (read(sig_fd, &info, sizeof(info)) == sizeof(info))
                break;
        } else if (source->kind == FRAVASHID_SRC_TIMER) {
            fravashid_app_timer_event(&app);
        } else if (source->kind == FRAVASHID_SRC_CONFIG) {
            fravashid_app_config_event(&app);
        } else if (source->kind == FRAVASHID_SRC_BUS) {
            fravashid_bus_event(&app.bus, (struct fravashid_bus_watch *)source,
                                event.events);
        } else if (source->kind == FRAVASHID_SRC_VIDEO) {
            fravashid_app_video_event(&app, (struct fravashid_video *)source);
        }
    }

    fravashid_app_close(&app);
    return status;
}
