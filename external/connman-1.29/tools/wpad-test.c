/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <gweb/gresolv.h>

static GTimer *timer;

static GMainLoop *main_loop;

static GResolv *resolv;

static void resolv_debug(const char *str, void *data)
{
	g_print("%s: %s\n", (const char *) data, str);
}

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static void resolv_result(GResolvResultStatus status,
					char **results, gpointer user_data)
{
	char *hostname = user_data;
	char *str, *ptr;
	gdouble elapsed;
	int i;

	if (status == G_RESOLV_RESULT_STATUS_SUCCESS) {
		g_print("Found %s\n", hostname);
		goto done;
	}

	g_print("No result for %s\n", hostname);

	ptr = strchr(hostname + 5, '.');
	if (!ptr || strlen(ptr) < 2) {
		g_print("No more names\n");
		goto done;
	}

	if (!strchr(ptr + 1, '.')) {
		g_print("Not found\n");
		goto done;
	}

	str = g_strdup_printf("wpad.%s", ptr + 1);

	g_resolv_lookup_hostname(resolv, str, resolv_result, str);

	g_free(hostname);

	return;

done:
	elapsed = g_timer_elapsed(timer, NULL);

	g_print("elapse: %f seconds\n", elapsed);

	if (results) {
		for (i = 0; results[i]; i++)
			g_print("result: %s\n", results[i]);
	}

	g_free(hostname);

	g_main_loop_quit(main_loop);
}

static void start_wpad(const char *search)
{
	char domainname[256];
	char *hostname;

	if (!search) {
		if (getdomainname(domainname, sizeof(domainname)) < 0) {
			g_printerr("Failed to get domain name\n");
			goto quit;
		}
	} else
		snprintf(domainname, sizeof(domainname), "%s", search);

	if (strlen(domainname) == 0) {
		g_printerr("Domain name is not set\n");
		goto quit;
	}

	g_print("domainname: %s\n", domainname);

	hostname = g_strdup_printf("wpad.%s", domainname);

	g_resolv_lookup_hostname(resolv, hostname, resolv_result, hostname);

	return;

quit:
	g_main_loop_quit(main_loop);
}

static bool option_debug = false;
static gchar *option_search = NULL;

static GOptionEntry options[] = {
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug,
					"Enable debug output" },
	{ "search", 's', 0, G_OPTION_ARG_STRING, &option_search,
					"Specify search domain name" },
	{ NULL },
};

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	struct sigaction sa;
	int index = 0;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");
		exit(1);
	}

	g_option_context_free(context);

	resolv = g_resolv_new(index);
	if (!resolv) {
		g_printerr("Failed to create resolver\n");
		return 1;
	}

	if (option_debug)
		g_resolv_set_debug(resolv, resolv_debug, "RESOLV");

	main_loop = g_main_loop_new(NULL, FALSE);

	if (argc > 1) {
		int i;

		for (i = 1; i < argc; i++)
			g_resolv_add_nameserver(resolv, argv[i], 53, 0);
	} else
		g_resolv_add_nameserver(resolv, "127.0.0.1", 53, 0);

	timer = g_timer_new();

	start_wpad(option_search);

	g_free(option_search);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_main_loop_run(main_loop);

	g_timer_destroy(timer);

	g_resolv_unref(resolv);

	g_main_loop_unref(main_loop);

	return 0;
}
