/* SPDX-License-Identifier: GPL-3.0-only */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "image.h"
#include "media.h"
#include "paint.h"
#include "playlist.h"

struct fravashid_slot {
    struct fravashid_app *app;
    struct fravashid_output *output;
    char *spec;
    struct fravashid_playlist list;
    struct fravashid_image image;
    struct fravashid_video *video;
};

static uint32_t *acquire(struct fravashid_app *app, struct fravashid_output *output,
                         int *stride_px)
{
    if (app->backend == FRAVASHID_BACKEND_WAYLAND)
        return fravashid_wl_acquire(output, stride_px);
    return fravashid_x11_acquire(output, stride_px);
}

static void commit(struct fravashid_app *app, struct fravashid_output *output)
{
    if (app->backend == FRAVASHID_BACKEND_WAYLAND)
        fravashid_wl_commit(output);
    else
        fravashid_x11_commit(output);
}

static const char *slot_path(const struct fravashid_slot *slot)
{
    return fravashid_playlist_current(&slot->list);
}

static void slot_paint(struct fravashid_slot *slot)
{
    struct fravashid_app *app = slot->app;
    struct fravashid_output *output = slot->output;
    uint32_t *pixels;
    int stride;

    pixels = acquire(app, output, &stride);
    if (!pixels)
        return;
    if (slot->image.pixels)
        fravashid_paint(pixels, output->width, output->height, stride, &slot->image,
                        app->settings.mode, app->settings.background);
    else
        fravashid_fill(pixels, output->width, output->height, stride,
                       app->settings.background);
    commit(app, output);
}

static void slot_publish(struct fravashid_slot *slot)
{
    struct fravashid_app *app = slot->app;
    const char *path = slot_path(slot);

    fravashid_bus_emit_changed(&app->bus, slot->output->name, path ? path : "");
    if (app->slot_count > 0 && app->slots[0] == slot)
        fravashid_bus_set_seurat_wallpaper(&app->bus,
                                           slot->image.pixels && path ? path : "");
}

static void slot_stop_video(struct fravashid_slot *slot)
{
    if (!slot->video)
        return;
    fravashid_video_close(slot->video, slot->app->epfd);
    slot->video = NULL;
}

static void slot_show(struct fravashid_slot *slot)
{
    struct fravashid_app *app = slot->app;
    const char *path = slot_path(slot);

    fravashid_image_free(&slot->image);
    if (path && fravashid_media_kind(path) == FRAVASHID_MEDIA_VIDEO) {
        if (!slot->video)
            slot->video = fravashid_video_open(app->epfd, slot);
        if (!slot->video || fravashid_video_play(slot->video, path, app->settings.mode,
                                                 app->settings.background) != 0)
            fprintf(stderr, "fravashid: cannot play %s\n", path);
    } else {
        slot_stop_video(slot);
        if (path && fravashid_image_load(path, app->settings.background,
                                         &slot->image) != 0)
            fprintf(stderr, "fravashid: cannot load %s\n", path);
    }
    slot_paint(slot);
    slot_publish(slot);
}

static int slot_apply_spec(struct fravashid_slot *slot, bool force)
{
    struct fravashid_app *app = slot->app;
    const char *spec = fravashid_settings_spec(&app->settings, slot->output->name, app->layout);

    if (!force && slot->spec && strcmp(spec, slot->spec) == 0)
        return 0;
    fravashid_playlist_free(&slot->list);
    if (spec[0] != '\0' && fravashid_playlist_load_path(&slot->list, spec) != 0)
        fprintf(stderr, "fravashid: no usable media in %s\n", spec);
    if (app->settings.shuffle && slot->list.count > 1)
        fravashid_playlist_shuffle(&slot->list,
                                   (unsigned)time(NULL) + (unsigned)app->slot_count);
    free(slot->spec);
    slot->spec = strdup(spec);
    slot_show(slot);
    return 1;
}

static void timer_rearm(struct fravashid_app *app)
{
    struct itimerspec spec;
    bool cycling = false;

    memset(&spec, 0, sizeof(spec));
    for (size_t i = 0; i < app->slot_count; i++) {
        if (app->slots[i]->list.count > 1)
            cycling = true;
    }
    if (cycling && app->settings.interval > 0) {
        spec.it_value.tv_sec = (time_t)app->settings.interval;
        spec.it_interval.tv_sec = (time_t)app->settings.interval;
    }
    timerfd_settime(app->timer_fd, 0, &spec, NULL);
}

static void slot_free(struct fravashid_slot *slot)
{
    slot_stop_video(slot);
    fravashid_image_free(&slot->image);
    fravashid_playlist_free(&slot->list);
    free(slot->spec);
    free(slot);
}

static void output_ready(struct fravashid_output *output, void *user)
{
    struct fravashid_app *app = user;
    struct fravashid_slot *slot = output->user;
    struct fravashid_slot **grown;

    if (slot) {
        slot_paint(slot);
        return;
    }
    slot = calloc(1, sizeof(*slot));
    grown = realloc(app->slots, (app->slot_count + 1) * sizeof(*grown));
    if (!slot || !grown) {
        free(slot);
        if (grown)
            app->slots = grown;
        return;
    }
    slot->app = app;
    slot->output = output;
    output->user = slot;
    app->slots = grown;
    app->slots[app->slot_count++] = slot;
    slot_apply_spec(slot, true);
    timer_rearm(app);
}

static void output_gone(struct fravashid_output *output, void *user)
{
    struct fravashid_app *app = user;
    struct fravashid_slot *slot = output->user;
    size_t index = 0;

    if (!slot)
        return;
    while (app->slots[index] != slot)
        index++;
    memmove(app->slots + index, app->slots + index + 1,
            (app->slot_count - index - 1) * sizeof(*app->slots));
    app->slot_count--;
    output->user = NULL;
    slot_free(slot);
    if (index == 0 && app->slot_count > 0)
        slot_publish(app->slots[0]);
    timer_rearm(app);
}

static const struct fravashid_events events = {
    .output_ready = output_ready,
    .output_gone = output_gone,
};

static void config_changed(const char *key, void *user)
{
    struct fravashid_app *app = user;

    (void)key;
    app->config_dirty = true;
}

static int add_source(int epfd, int fd, struct fravashid_source *source)
{
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = source};

    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static enum fravashid_backend choose_backend(enum fravashid_backend wanted)
{
    const char *wayland = getenv("WAYLAND_DISPLAY");

    if (wanted != FRAVASHID_BACKEND_AUTO)
        return wanted;
    return wayland && *wayland ? FRAVASHID_BACKEND_WAYLAND : FRAVASHID_BACKEND_X11;
}

int fravashid_app_open(struct fravashid_app *app, int epfd, enum fravashid_backend backend)
{
    int fd;

    memset(app, 0, sizeof(*app));
    app->epfd = epfd;
    app->timer_fd = -1;
    app->config_source.kind = FRAVASHID_SRC_CONFIG;
    app->timer_source.kind = FRAVASHID_SRC_TIMER;
    app->display_source.kind = FRAVASHID_SRC_DISPLAY;
    fravashid_settings_defaults(&app->settings);

    app->cfgd = cfgd_open();
    fd = app->cfgd ? cfgd_notify_fd(app->cfgd) : -1;
    if (fd >= 0 && cfgd_subscribe(app->cfgd, FRAVASHID_SETTINGS_PREFIX,
                                  config_changed, app) >= 0 &&
        add_source(epfd, fd, &app->config_source) == 0)
        fravashid_settings_load(app->cfgd, &app->settings);
    else
        fprintf(stderr, "fravashid: settings are not available, using defaults\n");

    app->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (app->timer_fd < 0 || add_source(epfd, app->timer_fd, &app->timer_source) != 0)
        return -1;

    if (fravashid_bus_open(&app->bus, epfd, app) != 0)
        return -1;

    app->backend = choose_backend(backend);
    if (app->backend == FRAVASHID_BACKEND_WAYLAND) {
        app->wl = fravashid_wl_open(&events, app);
        fd = app->wl ? fravashid_wl_fd(app->wl) : -1;
    } else {
        app->x11 = fravashid_x11_open(&events, app);
        fd = app->x11 ? fravashid_x11_fd(app->x11) : -1;
    }
    if (fd < 0 || add_source(epfd, fd, &app->display_source) != 0)
        return -1;
    return 0;
}

void fravashid_app_close(struct fravashid_app *app)
{
    for (size_t i = 0; i < app->slot_count; i++)
        slot_free(app->slots[i]);
    free(app->slots);
    if (app->wl)
        fravashid_wl_close(app->wl);
    if (app->x11)
        fravashid_x11_close(app->x11);
    fravashid_bus_close(&app->bus);
    if (app->cfgd)
        cfgd_close(app->cfgd);
    if (app->timer_fd >= 0)
        close(app->timer_fd);
    fravashid_settings_free(&app->settings);
}

int fravashid_app_prepare(struct fravashid_app *app)
{
    return app->wl ? fravashid_wl_prepare(app->wl) : 0;
}

int fravashid_app_finish(struct fravashid_app *app, int display_readable)
{
    if (app->wl)
        return fravashid_wl_finish(app->wl, display_readable);
    return display_readable ? fravashid_x11_dispatch(app->x11) : 0;
}

void fravashid_app_config_event(struct fravashid_app *app)
{
    enum fravashid_mode mode = app->settings.mode;
    uint32_t background = app->settings.background;
    bool shuffle = app->settings.shuffle;

    cfgd_dispatch(app->cfgd);
    if (!app->config_dirty)
        return;
    app->config_dirty = false;
    fravashid_settings_load(app->cfgd, &app->settings);
    for (size_t i = 0; i < app->slot_count; i++) {
        struct fravashid_slot *slot = app->slots[i];
        int reloaded = slot_apply_spec(slot, shuffle != app->settings.shuffle);

        if (!reloaded && (mode != app->settings.mode ||
                          background != app->settings.background))
            slot_show(slot);
    }
    timer_rearm(app);
}

void fravashid_app_set_layout(struct fravashid_app *app, const char *name)
{
    if (strcmp(app->layout, name) == 0 || strlen(name) >= sizeof(app->layout))
        return;
    snprintf(app->layout, sizeof(app->layout), "%s", name);
    for (size_t i = 0; i < app->slot_count; i++)
        slot_apply_spec(app->slots[i], false);
    timer_rearm(app);
}

void fravashid_app_step(struct fravashid_app *app, int step)
{
    for (size_t i = 0; i < app->slot_count; i++) {
        struct fravashid_slot *slot = app->slots[i];

        if (slot->list.count < 2)
            continue;
        fravashid_playlist_advance(&slot->list, step);
        slot_show(slot);
    }
}

void fravashid_app_timer_event(struct fravashid_app *app)
{
    uint64_t expirations;

    if (read(app->timer_fd, &expirations, sizeof(expirations)) < 0)
        return;
    fravashid_app_step(app, 1);
}

void fravashid_app_video_event(struct fravashid_app *app, struct fravashid_video *video)
{
    struct fravashid_slot *slot = fravashid_video_user(video);
    struct fravashid_output *output = slot->output;
    uint32_t *pixels;
    int stride;

    if (!fravashid_video_update(video))
        return;
    pixels = acquire(app, output, &stride);
    if (!pixels)
        return;
    fravashid_video_render(video, pixels, output->width, output->height, stride);
    commit(app, output);
}

size_t fravashid_app_current(const struct fravashid_app *app, size_t index,
                             const char **output, const char **path)
{
    if (index < app->slot_count) {
        const char *current = slot_path(app->slots[index]);

        *output = app->slots[index]->output->name;
        *path = current ? current : "";
    }
    return app->slot_count;
}
