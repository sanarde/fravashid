/* SPDX-License-Identifier: GPL-3.0-only */
#include "image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <jpeglib.h>
#include <png.h>

#define MAX_PIXELS (1u << 28)

struct jpeg_failure {
    struct jpeg_error_mgr manager;
    jmp_buf jump;
};

static void jpeg_fail(j_common_ptr cinfo)
{
    struct jpeg_failure *failure = (struct jpeg_failure *)cinfo->err;

    longjmp(failure->jump, 1);
}

static void blend_over(uint32_t *pixels, size_t count, uint32_t background)
{
    uint32_t br = (background >> 16) & 0xff;
    uint32_t bg = (background >> 8) & 0xff;
    uint32_t bb = background & 0xff;

    for (size_t i = 0; i < count; i++) {
        uint32_t p = pixels[i];
        uint32_t a = p >> 24;
        uint32_t r = (((p >> 16) & 0xff) * a + br * (255 - a)) / 255;
        uint32_t g = (((p >> 8) & 0xff) * a + bg * (255 - a)) / 255;
        uint32_t b = ((p & 0xff) * a + bb * (255 - a)) / 255;

        pixels[i] = (r << 16) | (g << 8) | b;
    }
}

static int load_png(const char *path, uint32_t background,
                    struct fravashid_image *image)
{
    png_image png;

    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&png, path))
        return -1;
    if (png.width == 0 || png.height == 0 ||
        (uint64_t)png.width * png.height > MAX_PIXELS) {
        png_image_free(&png);
        return -1;
    }
    png.format = PNG_FORMAT_BGRA;
    image->width = (int)png.width;
    image->height = (int)png.height;
    image->pixels = malloc((size_t)png.width * png.height * sizeof(uint32_t));
    if (!image->pixels) {
        png_image_free(&png);
        return -1;
    }
    if (!png_image_finish_read(&png, NULL, image->pixels, 0, NULL)) {
        free(image->pixels);
        image->pixels = NULL;
        png_image_free(&png);
        return -1;
    }
    blend_over(image->pixels, (size_t)image->width * image->height, background);
    return 0;
}

static int load_jpeg(FILE *file, struct fravashid_image *image)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_failure failure;
    uint32_t *volatile pixels = NULL;

    cinfo.err = jpeg_std_error(&failure.manager);
    failure.manager.error_exit = jpeg_fail;
    if (setjmp(failure.jump)) {
        jpeg_destroy_decompress(&cinfo);
        free(pixels);
        return -1;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_EXT_BGRX;
    if (cinfo.image_width == 0 || cinfo.image_height == 0 ||
        (uint64_t)cinfo.image_width * cinfo.image_height > MAX_PIXELS)
        longjmp(failure.jump, 1);
    pixels = malloc((size_t)cinfo.image_width * cinfo.image_height *
                    sizeof(uint32_t));
    if (!pixels)
        longjmp(failure.jump, 1);
    jpeg_start_decompress(&cinfo);
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = (JSAMPROW)(pixels +
                                  (size_t)cinfo.output_scanline * cinfo.output_width);

        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    image->width = (int)cinfo.output_width;
    image->height = (int)cinfo.output_height;
    image->pixels = pixels;
    jpeg_destroy_decompress(&cinfo);
    return 0;
}

int fravashid_image_load(const char *path, uint32_t background,
                         struct fravashid_image *image)
{
    unsigned char magic[4];
    FILE *file = fopen(path, "rb");
    size_t n;
    int rc;

    image->width = 0;
    image->height = 0;
    image->pixels = NULL;
    if (!file)
        return -1;
    n = fread(magic, 1, sizeof(magic), file);
    if (n == sizeof(magic) && memcmp(magic, "\x89PNG", 4) == 0) {
        fclose(file);
        return load_png(path, background, image);
    }
    if (n >= 2 && magic[0] == 0xff && magic[1] == 0xd8) {
        rewind(file);
        rc = load_jpeg(file, image);
        fclose(file);
        return rc;
    }
    fclose(file);
    return -1;
}

void fravashid_image_free(struct fravashid_image *image)
{
    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}
