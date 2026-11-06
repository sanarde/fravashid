/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_MEDIA_H
#define FRAVASHID_MEDIA_H

enum fravashid_media_kind {
    FRAVASHID_MEDIA_NONE,
    FRAVASHID_MEDIA_IMAGE,
    FRAVASHID_MEDIA_VIDEO,
};

enum fravashid_media_kind fravashid_media_kind(const char *path);

#endif
