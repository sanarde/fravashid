/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_OUTPUT_H
#define FRAVASHID_OUTPUT_H

struct fravashid_output {
    char name[64];
    int width;
    int height;
    void *user;
};

struct fravashid_events {
    void (*output_ready)(struct fravashid_output *output, void *user);
    void (*output_gone)(struct fravashid_output *output, void *user);
};

#endif
