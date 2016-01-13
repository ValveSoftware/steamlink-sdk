/*
 *
 *  Web service library with GLib integration
 *
 *  Copyright (C) 2009-2012  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <gnutls/gnutls.h>

#include "giognutls.h"

//#define DBG(fmt, arg...)  printf("%s: " fmt "\n" , __func__ , ## arg)
#define DBG(fmt, arg...)

typedef struct _GIOGnuTLSChannel GIOGnuTLSChannel;
typedef struct _GIOGnuTLSWatch GIOGnuTLSWatch;

struct _GIOGnuTLSChannel {
	GIOChannel channel;
	gint fd;
	gnutls_certificate_credentials_t cred;
	gnutls_session_t session;
	bool established;
	bool again;
};

struct _GIOGnuTLSWatch {
	GSource source;
	GPollFD pollfd;
	GIOChannel *channel;
	GIOCondition condition;
};

static volatile int global_init_done = 0;

static inline void g_io_gnutls_global_init(void)
{
	if (__sync_bool_compare_and_swap(&global_init_done, 0, 1))
		gnutls_global_init();
}

static GIOStatus check_handshake(GIOChannel *channel, GError **err)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;
	int result;

	DBG("channel %p", channel);

	if (gnutls_channel->established)
		return G_IO_STATUS_NORMAL;

again:
	result = gnutls_handshake(gnutls_channel->session);

	if (result == GNUTLS_E_INTERRUPTED || result == GNUTLS_E_AGAIN) {
		GIOFlags flags = g_io_channel_get_flags(channel);

		if (gnutls_channel->again)
			return G_IO_STATUS_AGAIN;

		if (flags & G_IO_FLAG_NONBLOCK)
			return G_IO_STATUS_AGAIN;

		goto again;
	}

	if (result < 0) {
		g_set_error(err, G_IO_CHANNEL_ERROR,
				G_IO_CHANNEL_ERROR_FAILED, "Handshake failed");
		return G_IO_STATUS_ERROR;
	}

	gnutls_channel->established = true;

	DBG("handshake done");

	return G_IO_STATUS_NORMAL;
}

static GIOStatus g_io_gnutls_read(GIOChannel *channel, gchar *buf,
				gsize count, gsize *bytes_read, GError **err)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;
	GIOStatus status;
	ssize_t result;

	DBG("channel %p count %zu", channel, count);

	*bytes_read = 0;

again:
	status = check_handshake(channel, err);
	if (status != G_IO_STATUS_NORMAL)
		return status;

	result = gnutls_record_recv(gnutls_channel->session, buf, count);

	DBG("result %zd", result);

	if (result == GNUTLS_E_REHANDSHAKE) {
		gnutls_channel->established = false;
		goto again;
	}

	if (result == GNUTLS_E_INTERRUPTED || result == GNUTLS_E_AGAIN) {
		GIOFlags flags = g_io_channel_get_flags(channel);

		if (gnutls_channel->again)
			return G_IO_STATUS_AGAIN;

		if (flags & G_IO_FLAG_NONBLOCK)
			return G_IO_STATUS_AGAIN;

		goto again;
	}

	if (result == GNUTLS_E_UNEXPECTED_PACKET_LENGTH)
		return G_IO_STATUS_EOF;

	if (result < 0) {
		g_set_error(err, G_IO_CHANNEL_ERROR,
				G_IO_CHANNEL_ERROR_FAILED, "Stream corrupted");
		return G_IO_STATUS_ERROR;
	}

	*bytes_read = result;

	return (result > 0) ? G_IO_STATUS_NORMAL : G_IO_STATUS_EOF;
}

static GIOStatus g_io_gnutls_write(GIOChannel *channel, const gchar *buf,
				gsize count, gsize *bytes_written, GError **err)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;
	GIOStatus status;
	ssize_t result;

	DBG("channel %p count %zu", channel, count);

	*bytes_written = 0;

again:
	status = check_handshake(channel, err);
	if (status != G_IO_STATUS_NORMAL)
		return status;

	result = gnutls_record_send(gnutls_channel->session, buf, count);

	DBG("result %zd", result);

	if (result == GNUTLS_E_REHANDSHAKE) {
		gnutls_channel->established = false;
		goto again;
	}

	if (result == GNUTLS_E_INTERRUPTED || result == GNUTLS_E_AGAIN) {
		GIOFlags flags = g_io_channel_get_flags(channel);

		if (gnutls_channel->again)
			return G_IO_STATUS_AGAIN;

		if (flags & G_IO_FLAG_NONBLOCK)
			return G_IO_STATUS_AGAIN;

		goto again;
	}

	if (result < 0) {
		g_set_error(err, G_IO_CHANNEL_ERROR,
				G_IO_CHANNEL_ERROR_FAILED, "Stream corrupted");
		return G_IO_STATUS_ERROR;
        }

	*bytes_written = result;

	return (result > 0) ? G_IO_STATUS_NORMAL : G_IO_STATUS_EOF;
}

static GIOStatus g_io_gnutls_seek(GIOChannel *channel, gint64 offset,
						GSeekType type, GError **err)
{
	DBG("channel %p", channel);

	g_set_error_literal(err, G_IO_CHANNEL_ERROR,
				G_IO_CHANNEL_ERROR_FAILED, "Not supported");
	return G_IO_STATUS_ERROR;
}

static GIOStatus g_io_gnutls_close(GIOChannel *channel, GError **err)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;

	DBG("channel %p", channel);

	if (gnutls_channel->established)
		gnutls_bye(gnutls_channel->session, GNUTLS_SHUT_RDWR);

	if (close(gnutls_channel->fd) < 0) {
		g_set_error_literal(err, G_IO_CHANNEL_ERROR,
				G_IO_CHANNEL_ERROR_FAILED, "Closing failed");
		return G_IO_STATUS_ERROR;
	}

	return G_IO_STATUS_NORMAL;
}

static void g_io_gnutls_free(GIOChannel *channel)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;

	DBG("channel %p", channel);

	gnutls_deinit(gnutls_channel->session);

	gnutls_certificate_free_credentials(gnutls_channel->cred);

	g_free(gnutls_channel);
}

static GIOStatus g_io_gnutls_set_flags(GIOChannel *channel,
						GIOFlags flags, GError **err)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;
	glong fcntl_flags = 0;

	DBG("channel %p flags %u", channel, flags);

	if (flags & G_IO_FLAG_NONBLOCK)
		fcntl_flags |= O_NONBLOCK;

	if (fcntl(gnutls_channel->fd, F_SETFL, fcntl_flags) < 0) {
		g_set_error_literal(err, G_IO_CHANNEL_ERROR,
			G_IO_CHANNEL_ERROR_FAILED, "Setting flags failed");
		return G_IO_STATUS_ERROR;
	}

	return G_IO_STATUS_NORMAL;
}

static GIOFlags g_io_gnutls_get_flags(GIOChannel *channel)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;
	GIOFlags flags = 0;
	glong fcntl_flags;

	DBG("channel %p", channel);

	fcntl_flags = fcntl(gnutls_channel->fd, F_GETFL);
	if (fcntl_flags < 0)
		return 0;

	if (fcntl_flags & O_NONBLOCK)
		flags |= G_IO_FLAG_NONBLOCK;

	return flags;
}

static gboolean g_io_gnutls_prepare(GSource *source, gint *timeout)
{
	DBG("source %p", source);

	*timeout = -1;

	return FALSE;
}

static gboolean g_io_gnutls_check(GSource *source)
{
	GIOGnuTLSWatch *watch = (GIOGnuTLSWatch *) source;
	GIOCondition condition = watch->pollfd.revents;

	DBG("source %p condition %u", source, condition);

	if (condition & watch->condition)
		return TRUE;

	return FALSE;
}

static gboolean g_io_gnutls_dispatch(GSource *source, GSourceFunc callback,
							gpointer user_data)
{
	GIOGnuTLSWatch *watch = (GIOGnuTLSWatch *) source;
	GIOFunc func = (GIOFunc) callback;
	GIOCondition condition = watch->pollfd.revents;

	DBG("source %p condition %u", source, condition);

	if (!func)
		return FALSE;

	return func(watch->channel, condition & watch->condition, user_data);
}

static void g_io_gnutls_finalize(GSource *source)
{
	GIOGnuTLSWatch *watch = (GIOGnuTLSWatch *) source;

	DBG("source %p", source);

	g_io_channel_unref(watch->channel);
}

static GSourceFuncs gnutls_watch_funcs = {
	g_io_gnutls_prepare,
	g_io_gnutls_check,
	g_io_gnutls_dispatch,
	g_io_gnutls_finalize,
};

static GSource *g_io_gnutls_create_watch(GIOChannel *channel,
						GIOCondition condition)
{
	GIOGnuTLSChannel *gnutls_channel = (GIOGnuTLSChannel *) channel;
	GIOGnuTLSWatch *watch;
	GSource *source;

	DBG("channel %p condition %u", channel, condition);

	source = g_source_new(&gnutls_watch_funcs, sizeof(GIOGnuTLSWatch));

	watch = (GIOGnuTLSWatch *) source;

	watch->channel = channel;
	g_io_channel_ref(channel);

	watch->condition = condition;

	watch->pollfd.fd = gnutls_channel->fd;
	watch->pollfd.events = condition;

	g_source_add_poll(source, &watch->pollfd);

	return source;
}

static GIOFuncs gnutls_channel_funcs = {
	g_io_gnutls_read,
	g_io_gnutls_write,
	g_io_gnutls_seek,
	g_io_gnutls_close,
	g_io_gnutls_create_watch,
	g_io_gnutls_free,
	g_io_gnutls_set_flags,
	g_io_gnutls_get_flags,
};

static ssize_t g_io_gnutls_push_func(gnutls_transport_ptr_t transport_data,
						const void *buf, size_t count)
{
	GIOGnuTLSChannel *gnutls_channel = transport_data;
	ssize_t result;

	DBG("count %zu", count);

	result = write(gnutls_channel->fd, buf, count);

	if (result < 0 && errno == EAGAIN)
		gnutls_channel->again = true;
	else
		gnutls_channel->again = false;

	DBG("result %zd", result);

	return result;
}

static ssize_t g_io_gnutls_pull_func(gnutls_transport_ptr_t transport_data,
						void *buf, size_t count)
{
	GIOGnuTLSChannel *gnutls_channel = transport_data;
	ssize_t result;

	DBG("count %zu", count);

	result = read(gnutls_channel->fd, buf, count);

	if (result < 0 && errno == EAGAIN)
		gnutls_channel->again = true;
	else
		gnutls_channel->again = false;

	DBG("result %zd", result);

	return result;
}

bool g_io_channel_supports_tls(void)
{
	return true;
}

GIOChannel *g_io_channel_gnutls_new(int fd)
{
	GIOGnuTLSChannel *gnutls_channel;
	GIOChannel *channel;
	int err;

	DBG("");

	gnutls_channel = g_new(GIOGnuTLSChannel, 1);

	channel = (GIOChannel *) gnutls_channel;

	g_io_channel_init(channel);
	channel->funcs = &gnutls_channel_funcs;

	gnutls_channel->fd = fd;

	channel->is_seekable = FALSE;
	channel->is_readable = TRUE;
	channel->is_writeable = TRUE;

	channel->do_encode = FALSE;

	g_io_gnutls_global_init();

        err = gnutls_init(&gnutls_channel->session, GNUTLS_CLIENT);
	if (err < 0) {
		g_free(gnutls_channel);
		return NULL;
	}

	gnutls_transport_set_ptr(gnutls_channel->session, gnutls_channel);
        gnutls_transport_set_push_function(gnutls_channel->session,
						g_io_gnutls_push_func);
        gnutls_transport_set_pull_function(gnutls_channel->session,
						g_io_gnutls_pull_func);
#if GNUTLS_VERSION_NUMBER < 0x020c00
	gnutls_transport_set_lowat(gnutls_channel->session, 0);

	gnutls_priority_set_direct(gnutls_channel->session,
						"NORMAL:%COMPAT", NULL);
#else
	gnutls_priority_set_direct(gnutls_channel->session,
		"NORMAL:-VERS-TLS-ALL:+VERS-TLS1.0:+VERS-SSL3.0:%COMPAT", NULL);
#endif

	gnutls_certificate_allocate_credentials(&gnutls_channel->cred);
	gnutls_credentials_set(gnutls_channel->session,
				GNUTLS_CRD_CERTIFICATE, gnutls_channel->cred);

	DBG("channel %p", channel);

	return channel;
}
