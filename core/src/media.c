/* SPDX-License-Identifier: GPL-3.0-only */
#include "media.h"

#include <string.h>
#include <strings.h>

struct extension {
    const char *name;
    enum fravashid_media_kind kind;
};

static const struct extension extensions[] = {
    {"png", FRAVASHID_MEDIA_IMAGE},
    {"jpg", FRAVASHID_MEDIA_IMAGE},
    {"jpeg", FRAVASHID_MEDIA_IMAGE},
    {"mp4", FRAVASHID_MEDIA_VIDEO},
    {"mkv", FRAVASHID_MEDIA_VIDEO},
    {"webm", FRAVASHID_MEDIA_VIDEO},
};

enum fravashid_media_kind fravashid_media_kind(const char *path)
{
    const char *base = strrchr(path, '/');
    const char *dot;

    base = base ? base + 1 : path;
    dot = strrchr(base, '.');
    if (!dot || dot == base)
        return FRAVASHID_MEDIA_NONE;
    dot++;
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        if (strcasecmp(dot, extensions[i].name) == 0)
            return extensions[i].kind;
    }
    return FRAVASHID_MEDIA_NONE;
}
