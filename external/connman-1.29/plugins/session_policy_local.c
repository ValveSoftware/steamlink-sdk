/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2014  BMW Car IT GmbH.
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

#include <errno.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <glib.h>

#include <gdbus.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/log.h>
#include <connman/session.h>
#include <connman/dbus.h>
#include <connman/inotify.h>

#include "src/shared/util.h"

#define POLICYDIR STORAGEDIR "/session_policy_local"

#define MODE		(S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | \
			S_IXGRP | S_IROTH | S_IXOTH)

static DBusConnection *connection;

static GHashTable *file_hash;    /* (filename, policy_file) */
static GHashTable *session_hash; /* (connman_session, policy_config) */

/* Global lookup tables for mapping sessions to policies */
static GHashTable *selinux_hash; /* (lsm context, policy_group) */
static GHashTable *uid_hash;     /* (uid, policy_group) */
static GHashTable *gid_hash;     /* (gid, policy_group) */

/*
 * A instance of struct policy_file is created per file in
 * POLICYDIR.
 */
struct policy_file {
	/*
	 * A valid file is a keyfile with one ore more groups. All
	 * groups are keept in this list.
	 */
	GSList *groups;
};

struct policy_group {
	char *selinux;
	char *uid;
	char *gid;

	/*
	 * Each policy_group owns a config and is not shared with
	 * sessions. Instead each session copies the valued from this
	 * object.
	 */
	struct connman_session_config *config;

	/* All 'users' of this policy. */
	GSList *sessions;
};

/* A struct policy_config object is created and owned by a session. */
struct policy_config {
	char *selinux;
	char *selinux_context;
	char *uid;
	GSList *gids;

	/* The policy config owned by the session */
	struct connman_session_config *config;

	/* To which policy belongs this policy_config */
	struct connman_session *session;
	/*
	 * Points to the policy_group when a config has been applied
	 * from a file.
	 */
	struct policy_group *group;
};

static void copy_session_config(struct connman_session_config *dst,
			struct connman_session_config *src)
{
	g_slist_free(dst->allowed_bearers);
	dst->allowed_bearers = g_slist_copy(src->allowed_bearers);
	dst->ecall = src->ecall;
	dst->type = src->type;
	dst->roaming_policy = src->roaming_policy;
	dst->priority = src->priority;
}

static void set_policy(struct policy_config *policy,
			struct policy_group *group)
{
	DBG("policy %p group %p", policy, group);

	group->sessions = g_slist_prepend(group->sessions, policy);
	policy->group = group;

	copy_session_config(policy->config, group->config);
}

static char *parse_selinux_type(const char *context)
{
	char *ident, **tokens;

	/*
	 * SELinux combines Role-Based Access Control (RBAC), Type
	 * Enforcment (TE) and optionally Multi-Level Security (MLS).
	 *
	 * When SELinux is enabled all processes and files are labeled
	 * with a contex that contains information such as user, role
	 * type (and optionally a level). E.g.
	 *
	 * $ ls -Z
	 * -rwxrwxr-x. wagi wagi unconfined_u:object_r:haifux_exec_t:s0 session_ui.py
	 *
	 * For identifyng application we (ab)using the type
	 * information. In the above example the haifux_exec_t type
	 * will be transfered to haifux_t as defined in the domain
	 * transition and thus we are able to identify the application
	 * as haifux_t.
	 */

	tokens = g_strsplit(context, ":", 0);
	if (g_strv_length(tokens) < 2) {
		g_strfreev(tokens);
		return NULL;
	}

	/* Use the SELinux type as identification token. */
	ident = g_strdup(tokens[2]);

	g_strfreev(tokens);

	return ident;
}

static void cleanup_config(gpointer user_data);

static void finish_create(struct policy_config *policy,
				connman_session_config_func_t cb,
				void *user_data)
{
	struct policy_group *group = NULL;
	GSList *list;

	if (policy->selinux)
		group = g_hash_table_lookup(selinux_hash, policy->selinux);

	if (group) {
		set_policy(policy, group);

		policy->config->id_type = CONNMAN_SESSION_ID_TYPE_LSM;
		policy->config->id = g_strdup(policy->selinux_context);
		goto done;
	}

	if (policy->uid)
		group = g_hash_table_lookup(uid_hash, policy->uid);

	if (group) {
		set_policy(policy, group);

		policy->config->id_type = CONNMAN_SESSION_ID_TYPE_UID;
		policy->config->id = g_strdup(policy->uid);
		goto done;
	}

	for (list = policy->gids; list; list = list->next) {
		char *gid = list->data;

		group = g_hash_table_lookup(gid_hash, gid);
		if (!group)
			continue;

		set_policy(policy, group);

		policy->config->id_type = CONNMAN_SESSION_ID_TYPE_GID;
		policy->config->id = g_strdup(gid);
		break;
	}
done:
	g_hash_table_replace(session_hash, policy->session, policy);

	(*cb)(policy->session, policy->config, user_data, 0);
}

static void failed_create(struct policy_config *policy,
			connman_session_config_func_t cb,
			void *user_data, int err)
{
	(*cb)(policy->session, NULL, user_data, err);

	cleanup_config(policy);
}

static void selinux_context_reply(const unsigned char *context, void *user_data,
					int err)
{
	struct cb_data *cbd = user_data;
	connman_session_config_func_t cb = cbd->cb;
	struct policy_config *policy = cbd->data;
	char *ident = NULL;

	DBG("session %p", policy->session);

	if (err == -EIO) {
		/* No SELinux support, drop back to UID/GID only mode */
		finish_create(policy, cb, cbd->user_data);
		goto done;
	}

	if (err < 0) {
		failed_create(policy, cb, cbd->user_data, err);
		goto done;
	}

	DBG("SELinux context %s", context);

	policy->selinux_context = g_strdup((const char *)context);
	ident = parse_selinux_type(policy->selinux_context);
	if (ident)
		policy->selinux = g_strdup(ident);

	finish_create(policy, cb, cbd->user_data);

done:
	g_free(cbd);
	g_free(ident);
}

static void get_uid_reply(unsigned int uid, void *user_data, int err)
{
	struct cb_data *cbd = user_data;
	connman_session_config_func_t cb = cbd->cb;
	struct policy_config *policy = cbd->data;
	const char *owner;
	struct passwd *pwd;
	struct group *grp;
	gid_t *groups = NULL;
	int nrgroups, i;

	DBG("session %p uid %d", policy->session, uid);

	if (err < 0) {
		cleanup_config(policy);
		goto err;
	}

	pwd = getpwuid((uid_t)uid);
	if (!pwd) {
		if (errno != 0)
			err = -errno;
		else
			err = -EINVAL;
		goto err;
	}

	policy->uid = g_strdup(pwd->pw_name);

	nrgroups = 0;
	getgrouplist(pwd->pw_name, pwd->pw_gid, NULL, &nrgroups);
	groups = g_try_new0(gid_t, nrgroups);
	if (!groups) {
		err = -ENOMEM;
		goto err;
	}

	err = getgrouplist(pwd->pw_name, pwd->pw_gid, groups, &nrgroups);
	if (err < 0)
		goto err;

	for (i = 0; i < nrgroups; i++) {
		grp = getgrgid(groups[i]);
		if (!grp) {
			if (errno != 0)
				err = -errno;
			else
				err = -EINVAL;
			goto err;
		}

		policy->gids = g_slist_prepend(policy->gids,
					g_strdup(grp->gr_name));
	}
	g_free(groups);

	owner = connman_session_get_owner(policy->session);

	err = connman_dbus_get_selinux_context(connection, owner,
						selinux_context_reply, cbd);
	if (err == 0) {
		/*
		 * We are able to ask for a SELinux context. Let's defer the
		 * creation of the session config until we get the answer
		 * from D-Bus.
		 */
		return;
	}

	finish_create(policy, cb, cbd->user_data);
	g_free(cbd);

	return;

err:
	failed_create(NULL, cb, cbd->user_data, err);
	g_free(cbd);
	g_free(groups);
}

static int policy_local_create(struct connman_session *session,
				connman_session_config_func_t cb,
				void *user_data)
{
	struct cb_data *cbd = cb_data_new(cb, user_data);
	struct policy_config *policy;
	const char *owner;
	int err;

	DBG("session %p", session);

	policy = g_new0(struct policy_config, 1);
	policy->config = connman_session_create_default_config();
	policy->session = session;

	cbd->data = policy;

	owner = connman_session_get_owner(session);

	err = connman_dbus_get_connection_unix_user(connection, owner,
						get_uid_reply, cbd);
	if (err < 0) {
		connman_error("Could not get UID");
		cleanup_config(policy);
		g_free(cbd);
		return err;
	}

	return 0;
}

static void policy_local_destroy(struct connman_session *session)
{
	struct policy_data *policy;

	DBG("session %p", session);

	policy = g_hash_table_lookup(session_hash, session);
	if (!policy)
		return;

	g_hash_table_remove(session_hash, session);
}

static struct connman_session_policy session_policy_local = {
	.name = "session local policy configuration",
	.priority = CONNMAN_SESSION_POLICY_PRIORITY_DEFAULT,
	.create = policy_local_create,
	.destroy = policy_local_destroy,
};

static int load_keyfile(const char *pathname, GKeyFile **keyfile)
{
	GError *error = NULL;
	int err;

	*keyfile = g_key_file_new();

	if (!g_key_file_load_from_file(*keyfile, pathname, 0, &error))
		goto err;

	return 0;

err:
	/*
	 * The fancy G_FILE_ERROR_* codes are identical to the native
	 * error codes.
	 */
	err = -error->code;

	DBG("Unable to load %s: %s", pathname, error->message);
	g_clear_error(&error);

	g_key_file_free(*keyfile);
	*keyfile = NULL;

	return err;
}

static int load_policy(GKeyFile *keyfile, const char *groupname,
			struct policy_group *group)
{
	struct connman_session_config *config = group->config;
	char *str, **tokens;
	int i, err = 0;

	group->selinux = g_key_file_get_string(keyfile, groupname,
						"selinux", NULL);

	group->gid = g_key_file_get_string(keyfile, groupname,
						"gid", NULL);

	group->uid = g_key_file_get_string(keyfile, groupname,
						"uid", NULL);

	if (!group->selinux && !group->gid && !group->uid)
		return -EINVAL;

	config->priority = g_key_file_get_boolean(keyfile, groupname,
						"Priority", NULL);

	str = g_key_file_get_string(keyfile, groupname, "RoamingPolicy",
				NULL);
	if (str) {
		config->roaming_policy = connman_session_parse_roaming_policy(str);
		g_free(str);
	}

	str = g_key_file_get_string(keyfile, groupname, "ConnectionType",
				NULL);
	if (str) {
		config->type = connman_session_parse_connection_type(str);
		g_free(str);
	}

	config->ecall = g_key_file_get_boolean(keyfile, groupname,
						"EmergencyCall", NULL);

	str = g_key_file_get_string(keyfile, groupname, "AllowedBearers",
				NULL);
	if (str) {
		tokens = g_strsplit(str, " ", 0);

		for (i = 0; tokens[i]; i++) {
			err = connman_session_parse_bearers(tokens[i],
					&config->allowed_bearers);
			if (err < 0)
				break;
		}

		g_free(str);
		g_strfreev(tokens);
	}

	DBG("group %p selinux %s uid %s gid %s", group, group->selinux,
		group->uid, group->gid);

	return err;
}

static void update_session(struct policy_config *policy)
{
	DBG("policy %p session %p", policy, policy->session);

	if (!policy->session)
		return;

	if (connman_session_config_update(policy->session) < 0)
		connman_session_destroy(policy->session);
}

static void set_default_config(gpointer user_data)
{
	struct policy_config *policy = user_data;

	connman_session_set_default_config(policy->config);
	policy->group = NULL;
	update_session(policy);
}

static void cleanup_config(gpointer user_data)
{
	struct policy_config *policy = user_data;

	DBG("policy %p group %p", policy, policy->group);

	if (policy->group)
		policy->group->sessions =
			g_slist_remove(policy->group->sessions, policy);

	g_slist_free(policy->config->allowed_bearers);
	g_free(policy->config->id);
	g_free(policy->config);
	g_free(policy->selinux_context);
	g_free(policy->selinux);
	g_free(policy->uid);
	g_slist_free_full(policy->gids, g_free);
	g_free(policy);
}

static void cleanup_group(gpointer user_data)
{
	struct policy_group *group = user_data;

	DBG("group %p", group);

	g_slist_free_full(group->sessions, set_default_config);

	g_slist_free(group->config->allowed_bearers);
	g_free(group->config->id);
	g_free(group->config);
	if (group->selinux)
		g_hash_table_remove(selinux_hash, group->selinux);
	if (group->uid)
		g_hash_table_remove(uid_hash, group->uid);
	if (group->gid)
		g_hash_table_remove(gid_hash, group->gid);
	g_free(group->selinux);
	g_free(group->uid);
	g_free(group->gid);
	g_free(group);
}

static void cleanup_file(gpointer user_data)
{
	struct policy_file *file = user_data;

	DBG("file %p", file);

	g_slist_free_full(file->groups, cleanup_group);
	g_free(file);
}

static void recheck_sessions(void)
{
	GHashTableIter iter;
	gpointer value, key;
	struct policy_group *group = NULL;
	GSList *list;

	g_hash_table_iter_init(&iter, session_hash);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct policy_config *policy = value;

		if (policy->group)
			continue;

		if (policy->selinux)
			group = g_hash_table_lookup(selinux_hash,
							policy->selinux);
		if (group) {
			policy->config->id_type = CONNMAN_SESSION_ID_TYPE_LSM;
			g_free(policy->config->id);
			policy->config->id = g_strdup(policy->selinux_context);
			update_session(policy);
			continue;
		}

		group = g_hash_table_lookup(uid_hash, policy->uid);
		if (group) {
			set_policy(policy, group);

			policy->config->id_type = CONNMAN_SESSION_ID_TYPE_UID;
			g_free(policy->config->id);
			policy->config->id = g_strdup(policy->uid);
			update_session(policy);
			continue;
		}

		for (list = policy->gids; list; list = list->next) {
			char *gid = list->data;
			group = g_hash_table_lookup(gid_hash, gid);
			if (group) {
				set_policy(policy, group);

				policy->config->id_type = CONNMAN_SESSION_ID_TYPE_GID;
				g_free(policy->config->id);
				policy->config->id = g_strdup(gid);
				update_session(policy);
			}
		}
	}
}

static int load_file(const char *filename, struct policy_file *file)
{
	GKeyFile *keyfile;
	struct policy_group *group;
	char **groupnames;
	char *pathname;
	int err = 0, i;

	DBG("%s", filename);

	pathname = g_strdup_printf("%s/%s", POLICYDIR, filename);
	err = load_keyfile(pathname, &keyfile);
	g_free(pathname);

	if (err < 0)
		return err;

	groupnames = g_key_file_get_groups(keyfile, NULL);

	for (i = 0; groupnames[i]; i++) {
		group = g_new0(struct policy_group, 1);
		group->config = g_new0(struct connman_session_config, 1);

		err = load_policy(keyfile, groupnames[i], group);
		if (err < 0) {
			g_free(group->config);
			g_free(group);
			break;
		}
		if (group->selinux)
			g_hash_table_replace(selinux_hash, group->selinux, group);

		if (group->uid)
			g_hash_table_replace(uid_hash, group->uid, group);

		if (group->gid)
			g_hash_table_replace(gid_hash, group->gid, group);

		file->groups = g_slist_prepend(file->groups, group);
	}

	g_strfreev(groupnames);

	if (err < 0)
		g_slist_free_full(file->groups, cleanup_group);

	g_key_file_free(keyfile);

	return err;
}

static bool is_filename_valid(const char *filename)
{
	if (!filename)
		return false;

	if (filename[0] == '.')
		return false;

	return g_str_has_suffix(filename, ".policy");
}

static int read_policies(void)
{
	GDir *dir;
	const gchar *filename;
	struct policy_file *file;

	DBG("");

	dir = g_dir_open(POLICYDIR, 0, NULL);
	if (!dir)
		return -EINVAL;

	while ((filename = g_dir_read_name(dir))) {
		if (!is_filename_valid(filename))
			continue;

		file = g_new0(struct policy_file, 1);
		if (load_file(filename, file) < 0) {
			g_free(file);
			continue;
		}

		g_hash_table_replace(file_hash, g_strdup(filename), file);
	}

	g_dir_close(dir);

	return 0;
}


static void notify_handler(struct inotify_event *event,
                                        const char *filename)
{
	struct policy_file *file;

	DBG("event %x file %s", event->mask, filename);

	if (event->mask & IN_CREATE)
		return;

	if (!is_filename_valid(filename))
		return;

	/*
	 * load_file() will modify the global selinux/uid/gid hash
	 * tables. We need to remove the old entries first before
	 * else the table points to the wrong entries.
	 */
	g_hash_table_remove(file_hash, filename);

	if (event->mask & (IN_DELETE | IN_MOVED_FROM))
		return;

	if (event->mask & (IN_MOVED_TO | IN_MODIFY)) {
		connman_info("Policy update for '%s'", filename);

		file = g_new0(struct policy_file, 1);
		if (load_file(filename, file) < 0) {
			g_free(file);
			return;
		}

		g_hash_table_replace(file_hash, g_strdup(filename), file);
		recheck_sessions();
	}
}

static int session_policy_local_init(void)
{
	int err;

	DBG("");

	/* If the dir doesn't exist, create it */
	if (!g_file_test(POLICYDIR, G_FILE_TEST_IS_DIR)) {
		if (mkdir(POLICYDIR, MODE) < 0) {
			if (errno != EEXIST)
				return -errno;
		}
	}

	connection = connman_dbus_get_connection();
	if (!connection)
		return -EIO;

	file_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					g_free, cleanup_file);
	session_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
						NULL, cleanup_config);
	selinux_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					NULL, NULL);
	uid_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					NULL, NULL);
	gid_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					NULL, NULL);

	err = connman_inotify_register(POLICYDIR, notify_handler);
	if (err < 0)
		goto err;

	err = connman_session_policy_register(&session_policy_local);
	if (err < 0)
		goto err_notify;

	read_policies();

	return 0;

err_notify:

	connman_inotify_unregister(POLICYDIR, notify_handler);

err:
	if (file_hash)
		g_hash_table_destroy(file_hash);

	if (session_hash)
		g_hash_table_destroy(session_hash);

	if (selinux_hash)
		g_hash_table_destroy(selinux_hash);

	if (uid_hash)
		g_hash_table_destroy(uid_hash);

	if (gid_hash)
		g_hash_table_destroy(gid_hash);

	connman_session_policy_unregister(&session_policy_local);

	dbus_connection_unref(connection);

	return err;
}

static void session_policy_local_exit(void)
{
	DBG("");

	g_hash_table_destroy(file_hash);
	g_hash_table_destroy(session_hash);
	g_hash_table_destroy(selinux_hash);
	g_hash_table_destroy(uid_hash);
	g_hash_table_destroy(gid_hash);

	connman_session_policy_unregister(&session_policy_local);

	dbus_connection_unref(connection);

	connman_inotify_unregister(POLICYDIR, notify_handler);
}

CONNMAN_PLUGIN_DEFINE(session_policy_local,
		"Session local file policy configuration plugin",
		VERSION, CONNMAN_PLUGIN_PRIORITY_DEFAULT,
		session_policy_local_init, session_policy_local_exit)
