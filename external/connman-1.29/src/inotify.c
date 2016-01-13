/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
 *  Copyright (C) 2012-2014  BMW Car IT GmbH.
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

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/inotify.h>

#include <connman/storage.h>

#include "connman.h"

struct connman_inotify {
	unsigned int refcount;

	GIOChannel *channel;
	uint watch;
	int wd;

	GSList *list;
};

static void cleanup_inotify(gpointer user_data);

static void connman_inotify_ref(struct connman_inotify *i)
{
	__sync_fetch_and_add(&i->refcount, 1);
}

static void connman_inotify_unref(gpointer data)
{
	struct connman_inotify *i = data;

	if (__sync_fetch_and_sub(&i->refcount, 1) != 1)
		return;

	cleanup_inotify(data);
}

static GHashTable *inotify_hash;

static gboolean inotify_data(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	struct connman_inotify *inotify = user_data;
	char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
	char *next_event;
	gsize bytes_read;
	GIOStatus status;
	GSList *list;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		inotify->watch = 0;
		return FALSE;
	}

	status = g_io_channel_read_chars(channel, buffer,
					sizeof(buffer), &bytes_read, NULL);

	switch (status) {
	case G_IO_STATUS_NORMAL:
		break;
	case G_IO_STATUS_AGAIN:
		return TRUE;
	default:
		connman_error("Reading from inotify channel failed");
		inotify->watch = 0;
		return FALSE;
	}

	next_event = buffer;

	connman_inotify_ref(inotify);

	while (bytes_read > 0) {
		struct inotify_event *event;
		gchar *ident;
		gsize len;

		event = (struct inotify_event *) next_event;
		if (event->len)
			ident = next_event + sizeof(struct inotify_event);
		else
			ident = NULL;

		len = sizeof(struct inotify_event) + event->len;

		/* check if inotify_event block fit */
		if (len > bytes_read)
			break;

		next_event += len;
		bytes_read -= len;

		for (list = inotify->list; list; list = list->next) {
			inotify_event_cb callback = list->data;

			(*callback)(event, ident);
		}
	}

	connman_inotify_unref(inotify);

	return TRUE;
}

static int create_watch(const char *path, struct connman_inotify *inotify)
{
	int fd;

	DBG("Add directory watch for %s", path);

	fd = inotify_init();
	if (fd < 0)
		return -EIO;

	inotify->wd = inotify_add_watch(fd, path,
					IN_MODIFY | IN_CREATE | IN_DELETE |
					IN_MOVED_TO | IN_MOVED_FROM);
	if (inotify->wd < 0) {
		connman_error("Creation of %s watch failed", path);
		close(fd);
		return -EIO;
	}

	inotify->channel = g_io_channel_unix_new(fd);
	if (!inotify->channel) {
		connman_error("Creation of inotify channel failed");
		inotify_rm_watch(fd, inotify->wd);
		inotify->wd = 0;

		close(fd);
		return -EIO;
	}

	g_io_channel_set_close_on_unref(inotify->channel, TRUE);
	g_io_channel_set_encoding(inotify->channel, NULL, NULL);
	g_io_channel_set_buffered(inotify->channel, FALSE);

	inotify->watch = g_io_add_watch(inotify->channel,
				G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR,
				inotify_data, inotify);

	return 0;
}

static void remove_watch(struct connman_inotify *inotify)
{
	int fd;

	if (!inotify->channel)
		return;

	if (inotify->watch > 0)
		g_source_remove(inotify->watch);

	fd = g_io_channel_unix_get_fd(inotify->channel);

	if (inotify->wd >= 0)
		inotify_rm_watch(fd, inotify->wd);

	g_io_channel_unref(inotify->channel);
}

int connman_inotify_register(const char *path, inotify_event_cb callback)
{
	struct connman_inotify *inotify;
	int err;

	if (!callback)
		return -EINVAL;

	inotify = g_hash_table_lookup(inotify_hash, path);
	if (inotify)
		goto update;

	inotify = g_try_new0(struct connman_inotify, 1);
	if (!inotify)
		return -ENOMEM;

	inotify->refcount = 1;
	inotify->wd = -1;

	err = create_watch(path, inotify);
	if (err < 0) {
		g_free(inotify);
		return err;
	}

	g_hash_table_replace(inotify_hash, g_strdup(path), inotify);

update:
	inotify->list = g_slist_prepend(inotify->list, callback);

	return 0;
}

static void cleanup_inotify(gpointer user_data)
{
	struct connman_inotify *inotify = user_data;

	g_slist_free(inotify->list);

	remove_watch(inotify);
	g_free(inotify);
}

void connman_inotify_unregister(const char *path, inotify_event_cb callback)
{
	struct connman_inotify *inotify;

	inotify = g_hash_table_lookup(inotify_hash, path);
	if (!inotify)
		return;

	inotify->list = g_slist_remove(inotify->list, callback);
	if (inotify->list)
		return;

	g_hash_table_remove(inotify_hash, path);
}

int __connman_inotify_init(void)
{
	DBG("");

	inotify_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, connman_inotify_unref);
	return 0;
}

void __connman_inotify_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(inotify_hash);
}
