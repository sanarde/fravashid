/* SPDX-License-Identifier: GPL-3.0-only */
#include "bus.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "app.h"

#define BUS_NAME "org.nuros.sanar.Fravashi"
#define OBJECT_PATH "/org/nuros/sanar/fravashi"
#define INTERFACE "org.nuros.sanar.Fravashi"
#define SEURAT_NAME "org.nuros.sanar.seurat"
#define SEURAT_PATH "/org/nuros/sanar/seurat"
#define SEURAT_INTERFACE "org.nuros.sanar.Seurat"
#define NAWI_NAME "org.nuros.sanar.Nawi"
#define NAWI_PATH "/org/nuros/sanar/nawi"
#define NAWI_INTERFACE "org.nuros.sanar.Nawi"

static const char introspection_xml[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\" "
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"" INTERFACE "\">\n"
    "    <method name=\"Next\"/>\n"
    "    <method name=\"Previous\"/>\n"
    "    <method name=\"GetCurrent\">\n"
    "      <arg type=\"a{ss}\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <signal name=\"Changed\">\n"
    "      <arg type=\"s\" name=\"output\"/>\n"
    "      <arg type=\"s\" name=\"path\"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

static int watch_update(struct fravashid_bus *bus, struct fravashid_bus_watch *w, int op)
{
    struct epoll_event ev = {.events = 0, .data.ptr = &w->source};

    if (dbus_watch_get_enabled(w->watch)) {
        unsigned int flags = dbus_watch_get_flags(w->watch);

        if (flags & DBUS_WATCH_READABLE)
            ev.events |= EPOLLIN;
        if (flags & DBUS_WATCH_WRITABLE)
            ev.events |= EPOLLOUT;
    }
    return epoll_ctl(bus->epfd, op, w->fd, &ev);
}

static dbus_bool_t add_watch(DBusWatch *watch, void *data)
{
    struct fravashid_bus *bus = data;
    struct fravashid_bus_watch *w = calloc(1, sizeof(*w));

    if (!w)
        return FALSE;
    w->source.kind = FRAVASHID_SRC_BUS;
    w->watch = watch;
    w->fd = fcntl(dbus_watch_get_unix_fd(watch), F_DUPFD_CLOEXEC, 0);
    if (w->fd < 0) {
        free(w);
        return FALSE;
    }
    if (watch_update(bus, w, EPOLL_CTL_ADD) != 0) {
        close(w->fd);
        free(w);
        return FALSE;
    }
    dbus_watch_set_data(watch, w, NULL);
    return TRUE;
}

static void remove_watch(DBusWatch *watch, void *data)
{
    struct fravashid_bus *bus = data;
    struct fravashid_bus_watch *w = dbus_watch_get_data(watch);

    epoll_ctl(bus->epfd, EPOLL_CTL_DEL, w->fd, NULL);
    close(w->fd);
    dbus_watch_set_data(watch, NULL, NULL);
    free(w);
}

static void toggle_watch(DBusWatch *watch, void *data)
{
    watch_update(data, dbus_watch_get_data(watch), EPOLL_CTL_MOD);
}

static void append_current(DBusMessage *message, const struct fravashid_app *app)
{
    DBusMessageIter iter, array, entry;
    size_t count = fravashid_app_current(app, 0, &(const char *){NULL},
                                         &(const char *){NULL});

    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{ss}", &array);
    for (size_t i = 0; i < count; i++) {
        const char *output;
        const char *path;

        fravashid_app_current(app, i, &output, &path);
        dbus_message_iter_open_container(&array, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &output);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &path);
        dbus_message_iter_close_container(&array, &entry);
    }
    dbus_message_iter_close_container(&iter, &array);
}

static void send_seurat_wallpaper(struct fravashid_bus *bus)
{
    DBusMessage *call;
    const char *path = bus->seurat_path ? bus->seurat_path : "";

    if (!bus->connection || !bus->seurat_path)
        return;
    call = dbus_message_new_method_call(SEURAT_NAME, SEURAT_PATH, SEURAT_INTERFACE,
                                        "SetWallpaper");
    if (!call)
        return;
    dbus_message_set_no_reply(call, TRUE);
    dbus_message_set_auto_start(call, FALSE);
    dbus_message_append_args(call, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID);
    dbus_connection_send(bus->connection, call, NULL);
    dbus_message_unref(call);
}

static void layout_reply(DBusPendingCall *pending, void *data)
{
    struct fravashid_bus *bus = data;
    DBusMessage *reply = dbus_pending_call_steal_reply(pending);
    const char *profile = NULL;

    if (reply && dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN &&
        dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &profile, DBUS_TYPE_INVALID))
        fravashid_app_set_layout(bus->app, profile);
    if (reply)
        dbus_message_unref(reply);
    dbus_pending_call_unref(pending);
}

static void query_layout(struct fravashid_bus *bus)
{
    DBusMessage *call;
    DBusPendingCall *pending = NULL;

    if (!bus->connection)
        return;
    call = dbus_message_new_method_call(NAWI_NAME, NAWI_PATH, NAWI_INTERFACE, "GetProfile");
    if (!call)
        return;
    dbus_message_set_auto_start(call, FALSE);
    if (dbus_connection_send_with_reply(bus->connection, call, &pending, 2000) && pending) {
        if (!dbus_pending_call_set_notify(pending, layout_reply, bus, NULL))
            dbus_pending_call_unref(pending);
    }
    dbus_message_unref(call);
}

static DBusHandlerResult handle_message(DBusConnection *connection,
                                        DBusMessage *message, void *data)
{
    struct fravashid_bus *bus = data;
    DBusMessage *reply;

    if (dbus_message_is_signal(message, "org.freedesktop.DBus", "NameOwnerChanged")) {
        const char *name = NULL, *old_owner = NULL, *new_owner = NULL;

        if (dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &name,
                                  DBUS_TYPE_STRING, &old_owner, DBUS_TYPE_STRING,
                                  &new_owner, DBUS_TYPE_INVALID) &&
            new_owner[0] != '\0') {
            if (strcmp(name, SEURAT_NAME) == 0)
                send_seurat_wallpaper(bus);
            else if (strcmp(name, NAWI_NAME) == 0)
                query_layout(bus);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_signal(message, NAWI_INTERFACE, "Changed")) {
        const char *profile = NULL;

        if (dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &profile,
                                  DBUS_TYPE_INVALID))
            fravashid_app_set_layout(bus->app, profile);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(message, INTERFACE, "Next")) {
        fravashid_app_step(bus->app, 1);
        reply = dbus_message_new_method_return(message);
    } else if (dbus_message_is_method_call(message, INTERFACE, "Previous")) {
        fravashid_app_step(bus->app, -1);
        reply = dbus_message_new_method_return(message);
    } else if (dbus_message_is_method_call(message, INTERFACE, "GetCurrent")) {
        reply = dbus_message_new_method_return(message);
        if (reply)
            append_current(reply, bus->app);
    } else if (dbus_message_is_method_call(message,
                                           "org.freedesktop.DBus.Introspectable",
                                           "Introspect")) {
        const char *xml = introspection_xml;

        reply = dbus_message_new_method_return(message);
        if (reply)
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
    } else {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static const DBusObjectPathVTable vtable = {
    .message_function = handle_message,
};

static DBusHandlerResult filter_message(DBusConnection *connection,
                                        DBusMessage *message, void *data)
{
    if (dbus_message_is_signal(message, "org.freedesktop.DBus", "NameOwnerChanged") ||
        dbus_message_is_signal(message, NAWI_INTERFACE, "Changed"))
        return handle_message(connection, message, data);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int fravashid_bus_open(struct fravashid_bus *bus, int epfd, struct fravashid_app *app)
{
    DBusError error;

    memset(bus, 0, sizeof(*bus));
    bus->epfd = epfd;
    bus->app = app;

    dbus_error_init(&error);
    bus->connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!bus->connection) {
        fprintf(stderr, "fravashid: session bus: %s\n", error.message);
        dbus_error_free(&error);
        return -1;
    }
    dbus_connection_set_exit_on_disconnect(bus->connection, FALSE);

    if (dbus_bus_request_name(bus->connection, BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                              &error) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "fravashid: cannot own %s%s%s\n", BUS_NAME,
                dbus_error_is_set(&error) ? ": " : "",
                dbus_error_is_set(&error) ? error.message : "");
        dbus_error_free(&error);
        goto fail;
    }
    dbus_bus_add_match(bus->connection,
                       "type='signal',sender='org.freedesktop.DBus',"
                       "interface='org.freedesktop.DBus',member='NameOwnerChanged',"
                       "arg0='" SEURAT_NAME "'",
                       NULL);
    dbus_bus_add_match(bus->connection,
                       "type='signal',sender='org.freedesktop.DBus',"
                       "interface='org.freedesktop.DBus',member='NameOwnerChanged',"
                       "arg0='" NAWI_NAME "'",
                       NULL);
    dbus_bus_add_match(bus->connection,
                       "type='signal',sender='" NAWI_NAME "',path='" NAWI_PATH "',"
                       "interface='" NAWI_INTERFACE "',member='Changed'",
                       NULL);
    if (!dbus_connection_register_object_path(bus->connection, OBJECT_PATH, &vtable, bus) ||
        !dbus_connection_add_filter(bus->connection, filter_message, bus, NULL) ||
        !dbus_connection_set_watch_functions(bus->connection, add_watch, remove_watch,
                                             toggle_watch, bus, NULL)) {
        fprintf(stderr, "fravashid: cannot register D-Bus object\n");
        goto fail;
    }
    query_layout(bus);
    return 0;

fail:
    dbus_connection_close(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
    return -1;
}

void fravashid_bus_close(struct fravashid_bus *bus)
{
    free(bus->seurat_path);
    bus->seurat_path = NULL;
    if (!bus->connection)
        return;
    dbus_connection_close(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
}

void fravashid_bus_event(struct fravashid_bus *bus, struct fravashid_bus_watch *watch,
                         uint32_t events)
{
    unsigned int flags = 0;

    if (!bus->connection || !dbus_watch_get_enabled(watch->watch))
        return;
    if (events & EPOLLIN)
        flags |= DBUS_WATCH_READABLE;
    if (events & EPOLLOUT)
        flags |= DBUS_WATCH_WRITABLE;
    if (events & EPOLLERR)
        flags |= DBUS_WATCH_ERROR;
    if (events & EPOLLHUP)
        flags |= DBUS_WATCH_HANGUP;

    dbus_watch_handle(watch->watch, flags);
    while (dbus_connection_get_dispatch_status(bus->connection) == DBUS_DISPATCH_DATA_REMAINS)
        dbus_connection_dispatch(bus->connection);

    if (!dbus_connection_get_is_connected(bus->connection)) {
        fprintf(stderr, "fravashid: disconnected from session bus\n");
        fravashid_bus_close(bus);
    }
}

void fravashid_bus_emit_changed(struct fravashid_bus *bus, const char *output,
                                const char *path)
{
    DBusMessage *signal;

    if (!bus->connection)
        return;
    signal = dbus_message_new_signal(OBJECT_PATH, INTERFACE, "Changed");
    if (!signal)
        return;
    dbus_message_append_args(signal, DBUS_TYPE_STRING, &output, DBUS_TYPE_STRING,
                             &path, DBUS_TYPE_INVALID);
    dbus_connection_send(bus->connection, signal, NULL);
    dbus_message_unref(signal);
}

void fravashid_bus_set_seurat_wallpaper(struct fravashid_bus *bus, const char *path)
{
    char *copy = strdup(path);

    if (!copy)
        return;
    free(bus->seurat_path);
    bus->seurat_path = copy;
    send_seurat_wallpaper(bus);
}
