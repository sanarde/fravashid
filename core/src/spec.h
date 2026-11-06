/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_SPEC_H
#define FRAVASHID_SPEC_H

#include <stddef.h>
#include <stdint.h>

int fravashid_color_parse(const char *text, uint32_t *xrgb);
const char *fravashid_outputs_lookup(const char *const *entries, size_t count,
                                     const char *output);

#endif
