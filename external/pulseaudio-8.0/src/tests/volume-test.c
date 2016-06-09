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
#include <math.h>

#include <check.h>

#include <pulse/volume.h>

#include <pulsecore/log.h>
#include <pulsecore/macro.h>

START_TEST (volume_test) {
    pa_volume_t v;
    pa_cvolume cv;
    float b;
    pa_channel_map map;
    pa_volume_t md = 0;
    unsigned mdn = 0;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    pa_log("Attenuation of sample 1 against 32767: %g dB", 20.0*log10(1.0/32767.0));
    pa_log("Smallest possible attenuation > 0 applied to 32767: %li", lrint(32767.0*pa_sw_volume_to_linear(1)));

    for (v = PA_VOLUME_MUTED; v <= PA_VOLUME_NORM*2; v += 256) {

        double dB = pa_sw_volume_to_dB(v);
        double f = pa_sw_volume_to_linear(v);

        pa_log_debug("Volume: %3i; percent: %i%%; decibel %0.2f; linear = %0.2f; volume(decibel): %3i; volume(linear): %3i",
               v, (v*100)/PA_VOLUME_NORM, dB, f, pa_sw_volume_from_dB(dB), pa_sw_volume_from_linear(f));
    }

    map.channels = cv.channels = 2;
    map.map[0] = PA_CHANNEL_POSITION_LEFT;
    map.map[1] = PA_CHANNEL_POSITION_RIGHT;

    for (v = PA_VOLUME_MUTED; v <= PA_VOLUME_NORM*2; v += 256) {
        char s[PA_CVOLUME_SNPRINT_VERBOSE_MAX];

        pa_cvolume_set(&cv, 2, v);

        pa_log_debug("Volume: %3i [%s]", v, pa_cvolume_snprint_verbose(s, sizeof(s), &cv, &map, true));
    }

    for (cv.values[0] = PA_VOLUME_MUTED; cv.values[0] <= PA_VOLUME_NORM*2; cv.values[0] += 4096)
        for (cv.values[1] = PA_VOLUME_MUTED; cv.values[1] <= PA_VOLUME_NORM*2; cv.values[1] += 4096) {
            char s[PA_CVOLUME_SNPRINT_VERBOSE_MAX];

            pa_log_debug("Volume: [%s]; balance: %2.1f",
                         pa_cvolume_snprint_verbose(s, sizeof(s), &cv, &map, true),
                         pa_cvolume_get_balance(&cv, &map));
        }

    for (cv.values[0] = PA_VOLUME_MUTED+4096; cv.values[0] <= PA_VOLUME_NORM*2; cv.values[0] += 4096)
        for (cv.values[1] = PA_VOLUME_MUTED; cv.values[1] <= PA_VOLUME_NORM*2; cv.values[1] += 4096)
            for (b = -1.0f; b <= 1.0f; b += 0.2f) {
                char s[PA_CVOLUME_SNPRINT_VERBOSE_MAX];
                pa_cvolume r;
                float k;

                pa_log_debug("Before: volume: [%s]; balance: %2.1f",
                             pa_cvolume_snprint_verbose(s, sizeof(s), &cv, &map, true),
                             pa_cvolume_get_balance(&cv, &map));

                r = cv;
                pa_cvolume_set_balance(&r, &map,b);

                k = pa_cvolume_get_balance(&r, &map);
                pa_log_debug("After: volume: [%s]; balance: %2.1f (intended: %2.1f) %s",
                             pa_cvolume_snprint_verbose(s, sizeof(s), &r, &map, true),
                             k,
                             b,
                             k < b - .05 || k > b + .5 ? "MISMATCH" : "");
            }

    for (v = PA_VOLUME_MUTED; v <= PA_VOLUME_NORM*2; v += 51) {

        double l = pa_sw_volume_to_linear(v);
        pa_volume_t k = pa_sw_volume_from_linear(l);
        double db = pa_sw_volume_to_dB(v);
        pa_volume_t r = pa_sw_volume_from_dB(db);
        pa_volume_t w;

        fail_unless(k == v);
        fail_unless(r == v);

        for (w = PA_VOLUME_MUTED; w < PA_VOLUME_NORM*2; w += 37) {

            double t = pa_sw_volume_to_linear(w);
            double db2 = pa_sw_volume_to_dB(w);
            pa_volume_t p, p1, p2;
            double q, qq;

            p = pa_sw_volume_multiply(v, w);
            if (isfinite(db) && isfinite(db2))
                qq = db + db2;
            else
                qq = -INFINITY;
            p2 = pa_sw_volume_from_dB(qq);
            q = l*t;
            p1 = pa_sw_volume_from_linear(q);

            if (p2 > p && p2 - p > md)
                md = p2 - p;
            if (p2 < p && p - p2 > md)
                md = p - p2;
            if (p1 > p && p1 - p > md)
                md = p1 - p;
            if (p1 < p && p - p1 > md)
                md = p - p1;

            if (p1 != p || p2 != p)
                mdn++;
        }
    }

    pa_log("max deviation: %lu n=%lu", (unsigned long) md, (unsigned long) mdn);

    fail_unless(md <= 1);

    /* mdn counts the times there were rounding errors during the test. The
     * number of rounding errors seems to vary slightly depending on the
     * hardware. The original limit was 251 errors, but it was increased to 253
     * when the test was failing on Tanu's laptop.
     * See https://bugs.freedesktop.org/show_bug.cgi?id=72374 */
    fail_unless(mdn <= 253);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Volume");
    tc = tcase_create("volume");
    tcase_add_test(tc, volume_test);
    tcase_set_timeout(tc, 120);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
