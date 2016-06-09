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

#include <check.h>

#include <pulse/util.h>
#include <pulse/xmalloc.h>

#include <pulsecore/log.h>

START_TEST (getbinaryname_test) {
    char *exename;
    size_t allocated = 128;

    for (;;) {
        exename = pa_xmalloc(allocated);

        if (!pa_get_binary_name(exename, allocated)) {
            pa_log_error("failed to read binary name");
            pa_xfree(exename);
            fail();
        }

        if (strlen(exename) < allocated - 1) {
            pa_log("%s", exename);
            pa_xfree(exename);
            return;
        }

        pa_xfree(exename);
        allocated *= 2;
    }
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Binary Name");
    tc = tcase_create("getbinaryname");
    tcase_add_test(tc, getbinaryname_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
