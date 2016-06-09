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
#include <stdlib.h>
#include <stdio.h>

#include <gconf/gconf-client.h>
#include <glib.h>

#include <pulsecore/core-util.h>

#define PA_GCONF_ROOT "/system/pulseaudio"
#define PA_GCONF_PATH_MODULES PA_GCONF_ROOT"/modules"

static void handle_module(GConfClient *client, const char *name) {
    gchar p[1024];
    gboolean enabled, locked;
    int i;

    pa_snprintf(p, sizeof(p), PA_GCONF_PATH_MODULES"/%s/locked", name);
    locked = gconf_client_get_bool(client, p, FALSE);

    if (locked)
        return;

    pa_snprintf(p, sizeof(p), PA_GCONF_PATH_MODULES"/%s/enabled", name);
    enabled = gconf_client_get_bool(client, p, FALSE);

    printf("%c%s%c", enabled ? '+' : '-', name, 0);

    if (enabled) {

        for (i = 0; i < 10; i++) {
            gchar *n, *a;

            pa_snprintf(p, sizeof(p), PA_GCONF_PATH_MODULES"/%s/name%i", name, i);
            if (!(n = gconf_client_get_string(client, p, NULL)) || !*n)
                break;

            pa_snprintf(p, sizeof(p), PA_GCONF_PATH_MODULES"/%s/args%i", name, i);
            a = gconf_client_get_string(client, p, NULL);

            printf("%s%c%s%c", n, 0, a ? a : "", 0);

            g_free(n);
            g_free(a);
        }

        printf("%c", 0);
    }

    fflush(stdout);
}

static void modules_callback(
        GConfClient* client,
        guint cnxn_id,
        GConfEntry *entry,
        gpointer user_data) {

    const char *n;
    char buf[128];

    g_assert(strncmp(entry->key, PA_GCONF_PATH_MODULES"/", sizeof(PA_GCONF_PATH_MODULES)) == 0);

    n = entry->key + sizeof(PA_GCONF_PATH_MODULES);

    g_strlcpy(buf, n, sizeof(buf));
    buf[strcspn(buf, "/")] = 0;

    handle_module(client, buf);
}

int main(int argc, char *argv[]) {
    GMainLoop *g;
    GConfClient *client;
    GSList *modules, *m;

#if !GLIB_CHECK_VERSION(2,36,0)
    g_type_init();
#endif

    if (!(client = gconf_client_get_default()))
        goto fail;

    gconf_client_add_dir(client, PA_GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
    gconf_client_notify_add(client, PA_GCONF_PATH_MODULES, modules_callback, NULL, NULL, NULL);

    modules = gconf_client_all_dirs(client, PA_GCONF_PATH_MODULES, NULL);

    for (m = modules; m; m = m->next) {
        char *e = strrchr(m->data, '/');
        handle_module(client, e ? e+1 : m->data);
    }

    g_slist_free(modules);

    /* Signal the parent that we are now initialized */
    printf("!");
    fflush(stdout);

    g = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(g);
    g_main_loop_unref(g);

    g_object_unref(G_OBJECT(client));

    return 0;

fail:
    return 1;
}
