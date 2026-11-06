/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_PLAYLIST_H
#define FRAVASHID_PLAYLIST_H

#include <stddef.h>

struct fravashid_playlist {
    char **paths;
    size_t count;
    size_t current;
};

int fravashid_playlist_load(struct fravashid_playlist *list, const char *dir);
void fravashid_playlist_free(struct fravashid_playlist *list);
void fravashid_playlist_shuffle(struct fravashid_playlist *list, unsigned seed);
const char *fravashid_playlist_current(const struct fravashid_playlist *list);
const char *fravashid_playlist_advance(struct fravashid_playlist *list, int step);

#endif
