/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_PAINT_H
#define FRAVASHID_PAINT_H

#include <stdint.h>

#include "layout.h"

struct fravashid_image {
    int width;
    int height;
    uint32_t *pixels;
};

void fravashid_paint(uint32_t *target, int target_w, int target_h, int stride_px,
                     const struct fravashid_image *image,
                     enum fravashid_mode mode, uint32_t background);
void fravashid_fill(uint32_t *target, int target_w, int target_h, int stride_px,
                    uint32_t color);

#endif
