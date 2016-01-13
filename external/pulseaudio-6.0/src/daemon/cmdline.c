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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <pulse/xmalloc.h>
#include <pulse/util.h>

#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/strbuf.h>
#include <pulsecore/macro.h>

#include "cmdline.h"

/* Argument codes for getopt_long() */
enum {
    ARG_HELP = 256,
    ARG_VERSION,
    ARG_DUMP_CONF,
    ARG_DUMP_MODULES,
    ARG_DAEMONIZE,
    ARG_FAIL,
    ARG_LOG_LEVEL,
    ARG_HIGH_PRIORITY,
    ARG_REALTIME,
    ARG_DISALLOW_MODULE_LOADING,
    ARG_DISALLOW_EXIT,
    ARG_EXIT_IDLE_TIME,
    ARG_SCACHE_IDLE_TIME,
    ARG_LOG_TARGET,
    ARG_LOG_META,
    ARG_LOG_TIME,
    ARG_LOG_BACKTRACE,
    ARG_LOAD,
    ARG_FILE,
    ARG_DL_SEARCH_PATH,
    ARG_RESAMPLE_METHOD,
    ARG_KILL,
    ARG_USE_PID_FILE,
    ARG_CHECK,
    ARG_NO_CPU_LIMIT,
    ARG_DISABLE_SHM,
    ARG_DUMP_RESAMPLE_METHODS,
    ARG_SYSTEM,
    ARG_CLEANUP_SHM,
    ARG_START
};

/* Table for getopt_long() */
static const struct option long_options[] = {
    {"help",                        0, 0, ARG_HELP},
    {"version",                     0, 0, ARG_VERSION},
    {"dump-conf",                   0, 0, ARG_DUMP_CONF},
    {"dump-modules",                0, 0, ARG_DUMP_MODULES},
    {"daemonize",                   2, 0, ARG_DAEMONIZE},
    {"fail",                        2, 0, ARG_FAIL},
    {"verbose",                     2, 0, ARG_LOG_LEVEL},
    {"log-level",                   2, 0, ARG_LOG_LEVEL},
    {"high-priority",               2, 0, ARG_HIGH_PRIORITY},
    {"realtime",                    2, 0, ARG_REALTIME},
    {"disallow-module-loading",     2, 0, ARG_DISALLOW_MODULE_LOADING},
    {"disallow-exit",               2, 0, ARG_DISALLOW_EXIT},
    {"exit-idle-time",              1, 0, ARG_EXIT_IDLE_TIME},
    {"scache-idle-time",            1, 0, ARG_SCACHE_IDLE_TIME},
    {"log-target",                  1, 0, ARG_LOG_TARGET},
    {"log-meta",                    2, 0, ARG_LOG_META},
    {"log-time",                    2, 0, ARG_LOG_TIME},
    {"log-backtrace",               1, 0, ARG_LOG_BACKTRACE},
    {"load",                        1, 0, ARG_LOAD},
    {"file",                        1, 0, ARG_FILE},
    {"dl-search-path",              1, 0, ARG_DL_SEARCH_PATH},
    {"resample-method",             1, 0, ARG_RESAMPLE_METHOD},
    {"kill",                        0, 0, ARG_KILL},
    {"start",                       0, 0, ARG_START},
    {"use-pid-file",                2, 0, ARG_USE_PID_FILE},
    {"check",                       0, 0, ARG_CHECK},
    {"system",                      2, 0, ARG_SYSTEM},
    {"no-cpu-limit",                2, 0, ARG_NO_CPU_LIMIT},
    {"disable-shm",                 2, 0, ARG_DISABLE_SHM},
    {"dump-resample-methods",       2, 0, ARG_DUMP_RESAMPLE_METHODS},
    {"cleanup-shm",                 2, 0, ARG_CLEANUP_SHM},
    {NULL, 0, 0, 0}
};

void pa_cmdline_help(const char *argv0) {
    pa_assert(argv0);

    printf(_("%s [options]\n\n"
           "COMMANDS:\n"
           "  -h, --help                            Show this help\n"
           "      --version                         Show version\n"
           "      --dump-conf                       Dump default configuration\n"
           "      --dump-modules                    Dump list of available modules\n"
           "      --dump-resample-methods           Dump available resample methods\n"
           "      --cleanup-shm                     Cleanup stale shared memory segments\n"
           "      --start                           Start the daemon if it is not running\n"
           "  -k  --kill                            Kill a running daemon\n"
           "      --check                           Check for a running daemon (only returns exit code)\n\n"

           "OPTIONS:\n"
           "      --system[=BOOL]                   Run as system-wide instance\n"
           "  -D, --daemonize[=BOOL]                Daemonize after startup\n"
           "      --fail[=BOOL]                     Quit when startup fails\n"
           "      --high-priority[=BOOL]            Try to set high nice level\n"
           "                                        (only available as root, when SUID or\n"
           "                                        with elevated RLIMIT_NICE)\n"
           "      --realtime[=BOOL]                 Try to enable realtime scheduling\n"
           "                                        (only available as root, when SUID or\n"
           "                                        with elevated RLIMIT_RTPRIO)\n"
           "      --disallow-module-loading[=BOOL]  Disallow module user requested module\n"
           "                                        loading/unloading after startup\n"
           "      --disallow-exit[=BOOL]            Disallow user requested exit\n"
           "      --exit-idle-time=SECS             Terminate the daemon when idle and this\n"
           "                                        time passed\n"
           "      --scache-idle-time=SECS           Unload autoloaded samples when idle and\n"
           "                                        this time passed\n"
           "      --log-level[=LEVEL]               Increase or set verbosity level\n"
           "  -v  --verbose                         Increase the verbosity level\n"
           "      --log-target={auto,syslog,stderr,file:PATH,newfile:PATH}\n"
           "                                        Specify the log target\n"
           "      --log-meta[=BOOL]                 Include code location in log messages\n"
           "      --log-time[=BOOL]                 Include timestamps in log messages\n"
           "      --log-backtrace=FRAMES            Include a backtrace in log messages\n"
           "  -p, --dl-search-path=PATH             Set the search path for dynamic shared\n"
           "                                        objects (plugins)\n"
           "      --resample-method=METHOD          Use the specified resampling method\n"
           "                                        (See --dump-resample-methods for\n"
           "                                        possible values)\n"
           "      --use-pid-file[=BOOL]             Create a PID file\n"
           "      --no-cpu-limit[=BOOL]             Do not install CPU load limiter on\n"
           "                                        platforms that support it.\n"
           "      --disable-shm[=BOOL]              Disable shared memory support.\n\n"

           "STARTUP SCRIPT:\n"
           "  -L, --load=\"MODULE ARGUMENTS\"         Load the specified plugin module with\n"
           "                                        the specified argument\n"
           "  -F, --file=FILENAME                   Run the specified script\n"
           "  -C                                    Open a command line on the running TTY\n"
           "                                        after startup\n\n"

           "  -n                                    Don't load default script file\n"),
           pa_path_get_filename(argv0));
}

int pa_cmdline_parse(pa_daemon_conf *conf, int argc, char *const argv [], int *d) {
    pa_strbuf *buf = NULL;
    int c;
    int b;

    pa_assert(conf);
    pa_assert(argc > 0);
    pa_assert(argv);

    buf = pa_strbuf_new();

    if (conf->script_commands)
        pa_strbuf_puts(buf, conf->script_commands);

    while ((c = getopt_long(argc, argv, "L:F:ChDnp:kv", long_options, NULL)) != -1) {
        switch (c) {
            case ARG_HELP:
            case 'h':
                conf->cmd = PA_CMD_HELP;
                break;

            case ARG_VERSION:
                conf->cmd = PA_CMD_VERSION;
                break;

            case ARG_DUMP_CONF:
                conf->cmd = PA_CMD_DUMP_CONF;
                break;

            case ARG_DUMP_MODULES:
                conf->cmd = PA_CMD_DUMP_MODULES;
                break;

            case ARG_DUMP_RESAMPLE_METHODS:
                conf->cmd = PA_CMD_DUMP_RESAMPLE_METHODS;
                break;

            case ARG_CLEANUP_SHM:
                conf->cmd = PA_CMD_CLEANUP_SHM;
                break;

            case 'k':
            case ARG_KILL:
                conf->cmd = PA_CMD_KILL;
                break;

            case ARG_START:
                conf->cmd = PA_CMD_START;
                conf->daemonize = true;
                break;

            case ARG_CHECK:
                conf->cmd = PA_CMD_CHECK;
                break;

            case ARG_LOAD:
            case 'L':
                pa_strbuf_printf(buf, "load-module %s\n", optarg);
                break;

            case ARG_FILE:
            case 'F': {
                char *p;
                pa_strbuf_printf(buf, ".include %s\n", p = pa_make_path_absolute(optarg));
                pa_xfree(p);
                break;
            }

            case 'C':
                pa_strbuf_puts(buf, "load-module module-cli exit_on_eof=1\n");
                break;

            case ARG_DAEMONIZE:
            case 'D':
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--daemonize expects boolean argument"));
                    goto fail;
                }
                conf->daemonize = !!b;
                break;

            case ARG_FAIL:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--fail expects boolean argument"));
                    goto fail;
                }
                conf->fail = !!b;
                break;

            case 'v':
            case ARG_LOG_LEVEL:

                if (optarg) {
                    if (pa_daemon_conf_set_log_level(conf, optarg) < 0) {
                        pa_log(_("--log-level expects log level argument (either numeric in range 0..4 or one of debug, info, notice, warn, error)."));
                        goto fail;
                    }
                } else {
                    if (conf->log_level < PA_LOG_LEVEL_MAX-1)
                        conf->log_level++;
                }

                break;

            case ARG_HIGH_PRIORITY:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--high-priority expects boolean argument"));
                    goto fail;
                }
                conf->high_priority = !!b;
                break;

            case ARG_REALTIME:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--realtime expects boolean argument"));
                    goto fail;
                }
                conf->realtime_scheduling = !!b;
                break;

            case ARG_DISALLOW_MODULE_LOADING:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--disallow-module-loading expects boolean argument"));
                    goto fail;
                }
                conf->disallow_module_loading = !!b;
                break;

            case ARG_DISALLOW_EXIT:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--disallow-exit expects boolean argument"));
                    goto fail;
                }
                conf->disallow_exit = !!b;
                break;

            case ARG_USE_PID_FILE:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--use-pid-file expects boolean argument"));
                    goto fail;
                }
                conf->use_pid_file = !!b;
                break;

            case 'p':
            case ARG_DL_SEARCH_PATH:
                pa_xfree(conf->dl_search_path);
                conf->dl_search_path = *optarg ? pa_xstrdup(optarg) : NULL;
                break;

            case 'n':
                conf->load_default_script_file = false;
                break;

            case ARG_LOG_TARGET:
                if (pa_daemon_conf_set_log_target(conf, optarg) < 0) {
#ifdef HAVE_SYSTEMD_JOURNAL
                    pa_log(_("Invalid log target: use either 'syslog', 'journal','stderr' or 'auto' or a valid file name 'file:<path>', 'newfile:<path>'."));
#else
                    pa_log(_("Invalid log target: use either 'syslog', 'stderr' or 'auto' or a valid file name 'file:<path>', 'newfile:<path>'."));
#endif
                    goto fail;
                }
                break;

            case ARG_LOG_TIME:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--log-time expects boolean argument"));
                    goto fail;
                }
                conf->log_time = !!b;
                break;

            case ARG_LOG_META:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--log-meta expects boolean argument"));
                    goto fail;
                }
                conf->log_meta = !!b;
                break;

            case ARG_LOG_BACKTRACE:
                conf->log_backtrace = (unsigned) atoi(optarg);
                break;

            case ARG_EXIT_IDLE_TIME:
                conf->exit_idle_time = atoi(optarg);
                break;

            case ARG_SCACHE_IDLE_TIME:
                conf->scache_idle_time = atoi(optarg);
                break;

            case ARG_RESAMPLE_METHOD:
                if (pa_daemon_conf_set_resample_method(conf, optarg) < 0) {
                    pa_log(_("Invalid resample method '%s'."), optarg);
                    goto fail;
                }
                break;

            case ARG_SYSTEM:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--system expects boolean argument"));
                    goto fail;
                }
                conf->system_instance = !!b;
                break;

            case ARG_NO_CPU_LIMIT:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--no-cpu-limit expects boolean argument"));
                    goto fail;
                }
                conf->no_cpu_limit = !!b;
                break;

            case ARG_DISABLE_SHM:
                if ((b = optarg ? pa_parse_boolean(optarg) : 1) < 0) {
                    pa_log(_("--disable-shm expects boolean argument"));
                    goto fail;
                }
                conf->disable_shm = !!b;
                break;

            default:
                goto fail;
        }
    }

    pa_xfree(conf->script_commands);
    conf->script_commands = pa_strbuf_tostring_free(buf);

    *d = optind;

    return 0;

fail:
    if (buf)
        pa_strbuf_free(buf);

    return -1;
}
