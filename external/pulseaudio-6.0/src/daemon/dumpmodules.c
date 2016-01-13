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

#include <string.h>
#include <stdio.h>
#include <ltdl.h>

#include <pulse/util.h>

#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/macro.h>
#include <pulsecore/modinfo.h>

#include "dumpmodules.h"

#define PREFIX "module-"

static void short_info(const char *name, const char *path, pa_modinfo *i) {
    pa_assert(name);
    pa_assert(i);

    printf("%-40s%s\n", name, i->description ? i->description : "n/a");
}

static void long_info(const char *name, const char *path, pa_modinfo *i) {
    static int nl = 0;
    pa_assert(name);
    pa_assert(i);

    if (nl)
        printf("\n");

    nl = 1;

    printf(_("Name: %s\n"), name);

    if (!i->description && !i->version && !i->author && !i->usage)
        printf(_("No module information available\n"));
    else {
        if (i->version)
            printf(_("Version: %s\n"), i->version);
        if (i->description)
            printf(_("Description: %s\n"), i->description);
        if (i->author)
            printf(_("Author: %s\n"), i->author);
        if (i->usage)
            printf(_("Usage: %s\n"), i->usage);
        printf(_("Load Once: %s\n"), pa_yes_no(i->load_once));
        if (i->deprecated)
            printf(_("DEPRECATION WARNING: %s\n"), i->deprecated);
    }

    if (path)
        printf(_("Path: %s\n"), path);
}

static void show_info(const char *name, const char *path, void (*info)(const char *name, const char *path, pa_modinfo*i)) {
    pa_modinfo *i;

    pa_assert(name);

    if ((i = pa_modinfo_get_by_name(path ? path : name))) {
        info(name, path, i);
        pa_modinfo_free(i);
    }
}

static int is_preloaded(const char *name) {
    const lt_dlsymlist *l;

    for (l = lt_preloaded_symbols; l->name; l++) {
        char buf[64], *e;

        if (l->address)
            continue;

        pa_snprintf(buf, sizeof(buf), "%s", l->name);
        if ((e = strrchr(buf, '.')))
            *e = 0;

        if (pa_streq(name, buf))
            return 1;
    }

    return 0;
}

static int callback(const char *path, lt_ptr data) {
    const char *e;
    pa_daemon_conf *c = (data);

    e = pa_path_get_filename(path);

    if (strlen(e) <= sizeof(PREFIX)-1 || strncmp(e, PREFIX, sizeof(PREFIX)-1))
        return 0;

    if (is_preloaded(e))
        return 0;

    show_info(e, path, c->log_level >= PA_LOG_INFO ? long_info : short_info);
    return 0;
}

void pa_dump_modules(pa_daemon_conf *c, int argc, char * const argv[]) {
    pa_assert(c);

    if (argc > 0) {
        int i;
        for (i = 0; i < argc; i++)
            show_info(argv[i], NULL, long_info);
    } else {
        const lt_dlsymlist *l;

        for (l = lt_preloaded_symbols; l->name; l++) {
            char buf[64], *e;

            if (l->address)
                continue;

            if (strlen(l->name) <= sizeof(PREFIX)-1 || strncmp(l->name, PREFIX, sizeof(PREFIX)-1))
                continue;

            pa_snprintf(buf, sizeof(buf), "%s", l->name);
            if ((e = strrchr(buf, '.')))
                *e = 0;

            show_info(buf, NULL, c->log_level >= PA_LOG_INFO ? long_info : short_info);
        }

        lt_dlforeachfile(NULL, callback, c);
    }
}
