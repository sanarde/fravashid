/* SPDX-License-Identifier: GPL-3.0-only */
#include "playlist.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "media.h"

static int compare_paths(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int is_regular_file(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int append(struct fravashid_playlist *list, size_t *capacity, char *path)
{
    if (list->count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 16;
        char **grown = realloc(list->paths, next * sizeof(*grown));

        if (!grown)
            return -1;
        list->paths = grown;
        *capacity = next;
    }
    list->paths[list->count++] = path;
    return 0;
}

int fravashid_playlist_load(struct fravashid_playlist *list, const char *dir)
{
    DIR *handle = opendir(dir);
    struct dirent *entry;
    size_t capacity = 0;

    list->paths = NULL;
    list->count = 0;
    list->current = 0;
    if (!handle)
        return -1;

    while ((entry = readdir(handle))) {
        char *path;

        if (entry->d_name[0] == '.')
            continue;
        if (fravashid_media_kind(entry->d_name) == FRAVASHID_MEDIA_NONE)
            continue;
        if (asprintf(&path, "%s/%s", dir, entry->d_name) < 0)
            goto fail;
        if (!is_regular_file(path)) {
            free(path);
            continue;
        }
        if (append(list, &capacity, path) != 0) {
            free(path);
            goto fail;
        }
    }
    closedir(handle);
    if (list->count > 1)
        qsort(list->paths, list->count, sizeof(*list->paths), compare_paths);
    return 0;

fail:
    closedir(handle);
    fravashid_playlist_free(list);
    return -1;
}

int fravashid_playlist_load_path(struct fravashid_playlist *list, const char *path)
{
    struct stat st;

    list->paths = NULL;
    list->count = 0;
    list->current = 0;
    if (stat(path, &st) != 0)
        return -1;
    if (S_ISDIR(st.st_mode))
        return fravashid_playlist_load(list, path);
    if (!S_ISREG(st.st_mode) || fravashid_media_kind(path) == FRAVASHID_MEDIA_NONE)
        return -1;
    list->paths = malloc(sizeof(*list->paths));
    if (!list->paths)
        return -1;
    list->paths[0] = strdup(path);
    if (!list->paths[0]) {
        free(list->paths);
        list->paths = NULL;
        return -1;
    }
    list->count = 1;
    return 0;
}

void fravashid_playlist_free(struct fravashid_playlist *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->paths[i]);
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
    list->current = 0;
}

static uint64_t next_random(uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void fravashid_playlist_shuffle(struct fravashid_playlist *list, unsigned seed)
{
    uint64_t state = seed;

    for (size_t i = list->count; i > 1; i--) {
        size_t j = next_random(&state) % i;
        char *tmp = list->paths[i - 1];

        list->paths[i - 1] = list->paths[j];
        list->paths[j] = tmp;
    }
    list->current = 0;
}

const char *fravashid_playlist_current(const struct fravashid_playlist *list)
{
    return list->count ? list->paths[list->current] : NULL;
}

const char *fravashid_playlist_advance(struct fravashid_playlist *list, int step)
{
    long long next;

    if (!list->count)
        return NULL;
    next = ((long long)list->current + step) % (long long)list->count;
    if (next < 0)
        next += (long long)list->count;
    list->current = (size_t)next;
    return list->paths[list->current];
}
