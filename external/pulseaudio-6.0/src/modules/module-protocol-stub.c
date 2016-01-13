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
#include <unistd.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <pulse/xmalloc.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/socket.h>
#include <pulsecore/socket-server.h>
#include <pulsecore/socket-util.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/native-common.h>
#include <pulsecore/creds.h>
#include <pulsecore/arpa-inet.h>

#ifdef USE_TCP_SOCKETS
#define SOCKET_DESCRIPTION "(TCP sockets)"
#define SOCKET_USAGE "port=<TCP port number> listen=<address to listen on>"
#else
#define SOCKET_DESCRIPTION "(UNIX sockets)"
#define SOCKET_USAGE "socket=<path to UNIX socket>"
#endif

#if defined(USE_PROTOCOL_SIMPLE)
#  include <pulsecore/protocol-simple.h>
#  define TCPWRAP_SERVICE "pulseaudio-simple"
#  define IPV4_PORT 4711
#  define UNIX_SOCKET "simple"
#  define MODULE_ARGUMENTS "rate", "format", "channels", "sink", "source", "playback", "record",

#  if defined(USE_TCP_SOCKETS)
#    include "module-simple-protocol-tcp-symdef.h"
#  else
#    include "module-simple-protocol-unix-symdef.h"
#  endif

  PA_MODULE_DESCRIPTION("Simple protocol "SOCKET_DESCRIPTION);
  PA_MODULE_USAGE("rate=<sample rate> "
                  "format=<sample format> "
                  "channels=<number of channels> "
                  "sink=<sink to connect to> "
                  "source=<source to connect to> "
                  "playback=<enable playback?> "
                  "record=<enable record?> "
                  SOCKET_USAGE);
#elif defined(USE_PROTOCOL_CLI)
#  include <pulsecore/protocol-cli.h>
#  define TCPWRAP_SERVICE "pulseaudio-cli"
#  define IPV4_PORT 4712
#  define UNIX_SOCKET "cli"
#  define MODULE_ARGUMENTS

#  ifdef USE_TCP_SOCKETS
#    include "module-cli-protocol-tcp-symdef.h"
#  else
#   include "module-cli-protocol-unix-symdef.h"
#  endif

  PA_MODULE_DESCRIPTION("Command line interface protocol "SOCKET_DESCRIPTION);
  PA_MODULE_USAGE(SOCKET_USAGE);
#elif defined(USE_PROTOCOL_HTTP)
#  include <pulsecore/protocol-http.h>
#  define TCPWRAP_SERVICE "pulseaudio-http"
#  define IPV4_PORT 4714
#  define UNIX_SOCKET "http"
#  define MODULE_ARGUMENTS

#  ifdef USE_TCP_SOCKETS
#    include "module-http-protocol-tcp-symdef.h"
#  else
#    include "module-http-protocol-unix-symdef.h"
#  endif

  PA_MODULE_DESCRIPTION("HTTP "SOCKET_DESCRIPTION);
  PA_MODULE_USAGE(SOCKET_USAGE);
#elif defined(USE_PROTOCOL_NATIVE)
#  include <pulsecore/protocol-native.h>
#  define TCPWRAP_SERVICE "pulseaudio-native"
#  define IPV4_PORT PA_NATIVE_DEFAULT_PORT
#  define UNIX_SOCKET PA_NATIVE_DEFAULT_UNIX_SOCKET
#  define MODULE_ARGUMENTS_COMMON "cookie", "auth-cookie", "auth-cookie-enabled", "auth-anonymous",

#  ifdef USE_TCP_SOCKETS
#    include "module-native-protocol-tcp-symdef.h"
#  else
#    include "module-native-protocol-unix-symdef.h"
#  endif

#  if defined(HAVE_CREDS) && !defined(USE_TCP_SOCKETS)
#    define MODULE_ARGUMENTS MODULE_ARGUMENTS_COMMON "auth-group", "auth-group-enable", "srbchannel",
#    define AUTH_USAGE "auth-group=<system group to allow access> auth-group-enable=<enable auth by UNIX group?> "
#    define SRB_USAGE "srbchannel=<enable shared ringbuffer communication channel?> "
#  elif defined(USE_TCP_SOCKETS)
#    define MODULE_ARGUMENTS MODULE_ARGUMENTS_COMMON "auth-ip-acl",
#    define AUTH_USAGE "auth-ip-acl=<IP address ACL to allow access> "
#    define SRB_USAGE
#  else
#    define MODULE_ARGUMENTS MODULE_ARGUMENTS_COMMON
#    define AUTH_USAGE
#    define SRB_USAGE
#    endif

  PA_MODULE_DESCRIPTION("Native protocol "SOCKET_DESCRIPTION);
  PA_MODULE_USAGE("auth-anonymous=<don't check for cookies?> "
                  "auth-cookie=<path to cookie file> "
                  "auth-cookie-enabled=<enable cookie authentication?> "
                  AUTH_USAGE
                  SRB_USAGE
                  SOCKET_USAGE);
#elif defined(USE_PROTOCOL_ESOUND)
#  include <pulsecore/protocol-esound.h>
#  include <pulsecore/esound.h>
#  define TCPWRAP_SERVICE "esound"
#  define IPV4_PORT ESD_DEFAULT_PORT
#  define MODULE_ARGUMENTS_COMMON "sink", "source", "auth-anonymous", "cookie", "auth-cookie", "auth-cookie-enabled",

#  ifdef USE_TCP_SOCKETS
#    include "module-esound-protocol-tcp-symdef.h"
#  else
#    include "module-esound-protocol-unix-symdef.h"
#  endif

#  if defined(USE_TCP_SOCKETS)
#    define MODULE_ARGUMENTS MODULE_ARGUMENTS_COMMON "auth-ip-acl",
#    define AUTH_USAGE "auth-ip-acl=<IP address ACL to allow access> "
#  else
#    define MODULE_ARGUMENTS MODULE_ARGUMENTS_COMMON
#    define AUTH_USAGE
#  endif

  PA_MODULE_DESCRIPTION("ESOUND protocol "SOCKET_DESCRIPTION);
  PA_MODULE_USAGE("sink=<sink to connect to> "
                  "source=<source to connect to> "
                  "auth-anonymous=<don't verify cookies?> "
                  "auth-cookie=<path to cookie file> "
                  "auth-cookie-enabled=<enable cookie authentication?> "
                  AUTH_USAGE
                  SOCKET_USAGE);
#else
#  error "Broken build system"
#endif

PA_MODULE_LOAD_ONCE(false);
PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_VERSION(PACKAGE_VERSION);

static const char* const valid_modargs[] = {
    MODULE_ARGUMENTS
#if defined(USE_TCP_SOCKETS)
    "port",
    "listen",
#else
    "socket",
#endif
    NULL
};

struct userdata {
    pa_module *module;

#if defined(USE_PROTOCOL_SIMPLE)
    pa_simple_protocol *simple_protocol;
    pa_simple_options *simple_options;
#elif defined(USE_PROTOCOL_CLI)
    pa_cli_protocol *cli_protocol;
#elif defined(USE_PROTOCOL_HTTP)
    pa_http_protocol *http_protocol;
#elif defined(USE_PROTOCOL_NATIVE)
    pa_native_protocol *native_protocol;
    pa_native_options *native_options;
#else
    pa_esound_protocol *esound_protocol;
    pa_esound_options *esound_options;
#endif

#if defined(USE_TCP_SOCKETS)
    pa_socket_server *socket_server_ipv4;
#  ifdef HAVE_IPV6
    pa_socket_server *socket_server_ipv6;
#  endif
#else
    pa_socket_server *socket_server_unix;
    char *socket_path;
#endif
};

static void socket_server_on_connection_cb(pa_socket_server*s, pa_iochannel *io, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(s);
    pa_assert(io);
    pa_assert(u);

#if defined(USE_PROTOCOL_SIMPLE)
    pa_simple_protocol_connect(u->simple_protocol, io, u->simple_options);
#elif defined(USE_PROTOCOL_CLI)
    pa_cli_protocol_connect(u->cli_protocol, io, u->module);
#elif defined(USE_PROTOCOL_HTTP)
    pa_http_protocol_connect(u->http_protocol, io, u->module);
#elif defined(USE_PROTOCOL_NATIVE)
    pa_native_protocol_connect(u->native_protocol, io, u->native_options);
#else
    pa_esound_protocol_connect(u->esound_protocol, io, u->esound_options);
#endif
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u = NULL;

#if defined(USE_TCP_SOCKETS)
    uint32_t port = IPV4_PORT;
    bool port_fallback = true;
    const char *listen_on;
#else
    int r;
#endif

#if defined(USE_PROTOCOL_NATIVE) || defined(USE_PROTOCOL_HTTP)
    char t[256];
#endif

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->module = m;

#if defined(USE_PROTOCOL_SIMPLE)
    u->simple_protocol = pa_simple_protocol_get(m->core);

    u->simple_options = pa_simple_options_new();
    if (pa_simple_options_parse(u->simple_options, m->core, ma) < 0)
        goto fail;
    u->simple_options->module = m;
#elif defined(USE_PROTOCOL_CLI)
    u->cli_protocol = pa_cli_protocol_get(m->core);
#elif defined(USE_PROTOCOL_HTTP)
    u->http_protocol = pa_http_protocol_get(m->core);
#elif defined(USE_PROTOCOL_NATIVE)
    u->native_protocol = pa_native_protocol_get(m->core);

    u->native_options = pa_native_options_new();
    if (pa_native_options_parse(u->native_options, m->core, ma) < 0)
        goto fail;
    u->native_options->module = m;
#else
    u->esound_protocol = pa_esound_protocol_get(m->core);

    u->esound_options = pa_esound_options_new();
    if (pa_esound_options_parse(u->esound_options, m->core, ma) < 0)
        goto fail;
    u->esound_options->module = m;
#endif

#if defined(USE_TCP_SOCKETS)

    if (pa_in_system_mode() || pa_modargs_get_value(ma, "port", NULL))
        port_fallback = false;

    if (pa_modargs_get_value_u32(ma, "port", &port) < 0 || port < 1 || port > 0xFFFF) {
        pa_log("port= expects a numerical argument between 1 and 65535.");
        goto fail;
    }

    listen_on = pa_modargs_get_value(ma, "listen", NULL);

    if (listen_on) {
#  ifdef HAVE_IPV6
        u->socket_server_ipv6 = pa_socket_server_new_ipv6_string(m->core->mainloop, listen_on, (uint16_t) port, port_fallback, TCPWRAP_SERVICE);
#  endif
        u->socket_server_ipv4 = pa_socket_server_new_ipv4_string(m->core->mainloop, listen_on, (uint16_t) port, port_fallback, TCPWRAP_SERVICE);
    } else {
#  ifdef HAVE_IPV6
        u->socket_server_ipv6 = pa_socket_server_new_ipv6_any(m->core->mainloop, (uint16_t) port, port_fallback, TCPWRAP_SERVICE);
#  endif
        u->socket_server_ipv4 = pa_socket_server_new_ipv4_any(m->core->mainloop, (uint16_t) port, port_fallback, TCPWRAP_SERVICE);
    }

#  ifdef HAVE_IPV6
    if (!u->socket_server_ipv4 && !u->socket_server_ipv6)
#  else
    if (!u->socket_server_ipv4)
#  endif
        goto fail;

    if (u->socket_server_ipv4)
        pa_socket_server_set_callback(u->socket_server_ipv4, socket_server_on_connection_cb, u);
#  ifdef HAVE_IPV6
    if (u->socket_server_ipv6)
        pa_socket_server_set_callback(u->socket_server_ipv6, socket_server_on_connection_cb, u);
#  endif

#else

#  if defined(USE_PROTOCOL_ESOUND)

#    if defined(USE_PER_USER_ESOUND_SOCKET)
    u->socket_path = pa_sprintf_malloc("/tmp/.esd-%lu/socket", (unsigned long) getuid());
#    else
    u->socket_path = pa_xstrdup("/tmp/.esd/socket");
#    endif

    /* This socket doesn't reside in our own runtime dir but in
     * /tmp/.esd/, hence we have to create the dir first */

    if (pa_make_secure_parent_dir(u->socket_path, pa_in_system_mode() ? 0755U : 0700U, (uid_t)-1, (gid_t)-1, false) < 0) {
        pa_log("Failed to create socket directory '%s': %s\n", u->socket_path, pa_cstrerror(errno));
        goto fail;
    }

#  else
    if (!(u->socket_path = pa_runtime_path(pa_modargs_get_value(ma, "socket", UNIX_SOCKET)))) {
        pa_log("Failed to generate socket path.");
        goto fail;
    }
#  endif

    if ((r = pa_unix_socket_remove_stale(u->socket_path)) < 0) {
        pa_log("Failed to remove stale UNIX socket '%s': %s", u->socket_path, pa_cstrerror(errno));
        goto fail;
    } else if (r > 0)
        pa_log_info("Removed stale UNIX socket '%s'.", u->socket_path);

    if (!(u->socket_server_unix = pa_socket_server_new_unix(m->core->mainloop, u->socket_path)))
        goto fail;

    pa_socket_server_set_callback(u->socket_server_unix, socket_server_on_connection_cb, u);

#endif

#if defined(USE_PROTOCOL_NATIVE)
#  if defined(USE_TCP_SOCKETS)
    if (u->socket_server_ipv4)
        if (pa_socket_server_get_address(u->socket_server_ipv4, t, sizeof(t)))
            pa_native_protocol_add_server_string(u->native_protocol, t);

#    ifdef HAVE_IPV6
    if (u->socket_server_ipv6)
        if (pa_socket_server_get_address(u->socket_server_ipv6, t, sizeof(t)))
            pa_native_protocol_add_server_string(u->native_protocol, t);
#    endif
#  else
    if (pa_socket_server_get_address(u->socket_server_unix, t, sizeof(t)))
        pa_native_protocol_add_server_string(u->native_protocol, t);

#  endif
#endif

#if defined(USE_PROTOCOL_HTTP)
#if defined(USE_TCP_SOCKETS)
    if (u->socket_server_ipv4)
        if (pa_socket_server_get_address(u->socket_server_ipv4, t, sizeof(t)))
            pa_http_protocol_add_server_string(u->http_protocol, t);

#ifdef HAVE_IPV6
    if (u->socket_server_ipv6)
        if (pa_socket_server_get_address(u->socket_server_ipv6, t, sizeof(t)))
            pa_http_protocol_add_server_string(u->http_protocol, t);
#endif /* HAVE_IPV6 */
#else /* USE_TCP_SOCKETS */
    if (pa_socket_server_get_address(u->socket_server_unix, t, sizeof(t)))
        pa_http_protocol_add_server_string(u->http_protocol, t);

#endif /* USE_TCP_SOCKETS */
#endif /* USE_PROTOCOL_HTTP */

    if (ma)
        pa_modargs_free(ma);

    return 0;

fail:

    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

#if defined(USE_PROTOCOL_SIMPLE)
    if (u->simple_protocol) {
        pa_simple_protocol_disconnect(u->simple_protocol, u->module);
        pa_simple_protocol_unref(u->simple_protocol);
    }
    if (u->simple_options)
        pa_simple_options_unref(u->simple_options);
#elif defined(USE_PROTOCOL_CLI)
    if (u->cli_protocol) {
        pa_cli_protocol_disconnect(u->cli_protocol, u->module);
        pa_cli_protocol_unref(u->cli_protocol);
    }
#elif defined(USE_PROTOCOL_HTTP)
    if (u->http_protocol) {
        char t[256];

#if defined(USE_TCP_SOCKETS)
        if (u->socket_server_ipv4)
            if (pa_socket_server_get_address(u->socket_server_ipv4, t, sizeof(t)))
                pa_http_protocol_remove_server_string(u->http_protocol, t);

#ifdef HAVE_IPV6
        if (u->socket_server_ipv6)
            if (pa_socket_server_get_address(u->socket_server_ipv6, t, sizeof(t)))
                pa_http_protocol_remove_server_string(u->http_protocol, t);
#endif /* HAVE_IPV6 */
#else /* USE_TCP_SOCKETS */
        if (u->socket_server_unix)
            if (pa_socket_server_get_address(u->socket_server_unix, t, sizeof(t)))
                pa_http_protocol_remove_server_string(u->http_protocol, t);
#endif /* USE_PROTOCOL_HTTP */

        pa_http_protocol_disconnect(u->http_protocol, u->module);
        pa_http_protocol_unref(u->http_protocol);
    }
#elif defined(USE_PROTOCOL_NATIVE)
    if (u->native_protocol) {

        char t[256];

#  if defined(USE_TCP_SOCKETS)
        if (u->socket_server_ipv4)
            if (pa_socket_server_get_address(u->socket_server_ipv4, t, sizeof(t)))
                pa_native_protocol_remove_server_string(u->native_protocol, t);

#    ifdef HAVE_IPV6
        if (u->socket_server_ipv6)
            if (pa_socket_server_get_address(u->socket_server_ipv6, t, sizeof(t)))
                pa_native_protocol_remove_server_string(u->native_protocol, t);
#    endif
#  else
        if (u->socket_server_unix)
            if (pa_socket_server_get_address(u->socket_server_unix, t, sizeof(t)))
                pa_native_protocol_remove_server_string(u->native_protocol, t);
#  endif

        pa_native_protocol_disconnect(u->native_protocol, u->module);
        pa_native_protocol_unref(u->native_protocol);
    }
    if (u->native_options)
        pa_native_options_unref(u->native_options);
#else
    if (u->esound_protocol) {
        pa_esound_protocol_disconnect(u->esound_protocol, u->module);
        pa_esound_protocol_unref(u->esound_protocol);
    }
    if (u->esound_options)
        pa_esound_options_unref(u->esound_options);
#endif

#if defined(USE_TCP_SOCKETS)
    if (u->socket_server_ipv4)
        pa_socket_server_unref(u->socket_server_ipv4);
#  ifdef HAVE_IPV6
    if (u->socket_server_ipv6)
        pa_socket_server_unref(u->socket_server_ipv6);
#  endif
#else
    if (u->socket_server_unix)
        pa_socket_server_unref(u->socket_server_unix);

# if defined(USE_PROTOCOL_ESOUND) && !defined(USE_PER_USER_ESOUND_SOCKET)
    if (u->socket_path) {
        char *p = pa_parent_dir(u->socket_path);
        rmdir(p);
        pa_xfree(p);
    }
# endif

    pa_xfree(u->socket_path);
#endif

    pa_xfree(u);
}
