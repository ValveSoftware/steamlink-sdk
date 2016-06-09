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

#include <pulse/util.h>
#include <pulsecore/asyncq.h>
#include <pulsecore/thread.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>

static void producer(void *_q) {
    pa_asyncq *q = _q;
    int i;

    for (i = 0; i < 1000; i++) {
        pa_log_debug("pushing %i", i);
        pa_asyncq_push(q, PA_UINT_TO_PTR(i+1), 1);
    }

    pa_asyncq_push(q, PA_UINT_TO_PTR(-1), true);
    pa_log_debug("pushed end");
}

static void consumer(void *_q) {
    pa_asyncq *q = _q;
    void *p;
    int i;

    pa_msleep(1000);

    for (i = 0;; i++) {
        p = pa_asyncq_pop(q, true);

        if (p == PA_UINT_TO_PTR(-1))
            break;

        fail_unless(p == PA_UINT_TO_PTR(i+1));

        pa_log_debug("popped %i", i);
    }

    pa_log_debug("popped end");
}

START_TEST (asyncq_test) {
    pa_asyncq *q;
    pa_thread *t1, *t2;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    q = pa_asyncq_new(0);
    fail_unless(q != NULL);

    t1 = pa_thread_new("producer", producer, q);
    fail_unless(t1 != NULL);
    t2 = pa_thread_new("consumer", consumer, q);
    fail_unless(t2 != NULL);

    pa_thread_free(t1);
    pa_thread_free(t2);

    pa_asyncq_free(q, NULL);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Async Queue");
    tc = tcase_create("asyncq");
    tcase_add_test(tc, asyncq_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
