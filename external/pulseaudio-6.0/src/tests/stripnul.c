/***
  This file is part of PulseAudio.

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <pulse/xmalloc.h>
#include <pulsecore/macro.h>

int main(int argc, char *argv[]) {
    FILE *i, *o;
    size_t granularity;
    bool found = false;
    uint8_t *zero;

    pa_assert_se(argc >= 2);
    pa_assert_se((granularity = (size_t) atoi(argv[1])) >= 1);
    pa_assert_se((i = (argc >= 3) ? fopen(argv[2], "r") : stdin));
    pa_assert_se((o = (argc >= 4) ? fopen(argv[3], "w") : stdout));

    zero = pa_xmalloc0(granularity);

    for (;;) {
        uint8_t buffer[16*1024], *p;
        size_t k;

        k = fread(buffer, granularity, sizeof(buffer)/granularity, i);

        if (k <= 0)
            break;

        if (found)
            pa_assert_se(fwrite(buffer, granularity, k, o) == k);
        else {
            for (p = buffer; ((size_t) (p-buffer)/granularity) < k; p += granularity)
                if (memcmp(p, zero, granularity)) {
                    size_t left;
                    found = true;
                    left = (size_t) (k - (size_t) (p-buffer)/granularity);
                    pa_assert_se(fwrite(p, granularity, left, o) == left);
                    break;
                }
        }
    }

    fflush(o);

    return 0;
}
