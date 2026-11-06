/* SPDX-License-Identifier: GPL-3.0-only */
#include "spec.h"

#include <stdlib.h>
#include <string.h>

int fravashid_color_parse(const char *text, uint32_t *xrgb)
{
    char *end;
    unsigned long v;

    if (*text == '#')
        text++;
    if (strlen(text) != 6 || strspn(text, "0123456789abcdefABCDEF") != 6)
        return -1;
    v = strtoul(text, &end, 16);
    if (*end != '\0')
        return -1;
    *xrgb = (uint32_t)v;
    return 0;
}

const char *fravashid_outputs_lookup(const char *const *entries, size_t count,
                                     const char *output)
{
    size_t length = strlen(output);

    for (size_t i = 0; i < count; i++) {
        if (strncmp(entries[i], output, length) == 0 && entries[i][length] == '=')
            return entries[i] + length + 1;
    }
    return NULL;
}
