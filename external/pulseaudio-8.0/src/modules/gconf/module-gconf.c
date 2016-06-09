/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering

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

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <pulse/xmalloc.h>
#include <pulsecore/module.h>
#include <pulsecore/core.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulse/mainloop-api.h>
#include <pulsecore/core-error.h>
#include <pulsecore/start-child.h>

#include "module-gconf-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("GConf Adapter");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#define MAX_MODULES 10
#define BUF_MAX 2048

struct userdata;

struct module_item {
    char *name;
    char *args;
    uint32_t index;
};

struct pa_module_info {
    struct userdata *userdata;
    char *name;

    struct module_item items[MAX_MODULES];
    unsigned n_items;
};

struct userdata {
    pa_core *core;
    pa_module *module;

    pa_hashmap *module_infos;

    pid_t pid;

    int fd;
    int fd_type;
    pa_io_event *io_event;

    char buf[BUF_MAX];
    size_t buf_fill;
};

static int fill_buf(struct userdata *u) {
    ssize_t r;
    pa_assert(u);

    if (u->buf_fill >= BUF_MAX) {
        pa_log("read buffer overflow");
        return -1;
    }

    if ((r = pa_read(u->fd, u->buf + u->buf_fill, BUF_MAX - u->buf_fill, &u->fd_type)) <= 0)
        return -1;

    u->buf_fill += (size_t) r;
    return 0;
}

static int read_byte(struct userdata *u) {
    int ret;
    pa_assert(u);

    if (u->buf_fill < 1)
        if (fill_buf(u) < 0)
            return -1;

    ret = u->buf[0];
    pa_assert(u->buf_fill > 0);
    u->buf_fill--;
    memmove(u->buf, u->buf+1, u->buf_fill);
    return ret;
}

static char *read_string(struct userdata *u) {
    pa_assert(u);

    for (;;) {
        char *e;

        if ((e = memchr(u->buf, 0, u->buf_fill))) {
            char *ret = pa_xstrdup(u->buf);
            u->buf_fill -= (size_t) (e - u->buf +1);
            memmove(u->buf, e+1, u->buf_fill);
            return ret;
        }

        if (fill_buf(u) < 0)
            return NULL;
    }
}

static void unload_one_module(struct pa_module_info *m, unsigned i) {
    struct userdata *u;

    pa_assert(m);
    pa_assert(i < m->n_items);

    u = m->userdata;

    if (m->items[i].index == PA_INVALID_INDEX)
        return;

    pa_log_debug("Unloading module #%i", m->items[i].index);
    pa_module_unload_by_index(u->core, m->items[i].index, true);
    m->items[i].index = PA_INVALID_INDEX;
    pa_xfree(m->items[i].name);
    pa_xfree(m->items[i].args);
    m->items[i].name = m->items[i].args = NULL;
}

static void unload_all_modules(struct pa_module_info *m) {
    unsigned i;

    pa_assert(m);

    for (i = 0; i < m->n_items; i++)
        unload_one_module(m, i);

    m->n_items = 0;
}

static void load_module(
        struct pa_module_info *m,
        unsigned i,
        const char *name,
        const char *args,
        bool is_new) {

    struct userdata *u;
    pa_module *mod;

    pa_assert(m);
    pa_assert(name);
    pa_assert(args);

    u = m->userdata;

    if (!is_new) {
        if (m->items[i].index != PA_INVALID_INDEX &&
            pa_streq(m->items[i].name, name) &&
            pa_streq(m->items[i].args, args))
            return;

        unload_one_module(m, i);
    }

    pa_log_debug("Loading module '%s' with args '%s' due to GConf configuration.", name, args);

    m->items[i].name = pa_xstrdup(name);
    m->items[i].args = pa_xstrdup(args);
    m->items[i].index = PA_INVALID_INDEX;

    if (!(mod = pa_module_load(u->core, name, args))) {
        pa_log("pa_module_load() failed");
        return;
    }

    m->items[i].index = mod->index;
}

static void module_info_free(void *p) {
    struct pa_module_info *m = p;

    pa_assert(m);

    unload_all_modules(m);
    pa_xfree(m->name);
    pa_xfree(m);
}

static int handle_event(struct userdata *u) {
    int opcode;
    int ret = 0;

    do {
        if ((opcode = read_byte(u)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                break;
            goto fail;
        }

        switch (opcode) {
            case '!':
                /* The helper tool is now initialized */
                ret = 1;
                break;

            case '+': {
                char *name;
                struct pa_module_info *m;
                unsigned i, j;

                if (!(name = read_string(u)))
                    goto fail;

                if (!(m = pa_hashmap_get(u->module_infos, name))) {
                    m = pa_xnew(struct pa_module_info, 1);
                    m->userdata = u;
                    m->name = name;
                    m->n_items = 0;
                    pa_hashmap_put(u->module_infos, m->name, m);
                } else
                    pa_xfree(name);

                i = 0;
                while (i < MAX_MODULES) {
                    char *module, *args;

                    if (!(module = read_string(u))) {
                        if (i > m->n_items) m->n_items = i;
                        goto fail;
                    }

                    if (!*module) {
                        pa_xfree(module);
                        break;
                    }

                    if (!(args = read_string(u))) {
                        pa_xfree(module);

                        if (i > m->n_items) m->n_items = i;
                        goto fail;
                    }

                    load_module(m, i, module, args, i >= m->n_items);

                    i++;

                    pa_xfree(module);
                    pa_xfree(args);
                }

                /* Unload all removed modules */
                for (j = i; j < m->n_items; j++)
                    unload_one_module(m, j);

                m->n_items = i;

                break;
            }

            case '-': {
                char *name;

                if (!(name = read_string(u)))
                    goto fail;

                pa_hashmap_remove_and_free(u->module_infos, name);
                pa_xfree(name);

                break;
            }
        }
    } while (u->buf_fill > 0 && ret == 0);

    return ret;

fail:
    pa_log("Unable to read or parse data from client.");
    return -1;
}

static void io_event_cb(
        pa_mainloop_api*a,
        pa_io_event* e,
        int fd,
        pa_io_event_flags_t events,
        void *userdata) {

    struct userdata *u = userdata;

    if (handle_event(u) < 0) {

        if (u->io_event) {
            u->core->mainloop->io_free(u->io_event);
            u->io_event = NULL;
        }

        pa_module_unload_request(u->module, true);
    }
}

int pa__init(pa_module*m) {
    struct userdata *u;
    int r;

    u = pa_xnew(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    m->userdata = u;
    u->module_infos = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) module_info_free);
    u->pid = (pid_t) -1;
    u->fd = -1;
    u->fd_type = 0;
    u->io_event = NULL;
    u->buf_fill = 0;

    if ((u->fd = pa_start_child_for_read(
#if defined(__linux__) && !defined(__OPTIMIZE__)
                              pa_run_from_build_tree() ? PA_BUILDDIR "/gconf-helper" :
#endif
                 PA_GCONF_HELPER, NULL, &u->pid)) < 0)
        goto fail;

    u->io_event = m->core->mainloop->io_new(
            m->core->mainloop,
            u->fd,
            PA_IO_EVENT_INPUT,
            io_event_cb,
            u);

    do {
        if ((r = handle_event(u)) < 0)
            goto fail;

        /* Read until the client signalled us that it is ready with
         * initialization */
    } while (r != 1);

    return 0;

fail:
    pa__done(m);
    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->pid != (pid_t) -1) {
        kill(u->pid, SIGTERM);

        for (;;) {
            if (waitpid(u->pid, NULL, 0) >= 0)
                break;

            if (errno != EINTR) {
                pa_log("waitpid() failed: %s", pa_cstrerror(errno));
                break;
            }
        }
    }

    if (u->io_event)
        m->core->mainloop->io_free(u->io_event);

    if (u->fd >= 0)
        pa_close(u->fd);

    if (u->module_infos)
        pa_hashmap_free(u->module_infos);

    pa_xfree(u);
}
