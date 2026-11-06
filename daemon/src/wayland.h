/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_WAYLAND_H
#define FRAVASHID_WAYLAND_H

#include <stdint.h>

#include "output.h"

struct fravashid_wl;

struct fravashid_wl *fravashid_wl_open(const struct fravashid_events *events,
                                       void *user);
void fravashid_wl_close(struct fravashid_wl *wl);
int fravashid_wl_fd(struct fravashid_wl *wl);
int fravashid_wl_prepare(struct fravashid_wl *wl);
int fravashid_wl_finish(struct fravashid_wl *wl, int readable);
uint32_t *fravashid_wl_acquire(struct fravashid_output *output, int *stride_px);
void fravashid_wl_commit(struct fravashid_output *output);

#endif
