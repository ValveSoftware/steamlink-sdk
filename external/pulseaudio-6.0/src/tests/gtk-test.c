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

#pragma GCC diagnostic ignored "-Wstrict-prototypes"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>

#include <pulse/context.h>
#include <pulse/glib-mainloop.h>

pa_context *ctxt;
pa_glib_mainloop *m;

static void context_state_callback(pa_context *c, void *userdata);

static void connect(void) {
    int r;

    ctxt = pa_context_new(pa_glib_mainloop_get_api(m), NULL);
    g_assert(ctxt);

    r = pa_context_connect(ctxt, NULL, PA_CONTEXT_NOAUTOSPAWN|PA_CONTEXT_NOFAIL, NULL);
    g_assert(r == 0);

    pa_context_set_state_callback(ctxt, context_state_callback, NULL);
}

static void context_state_callback(pa_context *c, void *userdata) {
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_FAILED:
            pa_context_unref(ctxt);
            ctxt = NULL;
            connect();
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[]) {

    GtkWidget *window;

    gtk_init(&argc, &argv);

    g_set_application_name("This is a test");
    gtk_window_set_default_icon_name("foobar");
    g_setenv("PULSE_PROP_media.role", "phone", TRUE);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW (window), g_get_application_name());
    gtk_widget_show_all(window);

    m = pa_glib_mainloop_new(NULL);
    g_assert(m);

    connect();
    gtk_main();

    pa_context_unref(ctxt);
    pa_glib_mainloop_free(m);

    return 0;
}
