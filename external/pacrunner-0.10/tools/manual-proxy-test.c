/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2011  Intel Corporation. All rights reserved.
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

#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>

#define DBG(fmt, arg...) do { \
	printf("%s() " fmt "\n", __func__ , ## arg); \
} while (0)

enum pacrunner_manual_exclude_appliance {
	PACRUNNER_MANUAL_EXCLUDE_POST = 0,
	PACRUNNER_MANUAL_EXCLUDE_PRE  = 1,
	PACRUNNER_MANUAL_EXCLUDE_ANY  = 2,
};

enum pacrunner_manual_protocol {
	PACRUNNER_PROTOCOL_ALL            = 0,
	PACRUNNER_PROTOCOL_HTTP           = 1,
	PACRUNNER_PROTOCOL_HTTPS          = 2,
	PACRUNNER_PROTOCOL_FTP            = 3,
	PACRUNNER_PROTOCOL_SOCKS          = 4,
	PACRUNNER_PROTOCOL_SOCKS4         = 5,
	PACRUNNER_PROTOCOL_SOCKS5         = 6,
	PACRUNNER_PROTOCOL_MAXIMUM_NUMBER = 7,
	PACRUNNER_PROTOCOL_UNKNOWN        = 8,
};

struct pacrunner_manual_exclude {
	enum pacrunner_manual_exclude_appliance appliance;
	int host_length;
	char *host;
};

static enum pacrunner_manual_protocol get_protocol_from_string(const char *protocol)
{
	if (!protocol)
		return PACRUNNER_PROTOCOL_ALL;

	if (g_strcmp0(protocol, "http") == 0)
		return PACRUNNER_PROTOCOL_HTTP;
	if (g_strcmp0(protocol, "https") == 0)
		return PACRUNNER_PROTOCOL_HTTPS;
	if (g_strcmp0(protocol, "ftp") == 0)
		return PACRUNNER_PROTOCOL_FTP;
	if (g_strcmp0(protocol, "socks") == 0)
		return PACRUNNER_PROTOCOL_SOCKS;
	if (g_strcmp0(protocol, "socks4") == 0)
		return PACRUNNER_PROTOCOL_SOCKS4;
	if (g_strcmp0(protocol, "socks5") == 0)
		return PACRUNNER_PROTOCOL_SOCKS5;

	return PACRUNNER_PROTOCOL_UNKNOWN;
}

static const char *get_protocol_to_string(enum pacrunner_manual_protocol protocol)
{
	switch (protocol) {
	case PACRUNNER_PROTOCOL_ALL:
		return "ALL";
	case PACRUNNER_PROTOCOL_HTTP:
		return "HTTP";
	case PACRUNNER_PROTOCOL_HTTPS:
		return "HTTPS";
	case PACRUNNER_PROTOCOL_FTP:
		return "FTP";
	case PACRUNNER_PROTOCOL_SOCKS:
		return "SOCKS";
	case PACRUNNER_PROTOCOL_SOCKS4:
		return "SOCKS4";
	case PACRUNNER_PROTOCOL_SOCKS5:
		return "SOCKS5";
	case PACRUNNER_PROTOCOL_MAXIMUM_NUMBER:
	case PACRUNNER_PROTOCOL_UNKNOWN:
	default:
		break;
	}

	return "UNKNOWN";
}

static const char *get_appliance_to_string(enum pacrunner_manual_exclude_appliance appliance)
{
	switch (appliance) {
	case PACRUNNER_MANUAL_EXCLUDE_POST:
		return "POST";
	case PACRUNNER_MANUAL_EXCLUDE_PRE:
		return "PRE";
	case PACRUNNER_MANUAL_EXCLUDE_ANY:
		return "ANY";
	default:
		break;
	}

	return "UNKNOWN";
}

static int parse_uri(char *uri,
			char **host,
			char **protocol,
			bool no_path,
			bool exclusion)
{
	int ret = PACRUNNER_MANUAL_EXCLUDE_POST;
	bool proto, post_confirmed, ipv6;
	char *scheme, *sep, *cur;
	int length;
	long int port;

	proto = post_confirmed = ipv6 = false;
	port = -1;

	/**
	 * Make sure host and protocol, if given, are properly set.
	 */
	if (host)
		*host = NULL;

	if (protocol)
		*protocol = NULL;

	/**
	 * The parsing will actually process on a copy of given uri
	 */
	scheme = g_strdup(uri);
	if (!scheme)
		goto error;

	cur = scheme;

	/**
	 * 1 - parsing protocol first
	 * Note: protocol scheme is here totally ignored
	 */
	sep = strstr(cur, "://");
	if (sep) {
		if (sep == cur)
			goto error;

		if (protocol) {
			*sep = '\0';

			*protocol = g_strdup(cur);
			if (!*protocol)
				goto error;
		}

		cur = sep + 3;
		proto = true;
	}

	/**
	 * 2 - detecting end of uri
	 * Note: in case of server/exclusion configuration,
	 * no path should be present
	 */
	sep = strchr(cur, '/');
	if (sep) {
		if (exclusion || (*(sep + 1) != '\0' &&
							no_path))
			goto error;

		*sep = '\0';
	}

	/**
	 * 3 - We skip <login:password> if present
	 * Note: exclusion rule cannot contain such authentication information
	 */
	sep = strchr(cur, '@');
	if (sep) {
		if (exclusion)
			goto error;

		*sep = '\0';
		cur = sep + 1;
	}

	/**
	 * 4 - Are we in front of a possible IPv6 address?
	 * Note: ipv6 format is not checked!
	 */
	sep = strchr(cur, '[');
	if (sep) {
		char *bracket;

		bracket = strchr(cur, ']');
		if (!bracket)
			goto error;

		cur = sep;
		sep = strchr(bracket, ':');

		ipv6 = true;
	} else
		sep = strchr(cur, ':');

	/**
	 * 5 - Checking port validity if present
	 * Note: exclusion rule cannot embed port
	 */
	if (sep) {
		char *err = NULL;

		if (exclusion)
			goto error;

		errno = 0;
		port = strtol(sep+1, &err, 10);
		if (*err != '\0' || port <= 0 || port > USHRT_MAX ||
					errno == ERANGE || errno == EINVAL)
			goto error;

		*sep = '\0';
	}

	/**
	 * 6 - We detect/trim '.'/'*' from start
	 * Note: This is valid only for exclusion URI since it defines
	 * its rule's appliance */
	for (sep = cur; *sep != '\0' && (*sep == '*' || *sep == '.'); sep++)
		*sep = '\0';

	if (sep != cur) {
		if (!exclusion)
			goto error;

		cur = sep;
		post_confirmed = true;
	}

	/**
	 * 7 - Parsing host if present
	 */
	length = strlen(cur);
	if (length > 0) {
		const char *forbidden_chars;
		char **forbidden = NULL;

		/**
		 * We first detect/trim '.'/'*' from end
		 * Note: valid only for exclusion
		 */
		for (sep = cur + length - 1;
			*sep != '\0' && (*sep == '*' || *sep == '.'); sep--)
			*sep = '\0';

		if (sep - cur + 1 != length) {
			if (!exclusion)
				goto error;

			length = sep - cur + 1;

			ret = PACRUNNER_MANUAL_EXCLUDE_PRE;
			if (post_confirmed)
				ret = PACRUNNER_MANUAL_EXCLUDE_ANY;
		}

		if ((length > 255) || (*cur == '-' || *sep == '-') ||
				((*cur == '\0') && (!exclusion ||
				(exclusion && !proto))))
			goto error;

		/**
		 * We do not allow some characters. However we do not run
		 * a strict check if it's an IP address which is given
		 */
		if (ipv6)
			forbidden_chars = "%?!,;@\\'*|<>{}()+=$&~# \"";
		else
			forbidden_chars = "%?!,;@\\'*|<>{}[]()+=$&~# \"";

		forbidden = g_strsplit_set(cur, forbidden_chars, -1);
		if (forbidden) {
			length = g_strv_length(forbidden);
			g_strfreev(forbidden);

			if (length > 1)
				goto error;
		}

		if (host && *cur != '\0') {
			if (port > 0) {
				/**
				 * Instead of transcoding the port back
				 * to string we just get the host:port line
				 * from the original uri.
				 * */
				cur = uri + (cur - scheme);

				sep = strchr(cur, '/');
				if (sep)
					length = sep - cur;
				else
					length = strlen(cur);

				*host = g_strndup(cur, length);
			} else
				*host = g_strdup(cur);

			if (!*host)
				goto error;
		}
	} else {
		if (!exclusion ||
				(exclusion && !proto))
			goto error;
		else
			ret = PACRUNNER_MANUAL_EXCLUDE_ANY;
	}

	g_free(scheme);

	return ret;

error:
	if (protocol) {
		g_free(*protocol);
		*protocol = NULL;
	}

	g_free(scheme);

	return -EINVAL;
}

static void free_exclude(gpointer data)
{
	struct pacrunner_manual_exclude *exclude;

	exclude = (struct pacrunner_manual_exclude *) data;
	if (!exclude)
		return;

	g_free(exclude->host);
	g_free(exclude);
}

static void __pacrunner_manual_destroy_excludes(GList **excludes)
{
	int i;

	if (!excludes)
		return;

	for (i = 0; i < PACRUNNER_PROTOCOL_MAXIMUM_NUMBER; i++)
		g_list_free_full(excludes[i], free_exclude);

	g_free(excludes);
}

static GList **__pacrunner_manual_create_excludes(const char **excludes)
{
	struct pacrunner_manual_exclude *exclude;
	char *host, *protocol;
	GList **result = NULL;
	int ret, proto;
	char **uri;

	if (!excludes)
		return NULL;

	result = g_try_malloc0(PACRUNNER_PROTOCOL_MAXIMUM_NUMBER *
							sizeof(GList *));
	if (!result)
		return NULL;

	for (uri = (char **)excludes; *uri; uri++) {
		ret = parse_uri(*uri, &host, &protocol, true, true);

		if (ret < 0)
			continue;

		proto = get_protocol_from_string(protocol);
		if (proto == PACRUNNER_PROTOCOL_UNKNOWN)
			goto error;

		exclude = g_try_malloc0(sizeof(
					struct pacrunner_manual_exclude));
		if (!exclude)
			goto error;

		exclude->appliance = ret;
		exclude->host = host;

		if (host)
			exclude->host_length = strlen(host);

		result[proto] = g_list_append(result[proto], exclude);

		g_free(protocol);
		protocol = NULL;
		host = NULL;
	}

	return result;

error:
	g_free(host);
	g_free(protocol);

	__pacrunner_manual_destroy_excludes(result);

	return NULL;
}

static void __pacrunner_manual_destroy_servers(GList **servers)
{
	int i;

	if (!servers)
		return;

	for (i = 0; i < PACRUNNER_PROTOCOL_MAXIMUM_NUMBER; i++)
		g_list_free_full(servers[i], g_free);

	g_free(servers);
}

static const char *protocol_to_prefix_string(enum pacrunner_manual_protocol proto)
{
	switch (proto) {
	case PACRUNNER_PROTOCOL_ALL:
	case PACRUNNER_PROTOCOL_HTTP:
	case PACRUNNER_PROTOCOL_HTTPS:
	case PACRUNNER_PROTOCOL_FTP:
		return "PROXY";
	case PACRUNNER_PROTOCOL_SOCKS:
		return "SOCKS";
	case PACRUNNER_PROTOCOL_SOCKS4:
		return "SOCKS4";
	case PACRUNNER_PROTOCOL_SOCKS5:
		return "SOCKS5";
	case PACRUNNER_PROTOCOL_MAXIMUM_NUMBER:
	case PACRUNNER_PROTOCOL_UNKNOWN:
		break;
	};

	return "";
}

static GList *append_proxy(GList *list,
			enum pacrunner_manual_protocol proto, char *host)
{
	char *proxy;

	proxy = g_strdup_printf("%s %s",
				protocol_to_prefix_string(proto), host);
	if (!proxy)
		return list;

	return g_list_append(list, proxy);
}

static GList **__pacrunner_manual_create_servers(const char **servers)
{
	char *host, *protocol;
	GList **result;
	char **uri;
	int proto;
	int ret;

	if (!servers)
		return NULL;

	result = g_try_malloc0(PACRUNNER_PROTOCOL_MAXIMUM_NUMBER *
							sizeof(GList *));
	if (!result)
		return NULL;

	for (uri = (char **)servers; *uri; uri++) {
		ret = parse_uri(*uri, &host, &protocol, true, false);

		if (ret < 0)
			continue;

		proto = get_protocol_from_string(protocol);
		if (proto == PACRUNNER_PROTOCOL_UNKNOWN)
			goto error;

		result[proto] = append_proxy(result[proto], proto, host);

		if (proto == PACRUNNER_PROTOCOL_SOCKS) {
			result[PACRUNNER_PROTOCOL_SOCKS4] = append_proxy(
					result[PACRUNNER_PROTOCOL_SOCKS4],
					PACRUNNER_PROTOCOL_SOCKS4, host);
			result[PACRUNNER_PROTOCOL_SOCKS5] = append_proxy(
					result[PACRUNNER_PROTOCOL_SOCKS5],
					PACRUNNER_PROTOCOL_SOCKS5, host);
		}

		g_free(protocol);
		g_free(host);
	}

	return result;

error:
	g_free(host);
	g_free(protocol);

	__pacrunner_manual_destroy_servers(result);

	return NULL;
}

static bool is_exclusion_matching(GList *excludes_list,
					const char *host)
{
	struct pacrunner_manual_exclude *exclusion;
	GList *excludes = NULL;
	char *cursor;

	for (excludes = excludes_list; excludes;
					excludes = excludes->next) {
		exclusion = (struct pacrunner_manual_exclude *) excludes->data;
		if (!exclusion)
			continue;

		cursor = NULL;

		if (exclusion->host)
			cursor = strstr(host, exclusion->host);

		switch (exclusion->appliance) {
		case PACRUNNER_MANUAL_EXCLUDE_POST:
			if (!cursor)
				break;

			if ((int)strlen(cursor) < exclusion->host_length)
				break;

			if (*(cursor + exclusion->host_length) == '\0')
				return true;

			break;
		case PACRUNNER_MANUAL_EXCLUDE_PRE:
			if (cursor == host)
				return true;

			break;
		case PACRUNNER_MANUAL_EXCLUDE_ANY:
			if (exclusion->host) {
				if (cursor)
					return true;
				else
					break;
			}

			return true;
		default:
			break;
		}
	}

	return false;
}

static bool is_url_excluded(GList **excludes,
				const char *host,
				enum pacrunner_manual_protocol proto)
{
	if (!excludes)
		return false;

	if (excludes[PACRUNNER_PROTOCOL_ALL])
		if (is_exclusion_matching(excludes[PACRUNNER_PROTOCOL_ALL],
								host))
			return true;

	if (proto == PACRUNNER_PROTOCOL_UNKNOWN)
		return false;

	if (excludes[proto])
		if (is_exclusion_matching(excludes[proto], host))
			return true;

	return false;
}

static inline char *append_server(char *prev_result, const char *proxy)
{
	char *result;

	if (!prev_result)
		return g_strdup(proxy);

	result = g_strjoin("; ", prev_result, proxy, NULL);
	if (!result)
		return prev_result;

	g_free(prev_result);

	return result;
}

static inline char *append_servers_to_proxy_string(char *prev_result,
							GList *proxies)
{
	char *result = prev_result;
	GList *list, *prev;

	prev = NULL;
	for (list = proxies; list && list != prev;
						prev = list, list = list->next)
		result = append_server(result, (const char *) list->data);

	return result;
}

static char *generate_proxy_string(GList **servers,
				enum pacrunner_manual_protocol proto)
{
	enum pacrunner_manual_protocol i;
	char *result = NULL;

	/* if the protocol is known, we will prefer to set same
	 * protocol-based proxies first, if any... */
	if (proto >= PACRUNNER_PROTOCOL_HTTP &&
				proto < PACRUNNER_PROTOCOL_MAXIMUM_NUMBER) {
		if (servers[proto])
			result = append_servers_to_proxy_string(result,
							servers[proto]);

		if (proto == PACRUNNER_PROTOCOL_SOCKS) {
			if (servers[PACRUNNER_PROTOCOL_SOCKS4])
				result = append_servers_to_proxy_string(result,
					servers[PACRUNNER_PROTOCOL_SOCKS4]);
			if (servers[PACRUNNER_PROTOCOL_SOCKS5])
				result = append_servers_to_proxy_string(result,
					servers[PACRUNNER_PROTOCOL_SOCKS5]);
		}
	}

	/* And/or we add the rest in the list */
	for (i = 0; i < PACRUNNER_PROTOCOL_MAXIMUM_NUMBER; i++) {
		if (i == proto || (proto == PACRUNNER_PROTOCOL_SOCKS &&
					(i == PACRUNNER_PROTOCOL_SOCKS4 ||
					 i == PACRUNNER_PROTOCOL_SOCKS5)))
			continue;

		if (servers[i])
			result = append_servers_to_proxy_string(result,
								servers[i]);
	}

	return result;
}

static char *__pacrunner_manual_execute(const char *url,
					GList **servers,
					GList **excludes)
{
	char *protocol = NULL;
	char *result = NULL;
	char *host = NULL;
	int proto;

	if (!servers)
		return NULL;

	if (parse_uri((char *)url, &host, &protocol, false, false) < 0)
		goto direct;

	proto = get_protocol_from_string(protocol);

	if (is_url_excluded(excludes, host, proto))
		goto direct;

	result = generate_proxy_string(servers, proto);

direct:
	g_free(protocol);
	g_free(host);

	return result;
}

static void dump_proxy_exclusion(gpointer data, gpointer user_data)
{
	struct pacrunner_manual_exclude *exclusion;

	exclusion = (struct pacrunner_manual_exclude *) data;
	if (!exclusion)
		return;

	printf("\tappliance: %s\n",
			get_appliance_to_string(exclusion->appliance));
	printf("\thost: %s (%d)\n", exclusion->host, exclusion->host_length);
}

static void dump_excludes(GList **excludes)
{
	int i;

	printf("EXCLUDES:\n");

	if (!excludes) {
		printf("\tNO EXCLUDES\n");
		return;
	}

	for (i = 0; i < PACRUNNER_PROTOCOL_MAXIMUM_NUMBER; i++) {
		printf("Protocol: %s\n", get_protocol_to_string(i));

		if (!excludes[i]) {
			printf("\tNONE\n");
			continue;
		}

		g_list_foreach(excludes[i], dump_proxy_exclusion, NULL);
		printf("\n");
	}
}

static void dump_proxy_server(gpointer data, gpointer user_data)
{
	printf("\t%s\n", (char *)data);
}

static void dump_servers(GList **servers)
{
	int i;

	printf("SERVERS:\n");

	if (!servers) {
		printf("\tNO SERVERS\n");
		return;
	}

	for (i = 0; i < PACRUNNER_PROTOCOL_MAXIMUM_NUMBER; i++) {
		printf("Protocol: %s\n", get_protocol_to_string(i));

		if (!servers[i]) {
			printf("\tNONE\n");
			continue;
		}

		g_list_foreach(servers[i], dump_proxy_server, NULL);
		printf("\n");
	}
}

static void print_help(void)
{
	printf("Manual Proxy configuration test tool\n"
		"Usage:\n"
		"-e,--excludes <list>: provides an exclusion list (optional)\n"
		"-h,--help           : prints this help message\n"
		"-s,--servers  <list>: provides the proxy server(s) (mandatory)\n"
		"-t,--test     <list>: provides the URLs to test (mandatory)\n"
		"list should be made like \"elt1,elt2\" and so on\n"
		"\n"
		"Servers example:\n"
		"http://stuff.com   -> http proxy\n"
		"stuff.com          -> general proxy\n"
		"ftp://foo.org:4578 -> ftp proxy, with specific port\n"
		"\n"
		"Exclusion example:\n"
		"If a uri matches an exclusion rule, it will not be proxied\n"
		"port number is not allowed in exclusion rules\n"
		"'*' is not allowed for protocols\n"
		"stuff*foo is not allowed\n"
		"http://      -> all http\n"
		"http://*foo* -> all http containing foo\n"
		"*com         -> all host ending with com\n"
		"ftp://stuff* -> all ftp starting with stuff\n");
}

static struct option long_options[] = {
	{"excludes", 1, 0, 'e'},
	{"help",     0, 0, 'h'},
	{"servers",  1, 0, 's'},
	{"test",     1, 0, 't'},
	{NULL,       0, 0,  0 }
};

static const char *short_options = "e:hs:t:";

int main(int argc, char *argv[])
{
	char **excludes_list, **servers_list, **tests_list;
	GList **excludes = NULL;
	GList **servers = NULL;
	int opt, option_index;
	char **url;

	excludes_list = NULL;
	servers_list = NULL;
	tests_list = NULL;
	option_index = 0;

	while ((opt = getopt_long(argc, argv, short_options,
				long_options, &option_index)) != -1) {
		switch (opt) {
		case 'e':
			excludes_list = g_strsplit(optarg, ",", 0);
			if (!excludes_list)
				exit(EXIT_FAILURE);

			break;
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);

			break;
		case 's':
			servers_list = g_strsplit(optarg, ",", 0);
			if (!servers_list)
				exit(EXIT_FAILURE);

			break;
		case 't':
			tests_list = g_strsplit(optarg, ",", 0);
			if (!tests_list)
				exit(EXIT_FAILURE);

			break;
		case '?':
			break;
		default:
			printf("Unknown option\n"
				"please see --help for more informations\n");
			exit(EXIT_FAILURE);

			break;
		}
	}

	if (!servers_list || !tests_list) {
		printf("You must provide server(s) and test(s) options\n");
		exit(EXIT_FAILURE);
	}

	servers = __pacrunner_manual_create_servers((const char **)
								servers_list);
	if (!servers)
		exit(EXIT_FAILURE);

	excludes = __pacrunner_manual_create_excludes((const char **)
								excludes_list);

	dump_excludes(excludes);
	dump_servers(servers);

	g_strfreev(excludes_list);
	g_strfreev(servers_list);

	for (url = tests_list; *url; url++) {
		char *proxy;

		printf("Url: %s -> ", *url);

		proxy = __pacrunner_manual_execute(*url, servers, excludes);
		if (!proxy)
			printf("DIRECT\n");
		else
			printf("%s\n", proxy);
	}

	g_strfreev(tests_list);

	__pacrunner_manual_destroy_excludes(excludes);
	__pacrunner_manual_destroy_servers(servers);

	return 0;
}
