/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
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
#include <dlfcn.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>

#include "pacrunner.h"

static GSList *plugins = NULL;

struct pacrunner_plugin {
	void *handle;
	struct pacrunner_plugin_desc *desc;
};

static bool add_plugin(void *handle, struct pacrunner_plugin_desc *desc)
{
	struct pacrunner_plugin *plugin;

	if (!desc->init)
		return false;

	plugin = g_try_new0(struct pacrunner_plugin, 1);
	if (!plugin)
		return false;

	plugin->handle = handle;
	plugin->desc = desc;

	if (desc->init() < 0) {
		g_free(plugin);
		return false;
	}

	plugins = g_slist_append(plugins, plugin);
	DBG("Plugin %s loaded", desc->name);

	return true;
}

static bool check_plugin(struct pacrunner_plugin_desc *desc,
				char **patterns, char **excludes)
{
	if (excludes) {
		for (; *excludes; excludes++)
			if (g_pattern_match_simple(*excludes, desc->name))
				break;
		if (*excludes) {
			pacrunner_info("Excluding %s", desc->name);
			return false;
		}
	}

	if (patterns) {
		for (; *patterns; patterns++)
			if (g_pattern_match_simple(*patterns, desc->name))
				break;
		if (!*patterns) {
			pacrunner_info("Ignoring %s", desc->name);
			return false;
		}
	}

	return true;
}

#include "builtin.h"

int __pacrunner_plugin_init(const char *pattern, const char *exclude)
{
	gchar **patterns = NULL;
	gchar **excludes = NULL;
	GDir *dir;
	const char *file;
	unsigned int i;

	if (strlen(PLUGINDIR) == 0)
		return -EINVAL;

	DBG("");

	if (pattern)
		patterns = g_strsplit_set(pattern, ", ", -1);

	if (exclude)
		excludes = g_strsplit_set(exclude, ", ", -1);

	for (i = 0; __pacrunner_builtin[i]; i++) {
		if (!check_plugin(__pacrunner_builtin[i], patterns, excludes))
			continue;

		add_plugin(NULL, __pacrunner_builtin[i]);
	}

	dir = g_dir_open(PLUGINDIR, 0, NULL);
	if (!dir)
		return -EIO;

	while ((file = g_dir_read_name(dir))) {
		struct pacrunner_plugin_desc *desc;
		void *handle;
		char *filename;

		if (g_str_has_prefix(file, "lib") ||
				!g_str_has_suffix(file, ".so"))
			continue;

		filename = g_build_filename(PLUGINDIR, file, NULL);

		handle = dlopen(filename, RTLD_NOW);
		if (!handle) {
			pacrunner_error("Can't load plugin %s: %s",
							filename, dlerror());
			g_free(filename);
			continue;
		}

		g_free(filename);

		desc = dlsym(handle, "pacrunner_plugin_desc");
		if (!desc) {
			pacrunner_error("Can't load plugin description: %s",
								dlerror());
			dlclose(handle);
			continue;
		}

		if (!check_plugin(desc, patterns, excludes)) {
			dlclose(handle);
			continue;
		}

		if (!add_plugin(handle, desc))
				dlclose(handle);
	}

	g_dir_close(dir);

	g_strfreev(patterns);
	g_strfreev(excludes);

	return 0;
}

void __pacrunner_plugin_cleanup(void)
{
	GSList *list;

	DBG("");

	for (list = plugins; list; list = list->next) {
		struct pacrunner_plugin *plugin = list->data;

		if (plugin->desc->exit)
			plugin->desc->exit();

		if (plugin->handle)
			dlclose(plugin->handle);

		g_free(plugin);
	}

	g_slist_free(plugins);
}
