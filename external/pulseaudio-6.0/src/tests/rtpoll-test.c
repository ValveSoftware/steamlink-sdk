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

#include <check.h>
#include <signal.h>

#include <pulsecore/poll.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>

static int before(pa_rtpoll_item *i) {
    pa_log("before");
    return 0;
}

static void after(pa_rtpoll_item *i) {
    pa_log("after");
}

static int worker(pa_rtpoll_item *w) {
    pa_log("worker");
    return 0;
}

START_TEST (rtpoll_test) {
    pa_rtpoll *p;
    pa_rtpoll_item *i, *w;
    struct pollfd *pollfd;

    p = pa_rtpoll_new();

    i = pa_rtpoll_item_new(p, PA_RTPOLL_EARLY, 1);
    pa_rtpoll_item_set_before_callback(i, before);
    pa_rtpoll_item_set_after_callback(i, after);

    pollfd = pa_rtpoll_item_get_pollfd(i, NULL);
    pollfd->fd = 0;
    pollfd->events = POLLIN;

    w = pa_rtpoll_item_new(p, PA_RTPOLL_NORMAL, 0);
    pa_rtpoll_item_set_before_callback(w, worker);

    pa_rtpoll_set_timer_relative(p, 10000000); /* 10 s */

    pa_rtpoll_run(p);

    pa_rtpoll_item_free(i);

    i = pa_rtpoll_item_new(p, PA_RTPOLL_EARLY, 1);
    pa_rtpoll_item_set_before_callback(i, before);
    pa_rtpoll_item_set_after_callback(i, after);

    pollfd = pa_rtpoll_item_get_pollfd(i, NULL);
    pollfd->fd = 0;
    pollfd->events = POLLIN;

    pa_rtpoll_run(p);

    pa_rtpoll_item_free(i);

    pa_rtpoll_item_free(w);

    pa_rtpoll_free(p);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("RT Poll");
    tc = tcase_create("rtpoll");
    tcase_add_test(tc, rtpoll_test);
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
