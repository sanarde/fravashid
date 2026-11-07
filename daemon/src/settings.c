/* SPDX-License-Identifier: GPL-3.0-only */
#include "settings.h"

#include <stdlib.h>
#include <string.h>

#include "spec.h"

#define KEY_WALLPAPER FRAVASHID_SETTINGS_PREFIX "wallpaper"
#define KEY_MODE FRAVASHID_SETTINGS_PREFIX "mode"
#define KEY_COLOR FRAVASHID_SETTINGS_PREFIX "color"
#define KEY_INTERVAL FRAVASHID_SETTINGS_PREFIX "interval"
#define KEY_SHUFFLE FRAVASHID_SETTINGS_PREFIX "shuffle"
#define KEY_OUTPUTS FRAVASHID_SETTINGS_PREFIX "outputs"
#define KEY_PROFILES FRAVASHID_SETTINGS_PREFIX "profiles"

void fravashid_settings_defaults(struct fravashid_settings *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->wallpaper = strdup("");
    settings->mode = FRAVASHID_MODE_FILL;
}

void fravashid_settings_load(cfgd_t *cfgd, struct fravashid_settings *settings)
{
    char *text;
    int64_t integer;
    bool flag;
    char **list;
    size_t count;

    fravashid_settings_free(settings);
    fravashid_settings_defaults(settings);

    text = cfgd_get_string(cfgd, KEY_WALLPAPER);
    if (text) {
        free(settings->wallpaper);
        settings->wallpaper = text;
    }
    text = cfgd_get_string(cfgd, KEY_MODE);
    if (text) {
        fravashid_mode_parse(text, &settings->mode);
        free(text);
    }
    text = cfgd_get_string(cfgd, KEY_COLOR);
    if (text) {
        fravashid_color_parse(text, &settings->background);
        free(text);
    }
    if (cfgd_get_int(cfgd, KEY_INTERVAL, &integer))
        settings->interval = integer;
    if (cfgd_get_bool(cfgd, KEY_SHUFFLE, &flag))
        settings->shuffle = flag;
    list = cfgd_get_strv(cfgd, KEY_OUTPUTS, &count);
    if (list) {
        settings->outputs = list;
        settings->outputs_count = count;
    }
    list = cfgd_get_strv(cfgd, KEY_PROFILES, &count);
    if (list) {
        settings->profiles = list;
        settings->profiles_count = count;
    }
}

void fravashid_settings_free(struct fravashid_settings *settings)
{
    free(settings->wallpaper);
    cfgd_strv_free(settings->outputs, settings->outputs_count);
    cfgd_strv_free(settings->profiles, settings->profiles_count);
    memset(settings, 0, sizeof(*settings));
}

const char *fravashid_settings_spec(const struct fravashid_settings *settings,
                                    const char *output, const char *layout)
{
    const char *spec = fravashid_outputs_lookup(
        (const char *const *)settings->outputs, settings->outputs_count, output);

    if (!spec && layout[0] != '\0')
        spec = fravashid_outputs_lookup((const char *const *)settings->profiles,
                                        settings->profiles_count, layout);
    return spec ? spec : settings->wallpaper;
}
