/* SPDX-License-Identifier: GPL-3.0-only */
#include "paint.h"

#include <stddef.h>

void fravashid_fill(uint32_t *target, int target_w, int target_h, int stride_px,
                    uint32_t color)
{
    for (int y = 0; y < target_h; y++) {
        uint32_t *row = target + (size_t)y * stride_px;

        for (int x = 0; x < target_w; x++)
            row[x] = color;
    }
}

static uint32_t mix(uint32_t a, uint32_t b, uint32_t fraction)
{
    uint32_t out = 0;

    for (int shift = 0; shift < 24; shift += 8) {
        uint32_t ca = (a >> shift) & 0xff;
        uint32_t cb = (b >> shift) & 0xff;

        out |= ((ca * (256 - fraction) + cb * fraction) >> 8) << shift;
    }
    return out;
}

static uint32_t sample(const struct fravashid_image *image, int sx, int sy)
{
    return image->pixels[(size_t)sy * image->width + sx];
}

static void draw_scaled(uint32_t *target, int stride_px,
                        const struct fravashid_image *image,
                        const struct fravashid_place *place)
{
    const struct fravashid_rect *src = &place->src;
    const struct fravashid_rect *dst = &place->dst;

    for (int y = 0; y < dst->h; y++) {
        long long fy = ((long long)(2 * y + 1) * src->h * 128 / dst->h) - 128;
        int y0, y1;
        uint32_t wy;

        if (fy < 0)
            fy = 0;
        y0 = (int)(fy >> 8);
        wy = (uint32_t)(fy & 0xff);
        if (y0 > src->h - 1)
            y0 = src->h - 1;
        y1 = y0 + 1 < src->h ? y0 + 1 : y0;

        uint32_t *row = target + (size_t)(dst->y + y) * stride_px + dst->x;

        for (int x = 0; x < dst->w; x++) {
            long long fx = ((long long)(2 * x + 1) * src->w * 128 / dst->w) - 128;
            int x0, x1;
            uint32_t wx, top, bottom;

            if (fx < 0)
                fx = 0;
            x0 = (int)(fx >> 8);
            wx = (uint32_t)(fx & 0xff);
            if (x0 > src->w - 1)
                x0 = src->w - 1;
            x1 = x0 + 1 < src->w ? x0 + 1 : x0;

            top = mix(sample(image, src->x + x0, src->y + y0),
                      sample(image, src->x + x1, src->y + y0), wx);
            bottom = mix(sample(image, src->x + x0, src->y + y1),
                         sample(image, src->x + x1, src->y + y1), wx);
            row[x] = mix(top, bottom, wy);
        }
    }
}

static void draw_copy(uint32_t *target, int stride_px,
                      const struct fravashid_image *image,
                      const struct fravashid_place *place)
{
    for (int y = 0; y < place->dst.h; y++) {
        uint32_t *row = target + (size_t)(place->dst.y + y) * stride_px + place->dst.x;

        for (int x = 0; x < place->dst.w; x++)
            row[x] = sample(image, place->src.x + x, place->src.y + y);
    }
}

static void draw_tiled(uint32_t *target, int target_w, int target_h, int stride_px,
                       const struct fravashid_image *image)
{
    for (int y = 0; y < target_h; y++) {
        uint32_t *row = target + (size_t)y * stride_px;

        for (int x = 0; x < target_w; x++)
            row[x] = sample(image, x % image->width, y % image->height);
    }
}

void fravashid_paint(uint32_t *target, int target_w, int target_h, int stride_px,
                     const struct fravashid_image *image,
                     enum fravashid_mode mode, uint32_t background)
{
    struct fravashid_place place;

    if (fravashid_place_compute(mode, image->width, image->height, target_w,
                                target_h, &place) != 0) {
        fravashid_fill(target, target_w, target_h, stride_px, background);
        return;
    }
    if (mode == FRAVASHID_MODE_TILE) {
        draw_tiled(target, target_w, target_h, stride_px, image);
        return;
    }
    if (mode == FRAVASHID_MODE_FIT || mode == FRAVASHID_MODE_CENTER)
        fravashid_fill(target, target_w, target_h, stride_px, background);
    if (place.src.w == place.dst.w && place.src.h == place.dst.h)
        draw_copy(target, stride_px, image, &place);
    else
        draw_scaled(target, stride_px, image, &place);
}
