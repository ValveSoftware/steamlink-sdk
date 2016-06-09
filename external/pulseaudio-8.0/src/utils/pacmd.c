/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

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
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/un.h>
#include <getopt.h>
#include <locale.h>

#include <pulse/util.h>
#include <pulse/xmalloc.h>

#include <pulsecore/i18n.h>
#include <pulsecore/poll.h>
#include <pulsecore/macro.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/pid.h>

static void help(const char *argv0) {
    printf("%s %s\n",    argv0, "exit");
    printf("%s %s\n",    argv0, "help");
    printf("%s %s\n",    argv0, "list-(modules|sinks|sources|clients|cards|samples)");
    printf("%s %s\n",    argv0, "list-(sink-inputs|source-outputs)");
    printf("%s %s\n",    argv0, "stat");
    printf("%s %s\n",    argv0, "info");
    printf("%s %s %s\n", argv0, "load-module", _("NAME [ARGS ...]"));
    printf("%s %s %s\n", argv0, "unload-module", _("NAME|#N"));
    printf("%s %s %s\n", argv0, "describe-module", _("NAME"));
    printf("%s %s %s\n", argv0, "set-(sink|source)-volume", _("NAME|#N VOLUME"));
    printf("%s %s %s\n", argv0, "set-(sink-input|source-output)-volume", _("#N VOLUME"));
    printf("%s %s %s\n", argv0, "set-(sink|source)-mute", _("NAME|#N 1|0"));
    printf("%s %s %s\n", argv0, "set-(sink-input|source-output)-mute", _("#N 1|0"));
    printf("%s %s %s\n", argv0, "update-(sink|source)-proplist", _("NAME|#N KEY=VALUE"));
    printf("%s %s %s\n", argv0, "update-(sink-input|source-output)-proplist", _("#N KEY=VALUE"));
    printf("%s %s %s\n", argv0, "set-default-(sink|source)", _("NAME|#N"));
    printf("%s %s %s\n", argv0, "kill-(client|sink-input|source-output)", _("#N"));
    printf("%s %s %s\n", argv0, "play-sample", _("NAME SINK|#N"));
    printf("%s %s %s\n", argv0, "remove-sample", _("NAME"));
    printf("%s %s %s\n", argv0, "load-sample", _("NAME FILENAME"));
    printf("%s %s %s\n", argv0, "load-sample-lazy", _("NAME FILENAME"));
    printf("%s %s %s\n", argv0, "load-sample-dir-lazy", _("PATHNAME"));
    printf("%s %s %s\n", argv0, "play-file", _("FILENAME SINK|#N"));
    printf("%s %s\n",    argv0, "dump");
    printf("%s %s %s\n", argv0, "move-(sink-input|source-output)", _("#N SINK|SOURCE"));
    printf("%s %s %s\n", argv0, "suspend-(sink|source)", _("NAME|#N 1|0"));
    printf("%s %s %s\n", argv0, "suspend", _("1|0"));
    printf("%s %s %s\n", argv0, "set-card-profile", _("CARD PROFILE"));
    printf("%s %s %s\n", argv0, "set-(sink|source)-port", _("NAME|#N PORT"));
    printf("%s %s %s\n", argv0, "set-port-latency-offset", _("CARD-NAME|CARD-#N PORT OFFSET"));
    printf("%s %s %s\n", argv0, "set-log-target", _("TARGET"));
    printf("%s %s %s\n", argv0, "set-log-level", _("NUMERIC-LEVEL"));
    printf("%s %s %s\n", argv0, "set-log-meta", _("1|0"));
    printf("%s %s %s\n", argv0, "set-log-time", _("1|0"));
    printf("%s %s %s\n", argv0, "set-log-backtrace", _("FRAMES"));

    printf(_("\n"
         "  -h, --help                            Show this help\n"
         "      --version                         Show version\n"
         "When no command is given pacmd starts in the interactive mode.\n" ));
}

enum {
    ARG_VERSION = 256
};

int main(int argc, char*argv[]) {
    pid_t pid;
    int fd = -1;
    int ret = 1, i;
    struct sockaddr_un sa;
    char *ibuf = NULL;
    char *obuf = NULL;
    size_t buf_size, ibuf_size, ibuf_index, ibuf_length, obuf_size, obuf_index, obuf_length;
    char *cli;
    bool ibuf_eof, obuf_eof, ibuf_closed, obuf_closed;
    struct pollfd pollfd[3];
    struct pollfd *watch_socket, *watch_stdin, *watch_stdout;
    int stdin_type = 0, stdout_type = 0, fd_type = 0;

    char *bn = NULL;
    int c;

    static const struct option long_options[] = {
        {"version",     0, NULL, ARG_VERSION},
        {"help",        0, NULL, 'h'},
        {NULL,          0, NULL, 0}
    };

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PULSE_LOCALEDIR);
#endif

    bn = pa_path_get_filename(argv[0]);

    while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
        switch (c) {
            case 'h' :
                help(bn);
                ret = 0;
                goto quit;
            case ARG_VERSION:
                printf(_("pacmd %s\n"
                         "Compiled with libpulse %s\n"
                         "Linked with libpulse %s\n"),
                       PACKAGE_VERSION,
                       pa_get_headers_version(),
                       pa_get_library_version());
                ret = 0;
                goto quit;
            default:
                goto quit;
        }
    }

    if (pa_pid_file_check_running(&pid, "pulseaudio") < 0) {
        pa_log(_("No PulseAudio daemon running, or not running as session daemon."));
        goto quit;
    }

    if ((fd = pa_socket_cloexec(PF_UNIX, SOCK_STREAM, 0)) < 0) {
        pa_log(_("socket(PF_UNIX, SOCK_STREAM, 0): %s"), strerror(errno));
        goto quit;
    }

    pa_zero(sa);
    sa.sun_family = AF_UNIX;

    if (!(cli = pa_runtime_path("cli")))
        goto quit;

    pa_strlcpy(sa.sun_path, cli, sizeof(sa.sun_path));
    pa_xfree(cli);

    for (i = 0; i < 5; i++) {
        int r;

        if ((r = connect(fd, (struct sockaddr*) &sa, sizeof(sa))) < 0 && (errno != ECONNREFUSED && errno != ENOENT)) {
            pa_log(_("connect(): %s"), strerror(errno));
            goto quit;
        }

        if (r >= 0)
            break;

        if (pa_pid_file_kill(SIGUSR2, NULL, "pulseaudio") < 0) {
            pa_log(_("Failed to kill PulseAudio daemon."));
            goto quit;
        }

        pa_msleep(300);
    }

    if (i >= 5) {
        pa_log(_("Daemon not responding."));
        goto quit;
    }

    buf_size = pa_pipe_buf(fd);
    ibuf_size = PA_MIN(buf_size, pa_pipe_buf(STDIN_FILENO));
    ibuf = pa_xmalloc(ibuf_size);
    obuf_size = PA_MIN(buf_size, pa_pipe_buf(STDOUT_FILENO));
    obuf = pa_xmalloc(obuf_size);
    ibuf_index = ibuf_length = obuf_index = obuf_length = 0;
    ibuf_eof = obuf_eof = ibuf_closed = obuf_closed = false;

    if (argc > 1) {
        for (i = 1; i < argc; i++) {
            size_t k;

            k = PA_MIN(ibuf_size - ibuf_length, strlen(argv[i]));
            memcpy(ibuf + ibuf_length, argv[i], k);
            ibuf_length += k;

            if (ibuf_length < ibuf_size) {
                ibuf[ibuf_length] = i < argc-1 ? ' ' : '\n';
                ibuf_length++;
            }
        }

        ibuf_eof = true;
    }

    if (!ibuf_eof && isatty(STDIN_FILENO)) {
        /* send hello to enable interactive mode (welcome message, prompt) */
        if (pa_write(fd, "hello\n", 6, &fd_type) < 0) {
            pa_log(_("write(): %s"), strerror(errno));
            goto quit;
        }
    }

    for (;;) {
        struct pollfd *p;

        if (ibuf_eof &&
            obuf_eof &&
            ibuf_length <= 0 &&
            obuf_length <= 0)
            break;

        if (ibuf_length <= 0 && ibuf_eof && !ibuf_closed) {
            shutdown(fd, SHUT_WR);
            ibuf_closed = true;
        }

        if (obuf_length <= 0 && obuf_eof && !obuf_closed) {
            shutdown(fd, SHUT_RD);
            obuf_closed = true;
        }

        pa_zero(pollfd);

        p = pollfd;

        if (ibuf_length > 0 || (!obuf_eof && obuf_length <= 0)) {
            watch_socket = p++;
            watch_socket->fd = fd;
            watch_socket->events =
                (ibuf_length > 0 ? POLLOUT : 0) |
                (!obuf_eof && obuf_length <= 0 ? POLLIN : 0);
        } else
            watch_socket = NULL;

        if (!ibuf_eof && ibuf_length <= 0) {
            watch_stdin = p++;
            watch_stdin->fd = STDIN_FILENO;
            watch_stdin->events = POLLIN;
        } else
            watch_stdin = NULL;

        if (obuf_length > 0) {
            watch_stdout = p++;
            watch_stdout->fd = STDOUT_FILENO;
            watch_stdout->events = POLLOUT;
        } else
            watch_stdout = NULL;

        if (pa_poll(pollfd, p-pollfd, -1) < 0) {

            if (errno == EINTR)
                continue;

            pa_log(_("poll(): %s"), strerror(errno));
            goto quit;
        }

        if (watch_stdin) {
            if (watch_stdin->revents & POLLIN) {
                ssize_t r;
                pa_assert(ibuf_length <= 0);

                if ((r = pa_read(STDIN_FILENO, ibuf, ibuf_size, &stdin_type)) <= 0) {
                    if (r < 0) {
                        pa_log(_("read(): %s"), strerror(errno));
                        goto quit;
                    }

                    ibuf_eof = true;
                } else {
                    ibuf_length = (size_t) r;
                    ibuf_index = 0;
                }
            } else if (watch_stdin->revents & POLLHUP)
                ibuf_eof = true;
        }

        if (watch_socket) {
            if (watch_socket->revents & POLLIN) {
                ssize_t r;
                pa_assert(obuf_length <= 0);

                if ((r = pa_read(fd, obuf, obuf_size, &fd_type)) <= 0) {
                    if (r < 0) {
                        pa_log(_("read(): %s"), strerror(errno));
                        goto quit;
                    }

                    obuf_eof = true;
                } else {
                    obuf_length = (size_t) r;
                    obuf_index = 0;
                }
            } else if (watch_socket->revents & POLLHUP)
                obuf_eof = true;
        }

        if (watch_stdout) {
            if (watch_stdout->revents & POLLHUP) {
                obuf_eof = true;
                obuf_length = 0;
            } else if (watch_stdout->revents & POLLOUT) {
                ssize_t r;
                pa_assert(obuf_length > 0);

                if ((r = pa_write(STDOUT_FILENO, obuf + obuf_index, obuf_length, &stdout_type)) < 0) {
                    pa_log(_("write(): %s"), strerror(errno));
                    goto quit;
                }

                obuf_length -= (size_t) r;
                obuf_index += obuf_index;
            }
        }

        if (watch_socket) {
            if (watch_socket->revents & POLLHUP) {
                ibuf_eof = true;
                ibuf_length = 0;
            } else if (watch_socket->revents & POLLOUT) {
                ssize_t r;
                pa_assert(ibuf_length > 0);

                if ((r = pa_write(fd, ibuf + ibuf_index, ibuf_length, &fd_type)) < 0) {
                    pa_log(_("write(): %s"), strerror(errno));
                    goto quit;
                }

                ibuf_length -= (size_t) r;
                ibuf_index += obuf_index;
            }
        }
    }

    ret = 0;

quit:
    if (fd >= 0)
        pa_close(fd);

    pa_xfree(obuf);
    pa_xfree(ibuf);

    return ret;
}
