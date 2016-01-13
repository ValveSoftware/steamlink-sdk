#ifndef foodaemonconfhfoo
#define foodaemonconfhfoo

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

#include <pulse/sample.h>
#include <pulse/channelmap.h>

#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/core.h>
#include <pulsecore/core-util.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

/* The actual command to execute */
typedef enum pa_daemon_conf_cmd {
    PA_CMD_DAEMON,  /* the default */
    PA_CMD_START,
    PA_CMD_HELP,
    PA_CMD_VERSION,
    PA_CMD_DUMP_CONF,
    PA_CMD_DUMP_MODULES,
    PA_CMD_KILL,
    PA_CMD_CHECK,
    PA_CMD_DUMP_RESAMPLE_METHODS,
    PA_CMD_CLEANUP_SHM
} pa_daemon_conf_cmd_t;

#ifdef HAVE_SYS_RESOURCE_H
typedef struct pa_rlimit {
    rlim_t value;
    bool is_set;
} pa_rlimit;
#endif

/* A structure containing configuration data for the PulseAudio server . */
typedef struct pa_daemon_conf {
    pa_daemon_conf_cmd_t cmd;
    bool daemonize,
        fail,
        high_priority,
        realtime_scheduling,
        disallow_module_loading,
        use_pid_file,
        system_instance,
        no_cpu_limit,
        disable_shm,
        disable_remixing,
        disable_lfe_remixing,
        load_default_script_file,
        disallow_exit,
        log_meta,
        log_time,
        flat_volumes,
        lock_memory,
        deferred_volume;
    pa_server_type_t local_server_type;
    int exit_idle_time,
        scache_idle_time,
        realtime_priority,
        nice_level,
        resample_method;
    char *script_commands, *dl_search_path, *default_script_file;
    pa_log_target *log_target;
    pa_log_level_t log_level;
    unsigned log_backtrace;
    char *config_file;

#ifdef HAVE_SYS_RESOURCE_H
    pa_rlimit rlimit_fsize, rlimit_data, rlimit_stack, rlimit_core;
#ifdef RLIMIT_RSS
    pa_rlimit rlimit_rss;
#endif
#ifdef RLIMIT_NOFILE
    pa_rlimit rlimit_nofile;
#endif
#ifdef RLIMIT_AS
    pa_rlimit rlimit_as;
#endif
#ifdef RLIMIT_NPROC
    pa_rlimit rlimit_nproc;
#endif
#ifdef RLIMIT_MEMLOCK
    pa_rlimit rlimit_memlock;
#endif
#ifdef RLIMIT_LOCKS
    pa_rlimit rlimit_locks;
#endif
#ifdef RLIMIT_SIGPENDING
    pa_rlimit rlimit_sigpending;
#endif
#ifdef RLIMIT_MSGQUEUE
    pa_rlimit rlimit_msgqueue;
#endif
#ifdef RLIMIT_NICE
    pa_rlimit rlimit_nice;
#endif
#ifdef RLIMIT_RTPRIO
    pa_rlimit rlimit_rtprio;
#endif
#ifdef RLIMIT_RTTIME
    pa_rlimit rlimit_rttime;
#endif
#endif

    unsigned default_n_fragments, default_fragment_size_msec;
    unsigned deferred_volume_safety_margin_usec;
    int deferred_volume_extra_delay_usec;
    pa_sample_spec default_sample_spec;
    uint32_t alternate_sample_rate;
    pa_channel_map default_channel_map;
    size_t shm_size;
} pa_daemon_conf;

/* Allocate a new structure and fill it with sane defaults */
pa_daemon_conf* pa_daemon_conf_new(void);
void pa_daemon_conf_free(pa_daemon_conf*c);

/* Load configuration data from the specified file overwriting the
 * current settings in *c. If filename is NULL load the default daemon
 * configuration file */
int pa_daemon_conf_load(pa_daemon_conf *c, const char *filename);

/* Pretty print the current configuration data of the daemon. The
 * returned string has to be freed manually. The output of this
 * function may be parsed with pa_daemon_conf_load(). */
char *pa_daemon_conf_dump(pa_daemon_conf *c);

/* Load the configuration data from the process' environment
 * overwriting the current settings in *c. */
int pa_daemon_conf_env(pa_daemon_conf *c);

/* Set these configuration variables in the structure by passing a string */
int pa_daemon_conf_set_log_target(pa_daemon_conf *c, const char *string);
int pa_daemon_conf_set_log_level(pa_daemon_conf *c, const char *string);
int pa_daemon_conf_set_resample_method(pa_daemon_conf *c, const char *string);
int pa_daemon_conf_set_local_server_type(pa_daemon_conf *c, const char *string);

const char *pa_daemon_conf_get_default_script_file(pa_daemon_conf *c);
FILE *pa_daemon_conf_open_default_script_file(pa_daemon_conf *c);

#endif
