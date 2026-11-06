/* SPDX-License-Identifier: GPL-3.0-only */
#include "check.h"
#include "layout.h"

static void check_rect(struct fravashid_rect r, int x, int y, int w, int h)
{
    CHECK(r.x == x);
    CHECK(r.y == y);
    CHECK(r.w == w);
    CHECK(r.h == h);
}

static void check_modes(void)
{
    enum fravashid_mode mode;

    CHECK(fravashid_mode_parse("fill", &mode) == 0 && mode == FRAVASHID_MODE_FILL);
    CHECK(fravashid_mode_parse("tile", &mode) == 0 && mode == FRAVASHID_MODE_TILE);
    CHECK(fravashid_mode_parse("zoom", &mode) != 0);
    CHECK_STR(fravashid_mode_name(FRAVASHID_MODE_FIT), "fit");
    CHECK_STR(fravashid_mode_name(FRAVASHID_MODE_CENTER), "center");
}

static void check_fill(void)
{
    struct fravashid_place p;

    CHECK(fravashid_place_compute(FRAVASHID_MODE_FILL, 400, 400, 200, 100, &p) == 0);
    check_rect(p.src, 0, 100, 400, 200);
    check_rect(p.dst, 0, 0, 200, 100);

    CHECK(fravashid_place_compute(FRAVASHID_MODE_FILL, 400, 400, 100, 200, &p) == 0);
    check_rect(p.src, 100, 0, 200, 400);
    check_rect(p.dst, 0, 0, 100, 200);

    CHECK(fravashid_place_compute(FRAVASHID_MODE_FILL, 1920, 1080, 1920, 1080, &p) == 0);
    check_rect(p.src, 0, 0, 1920, 1080);
}

static void check_fit(void)
{
    struct fravashid_place p;

    CHECK(fravashid_place_compute(FRAVASHID_MODE_FIT, 400, 400, 200, 100, &p) == 0);
    check_rect(p.src, 0, 0, 400, 400);
    check_rect(p.dst, 50, 0, 100, 100);

    CHECK(fravashid_place_compute(FRAVASHID_MODE_FIT, 400, 400, 100, 200, &p) == 0);
    check_rect(p.dst, 0, 50, 100, 100);
}

static void check_center(void)
{
    struct fravashid_place p;

    CHECK(fravashid_place_compute(FRAVASHID_MODE_CENTER, 100, 100, 300, 200, &p) == 0);
    check_rect(p.src, 0, 0, 100, 100);
    check_rect(p.dst, 100, 50, 100, 100);

    CHECK(fravashid_place_compute(FRAVASHID_MODE_CENTER, 500, 100, 300, 200, &p) == 0);
    check_rect(p.src, 100, 0, 300, 100);
    check_rect(p.dst, 0, 50, 300, 100);
}

static void check_stretch_and_tile(void)
{
    struct fravashid_place p;

    CHECK(fravashid_place_compute(FRAVASHID_MODE_STRETCH, 10, 20, 300, 200, &p) == 0);
    check_rect(p.src, 0, 0, 10, 20);
    check_rect(p.dst, 0, 0, 300, 200);

    CHECK(fravashid_place_compute(FRAVASHID_MODE_TILE, 10, 20, 300, 200, &p) == 0);
    check_rect(p.src, 0, 0, 10, 20);
    check_rect(p.dst, 0, 0, 300, 200);
}

static void check_invalid(void)
{
    struct fravashid_place p;

    CHECK(fravashid_place_compute(FRAVASHID_MODE_FILL, 0, 10, 10, 10, &p) != 0);
    CHECK(fravashid_place_compute(FRAVASHID_MODE_FILL, 10, 10, 10, 0, &p) != 0);
}

static void check_extreme_aspect(void)
{
    struct fravashid_place p;

    CHECK(fravashid_place_compute(FRAVASHID_MODE_FILL, 1, 10000, 10000, 1, &p) == 0);
    CHECK(p.src.w >= 1 && p.src.h >= 1);
    CHECK(fravashid_place_compute(FRAVASHID_MODE_FIT, 1, 10000, 10000, 1, &p) == 0);
    CHECK(p.dst.w >= 1 && p.dst.h >= 1);
}

int main(void)
{
    check_modes();
    check_fill();
    check_fit();
    check_center();
    check_stretch_and_tile();
    check_invalid();
    check_extreme_aspect();
    return check_failures ? 1 : 0;
}
