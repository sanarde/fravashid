/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_APP_H
#define FRAVASHID_APP_H

#include <cfgd/cfgd.h>
#include <stdbool.h>
#include <stddef.h>

#include "bus.h"
#include "loop.h"
#include "output.h"
#include "settings.h"
#include "video.h"
#include "wayland.h"
#include "x11.h"

enum fravashid_backend {
    FRAVASHID_BACKEND_AUTO,
    FRAVASHID_BACKEND_WAYLAND,
    FRAVASHID_BACKEND_X11,
};

struct fravashid_slot;

struct fravashid_app {
    int epfd;
    cfgd_t *cfgd;
    struct fravashid_settings settings;
    struct fravashid_source config_source;
    struct fravashid_source timer_source;
    struct fravashid_source display_source;
    int timer_fd;
    bool config_dirty;
    enum fravashid_backend backend;
    struct fravashid_wl *wl;
    struct fravashid_x11 *x11;
    struct fravashid_slot **slots;
    size_t slot_count;
    struct fravashid_bus bus;
};

int fravashid_app_open(struct fravashid_app *app, int epfd,
                       enum fravashid_backend backend);
void fravashid_app_close(struct fravashid_app *app);
int fravashid_app_prepare(struct fravashid_app *app);
int fravashid_app_finish(struct fravashid_app *app, int display_readable);
void fravashid_app_config_event(struct fravashid_app *app);
void fravashid_app_timer_event(struct fravashid_app *app);
void fravashid_app_video_event(struct fravashid_app *app, struct fravashid_video *video);
void fravashid_app_step(struct fravashid_app *app, int step);
size_t fravashid_app_current(const struct fravashid_app *app, size_t index,
                             const char **output, const char **path);

#endif
