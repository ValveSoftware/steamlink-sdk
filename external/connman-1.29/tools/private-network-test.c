/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
 *  Copyright (C) 2011	ProFUSION embedded systems
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

#define CONNMAN_SERVICE "net.connman"

#define MANAGER_PATH	"/"
#define MANAGER_INTERFACE CONNMAN_SERVICE ".Manager"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <gdbus.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include <dbus/dbus.h>

#ifndef DBUS_TYPE_UNIX_FD
#define DBUS_TYPE_UNIX_FD -1
#endif

static int release_private_network(DBusConnection *conn)
{
	DBusMessage *msg, *reply;
	DBusError error;

	msg = dbus_message_new_method_call(CONNMAN_SERVICE, MANAGER_PATH,
				MANAGER_INTERFACE, "ReleasePrivateNetwork");

	dbus_error_init(&error);

	printf("Releasing private-network...\n");
	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1,
									&error);
	dbus_message_unref(msg);

	if (!reply) {
		if (dbus_error_is_set(&error)) {
			fprintf(stderr, "1. %s\n", error.message);
			dbus_error_free(&error);
		} else {
			fprintf(stderr, "Release() failed");
		}

		return -1;
	}

	return 0;
}

static void request_private_network(DBusConnection *conn, int *out_fd,
					char **out_server_ip,
					char **out_peer_ip,
					char **out_primary_dns,
					char **out_secondary_dns)
{
	DBusMessageIter array, dict, entry;
	DBusMessage *msg, *reply;
	DBusError error;

	msg = dbus_message_new_method_call(CONNMAN_SERVICE, MANAGER_PATH,
				MANAGER_INTERFACE, "RequestPrivateNetwork");

	dbus_error_init(&error);

	printf("Requesting private-network...\n");
	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1,
									&error);
	dbus_message_unref(msg);

	if (!reply) {
		if (dbus_error_is_set(&error)) {
			fprintf(stderr, "1. %s\n", error.message);
			dbus_error_free(&error);
		} else {
			fprintf(stderr, "Request() failed");
		}
		return;
	}

	if (!dbus_message_iter_init(reply, &array))
		goto done;

	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_UNIX_FD)
		goto done;

	dbus_message_iter_get_basic(&array, out_fd);
	printf("Fildescriptor = %d\n", *out_fd);

	dbus_message_iter_next(&array);

	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_ARRAY)
		goto done;

	dbus_message_iter_recurse(&array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter iter;
		const char *key;
		int type;

		dbus_message_iter_recurse(&dict, &entry);

		dbus_message_iter_get_basic(&entry, &key);

		printf("key %s", key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &iter);

		type = dbus_message_iter_get_arg_type(&iter);
		if (type != DBUS_TYPE_STRING)
			break;

		if (g_str_equal(key, "ServerIPv4")
				&& type == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(&iter, out_server_ip);
			printf(" = %s\n", *out_server_ip);

		} else if (g_str_equal(key, "PeerIPv4")
				&& type == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(&iter, out_peer_ip);
			printf(" = %s\n", *out_peer_ip);

		} else if (g_str_equal(key, "PrimaryDNS")
				&& type == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(&iter, out_primary_dns);
			printf(" = %s\n", *out_primary_dns);

		} else if (g_str_equal(key, "SecondaryDNS")
				&& type == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(&iter, out_secondary_dns);
			printf(" = %s\n", *out_secondary_dns);
		}

		dbus_message_iter_next(&dict);
	}

done:
	dbus_message_unref(reply);
}

int main(int argc, char *argv[])
{
	DBusConnection *conn;
	int fd = -1;
	char *server_ip;
	char *peer_ip;
	char *primary_dns;
	char *secondary_dns;

	/*
	 * IP packet: src: 192.168.219.2 dst www.connman.net
	 * HTTP GET / request
	 */
	int buf1[81] = { 0x45, 0x00, 0x00, 0x51, 0x5a, 0xbe, 0x00, 0x00, 0x40,
			 0x06, 0x50, 0x73, 0xc0, 0xa8, 0xdb, 0x01, 0x3e, 0x4b,
			 0xf5, 0x80, 0x30, 0x3b, 0x00, 0x50, 0x00, 0x00, 0x00,
			 0x28, 0x04, 0xfd, 0xac, 0x9b, 0x50, 0x18, 0x02, 0x00,
			 0xa1, 0xb3, 0x00, 0x00, 0x47, 0x45, 0x54, 0x20, 0x2f,
			 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31,
			 0x0d, 0x0a, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20, 0x77,
			 0x77, 0x77, 0x2e, 0x63, 0x6f, 0x6e, 0x6e, 0x6d, 0x61,
			 0x6e, 0x2e, 0x6e, 0x65, 0x74, 0x0d, 0x0a, 0x0d, 0x0a};


	int buf2[81] = { 0x45, 0x00, 0x00, 0x51, 0x57, 0x9d, 0x00, 0x00, 0x40,
			 0x06, 0x53, 0x93, 0xc0, 0xa8, 0xdb, 0x02, 0x3e, 0x4b,
			 0xf5, 0x80, 0x30, 0x3b, 0x00, 0x50, 0x00, 0x00, 0x00,
			 0x28, 0x17, 0xdb, 0x2e, 0x6d, 0x50, 0x18, 0x02, 0x00,
			 0x0d, 0x03, 0x00, 0x00, 0x47, 0x45, 0x54, 0x20, 0x2f,
			 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31,
			 0x0d, 0x0a, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20, 0x77,
			 0x77, 0x77, 0x2e, 0x63, 0x6f, 0x6e, 0x6e, 0x6d, 0x61,
			 0x6e, 0x2e, 0x6e, 0x65, 0x74, 0x0d, 0x0a, 0x0d, 0x0a};

	if (DBUS_TYPE_UNIX_FD < 0) {
		fprintf(stderr, "File-descriptor passing not supported\n");
		exit(1);
	}

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		fprintf(stderr, "Can't get on system bus\n");
		exit(1);
	}

	request_private_network(conn, &fd, &server_ip, &peer_ip,
					&primary_dns, &secondary_dns);
	if (fd < 0)
		return -1;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	printf("Press ENTER to write data to the network.\n");
	getchar();


	if (!fork()) {
		if (write(fd, buf1, sizeof(buf1)) < 0) {
			fprintf(stderr, "err on write() buf1\n");
			return -1;
		}

		if (write(fd, buf2, sizeof(buf2)) < 0) {
			fprintf(stderr, "err on write() buf2\n");
			return -1;
		}

		printf("Press ENTER to release private network.\n");
		getchar();

		if (release_private_network(conn) < 0)
			return -1;

		close(fd);

		dbus_connection_unref(conn);

	} else {
		struct pollfd p;
		char buf[1500];
		int len;

		p.fd = fd;
		p.events = POLLIN | POLLERR | POLLHUP;

		while (1) {
			p.revents = 0;
			if (poll(&p, 1, -1) <= 0)
				return -1;

			if (p.revents & (POLLERR | POLLHUP))
				return -1;

			len = read(fd, buf, sizeof(buf));
			if (len < 0)
				return -1;

			printf("%d bytes received\n", len);
		}
	}

	return 0;
}
