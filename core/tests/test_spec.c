/* SPDX-License-Identifier: GPL-3.0-only */
#include "check.h"
#include "spec.h"

int main(void)
{
    uint32_t color = 0;
    const char *const entries[] = {"DP-1=/a/b.png", "DP-10=/c", "HDMI-A-1=/d dir"};

    CHECK(fravashid_color_parse("#1a2B3c", &color) == 0 && color == 0x1a2b3c);
    CHECK(fravashid_color_parse("ffffff", &color) == 0 && color == 0xffffff);
    CHECK(fravashid_color_parse("#fff", &color) != 0);
    CHECK(fravashid_color_parse("#12345g", &color) != 0);
    CHECK(fravashid_color_parse("", &color) != 0);

    CHECK_STR(fravashid_outputs_lookup(entries, 3, "DP-1"), "/a/b.png");
    CHECK_STR(fravashid_outputs_lookup(entries, 3, "DP-10"), "/c");
    CHECK_STR(fravashid_outputs_lookup(entries, 3, "HDMI-A-1"), "/d dir");
    CHECK(fravashid_outputs_lookup(entries, 3, "DP") == NULL);
    CHECK(fravashid_outputs_lookup(entries, 0, "DP-1") == NULL);
    return check_failures ? 1 : 0;
}
