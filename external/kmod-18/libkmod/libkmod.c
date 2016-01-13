/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include "libkmod.h"
#include "libkmod-internal.h"
#include "libkmod-index.h"

#define KMOD_HASH_SIZE (256)
#define KMOD_LRU_MAX (128)
#define _KMOD_INDEX_MODULES_SIZE KMOD_INDEX_MODULES_BUILTIN + 1

/**
 * SECTION:libkmod
 * @short_description: libkmod context
 *
 * The context contains the default values for the library user,
 * and is passed to all library operations.
 */

static struct _index_files {
	const char *fn;
	const char *prefix;
} index_files[] = {
	[KMOD_INDEX_MODULES_DEP] = { .fn = "modules.dep", .prefix = "" },
	[KMOD_INDEX_MODULES_ALIAS] = { .fn = "modules.alias", .prefix = "alias " },
	[KMOD_INDEX_MODULES_SYMBOL] = { .fn = "modules.symbols", .prefix = "alias "},
	[KMOD_INDEX_MODULES_BUILTIN] = { .fn = "modules.builtin", .prefix = ""},
};

static const char *default_config_paths[] = {
	SYSCONFDIR "/modprobe.d",
	"/run/modprobe.d",
	"/lib/modprobe.d",
	NULL
};

/**
 * kmod_ctx:
 *
 * Opaque object representing the library context.
 */
struct kmod_ctx {
	int refcount;
	int log_priority;
	void (*log_fn)(void *data,
			int priority, const char *file, int line,
			const char *fn, const char *format, va_list args);
	void *log_data;
	const void *userdata;
	char *dirname;
	struct kmod_config *config;
	struct hash *modules_by_name;
	struct index_mm *indexes[_KMOD_INDEX_MODULES_SIZE];
	unsigned long long indexes_stamp[_KMOD_INDEX_MODULES_SIZE];
};

void kmod_log(const struct kmod_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, ...)
{
	va_list args;

	if (ctx->log_fn == NULL)
		return;

	va_start(args, format);
	ctx->log_fn(ctx->log_data, priority, file, line, fn, format, args);
	va_end(args);
}

_printf_format_(6, 0)
static void log_filep(void *data,
			int priority, const char *file, int line,
			const char *fn, const char *format, va_list args)
{
	FILE *fp = data;
#ifdef ENABLE_DEBUG
	char buf[16];
	const char *priname;
	switch (priority) {
	case LOG_EMERG:
		priname = "EMERGENCY";
		break;
	case LOG_ALERT:
		priname = "ALERT";
		break;
	case LOG_CRIT:
		priname = "CRITICAL";
		break;
	case LOG_ERR:
		priname = "ERROR";
		break;
	case LOG_WARNING:
		priname = "WARNING";
		break;
	case LOG_NOTICE:
		priname = "NOTICE";
		break;
	case LOG_INFO:
		priname = "INFO";
		break;
	case LOG_DEBUG:
		priname = "DEBUG";
		break;
	default:
		snprintf(buf, sizeof(buf), "L:%d", priority);
		priname = buf;
	}
	fprintf(fp, "libkmod: %s %s:%d %s: ", priname, file, line, fn);
#else
	fprintf(fp, "libkmod: %s: ", fn);
#endif
	vfprintf(fp, format, args);
}

const char *kmod_get_dirname(const struct kmod_ctx *ctx)
{
	return ctx->dirname;
}

/**
 * kmod_get_userdata:
 * @ctx: kmod library context
 *
 * Retrieve stored data pointer from library context. This might be useful
 * to access from callbacks.
 *
 * Returns: stored userdata
 */
KMOD_EXPORT void *kmod_get_userdata(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	return (void *)ctx->userdata;
}

/**
 * kmod_set_userdata:
 * @ctx: kmod library context
 * @userdata: data pointer
 *
 * Store custom @userdata in the library context.
 */
KMOD_EXPORT void kmod_set_userdata(struct kmod_ctx *ctx, const void *userdata)
{
	if (ctx == NULL)
		return;
	ctx->userdata = userdata;
}

static int log_priority(const char *priority)
{
	char *endptr;
	int prio;

	prio = strtol(priority, &endptr, 10);
	if (endptr[0] == '\0' || isspace(endptr[0]))
		return prio;
	if (strncmp(priority, "err", 3) == 0)
		return LOG_ERR;
	if (strncmp(priority, "info", 4) == 0)
		return LOG_INFO;
	if (strncmp(priority, "debug", 5) == 0)
		return LOG_DEBUG;
	return 0;
}

static const char *dirname_default_prefix = "/lib/modules";

static char *get_kernel_release(const char *dirname)
{
	struct utsname u;
	char *p;

	if (dirname != NULL)
		return path_make_absolute_cwd(dirname);

	if (uname(&u) < 0)
		return NULL;

	if (asprintf(&p, "%s/%s", dirname_default_prefix, u.release) < 0)
		return NULL;

	return p;
}

/**
 * kmod_new:
 * @dirname: what to consider as linux module's directory, if NULL
 *           defaults to /lib/modules/`uname -r`. If it's relative,
 *           it's treated as relative to the current working directory.
 *           Otherwise, give an absolute dirname.
 * @config_paths: ordered array of paths (directories or files) where
 *                to load from user-defined configuration parameters such as
 *                alias, blacklists, commands (install, remove). If
 *                NULL defaults to /run/modprobe.d, /etc/modprobe.d and
 *                /lib/modprobe.d. Give an empty vector if configuration should
 *                not be read. This array must be null terminated.
 *
 * Create kmod library context. This reads the kmod configuration
 * and fills in the default values.
 *
 * The initial refcount is 1, and needs to be decremented to
 * release the resources of the kmod library context.
 *
 * Returns: a new kmod library context
 */
KMOD_EXPORT struct kmod_ctx *kmod_new(const char *dirname,
					const char * const *config_paths)
{
	const char *env;
	struct kmod_ctx *ctx;
	int err;

	ctx = calloc(1, sizeof(struct kmod_ctx));
	if (!ctx)
		return NULL;

	ctx->refcount = 1;
	ctx->log_fn = log_filep;
	ctx->log_data = stderr;
	ctx->log_priority = LOG_ERR;

	ctx->dirname = get_kernel_release(dirname);

	/* environment overwrites config */
	env = secure_getenv("KMOD_LOG");
	if (env != NULL)
		kmod_set_log_priority(ctx, log_priority(env));

	if (config_paths == NULL)
		config_paths = default_config_paths;
	err = kmod_config_new(ctx, &ctx->config, config_paths);
	if (err < 0) {
		ERR(ctx, "could not create config\n");
		goto fail;
	}

	ctx->modules_by_name = hash_new(KMOD_HASH_SIZE, NULL);
	if (ctx->modules_by_name == NULL) {
		ERR(ctx, "could not create by-name hash\n");
		goto fail;
	}

	INFO(ctx, "ctx %p created\n", ctx);
	DBG(ctx, "log_priority=%d\n", ctx->log_priority);

	return ctx;

fail:
	free(ctx->modules_by_name);
	free(ctx->dirname);
	free(ctx);
	return NULL;
}

/**
 * kmod_ref:
 * @ctx: kmod library context
 *
 * Take a reference of the kmod library context.
 *
 * Returns: the passed kmod library context
 */
KMOD_EXPORT struct kmod_ctx *kmod_ref(struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

/**
 * kmod_unref:
 * @ctx: kmod library context
 *
 * Drop a reference of the kmod library context. If the refcount
 * reaches zero, the resources of the context will be released.
 *
 * Returns: the passed kmod library context or NULL if it's freed
 */
KMOD_EXPORT struct kmod_ctx *kmod_unref(struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;

	if (--ctx->refcount > 0)
		return ctx;

	INFO(ctx, "context %p released\n", ctx);

	kmod_unload_resources(ctx);
	hash_free(ctx->modules_by_name);
	free(ctx->dirname);
	if (ctx->config)
		kmod_config_free(ctx->config);

	free(ctx);
	return NULL;
}

/**
 * kmod_set_log_fn:
 * @ctx: kmod library context
 * @log_fn: function to be called for logging messages
 * @data: data to pass to log function
 *
 * The built-in logging writes to stderr. It can be
 * overridden by a custom function, to plug log messages
 * into the user's logging functionality.
 */
KMOD_EXPORT void kmod_set_log_fn(struct kmod_ctx *ctx,
					void (*log_fn)(void *data,
						int priority, const char *file,
						int line, const char *fn,
						const char *format, va_list args),
					const void *data)
{
	if (ctx == NULL)
		return;
	ctx->log_fn = log_fn;
	ctx->log_data = (void *)data;
	INFO(ctx, "custom logging function %p registered\n", log_fn);
}

/**
 * kmod_get_log_priority:
 * @ctx: kmod library context
 *
 * Returns: the current logging priority
 */
KMOD_EXPORT int kmod_get_log_priority(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return -1;
	return ctx->log_priority;
}

/**
 * kmod_set_log_priority:
 * @ctx: kmod library context
 * @priority: the new logging priority
 *
 * Set the current logging priority. The value controls which messages
 * are logged.
 */
KMOD_EXPORT void kmod_set_log_priority(struct kmod_ctx *ctx, int priority)
{
	if (ctx == NULL)
		return;
	ctx->log_priority = priority;
}

struct kmod_module *kmod_pool_get_module(struct kmod_ctx *ctx,
							const char *key)
{
	struct kmod_module *mod;

	mod = hash_find(ctx->modules_by_name, key);

	DBG(ctx, "get module name='%s' found=%p\n", key, mod);

	return mod;
}

void kmod_pool_add_module(struct kmod_ctx *ctx, struct kmod_module *mod,
							const char *key)
{
	DBG(ctx, "add %p key='%s'\n", mod, key);

	hash_add(ctx->modules_by_name, key, mod);
}

void kmod_pool_del_module(struct kmod_ctx *ctx, struct kmod_module *mod,
							const char *key)
{
	DBG(ctx, "del %p key='%s'\n", mod, key);

	hash_del(ctx->modules_by_name, key);
}

static int kmod_lookup_alias_from_alias_bin(struct kmod_ctx *ctx,
						enum kmod_index index_number,
						const char *name,
						struct kmod_list **list)
{
	int err, nmatch = 0;
	struct index_file *idx;
	struct index_value *realnames, *realname;

	if (ctx->indexes[index_number] != NULL) {
		DBG(ctx, "use mmaped index '%s' for name=%s\n",
			index_files[index_number].fn, name);
		realnames = index_mm_searchwild(ctx->indexes[index_number],
									name);
	} else {
		char fn[PATH_MAX];

		snprintf(fn, sizeof(fn), "%s/%s.bin", ctx->dirname,
					index_files[index_number].fn);

		DBG(ctx, "file=%s name=%s\n", fn, name);

		idx = index_file_open(fn);
		if (idx == NULL)
			return -ENOSYS;

		realnames = index_searchwild(idx, name);
		index_file_close(idx);
	}

	for (realname = realnames; realname; realname = realname->next) {
		struct kmod_module *mod;

		err = kmod_module_new_from_alias(ctx, name, realname->value, &mod);
		if (err < 0) {
			ERR(ctx, "Could not create module for alias=%s realname=%s: %s\n",
			    name, realname->value, strerror(-err));
			goto fail;
		}

		*list = kmod_list_append(*list, mod);
		nmatch++;
	}

	index_values_free(realnames);
	return nmatch;

fail:
	*list = kmod_list_remove_n_latest(*list, nmatch);
	index_values_free(realnames);
	return err;

}

int kmod_lookup_alias_from_symbols_file(struct kmod_ctx *ctx, const char *name,
						struct kmod_list **list)
{
	if (!strstartswith(name, "symbol:"))
		return 0;

	return kmod_lookup_alias_from_alias_bin(ctx, KMOD_INDEX_MODULES_SYMBOL,
								name, list);
}

int kmod_lookup_alias_from_aliases_file(struct kmod_ctx *ctx, const char *name,
						struct kmod_list **list)
{
	return kmod_lookup_alias_from_alias_bin(ctx, KMOD_INDEX_MODULES_ALIAS,
								name, list);
}

int kmod_lookup_alias_from_builtin_file(struct kmod_ctx *ctx, const char *name,
						struct kmod_list **list)
{
	char *line = NULL;
	int err = 0;

	assert(*list == NULL);

	if (ctx->indexes[KMOD_INDEX_MODULES_BUILTIN]) {
		DBG(ctx, "use mmaped index '%s' modname=%s\n",
				index_files[KMOD_INDEX_MODULES_BUILTIN].fn,
				name);
		line = index_mm_search(ctx->indexes[KMOD_INDEX_MODULES_BUILTIN],
									name);
	} else {
		struct index_file *idx;
		char fn[PATH_MAX];

		snprintf(fn, sizeof(fn), "%s/%s.bin", ctx->dirname,
				index_files[KMOD_INDEX_MODULES_BUILTIN].fn);
		DBG(ctx, "file=%s modname=%s\n", fn, name);

		idx = index_file_open(fn);
		if (idx == NULL) {
			DBG(ctx, "could not open builtin file '%s'\n", fn);
			goto finish;
		}

		line = index_search(idx, name);
		index_file_close(idx);
	}

	if (line != NULL) {
		struct kmod_module *mod;

		err = kmod_module_new_from_name(ctx, name, &mod);
		if (err < 0) {
			ERR(ctx, "Could not create module from name %s: %s\n",
							name, strerror(-err));
			goto finish;
		}

		kmod_module_set_builtin(mod, true);
		*list = kmod_list_append(*list, mod);
		if (*list == NULL)
			err = -ENOMEM;
	}

finish:
	free(line);
	return err;
}

char *kmod_search_moddep(struct kmod_ctx *ctx, const char *name)
{
	struct index_file *idx;
	char fn[PATH_MAX];
	char *line;

	if (ctx->indexes[KMOD_INDEX_MODULES_DEP]) {
		DBG(ctx, "use mmaped index '%s' modname=%s\n",
				index_files[KMOD_INDEX_MODULES_DEP].fn, name);
		return index_mm_search(ctx->indexes[KMOD_INDEX_MODULES_DEP],
									name);
	}

	snprintf(fn, sizeof(fn), "%s/%s.bin", ctx->dirname,
					index_files[KMOD_INDEX_MODULES_DEP].fn);

	DBG(ctx, "file=%s modname=%s\n", fn, name);

	idx = index_file_open(fn);
	if (idx == NULL) {
		DBG(ctx, "could not open moddep file '%s'\n", fn);
		return NULL;
	}

	line = index_search(idx, name);
	index_file_close(idx);

	return line;
}

int kmod_lookup_alias_from_moddep_file(struct kmod_ctx *ctx, const char *name,
						struct kmod_list **list)
{
	char *line;
	int n = 0;

	/*
	 * Module names do not contain ':'. Return early if we know it will
	 * not be found.
	 */
	if (strchr(name, ':'))
		return 0;

	line = kmod_search_moddep(ctx, name);
	if (line != NULL) {
		struct kmod_module *mod;

		n = kmod_module_new_from_name(ctx, name, &mod);
		if (n < 0) {
			ERR(ctx, "Could not create module from name %s: %s\n",
			    name, strerror(-n));
			goto finish;
		}

		*list = kmod_list_append(*list, mod);
		kmod_module_parse_depline(mod, line);
	}

finish:
	free(line);

	return n;
}

int kmod_lookup_alias_from_config(struct kmod_ctx *ctx, const char *name,
						struct kmod_list **list)
{
	struct kmod_config *config = ctx->config;
	struct kmod_list *l;
	int err, nmatch = 0;

	kmod_list_foreach(l, config->aliases) {
		const char *aliasname = kmod_alias_get_name(l);
		const char *modname = kmod_alias_get_modname(l);

		if (fnmatch(aliasname, name, 0) == 0) {
			struct kmod_module *mod;

			err = kmod_module_new_from_alias(ctx, aliasname,
								modname, &mod);
			if (err < 0) {
				ERR(ctx, "Could not create module for alias=%s modname=%s: %s\n",
				    name, modname, strerror(-err));
				goto fail;
			}

			*list = kmod_list_append(*list, mod);
			nmatch++;
		}
	}

	return nmatch;

fail:
	*list = kmod_list_remove_n_latest(*list, nmatch);
	return err;
}

int kmod_lookup_alias_from_commands(struct kmod_ctx *ctx, const char *name,
						struct kmod_list **list)
{
	struct kmod_config *config = ctx->config;
	struct kmod_list *l, *node;
	int err, nmatch = 0;

	kmod_list_foreach(l, config->install_commands) {
		const char *modname = kmod_command_get_modname(l);

		if (streq(modname, name)) {
			const char *cmd = kmod_command_get_command(l);
			struct kmod_module *mod;

			err = kmod_module_new_from_name(ctx, modname, &mod);
			if (err < 0) {
				ERR(ctx, "Could not create module from name %s: %s\n",
				    modname, strerror(-err));
				return err;
			}

			node = kmod_list_append(*list, mod);
			if (node == NULL) {
				ERR(ctx, "out of memory\n");
				return -ENOMEM;
			}

			*list = node;
			nmatch = 1;

			kmod_module_set_install_commands(mod, cmd);

			/*
			 * match only the first one, like modprobe from
			 * module-init-tools does
			 */
			break;
		}
	}

	if (nmatch)
		return nmatch;

	kmod_list_foreach(l, config->remove_commands) {
		const char *modname = kmod_command_get_modname(l);

		if (streq(modname, name)) {
			const char *cmd = kmod_command_get_command(l);
			struct kmod_module *mod;

			err = kmod_module_new_from_name(ctx, modname, &mod);
			if (err < 0) {
				ERR(ctx, "Could not create module from name %s: %s\n",
				    modname, strerror(-err));
				return err;
			}

			node = kmod_list_append(*list, mod);
			if (node == NULL) {
				ERR(ctx, "out of memory\n");
				return -ENOMEM;
			}

			*list = node;
			nmatch = 1;

			kmod_module_set_remove_commands(mod, cmd);

			/*
			 * match only the first one, like modprobe from
			 * module-init-tools does
			 */
			break;
		}
	}

	return nmatch;
}

void kmod_set_modules_visited(struct kmod_ctx *ctx, bool visited)
{
	struct hash_iter iter;
	const void *v;

	hash_iter_init(ctx->modules_by_name, &iter);
	while (hash_iter_next(&iter, NULL, &v))
		kmod_module_set_visited((struct kmod_module *)v, visited);
}

void kmod_set_modules_required(struct kmod_ctx *ctx, bool required)
{
	struct hash_iter iter;
	const void *v;

	hash_iter_init(ctx->modules_by_name, &iter);
	while (hash_iter_next(&iter, NULL, &v))
		kmod_module_set_required((struct kmod_module *)v, required);
}

static bool is_cache_invalid(const char *path, unsigned long long stamp)
{
	struct stat st;

	if (stat(path, &st) < 0)
		return true;

	if (stamp != stat_mstamp(&st))
		return true;

	return false;
}

/**
 * kmod_validate_resources:
 * @ctx: kmod library context
 *
 * Check if indexes and configuration files changed on disk and the current
 * context is not valid anymore.
 *
 * Returns: KMOD_RESOURCES_OK if resources are still valid,
 * KMOD_RESOURCES_MUST_RELOAD if it's sufficient to call
 * kmod_unload_resources() and kmod_load_resources() or
 * KMOD_RESOURCES_MUST_RECREATE if @ctx must be re-created.
 */
KMOD_EXPORT int kmod_validate_resources(struct kmod_ctx *ctx)
{
	struct kmod_list *l;
	size_t i;

	if (ctx == NULL || ctx->config == NULL)
		return KMOD_RESOURCES_MUST_RECREATE;

	kmod_list_foreach(l, ctx->config->paths) {
		struct kmod_config_path *cf = l->data;

		if (is_cache_invalid(cf->path, cf->stamp))
			return KMOD_RESOURCES_MUST_RECREATE;
	}

	for (i = 0; i < _KMOD_INDEX_MODULES_SIZE; i++) {
		char path[PATH_MAX];

		if (ctx->indexes[i] == NULL)
			continue;

		snprintf(path, sizeof(path), "%s/%s.bin", ctx->dirname,
						index_files[i].fn);

		if (is_cache_invalid(path, ctx->indexes_stamp[i]))
			return KMOD_RESOURCES_MUST_RELOAD;
	}

	return KMOD_RESOURCES_OK;
}

/**
 * kmod_load_resources:
 * @ctx: kmod library context
 *
 * Load indexes and keep them open in @ctx. This way it's faster to lookup
 * information within the indexes. If this function is not called before a
 * search, the necessary index is always opened and closed.
 *
 * If user will do more than one or two lookups, insertions, deletions, most
 * likely it's good to call this function first. Particularly in a daemon like
 * udev that on bootup issues hundreds of calls to lookup the index, calling
 * this function will speedup the searches.
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT int kmod_load_resources(struct kmod_ctx *ctx)
{
	size_t i;

	if (ctx == NULL)
		return -ENOENT;

	for (i = 0; i < _KMOD_INDEX_MODULES_SIZE; i++) {
		char path[PATH_MAX];

		if (ctx->indexes[i] != NULL) {
			INFO(ctx, "Index %s already loaded\n",
							index_files[i].fn);
			continue;
		}

		snprintf(path, sizeof(path), "%s/%s.bin", ctx->dirname,
							index_files[i].fn);
		ctx->indexes[i] = index_mm_open(ctx, path,
						&ctx->indexes_stamp[i]);
		if (ctx->indexes[i] == NULL)
			goto fail;
	}

	return 0;

fail:
	kmod_unload_resources(ctx);
	return -ENOMEM;
}

/**
 * kmod_unload_resources:
 * @ctx: kmod library context
 *
 * Unload all the indexes. This will free the resources to maintain the index
 * open and all subsequent searches will need to open and close the index.
 *
 * User is free to call kmod_load_resources() and kmod_unload_resources() as
 * many times as wanted during the lifecycle of @ctx. For example, if a daemon
 * knows that when starting up it will lookup a lot of modules, it could call
 * kmod_load_resources() and after the first burst of searches is gone, it
 * could free the resources by calling kmod_unload_resources().
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT void kmod_unload_resources(struct kmod_ctx *ctx)
{
	size_t i;

	if (ctx == NULL)
		return;

	for (i = 0; i < _KMOD_INDEX_MODULES_SIZE; i++) {
		if (ctx->indexes[i] != NULL) {
			index_mm_close(ctx->indexes[i]);
			ctx->indexes[i] = NULL;
			ctx->indexes_stamp[i] = 0;
		}
	}
}

/**
 * kmod_dump_index:
 * @ctx: kmod library context
 * @type: index to dump, valid indexes are
 * KMOD_INDEX_MODULES_DEP: index of module dependencies;
 * KMOD_INDEX_MODULES_ALIAS: index of module aliases;
 * KMOD_INDEX_MODULES_SYMBOL: index of symbol aliases;
 * KMOD_INDEX_MODULES_BUILTIN: index of builtin module.
 * @fd: file descriptor to dump index to
 *
 * Dump index to file descriptor. Note that this function doesn't use stdio.h
 * so call fflush() before calling this function to be sure data is written in
 * order.
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT int kmod_dump_index(struct kmod_ctx *ctx, enum kmod_index type,
									int fd)
{
	if (ctx == NULL)
		return -ENOSYS;

	if (type < 0 || type >= _KMOD_INDEX_MODULES_SIZE)
		return -ENOENT;

	if (ctx->indexes[type] != NULL) {
		DBG(ctx, "use mmaped index '%s'\n", index_files[type].fn);
		index_mm_dump(ctx->indexes[type], fd,
						index_files[type].prefix);
	} else {
		char fn[PATH_MAX];
		struct index_file *idx;

		snprintf(fn, sizeof(fn), "%s/%s.bin", ctx->dirname,
						index_files[type].fn);

		DBG(ctx, "file=%s\n", fn);

		idx = index_file_open(fn);
		if (idx == NULL)
			return -ENOSYS;

		index_dump(idx, fd, index_files[type].prefix);
		index_file_close(idx);
	}

	return 0;
}

const struct kmod_config *kmod_get_config(const struct kmod_ctx *ctx)
{
	return ctx->config;
}
