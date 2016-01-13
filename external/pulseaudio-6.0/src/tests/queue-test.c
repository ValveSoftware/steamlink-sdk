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

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <check.h>

#include <pulsecore/queue.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>

START_TEST (queue_test) {
    pa_queue *q;

    q = pa_queue_new();
    fail_unless(q != NULL);

    fail_unless(pa_queue_isempty(q));

    pa_queue_push(q, (void*) "eins");
    pa_log("%s\n", (char*) pa_queue_pop(q));

    fail_unless(pa_queue_isempty(q));

    pa_queue_push(q, (void*) "zwei");
    pa_queue_push(q, (void*) "drei");
    pa_queue_push(q, (void*) "vier");

    pa_log("%s\n", (char*) pa_queue_pop(q));
    pa_log("%s\n", (char*) pa_queue_pop(q));

    pa_queue_push(q, (void*) "fuenf");

    pa_log("%s\n", (char*) pa_queue_pop(q));
    pa_log("%s\n", (char*) pa_queue_pop(q));

    fail_unless(pa_queue_isempty(q));

    pa_queue_push(q, (void*) "sechs");
    pa_queue_push(q, (void*) "sieben");

    pa_queue_free(q, NULL);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Queue");
    tc = tcase_create("queue");
    tcase_add_test(tc, queue_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
