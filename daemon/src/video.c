/* SPDX-License-Identifier: GPL-3.0-only */
#include "video.h"

#include <mpv/client.h>
#include <mpv/render.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

struct fravashid_video {
    struct fravashid_source source;
    mpv_handle *mpv;
    mpv_render_context *render;
    int event_fd;
    void *user;
};

static void wake(void *data)
{
    struct fravashid_video *video = data;
    uint64_t one = 1;

    if (write(video->event_fd, &one, sizeof(one)) < 0)
        return;
}

static int set_options(mpv_handle *mpv)
{
    static const char *const options[][2] = {
        {"vo", "libmpv"},
        {"audio", "no"},
        {"loop-file", "inf"},
        {"idle", "yes"},
        {"config", "no"},
        {"terminal", "no"},
        {"load-scripts", "no"},
        {"osc", "no"},
        {"input-default-bindings", "no"},
        {"background", "color"},
    };

    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
        if (mpv_set_option_string(mpv, options[i][0], options[i][1]) < 0) {
            fprintf(stderr, "fravashid: mpv option %s is not accepted\n",
                    options[i][0]);
            return -1;
        }
    }
    return 0;
}

struct fravashid_video *fravashid_video_open(int epfd, void *user)
{
    struct fravashid_video *video = calloc(1, sizeof(*video));
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_SW},
        {MPV_RENDER_PARAM_INVALID, NULL},
    };
    struct epoll_event ev;

    if (!video)
        return NULL;
    video->source.kind = FRAVASHID_SRC_VIDEO;
    video->user = user;
    video->event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    video->mpv = mpv_create();
    if (video->event_fd < 0 || !video->mpv || set_options(video->mpv) != 0 ||
        mpv_initialize(video->mpv) < 0 ||
        mpv_render_context_create(&video->render, video->mpv, params) < 0)
        goto fail;
    mpv_set_wakeup_callback(video->mpv, wake, video);
    mpv_render_context_set_update_callback(video->render, wake, video);
    ev.events = EPOLLIN;
    ev.data.ptr = &video->source;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, video->event_fd, &ev) != 0)
        goto fail;
    return video;

fail:
    if (video->render)
        mpv_render_context_free(video->render);
    if (video->mpv)
        mpv_terminate_destroy(video->mpv);
    if (video->event_fd >= 0)
        close(video->event_fd);
    free(video);
    return NULL;
}

void fravashid_video_close(struct fravashid_video *video, int epfd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, video->event_fd, NULL);
    mpv_render_context_free(video->render);
    mpv_terminate_destroy(video->mpv);
    close(video->event_fd);
    free(video);
}

void *fravashid_video_user(const struct fravashid_video *video)
{
    return video->user;
}

static void apply_mode(mpv_handle *mpv, enum fravashid_mode mode, uint32_t background)
{
    char color[16];

    snprintf(color, sizeof(color), "#%06x", background & 0xffffff);
    mpv_set_property_string(mpv, "background-color", color);
    mpv_set_property_string(mpv, "keepaspect",
                            mode == FRAVASHID_MODE_STRETCH ? "no" : "yes");
    mpv_set_property_string(mpv, "panscan", mode == FRAVASHID_MODE_FILL ? "1" : "0");
    mpv_set_property_string(mpv, "video-unscaled",
                            mode == FRAVASHID_MODE_CENTER ? "yes" : "no");
}

int fravashid_video_play(struct fravashid_video *video, const char *path,
                         enum fravashid_mode mode, uint32_t background)
{
    const char *command[] = {"loadfile", path, "replace", NULL};

    apply_mode(video->mpv, mode, background);
    return mpv_command(video->mpv, command) < 0 ? -1 : 0;
}

bool fravashid_video_update(struct fravashid_video *video)
{
    uint64_t count;

    if (read(video->event_fd, &count, sizeof(count)) < 0)
        return false;
    for (;;) {
        mpv_event *event = mpv_wait_event(video->mpv, 0);

        if (event->event_id == MPV_EVENT_NONE)
            break;
        if (event->event_id == MPV_EVENT_END_FILE) {
            mpv_event_end_file *end = event->data;

            if (end->reason == MPV_END_FILE_REASON_ERROR)
                fprintf(stderr, "fravashid: cannot play video: %s\n",
                        mpv_error_string(end->error));
        }
    }
    return (mpv_render_context_update(video->render) & MPV_RENDER_UPDATE_FRAME) != 0;
}

void fravashid_video_render(struct fravashid_video *video, uint32_t *target,
                            int width, int height, int stride_px)
{
    int size[2] = {width, height};
    size_t stride = (size_t)stride_px * sizeof(uint32_t);
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_SW_SIZE, size},
        {MPV_RENDER_PARAM_SW_FORMAT, (void *)"bgr0"},
        {MPV_RENDER_PARAM_SW_STRIDE, &stride},
        {MPV_RENDER_PARAM_SW_POINTER, target},
        {MPV_RENDER_PARAM_INVALID, NULL},
    };

    mpv_render_context_render(video->render, params);
}
