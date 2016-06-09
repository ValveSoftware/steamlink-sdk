/***
  This file is part of PulseAudio.

  Copyright 2008 Lennart Poettering

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

#include <check.h>

#include <string.h>

#include <pulsecore/poll.h>
#include <pulsecore/macro.h>
#include <pulsecore/thread.h>
#include <pulsecore/lock-autospawn.h>
#include <pulse/util.h>

static void thread_func(void*k) {
    fail_unless(pa_autospawn_lock_init() >= 0);

    pa_log("%i, Trying to acquire lock.", PA_PTR_TO_INT(k));

    fail_unless(pa_autospawn_lock_acquire(true) > 0);

    pa_log("%i, Got the lock!, Sleeping for 5s", PA_PTR_TO_INT(k));

    pa_msleep(5000);

    pa_log("%i, Releasing", PA_PTR_TO_INT(k));

    pa_autospawn_lock_release();

    pa_autospawn_lock_done(false);
}

static void thread_func2(void *k) {
    int fd;

    fail_unless((fd = pa_autospawn_lock_init()) >= 0);

    pa_log("%i, Trying to acquire lock.", PA_PTR_TO_INT(k));

    for (;;) {
        struct pollfd pollfd;
        int j;

        if ((j = pa_autospawn_lock_acquire(false)) > 0)
            break;

        fail_unless(j == 0);

        memset(&pollfd, 0, sizeof(pollfd));
        pollfd.fd = fd;
        pollfd.events = POLLIN;

        fail_unless(pa_poll(&pollfd, 1, -1) == 1);

        pa_log("%i, woke up", PA_PTR_TO_INT(k));
    }

    pa_log("%i, Got the lock!, Sleeping for 5s", PA_PTR_TO_INT(k));

    pa_msleep(5000);

    pa_log("%i, Releasing", PA_PTR_TO_INT(k));

    pa_autospawn_lock_release();

    pa_autospawn_lock_done(false);
}

START_TEST (lockautospawn_test) {
    pa_thread *a, *b, *c, *d;

    pa_assert_se((a = pa_thread_new("test1", thread_func, PA_INT_TO_PTR(1))));
    pa_assert_se((b = pa_thread_new("test2", thread_func2, PA_INT_TO_PTR(2))));
    pa_assert_se((c = pa_thread_new("test3", thread_func2, PA_INT_TO_PTR(3))));
    pa_assert_se((d = pa_thread_new("test4", thread_func, PA_INT_TO_PTR(4))));

    pa_thread_join(a);
    pa_thread_join(b);
    pa_thread_join(c);
    pa_thread_join(d);

    pa_thread_free(a);
    pa_thread_free(b);
    pa_thread_free(c);
    pa_thread_free(d);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Lock Auto Spawn");
    tc = tcase_create("lockautospawn");
    tcase_add_test(tc, lockautospawn_test);
    /* the default timeout is too small,
     * set it to a reasonable large one.
     */
    tcase_set_timeout(tc, 60 * 60);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
