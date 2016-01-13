/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2011  BMW Car IT GmbH.
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
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include <gdbus.h>

#include "session-test.h"

#define POLICYDIR STORAGEDIR "/session_policy_local"

enum test_session_state {
	TEST_SESSION_STATE_0 = 0,
	TEST_SESSION_STATE_1 = 1,
	TEST_SESSION_STATE_2 = 2,
	TEST_SESSION_STATE_3 = 3,
};

static enum test_session_state get_session_state(struct test_session *session)
{
	return GPOINTER_TO_UINT(session->fix->user_data);
}

static void set_session_state(struct test_session *session,
				enum test_session_state state)
{
	session->fix->user_data = GUINT_TO_POINTER(state);
}

static struct test_session *get_session(struct test_session *session,
					unsigned int index)
{
	return &session->fix->session[index];
}

static void test_session_create_no_notify(struct test_fix *fix)
{
	DBusMessage *msg;

	util_session_create(fix, 1);

	msg = manager_create_session(fix->session->connection,
					fix->session->info, "/foo");
	g_assert(msg);
	g_assert(dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_ERROR);

	dbus_message_unref(msg);

	util_idle_call(fix, util_quit_loop, util_session_destroy);
}

static void test_session_destroy_no_notify(struct test_fix *fix)
{
	DBusMessage *msg;

	util_session_create(fix, 1);

	msg = manager_destroy_session(fix->session->connection, "/foo");
	g_assert(!msg);

	util_idle_call(fix, util_quit_loop, util_session_destroy);
}

static void test_session_create_notify(struct test_session *session)
{
	LOG("session %p", session);

	util_idle_call(session->fix, util_quit_loop, util_session_destroy);
}

static void test_session_create(struct test_fix *fix)
{
	struct test_session *session;
	DBusMessage *msg;
	int err;

	util_session_create(fix, 1);
	session = fix->session;

	session->notify_path = "/foo";
	session->notify = test_session_create_notify;

	err = session_notify_register(session, session->notify_path);
	g_assert(err == 0);

	msg = manager_create_session(session->connection,
					session->info,
					session->notify_path);
	g_assert(msg);
	g_assert(dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_ERROR);

	dbus_message_unref(msg);
}

static void test_session_create_destroy(struct test_fix *fix)
{
	struct test_session *session;

	util_session_create(fix, 1);
	session = fix->session;

	session->notify_path = g_strdup("/foo");

	util_session_init(fix->session);
	util_session_cleanup(fix->session);

	util_idle_call(fix, util_quit_loop, util_session_destroy);
}

static void test_session_create_dup_notification(struct test_fix *fix)
{
	struct test_session *session0, *session1;
	DBusMessage *msg;

	util_session_create(fix, 2);
	session0 = &fix->session[0];
	session1 = &fix->session[1];

	session0->notify_path = g_strdup("/foo");
	session1->notify_path = session0->notify_path;

	util_session_init(session0);

	msg = manager_create_session(session1->connection,
					session1->info,
					session1->notify_path);
	g_assert(msg);

	util_session_cleanup(session0);

	util_idle_call(fix, util_quit_loop, util_session_destroy);
}

static void test_session_create_many_notify(struct test_session *session)
{
	unsigned int nr;

	LOG("session %p", session);

	nr = GPOINTER_TO_UINT(session->fix->user_data);
	nr--;
	session->fix->user_data = GUINT_TO_POINTER(nr);

	if (nr > 0)
		return;

	util_idle_call(session->fix, util_quit_loop, util_session_destroy);
}

static void test_session_create_many(struct test_fix *fix)
{
	struct test_session *session;
	unsigned int i, max;

	max = 100;

	fix->user_data = GUINT_TO_POINTER(max);

	util_session_create(fix, max);

	for (i = 0; i < max; i++) {
		session = &fix->session[i];

		session->notify_path = g_strdup_printf("/foo/%d", i);
		session->notify = test_session_create_many_notify;

		util_session_init(session);
	}
}

static void set_session_mode(struct test_fix *fix,
					bool enable)
{
	DBusMessage *msg;

	msg = manager_set_session_mode(fix->main_connection, enable);
	g_assert(msg);
	g_assert(dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_ERROR);

	dbus_message_unref(msg);
}

static void test_session_connect_notify(struct test_session *session)
{
	LOG("session %p state %d", session, session->info->state);

	if (session->info->state == CONNMAN_SESSION_STATE_DISCONNECTED)
		return;

	util_session_cleanup(session);

	util_idle_call(session->fix, util_quit_loop, util_session_destroy);
}

static void test_session_connect(struct test_fix *fix)
{
	struct test_session *session;
	DBusMessage *msg;

	util_session_create(fix, 1);
	session = fix->session;

	session->notify_path = g_strdup("/foo");
	session->notify =  test_session_connect_notify;
	util_session_init(session);

	msg = session_connect(session->connection, session);
	g_assert(msg);
	g_assert(dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_ERROR);

	dbus_message_unref(msg);
}

static void test_session_disconnect_notify(struct test_session *session)
{
	LOG("session %p state %d", session, session->info->state);

	if (session->info->state >= CONNMAN_SESSION_STATE_CONNECTED)
		return;

	util_session_cleanup(session);

	util_idle_call(session->fix, util_quit_loop, util_session_destroy);
}

static void test_session_disconnect(struct test_fix *fix)
{
	struct test_session *session;
	DBusMessage *msg;

	util_session_create(fix, 1);
	session = fix->session;

	session->notify_path = g_strdup("/foo");
	session->notify =  test_session_disconnect_notify;
	util_session_init(session);

	msg = session_disconnect(session->connection, session);
	g_assert(msg);
	dbus_message_unref(msg);
}

static void test_session_connect_disconnect_notify(struct test_session *session)
{
	enum test_session_state state = get_session_state(session);
	enum test_session_state next_state = state;
	DBusMessage *msg;

	LOG("state %d session %p %s state %d", state, session,
		session->notify_path, session->info->state);

	switch (state) {
	case TEST_SESSION_STATE_0:
		if (session->info->state == CONNMAN_SESSION_STATE_DISCONNECTED)
			next_state = TEST_SESSION_STATE_1;
		if (session->info->state == CONNMAN_SESSION_STATE_CONNECTED) {
			LOG("state was already connected, continuing");
			next_state = TEST_SESSION_STATE_2;
		}
		break;
	case TEST_SESSION_STATE_1:
		if (session->info->state >= CONNMAN_SESSION_STATE_CONNECTED)
			next_state = TEST_SESSION_STATE_2;
		break;
	case TEST_SESSION_STATE_2:
		if (session->info->state == CONNMAN_SESSION_STATE_DISCONNECTED)
			next_state = TEST_SESSION_STATE_3;
	default:
		break;
	}

	if (state == next_state)
		return;

	set_session_state(session, next_state);

	LOG("next_state %d", next_state);

	switch (next_state) {
	case TEST_SESSION_STATE_1:
		msg = session_connect(session->connection, session);
		g_assert(msg);
		dbus_message_unref(msg);
		return;
	case TEST_SESSION_STATE_2:
		msg = session_disconnect(session->connection, session);
		g_assert(msg);
		dbus_message_unref(msg);
		return;
	case TEST_SESSION_STATE_3:
		util_session_cleanup(session);
		util_idle_call(session->fix, util_quit_loop,
				util_session_destroy);
		return;
	default:
		return;
	}
}

static void test_session_connect_disconnect(struct test_fix *fix)
{
	struct test_session *session;

	/*
	 * +-------------------+
	 * |       START       |
	 * +-------------------+
	 *   |
	 *   | connect foo
	 *   v
	 * +-------------------+
	 * |   FOO-CONNECTED   |
	 * +-------------------+
	 *  |
	 *  | disconnect foo
	 *  v
	 * +-------------------+
	 * |        END        |
	 * +-------------------+
	 */

	util_session_create(fix, 1);
	session = fix->session;

	session->notify_path = g_strdup("/foo");
	session->notify =  test_session_connect_disconnect_notify;

	util_session_init(session);

	set_session_state(session, TEST_SESSION_STATE_0);
}

static void test_session_connect_free_ride_notify(struct test_session *session)
{
	struct test_session *session0 = get_session(session, 0);
	struct test_session *session1 = get_session(session, 1);
	enum test_session_state state = get_session_state(session);
	enum test_session_state next_state = state;
	DBusMessage *msg;

	LOG("state %d session %p %s state %d", state, session,
		session->notify_path, session->info->state);

	switch (state) {
	case TEST_SESSION_STATE_0:
		if (session0->info->state == CONNMAN_SESSION_STATE_DISCONNECTED
				&& session1->info->state ==
					CONNMAN_SESSION_STATE_DISCONNECTED) {
			next_state = TEST_SESSION_STATE_1;
		}
		if (session0->info->state == CONNMAN_SESSION_STATE_CONNECTED &&
				session1->info->state ==
				CONNMAN_SESSION_STATE_CONNECTED) {
			LOG("state was already connected, continuing");
			next_state = TEST_SESSION_STATE_2;
		}

		break;
	case TEST_SESSION_STATE_1:
		if (session0->info->state >= CONNMAN_SESSION_STATE_CONNECTED &&
				session1->info->state >=
					CONNMAN_SESSION_STATE_CONNECTED) {
			next_state = TEST_SESSION_STATE_2;
		}

		break;
	case TEST_SESSION_STATE_2:
		if (session0->info->state == CONNMAN_SESSION_STATE_DISCONNECTED
				&& (session1->info->state ==
					CONNMAN_SESSION_STATE_DISCONNECTED ||
						session1->info->state ==
					CONNMAN_SESSION_STATE_CONNECTED) ) {
			LOG("session0 /foo is disconnected, session1 /bar "
				"can be either connected or disconnected");
			next_state = TEST_SESSION_STATE_3;
		}

		break;
	case TEST_SESSION_STATE_3:

		return;
	}

	if (state == next_state)
		return;

	set_session_state(session, next_state);

	LOG("next_state %d", next_state);

	switch (next_state) {
	case TEST_SESSION_STATE_0:

		return;
	case TEST_SESSION_STATE_1:
		msg = session_connect(session0->connection, session0);
		g_assert(msg);
		dbus_message_unref(msg);

		return;

	case TEST_SESSION_STATE_2:
		msg = session_disconnect(session0->connection, session0);
		g_assert(msg);
		dbus_message_unref(msg);

		return;
	case TEST_SESSION_STATE_3:
		util_session_cleanup(session0);
		util_session_cleanup(session1);

		util_idle_call(session0->fix, util_quit_loop,
				util_session_destroy);

		return;
	}
}

static void test_session_connect_free_ride(struct test_fix *fix)
{
	struct test_session *session0, *session1;

	/*
	 * +-------------------+
	 * |       START       |
	 * +-------------------+
	 *   |
	 *   | connect foo
	 *   v
	 * +-------------------+
	 * |   FOO-CONNECTED   |
	 * +-------------------+
	 *   |
	 *   | free-ride bar
	 *   v
	 * +-------------------+
	 * | FOO-BAR-CONNECTED |
	 * +-------------------+
	 *  |
	 *  | disconnect foo
	 *  v
	 * +-------------------+
	 * |        END        |
	 * +-------------------+
	 */

	util_session_create(fix, 2);
	session0 = &fix->session[0];
	session1 = &fix->session[1];

	session0->notify_path = g_strdup("/foo");
	session1->notify_path = g_strdup("/bar");
	session0->notify = test_session_connect_free_ride_notify;
	session1->notify = test_session_connect_free_ride_notify;

	util_session_init(session0);
	util_session_init(session1);

	set_session_state(session0, TEST_SESSION_STATE_0);
}

static void policy_save(GKeyFile *keyfile, char *pathname)
{
	gchar *data = NULL;
	gsize length = 0;
	GError *error = NULL;

	data = g_key_file_to_data(keyfile, &length, NULL);

	if (!g_file_set_contents(pathname, data, length, &error)) {
		DBG("Failed to store information: %s", error->message);
		g_error_free(error);
		g_assert(0);
	}

	g_free(data);
}

static void policy_allowed_bearers(const char *allowed_bearers)
{
	struct passwd *pwd;
	uid_t uid;
	char *pathname;
	GKeyFile *keyfile;

	LOG("update to '%s'", allowed_bearers);

	uid = getuid();
	pwd = getpwuid(uid);
	g_assert(pwd);

	keyfile = g_key_file_new();
	g_key_file_set_string(keyfile, "policy_foo", "uid", pwd->pw_name);
	g_key_file_set_string(keyfile, "policy_foo", "AllowedBearers",
				allowed_bearers);

	pathname = g_strdup_printf("%s/foo.policy", POLICYDIR);
	policy_save(keyfile, pathname);

	g_free(pathname);
	g_key_file_free(keyfile);
}

static void policy_remove_file(void)
{
	char *pathname;

	pathname = g_strdup_printf("%s/foo.policy", POLICYDIR);
	unlink(pathname);
	g_free(pathname);
}

static void test_session_policy_notify(struct test_session *session)
{
	enum test_session_state state = get_session_state(session);
	enum test_session_state next_state = state;
	DBusMessage *msg;

	LOG("state %d session %p %s state %d", state, session,
		session->notify_path, session->info->state);

	switch (state) {
	case TEST_SESSION_STATE_0:
		if (session->info->state == CONNMAN_SESSION_STATE_DISCONNECTED)
			next_state = TEST_SESSION_STATE_1;
		break;
	case TEST_SESSION_STATE_1:
		if (session->info->state >= CONNMAN_SESSION_STATE_CONNECTED)
			next_state = TEST_SESSION_STATE_2;
		break;
	case TEST_SESSION_STATE_2:
		if (session->info->state == CONNMAN_SESSION_STATE_DISCONNECTED)
			next_state = TEST_SESSION_STATE_3;
	default:
		break;
	}

	if (state == next_state)
		return;

	set_session_state(session, next_state);

	LOG("next_state %d", next_state);

	switch (next_state) {
	case TEST_SESSION_STATE_1:
		policy_allowed_bearers("ethernet");

		msg = session_connect(session->connection, session);
		g_assert(msg);
		dbus_message_unref(msg);
		return;
	case TEST_SESSION_STATE_2:
		policy_allowed_bearers("");
		return;
	case TEST_SESSION_STATE_3:
		policy_remove_file();
		util_session_cleanup(session);
		util_idle_call(session->fix, util_quit_loop,
				util_session_destroy);
		return;
	default:
		return;
	}
}

static void test_session_policy(struct test_fix *fix)
{
	struct test_session *session;

	/*
	 * +-------------------+
	 * |       START       |
	 * +-------------------+
	 *   |
	 *   | write policy AllowedBearers = ethernet
	 *   v
	 * +-------------------+
	 * |   FOO-CONNECTED   |
	 * +-------------------+
	 *  |
	 *  | write policy AllowedBearers =
	 *  v
	 * +-------------------+
	 * |        END        |
	 * +-------------------+
	 */

	policy_remove_file();

	util_session_create(fix, 1);
	session = fix->session;

	session->notify_path = g_strdup("/foo");
	session->notify =  test_session_policy_notify;

	util_session_init(session);

	set_session_state(session, TEST_SESSION_STATE_0);
}

static bool is_online(struct test_fix *fix)
{
	if (g_strcmp0(fix->manager.state, "online") == 0)
		return true;

	return false;
}

static void enable_session_mode(struct test_fix *fix)
{
	set_session_mode(fix, true);

	if (!is_online(fix))
		util_idle_call(fix, util_quit_loop, NULL);
}

static void manager_state_changed(struct test_fix *fix)
{
	if (!is_online(fix)) {
		fix->manager_changed = NULL;
		util_idle_call(fix, util_quit_loop, NULL);
	}
}

static void disable_session_mode(struct test_fix *fix)
{
	set_session_mode(fix, false);
}

static void setup_cb(struct test_fix *fix)
{
	fix->manager_changed = manager_state_changed;

	util_call(fix, enable_session_mode, NULL);
}

static void teardown_cb(struct test_fix *fix)
{
	util_call(fix, disable_session_mode, NULL);
	util_idle_call(fix, util_quit_loop, NULL);
}

int main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	util_test_add("/manager/session create no notify",
		test_session_create_no_notify, setup_cb, teardown_cb);
	util_test_add("/manager/session destroy no notify",
		test_session_destroy_no_notify, setup_cb, teardown_cb);
	util_test_add("/manager/session create",
		test_session_create, setup_cb, teardown_cb);
	util_test_add("/manager/session create destroy",
		test_session_create_destroy, setup_cb, teardown_cb);
	util_test_add("/manager/session create duplicate notification",
		test_session_create_dup_notification, setup_cb, teardown_cb);
	util_test_add("/manager/session create many",
		test_session_create_many, setup_cb, teardown_cb);

	util_test_add("/session/connect",
		test_session_connect, setup_cb, teardown_cb);
	util_test_add("/session/disconnect",
		test_session_disconnect, setup_cb, teardown_cb);
	util_test_add("/session/connect disconnect",
		test_session_connect_disconnect, setup_cb, teardown_cb);
	util_test_add("/session/connect free-ride",
		test_session_connect_free_ride, setup_cb, teardown_cb);

	util_test_add("/session/policy",
		test_session_policy, setup_cb, teardown_cb);

	return g_test_run();
}
