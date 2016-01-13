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
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <check.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>

#include <pulsecore/core-util.h>
#include <pulsecore/core-rtclock.h>

#ifdef GLIB_MAIN_LOOP

#include <glib.h>
#include <pulse/glib-mainloop.h>

static GMainLoop* glib_main_loop = NULL;

#else /* GLIB_MAIN_LOOP */
#include <pulse/mainloop.h>
#endif /* GLIB_MAIN_LOOP */

static pa_defer_event *de;

static void iocb(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t f, void *userdata) {
    unsigned char c;
    pa_assert_se(read(fd, &c, sizeof(c)) >= 0);
    fprintf(stderr, "IO EVENT: %c\n", c < 32 ? '.' : c);
    a->defer_enable(de, 1);
}

static void dcb(pa_mainloop_api*a, pa_defer_event *e, void *userdata) {
    fprintf(stderr, "DEFER EVENT\n");
    a->defer_enable(e, 0);
}

static void tcb(pa_mainloop_api*a, pa_time_event *e, const struct timeval *tv, void *userdata) {
    fprintf(stderr, "TIME EVENT\n");

#if defined(GLIB_MAIN_LOOP)
    g_main_loop_quit(glib_main_loop);
#else
    a->quit(a, 0);
#endif
}

START_TEST (mainloop_test) {
    pa_mainloop_api *a;
    pa_io_event *ioe;
    pa_time_event *te;
    struct timeval tv;

#ifdef GLIB_MAIN_LOOP
    pa_glib_mainloop *g;

    glib_main_loop = g_main_loop_new(NULL, FALSE);
    fail_if(!glib_main_loop);

    g = pa_glib_mainloop_new(NULL);
    fail_if(!g);

    a = pa_glib_mainloop_get_api(g);
    fail_if(!a);
#else /* GLIB_MAIN_LOOP */
    pa_mainloop *m;

    m = pa_mainloop_new();
    fail_if(!m);

    a = pa_mainloop_get_api(m);
    fail_if(!a);
#endif /* GLIB_MAIN_LOOP */

    ioe = a->io_new(a, 0, PA_IO_EVENT_INPUT, iocb, NULL);
    fail_if(!ioe);

    de = a->defer_new(a, dcb, NULL);
    fail_if(!de);

    te = a->time_new(a, pa_timeval_rtstore(&tv, pa_rtclock_now() + 2 * PA_USEC_PER_SEC, true), tcb, NULL);

#if defined(GLIB_MAIN_LOOP)
    g_main_loop_run(glib_main_loop);
#else
    pa_mainloop_run(m, NULL);
#endif

    a->time_free(te);
    a->defer_free(de);
    a->io_free(ioe);

#ifdef GLIB_MAIN_LOOP
    pa_glib_mainloop_free(g);
    g_main_loop_unref(glib_main_loop);
#else
    pa_mainloop_free(m);
#endif
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("MainLoop");
    tc = tcase_create("mainloop");
    tcase_add_test(tc, mainloop_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
