/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"
#include "playlist.h"

static void touch(const char *dir, const char *name)
{
    char path[512];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "w");
    if (f)
        fclose(f);
}

static void remove_in(const char *dir, const char *name)
{
    char path[512];

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    unlink(path);
}

static int ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);

    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static void check_load_and_walk(const char *dir)
{
    struct fravashid_playlist list;

    CHECK(fravashid_playlist_load(&list, dir) == 0);
    CHECK(list.count == 3);
    CHECK(ends_with(fravashid_playlist_current(&list), "/a.png"));
    CHECK(ends_with(fravashid_playlist_advance(&list, 1), "/b.JPG"));
    CHECK(ends_with(fravashid_playlist_advance(&list, 1), "/c.mp4"));
    CHECK(ends_with(fravashid_playlist_advance(&list, 1), "/a.png"));
    CHECK(ends_with(fravashid_playlist_advance(&list, -1), "/c.mp4"));
    CHECK(ends_with(fravashid_playlist_advance(&list, -4), "/b.JPG"));
    fravashid_playlist_free(&list);
}

static void check_shuffle(const char *dir)
{
    struct fravashid_playlist a, b;
    int seen[3] = {0, 0, 0};

    CHECK(fravashid_playlist_load(&a, dir) == 0);
    CHECK(fravashid_playlist_load(&b, dir) == 0);
    fravashid_playlist_shuffle(&a, 7);
    fravashid_playlist_shuffle(&b, 7);
    for (size_t i = 0; i < a.count; i++) {
        CHECK_STR(a.paths[i], b.paths[i]);
        if (ends_with(a.paths[i], "/a.png"))
            seen[0]++;
        if (ends_with(a.paths[i], "/b.JPG"))
            seen[1]++;
        if (ends_with(a.paths[i], "/c.mp4"))
            seen[2]++;
    }
    CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
    fravashid_playlist_free(&a);
    fravashid_playlist_free(&b);
}

static void check_missing_and_empty(const char *dir)
{
    struct fravashid_playlist list;
    char missing[512];

    snprintf(missing, sizeof(missing), "%s/none", dir);
    CHECK(fravashid_playlist_load(&list, missing) != 0);
    CHECK(list.count == 0);
    CHECK(fravashid_playlist_current(&list) == NULL);
    CHECK(fravashid_playlist_advance(&list, 1) == NULL);
}

int main(void)
{
    char dir[] = "/tmp/fravashid-playlist-XXXXXX";
    char sub[512];

    if (!mkdtemp(dir))
        return 1;
    touch(dir, "b.JPG");
    touch(dir, "a.png");
    touch(dir, "c.mp4");
    touch(dir, "notes.txt");
    touch(dir, ".hidden.png");
    snprintf(sub, sizeof(sub), "%s/folder.png", dir);
    mkdir(sub, 0700);

    check_load_and_walk(dir);
    check_shuffle(dir);
    check_missing_and_empty(dir);

    rmdir(sub);
    remove_in(dir, "b.JPG");
    remove_in(dir, "a.png");
    remove_in(dir, "c.mp4");
    remove_in(dir, "notes.txt");
    remove_in(dir, ".hidden.png");
    rmdir(dir);
    return check_failures ? 1 : 0;
}
