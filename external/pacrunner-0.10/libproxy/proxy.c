/*
 *
 *  libproxy - A library for proxy configuration
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms and conditions of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>

#include "proxy.h"

struct _pxProxyFactory {
	DBusConnection *conn;
};

pxProxyFactory *px_proxy_factory_new(void)
{
	pxProxyFactory *factory;

	factory = malloc(sizeof(*factory));
	if (!factory)
		return NULL;

	memset(factory, 0, sizeof(*factory));

	factory->conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, NULL);
	if (!factory->conn) {
		free(factory);
		return NULL;
	}

	dbus_connection_set_exit_on_disconnect(factory->conn, FALSE);

	return factory;
}

void px_proxy_factory_free(pxProxyFactory *factory)
{
	if (!factory)
		return;

	dbus_connection_close(factory->conn);

	free(factory);
}

static char *parse_result(const char *str)
{
	const char *protocol;
	int prefix_len;
	char *result;
	int len = 0;

	if (strcasecmp(str, "DIRECT") == 0)
		return strdup("direct://");

	if (strncasecmp(str, "PROXY ", 6) == 0) {
		prefix_len = 6;
		protocol = "http";
		len = 8;
	} else if (strncasecmp(str, "SOCKS ", 6) == 0) {
		prefix_len = 6;
		protocol = "socks";
		len = 9;
	} else if (strncasecmp(str, "SOCKS4 ", 7) == 0) {
		prefix_len = 7;
		protocol = "socks4";
		len = 10;
	} else if (strncasecmp(str, "SOCKS5 ", 7) == 0) {
		prefix_len = 7;
		protocol = "socks5";
		len = 10;
	} else
		return strdup("direct://");

	len = strlen(str + prefix_len) + len;

	result = malloc(len);
	if (result)
		sprintf(result, "%s://%s", protocol, str + prefix_len);

	return result;
}

static char **append_result(char **prev_results, int *size, char *result)
{
	char **results;

	results = realloc(prev_results, sizeof(char *) * (*size + 2));
	if (!results) {
		free(result);
		return prev_results;
	}

	results[*size] = result;
	results[*size + 1] = NULL;

	*size = *size + 1;

	return results;
}

static char **extract_results(const char *str)
{
	char *copy_str, *lookup, *pos, *c, *result;
	char **results = NULL;
	int nb_results = 0;

	copy_str = strdup(str);
	if (!copy_str)
		return NULL;

	pos = copy_str;

	while (1) {
		if (!pos || *pos == '\0' || strlen(pos) < 6)
			break;

		lookup = pos;

		c = strchr(pos, ';');
		if (c) {
			for (*c = '\0', c++;
				c && *c == ' '; *c = '\0', c++);
		}

		pos = c;

		result = parse_result(lookup);
		if (result)
			results = append_result(results, &nb_results, result);
	}

	free(copy_str);

	return results;
}

char **px_proxy_factory_get_proxies(pxProxyFactory *factory, const char *url)
{
	DBusMessage *msg, *reply;
	const char *str = NULL;
	char *scheme, *host, *port, *path, **result;

	if (!factory)
		return NULL;

	if (!url)
		return NULL;

	msg = dbus_message_new_method_call("org.pacrunner",
			"/org/pacrunner/client", "org.pacrunner.Client",
							"FindProxyForURL");
	if (!msg)
		goto direct;

	scheme = strdup(url);
	if (!scheme) {
		dbus_message_unref(msg);
		goto direct;
	}

	host = strstr(scheme, "://");
	if (host) {
		*host = '\0';
		host += 3;
	} else {
		dbus_message_unref(msg);
		goto direct;
	}

	path = strchr(host, '/');
	if (path)
		*(path++) = '\0';

	port = strrchr(host, ':');
	if (port) {
		char *end;
		int tmp __attribute__ ((unused));

		tmp = strtol(port + 1, &end, 10);
		if (*end == '\0')
			*port = '\0';
	}

	dbus_message_append_args(msg, DBUS_TYPE_STRING, &url,
				DBUS_TYPE_STRING, &host, DBUS_TYPE_INVALID);
	free(scheme);

	reply = dbus_connection_send_with_reply_and_block(factory->conn,
							msg, -1, NULL);

	dbus_message_unref(msg);

	if (!reply)
		goto direct;

	dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &str,
							DBUS_TYPE_INVALID);

	if (!str || strlen(str) == 0)
		str = "DIRECT";

	result = extract_results(str);

	dbus_message_unref(reply);

	if (!result)
		goto direct;

	return result;

direct:
	result = malloc(sizeof(char *) * 2);
	if (!result)
		return NULL;

	result[0] = strdup("direct://");
	result[1] = NULL;

	return result;
}
