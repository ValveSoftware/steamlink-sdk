/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2013  Intel Corporation. All rights reserved.
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/vfs.h>
#include <sys/inotify.h>
#include <glib.h>

#include <connman/log.h>
#include "../src/connman.h"

#include "vpn.h"

enum what {
	REMOVE = 1,
	ADD = 2,
};

struct vpn_config_provider {
	char *provider_identifier;
	char *ident;
	char *name;
	char *type;
	char *host;
	char *domain;
	char *networks;
	GHashTable *setting_strings;

	char *config_ident; /* file prefix */
	char *config_entry; /* entry name */
};

struct vpn_config {
	char *ident;
	char *name;
	char *description;
	GHashTable *provider_table;
};

static GHashTable *config_table = NULL;

static bool cleanup = false;

/* Definition of possible strings in the .config files */
#define CONFIG_KEY_NAME                "Name"
#define CONFIG_KEY_DESC                "Description"

static const char *config_possible_keys[] = {
	CONFIG_KEY_NAME,
	CONFIG_KEY_DESC,
	NULL,
};

static void unregister_config(gpointer data)
{
	struct vpn_config *config = data;

	connman_info("Removing configuration %s", config->ident);

	g_hash_table_destroy(config->provider_table);

	g_free(config->description);
	g_free(config->name);
	g_free(config->ident);
	g_free(config);
}

static void unregister_provider(gpointer data)
{
	struct vpn_config_provider *config_provider = data;
	struct vpn_provider *provider;
	char *provider_id;

	if (cleanup)
		goto free_only;

	provider_id = config_provider->provider_identifier;

	connman_info("Removing provider configuration %s provider %s",
				config_provider->ident, provider_id);

	provider = __vpn_provider_lookup(provider_id);
	if (provider)
		__vpn_provider_delete(provider);
	else {
		if (!__connman_storage_remove_provider(provider_id))
			DBG("Could not remove all files for provider %s",
								provider_id);
	}

free_only:
	g_free(config_provider->ident);
	g_free(config_provider->type);
	g_free(config_provider->name);
	g_free(config_provider->host);
	g_free(config_provider->domain);
	g_free(config_provider->networks);
	g_hash_table_destroy(config_provider->setting_strings);
	g_free(config_provider->provider_identifier);
	g_free(config_provider->config_ident);
	g_free(config_provider->config_entry);
	g_free(config_provider);
}

static int set_string(struct vpn_config_provider *config_provider,
					const char *key, const char *value)
{
	DBG("provider %p key %s value %s", config_provider, key, value);

	if (g_str_equal(key, "Type")) {
		g_free(config_provider->type);
		config_provider->type = g_strdup(value);
	} else if (g_str_equal(key, "Name")) {
		g_free(config_provider->name);
		config_provider->name = g_strdup(value);
	} else if (g_str_equal(key, "Host")) {
		g_free(config_provider->host);
		config_provider->host = g_strdup(value);
	} else if (g_str_equal(key, "Domain")) {
		g_free(config_provider->domain);
		config_provider->domain = g_strdup(value);
	} else if (g_str_equal(key, "Networks")) {
		g_free(config_provider->networks);
		config_provider->networks = g_strdup(value);
	}

	g_hash_table_replace(config_provider->setting_strings,
					g_strdup(key), g_strdup(value));
	return 0;
}

static const char *get_string(struct vpn_config_provider *config_provider,
							const char *key)
{
	DBG("provider %p key %s", config_provider, key);

	if (g_str_equal(key, "Type"))
		return config_provider->type;
	else if (g_str_equal(key, "Name"))
		return config_provider->name;
	else if (g_str_equal(key, "Host"))
		return config_provider->host;
	else if (g_str_equal(key, "Domain"))
		return config_provider->domain;
	else if (g_str_equal(key, "Networks"))
		return config_provider->networks;

	return g_hash_table_lookup(config_provider->setting_strings, key);
}

static void add_keys(struct vpn_config_provider *config_provider,
			GKeyFile *keyfile, const char *group)
{
	char **avail_keys;
	gsize nb_avail_keys, i;

	avail_keys = g_key_file_get_keys(keyfile, group, &nb_avail_keys, NULL);
	if (!avail_keys)
		return;

	for (i = 0 ; i < nb_avail_keys; i++) {
		char *value = g_key_file_get_value(keyfile, group,
						avail_keys[i], NULL);
		if (!value) {
			connman_warn("Cannot find value for %s",
							avail_keys[i]);
			continue;
		}

		set_string(config_provider, avail_keys[i], value);
		g_free(value);
	}

	g_strfreev(avail_keys);
}

static int load_provider(GKeyFile *keyfile, const char *group,
				struct vpn_config *config, enum what action)
{
	struct vpn_config_provider *config_provider;
	const char *ident, *host, *domain;
	int err;

	/* Strip off "provider_" prefix */
	ident = group + 9;

	if (strlen(ident) < 1)
		return -EINVAL;

	config_provider = g_hash_table_lookup(config->provider_table, ident);
	if (config_provider)
		return -EALREADY;

	config_provider = g_try_new0(struct vpn_config_provider, 1);
	if (!config_provider)
		return -ENOMEM;

	config_provider->ident = g_strdup(ident);

	config_provider->setting_strings = g_hash_table_new_full(g_str_hash,
						g_str_equal, g_free, g_free);

	add_keys(config_provider, keyfile, group);

	host = get_string(config_provider, "Host");
	domain = get_string(config_provider, "Domain");
	if (host && domain) {
		char *id = __vpn_provider_create_identifier(host, domain);

		struct vpn_provider *provider;
		provider = __vpn_provider_lookup(id);
		if (provider) {
			if (action == REMOVE) {
				__vpn_provider_delete(provider);
				err = 0;
			} else {
				connman_warn("Provider configuration %s "
						"already exist", id);
				err = -EALREADY;
			}

			g_free(id);
			goto err;
		}

		config_provider->provider_identifier = id;

		DBG("provider identifier %s", id);
	} else {
		DBG("invalid values host %s domain %s", host, domain);
		err = -EINVAL;
		goto err;
	}

	config_provider->config_ident = g_strdup(config->ident);
	config_provider->config_entry = g_strdup_printf("provider_%s",
						config_provider->ident);

	g_hash_table_insert(config->provider_table,
				config_provider->ident,	config_provider);

	err = __vpn_provider_create_from_config(
					config_provider->setting_strings,
					config_provider->config_ident,
					config_provider->config_entry);
	if (err != 0) {
		DBG("Cannot create provider from config file (%d/%s)",
			-err, strerror(-err));
		goto err;
	}

	connman_info("Added provider configuration %s",
						config_provider->ident);
	return 0;

err:
	g_free(config_provider->ident);
	g_free(config_provider->type);
	g_free(config_provider->name);
	g_free(config_provider->host);
	g_free(config_provider->domain);
	g_free(config_provider->networks);
	g_hash_table_destroy(config_provider->setting_strings);
	g_free(config_provider);

	return err;
}

static void check_keys(GKeyFile *keyfile, const char *group,
			const char **possible_keys)
{
	char **avail_keys;
	gsize nb_avail_keys, i, j;

	avail_keys = g_key_file_get_keys(keyfile, group, &nb_avail_keys, NULL);
	if (!avail_keys)
		return;

	for (i = 0 ; i < nb_avail_keys; i++) {
		for (j = 0; possible_keys[j] ; j++)
			if (g_strcmp0(avail_keys[i], possible_keys[j]) == 0)
				break;

		if (!possible_keys[j])
			connman_warn("Unknown configuration key %s in [%s]",
					avail_keys[i], group);
	}

	g_strfreev(avail_keys);
}

static int load_config(struct vpn_config *config, char *path, enum what action)
{
	GKeyFile *keyfile;
	gsize length;
	char **groups;
	char *str;
	bool found = false;
	int i;

	DBG("config %p", config);

	keyfile = __connman_storage_load_provider_config(config->ident);
	if (!keyfile)
		return -EIO;

	/* Verify keys validity of the global section */
	check_keys(keyfile, "global", config_possible_keys);

	str = __vpn_config_get_string(keyfile, "global", CONFIG_KEY_NAME, NULL);
	if (str) {
		g_free(config->name);
		config->name = str;
	}

	str = __vpn_config_get_string(keyfile, "global", CONFIG_KEY_DESC, NULL);
	if (str) {
		g_free(config->description);
		config->description = str;
	}

	groups = g_key_file_get_groups(keyfile, &length);

	for (i = 0; groups[i]; i++) {
		if (g_str_has_prefix(groups[i], "provider_")) {
			int ret = load_provider(keyfile, groups[i], config,
						action);
			if (ret == 0 || ret == -EALREADY)
				found = true;
		}
	}

	if (!found)
		connman_warn("Config file %s/%s.config does not contain any "
			"configuration that can be provisioned!",
			path, config->ident);

	g_strfreev(groups);

	g_key_file_free(keyfile);

	return 0;
}

static struct vpn_config *create_config(const char *ident)
{
	struct vpn_config *config;

	DBG("ident %s", ident);

	if (g_hash_table_lookup(config_table, ident))
		return NULL;

	config = g_try_new0(struct vpn_config, 1);
	if (!config)
		return NULL;

	config->ident = g_strdup(ident);

	config->provider_table = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, unregister_provider);

	g_hash_table_insert(config_table, config->ident, config);

	connman_info("Adding configuration %s", config->ident);

	return config;
}

static bool validate_ident(const char *ident)
{
	unsigned int i;

	if (!ident)
		return false;

	for (i = 0; i < strlen(ident); i++)
		if (!g_ascii_isprint(ident[i]))
			return false;

	return true;
}

static char *get_dir(void)
{
	return g_strdup_printf("%s", VPN_STORAGEDIR);
}

static int read_configs(void)
{
	GDir *dir;
	char *path = get_dir();

	DBG("path %s", path);

	dir = g_dir_open(path, 0, NULL);
	if (dir) {
		const gchar *file;

		while ((file = g_dir_read_name(dir))) {
			GString *str;
			gchar *ident;

			if (!g_str_has_suffix(file, ".config"))
				continue;

			ident = g_strrstr(file, ".config");
			if (!ident)
				continue;

			str = g_string_new_len(file, ident - file);
			if (!str)
				continue;

			ident = g_string_free(str, FALSE);

			if (validate_ident(ident)) {
				struct vpn_config *config;

				config = create_config(ident);
				if (config)
					load_config(config, path, ADD);
			} else {
				connman_error("Invalid config ident %s", ident);
			}
			g_free(ident);
		}

		g_dir_close(dir);
	}

	g_free(path);

	return 0;
}

static void config_notify_handler(struct inotify_event *event,
                                        const char *ident)
{
	char *ext;

	if (!ident)
		return;

	if (!g_str_has_suffix(ident, ".config"))
		return;

	ext = g_strrstr(ident, ".config");
	if (!ext)
		return;

	*ext = '\0';

	if (!validate_ident(ident)) {
		connman_error("Invalid config ident %s", ident);
		return;
	}

	if (event->mask & IN_CREATE)
		return;

	if (event->mask & IN_DELETE) {
		g_hash_table_remove(config_table, ident);
		return;
	}

	if (event->mask & IN_MODIFY) {
		struct vpn_config *config;
		char *path = get_dir();

		config = g_hash_table_lookup(config_table, ident);
		if (config) {
			g_hash_table_remove_all(config->provider_table);
			load_config(config, path, REMOVE);

			/* Re-scan the config file for any changes */
			g_hash_table_remove_all(config->provider_table);
			load_config(config, path, ADD);
		} else {
			/*
			 * Inotify will send create event followed by modify
			 * event for any config file that is copied to
			 * monitored directory. So in practice we should just
			 * ignore the create event and trust only the modify
			 * one in order to avoid create/remove/create loop
			 */
			config = create_config(ident);
			if (config)
				load_config(config, path, ADD);
		}

		g_free(path);
	}
}

int __vpn_config_init(void)
{
	char *dir = get_dir();

	DBG("");

	config_table = g_hash_table_new_full(g_str_hash, g_str_equal,
						NULL, unregister_config);

	connman_inotify_register(dir, config_notify_handler);

	g_free(dir);

	return read_configs();
}

void __vpn_config_cleanup(void)
{
	char *dir = get_dir();

	DBG("");

	cleanup = true;

	connman_inotify_unregister(dir, config_notify_handler);

	g_free(dir);

	g_hash_table_destroy(config_table);
	config_table = NULL;

	cleanup = false;
}

char *__vpn_config_get_string(GKeyFile *key_file,
	const char *group_name, const char *key, GError **error)
{
	char *str = g_key_file_get_string(key_file, group_name, key, error);
	if (!str)
		return NULL;

	return g_strchomp(str);
}

char **__vpn_config_get_string_list(GKeyFile *key_file,
	const char *group_name, const char *key, gsize *length, GError **error)
{
	char **p;
	char **strlist = g_key_file_get_string_list(key_file, group_name, key,
		length, error);
	if (!strlist)
		return NULL;

	p = strlist;
	while (*p) {
		*p = g_strstrip(*p);
		p++;
	}

	return strlist;
}
