/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>

#include "check.h"
#include "paint.h"

#define RED 0xff0000
#define BLUE 0x0000ff
#define BG 0x101010

static void check_stretch_solid(void)
{
    uint32_t pixels[4] = {RED, RED, RED, RED};
    struct fravashid_image image = {2, 2, pixels};
    uint32_t target[6 * 5];

    fravashid_paint(target, 6, 5, 6, &image, FRAVASHID_MODE_STRETCH, BG);
    for (int i = 0; i < 6 * 5; i++)
        CHECK(target[i] == RED);
}

static void check_fit_bars(void)
{
    uint32_t pixels[4] = {RED, RED, RED, RED};
    struct fravashid_image image = {2, 2, pixels};
    uint32_t target[8 * 4];

    fravashid_paint(target, 8, 4, 8, &image, FRAVASHID_MODE_FIT, BG);
    CHECK(target[0] == BG);
    CHECK(target[1 * 8 + 1] == BG);
    CHECK(target[1 * 8 + 3] == RED);
    CHECK(target[1 * 8 + 4] == RED);
    CHECK(target[1 * 8 + 7] == BG);
}

static void check_tile(void)
{
    uint32_t pixels[2] = {RED, BLUE};
    struct fravashid_image image = {2, 1, pixels};
    uint32_t target[5 * 2];

    fravashid_paint(target, 5, 2, 5, &image, FRAVASHID_MODE_TILE, BG);
    CHECK(target[0] == RED);
    CHECK(target[1] == BLUE);
    CHECK(target[2] == RED);
    CHECK(target[4] == RED);
    CHECK(target[5 + 3] == BLUE);
}

static void check_center_copy(void)
{
    uint32_t pixels[4] = {RED, BLUE, BLUE, RED};
    struct fravashid_image image = {2, 2, pixels};
    uint32_t target[4 * 4];

    fravashid_paint(target, 4, 4, 4, &image, FRAVASHID_MODE_CENTER, BG);
    CHECK(target[0] == BG);
    CHECK(target[1 * 4 + 1] == RED);
    CHECK(target[1 * 4 + 2] == BLUE);
    CHECK(target[2 * 4 + 1] == BLUE);
    CHECK(target[2 * 4 + 2] == RED);
    CHECK(target[3 * 4 + 3] == BG);
}

static void check_stride(void)
{
    uint32_t pixels[1] = {RED};
    struct fravashid_image image = {1, 1, pixels};
    uint32_t target[3 * 2];

    fravashid_fill(target, 3, 2, 3, BG);
    fravashid_paint(target, 2, 2, 3, &image, FRAVASHID_MODE_STRETCH, BG);
    CHECK(target[0] == RED && target[1] == RED);
    CHECK(target[2] == BG);
    CHECK(target[3] == RED && target[4] == RED);
    CHECK(target[5] == BG);
}

int main(void)
{
    check_stretch_solid();
    check_fit_bars();
    check_tile();
    check_center_copy();
    check_stride();
    return check_failures ? 1 : 0;
}
