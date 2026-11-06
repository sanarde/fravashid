/* SPDX-License-Identifier: GPL-3.0-only */
#include "wayland.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define BUFFER_COUNT 2

struct wl_output_data;

struct buffer {
    struct wl_buffer *buffer;
    uint32_t *data;
    size_t size;
    int width;
    int height;
    bool busy;
    bool stale;
};

struct fravashid_wl {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;
    const struct fravashid_events *events;
    void *user;
    struct wl_output_data *outputs;
    bool reading;
};

struct wl_output_data {
    struct fravashid_output base;
    struct fravashid_wl *wl;
    struct wl_output *wl_output;
    uint32_t global;
    int scale;
    bool named;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    bool configured;
    int logical_width;
    int logical_height;
    struct buffer buffers[BUFFER_COUNT];
    struct wl_output_data *next;
};

static void buffer_destroy(struct buffer *buffer)
{
    if (!buffer->buffer)
        return;
    wl_buffer_destroy(buffer->buffer);
    munmap(buffer->data, buffer->size);
    memset(buffer, 0, sizeof(*buffer));
}

static void buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    struct buffer *buffer = data;

    (void)wl_buffer;
    if (buffer->stale)
        buffer_destroy(buffer);
    else
        buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static int buffer_create(struct buffer *buffer, struct wl_shm *shm, int width,
                         int height)
{
    size_t size = (size_t)width * height * sizeof(uint32_t);
    int fd = memfd_create("fravashid", MFD_CLOEXEC);
    struct wl_shm_pool *pool;
    void *data;

    if (fd < 0)
        return -1;
    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return -1;
    }
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return -1;
    }
    pool = wl_shm_create_pool(shm, fd, (int32_t)size);
    buffer->buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
                                               width * 4, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    buffer->data = data;
    buffer->size = size;
    buffer->width = width;
    buffer->height = height;
    buffer->busy = false;
    buffer->stale = false;
    wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
    return 0;
}

static void output_update(struct wl_output_data *output)
{
    int width = output->logical_width * output->scale;
    int height = output->logical_height * output->scale;

    if (!output->configured || width <= 0 || height <= 0)
        return;
    if (width == output->base.width && height == output->base.height)
        return;
    output->base.width = width;
    output->base.height = height;
    if (wl_surface_get_version(output->surface) >= 3)
        wl_surface_set_buffer_scale(output->surface, output->scale);
    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct buffer *buffer = &output->buffers[i];

        if (!buffer->buffer)
            continue;
        if (buffer->busy)
            buffer->stale = true;
        else
            buffer_destroy(buffer);
    }
    output->wl->events->output_ready(&output->base, output->wl->user);
}

static void layer_configure(void *data, struct zwlr_layer_surface_v1 *layer,
                            uint32_t serial, uint32_t width, uint32_t height)
{
    struct wl_output_data *output = data;

    zwlr_layer_surface_v1_ack_configure(layer, serial);
    output->logical_width = (int)width;
    output->logical_height = (int)height;
    output->configured = true;
    output_update(output);
}

static void output_destroy(struct wl_output_data *output);

static void layer_closed(void *data, struct zwlr_layer_surface_v1 *layer)
{
    struct wl_output_data *output = data;
    struct fravashid_wl *wl = output->wl;
    struct wl_output_data **link = &wl->outputs;

    (void)layer;
    while (*link && *link != output)
        link = &(*link)->next;
    if (*link)
        *link = output->next;
    wl->events->output_gone(&output->base, wl->user);
    output_destroy(output);
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

static void output_geometry(void *data, struct wl_output *wl_output, int32_t x,
                            int32_t y, int32_t pw, int32_t ph, int32_t subpixel,
                            const char *make, const char *model, int32_t transform)
{
    (void)data; (void)wl_output; (void)x; (void)y; (void)pw; (void)ph;
    (void)subpixel; (void)make; (void)model; (void)transform;
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh)
{
    (void)data; (void)wl_output; (void)flags; (void)width; (void)height;
    (void)refresh;
}

static void output_create_surface(struct wl_output_data *output)
{
    struct fravashid_wl *wl = output->wl;

    output->surface = wl_compositor_create_surface(wl->compositor);
    output->layer = zwlr_layer_shell_v1_get_layer_surface(
        wl->layer_shell, output->surface, output->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "fravashid");
    zwlr_layer_surface_v1_set_anchor(
        output->layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                           ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                           ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                           ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(output->layer, -1);
    zwlr_layer_surface_v1_set_size(output->layer, 0, 0);
    zwlr_layer_surface_v1_add_listener(output->layer, &layer_listener, output);
    wl_surface_commit(output->surface);
}

static void output_done(void *data, struct wl_output *wl_output)
{
    struct wl_output_data *output = data;

    (void)wl_output;
    if (!output->named)
        snprintf(output->base.name, sizeof(output->base.name), "wl-%u",
                 output->global);
    if (!output->surface && output->wl->compositor && output->wl->layer_shell)
        output_create_surface(output);
    else
        output_update(output);
}

static void output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
    struct wl_output_data *output = data;

    (void)wl_output;
    output->scale = factor > 0 ? factor : 1;
}

static void output_name(void *data, struct wl_output *wl_output, const char *name)
{
    struct wl_output_data *output = data;

    (void)wl_output;
    snprintf(output->base.name, sizeof(output->base.name), "%s", name);
    output->named = true;
}

static void output_description(void *data, struct wl_output *wl_output,
                               const char *description)
{
    (void)data; (void)wl_output; (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void output_destroy(struct wl_output_data *output)
{
    for (int i = 0; i < BUFFER_COUNT; i++)
        buffer_destroy(&output->buffers[i]);
    if (output->layer)
        zwlr_layer_surface_v1_destroy(output->layer);
    if (output->surface)
        wl_surface_destroy(output->surface);
    if (wl_output_get_version(output->wl_output) >= 3)
        wl_output_release(output->wl_output);
    else
        wl_output_destroy(output->wl_output);
    free(output);
}

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version)
{
    struct fravashid_wl *wl = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wl->compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
                                          version < 4 ? version : 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        wl->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        wl->layer_shell =
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output_data *output;

        output = calloc(1, sizeof(*output));
        if (!output)
            return;
        output->wl = wl;
        output->global = name;
        output->scale = 1;
        output->wl_output = wl_registry_bind(registry, name, &wl_output_interface,
                                             version < 4 ? version : 4);
        output->next = wl->outputs;
        wl->outputs = output;
        wl_output_add_listener(output->wl_output, &output_listener, output);
    }
}

static void registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    struct fravashid_wl *wl = data;
    struct wl_output_data **link = &wl->outputs;

    (void)registry;
    while (*link && (*link)->global != name)
        link = &(*link)->next;
    if (*link) {
        struct wl_output_data *output = *link;

        *link = output->next;
        wl->events->output_gone(&output->base, wl->user);
        output_destroy(output);
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

struct fravashid_wl *fravashid_wl_open(const struct fravashid_events *events,
                                       void *user)
{
    struct fravashid_wl *wl = calloc(1, sizeof(*wl));

    if (!wl)
        return NULL;
    wl->events = events;
    wl->user = user;
    wl->display = wl_display_connect(NULL);
    if (!wl->display) {
        fprintf(stderr, "fravashid: cannot connect to the Wayland display\n");
        free(wl);
        return NULL;
    }
    wl->registry = wl_display_get_registry(wl->display);
    wl_registry_add_listener(wl->registry, &registry_listener, wl);
    wl_display_roundtrip(wl->display);
    if (!wl->compositor || !wl->shm || !wl->layer_shell) {
        fprintf(stderr, "fravashid: the compositor does not support "
                        "wlr-layer-shell\n");
        fravashid_wl_close(wl);
        return NULL;
    }
    wl_display_roundtrip(wl->display);
    wl_display_roundtrip(wl->display);
    return wl;
}

void fravashid_wl_close(struct fravashid_wl *wl)
{
    while (wl->outputs) {
        struct wl_output_data *output = wl->outputs;

        wl->outputs = output->next;
        output_destroy(output);
    }
    if (wl->layer_shell)
        zwlr_layer_shell_v1_destroy(wl->layer_shell);
    if (wl->shm)
        wl_shm_destroy(wl->shm);
    if (wl->compositor)
        wl_compositor_destroy(wl->compositor);
    if (wl->registry)
        wl_registry_destroy(wl->registry);
    wl_display_disconnect(wl->display);
    free(wl);
}

int fravashid_wl_fd(struct fravashid_wl *wl)
{
    return wl_display_get_fd(wl->display);
}

int fravashid_wl_prepare(struct fravashid_wl *wl)
{
    while (wl_display_prepare_read(wl->display) != 0)
        wl_display_dispatch_pending(wl->display);
    wl->reading = true;
    if (wl_display_flush(wl->display) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(wl->display);
        wl->reading = false;
        return -1;
    }
    return 0;
}

int fravashid_wl_finish(struct fravashid_wl *wl, int readable)
{
    int rc = 0;

    if (wl->reading) {
        if (readable) {
            if (wl_display_read_events(wl->display) < 0)
                rc = -1;
        } else {
            wl_display_cancel_read(wl->display);
        }
        wl->reading = false;
    }
    if (rc == 0 && wl_display_dispatch_pending(wl->display) < 0)
        rc = -1;
    return rc;
}

uint32_t *fravashid_wl_acquire(struct fravashid_output *base, int *stride_px)
{
    struct wl_output_data *output = (struct wl_output_data *)base;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct buffer *buffer = &output->buffers[i];

        if (buffer->buffer && !buffer->busy && !buffer->stale)
            goto found;
    }
    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct buffer *buffer = &output->buffers[i];

        if (!buffer->buffer) {
            if (buffer_create(buffer, output->wl->shm, base->width, base->height) != 0)
                return NULL;
            goto found;
        }
    }
    return NULL;

found:
    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct buffer *buffer = &output->buffers[i];

        if (buffer->buffer && !buffer->busy && !buffer->stale) {
            *stride_px = buffer->width;
            return buffer->data;
        }
    }
    return NULL;
}

void fravashid_wl_commit(struct fravashid_output *base)
{
    struct wl_output_data *output = (struct wl_output_data *)base;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct buffer *buffer = &output->buffers[i];

        if (buffer->buffer && !buffer->busy && !buffer->stale) {
            buffer->busy = true;
            wl_surface_attach(output->surface, buffer->buffer, 0, 0);
            wl_surface_damage(output->surface, 0, 0, INT32_MAX, INT32_MAX);
            wl_surface_commit(output->surface);
            return;
        }
    }
}
