/* SPDX-License-Identifier: GPL-3.0-only */
#include "check.h"
#include "media.h"

int main(void)
{
    CHECK(fravashid_media_kind("a.png") == FRAVASHID_MEDIA_IMAGE);
    CHECK(fravashid_media_kind("/x/y/a.JPG") == FRAVASHID_MEDIA_IMAGE);
    CHECK(fravashid_media_kind("a.b.jpeg") == FRAVASHID_MEDIA_IMAGE);
    CHECK(fravashid_media_kind("clip.webm") == FRAVASHID_MEDIA_VIDEO);
    CHECK(fravashid_media_kind("/x.d/clip.MKV") == FRAVASHID_MEDIA_VIDEO);
    CHECK(fravashid_media_kind("clip.mp4") == FRAVASHID_MEDIA_VIDEO);
    CHECK(fravashid_media_kind("notes.txt") == FRAVASHID_MEDIA_NONE);
    CHECK(fravashid_media_kind("png") == FRAVASHID_MEDIA_NONE);
    CHECK(fravashid_media_kind(".png") == FRAVASHID_MEDIA_NONE);
    CHECK(fravashid_media_kind("/x.png/file") == FRAVASHID_MEDIA_NONE);
    CHECK(fravashid_media_kind("a.") == FRAVASHID_MEDIA_NONE);
    return check_failures ? 1 : 0;
}
