/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#include <pulse/xmalloc.h>
#include <pulse/timeval.h>
#include <pulse/version.h>

#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/strbuf.h>
#include <pulsecore/conf-parser.h>
#include <pulsecore/resampler.h>
#include <pulsecore/macro.h>

#include "daemon-conf.h"

#define DEFAULT_SCRIPT_FILE PA_DEFAULT_CONFIG_DIR PA_PATH_SEP "default.pa"
#define DEFAULT_SCRIPT_FILE_USER PA_PATH_SEP "default.pa"
#define DEFAULT_SYSTEM_SCRIPT_FILE PA_DEFAULT_CONFIG_DIR PA_PATH_SEP "system.pa"

#define DEFAULT_CONFIG_FILE PA_DEFAULT_CONFIG_DIR PA_PATH_SEP "daemon.conf"
#define DEFAULT_CONFIG_FILE_USER PA_PATH_SEP "daemon.conf"

#define ENV_SCRIPT_FILE "PULSE_SCRIPT"
#define ENV_CONFIG_FILE "PULSE_CONFIG"
#define ENV_DL_SEARCH_PATH "PULSE_DLPATH"

static const pa_daemon_conf default_conf = {
    .cmd = PA_CMD_DAEMON,
    .daemonize = false,
    .fail = true,
    .high_priority = true,
    .nice_level = -11,
    .realtime_scheduling = true,
    .realtime_priority = 5,  /* Half of JACK's default rtprio */
    .disallow_module_loading = false,
    .disallow_exit = false,
    .flat_volumes = true,
    .exit_idle_time = 20,
    .scache_idle_time = 20,
    .script_commands = NULL,
    .dl_search_path = NULL,
    .load_default_script_file = true,
    .default_script_file = NULL,
    .log_target = NULL,
    .log_level = PA_LOG_NOTICE,
    .log_backtrace = 0,
    .log_meta = false,
    .log_time = false,
    .resample_method = PA_RESAMPLER_AUTO,
    .disable_remixing = false,
    .disable_lfe_remixing = true,
    .config_file = NULL,
    .use_pid_file = true,
    .system_instance = false,
#ifdef HAVE_DBUS
    .local_server_type = PA_SERVER_TYPE_UNSET, /* The actual default is _USER, but we have to detect when the user doesn't specify this option. */
#endif
    .no_cpu_limit = true,
    .disable_shm = false,
    .lock_memory = false,
    .deferred_volume = true,
    .default_n_fragments = 4,
    .default_fragment_size_msec = 25,
    .deferred_volume_safety_margin_usec = 8000,
    .deferred_volume_extra_delay_usec = 0,
    .default_sample_spec = { .format = PA_SAMPLE_S16NE, .rate = 44100, .channels = 2 },
    .alternate_sample_rate = 48000,
    .default_channel_map = { .channels = 2, .map = { PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT } },
    .shm_size = 0
#ifdef HAVE_SYS_RESOURCE_H
   ,.rlimit_fsize = { .value = 0, .is_set = false },
    .rlimit_data = { .value = 0, .is_set = false },
    .rlimit_stack = { .value = 0, .is_set = false },
    .rlimit_core = { .value = 0, .is_set = false }
#ifdef RLIMIT_RSS
   ,.rlimit_rss = { .value = 0, .is_set = false }
#endif
#ifdef RLIMIT_NPROC
   ,.rlimit_nproc = { .value = 0, .is_set = false }
#endif
#ifdef RLIMIT_NOFILE
   ,.rlimit_nofile = { .value = 256, .is_set = true }
#endif
#ifdef RLIMIT_MEMLOCK
   ,.rlimit_memlock = { .value = 0, .is_set = false }
#endif
#ifdef RLIMIT_AS
   ,.rlimit_as = { .value = 0, .is_set = false }
#endif
#ifdef RLIMIT_LOCKS
   ,.rlimit_locks = { .value = 0, .is_set = false }
#endif
#ifdef RLIMIT_SIGPENDING
   ,.rlimit_sigpending = { .value = 0, .is_set = false }
#endif
#ifdef RLIMIT_MSGQUEUE
   ,.rlimit_msgqueue = { .value = 0, .is_set = false }
#endif
#ifdef RLIMIT_NICE
   ,.rlimit_nice = { .value = 31, .is_set = true }     /* nice level of -11 */
#endif
#ifdef RLIMIT_RTPRIO
   ,.rlimit_rtprio = { .value = 9, .is_set = true }    /* One below JACK's default for the server */
#endif
#ifdef RLIMIT_RTTIME
   ,.rlimit_rttime = { .value = 200*PA_USEC_PER_MSEC, .is_set = true } /* rtkit's limit is 200 ms */
#endif
#endif
};

pa_daemon_conf *pa_daemon_conf_new(void) {
    pa_daemon_conf *c;

    c = pa_xnewdup(pa_daemon_conf, &default_conf, 1);

#ifdef OS_IS_WIN32
    c->dl_search_path = pa_sprintf_malloc("%s" PA_PATH_SEP "lib" PA_PATH_SEP "pulse-%d.%d" PA_PATH_SEP "modules",
                                          pa_win32_get_toplevel(NULL), PA_MAJOR, PA_MINOR);
#else
    if (pa_run_from_build_tree()) {
        pa_log_notice("Detected that we are run from the build tree, fixing search path.");
        c->dl_search_path = pa_xstrdup(PA_BUILDDIR);
    } else
        c->dl_search_path = pa_xstrdup(PA_DLSEARCHPATH);
#endif

    return c;
}

void pa_daemon_conf_free(pa_daemon_conf *c) {
    pa_assert(c);

    pa_xfree(c->script_commands);
    pa_xfree(c->dl_search_path);
    pa_xfree(c->default_script_file);

    if (c->log_target)
        pa_log_target_free(c->log_target);

    pa_xfree(c->config_file);
    pa_xfree(c);
}

int pa_daemon_conf_set_log_target(pa_daemon_conf *c, const char *string) {
    pa_log_target *log_target = NULL;

    pa_assert(c);
    pa_assert(string);

    if (!pa_streq(string, "auto")) {
        log_target = pa_log_parse_target(string);

        if (!log_target)
            return -1;
    }

    c->log_target = log_target;

    return 0;
}

int pa_daemon_conf_set_log_level(pa_daemon_conf *c, const char *string) {
    uint32_t u;
    pa_assert(c);
    pa_assert(string);

    if (pa_atou(string, &u) >= 0) {
        if (u >= PA_LOG_LEVEL_MAX)
            return -1;

        c->log_level = (pa_log_level_t) u;
    } else if (pa_startswith(string, "debug"))
        c->log_level = PA_LOG_DEBUG;
    else if (pa_startswith(string, "info"))
        c->log_level = PA_LOG_INFO;
    else if (pa_startswith(string, "notice"))
        c->log_level = PA_LOG_NOTICE;
    else if (pa_startswith(string, "warn"))
        c->log_level = PA_LOG_WARN;
    else if (pa_startswith(string, "err"))
        c->log_level = PA_LOG_ERROR;
    else
        return -1;

    return 0;
}

int pa_daemon_conf_set_resample_method(pa_daemon_conf *c, const char *string) {
    int m;
    pa_assert(c);
    pa_assert(string);

    if ((m = pa_parse_resample_method(string)) < 0)
        return -1;

    c->resample_method = m;
    return 0;
}

int pa_daemon_conf_set_local_server_type(pa_daemon_conf *c, const char *string) {
    pa_assert(c);
    pa_assert(string);

    if (pa_streq(string, "user"))
        c->local_server_type = PA_SERVER_TYPE_USER;
    else if (pa_streq(string, "system")) {
        c->local_server_type = PA_SERVER_TYPE_SYSTEM;
    } else if (pa_streq(string, "none")) {
        c->local_server_type = PA_SERVER_TYPE_NONE;
    } else
        return -1;

    return 0;
}

static int parse_log_target(pa_config_parser_state *state) {
    pa_daemon_conf *c;

    pa_assert(state);

    c = state->data;

    if (pa_daemon_conf_set_log_target(c, state->rvalue) < 0) {
        pa_log(_("[%s:%u] Invalid log target '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    return 0;
}

static int parse_log_level(pa_config_parser_state *state) {
    pa_daemon_conf *c;

    pa_assert(state);

    c = state->data;

    if (pa_daemon_conf_set_log_level(c, state->rvalue) < 0) {
        pa_log(_("[%s:%u] Invalid log level '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    return 0;
}

static int parse_resample_method(pa_config_parser_state *state) {
    pa_daemon_conf *c;

    pa_assert(state);

    c = state->data;

    if (pa_daemon_conf_set_resample_method(c, state->rvalue) < 0) {
        pa_log(_("[%s:%u] Invalid resample method '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    return 0;
}

#ifdef HAVE_SYS_RESOURCE_H
static int parse_rlimit(pa_config_parser_state *state) {
    struct pa_rlimit *r;

    pa_assert(state);

    r = state->data;

    if (state->rvalue[strspn(state->rvalue, "\t ")] == 0) {
        /* Empty string */
        r->is_set = 0;
        r->value = 0;
    } else {
        int32_t k;
        if (pa_atoi(state->rvalue, &k) < 0) {
            pa_log(_("[%s:%u] Invalid rlimit '%s'."), state->filename, state->lineno, state->rvalue);
            return -1;
        }
        r->is_set = k >= 0;
        r->value = k >= 0 ? (rlim_t) k : 0;
    }

    return 0;
}
#endif

static int parse_sample_format(pa_config_parser_state *state) {
    pa_daemon_conf *c;
    pa_sample_format_t f;

    pa_assert(state);

    c = state->data;

    if ((f = pa_parse_sample_format(state->rvalue)) < 0) {
        pa_log(_("[%s:%u] Invalid sample format '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    c->default_sample_spec.format = f;
    return 0;
}

static int parse_sample_rate(pa_config_parser_state *state) {
    pa_daemon_conf *c;
    uint32_t r;

    pa_assert(state);

    c = state->data;

    if (pa_atou(state->rvalue, &r) < 0 || !pa_sample_rate_valid(r)) {
        pa_log(_("[%s:%u] Invalid sample rate '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    c->default_sample_spec.rate = r;
    return 0;
}

static int parse_alternate_sample_rate(pa_config_parser_state *state) {
    pa_daemon_conf *c;
    uint32_t r;

    pa_assert(state);

    c = state->data;

    if (pa_atou(state->rvalue, &r) < 0 || !pa_sample_rate_valid(r)) {
        pa_log(_("[%s:%u] Invalid sample rate '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    c->alternate_sample_rate = r;
    return 0;
}

struct channel_conf_info {
    pa_daemon_conf *conf;
    bool default_sample_spec_set;
    bool default_channel_map_set;
};

static int parse_sample_channels(pa_config_parser_state *state) {
    struct channel_conf_info *i;
    int32_t n;

    pa_assert(state);

    i = state->data;

    if (pa_atoi(state->rvalue, &n) < 0 || !pa_channels_valid(n)) {
        pa_log(_("[%s:%u] Invalid sample channels '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    i->conf->default_sample_spec.channels = (uint8_t) n;
    i->default_sample_spec_set = true;
    return 0;
}

static int parse_channel_map(pa_config_parser_state *state) {
    struct channel_conf_info *i;

    pa_assert(state);

    i = state->data;

    if (!pa_channel_map_parse(&i->conf->default_channel_map, state->rvalue)) {
        pa_log(_("[%s:%u] Invalid channel map '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    i->default_channel_map_set = true;
    return 0;
}

static int parse_fragments(pa_config_parser_state *state) {
    pa_daemon_conf *c;
    int32_t n;

    pa_assert(state);

    c = state->data;

    if (pa_atoi(state->rvalue, &n) < 0 || n < 2) {
        pa_log(_("[%s:%u] Invalid number of fragments '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    c->default_n_fragments = (unsigned) n;
    return 0;
}

static int parse_fragment_size_msec(pa_config_parser_state *state) {
    pa_daemon_conf *c;
    int32_t n;

    pa_assert(state);

    c = state->data;

    if (pa_atoi(state->rvalue, &n) < 0 || n < 1) {
        pa_log(_("[%s:%u] Invalid fragment size '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    c->default_fragment_size_msec = (unsigned) n;
    return 0;
}

static int parse_nice_level(pa_config_parser_state *state) {
    pa_daemon_conf *c;
    int32_t level;

    pa_assert(state);

    c = state->data;

    if (pa_atoi(state->rvalue, &level) < 0 || level < -20 || level > 19) {
        pa_log(_("[%s:%u] Invalid nice level '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    c->nice_level = (int) level;
    return 0;
}

static int parse_rtprio(pa_config_parser_state *state) {
#if !defined(OS_IS_WIN32) && defined(HAVE_SCHED_H)
    pa_daemon_conf *c;
    int32_t rtprio;
#endif

    pa_assert(state);

#ifdef OS_IS_WIN32
    pa_log("[%s:%u] Realtime priority not available on win32.", state->filename, state->lineno);
#else
# ifdef HAVE_SCHED_H
    c = state->data;

    if (pa_atoi(state->rvalue, &rtprio) < 0 || rtprio < sched_get_priority_min(SCHED_FIFO) || rtprio > sched_get_priority_max(SCHED_FIFO)) {
        pa_log("[%s:%u] Invalid realtime priority '%s'.", state->filename, state->lineno, state->rvalue);
        return -1;
    }

    c->realtime_priority = (int) rtprio;
# endif
#endif /* OS_IS_WIN32 */

    return 0;
}

#ifdef HAVE_DBUS
static int parse_server_type(pa_config_parser_state *state) {
    pa_daemon_conf *c;

    pa_assert(state);

    c = state->data;

    if (pa_daemon_conf_set_local_server_type(c, state->rvalue) < 0) {
        pa_log(_("[%s:%u] Invalid server type '%s'."), state->filename, state->lineno, state->rvalue);
        return -1;
    }

    return 0;
}
#endif

int pa_daemon_conf_load(pa_daemon_conf *c, const char *filename) {
    int r = -1;
    FILE *f = NULL;
    struct channel_conf_info ci;
    pa_config_item table[] = {
        { "daemonize",                  pa_config_parse_bool,     &c->daemonize, NULL },
        { "fail",                       pa_config_parse_bool,     &c->fail, NULL },
        { "high-priority",              pa_config_parse_bool,     &c->high_priority, NULL },
        { "realtime-scheduling",        pa_config_parse_bool,     &c->realtime_scheduling, NULL },
        { "disallow-module-loading",    pa_config_parse_bool,     &c->disallow_module_loading, NULL },
        { "allow-module-loading",       pa_config_parse_not_bool, &c->disallow_module_loading, NULL },
        { "disallow-exit",              pa_config_parse_bool,     &c->disallow_exit, NULL },
        { "allow-exit",                 pa_config_parse_not_bool, &c->disallow_exit, NULL },
        { "use-pid-file",               pa_config_parse_bool,     &c->use_pid_file, NULL },
        { "system-instance",            pa_config_parse_bool,     &c->system_instance, NULL },
#ifdef HAVE_DBUS
        { "local-server-type",          parse_server_type,        c, NULL },
#endif
        { "no-cpu-limit",               pa_config_parse_bool,     &c->no_cpu_limit, NULL },
        { "cpu-limit",                  pa_config_parse_not_bool, &c->no_cpu_limit, NULL },
        { "disable-shm",                pa_config_parse_bool,     &c->disable_shm, NULL },
        { "enable-shm",                 pa_config_parse_not_bool, &c->disable_shm, NULL },
        { "flat-volumes",               pa_config_parse_bool,     &c->flat_volumes, NULL },
        { "lock-memory",                pa_config_parse_bool,     &c->lock_memory, NULL },
        { "enable-deferred-volume",     pa_config_parse_bool,     &c->deferred_volume, NULL },
        { "exit-idle-time",             pa_config_parse_int,      &c->exit_idle_time, NULL },
        { "scache-idle-time",           pa_config_parse_int,      &c->scache_idle_time, NULL },
        { "realtime-priority",          parse_rtprio,             c, NULL },
        { "dl-search-path",             pa_config_parse_string,   &c->dl_search_path, NULL },
        { "default-script-file",        pa_config_parse_string,   &c->default_script_file, NULL },
        { "log-target",                 parse_log_target,         c, NULL },
        { "log-level",                  parse_log_level,          c, NULL },
        { "verbose",                    parse_log_level,          c, NULL },
        { "resample-method",            parse_resample_method,    c, NULL },
        { "default-sample-format",      parse_sample_format,      c, NULL },
        { "default-sample-rate",        parse_sample_rate,        c, NULL },
        { "alternate-sample-rate",      parse_alternate_sample_rate, c, NULL },
        { "default-sample-channels",    parse_sample_channels,    &ci,  NULL },
        { "default-channel-map",        parse_channel_map,        &ci,  NULL },
        { "default-fragments",          parse_fragments,          c, NULL },
        { "default-fragment-size-msec", parse_fragment_size_msec, c, NULL },
        { "deferred-volume-safety-margin-usec",
                                        pa_config_parse_unsigned, &c->deferred_volume_safety_margin_usec, NULL },
        { "deferred-volume-extra-delay-usec",
                                        pa_config_parse_int,      &c->deferred_volume_extra_delay_usec, NULL },
        { "nice-level",                 parse_nice_level,         c, NULL },
        { "disable-remixing",           pa_config_parse_bool,     &c->disable_remixing, NULL },
        { "enable-remixing",            pa_config_parse_not_bool, &c->disable_remixing, NULL },
        { "disable-lfe-remixing",       pa_config_parse_bool,     &c->disable_lfe_remixing, NULL },
        { "enable-lfe-remixing",        pa_config_parse_not_bool, &c->disable_lfe_remixing, NULL },
        { "load-default-script-file",   pa_config_parse_bool,     &c->load_default_script_file, NULL },
        { "shm-size-bytes",             pa_config_parse_size,     &c->shm_size, NULL },
        { "log-meta",                   pa_config_parse_bool,     &c->log_meta, NULL },
        { "log-time",                   pa_config_parse_bool,     &c->log_time, NULL },
        { "log-backtrace",              pa_config_parse_unsigned, &c->log_backtrace, NULL },
#ifdef HAVE_SYS_RESOURCE_H
        { "rlimit-fsize",               parse_rlimit,             &c->rlimit_fsize, NULL },
        { "rlimit-data",                parse_rlimit,             &c->rlimit_data, NULL },
        { "rlimit-stack",               parse_rlimit,             &c->rlimit_stack, NULL },
        { "rlimit-core",                parse_rlimit,             &c->rlimit_core, NULL },
#ifdef RLIMIT_RSS
        { "rlimit-rss",                 parse_rlimit,             &c->rlimit_rss, NULL },
#endif
#ifdef RLIMIT_NOFILE
        { "rlimit-nofile",              parse_rlimit,             &c->rlimit_nofile, NULL },
#endif
#ifdef RLIMIT_AS
        { "rlimit-as",                  parse_rlimit,             &c->rlimit_as, NULL },
#endif
#ifdef RLIMIT_NPROC
        { "rlimit-nproc",               parse_rlimit,             &c->rlimit_nproc, NULL },
#endif
#ifdef RLIMIT_MEMLOCK
        { "rlimit-memlock",             parse_rlimit,             &c->rlimit_memlock, NULL },
#endif
#ifdef RLIMIT_LOCKS
        { "rlimit-locks",               parse_rlimit,             &c->rlimit_locks, NULL },
#endif
#ifdef RLIMIT_SIGPENDING
        { "rlimit-sigpending",          parse_rlimit,             &c->rlimit_sigpending, NULL },
#endif
#ifdef RLIMIT_MSGQUEUE
        { "rlimit-msgqueue",            parse_rlimit,             &c->rlimit_msgqueue, NULL },
#endif
#ifdef RLIMIT_NICE
        { "rlimit-nice",                parse_rlimit,             &c->rlimit_nice, NULL },
#endif
#ifdef RLIMIT_RTPRIO
        { "rlimit-rtprio",              parse_rlimit,             &c->rlimit_rtprio, NULL },
#endif
#ifdef RLIMIT_RTTIME
        { "rlimit-rttime",              parse_rlimit,             &c->rlimit_rttime, NULL },
#endif
#endif
        { NULL,                         NULL,                     NULL, NULL },
    };

    pa_xfree(c->config_file);
    c->config_file = NULL;

    f = filename ?
        pa_fopen_cloexec(c->config_file = pa_xstrdup(filename), "r") :
        pa_open_config_file(DEFAULT_CONFIG_FILE, DEFAULT_CONFIG_FILE_USER, ENV_CONFIG_FILE, &c->config_file);

    if (!f && errno != ENOENT) {
        pa_log_warn(_("Failed to open configuration file: %s"), pa_cstrerror(errno));
        goto finish;
    }

    ci.default_channel_map_set = ci.default_sample_spec_set = false;
    ci.conf = c;

    r = f ? pa_config_parse(c->config_file, f, table, NULL, NULL) : 0;

    if (r >= 0) {

        /* Make sure that channel map and sample spec fit together */

        if (ci.default_sample_spec_set &&
            ci.default_channel_map_set &&
            c->default_channel_map.channels != c->default_sample_spec.channels) {
            pa_log_error(_("The specified default channel map has a different number of channels than the specified default number of channels."));
            r = -1;
            goto finish;
        } else if (ci.default_sample_spec_set)
            pa_channel_map_init_extend(&c->default_channel_map, c->default_sample_spec.channels, PA_CHANNEL_MAP_DEFAULT);
        else if (ci.default_channel_map_set)
            c->default_sample_spec.channels = c->default_channel_map.channels;
    }

finish:
    if (f)
        fclose(f);

    return r;
}

int pa_daemon_conf_env(pa_daemon_conf *c) {
    char *e;
    pa_assert(c);

    if ((e = getenv(ENV_DL_SEARCH_PATH))) {
        pa_xfree(c->dl_search_path);
        c->dl_search_path = pa_xstrdup(e);
    }
    if ((e = getenv(ENV_SCRIPT_FILE))) {
        pa_xfree(c->default_script_file);
        c->default_script_file = pa_xstrdup(e);
    }

    return 0;
}

const char *pa_daemon_conf_get_default_script_file(pa_daemon_conf *c) {
    pa_assert(c);

    if (!c->default_script_file) {
        if (c->system_instance)
            c->default_script_file = pa_find_config_file(DEFAULT_SYSTEM_SCRIPT_FILE, NULL, ENV_SCRIPT_FILE);
        else
            c->default_script_file = pa_find_config_file(DEFAULT_SCRIPT_FILE, DEFAULT_SCRIPT_FILE_USER, ENV_SCRIPT_FILE);
    }

    return c->default_script_file;
}

FILE *pa_daemon_conf_open_default_script_file(pa_daemon_conf *c) {
    FILE *f;
    pa_assert(c);

    if (!c->default_script_file) {
        if (c->system_instance)
            f = pa_open_config_file(DEFAULT_SYSTEM_SCRIPT_FILE, NULL, ENV_SCRIPT_FILE, &c->default_script_file);
        else
            f = pa_open_config_file(DEFAULT_SCRIPT_FILE, DEFAULT_SCRIPT_FILE_USER, ENV_SCRIPT_FILE, &c->default_script_file);
    } else
        f = pa_fopen_cloexec(c->default_script_file, "r");

    return f;
}

char *pa_daemon_conf_dump(pa_daemon_conf *c) {
    static const char* const log_level_to_string[] = {
        [PA_LOG_DEBUG] = "debug",
        [PA_LOG_INFO] = "info",
        [PA_LOG_NOTICE] = "notice",
        [PA_LOG_WARN] = "warning",
        [PA_LOG_ERROR] = "error"
    };

#ifdef HAVE_DBUS
    static const char* const server_type_to_string[] = {
        [PA_SERVER_TYPE_UNSET] = "!!UNSET!!",
        [PA_SERVER_TYPE_USER] = "user",
        [PA_SERVER_TYPE_SYSTEM] = "system",
        [PA_SERVER_TYPE_NONE] = "none"
    };
#endif

    pa_strbuf *s;
    char cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    char *log_target = NULL;

    pa_assert(c);

    s = pa_strbuf_new();

    if (c->config_file)
        pa_strbuf_printf(s, _("### Read from configuration file: %s ###\n"), c->config_file);

    pa_assert(c->log_level < PA_LOG_LEVEL_MAX);

    if (c->log_target)
        log_target = pa_log_target_to_string(c->log_target);

    pa_strbuf_printf(s, "daemonize = %s\n", pa_yes_no(c->daemonize));
    pa_strbuf_printf(s, "fail = %s\n", pa_yes_no(c->fail));
    pa_strbuf_printf(s, "high-priority = %s\n", pa_yes_no(c->high_priority));
    pa_strbuf_printf(s, "nice-level = %i\n", c->nice_level);
    pa_strbuf_printf(s, "realtime-scheduling = %s\n", pa_yes_no(c->realtime_scheduling));
    pa_strbuf_printf(s, "realtime-priority = %i\n", c->realtime_priority);
    pa_strbuf_printf(s, "allow-module-loading = %s\n", pa_yes_no(!c->disallow_module_loading));
    pa_strbuf_printf(s, "allow-exit = %s\n", pa_yes_no(!c->disallow_exit));
    pa_strbuf_printf(s, "use-pid-file = %s\n", pa_yes_no(c->use_pid_file));
    pa_strbuf_printf(s, "system-instance = %s\n", pa_yes_no(c->system_instance));
#ifdef HAVE_DBUS
    pa_strbuf_printf(s, "local-server-type = %s\n", server_type_to_string[c->local_server_type]);
#endif
    pa_strbuf_printf(s, "cpu-limit = %s\n", pa_yes_no(!c->no_cpu_limit));
    pa_strbuf_printf(s, "enable-shm = %s\n", pa_yes_no(!c->disable_shm));
    pa_strbuf_printf(s, "flat-volumes = %s\n", pa_yes_no(c->flat_volumes));
    pa_strbuf_printf(s, "lock-memory = %s\n", pa_yes_no(c->lock_memory));
    pa_strbuf_printf(s, "exit-idle-time = %i\n", c->exit_idle_time);
    pa_strbuf_printf(s, "scache-idle-time = %i\n", c->scache_idle_time);
    pa_strbuf_printf(s, "dl-search-path = %s\n", pa_strempty(c->dl_search_path));
    pa_strbuf_printf(s, "default-script-file = %s\n", pa_strempty(pa_daemon_conf_get_default_script_file(c)));
    pa_strbuf_printf(s, "load-default-script-file = %s\n", pa_yes_no(c->load_default_script_file));
    pa_strbuf_printf(s, "log-target = %s\n", pa_strempty(log_target));
    pa_strbuf_printf(s, "log-level = %s\n", log_level_to_string[c->log_level]);
    pa_strbuf_printf(s, "resample-method = %s\n", pa_resample_method_to_string(c->resample_method));
    pa_strbuf_printf(s, "enable-remixing = %s\n", pa_yes_no(!c->disable_remixing));
    pa_strbuf_printf(s, "enable-lfe-remixing = %s\n", pa_yes_no(!c->disable_lfe_remixing));
    pa_strbuf_printf(s, "default-sample-format = %s\n", pa_sample_format_to_string(c->default_sample_spec.format));
    pa_strbuf_printf(s, "default-sample-rate = %u\n", c->default_sample_spec.rate);
    pa_strbuf_printf(s, "alternate-sample-rate = %u\n", c->alternate_sample_rate);
    pa_strbuf_printf(s, "default-sample-channels = %u\n", c->default_sample_spec.channels);
    pa_strbuf_printf(s, "default-channel-map = %s\n", pa_channel_map_snprint(cm, sizeof(cm), &c->default_channel_map));
    pa_strbuf_printf(s, "default-fragments = %u\n", c->default_n_fragments);
    pa_strbuf_printf(s, "default-fragment-size-msec = %u\n", c->default_fragment_size_msec);
    pa_strbuf_printf(s, "enable-deferred-volume = %s\n", pa_yes_no(c->deferred_volume));
    pa_strbuf_printf(s, "deferred-volume-safety-margin-usec = %u\n", c->deferred_volume_safety_margin_usec);
    pa_strbuf_printf(s, "deferred-volume-extra-delay-usec = %d\n", c->deferred_volume_extra_delay_usec);
    pa_strbuf_printf(s, "shm-size-bytes = %lu\n", (unsigned long) c->shm_size);
    pa_strbuf_printf(s, "log-meta = %s\n", pa_yes_no(c->log_meta));
    pa_strbuf_printf(s, "log-time = %s\n", pa_yes_no(c->log_time));
    pa_strbuf_printf(s, "log-backtrace = %u\n", c->log_backtrace);
#ifdef HAVE_SYS_RESOURCE_H
    pa_strbuf_printf(s, "rlimit-fsize = %li\n", c->rlimit_fsize.is_set ? (long int) c->rlimit_fsize.value : -1);
    pa_strbuf_printf(s, "rlimit-data = %li\n", c->rlimit_data.is_set ? (long int) c->rlimit_data.value : -1);
    pa_strbuf_printf(s, "rlimit-stack = %li\n", c->rlimit_stack.is_set ? (long int) c->rlimit_stack.value : -1);
    pa_strbuf_printf(s, "rlimit-core = %li\n", c->rlimit_core.is_set ? (long int) c->rlimit_core.value : -1);
#ifdef RLIMIT_RSS
    pa_strbuf_printf(s, "rlimit-rss = %li\n", c->rlimit_rss.is_set ? (long int) c->rlimit_rss.value : -1);
#endif
#ifdef RLIMIT_AS
    pa_strbuf_printf(s, "rlimit-as = %li\n", c->rlimit_as.is_set ? (long int) c->rlimit_as.value : -1);
#endif
#ifdef RLIMIT_NPROC
    pa_strbuf_printf(s, "rlimit-nproc = %li\n", c->rlimit_nproc.is_set ? (long int) c->rlimit_nproc.value : -1);
#endif
#ifdef RLIMIT_NOFILE
    pa_strbuf_printf(s, "rlimit-nofile = %li\n", c->rlimit_nofile.is_set ? (long int) c->rlimit_nofile.value : -1);
#endif
#ifdef RLIMIT_MEMLOCK
    pa_strbuf_printf(s, "rlimit-memlock = %li\n", c->rlimit_memlock.is_set ? (long int) c->rlimit_memlock.value : -1);
#endif
#ifdef RLIMIT_LOCKS
    pa_strbuf_printf(s, "rlimit-locks = %li\n", c->rlimit_locks.is_set ? (long int) c->rlimit_locks.value : -1);
#endif
#ifdef RLIMIT_SIGPENDING
    pa_strbuf_printf(s, "rlimit-sigpending = %li\n", c->rlimit_sigpending.is_set ? (long int) c->rlimit_sigpending.value : -1);
#endif
#ifdef RLIMIT_MSGQUEUE
    pa_strbuf_printf(s, "rlimit-msgqueue = %li\n", c->rlimit_msgqueue.is_set ? (long int) c->rlimit_msgqueue.value : -1);
#endif
#ifdef RLIMIT_NICE
    pa_strbuf_printf(s, "rlimit-nice = %li\n", c->rlimit_nice.is_set ? (long int) c->rlimit_nice.value : -1);
#endif
#ifdef RLIMIT_RTPRIO
    pa_strbuf_printf(s, "rlimit-rtprio = %li\n", c->rlimit_rtprio.is_set ? (long int) c->rlimit_rtprio.value : -1);
#endif
#ifdef RLIMIT_RTTIME
    pa_strbuf_printf(s, "rlimit-rttime = %li\n", c->rlimit_rttime.is_set ? (long int) c->rlimit_rttime.value : -1);
#endif
#endif

    pa_xfree(log_target);

    return pa_strbuf_tostring_free(s);
}
