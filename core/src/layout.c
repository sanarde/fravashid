/* SPDX-License-Identifier: GPL-3.0-only */
#include "layout.h"

#include <string.h>

static const char *const mode_names[] = {
    [FRAVASHID_MODE_FILL] = "fill",
    [FRAVASHID_MODE_FIT] = "fit",
    [FRAVASHID_MODE_STRETCH] = "stretch",
    [FRAVASHID_MODE_CENTER] = "center",
    [FRAVASHID_MODE_TILE] = "tile",
};

int fravashid_mode_parse(const char *name, enum fravashid_mode *mode)
{
    for (size_t i = 0; i < sizeof(mode_names) / sizeof(mode_names[0]); i++) {
        if (strcmp(name, mode_names[i]) == 0) {
            *mode = (enum fravashid_mode)i;
            return 0;
        }
    }
    return -1;
}

const char *fravashid_mode_name(enum fravashid_mode mode)
{
    return mode_names[mode];
}

static int at_least_one(long long v)
{
    return v < 1 ? 1 : (int)v;
}

static struct fravashid_rect rect(int x, int y, int w, int h)
{
    struct fravashid_rect r = {x, y, w, h};

    return r;
}

static void place_fill(struct fravashid_place *p, int iw, int ih, int ow, int oh)
{
    int sw = iw;
    int sh = ih;

    if ((long long)ow * ih > (long long)oh * iw)
        sh = at_least_one((long long)oh * iw / ow);
    else
        sw = at_least_one((long long)ow * ih / oh);
    p->src = rect((iw - sw) / 2, (ih - sh) / 2, sw, sh);
    p->dst = rect(0, 0, ow, oh);
}

static void place_fit(struct fravashid_place *p, int iw, int ih, int ow, int oh)
{
    int dw = ow;
    int dh = oh;

    if ((long long)ow * ih > (long long)oh * iw)
        dw = at_least_one((long long)iw * oh / ih);
    else
        dh = at_least_one((long long)ih * ow / iw);
    p->src = rect(0, 0, iw, ih);
    p->dst = rect((ow - dw) / 2, (oh - dh) / 2, dw, dh);
}

static void place_center(struct fravashid_place *p, int iw, int ih, int ow, int oh)
{
    int w = iw < ow ? iw : ow;
    int h = ih < oh ? ih : oh;

    p->src = rect((iw - w) / 2, (ih - h) / 2, w, h);
    p->dst = rect((ow - w) / 2, (oh - h) / 2, w, h);
}

int fravashid_place_compute(enum fravashid_mode mode, int image_w, int image_h,
                            int output_w, int output_h,
                            struct fravashid_place *place)
{
    if (image_w < 1 || image_h < 1 || output_w < 1 || output_h < 1)
        return -1;

    switch (mode) {
    case FRAVASHID_MODE_FILL:
        place_fill(place, image_w, image_h, output_w, output_h);
        return 0;
    case FRAVASHID_MODE_FIT:
        place_fit(place, image_w, image_h, output_w, output_h);
        return 0;
    case FRAVASHID_MODE_CENTER:
        place_center(place, image_w, image_h, output_w, output_h);
        return 0;
    case FRAVASHID_MODE_STRETCH:
    case FRAVASHID_MODE_TILE:
        place->src = rect(0, 0, image_w, image_h);
        place->dst = rect(0, 0, output_w, output_h);
        return 0;
    }
    return -1;
}
