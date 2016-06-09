/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/rtclock.h>
#include <pulse/timeval.h>

#include <pulsecore/core-util.h>
#include <pulsecore/core-error.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>

#include "cpulimit.h"

#ifdef HAVE_SIGXCPU

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

/* This module implements a watchdog that makes sure that the current
 * process doesn't consume more than 70% CPU time for 10 seconds. This
 * is very useful when using SCHED_FIFO scheduling which effectively
 * disables multitasking. */

/* Method of operation: Using SIGXCPU a signal handler is called every
 * 10s process CPU time. That function checks if less than 14s system
 * time have passed. In that case, it tries to contact the main event
 * loop through a pipe. After two additional seconds it is checked
 * whether the main event loop contact was successful. If not, the
 * program is terminated forcibly. */

/* Utilize this much CPU time at maximum */
#define CPUTIME_PERCENT 70

/* Check every 10s */
#define CPUTIME_INTERVAL_SOFT (10)

/* Recheck after 5s */
#define CPUTIME_INTERVAL_HARD (5)

/* Time of the last CPU load check */
static pa_usec_t last_time = 0;

/* Pipe for communicating with the main loop */
static int the_pipe[2] = {-1, -1};

/* Main event loop and IO event for the FIFO */
static pa_mainloop_api *api = NULL;
static pa_io_event *io_event = NULL;

/* Saved sigaction struct for SIGXCPU */
static struct sigaction sigaction_prev;

/* Nonzero after pa_cpu_limit_init() */
static bool installed = false;

/* The current state of operation */
static enum {
    PHASE_IDLE,   /* Normal state */
    PHASE_SOFT    /* After CPU overload has been detected */
} phase = PHASE_IDLE;

/* Reset the SIGXCPU timer to the next t seconds */
static void reset_cpu_time(int t) {
    long n;
    struct rlimit rl;
    struct rusage ru;

    /* Get the current CPU time of the current process */
    pa_assert_se(getrusage(RUSAGE_SELF, &ru) >= 0);

    n = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec + t;
    pa_assert_se(getrlimit(RLIMIT_CPU, &rl) >= 0);

    rl.rlim_cur = (rlim_t) n;
    pa_assert_se(setrlimit(RLIMIT_CPU, &rl) >= 0);
}

/* A simple, thread-safe puts() work-alike */
static void write_err(const char *p) {
    pa_loop_write(2, p, strlen(p), NULL);
}

/* The signal handler, called on every SIGXCPU */
static void signal_handler(int sig) {
    int saved_errno;

    saved_errno = errno;
    pa_assert(sig == SIGXCPU);

    if (phase == PHASE_IDLE) {
        pa_usec_t now, elapsed;

#ifdef PRINT_CPU_LOAD
        char t[256];
#endif

        now = pa_rtclock_now();
        elapsed = now - last_time;

#ifdef PRINT_CPU_LOAD
        pa_snprintf(t, sizeof(t), "Using %0.1f%% CPU\n", ((double) CPUTIME_INTERVAL_SOFT * (double) PA_USEC_PER_SEC) / (double) elapsed * 100.0);
        write_err(t);
#endif

        if (((double) CPUTIME_INTERVAL_SOFT * (double) PA_USEC_PER_SEC) >= ((double) elapsed * (double) CPUTIME_PERCENT / 100.0)) {
            static const char c = 'X';

            write_err("Soft CPU time limit exhausted, terminating.\n");

            /* Try a soft cleanup */
            (void) pa_write(the_pipe[1], &c, sizeof(c), NULL);
            phase = PHASE_SOFT;
            reset_cpu_time(CPUTIME_INTERVAL_HARD);

        } else {

            /* Everything's fine */
            reset_cpu_time(CPUTIME_INTERVAL_SOFT);
            last_time = now;
        }

    } else if (phase == PHASE_SOFT) {
        write_err("Hard CPU time limit exhausted, terminating forcibly.\n");
        abort(); /* Forced exit */
    }

    errno = saved_errno;
}

/* Callback for IO events on the FIFO */
static void callback(pa_mainloop_api*m, pa_io_event*e, int fd, pa_io_event_flags_t f, void *userdata) {
    char c;
    pa_assert(m);
    pa_assert(e);
    pa_assert(f == PA_IO_EVENT_INPUT);
    pa_assert(e == io_event);
    pa_assert(fd == the_pipe[0]);

    pa_log("Received request to terminate due to CPU overload.");

    (void) pa_read(the_pipe[0], &c, sizeof(c), NULL);
    m->quit(m, 1); /* Quit the main loop */
}

/* Initializes CPU load limiter */
int pa_cpu_limit_init(pa_mainloop_api *m) {
    struct sigaction sa;

    pa_assert(m);
    pa_assert(!api);
    pa_assert(!io_event);
    pa_assert(the_pipe[0] == -1);
    pa_assert(the_pipe[1] == -1);
    pa_assert(!installed);

    last_time = pa_rtclock_now();

    /* Prepare the main loop pipe */
    if (pa_pipe_cloexec(the_pipe) < 0) {
        pa_log("pipe() failed: %s", pa_cstrerror(errno));
        return -1;
    }

    pa_make_fd_nonblock(the_pipe[0]);
    pa_make_fd_nonblock(the_pipe[1]);

    api = m;
    io_event = api->io_new(m, the_pipe[0], PA_IO_EVENT_INPUT, callback, NULL);

    phase = PHASE_IDLE;

    /* Install signal handler for SIGXCPU */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGXCPU, &sa, &sigaction_prev) < 0) {
        pa_cpu_limit_done();
        return -1;
    }

    installed = true;

    reset_cpu_time(CPUTIME_INTERVAL_SOFT);

    return 0;
}

/* Shutdown CPU load limiter */
void pa_cpu_limit_done(void) {

    if (io_event) {
        pa_assert(api);
        api->io_free(io_event);
        io_event = NULL;
        api = NULL;
    }

    pa_close_pipe(the_pipe);

    if (installed) {
        pa_assert_se(sigaction(SIGXCPU, &sigaction_prev, NULL) >= 0);
        installed = false;
    }
}

#else /* HAVE_SIGXCPU */

int pa_cpu_limit_init(pa_mainloop_api *m) {
    return 0;
}

void pa_cpu_limit_done(void) {
}

#endif
