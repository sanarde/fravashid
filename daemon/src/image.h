/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_IMAGE_H
#define FRAVASHID_IMAGE_H

#include <stdint.h>

#include "paint.h"

int fravashid_image_load(const char *path, uint32_t background,
                         struct fravashid_image *image);
void fravashid_image_free(struct fravashid_image *image);

#endif
