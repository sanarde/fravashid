/* SPDX-License-Identifier: GPL-3.0-only */
#include "check.h"
#include "settings.h"

int main(void)
{
    char *outputs[] = {"DP-1=/pics/dp1.png"};
    char *profiles[] = {"desk=/pics/desk", "dock=/pics/dock.png"};
    struct fravashid_settings settings = {
        .wallpaper = "/pics/default.png",
        .outputs = outputs,
        .outputs_count = 1,
        .profiles = profiles,
        .profiles_count = 2,
    };

    CHECK_STR(fravashid_settings_spec(&settings, "DP-1", "desk"), "/pics/dp1.png");
    CHECK_STR(fravashid_settings_spec(&settings, "eDP-1", "desk"), "/pics/desk");
    CHECK_STR(fravashid_settings_spec(&settings, "eDP-1", "dock"), "/pics/dock.png");
    CHECK_STR(fravashid_settings_spec(&settings, "eDP-1", "other"), "/pics/default.png");
    CHECK_STR(fravashid_settings_spec(&settings, "eDP-1", ""), "/pics/default.png");
    CHECK_STR(fravashid_settings_spec(&settings, "DP-1", ""), "/pics/dp1.png");

    return check_failures != 0;
}
