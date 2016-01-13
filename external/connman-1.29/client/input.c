/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2014  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <errno.h>
#include <glib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <gdbus.h>
#include "input.h"
#include "commands.h"

static DBusConnection *connection;
static GMainLoop *main_loop;
static bool interactive = false;

static bool save_input;
static char *saved_line;
static int saved_point;

void __connmanctl_quit(void)
{
	if (main_loop)
		g_main_loop_quit(main_loop);
}

bool __connmanctl_is_interactive(void)
{
	return interactive;
}

void __connmanctl_save_rl(void)
{
	save_input = !RL_ISSTATE(RL_STATE_DONE);

	if (save_input) {
		saved_point = rl_point;
		saved_line = rl_copy_text(0, rl_end);
		rl_save_prompt();
		rl_replace_line("", 0);
		rl_redisplay();
	}
}

void __connmanctl_redraw_rl(void)
{
	if (save_input) {
		rl_restore_prompt();
		rl_replace_line(saved_line, 0);
		rl_point = saved_point;
		rl_redisplay();
		free(saved_line);
	}

	save_input = 0;
}

static void rl_handler(char *input)
{
	char **args, **trim_args;
	int num, len, err, i;

	if (!input) {
		rl_newline(1, '\n');
		g_main_loop_quit(main_loop);
		return;
	}

	args = g_strsplit(input, " ", 0);
	num = g_strv_length(args);

	trim_args = g_new0(char *, num + 1);
	for (i = 0, len = 0; i < num; i++) {
		if (*args[i] != '\0') {
			trim_args[len] = args[i];
			len++;
		}
	}

	if (len > 0) {

		add_history(input);

		err = __connmanctl_commands(connection, trim_args, len);

		if (err > 0)
			g_main_loop_quit(main_loop);
	}

	g_strfreev(args);
	g_free(trim_args);
}

static gboolean input_handler(GIOChannel *channel, GIOCondition condition,
		gpointer user_data)
{
	if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		g_main_loop_quit(main_loop);
		return FALSE;
	}

	rl_callback_read_char();
	return TRUE;
}

static char **complete_agent(const char *text, int start, int end)
{
	rl_attempted_completion_over = 1;

	return NULL;
}

/* Return how many parameters we have typed */
int __connmanctl_input_calc_level(void)
{
	int count = 0;
	char *ptr;

	ptr = rl_line_buffer;

	while (*ptr) {
		if (*ptr == ' ') {
			if (*(ptr + 1) == ' ') {
				ptr++;
				continue;
			} else
				count++;
		}
		ptr++;
	}

	return count;
}

void __connmanctl_input_lookup_end(void)
{
	rl_attempted_completion_over = 1;
}

static char **complete_command(const char *text, int start, int end)
{
	if (start == 0) {
		return rl_completion_matches(text,
				__connmanctl_lookup_command);

	} else {
		__connmanctl_lookup_cb cb;
		char **str = NULL;

		cb = __connmanctl_get_lookup_func(rl_line_buffer);
		if (cb)
			str = rl_completion_matches(text, cb);
		else
			rl_attempted_completion_over = 1;

		return str;
	}
}

static struct {
	connmanctl_input_func_t cb;
	void *user_data;
} agent_handler;

static void rl_agent_handler(char *input)
{
	agent_handler.cb(input, agent_handler.user_data);
}

void __connmanctl_agent_mode(const char *prompt,
		connmanctl_input_func_t input_handler, void *user_data)
{
	agent_handler.cb = input_handler;
	agent_handler.user_data = user_data;

	if (input_handler)
		rl_callback_handler_install(prompt, rl_agent_handler);
	else {
		rl_set_prompt(prompt);
		rl_callback_handler_remove();
		rl_redisplay();
	}
	rl_attempted_completion_function = complete_agent;
}

void __connmanctl_command_mode(void)
{
	rl_callback_handler_install("connmanctl> ", rl_handler);
	rl_attempted_completion_function = complete_command;
}

int __connmanctl_input_init(int argc, char *argv[])
{
	char *help[] = {
		"help",
		NULL
	};
	guint source = 0;
	int err;
	DBusError dbus_err;
	GIOChannel *channel;

	dbus_error_init(&dbus_err);
	connection = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, &dbus_err);

	if (dbus_error_is_set(&dbus_err)) {
		fprintf(stderr, "Error: %s\n", dbus_err.message);
		dbus_error_free(&dbus_err);
		return 1;
	}

	if (argc < 2) {
		interactive = true;

		channel = g_io_channel_unix_new(fileno(stdin));
		source = g_io_add_watch(channel,
				G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
				input_handler, NULL);
		g_io_channel_unref(channel);

		__connmanctl_monitor_completions(connection);

		__connmanctl_command_mode();

		err = -EINPROGRESS;
	} else {
		interactive = false;

		if (strcmp(argv[1], "--help") == 0 ||
				strcmp(argv[1], "-h") == 0)
			err = __connmanctl_commands(connection, help, 1);
		else
			err = __connmanctl_commands(connection, argv + 1,
					argc - 1);
	}

	if (err == -EINPROGRESS) {
		main_loop = g_main_loop_new(NULL, FALSE);
		g_main_loop_run(main_loop);

		err = 0;
	}

	if (interactive) {
		g_source_remove(source);
		__connmanctl_monitor_completions(NULL);

		rl_callback_handler_remove();
		rl_message("");
	}

	dbus_connection_unref(connection);
	if (main_loop)
		g_main_loop_unref(main_loop);

	if (err < 0)
		err = -err;
	else
		err = 0;

	return err;
}
