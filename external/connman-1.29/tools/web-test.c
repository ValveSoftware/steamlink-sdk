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
#include <string.h>
#include <signal.h>

#include <gweb/gweb.h>

static GTimer *timer;

static GMainLoop *main_loop;

static void web_debug(const char *str, void *data)
{
	g_print("%s: %s\n", (const char *) data, str);
}

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static bool web_result(GWebResult *result, gpointer user_data)
{
	const guint8 *chunk;
	gsize length;
	guint16 status;
	gdouble elapsed;

	g_web_result_get_chunk(result, &chunk, &length);

	if (length > 0) {
		printf("%s\n", (char *) chunk);
		return true;
	}

	status = g_web_result_get_status(result);

	g_print("status: %03u\n", status);

	elapsed = g_timer_elapsed(timer, NULL);

	g_print("elapse: %f seconds\n", elapsed);

	g_main_loop_quit(main_loop);

	return false;
}

static bool option_debug = false;
static gchar *option_proxy = NULL;
static gchar *option_nameserver = NULL;
static gchar *option_user_agent = NULL;
static gchar *option_http_version = NULL;

static GOptionEntry options[] = {
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug,
					"Enable debug output" },
	{ "proxy", 'p', 0, G_OPTION_ARG_STRING, &option_proxy,
					"Specify proxy", "ADDRESS" },
	{ "nameserver", 'n', 0, G_OPTION_ARG_STRING, &option_nameserver,
					"Specify nameserver", "ADDRESS" },
	{ "user-agent", 'A', 0, G_OPTION_ARG_STRING, &option_user_agent,
					"Specific user agent", "STRING" },
	{ "http-version", 'H', 0, G_OPTION_ARG_STRING, &option_http_version,
					"Specific HTTP version", "STRING" },
	{ NULL },
};

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	struct sigaction sa;
	GWeb *web;
	int index = 0;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");
		return 1;
	}

	g_option_context_free(context);

	if (argc < 2) {
		fprintf(stderr, "Missing argument\n");
		return 1;
	}

	web = g_web_new(index);
	if (!web) {
		fprintf(stderr, "Failed to create web service\n");
		return 1;
	}

	if (option_debug)
		g_web_set_debug(web, web_debug, "WEB");

	main_loop = g_main_loop_new(NULL, FALSE);

	if (option_proxy) {
		g_web_set_proxy(web, option_proxy);
		g_free(option_proxy);
	}

	if (option_nameserver) {
		g_web_add_nameserver(web, option_nameserver);
		g_free(option_nameserver);
	}

	if (option_user_agent) {
		g_web_set_user_agent(web, "%s", option_user_agent);
		g_free(option_user_agent);
	}

	if (option_http_version) {
		g_web_set_http_version(web, option_http_version);
		g_free(option_http_version);
	}

	timer = g_timer_new();

	if (g_web_request_get(web, argv[1], web_result, NULL,  NULL) == 0) {
		fprintf(stderr, "Failed to start request\n");
		return 1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_main_loop_run(main_loop);

	g_timer_destroy(timer);

	g_web_unref(web);

	g_main_loop_unref(main_loop);

	return 0;
}
