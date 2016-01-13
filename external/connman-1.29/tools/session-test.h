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

#include <glib.h>

#include <connman/dbus.h>

#include "../src/connman.h"

struct test_session;
struct test_fix;

struct test_manager {
	char *state;
};

typedef void (* util_test_func_t) (struct test_fix *fix);

struct test_fix {
	gpointer user_data;

	GMainLoop *main_loop;
	DBusConnection *main_connection;
	unsigned int watch;
	unsigned int manager_watch;

	struct test_manager manager;
	util_test_func_t manager_changed;

	/* session test cases */
	unsigned int max_sessions;
	struct test_session *session;
};

void util_quit_loop(struct test_fix *fix);
void util_idle_call(struct test_fix *fix, util_test_func_t func,
			util_test_func_t destroy);
void util_call(struct test_fix *fix, util_test_func_t func,
		util_test_func_t destroy);
void util_test_add(const char *test_name, util_test_func_t test,
			util_test_func_t setup,
			util_test_func_t teardown);
void util_setup(struct test_fix *fix);
void util_teardown(struct test_fix *fix);

void util_session_create(struct test_fix *fix, unsigned int max_sessions);
void util_session_destroy(struct test_fix *fix);
void util_session_init(struct test_session *session);
void util_session_cleanup(struct test_session *session);

typedef void (* notify_func_t) (struct test_session *session);

enum connman_session_state {
	CONNMAN_SESSION_STATE_DISCONNECTED   = 0,
	CONNMAN_SESSION_STATE_CONNECTED      = 1,
	CONNMAN_SESSION_STATE_ONLINE         = 2,
};

struct test_session_info {
	enum connman_session_state state;
	char *name;
	char *bearer;
	char *interface;
	enum connman_session_type type;
	/* ipv4, ipv6 dicts */
	GSList *allowed_bearers;
};

struct test_session {
	void *user_data;

	struct test_fix *fix;
	DBusConnection *connection;

	char *session_path;
	char *notify_path;
	notify_func_t notify;

	struct test_session_info *info;
};

struct test_bearer_info {
	char *name;
};

/* session-api.c */
void bearer_info_cleanup(gpointer bearer_info, gpointer user_data);

void session_append_settings(DBusMessageIter *dict,
				struct test_session_info *info);
int session_notify_register(struct test_session *session,
				const char *notify_path);
int session_notify_unregister(struct test_session *session,
				const char *notify_path);

DBusMessage *session_connect(DBusConnection *connection,
				struct test_session *session);
DBusMessage *session_disconnect(DBusConnection *connection,
					struct test_session *session);

/* manager-api.c */
DBusMessage *manager_get_services(DBusConnection *connection);
DBusMessage *manager_get_properties(DBusConnection *connection);
DBusMessage *manager_create_session(DBusConnection *connection,
					struct test_session_info *info,
					const char *notifier_path);
DBusMessage *manager_destroy_session(DBusConnection *connection,
					const char *notifier_path);
DBusMessage *manager_set_session_mode(DBusConnection *connection,
					bool enable);
int manager_parse_properties(DBusMessage *msg,
				struct test_manager *manager);

/* #define DEBUG */
#ifdef DEBUG
#include <stdio.h>

#define LOG(fmt, arg...) do { \
	fprintf(stdout, "%s:%s() " fmt "\n", \
			__FILE__, __FUNCTION__ , ## arg); \
} while (0)
#else
#define LOG(fmt, arg...)
#endif
