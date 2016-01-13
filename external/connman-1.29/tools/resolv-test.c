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
#include <string.h>
#include <signal.h>

#include <gweb/gresolv.h>

static GTimer *timer;

static GMainLoop *main_loop;

static void resolv_debug(const char *str, void *data)
{
	g_print("%s: %s\n", (const char *) data, str);
}

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static const char *status2str(GResolvResultStatus status)
{
	switch (status) {
	case G_RESOLV_RESULT_STATUS_SUCCESS:
		return "success";
	case G_RESOLV_RESULT_STATUS_ERROR:
		return "error";
	case G_RESOLV_RESULT_STATUS_NO_RESPONSE:
		return "no response";
	case G_RESOLV_RESULT_STATUS_FORMAT_ERROR:
		return "format error";
	case G_RESOLV_RESULT_STATUS_SERVER_FAILURE:
		return "server failure";
	case G_RESOLV_RESULT_STATUS_NAME_ERROR:
		return "name error";
	case G_RESOLV_RESULT_STATUS_NOT_IMPLEMENTED:
		return "not implemented";
	case G_RESOLV_RESULT_STATUS_REFUSED:
		return "refused";
	}

	return NULL;
}

static void resolv_result(GResolvResultStatus status,
					char **results, gpointer user_data)
{
	gdouble elapsed;
	int i;

	elapsed = g_timer_elapsed(timer, NULL);

	g_print("elapse: %f seconds\n", elapsed);

	g_print("status: %s\n", status2str(status));

	if (results) {
		for (i = 0; results[i]; i++)
			g_print("result: %s\n", results[i]);
	}

	g_main_loop_quit(main_loop);
}

static bool option_debug = false;

static GOptionEntry options[] = {
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug,
					"Enable debug output" },
	{ NULL },
};

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	struct sigaction sa;
	GResolv *resolv;
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

	if (argc < 2) {
		printf("missing argument\n");
		return 1;
	}

	resolv = g_resolv_new(index);
	if (!resolv) {
		printf("failed to create resolver\n");
		return 1;
	}

	if (option_debug)
		g_resolv_set_debug(resolv, resolv_debug, "RESOLV");

	main_loop = g_main_loop_new(NULL, FALSE);

	if (argc > 2) {
		int i;

		for (i = 2; i < argc; i++)
			g_resolv_add_nameserver(resolv, argv[i], 53, 0);
	}

	timer = g_timer_new();

	if (g_resolv_lookup_hostname(resolv, argv[1],
					resolv_result, NULL) == 0) {
		printf("failed to start lookup\n");
		return 1;
	}

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
