/* SPDX-License-Identifier: GPL-3.0-only */
#include <cfgd/cfgd.h>
#include <dbus/dbus.h>
#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _(text) gettext(text)

#define BUS_NAME "org.nuros.sanar.Fravashi"
#define OBJECT_PATH "/org/nuros/sanar/fravashi"
#define INTERFACE "org.nuros.sanar.Fravashi"
#define KEY_WALLPAPER "/org/nuros/sanar/fravashi/wallpaper"
#define KEY_OUTPUTS "/org/nuros/sanar/fravashi/outputs"

static DBusMessage *call(const char *method)
{
    DBusError error;
    DBusConnection *connection;
    DBusMessage *message;
    DBusMessage *reply;

    dbus_error_init(&error);
    connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!connection) {
        fprintf(stderr, _("fravashi: cannot connect to the session bus: %s\n"),
                error.message);
        dbus_error_free(&error);
        return NULL;
    }
    message = dbus_message_new_method_call(BUS_NAME, OBJECT_PATH, INTERFACE, method);
    if (!message)
        return NULL;
    reply = dbus_connection_send_with_reply_and_block(connection, message, 5000, &error);
    dbus_message_unref(message);
    if (!reply) {
        fprintf(stderr, _("fravashi: fravashid does not answer: %s\n"), error.message);
        dbus_error_free(&error);
    }
    return reply;
}

static int command_step(const char *method)
{
    DBusMessage *reply = call(method);

    if (!reply)
        return 1;
    dbus_message_unref(reply);
    return 0;
}

static int command_get(void)
{
    DBusMessage *reply = call("GetCurrent");
    DBusMessageIter iter, array, entry;

    if (!reply)
        return 1;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_DICT_ENTRY) {
        const char *output;
        const char *path;

        dbus_message_iter_recurse(&array, &entry);
        dbus_message_iter_get_basic(&entry, &output);
        dbus_message_iter_next(&entry);
        dbus_message_iter_get_basic(&entry, &path);
        printf("%s\t%s\n", output, path);
        dbus_message_iter_next(&array);
    }
    dbus_message_unref(reply);
    return 0;
}

static int set_output(cfgd_t *cfgd, const char *output, const char *path)
{
    size_t count;
    size_t length = strlen(output);
    char **list = cfgd_get_strv(cfgd, KEY_OUTPUTS, &count);
    const char **items = calloc(count + 1, sizeof(*items));
    char *entry;
    size_t n = 0;
    int status;

    if (!list || !items) {
        cfgd_strv_free(list, count);
        free(items);
        return CFGD_ERR_FAILED;
    }
    if (asprintf(&entry, "%s=%s", output, path) < 0) {
        cfgd_strv_free(list, count);
        free(items);
        return CFGD_ERR_FAILED;
    }
    for (size_t i = 0; i < count; i++) {
        if (strncmp(list[i], output, length) != 0 || list[i][length] != '=')
            items[n++] = list[i];
    }
    items[n++] = entry;
    status = cfgd_set_strv(cfgd, KEY_OUTPUTS, items, n);
    cfgd_strv_free(list, count);
    free(items);
    free(entry);
    return status;
}

static int command_set(int argc, char **argv)
{
    const char *output = NULL;
    const char *path;
    char resolved[PATH_MAX];
    cfgd_t *cfgd;
    int status;

    if (argc == 5 && strcmp(argv[2], "-o") == 0) {
        output = argv[3];
        path = argv[4];
    } else if (argc == 3) {
        path = argv[2];
    } else {
        fprintf(stderr, _("usage: fravashi set [-o OUTPUT] PATH\n"));
        return 2;
    }
    if (path[0] != '\0' && !realpath(path, resolved)) {
        fprintf(stderr, _("fravashi: cannot resolve %s\n"), path);
        return 1;
    }
    if (path[0] == '\0')
        resolved[0] = '\0';

    cfgd = cfgd_open();
    if (!cfgd) {
        fprintf(stderr, _("fravashi: cannot open the settings\n"));
        return 1;
    }
    status = output ? set_output(cfgd, output, resolved)
                    : cfgd_set_string(cfgd, KEY_WALLPAPER, resolved);
    cfgd_close(cfgd);
    if (status != CFGD_OK) {
        fprintf(stderr, _("fravashi: cannot save the setting (%d)\n"), status);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    bindtextdomain("fravashid", FRAVASHID_LOCALEDIR);
    textdomain("fravashid");

    if (argc == 2 && strcmp(argv[1], "get") == 0)
        return command_get();
    if (argc == 2 && strcmp(argv[1], "next") == 0)
        return command_step("Next");
    if (argc == 2 && strcmp(argv[1], "prev") == 0)
        return command_step("Previous");
    if (argc >= 2 && strcmp(argv[1], "set") == 0)
        return command_set(argc, argv);

    fprintf(stderr, _("usage: fravashi get | next | prev | set [-o OUTPUT] PATH\n"));
    return 2;
}
