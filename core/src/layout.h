/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_LAYOUT_H
#define FRAVASHID_LAYOUT_H

enum fravashid_mode {
    FRAVASHID_MODE_FILL,
    FRAVASHID_MODE_FIT,
    FRAVASHID_MODE_STRETCH,
    FRAVASHID_MODE_CENTER,
    FRAVASHID_MODE_TILE,
};

struct fravashid_rect {
    int x;
    int y;
    int w;
    int h;
};

struct fravashid_place {
    struct fravashid_rect src;
    struct fravashid_rect dst;
};

int fravashid_mode_parse(const char *name, enum fravashid_mode *mode);
const char *fravashid_mode_name(enum fravashid_mode mode);
int fravashid_place_compute(enum fravashid_mode mode, int image_w, int image_h,
                            int output_w, int output_h,
                            struct fravashid_place *place);

#endif
