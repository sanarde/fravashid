/* SPDX-License-Identifier: GPL-3.0-only */
#include "check.h"
#include "image.h"

#ifndef TEST_DATA_DIR
#error TEST_DATA_DIR is not defined
#endif

static int near(uint32_t a, uint32_t b)
{
    for (int shift = 0; shift < 24; shift += 8) {
        int d = (int)((a >> shift) & 0xff) - (int)((b >> shift) & 0xff);

        if (d > 8 || d < -8)
            return 0;
    }
    return 1;
}

static void check_png(void)
{
    struct fravashid_image image;

    CHECK(fravashid_image_load(TEST_DATA_DIR "/red_blue.png", 0, &image) == 0);
    CHECK(image.width == 4 && image.height == 2);
    CHECK((image.pixels[0] & 0xffffff) == 0xff0000);
    CHECK((image.pixels[3] & 0xffffff) == 0x0000ff);
    CHECK((image.pixels[4 + 1] & 0xffffff) == 0xff0000);
    fravashid_image_free(&image);
    CHECK(image.pixels == NULL);
}

static void check_jpeg(void)
{
    struct fravashid_image image;

    CHECK(fravashid_image_load(TEST_DATA_DIR "/red_blue.jpg", 0, &image) == 0);
    CHECK(image.width == 4 && image.height == 2);
    CHECK(near(image.pixels[0] & 0xffffff, 0xff0000));
    CHECK(near(image.pixels[3] & 0xffffff, 0x0000ff));
    fravashid_image_free(&image);
}

static void check_alpha_background(void)
{
    struct fravashid_image image;

    CHECK(fravashid_image_load(TEST_DATA_DIR "/alpha.png", 0x00ff00, &image) == 0);
    CHECK((image.pixels[0] & 0xffffff) == 0x00ff00);
    fravashid_image_free(&image);
}

static void check_failures_reported(void)
{
    struct fravashid_image image;

    CHECK(fravashid_image_load(TEST_DATA_DIR "/missing.png", 0, &image) != 0);
    CHECK(image.pixels == NULL);
    CHECK(fravashid_image_load(TEST_DATA_DIR "/../meson.build", 0, &image) != 0);
}

int main(void)
{
    check_png();
    check_jpeg();
    check_alpha_background();
    check_failures_reported();
    return check_failures ? 1 : 0;
}
