/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "connman.h"

enum rfkill_type {
	RFKILL_TYPE_ALL = 0,
	RFKILL_TYPE_WLAN,
	RFKILL_TYPE_BLUETOOTH,
	RFKILL_TYPE_UWB,
	RFKILL_TYPE_WWAN,
	RFKILL_TYPE_GPS,
	RFKILL_TYPE_FM,
	NUM_RFKILL_TYPES,
};

enum rfkill_operation {
	RFKILL_OP_ADD = 0,
	RFKILL_OP_DEL,
	RFKILL_OP_CHANGE,
	RFKILL_OP_CHANGE_ALL,
};

struct rfkill_event {
	uint32_t idx;
	uint8_t  type;
	uint8_t  op;
	uint8_t  soft;
	uint8_t  hard;
};

static enum connman_service_type convert_type(uint8_t type)
{
	switch (type) {
	case RFKILL_TYPE_WLAN:
		return CONNMAN_SERVICE_TYPE_WIFI;
	case RFKILL_TYPE_BLUETOOTH:
		return CONNMAN_SERVICE_TYPE_BLUETOOTH;
	case RFKILL_TYPE_WWAN:
		return CONNMAN_SERVICE_TYPE_CELLULAR;
	}

	return CONNMAN_SERVICE_TYPE_UNKNOWN;
}

static enum rfkill_type convert_service_type(enum connman_service_type type)
{
	switch (type) {
	case CONNMAN_SERVICE_TYPE_WIFI:
		return RFKILL_TYPE_WLAN;
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
		return RFKILL_TYPE_BLUETOOTH;
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		return RFKILL_TYPE_WWAN;
	case CONNMAN_SERVICE_TYPE_GPS:
		return RFKILL_TYPE_GPS;
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_VPN:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_P2P:
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
		return NUM_RFKILL_TYPES;
	}

	return NUM_RFKILL_TYPES;
}

static GIOStatus rfkill_process(GIOChannel *chan)
{
	unsigned char buf[32];
	struct rfkill_event *event = (void *) buf;
	enum connman_service_type type;
	gsize len;
	GIOStatus status;

	DBG("");

	memset(buf, 0, sizeof(buf));

	status = g_io_channel_read_chars(chan, (gchar *) buf,
			sizeof(struct rfkill_event), &len, NULL);

	if (status != G_IO_STATUS_NORMAL)
		return status;

	if (len != sizeof(struct rfkill_event))
		return status;

	DBG("idx %u type %u op %u soft %u hard %u", event->idx,
						event->type, event->op,
						event->soft, event->hard);

	type = convert_type(event->type);

	switch (event->op) {
	case RFKILL_OP_ADD:
		__connman_technology_add_rfkill(event->idx, type,
						event->soft, event->hard);
		break;
	case RFKILL_OP_DEL:
		__connman_technology_remove_rfkill(event->idx, type);
		break;
	case RFKILL_OP_CHANGE:
		__connman_technology_update_rfkill(event->idx, type,
						event->soft, event->hard);
		break;
	default:
		break;
	}

	return status;
}

static gboolean rfkill_event(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
		return FALSE;

	if (rfkill_process(chan) == G_IO_STATUS_ERROR)
		return FALSE;

	return TRUE;
}

static guint watch = 0;

int __connman_rfkill_block(enum connman_service_type type, bool block)
{
	uint8_t rfkill_type;
	struct rfkill_event event;
	ssize_t len;
	int fd, err = 0;

	DBG("type %d block %d", type, block);

	rfkill_type = convert_service_type(type);
	if (rfkill_type == NUM_RFKILL_TYPES)
		return -EINVAL;

	fd = open("/dev/rfkill", O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	memset(&event, 0, sizeof(event));
	event.op = RFKILL_OP_CHANGE_ALL;
	event.type = rfkill_type;
	event.soft = block;

	len = write(fd, &event, sizeof(event));
	if (len < 0) {
		err = -errno;
		connman_error("Failed to change RFKILL state");
	}

	close(fd);

	return err;
}

int __connman_rfkill_init(void)
{
	GIOChannel *channel;
	GIOFlags flags;
	int fd;

	DBG("");

	fd = open("/dev/rfkill", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		connman_error("Failed to open RFKILL control device");
		return -EIO;
	}

	channel = g_io_channel_unix_new(fd);
	g_io_channel_set_close_on_unref(channel, TRUE);

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	flags = g_io_channel_get_flags(channel);
	flags |= G_IO_FLAG_NONBLOCK;
	g_io_channel_set_flags(channel, flags, NULL);

	/* Process current RFKILL events sent on device open */
	while (rfkill_process(channel) == G_IO_STATUS_NORMAL);

	watch = g_io_add_watch(channel,
				G_IO_IN | G_IO_NVAL | G_IO_HUP | G_IO_ERR,
				rfkill_event, NULL);

	g_io_channel_unref(channel);

	return watch ? 0 : -EIO;
}

void __connman_rfkill_cleanup(void)
{
	DBG("");

	if (watch) {
		g_source_remove(watch);
		watch = 0;
	}
}
