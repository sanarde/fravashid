/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_X11_H
#define FRAVASHID_X11_H

#include <stdint.h>

#include "output.h"

struct fravashid_x11;

struct fravashid_x11 *fravashid_x11_open(const struct fravashid_events *events,
                                         void *user);
void fravashid_x11_close(struct fravashid_x11 *x11);
int fravashid_x11_fd(struct fravashid_x11 *x11);
int fravashid_x11_dispatch(struct fravashid_x11 *x11);
uint32_t *fravashid_x11_acquire(struct fravashid_output *output, int *stride_px);
void fravashid_x11_commit(struct fravashid_output *output);

#endif
