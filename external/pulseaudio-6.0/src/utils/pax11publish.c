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

#include <stdio.h>
#include <getopt.h>
#include <assert.h>
#include <locale.h>

#include <xcb/xcb.h>

#include <pulse/util.h>
#include <pulse/client-conf.h>

#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/log.h>
#include <pulsecore/authkey.h>
#include <pulsecore/native-common.h>
#include <pulsecore/x11prop.h>

int main(int argc, char *argv[]) {
    const char *dname = NULL, *sink = NULL, *source = NULL, *server = NULL, *cookie_file = PA_NATIVE_COOKIE_FILE;
    int c, ret = 1, screen = 0;
    xcb_connection_t *xcb = NULL;
    enum { DUMP, EXPORT, IMPORT, REMOVE } mode = DUMP;

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PULSE_LOCALEDIR);
#endif

    while ((c = getopt(argc, argv, "deiD:S:O:I:c:hr")) != -1) {
        switch (c) {
            case 'D' :
                dname = optarg;
                break;
            case 'h':
                printf(_("%s [-D display] [-S server] [-O sink] [-I source] [-c file]  [-d|-e|-i|-r]\n\n"
                       " -d    Show current PulseAudio data attached to X11 display (default)\n"
                       " -e    Export local PulseAudio data to X11 display\n"
                       " -i    Import PulseAudio data from X11 display to local environment variables and cookie file.\n"
                       " -r    Remove PulseAudio data from X11 display\n"),
                       pa_path_get_filename(argv[0]));
                ret = 0;
                goto finish;
            case 'd':
                mode = DUMP;
                break;
            case 'e':
                mode = EXPORT;
                break;
            case 'i':
                mode = IMPORT;
                break;
            case 'r':
                mode = REMOVE;
                break;
            case 'c':
                cookie_file = optarg;
                break;
            case 'I':
                source = optarg;
                break;
            case 'O':
                sink = optarg;
                break;
            case 'S':
                server = optarg;
                break;
            default:
                fprintf(stderr, _("Failed to parse command line.\n"));
                goto finish;
        }
    }

    if (!(xcb = xcb_connect(dname, &screen))) {
        pa_log(_("xcb_connect() failed"));
        goto finish;
    }

    if (xcb_connection_has_error(xcb)) {
        pa_log(_("xcb_connection_has_error() returned true"));
        goto finish;
    }

    switch (mode) {
        case DUMP: {
            char t[1024];
            if (pa_x11_get_prop(xcb, screen, "PULSE_SERVER", t, sizeof(t)))
                printf(_("Server: %s\n"), t);
            if (pa_x11_get_prop(xcb, screen, "PULSE_SOURCE", t, sizeof(t)))
                printf(_("Source: %s\n"), t);
            if (pa_x11_get_prop(xcb, screen, "PULSE_SINK", t, sizeof(t)))
                printf(_("Sink: %s\n"), t);
            if (pa_x11_get_prop(xcb, screen, "PULSE_COOKIE", t, sizeof(t)))
                printf(_("Cookie: %s\n"), t);

            break;
        }

        case IMPORT: {
            char t[1024];
            if (pa_x11_get_prop(xcb, screen, "PULSE_SERVER", t, sizeof(t)))
                printf("PULSE_SERVER='%s'\nexport PULSE_SERVER\n", t);
            if (pa_x11_get_prop(xcb, screen, "PULSE_SOURCE", t, sizeof(t)))
                printf("PULSE_SOURCE='%s'\nexport PULSE_SOURCE\n", t);
            if (pa_x11_get_prop(xcb, screen, "PULSE_SINK", t, sizeof(t)))
                printf("PULSE_SINK='%s'\nexport PULSE_SINK\n", t);

            if (pa_x11_get_prop(xcb, screen, "PULSE_COOKIE", t, sizeof(t))) {
                uint8_t cookie[PA_NATIVE_COOKIE_LENGTH];
                size_t l;
                if ((l = pa_parsehex(t, cookie, sizeof(cookie))) != sizeof(cookie)) {
                    fprintf(stderr, _("Failed to parse cookie data\n"));
                    goto finish;
                }

                if (pa_authkey_save(cookie_file, cookie, l) < 0) {
                    fprintf(stderr, _("Failed to save cookie data\n"));
                    goto finish;
                }
            }

            break;
        }

        case EXPORT: {
            pa_client_conf *conf = pa_client_conf_new();
            uint8_t cookie[PA_NATIVE_COOKIE_LENGTH];
            char hx[PA_NATIVE_COOKIE_LENGTH*2+1];
            assert(conf);

            pa_client_conf_load(conf, false, true);

            pa_x11_del_prop(xcb, screen, "PULSE_SERVER");
            pa_x11_del_prop(xcb, screen, "PULSE_SINK");
            pa_x11_del_prop(xcb, screen, "PULSE_SOURCE");
            pa_x11_del_prop(xcb, screen, "PULSE_ID");
            pa_x11_del_prop(xcb, screen, "PULSE_COOKIE");

            if (server)
                pa_x11_set_prop(xcb, screen, "PULSE_SERVER", server);
            else if (conf->default_server)
                pa_x11_set_prop(xcb, screen, "PULSE_SERVER", conf->default_server);
            else {
                char hn[256];
                if (!pa_get_fqdn(hn, sizeof(hn))) {
                    fprintf(stderr, _("Failed to get FQDN.\n"));
                    goto finish;
                }

                pa_x11_set_prop(xcb, screen, "PULSE_SERVER", hn);
            }

            if (sink)
                pa_x11_set_prop(xcb, screen, "PULSE_SINK", sink);
            else if (conf->default_sink)
                pa_x11_set_prop(xcb, screen, "PULSE_SINK", conf->default_sink);

            if (source)
                pa_x11_set_prop(xcb, screen, "PULSE_SOURCE", source);
            if (conf->default_source)
                pa_x11_set_prop(xcb, screen, "PULSE_SOURCE", conf->default_source);

            pa_client_conf_free(conf);

            if (pa_authkey_load(cookie_file, true, cookie, sizeof(cookie)) < 0) {
                fprintf(stderr, _("Failed to load cookie data\n"));
                goto finish;
            }

            pa_x11_set_prop(xcb, screen, "PULSE_COOKIE", pa_hexstr(cookie, sizeof(cookie), hx, sizeof(hx)));
            break;
        }

        case REMOVE:
            pa_x11_del_prop(xcb, screen, "PULSE_SERVER");
            pa_x11_del_prop(xcb, screen, "PULSE_SINK");
            pa_x11_del_prop(xcb, screen, "PULSE_SOURCE");
            pa_x11_del_prop(xcb, screen, "PULSE_ID");
            pa_x11_del_prop(xcb, screen, "PULSE_COOKIE");
            pa_x11_del_prop(xcb, screen, "PULSE_SESSION_ID");
            break;

        default:
            fprintf(stderr, _("Not yet implemented.\n"));
            goto finish;
    }

    ret = 0;

finish:

    if (xcb) {
        xcb_flush(xcb);
        xcb_disconnect(xcb);
    }

    return ret;
}
