/* SPDX-License-Identifier: GPL-3.0-only */
#include "x11.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>

struct x11_output {
    struct fravashid_output base;
    struct fravashid_x11 *x11;
    int x;
    int y;
    uint32_t *pixels;
    struct x11_output *next;
};

struct fravashid_x11 {
    xcb_connection_t *connection;
    xcb_screen_t *screen;
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    int pixmap_width;
    int pixmap_height;
    uint8_t first_randr_event;
    const struct fravashid_events *events;
    void *user;
    struct x11_output *outputs;
};

static int format_supported(xcb_connection_t *connection, xcb_screen_t *screen)
{
    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_format_iterator_t it = xcb_setup_pixmap_formats_iterator(setup);

    if (setup->image_byte_order != XCB_IMAGE_ORDER_LSB_FIRST)
        return 0;
    for (; it.rem; xcb_format_next(&it)) {
        if (it.data->depth == screen->root_depth && it.data->bits_per_pixel == 32)
            return 1;
    }
    return 0;
}

static xcb_atom_t intern(xcb_connection_t *connection, const char *name)
{
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
        connection, xcb_intern_atom(connection, 0, strlen(name), name), NULL);
    xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;

    free(reply);
    return atom;
}

static void pixmap_publish(struct fravashid_x11 *x11)
{
    xcb_connection_t *c = x11->connection;
    xcb_window_t root = x11->screen->root;
    const char *names[] = {"_XROOTPMAP_ID", "ESETROOT_PMAP_ID"};

    xcb_change_window_attributes(c, root, XCB_CW_BACK_PIXMAP, &x11->pixmap);
    for (int i = 0; i < 2; i++) {
        xcb_atom_t atom = intern(c, names[i]);

        if (atom != XCB_ATOM_NONE)
            xcb_change_property(c, XCB_PROP_MODE_REPLACE, root, atom,
                                XCB_ATOM_PIXMAP, 32, 1, &x11->pixmap);
    }
    xcb_clear_area(c, 0, root, 0, 0, 0, 0);
    xcb_flush(c);
}

static void pixmap_resize(struct fravashid_x11 *x11)
{
    xcb_connection_t *c = x11->connection;
    xcb_get_geometry_reply_t *geometry = xcb_get_geometry_reply(
        c, xcb_get_geometry(c, x11->screen->root), NULL);

    if (!geometry)
        return;
    if (x11->pixmap && geometry->width == x11->pixmap_width &&
        geometry->height == x11->pixmap_height) {
        free(geometry);
        return;
    }
    if (x11->pixmap)
        xcb_free_pixmap(c, x11->pixmap);
    x11->pixmap = xcb_generate_id(c);
    x11->pixmap_width = geometry->width;
    x11->pixmap_height = geometry->height;
    xcb_create_pixmap(c, x11->screen->root_depth, x11->pixmap, x11->screen->root,
                      geometry->width, geometry->height);
    free(geometry);
    pixmap_publish(x11);
}

static void outputs_clear(struct fravashid_x11 *x11)
{
    while (x11->outputs) {
        struct x11_output *output = x11->outputs;

        x11->outputs = output->next;
        x11->events->output_gone(&output->base, x11->user);
        free(output->pixels);
        free(output);
    }
}

static void outputs_query(struct fravashid_x11 *x11)
{
    xcb_connection_t *c = x11->connection;
    xcb_randr_get_monitors_reply_t *monitors = xcb_randr_get_monitors_reply(
        c, xcb_randr_get_monitors(c, x11->screen->root, 1), NULL);
    struct x11_output *tail = NULL;

    if (!monitors)
        return;
    pixmap_resize(x11);
    for (xcb_randr_monitor_info_iterator_t it =
             xcb_randr_get_monitors_monitors_iterator(monitors);
         it.rem; xcb_randr_monitor_info_next(&it)) {
        xcb_get_atom_name_reply_t *name = xcb_get_atom_name_reply(
            c, xcb_get_atom_name(c, it.data->name), NULL);
        struct x11_output *output = calloc(1, sizeof(*output));

        if (!output) {
            free(name);
            continue;
        }
        output->x11 = x11;
        output->x = it.data->x;
        output->y = it.data->y;
        output->base.width = it.data->width;
        output->base.height = it.data->height;
        if (name)
            snprintf(output->base.name, sizeof(output->base.name), "%.*s",
                     xcb_get_atom_name_name_length(name),
                     xcb_get_atom_name_name(name));
        else
            snprintf(output->base.name, sizeof(output->base.name), "x11-%d",
                     output->x);
        free(name);
        output->pixels = aligned_alloc(64, ((size_t)output->base.width *
                                                output->base.height * 4 + 63) & ~(size_t)63);
        if (!output->pixels) {
            free(output);
            continue;
        }
        if (tail)
            tail->next = output;
        else
            x11->outputs = output;
        tail = output;
    }
    free(monitors);
    for (struct x11_output *output = x11->outputs; output; output = output->next)
        x11->events->output_ready(&output->base, x11->user);
}

struct fravashid_x11 *fravashid_x11_open(const struct fravashid_events *events,
                                         void *user)
{
    struct fravashid_x11 *x11 = calloc(1, sizeof(*x11));
    const xcb_query_extension_reply_t *randr;
    xcb_screen_iterator_t screens;
    int screen_number;

    if (!x11)
        return NULL;
    x11->events = events;
    x11->user = user;
    x11->connection = xcb_connect(NULL, &screen_number);
    if (xcb_connection_has_error(x11->connection)) {
        fprintf(stderr, "fravashid: cannot connect to the X server\n");
        goto fail;
    }
    screens = xcb_setup_roots_iterator(xcb_get_setup(x11->connection));
    for (int i = 0; i < screen_number; i++)
        xcb_screen_next(&screens);
    x11->screen = screens.data;
    if (!format_supported(x11->connection, x11->screen)) {
        fprintf(stderr, "fravashid: unsupported X11 pixel format\n");
        goto fail;
    }
    randr = xcb_get_extension_data(x11->connection, &xcb_randr_id);
    if (!randr || !randr->present) {
        fprintf(stderr, "fravashid: the X server has no RandR extension\n");
        goto fail;
    }
    x11->gc = xcb_generate_id(x11->connection);
    xcb_create_gc(x11->connection, x11->gc, x11->screen->root, 0, NULL);
    x11->first_randr_event = randr->first_event;
    xcb_randr_select_input(x11->connection, x11->screen->root,
                           XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    outputs_query(x11);
    xcb_flush(x11->connection);
    return x11;

fail:
    fravashid_x11_close(x11);
    return NULL;
}

void fravashid_x11_close(struct fravashid_x11 *x11)
{
    while (x11->outputs) {
        struct x11_output *output = x11->outputs;

        x11->outputs = output->next;
        free(output->pixels);
        free(output);
    }
    xcb_disconnect(x11->connection);
    free(x11);
}

int fravashid_x11_fd(struct fravashid_x11 *x11)
{
    return xcb_get_file_descriptor(x11->connection);
}

int fravashid_x11_dispatch(struct fravashid_x11 *x11)
{
    xcb_generic_event_t *event;
    bool changed = false;

    while ((event = xcb_poll_for_event(x11->connection))) {
        if ((event->response_type & 0x7f) ==
            x11->first_randr_event + XCB_RANDR_SCREEN_CHANGE_NOTIFY)
            changed = true;
        free(event);
    }
    if (xcb_connection_has_error(x11->connection))
        return -1;
    if (changed) {
        outputs_clear(x11);
        outputs_query(x11);
        xcb_flush(x11->connection);
    }
    return 0;
}

uint32_t *fravashid_x11_acquire(struct fravashid_output *base, int *stride_px)
{
    struct x11_output *output = (struct x11_output *)base;

    *stride_px = base->width;
    return output->pixels;
}

void fravashid_x11_commit(struct fravashid_output *base)
{
    struct x11_output *output = (struct x11_output *)base;
    struct fravashid_x11 *x11 = output->x11;
    xcb_connection_t *c = x11->connection;
    size_t row_bytes = (size_t)base->width * 4;
    size_t max_bytes = (size_t)xcb_get_maximum_request_length(c) * 4 - 64;
    int rows = (int)(max_bytes / row_bytes);

    if (rows < 1)
        return;
    for (int y = 0; y < base->height; y += rows) {
        int n = base->height - y < rows ? base->height - y : rows;

        xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, x11->pixmap, x11->gc,
                      base->width, n, output->x, output->y + y, 0,
                      x11->screen->root_depth, (uint32_t)(row_bytes * n),
                      (const uint8_t *)output->pixels + row_bytes * y);
    }
    xcb_clear_area(c, 0, x11->screen->root, output->x, output->y, base->width,
                   base->height);
    xcb_flush(c);
}
