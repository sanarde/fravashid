/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_BUS_H
#define FRAVASHID_BUS_H

#include <dbus/dbus.h>
#include <stdint.h>

#include "loop.h"

struct fravashid_app;

struct fravashid_bus_watch {
    struct fravashid_source source;
    DBusWatch *watch;
    int fd;
};

struct fravashid_bus {
    DBusConnection *connection;
    int epfd;
    struct fravashid_app *app;
    char *seurat_path;
};

int fravashid_bus_open(struct fravashid_bus *bus, int epfd, struct fravashid_app *app);
void fravashid_bus_close(struct fravashid_bus *bus);
void fravashid_bus_event(struct fravashid_bus *bus, struct fravashid_bus_watch *watch,
                         uint32_t events);
void fravashid_bus_emit_changed(struct fravashid_bus *bus, const char *output,
                                const char *path);
void fravashid_bus_set_seurat_wallpaper(struct fravashid_bus *bus, const char *path);

#endif
