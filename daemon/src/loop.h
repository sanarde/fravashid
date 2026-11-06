/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FRAVASHID_LOOP_H
#define FRAVASHID_LOOP_H

enum fravashid_source_kind {
    FRAVASHID_SRC_SIGNAL,
    FRAVASHID_SRC_TIMER,
    FRAVASHID_SRC_CONFIG,
    FRAVASHID_SRC_DISPLAY,
    FRAVASHID_SRC_BUS,
    FRAVASHID_SRC_VIDEO,
};

struct fravashid_source {
    enum fravashid_source_kind kind;
};

#endif
