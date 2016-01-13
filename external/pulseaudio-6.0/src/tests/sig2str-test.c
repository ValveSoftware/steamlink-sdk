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
#include <stdio.h>

#include <pulsecore/macro.h>
#include <pulsecore/core-util.h>

static const char *names[] = {
    "SIG-1",
    "SIG0",
    "SIGHUP",
    "SIGINT",
    "SIGQUIT",
    "SIGULL",
    "SIGTRAP",
    "SIGABRT",
    "SIGBUS",
    "SIGFPE",
    "SIGKILL",
    "SIGUSR1",
    "SIGSEGV",
    "SIGUSR2",
    "SIGPIPE",
    "SIGALRM",
    "SIGTERM",
    "SIGSTKFLT",
    "SIGCHLD",
    "SIGCONT",
    "SIGSTOP",
    "SIGTSTP",
    "SIGTTIN",
    "SIGTTOU",
    "SIGURG",
    "SIGXCPU",
    "SIGXFSZ",
    "SIGVTALRM",
    "SIGPROF",
    "SIGWINCH",
    "SIGIO",
    "SIGPWR",
    "SIGSYS",
    "SIG32",
    "SIG33",
    "SIGRTMIN+0",
    "SIGRTMIN+1",
    "SIGRTMIN+2",
    "SIGRTMIN+3",
    "SIGRTMIN+4",
    "SIGRTMIN+5",
    "SIGRTMIN+6",
    "SIGRTMIN+7",
    "SIGRTMIN+8",
    "SIGRTMIN+9",
    "SIGRTMIN+10",
    "SIGRTMIN+11",
    "SIGRTMIN+12",
    "SIGRTMIN+13",
    "SIGRTMIN+14",
    "SIGRTMIN+15",
    "SIGRTMIN+16",
    "SIGRTMIN+17",
    "SIGRTMIN+18",
    "SIGRTMIN+19",
    "SIGRTMIN+20",
    "SIGRTMIN+21",
    "SIGRTMIN+22",
    "SIGRTMIN+23",
    "SIGRTMIN+24",
    "SIGRTMIN+25",
    "SIGRTMIN+26",
    "SIGRTMIN+27",
    "SIGRTMIN+28",
    "SIGRTMIN+29",
    "SIGRTMIN+30",
    "SIG65"
};

START_TEST (sig2str_test) {
    int sig;

    for (sig = -1; sig <= NSIG; sig++) {
        printf("%i = %s\n", sig, pa_sig2str(sig));
        fail_unless(pa_streq(pa_sig2str(sig), names[sig+1]));
    }
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Signal String");
    tc = tcase_create("sig2str");
    tcase_add_test(tc, sig2str_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
