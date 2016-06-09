/***
    This file is part of PulseAudio.

    Copyright 2012 Lennart Poettering

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
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include <systemd/sd-login.h>

#include <pulse/xmalloc.h>

#include <pulsecore/module.h>
#include <pulsecore/log.h>
#include <pulsecore/hashmap.h>
#include <pulsecore/idxset.h>
#include <pulsecore/modargs.h>

#include "module-systemd-login-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Create a client for each login session of this user");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

static const char* const valid_modargs[] = {
    NULL
};

struct session {
    char *id;
    pa_client *client;
};

struct userdata {
    pa_module *module;
    pa_core *core;
    pa_hashmap *sessions, *previous_sessions;
    sd_login_monitor *monitor;
    pa_io_event *io;
};

static int add_session(struct userdata *u, const char *id) {
    struct session *session;
    pa_client_new_data data;

    session = pa_xnew(struct session, 1);
    session->id = pa_xstrdup(id);

    pa_client_new_data_init(&data);
    data.module = u->module;
    data.driver = __FILE__;
    pa_proplist_setf(data.proplist, PA_PROP_APPLICATION_NAME, "Login Session %s", id);
    pa_proplist_sets(data.proplist, "systemd-login.session", id);
    session->client = pa_client_new(u->core, &data);
    pa_client_new_data_done(&data);

    if (!session->client) {
        pa_xfree(session->id);
        pa_xfree(session);
        return -1;
    }

    pa_hashmap_put(u->sessions, session->id, session);

    pa_log_debug("Added new session %s", id);
    return 0;
}

static void free_session(struct session *session) {
    pa_assert(session);

    pa_log_debug("Removing session %s", session->id);

    pa_client_free(session->client);
    pa_xfree(session->id);
    pa_xfree(session);
}

static int get_session_list(struct userdata *u) {
    int r;
    char **sessions;
    pa_hashmap *h;
    struct session *o;

    pa_assert(u);

    r = sd_uid_get_sessions(getuid(), 0, &sessions);
    if (r < 0)
        return -1;

    /* We copy all sessions that still exist from one hashmap to the
     * other and then flush the remaining ones */

    h = u->previous_sessions;
    u->previous_sessions = u->sessions;
    u->sessions = h;

    if (sessions) {
        char **s;

        /* Note that the sessions array is allocated with libc's
         * malloc()/free() calls, hence do not use pa_xfree() to free
         * this here. */

        for (s = sessions; *s; s++) {
            o = pa_hashmap_remove(u->previous_sessions, *s);
            if (o)
                pa_hashmap_put(u->sessions, o->id, o);
            else
                add_session(u, *s);

            free(*s);
        }

        free(sessions);
    }

    pa_hashmap_remove_all(u->previous_sessions);

    return 0;
}

static void monitor_cb(
        pa_mainloop_api*a,
        pa_io_event* e,
        int fd,
        pa_io_event_flags_t events,
        void *userdata) {

    struct userdata *u = userdata;

    pa_assert(u);

    sd_login_monitor_flush(u->monitor);
    get_session_list(u);
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_modargs *ma;
    sd_login_monitor *monitor = NULL;
    int r;

    pa_assert(m);

    /* If we are not actually running logind become a NOP */
    if (access("/run/systemd/seats/", F_OK) < 0)
        return 0;

    ma = pa_modargs_new(m->argument, valid_modargs);
    if (!ma) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    r = sd_login_monitor_new("session", &monitor);
    if (r < 0) {
        pa_log("Failed to create session monitor: %s", strerror(-r));
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->sessions = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) free_session);
    u->previous_sessions = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) free_session);
    u->monitor = monitor;

    u->io = u->core->mainloop->io_new(u->core->mainloop, sd_login_monitor_get_fd(monitor), PA_IO_EVENT_INPUT, monitor_cb, u);

    if (get_session_list(u) < 0)
        goto fail;

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    u = m->userdata;
    if (!u)
        return;

    if (u->sessions) {
        pa_hashmap_free(u->sessions);
        pa_hashmap_free(u->previous_sessions);
    }

    if (u->io)
        m->core->mainloop->io_free(u->io);

    if (u->monitor)
        sd_login_monitor_unref(u->monitor);

    pa_xfree(u);
}
