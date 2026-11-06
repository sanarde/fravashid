/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_VIDEO_H
#define FRAVASHID_VIDEO_H

#include <stdbool.h>
#include <stdint.h>

#include "layout.h"
#include "loop.h"

struct fravashid_video;

struct fravashid_video *fravashid_video_open(int epfd, void *user);
void fravashid_video_close(struct fravashid_video *video, int epfd);
void *fravashid_video_user(const struct fravashid_video *video);
int fravashid_video_play(struct fravashid_video *video, const char *path,
                         enum fravashid_mode mode, uint32_t background);
bool fravashid_video_update(struct fravashid_video *video);
void fravashid_video_render(struct fravashid_video *video, uint32_t *target,
                            int width, int height, int stride_px);

#endif
