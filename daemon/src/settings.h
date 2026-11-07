/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_SETTINGS_H
#define FRAVASHID_SETTINGS_H

#include <cfgd/cfgd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "layout.h"

#define FRAVASHID_SETTINGS_PREFIX "/org/nuros/sanar/fravashi/"

struct fravashid_settings {
    char *wallpaper;
    enum fravashid_mode mode;
    uint32_t background;
    int64_t interval;
    bool shuffle;
    char **outputs;
    size_t outputs_count;
    char **profiles;
    size_t profiles_count;
};

void fravashid_settings_defaults(struct fravashid_settings *settings);
void fravashid_settings_load(cfgd_t *cfgd, struct fravashid_settings *settings);
void fravashid_settings_free(struct fravashid_settings *settings);
const char *fravashid_settings_spec(const struct fravashid_settings *settings,
                                    const char *output, const char *layout);

#endif
