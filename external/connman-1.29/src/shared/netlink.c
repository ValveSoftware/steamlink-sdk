/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2011-2012  Intel Corporation. All rights reserved.
 *  Copyright (C) 2013-2014  BMW Car IT GmbH.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * This file is a copy from ELL which has been ported to use GLib's
 * data structures.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <gdbus.h>

#include "src/shared/util.h"
#include "src/shared/netlink.h"

#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif

struct command {
	unsigned int id;
	uint32_t seq;
	uint32_t len;
	netlink_command_func_t handler;
	netlink_destroy_func_t destroy;
	void *user_data;
};

struct notify {
	uint32_t group;
	netlink_notify_func_t handler;
	netlink_destroy_func_t destroy;
	void *user_data;
};

struct netlink_info {
	uint32_t pid;
	GIOChannel *channel;
	uint32_t next_seq;
	GQueue *command_queue;
	GHashTable *command_pending;
	GHashTable *command_lookup;
	unsigned int next_command_id;
	GHashTable *notify_groups;
	GHashTable *notify_lookup;
	unsigned int next_notify_id;
	netlink_debug_func_t debug_handler;
	netlink_destroy_func_t debug_destroy;
	void *debug_data;
};


static void destroy_command(struct command *command)
{
	if (command->destroy)
		command->destroy(command->user_data);

	g_free(command);
}

static void destroy_notify(struct notify *notify)
{
	if (notify->destroy)
		notify->destroy(notify->user_data);

	g_free(notify);
}

static gboolean can_write_data(GIOChannel *chan,
				GIOCondition cond, gpointer user_data)
{
	struct netlink_info *netlink = user_data;
	struct command *command;
	struct sockaddr_nl addr;
	const void *data;
	ssize_t written;
	int sk;

	command = g_queue_pop_head(netlink->command_queue);
	if (!command)
		return FALSE;

	sk = g_io_channel_unix_get_fd(chan);

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = 0;

	data = ((void *) command) + NLMSG_ALIGN(sizeof(struct command));

	written = sendto(sk, data, command->len, 0,
				(struct sockaddr *) &addr, sizeof(addr));
	if (written < 0 || (uint32_t) written != command->len) {
		g_hash_table_remove(netlink->command_lookup,
					GUINT_TO_POINTER(command->id));
		destroy_command(command);
		return FALSE;
	}

	util_hexdump('<', data, command->len,
				netlink->debug_handler, netlink->debug_data);

	g_hash_table_replace(netlink->command_pending,
				GUINT_TO_POINTER(command->seq), command);

	return g_queue_get_length(netlink->command_queue) > 0;
}

static void do_notify(gpointer key, gpointer value, gpointer user_data)
{
	struct nlmsghdr *nlmsg = user_data;
	struct notify *notify = value;

	if (notify->handler) {
		notify->handler(nlmsg->nlmsg_type, NLMSG_DATA(nlmsg),
			nlmsg->nlmsg_len - NLMSG_HDRLEN, notify->user_data);
	}
}

static void process_broadcast(struct netlink_info *netlink, uint32_t group,
						struct nlmsghdr *nlmsg)
{
	GHashTable *notify_list;

	notify_list = g_hash_table_lookup(netlink->notify_groups,
					       GUINT_TO_POINTER(group));
	if (!notify_list)
		return;

	g_hash_table_foreach(notify_list, do_notify, nlmsg);
}

static void process_message(struct netlink_info *netlink,
				struct nlmsghdr *nlmsg)
{
	const void *data = nlmsg;
	struct command *command;

	command = g_hash_table_lookup(netlink->command_pending,
					GUINT_TO_POINTER(nlmsg->nlmsg_seq));
	if (!command)
		return;

	g_hash_table_remove(netlink->command_pending,
					GUINT_TO_POINTER(nlmsg->nlmsg_seq));

	if (!command->handler)
		goto done;

	if (nlmsg->nlmsg_type < NLMSG_MIN_TYPE) {
		const struct nlmsgerr *err;

		switch (nlmsg->nlmsg_type) {
		case NLMSG_ERROR:
			err = data + NLMSG_HDRLEN;

			command->handler(-err->error, 0, NULL, 0,
							command->user_data);
			break;
		}
	} else {
		command->handler(0, nlmsg->nlmsg_type, data + NLMSG_HDRLEN,
					nlmsg->nlmsg_len - NLMSG_HDRLEN,
					command->user_data);
	}

done:
	g_hash_table_remove(netlink->command_lookup,
				GUINT_TO_POINTER(command->id));

	destroy_command(command);
}

static void process_multi(struct netlink_info *netlink, struct nlmsghdr *nlmsg)
{
	const void *data = nlmsg;
	struct command *command;

	command = g_hash_table_lookup(netlink->command_pending,
					GUINT_TO_POINTER(nlmsg->nlmsg_seq));
	if (!command)
		return;

	if (!command->handler)
		goto done;

	if (nlmsg->nlmsg_type < NLMSG_MIN_TYPE) {
		const struct nlmsgerr *err;

		switch (nlmsg->nlmsg_type) {
		case NLMSG_DONE:
		case NLMSG_ERROR:
			err = data + NLMSG_HDRLEN;

			command->handler(-err->error, 0, NULL, 0,
							command->user_data);
			break;
		}
	} else {
		command->handler(0, nlmsg->nlmsg_type, data + NLMSG_HDRLEN,
					nlmsg->nlmsg_len - NLMSG_HDRLEN,
					command->user_data);
		return;
	}

done:
	g_hash_table_remove(netlink->command_pending,
			GUINT_TO_POINTER(nlmsg->nlmsg_seq));

	g_hash_table_remove(netlink->command_lookup,
			GUINT_TO_POINTER(command->id));

	destroy_command(command);
}

static gboolean can_read_data(GIOChannel *chan,
				GIOCondition cond, gpointer data)
{
	struct netlink_info *netlink = data;
	struct cmsghdr *cmsg;
	struct msghdr msg;
	struct iovec iov;
	struct nlmsghdr *nlmsg;
	unsigned char buffer[4096];
	unsigned char control[32];
	uint32_t group = 0;
	ssize_t len;
	int sk;

	sk = g_io_channel_unix_get_fd(chan);

	iov.iov_base = buffer;
	iov.iov_len = sizeof(buffer);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

	len = recvmsg(sk, &msg, 0);
	if (len < 0)
		return FALSE;

	util_hexdump('>', buffer, len, netlink->debug_handler,
			netlink->debug_data);

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
					cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		struct nl_pktinfo *pktinfo;

		if (cmsg->cmsg_level != SOL_NETLINK)
			continue;

		if (cmsg->cmsg_type != NETLINK_PKTINFO)
			continue;

		pktinfo = (void *) CMSG_DATA(cmsg);

		group = pktinfo->group;
	}

	for (nlmsg = iov.iov_base; NLMSG_OK(nlmsg, (uint32_t) len);
					nlmsg = NLMSG_NEXT(nlmsg, len)) {
		if (group > 0 && nlmsg->nlmsg_seq == 0) {
			process_broadcast(netlink, group, nlmsg);
			continue;
		}

		if (nlmsg->nlmsg_pid != netlink->pid)
			continue;

		if (nlmsg->nlmsg_flags & NLM_F_MULTI)
			process_multi(netlink, nlmsg);
		else
			process_message(netlink, nlmsg);
	}

	return TRUE;
}

static gboolean netlink_event(GIOChannel *chan,
				GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
		return FALSE;

	return TRUE;
}

static int create_netlink_socket(int protocol, uint32_t *pid)
{
	struct sockaddr_nl addr;
	socklen_t addrlen = sizeof(addr);
	int sk, pktinfo = 1;

	sk = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
								protocol);
	if (sk < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(sk);
		return -1;
	}

	if (getsockname(sk, (struct sockaddr *) &addr, &addrlen) < 0) {
		close(sk);
		return -1;
	}

	if (setsockopt(sk, SOL_NETLINK, NETLINK_PKTINFO,
					&pktinfo, sizeof(pktinfo)) < 0) {
		close(sk);
		return -1;
	}

	if (pid)
		*pid = addr.nl_pid;

	return sk;
}

struct netlink_info *netlink_new(int protocol)
{
	struct netlink_info *netlink;
	int sk;

	netlink = g_try_new0(struct netlink_info, 1);
	if (!netlink)
		return NULL;

	netlink->next_seq = 1;
	netlink->next_command_id = 1;
	netlink->next_notify_id = 1;

	sk = create_netlink_socket(protocol, &netlink->pid);
	if (sk < 0) {
		g_free(netlink);
		return NULL;
	}

	netlink->channel = g_io_channel_unix_new(sk);
	g_io_channel_set_close_on_unref(netlink->channel, TRUE);

	g_io_channel_set_encoding(netlink->channel, NULL, NULL);
	g_io_channel_set_buffered(netlink->channel, FALSE);

	g_io_add_watch(netlink->channel, G_IO_IN, can_read_data, netlink);
	g_io_add_watch(netlink->channel, G_IO_NVAL | G_IO_HUP | G_IO_ERR,
			netlink_event, netlink);

	netlink->command_queue = g_queue_new();
	netlink->command_pending = g_hash_table_new(g_direct_hash,
							g_direct_equal);
	netlink->command_lookup = g_hash_table_new(g_direct_hash,
							g_direct_equal);

	netlink->notify_groups = g_hash_table_new(g_direct_hash,
							g_direct_equal);
	netlink->notify_lookup = g_hash_table_new(g_direct_hash,
							g_direct_equal);

	return netlink;
}

static gboolean cleanup_notify(gpointer key, gpointer value, gpointer user_data)
{
	struct notify *notify = value;

	destroy_notify(notify);

	return TRUE;

}

static gboolean cleanup_notify_group(gpointer key, gpointer value,
					gpointer user_data)
{
	GHashTable *notify_list = value;

	g_hash_table_foreach_remove(notify_list, cleanup_notify, user_data);
	g_hash_table_destroy(notify_list);

	return TRUE;
}

static gboolean cleanup_command(gpointer key, gpointer value,
					gpointer user_data)
{
	struct command *command = value;

	destroy_command(command);

	return TRUE;
}

void netlink_destroy(struct netlink_info *netlink)
{
	g_hash_table_destroy(netlink->notify_lookup);

	g_hash_table_foreach_remove(netlink->notify_groups,
				cleanup_notify_group, NULL);
	g_hash_table_destroy(netlink->notify_groups);

	g_queue_free(netlink->command_queue);

	g_hash_table_destroy(netlink->command_pending);

	g_hash_table_foreach_remove(netlink->command_lookup,
				cleanup_command, NULL);
	g_hash_table_destroy(netlink->command_lookup);

	g_io_channel_shutdown(netlink->channel, TRUE, NULL);
	g_io_channel_unref(netlink->channel);

	g_free(netlink);
}

unsigned int netlink_send(struct netlink_info *netlink,
			uint16_t type, uint16_t flags, const void *data,
			uint32_t len, netlink_command_func_t function,
			void *user_data, netlink_destroy_func_t destroy)
{
	struct command *command;
	struct nlmsghdr *nlmsg;
	size_t size;

	if (!netlink)
		return 0;

	if (!netlink->command_queue ||
			!netlink->command_pending ||
			!netlink->command_lookup)
		return 0;

	size = NLMSG_ALIGN(sizeof(struct command)) +
					NLMSG_HDRLEN + NLMSG_ALIGN(len);

	command = g_try_malloc0(size);
	if (!command)
		return 0;

	command->handler = function;
	command->destroy = destroy;
	command->user_data = user_data;

	command->id = netlink->next_command_id;

	g_hash_table_replace(netlink->command_lookup,
				GUINT_TO_POINTER(command->id), command);

	command->seq = netlink->next_seq++;
	command->len = NLMSG_HDRLEN + NLMSG_ALIGN(len);

	nlmsg = ((void *) command) + NLMSG_ALIGN(sizeof(struct command));

	nlmsg->nlmsg_len = command->len;
	nlmsg->nlmsg_type = type;
	nlmsg->nlmsg_flags = NLM_F_REQUEST | flags;
	nlmsg->nlmsg_seq = command->seq;
	nlmsg->nlmsg_pid = netlink->pid;

	if (data && len > 0)
		memcpy(((void *) nlmsg) + NLMSG_HDRLEN, data, len);

	g_queue_push_tail(netlink->command_queue, command);

	netlink->next_command_id++;

	/* Arm IOChannel to call can_write_data in case it is not armed yet. */
	if (g_queue_get_length(netlink->command_queue) == 1)
		g_io_add_watch(netlink->channel, G_IO_OUT, can_write_data,
				netlink);

	return command->id;
}

bool netlink_cancel(struct netlink_info *netlink, unsigned int id)
{
	struct command *command;

	if (!netlink || id == 0)
		return false;

	if (!netlink->command_queue ||
			!netlink->command_pending ||
			!netlink->command_lookup)
		return false;

	command = g_hash_table_lookup(netlink->command_lookup,
					GUINT_TO_POINTER(id));
	if (!command)
		return false;

	g_hash_table_remove(netlink->command_lookup, GUINT_TO_POINTER(id));

	if (!g_queue_remove(netlink->command_queue, command)) {
		g_hash_table_remove(netlink->command_pending,
					GUINT_TO_POINTER(command->seq));
	}

	destroy_command(command);

	return true;
}

static bool add_membership(struct netlink_info *netlink, uint32_t group)
{
	int sk, value = group;

	sk = g_io_channel_unix_get_fd(netlink->channel);

	if (setsockopt(sk, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
						&value, sizeof(value)) < 0)
		return false;

	return true;
}

static bool drop_membership(struct netlink_info *netlink, uint32_t group)
{
	int sk, value = group;

	sk = g_io_channel_unix_get_fd(netlink->channel);

	if (setsockopt(sk, SOL_NETLINK, NETLINK_DROP_MEMBERSHIP,
						&value, sizeof(value)) < 0)
		return false;

	return true;
}

unsigned int netlink_register(struct netlink_info *netlink,
			uint32_t group, netlink_notify_func_t function,
			void *user_data, netlink_destroy_func_t destroy)
{
	GHashTable *notify_list;
	struct notify *notify;
	unsigned int id;

	if (!netlink)
		return 0;

	if (!netlink->notify_groups || !netlink->notify_lookup)
		return 0;

	notify_list = g_hash_table_lookup(netlink->notify_groups,
						GUINT_TO_POINTER(group));
	if (!notify_list) {
		notify_list = g_hash_table_new(g_direct_hash, g_direct_equal);
		if (!notify_list)
			return 0;

		g_hash_table_replace(netlink->notify_groups,
					GUINT_TO_POINTER(group), notify_list);
	}

	notify = g_new(struct notify, 1);

	notify->group = group;
	notify->handler = function;
	notify->destroy = destroy;
	notify->user_data = user_data;

	id = netlink->next_notify_id;

	g_hash_table_replace(netlink->notify_lookup,
				GUINT_TO_POINTER(id), notify_list);
	g_hash_table_replace(notify_list, GUINT_TO_POINTER(id), notify);

	if (g_hash_table_size(notify_list) == 1) {
		if (add_membership(netlink, notify->group) == false)
			goto remove_notify;
	}

	netlink->next_notify_id++;

	return id;

remove_notify:
	g_hash_table_remove(notify_list, GUINT_TO_POINTER(id));
	g_hash_table_remove(netlink->notify_lookup, GUINT_TO_POINTER(id));
	g_free(notify);

	return 0;
}

bool netlink_unregister(struct netlink_info *netlink, unsigned int id)
{
	GHashTable *notify_list;
	struct notify *notify;

	if (!netlink || id == 0)
		return false;

	if (!netlink->notify_groups || !netlink->notify_lookup)
		return false;

	notify_list = g_hash_table_lookup(netlink->notify_lookup,
						GUINT_TO_POINTER(id));

	if (!notify_list)
		return false;

	g_hash_table_remove(netlink->notify_lookup, GUINT_TO_POINTER(id));

	notify = g_hash_table_lookup(notify_list, GUINT_TO_POINTER(id));
	if (!notify)
		return false;

	g_hash_table_remove(notify_list, GUINT_TO_POINTER(id));

	if (g_hash_table_size(notify_list) == 0)
		drop_membership(netlink, notify->group);

	destroy_notify(notify);

	return true;
}

bool netlink_set_debug(struct netlink_info *netlink,
			netlink_debug_func_t function,
			void *user_data, netlink_destroy_func_t destroy)
{
	if (!netlink)
		return false;

	if (netlink->debug_destroy)
		netlink->debug_destroy(netlink->debug_data);

	netlink->debug_handler = function;
	netlink->debug_destroy = destroy;
	netlink->debug_data = user_data;

	return true;
}
