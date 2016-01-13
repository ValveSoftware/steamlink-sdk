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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <fnmatch.h>

#ifdef HAVE_LINUX_MODULE_H
#include <linux/module.h>
#endif

#include "libkmod.h"
#include "libkmod-internal.h"

/**
 * SECTION:libkmod-module
 * @short_description: operate on kernel modules
 */

/**
 * kmod_module:
 *
 * Opaque object representing a module.
 */
struct kmod_module {
	struct kmod_ctx *ctx;
	char *hashkey;
	char *name;
	char *path;
	struct kmod_list *dep;
	char *options;
	const char *install_commands;	/* owned by kmod_config */
	const char *remove_commands;	/* owned by kmod_config */
	char *alias; /* only set if this module was created from an alias */
	struct kmod_file *file;
	int n_dep;
	int refcount;
	struct {
		bool dep : 1;
		bool options : 1;
		bool install_commands : 1;
		bool remove_commands : 1;
	} init;

	/*
	 * private field used by kmod_module_get_probe_list() to detect
	 * dependency loops
	 */
	bool visited : 1;

	/*
	 * set by kmod_module_get_probe_list: indicates for probe_insert()
	 * whether the module's command and softdep should be ignored
	 */
	bool ignorecmd : 1;

	/*
	 * set by kmod_module_get_probe_list: indicates whether this is the
	 * module the user asked for or its dependency, or whether this
	 * is a softdep only
	 */
	bool required : 1;

	/*
	 * if module was created by searching the modules.builtin file, this
	 * is set. There's nothing much useful one can do with such a
	 * "module", except knowing it's builtin.
	 */
	bool builtin : 1;
};

static inline const char *path_join(const char *path, size_t prefixlen,
							char buf[PATH_MAX])
{
	size_t pathlen;

	if (path[0] == '/')
		return path;

	pathlen = strlen(path);
	if (prefixlen + pathlen + 1 >= PATH_MAX)
		return NULL;

	memcpy(buf + prefixlen, path, pathlen + 1);
	return buf;
}

static inline bool module_is_inkernel(struct kmod_module *mod)
{
	int state = kmod_module_get_initstate(mod);

	if (state == KMOD_MODULE_LIVE ||
			state == KMOD_MODULE_BUILTIN)
		return true;

	return false;
}

int kmod_module_parse_depline(struct kmod_module *mod, char *line)
{
	struct kmod_ctx *ctx = mod->ctx;
	struct kmod_list *list = NULL;
	const char *dirname;
	char buf[PATH_MAX];
	char *p, *saveptr;
	int err = 0, n = 0;
	size_t dirnamelen;

	if (mod->init.dep)
		return mod->n_dep;
	assert(mod->dep == NULL);
	mod->init.dep = true;

	p = strchr(line, ':');
	if (p == NULL)
		return 0;

	*p = '\0';
	dirname = kmod_get_dirname(mod->ctx);
	dirnamelen = strlen(dirname);
	if (dirnamelen + 2 >= PATH_MAX)
		return 0;

	memcpy(buf, dirname, dirnamelen);
	buf[dirnamelen] = '/';
	dirnamelen++;
	buf[dirnamelen] = '\0';

	if (mod->path == NULL) {
		const char *str = path_join(line, dirnamelen, buf);
		if (str == NULL)
			return 0;
		mod->path = strdup(str);
		if (mod->path == NULL)
			return 0;
	}

	p++;
	for (p = strtok_r(p, " \t", &saveptr); p != NULL;
					p = strtok_r(NULL, " \t", &saveptr)) {
		struct kmod_module *depmod;
		const char *path;

		path = path_join(p, dirnamelen, buf);
		if (path == NULL) {
			ERR(ctx, "could not join path '%s' and '%s'.\n",
			    dirname, p);
			goto fail;
		}

		err = kmod_module_new_from_path(ctx, path, &depmod);
		if (err < 0) {
			ERR(ctx, "ctx=%p path=%s error=%s\n",
						ctx, path, strerror(-err));
			goto fail;
		}

		DBG(ctx, "add dep: %s\n", path);

		list = kmod_list_prepend(list, depmod);
		n++;
	}

	DBG(ctx, "%d dependencies for %s\n", n, mod->name);

	mod->dep = list;
	mod->n_dep = n;
	return n;

fail:
	kmod_module_unref_list(list);
	mod->init.dep = false;
	return err;
}

void kmod_module_set_visited(struct kmod_module *mod, bool visited)
{
	mod->visited = visited;
}

void kmod_module_set_builtin(struct kmod_module *mod, bool builtin)
{
	mod->builtin = builtin;
}

void kmod_module_set_required(struct kmod_module *mod, bool required)
{
	mod->required = required;
}

/*
 * Memory layout with alias:
 *
 * struct kmod_module {
 *        hashkey -----.
 *        alias -----. |
 *        name ----. | |
 * }               | | |
 * name <----------' | |
 * alias <-----------' |
 * name\alias <--------'
 *
 * Memory layout without alias:
 *
 * struct kmod_module {
 *        hashkey ---.
 *        alias -----|----> NULL
 *        name ----. |
 * }               | |
 * name <----------'-'
 *
 * @key is "name\alias" or "name" (in which case alias == NULL)
 */
static int kmod_module_new(struct kmod_ctx *ctx, const char *key,
				const char *name, size_t namelen,
				const char *alias, size_t aliaslen,
				struct kmod_module **mod)
{
	struct kmod_module *m;
	size_t keylen;

	m = kmod_pool_get_module(ctx, key);
	if (m != NULL) {
		*mod = kmod_module_ref(m);
		return 0;
	}

	if (alias == NULL)
		keylen = namelen;
	else
		keylen = namelen + aliaslen + 1;

	m = malloc(sizeof(*m) + (alias == NULL ? 1 : 2) * (keylen + 1));
	if (m == NULL)
		return -ENOMEM;

	memset(m, 0, sizeof(*m));

	m->ctx = kmod_ref(ctx);
	m->name = (char *)m + sizeof(*m);
	memcpy(m->name, key, keylen + 1);
	if (alias == NULL) {
		m->hashkey = m->name;
		m->alias = NULL;
	} else {
		m->name[namelen] = '\0';
		m->alias = m->name + namelen + 1;
		m->hashkey = m->name + keylen + 1;
		memcpy(m->hashkey, key, keylen + 1);
	}

	m->refcount = 1;
	kmod_pool_add_module(ctx, m, m->hashkey);
	*mod = m;

	return 0;
}

/**
 * kmod_module_new_from_name:
 * @ctx: kmod library context
 * @name: name of the module
 * @mod: where to save the created struct kmod_module
 *
 * Create a new struct kmod_module using the module name. @name can not be an
 * alias, file name or anything else; it must be a module name. There's no
 * check if the module exists in the system.
 *
 * This function is also used internally by many others that return a new
 * struct kmod_module or a new list of modules.
 *
 * The initial refcount is 1, and needs to be decremented to release the
 * resources of the kmod_module. Since libkmod keeps track of all
 * kmod_modules created, they are all released upon @ctx destruction too. Do
 * not unref @ctx before all the desired operations with the returned
 * kmod_module are done.
 *
 * Returns: 0 on success or < 0 otherwise. It fails if name is not a valid
 * module name or if memory allocation failed.
 */
KMOD_EXPORT int kmod_module_new_from_name(struct kmod_ctx *ctx,
						const char *name,
						struct kmod_module **mod)
{
	size_t namelen;
	char name_norm[PATH_MAX];

	if (ctx == NULL || name == NULL || mod == NULL)
		return -ENOENT;

	modname_normalize(name, name_norm, &namelen);

	return kmod_module_new(ctx, name_norm, name_norm, namelen, NULL, 0, mod);
}

int kmod_module_new_from_alias(struct kmod_ctx *ctx, const char *alias,
				const char *name, struct kmod_module **mod)
{
	int err;
	char key[PATH_MAX];
	size_t namelen = strlen(name);
	size_t aliaslen = strlen(alias);

	if (namelen + aliaslen + 2 > PATH_MAX)
		return -ENAMETOOLONG;

	memcpy(key, name, namelen);
	memcpy(key + namelen + 1, alias, aliaslen + 1);
	key[namelen] = '\\';

	err = kmod_module_new(ctx, key, name, namelen, alias, aliaslen, mod);
	if (err < 0)
		return err;

	return 0;
}

/**
 * kmod_module_new_from_path:
 * @ctx: kmod library context
 * @path: path where to find the given module
 * @mod: where to save the created struct kmod_module
 *
 * Create a new struct kmod_module using the module path. @path must be an
 * existent file with in the filesystem and must be accessible to libkmod.
 *
 * The initial refcount is 1, and needs to be decremented to release the
 * resources of the kmod_module. Since libkmod keeps track of all
 * kmod_modules created, they are all released upon @ctx destruction too. Do
 * not unref @ctx before all the desired operations with the returned
 * kmod_module are done.
 *
 * If @path is relative, it's treated as relative to the current working
 * directory. Otherwise, give an absolute path.
 *
 * Returns: 0 on success or < 0 otherwise. It fails if file does not exist, if
 * it's not a valid file for a kmod_module or if memory allocation failed.
 */
KMOD_EXPORT int kmod_module_new_from_path(struct kmod_ctx *ctx,
						const char *path,
						struct kmod_module **mod)
{
	struct kmod_module *m;
	int err;
	struct stat st;
	char name[PATH_MAX];
	char *abspath;
	size_t namelen;

	if (ctx == NULL || path == NULL || mod == NULL)
		return -ENOENT;

	abspath = path_make_absolute_cwd(path);
	if (abspath == NULL) {
		DBG(ctx, "no absolute path for %s\n", path);
		return -ENOMEM;
	}

	err = stat(abspath, &st);
	if (err < 0) {
		err = -errno;
		DBG(ctx, "stat %s: %s\n", path, strerror(errno));
		free(abspath);
		return err;
	}

	if (path_to_modname(path, name, &namelen) == NULL) {
		DBG(ctx, "could not get modname from path %s\n", path);
		free(abspath);
		return -ENOENT;
	}

	m = kmod_pool_get_module(ctx, name);
	if (m != NULL) {
		if (m->path == NULL)
			m->path = abspath;
		else if (streq(m->path, abspath))
			free(abspath);
		else {
			ERR(ctx, "kmod_module '%s' already exists with different path: new-path='%s' old-path='%s'\n",
							name, abspath, m->path);
			free(abspath);
			return -EEXIST;
		}

		*mod = kmod_module_ref(m);
		return 0;
	}

	err = kmod_module_new(ctx, name, name, namelen, NULL, 0, &m);
	if (err < 0) {
		free(abspath);
		return err;
	}

	m->path = abspath;
	*mod = m;

	return 0;
}

/**
 * kmod_module_unref:
 * @mod: kmod module
 *
 * Drop a reference of the kmod module. If the refcount reaches zero, its
 * resources are released.
 *
 * Returns: NULL if @mod is NULL or if the module was released. Otherwise it
 * returns the passed @mod with its refcount decremented.
 */
KMOD_EXPORT struct kmod_module *kmod_module_unref(struct kmod_module *mod)
{
	if (mod == NULL)
		return NULL;

	if (--mod->refcount > 0)
		return mod;

	DBG(mod->ctx, "kmod_module %p released\n", mod);

	kmod_pool_del_module(mod->ctx, mod, mod->hashkey);
	kmod_module_unref_list(mod->dep);

	if (mod->file)
		kmod_file_unref(mod->file);

	kmod_unref(mod->ctx);
	free(mod->options);
	free(mod->path);
	free(mod);
	return NULL;
}

/**
 * kmod_module_ref:
 * @mod: kmod module
 *
 * Take a reference of the kmod module, incrementing its refcount.
 *
 * Returns: the passed @module with its refcount incremented.
 */
KMOD_EXPORT struct kmod_module *kmod_module_ref(struct kmod_module *mod)
{
	if (mod == NULL)
		return NULL;

	mod->refcount++;

	return mod;
}

#define CHECK_ERR_AND_FINISH(_err, _label_err, _list, label_finish)	\
	do {								\
		if ((_err) < 0)						\
			goto _label_err;				\
		if (*(_list) != NULL)					\
			goto finish;					\
	} while (0)

/**
 * kmod_module_new_from_lookup:
 * @ctx: kmod library context
 * @given_alias: alias to look for
 * @list: an empty list where to save the list of modules matching
 * @given_alias
 *
 * Create a new list of kmod modules using an alias or module name and lookup
 * libkmod's configuration files and indexes in order to find the module.
 * Once it's found in one of the places, it stops searching and create the
 * list of modules that is saved in @list.
 *
 * The search order is: 1. aliases in configuration file; 2. module names in
 * modules.dep index; 3. symbol aliases in modules.symbols index; 4. aliases
 * in modules.alias index.
 *
 * The initial refcount is 1, and needs to be decremented to release the
 * resources of the kmod_module. The returned @list must be released by
 * calling kmod_module_unref_list(). Since libkmod keeps track of all
 * kmod_modules created, they are all released upon @ctx destruction too. Do
 * not unref @ctx before all the desired operations with the returned list are
 * completed.
 *
 * Returns: 0 on success or < 0 otherwise. It fails if any of the lookup
 * methods failed, which is basically due to memory allocation fail. If module
 * is not found, it still returns 0, but @list is an empty list.
 */
KMOD_EXPORT int kmod_module_new_from_lookup(struct kmod_ctx *ctx,
						const char *given_alias,
						struct kmod_list **list)
{
	int err;
	char alias[PATH_MAX];

	if (ctx == NULL || given_alias == NULL)
		return -ENOENT;

	if (list == NULL || *list != NULL) {
		ERR(ctx, "An empty list is needed to create lookup\n");
		return -ENOSYS;
	}

	if (alias_normalize(given_alias, alias, NULL) < 0) {
		DBG(ctx, "invalid alias: %s\n", given_alias);
		return -EINVAL;
	}

	DBG(ctx, "input alias=%s, normalized=%s\n", given_alias, alias);

	/* Aliases from config file override all the others */
	err = kmod_lookup_alias_from_config(ctx, alias, list);
	CHECK_ERR_AND_FINISH(err, fail, list, finish);

	DBG(ctx, "lookup modules.dep %s\n", alias);
	err = kmod_lookup_alias_from_moddep_file(ctx, alias, list);
	CHECK_ERR_AND_FINISH(err, fail, list, finish);

	DBG(ctx, "lookup modules.symbols %s\n", alias);
	err = kmod_lookup_alias_from_symbols_file(ctx, alias, list);
	CHECK_ERR_AND_FINISH(err, fail, list, finish);

	DBG(ctx, "lookup install and remove commands %s\n", alias);
	err = kmod_lookup_alias_from_commands(ctx, alias, list);
	CHECK_ERR_AND_FINISH(err, fail, list, finish);

	DBG(ctx, "lookup modules.aliases %s\n", alias);
	err = kmod_lookup_alias_from_aliases_file(ctx, alias, list);
	CHECK_ERR_AND_FINISH(err, fail, list, finish);

	DBG(ctx, "lookup modules.builtin %s\n", alias);
	err = kmod_lookup_alias_from_builtin_file(ctx, alias, list);
	CHECK_ERR_AND_FINISH(err, fail, list, finish);

finish:
	DBG(ctx, "lookup %s=%d, list=%p\n", alias, err, *list);
	return err;
fail:
	DBG(ctx, "Failed to lookup %s\n", alias);
	kmod_module_unref_list(*list);
	*list = NULL;
	return err;
}
#undef CHECK_ERR_AND_FINISH

/**
 * kmod_module_unref_list:
 * @list: list of kmod modules
 *
 * Drop a reference of each kmod module in @list and releases the resources
 * taken by the list itself.
 *
 * Returns: 0
 */
KMOD_EXPORT int kmod_module_unref_list(struct kmod_list *list)
{
	for (; list != NULL; list = kmod_list_remove(list))
		kmod_module_unref(list->data);

	return 0;
}

/**
 * kmod_module_get_filtered_blacklist:
 * @ctx: kmod library context
 * @input: list of kmod_module to be filtered with blacklist
 * @output: where to save the new list
 *
 * This function should not be used. Use kmod_module_apply_filter instead.
 *
 * Given a list @input, this function filter it out with config's blacklist
 * and save it in @output.
 *
 * Returns: 0 on success or < 0 otherwise. @output is saved with the updated
 * list.
 */
KMOD_EXPORT int kmod_module_get_filtered_blacklist(const struct kmod_ctx *ctx,
						const struct kmod_list *input,
						struct kmod_list **output)
{
	return kmod_module_apply_filter(ctx, KMOD_FILTER_BLACKLIST, input, output);
}

static const struct kmod_list *module_get_dependencies_noref(const struct kmod_module *mod)
{
	if (!mod->init.dep) {
		/* lazy init */
		char *line = kmod_search_moddep(mod->ctx, mod->name);

		if (line == NULL)
			return NULL;

		kmod_module_parse_depline((struct kmod_module *)mod, line);
		free(line);

		if (!mod->init.dep)
			return NULL;
	}

	return mod->dep;
}

/**
 * kmod_module_get_dependencies:
 * @mod: kmod module
 *
 * Search the modules.dep index to find the dependencies of the given @mod.
 * The result is cached in @mod, so subsequent calls to this function will
 * return the already searched list of modules.
 *
 * Returns: NULL on failure. Otherwise it returns a list of kmod modules
 * that can be released by calling kmod_module_unref_list().
 */
KMOD_EXPORT struct kmod_list *kmod_module_get_dependencies(const struct kmod_module *mod)
{
	struct kmod_list *l, *l_new, *list_new = NULL;

	if (mod == NULL)
		return NULL;

	module_get_dependencies_noref(mod);

	kmod_list_foreach(l, mod->dep) {
		l_new = kmod_list_append(list_new, kmod_module_ref(l->data));
		if (l_new == NULL) {
			kmod_module_unref(l->data);
			goto fail;
		}

		list_new = l_new;
	}

	return list_new;

fail:
	ERR(mod->ctx, "out of memory\n");
	kmod_module_unref_list(list_new);
	return NULL;
}

/**
 * kmod_module_get_module:
 * @entry: an entry in a list of kmod modules.
 *
 * Get the kmod module of this @entry in the list, increasing its refcount.
 * After it's used, unref it. Since the refcount is incremented upon return,
 * you still have to call kmod_module_unref_list() to release the list of kmod
 * modules.
 *
 * Returns: NULL on failure or the kmod_module contained in this list entry
 * with its refcount incremented.
 */
KMOD_EXPORT struct kmod_module *kmod_module_get_module(const struct kmod_list *entry)
{
	if (entry == NULL)
		return NULL;

	return kmod_module_ref(entry->data);
}

/**
 * kmod_module_get_name:
 * @mod: kmod module
 *
 * Get the name of this kmod module. Name is always available, independently
 * if it was created by kmod_module_new_from_name() or another function and
 * it's always normalized (dashes are replaced with underscores).
 *
 * Returns: the name of this kmod module.
 */
KMOD_EXPORT const char *kmod_module_get_name(const struct kmod_module *mod)
{
	if (mod == NULL)
		return NULL;

	return mod->name;
}

/**
 * kmod_module_get_path:
 * @mod: kmod module
 *
 * Get the path of this kmod module. If this kmod module was not created by
 * path, it can search the modules.dep index in order to find out the module
 * under context's dirname.
 *
 * Returns: the path of this kmod module or NULL if such information is not
 * available.
 */
KMOD_EXPORT const char *kmod_module_get_path(const struct kmod_module *mod)
{
	char *line;

	if (mod == NULL)
		return NULL;

	DBG(mod->ctx, "name='%s' path='%s'\n", mod->name, mod->path);

	if (mod->path != NULL)
		return mod->path;
	if (mod->init.dep)
		return NULL;

	/* lazy init */
	line = kmod_search_moddep(mod->ctx, mod->name);
	if (line == NULL)
		return NULL;

	kmod_module_parse_depline((struct kmod_module *) mod, line);
	free(line);

	return mod->path;
}


extern long delete_module(const char *name, unsigned int flags);

/**
 * kmod_module_remove_module:
 * @mod: kmod module
 * @flags: flags to pass to Linux kernel when removing the module. The only valid flag is
 * KMOD_REMOVE_FORCE: force remove module regardless if it's still in
 * use by a kernel subsystem or other process;
 * KMOD_REMOVE_NOWAIT is always enforced, causing us to pass O_NONBLOCK to
 * delete_module(2).
 *
 * Remove a module from Linux kernel.
 *
 * Returns: 0 on success or < 0 on failure.
 */
KMOD_EXPORT int kmod_module_remove_module(struct kmod_module *mod,
							unsigned int flags)
{
	int err;

	if (mod == NULL)
		return -ENOENT;

	/* Filter out other flags and force ONONBLOCK */
	flags &= KMOD_REMOVE_FORCE;
	flags |= KMOD_REMOVE_NOWAIT;

	err = delete_module(mod->name, flags);
	if (err != 0) {
		err = -errno;
		ERR(mod->ctx, "could not remove '%s': %m\n", mod->name);
	}

	return err;
}

extern long init_module(const void *mem, unsigned long len, const char *args);

/**
 * kmod_module_insert_module:
 * @mod: kmod module
 * @flags: flags are not passed to Linux Kernel, but instead they dictate the
 * behavior of this function, valid flags are
 * KMOD_INSERT_FORCE_VERMAGIC: ignore kernel version magic;
 * KMOD_INSERT_FORCE_MODVERSION: ignore symbol version hashes.
 * @options: module's options to pass to Linux Kernel.
 *
 * Insert a module in Linux kernel. It opens the file pointed by @mod,
 * mmap'ing it and passing to kernel.
 *
 * Returns: 0 on success or < 0 on failure. If module is already loaded it
 * returns -EEXIST.
 */
KMOD_EXPORT int kmod_module_insert_module(struct kmod_module *mod,
							unsigned int flags,
							const char *options)
{
	int err;
	const void *mem;
	off_t size;
	struct kmod_elf *elf;
	const char *path;
	const char *args = options ? options : "";

	if (mod == NULL)
		return -ENOENT;

	path = kmod_module_get_path(mod);
	if (path == NULL) {
		ERR(mod->ctx, "could not find module by name='%s'\n", mod->name);
		return -ENOSYS;
	}

	mod->file = kmod_file_open(mod->ctx, path);
	if (mod->file == NULL) {
		err = -errno;
		return err;
	}

	if (kmod_file_get_direct(mod->file)) {
		unsigned int kernel_flags = 0;

		if (flags & KMOD_INSERT_FORCE_VERMAGIC)
			kernel_flags |= MODULE_INIT_IGNORE_VERMAGIC;
		if (flags & KMOD_INSERT_FORCE_MODVERSION)
			kernel_flags |= MODULE_INIT_IGNORE_MODVERSIONS;

		err = finit_module(kmod_file_get_fd(mod->file), args, kernel_flags);
		if (err == 0 || errno != ENOSYS)
			goto init_finished;
	}

	if (flags & (KMOD_INSERT_FORCE_VERMAGIC | KMOD_INSERT_FORCE_MODVERSION)) {
		elf = kmod_file_get_elf(mod->file);
		if (elf == NULL) {
			err = -errno;
			return err;
		}

		if (flags & KMOD_INSERT_FORCE_MODVERSION) {
			err = kmod_elf_strip_section(elf, "__versions");
			if (err < 0)
				INFO(mod->ctx, "Failed to strip modversion: %s\n", strerror(-err));
		}

		if (flags & KMOD_INSERT_FORCE_VERMAGIC) {
			err = kmod_elf_strip_vermagic(elf);
			if (err < 0)
				INFO(mod->ctx, "Failed to strip vermagic: %s\n", strerror(-err));
		}

		mem = kmod_elf_get_memory(elf);
	} else {
		mem = kmod_file_get_contents(mod->file);
	}
	size = kmod_file_get_size(mod->file);

	err = init_module(mem, size, args);
init_finished:
	if (err < 0) {
		err = -errno;
		INFO(mod->ctx, "Failed to insert module '%s': %m\n", path);
	}
	return err;
}

static bool module_is_blacklisted(struct kmod_module *mod)
{
	struct kmod_ctx *ctx = mod->ctx;
	const struct kmod_config *config = kmod_get_config(ctx);
	const struct kmod_list *bl = config->blacklists;
	const struct kmod_list *l;

	kmod_list_foreach(l, bl) {
		const char *modname = kmod_blacklist_get_modname(l);

		if (streq(modname, mod->name))
			return true;
	}

	return false;
}

/**
 * kmod_module_apply_filter
 * @ctx: kmod library context
 * @filter_type: bitmask to filter modules out, valid types are
 * KMOD_FILTER_BLACKLIST: filter modules in blacklist out;
 * KMOD_FILTER_BUILTIN: filter builtin modules out.
 * @input: list of kmod_module to be filtered
 * @output: where to save the new list
 *
 * Given a list @input, this function filter it out by the filter mask
 * and save it in @output.
 *
 * Returns: 0 on success or < 0 otherwise. @output is saved with the updated
 * list.
 */
KMOD_EXPORT int kmod_module_apply_filter(const struct kmod_ctx *ctx,
						enum kmod_filter filter_type,
						const struct kmod_list *input,
						struct kmod_list **output)
{
	const struct kmod_list *li;

	if (ctx == NULL || output == NULL)
		return -ENOENT;

	*output = NULL;
	if (input == NULL)
		return 0;

	kmod_list_foreach(li, input) {
		struct kmod_module *mod = li->data;
		struct kmod_list *node;

		if ((filter_type & KMOD_FILTER_BLACKLIST) &&
				module_is_blacklisted(mod))
			continue;

		if ((filter_type & KMOD_FILTER_BUILTIN) && mod->builtin)
			continue;

		node = kmod_list_append(*output, mod);
		if (node == NULL)
			goto fail;

		*output = node;
		kmod_module_ref(mod);
	}

	return 0;

fail:
	kmod_module_unref_list(*output);
	*output = NULL;
	return -ENOMEM;
}

static int command_do(struct kmod_module *mod, const char *type,
							const char *cmd)
{
	const char *modname = kmod_module_get_name(mod);
	int err;

	DBG(mod->ctx, "%s %s\n", type, cmd);

	setenv("MODPROBE_MODULE", modname, 1);
	err = system(cmd);
	unsetenv("MODPROBE_MODULE");

	if (err == -1 || WEXITSTATUS(err)) {
		ERR(mod->ctx, "Error running %s command for %s\n",
								type, modname);
		if (err != -1)
			err = -WEXITSTATUS(err);
	}

	return err;
}

struct probe_insert_cb {
	int (*run_install)(struct kmod_module *m, const char *cmd, void *data);
	void *data;
};

static int module_do_install_commands(struct kmod_module *mod,
					const char *options,
					struct probe_insert_cb *cb)
{
	const char *command = kmod_module_get_install_commands(mod);
	char *p;
	_cleanup_free_ char *cmd;
	int err;
	size_t cmdlen, options_len, varlen;

	assert(command);

	if (options == NULL)
		options = "";

	options_len = strlen(options);
	cmdlen = strlen(command);
	varlen = sizeof("$CMDLINE_OPTS") - 1;

	cmd = memdup(command, cmdlen + 1);
	if (cmd == NULL)
		return -ENOMEM;

	while ((p = strstr(cmd, "$CMDLINE_OPTS")) != NULL) {
		size_t prefixlen = p - cmd;
		size_t suffixlen = cmdlen - prefixlen - varlen;
		size_t slen = cmdlen - varlen + options_len;
		char *suffix = p + varlen;
		char *s = malloc(slen + 1);
		if (!s)
			return -ENOMEM;

		memcpy(s, cmd, p - cmd);
		memcpy(s + prefixlen, options, options_len);
		memcpy(s + prefixlen + options_len, suffix, suffixlen);
		s[slen] = '\0';

		free(cmd);
		cmd = s;
		cmdlen = slen;
	}

	if (cb->run_install != NULL)
		err = cb->run_install(mod, cmd, cb->data);
	else
		err = command_do(mod, "install", cmd);

	return err;
}

static char *module_options_concat(const char *opt, const char *xopt)
{
	// TODO: we might need to check if xopt overrides options on opt
	size_t optlen = opt == NULL ? 0 : strlen(opt);
	size_t xoptlen = xopt == NULL ? 0 : strlen(xopt);
	char *r;

	if (optlen == 0 && xoptlen == 0)
		return NULL;

	r = malloc(optlen + xoptlen + 2);

	if (opt != NULL) {
		memcpy(r, opt, optlen);
		r[optlen] = ' ';
		optlen++;
	}

	if (xopt != NULL)
		memcpy(r + optlen, xopt, xoptlen);

	r[optlen + xoptlen] = '\0';

	return r;
}

static int __kmod_module_get_probe_list(struct kmod_module *mod,
						bool required,
						bool ignorecmd,
						struct kmod_list **list);

/* re-entrant */
static int __kmod_module_fill_softdep(struct kmod_module *mod,
						struct kmod_list **list)
{
	struct kmod_list *pre = NULL, *post = NULL, *l;
	int err;

	err = kmod_module_get_softdeps(mod, &pre, &post);
	if (err < 0) {
		ERR(mod->ctx, "could not get softdep: %s\n",
							strerror(-err));
		goto fail;
	}

	kmod_list_foreach(l, pre) {
		struct kmod_module *m = l->data;
		err = __kmod_module_get_probe_list(m, false, false, list);
		if (err < 0)
			goto fail;
	}

	l = kmod_list_append(*list, kmod_module_ref(mod));
	if (l == NULL) {
		kmod_module_unref(mod);
		err = -ENOMEM;
		goto fail;
	}
	*list = l;
	mod->ignorecmd = (pre != NULL || post != NULL);

	kmod_list_foreach(l, post) {
		struct kmod_module *m = l->data;
		err = __kmod_module_get_probe_list(m, false, false, list);
		if (err < 0)
			goto fail;
	}

fail:
	kmod_module_unref_list(pre);
	kmod_module_unref_list(post);

	return err;
}

/* re-entrant */
static int __kmod_module_get_probe_list(struct kmod_module *mod,
						bool required,
						bool ignorecmd,
						struct kmod_list **list)
{
	struct kmod_list *dep, *l;
	int err = 0;

	if (mod->visited) {
		DBG(mod->ctx, "Ignore module '%s': already visited\n",
								mod->name);
		return 0;
	}
	mod->visited = true;

	dep = kmod_module_get_dependencies(mod);
	if (required) {
		/*
		 * Called from kmod_module_probe_insert_module(); set the
		 * ->required flag on mod and all its dependencies before
		 * they are possibly visited through some softdeps.
		 */
		mod->required = true;
		kmod_list_foreach(l, dep) {
			struct kmod_module *m = l->data;
			m->required = true;
		}
	}

	kmod_list_foreach(l, dep) {
		struct kmod_module *m = l->data;
		err = __kmod_module_fill_softdep(m, list);
		if (err < 0)
			goto finish;
	}

	if (ignorecmd) {
		l = kmod_list_append(*list, kmod_module_ref(mod));
		if (l == NULL) {
			kmod_module_unref(mod);
			err = -ENOMEM;
			goto finish;
		}
		*list = l;
		mod->ignorecmd = true;
	} else
		err = __kmod_module_fill_softdep(mod, list);

finish:
	kmod_module_unref_list(dep);
	return err;
}

static int kmod_module_get_probe_list(struct kmod_module *mod,
						bool ignorecmd,
						struct kmod_list **list)
{
	int err;

	assert(mod != NULL);
	assert(list != NULL && *list == NULL);

	/*
	 * Make sure we don't get screwed by previous calls to this function
	 */
	kmod_set_modules_visited(mod->ctx, false);
	kmod_set_modules_required(mod->ctx, false);

	err = __kmod_module_get_probe_list(mod, true, ignorecmd, list);
	if (err < 0) {
		kmod_module_unref_list(*list);
		*list = NULL;
	}

	return err;
}

/**
 * kmod_module_probe_insert_module:
 * @mod: kmod module
 * @flags: flags are not passed to Linux Kernel, but instead they dictate the
 * behavior of this function, valid flags are
 * KMOD_PROBE_FORCE_VERMAGIC: ignore kernel version magic;
 * KMOD_PROBE_FORCE_MODVERSION: ignore symbol version hashes;
 * KMOD_PROBE_IGNORE_COMMAND: whether the probe should ignore install
 * commands and softdeps configured in the system;
 * KMOD_PROBE_IGNORE_LOADED: do not check whether the module is already
 * live in kernel or not;
 * KMOD_PROBE_DRY_RUN: dry run, do not insert module, just call the
 * associated callback function;
 * KMOD_PROBE_FAIL_ON_LOADED: if KMOD_PROBE_IGNORE_LOADED is not specified
 * and the module is already live in kernel, the function will fail if this
 * flag is specified;
 * KMOD_PROBE_APPLY_BLACKLIST_ALL: probe will apply KMOD_FILTER_BLACKLIST
 * filter to this module and its dependencies. If any of the dependencies (or
 * the module) is blacklisted, the probe will fail, unless the blacklisted
 * module is already live in kernel;
 * KMOD_PROBE_APPLY_BLACKLIST: probe will fail if the module is blacklisted;
 * KMOD_PROBE_APPLY_BLACKLIST_ALIAS_ONLY: probe will fail if the module is an
 * alias and is blacklisted.
 * @extra_options: module's options to pass to Linux Kernel. It applies only
 * to @mod, not to its dependencies.
 * @run_install: function to run when @mod is backed by an install command.
 * @data: data to give back to @run_install callback
 * @print_action: function to call with the action being taken (install or
 * insmod). It's useful for tools like modprobe when running with verbose
 * output or in dry-run mode.
 *
 * Insert a module in Linux kernel resolving dependencies, soft dependencies,
 * install commands and applying blacklist.
 *
 * If @run_install is NULL, this function will fork and exec by calling
 * system(3). Don't pass a NULL argument in @run_install if your binary is
 * setuid/setgid (see warning in system(3)). If you need control over the
 * execution of an install command, give a callback function instead.
 *
 * Returns: 0 on success, > 0 if stopped by a reason given in @flags or < 0 on
 * failure.
 */
KMOD_EXPORT int kmod_module_probe_insert_module(struct kmod_module *mod,
			unsigned int flags, const char *extra_options,
			int (*run_install)(struct kmod_module *m,
						const char *cmd, void *data),
			const void *data,
			void (*print_action)(struct kmod_module *m,
						bool install,
						const char *options))
{
	struct kmod_list *list = NULL, *l;
	struct probe_insert_cb cb;
	int err;

	if (mod == NULL)
		return -ENOENT;

	if (!(flags & KMOD_PROBE_IGNORE_LOADED)
					&& module_is_inkernel(mod)) {
		if (flags & KMOD_PROBE_FAIL_ON_LOADED)
			return -EEXIST;
		else
			return 0;
	}

	/*
	 * Ugly assignement + check. We need to check if we were told to check
	 * blacklist and also return the reason why we failed.
	 * KMOD_PROBE_APPLY_BLACKLIST_ALIAS_ONLY will take effect only if the
	 * module is an alias, so we also need to check it
	 */
	if ((mod->alias != NULL && ((err = flags & KMOD_PROBE_APPLY_BLACKLIST_ALIAS_ONLY)))
			|| (err = flags & KMOD_PROBE_APPLY_BLACKLIST_ALL)
			|| (err = flags & KMOD_PROBE_APPLY_BLACKLIST)) {
		if (module_is_blacklisted(mod))
			return err;
	}

	err = kmod_module_get_probe_list(mod,
				!!(flags & KMOD_PROBE_IGNORE_COMMAND), &list);
	if (err < 0)
		return err;

	if (flags & KMOD_PROBE_APPLY_BLACKLIST_ALL) {
		struct kmod_list *filtered = NULL;

		err = kmod_module_apply_filter(mod->ctx,
				KMOD_FILTER_BLACKLIST, list, &filtered);
		if (err < 0)
			return err;

		kmod_module_unref_list(list);
		if (filtered == NULL)
			return KMOD_PROBE_APPLY_BLACKLIST_ALL;

		list = filtered;
	}

	cb.run_install = run_install;
	cb.data = (void *) data;

	kmod_list_foreach(l, list) {
		struct kmod_module *m = l->data;
		const char *moptions = kmod_module_get_options(m);
		const char *cmd = kmod_module_get_install_commands(m);
		char *options;

		if (!(flags & KMOD_PROBE_IGNORE_LOADED)
						&& module_is_inkernel(m)) {
			DBG(mod->ctx, "Ignoring module '%s': already loaded\n",
								m->name);
			err = -EEXIST;
			goto finish_module;
		}

		options = module_options_concat(moptions,
					m == mod ? extra_options : NULL);

		if (cmd != NULL && !m->ignorecmd) {
			if (print_action != NULL)
				print_action(m, true, options ?: "");

			if (!(flags & KMOD_PROBE_DRY_RUN))
				err = module_do_install_commands(m, options,
									&cb);
		} else {
			if (print_action != NULL)
				print_action(m, false, options ?: "");

			if (!(flags & KMOD_PROBE_DRY_RUN))
				err = kmod_module_insert_module(m, flags,
								options);
		}

		free(options);

finish_module:
		/*
		 * Treat "already loaded" error. If we were told to stop on
		 * already loaded and the module being loaded is not a softdep
		 * or dep, bail out. Otherwise, just ignore and continue.
		 *
		 * We need to check here because of race conditions. We
		 * checked first if module was already loaded but it may have
		 * been loaded between the check and the moment we try to
		 * insert it.
		 */
		if (err == -EEXIST && m == mod &&
				(flags & KMOD_PROBE_FAIL_ON_LOADED))
			break;

		/*
		 * Ignore errors from softdeps
		 */
		if (err == -EEXIST || !m->required)
			err = 0;

		else if (err < 0)
			break;
	}

	kmod_module_unref_list(list);
	return err;
}

/**
 * kmod_module_get_options:
 * @mod: kmod module
 *
 * Get options of this kmod module. Options come from the configuration file
 * and are cached in @mod. The first call to this function will search for
 * this module in configuration and subsequent calls return the cached string.
 *
 * Returns: a string with all the options separated by spaces. This string is
 * owned by @mod, do not free it.
 */
KMOD_EXPORT const char *kmod_module_get_options(const struct kmod_module *mod)
{
	if (mod == NULL)
		return NULL;

	if (!mod->init.options) {
		/* lazy init */
		struct kmod_module *m = (struct kmod_module *)mod;
		const struct kmod_list *l;
		const struct kmod_config *config;
		char *opts = NULL;
		size_t optslen = 0;

		config = kmod_get_config(mod->ctx);

		kmod_list_foreach(l, config->options) {
			const char *modname = kmod_option_get_modname(l);
			const char *str;
			size_t len;
			void *tmp;

			DBG(mod->ctx, "modname=%s mod->name=%s mod->alias=%s\n", modname, mod->name, mod->alias);
			if (!(streq(modname, mod->name) || (mod->alias != NULL &&
						streq(modname, mod->alias))))
				continue;

			DBG(mod->ctx, "passed = modname=%s mod->name=%s mod->alias=%s\n", modname, mod->name, mod->alias);
			str = kmod_option_get_options(l);
			len = strlen(str);
			if (len < 1)
				continue;

			tmp = realloc(opts, optslen + len + 2);
			if (tmp == NULL) {
				free(opts);
				goto failed;
			}

			opts = tmp;

			if (optslen > 0) {
				opts[optslen] = ' ';
				optslen++;
			}

			memcpy(opts + optslen, str, len);
			optslen += len;
			opts[optslen] = '\0';
		}

		m->init.options = true;
		m->options = opts;
	}

	return mod->options;

failed:
	ERR(mod->ctx, "out of memory\n");
	return NULL;
}

/**
 * kmod_module_get_install_commands:
 * @mod: kmod module
 *
 * Get install commands for this kmod module. Install commands come from the
 * configuration file and are cached in @mod. The first call to this function
 * will search for this module in configuration and subsequent calls return
 * the cached string. The install commands are returned as they were in the
 * configuration, concatenated by ';'. No other processing is made in this
 * string.
 *
 * Returns: a string with all install commands separated by semicolons. This
 * string is owned by @mod, do not free it.
 */
KMOD_EXPORT const char *kmod_module_get_install_commands(const struct kmod_module *mod)
{
	if (mod == NULL)
		return NULL;

	if (!mod->init.install_commands) {
		/* lazy init */
		struct kmod_module *m = (struct kmod_module *)mod;
		const struct kmod_list *l;
		const struct kmod_config *config;

		config = kmod_get_config(mod->ctx);

		kmod_list_foreach(l, config->install_commands) {
			const char *modname = kmod_command_get_modname(l);

			if (fnmatch(modname, mod->name, 0) != 0)
				continue;

			m->install_commands = kmod_command_get_command(l);

			/*
			 * find only the first command, as modprobe from
			 * module-init-tools does
			 */
			break;
		}

		m->init.install_commands = true;
	}

	return mod->install_commands;
}

void kmod_module_set_install_commands(struct kmod_module *mod, const char *cmd)
{
	mod->init.install_commands = true;
	mod->install_commands = cmd;
}

static struct kmod_list *lookup_softdep(struct kmod_ctx *ctx, const char * const * array, unsigned int count)
{
	struct kmod_list *ret = NULL;
	unsigned i;

	for (i = 0; i < count; i++) {
		const char *depname = array[i];
		struct kmod_list *lst = NULL;
		int err;

		err = kmod_module_new_from_lookup(ctx, depname, &lst);
		if (err < 0) {
			ERR(ctx, "failed to lookup soft dependency '%s', continuing anyway.\n", depname);
			continue;
		} else if (lst != NULL)
			ret = kmod_list_append_list(ret, lst);
	}
	return ret;
}

/**
 * kmod_module_get_softdeps:
 * @mod: kmod module
 * @pre: where to save the list of preceding soft dependencies.
 * @post: where to save the list of post soft dependencies.
 *
 * Get soft dependencies for this kmod module. Soft dependencies come
 * from configuration file and are not cached in @mod because it may include
 * dependency cycles that would make we leak kmod_module. Any call
 * to this function will search for this module in configuration, allocate a
 * list and return the result.
 *
 * Both @pre and @post are newly created list of kmod_module and
 * should be unreferenced with kmod_module_unref_list().
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT int kmod_module_get_softdeps(const struct kmod_module *mod,
						struct kmod_list **pre,
						struct kmod_list **post)
{
	const struct kmod_list *l;
	const struct kmod_config *config;

	if (mod == NULL || pre == NULL || post == NULL)
		return -ENOENT;

	assert(*pre == NULL);
	assert(*post == NULL);

	config = kmod_get_config(mod->ctx);

	kmod_list_foreach(l, config->softdeps) {
		const char *modname = kmod_softdep_get_name(l);
		const char * const *array;
		unsigned count;

		if (fnmatch(modname, mod->name, 0) != 0)
			continue;

		array = kmod_softdep_get_pre(l, &count);
		*pre = lookup_softdep(mod->ctx, array, count);
		array = kmod_softdep_get_post(l, &count);
		*post = lookup_softdep(mod->ctx, array, count);

		/*
		 * find only the first command, as modprobe from
		 * module-init-tools does
		 */
		break;
	}

	return 0;
}

/**
 * kmod_module_get_remove_commands:
 * @mod: kmod module
 *
 * Get remove commands for this kmod module. Remove commands come from the
 * configuration file and are cached in @mod. The first call to this function
 * will search for this module in configuration and subsequent calls return
 * the cached string. The remove commands are returned as they were in the
 * configuration, concatenated by ';'. No other processing is made in this
 * string.
 *
 * Returns: a string with all remove commands separated by semicolons. This
 * string is owned by @mod, do not free it.
 */
KMOD_EXPORT const char *kmod_module_get_remove_commands(const struct kmod_module *mod)
{
	if (mod == NULL)
		return NULL;

	if (!mod->init.remove_commands) {
		/* lazy init */
		struct kmod_module *m = (struct kmod_module *)mod;
		const struct kmod_list *l;
		const struct kmod_config *config;

		config = kmod_get_config(mod->ctx);

		kmod_list_foreach(l, config->remove_commands) {
			const char *modname = kmod_command_get_modname(l);

			if (fnmatch(modname, mod->name, 0) != 0)
				continue;

			m->remove_commands = kmod_command_get_command(l);

			/*
			 * find only the first command, as modprobe from
			 * module-init-tools does
			 */
			break;
		}

		m->init.remove_commands = true;
	}

	return mod->remove_commands;
}

void kmod_module_set_remove_commands(struct kmod_module *mod, const char *cmd)
{
	mod->init.remove_commands = true;
	mod->remove_commands = cmd;
}

/**
 * SECTION:libkmod-loaded
 * @short_description: currently loaded modules
 *
 * Information about currently loaded modules, as reported by Linux kernel.
 * These information are not cached by libkmod and are always read from /sys
 * and /proc/modules.
 */

/**
 * kmod_module_new_from_loaded:
 * @ctx: kmod library context
 * @list: where to save the list of loaded modules
 *
 * Create a new list of kmod modules with all modules currently loaded in
 * kernel. It uses /proc/modules to get the names of loaded modules and to
 * create kmod modules by calling kmod_module_new_from_name() in each of them.
 * They are put in @list in no particular order.
 *
 * The initial refcount is 1, and needs to be decremented to release the
 * resources of the kmod_module. The returned @list must be released by
 * calling kmod_module_unref_list(). Since libkmod keeps track of all
 * kmod_modules created, they are all released upon @ctx destruction too. Do
 * not unref @ctx before all the desired operations with the returned list are
 * completed.
 *
 * Returns: 0 on success or < 0 on error.
 */
KMOD_EXPORT int kmod_module_new_from_loaded(struct kmod_ctx *ctx,
						struct kmod_list **list)
{
	struct kmod_list *l = NULL;
	FILE *fp;
	char line[4096];

	if (ctx == NULL || list == NULL)
		return -ENOENT;

	fp = fopen("/proc/modules", "re");
	if (fp == NULL) {
		int err = -errno;
		ERR(ctx, "could not open /proc/modules: %s\n", strerror(errno));
		return err;
	}

	while (fgets(line, sizeof(line), fp)) {
		struct kmod_module *m;
		struct kmod_list *node;
		int err;
		char *saveptr, *name = strtok_r(line, " \t", &saveptr);

		err = kmod_module_new_from_name(ctx, name, &m);
		if (err < 0) {
			ERR(ctx, "could not get module from name '%s': %s\n",
				name, strerror(-err));
			continue;
		}

		node = kmod_list_append(l, m);
		if (node)
			l = node;
		else {
			ERR(ctx, "out of memory\n");
			kmod_module_unref(m);
		}
	}

	fclose(fp);
	*list = l;

	return 0;
}

/**
 * kmod_module_initstate_str:
 * @state: the state as returned by kmod_module_get_initstate()
 *
 * Translate a initstate to a string.
 *
 * Returns: the string associated to the @state. This string is statically
 * allocated, do not free it.
 */
KMOD_EXPORT const char *kmod_module_initstate_str(enum kmod_module_initstate state)
{
	switch (state) {
	case KMOD_MODULE_BUILTIN:
		return "builtin";
	case KMOD_MODULE_LIVE:
		return "live";
	case KMOD_MODULE_COMING:
		return "coming";
	case KMOD_MODULE_GOING:
		return "going";
	default:
		return NULL;
	}
}

/**
 * kmod_module_get_initstate:
 * @mod: kmod module
 *
 * Get the initstate of this @mod, as returned by Linux Kernel, by reading
 * /sys filesystem.
 *
 * Returns: < 0 on error or module state if module is found in kernel, valid states are
 * KMOD_MODULE_BUILTIN: module is builtin;
 * KMOD_MODULE_LIVE: module is live in kernel;
 * KMOD_MODULE_COMING: module is being loaded;
 * KMOD_MODULE_GOING: module is being unloaded.
 */
KMOD_EXPORT int kmod_module_get_initstate(const struct kmod_module *mod)
{
	char path[PATH_MAX], buf[32];
	int fd, err, pathlen;

	if (mod == NULL)
		return -ENOENT;

	if (mod->builtin)
		return KMOD_MODULE_BUILTIN;

	pathlen = snprintf(path, sizeof(path),
				"/sys/module/%s/initstate", mod->name);
	fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd < 0) {
		err = -errno;

		DBG(mod->ctx, "could not open '%s': %s\n",
			path, strerror(-err));

		if (pathlen > (int)sizeof("/initstate") - 1) {
			struct stat st;
			path[pathlen - (sizeof("/initstate") - 1)] = '\0';
			if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
				return KMOD_MODULE_BUILTIN;
		}

		DBG(mod->ctx, "could not open '%s': %s\n",
			path, strerror(-err));
		return err;
	}

	err = read_str_safe(fd, buf, sizeof(buf));
	close(fd);
	if (err < 0) {
		ERR(mod->ctx, "could not read from '%s': %s\n",
			path, strerror(-err));
		return err;
	}

	if (streq(buf, "live\n"))
		return KMOD_MODULE_LIVE;
	else if (streq(buf, "coming\n"))
		return KMOD_MODULE_COMING;
	else if (streq(buf, "going\n"))
		return KMOD_MODULE_GOING;

	ERR(mod->ctx, "unknown %s: '%s'\n", path, buf);
	return -EINVAL;
}

/**
 * kmod_module_get_size:
 * @mod: kmod module
 *
 * Get the size of this kmod module as returned by Linux kernel. If supported,
 * the size is read from the coresize attribute in /sys/module. For older
 * kernels, this falls back on /proc/modules and searches for the specified
 * module to get its size.
 *
 * Returns: the size of this kmod module.
 */
KMOD_EXPORT long kmod_module_get_size(const struct kmod_module *mod)
{
	FILE *fp;
	char line[4096];
	int lineno = 0;
	long size = -ENOENT;
	int dfd, cfd;

	if (mod == NULL)
		return -ENOENT;

	/* try to open the module dir in /sys. If this fails, don't
	 * bother trying to find the size as we know the module isn't
	 * loaded.
	 */
	snprintf(line, sizeof(line), "/sys/module/%s", mod->name);
	dfd = open(line, O_RDONLY);
	if (dfd < 0)
		return -errno;

	/* available as of linux 3.3.x */
	cfd = openat(dfd, "coresize", O_RDONLY|O_CLOEXEC);
	if (cfd >= 0) {
		if (read_str_long(cfd, &size, 10) < 0)
			ERR(mod->ctx, "failed to read coresize from %s\n", line);
		close(cfd);
		goto done;
	}

	/* fall back on parsing /proc/modules */
	fp = fopen("/proc/modules", "re");
	if (fp == NULL) {
		int err = -errno;
		ERR(mod->ctx,
		    "could not open /proc/modules: %s\n", strerror(errno));
		close(dfd);
		return err;
	}

	while (fgets(line, sizeof(line), fp)) {
		char *saveptr, *endptr, *tok = strtok_r(line, " \t", &saveptr);
		long value;

		lineno++;
		if (tok == NULL || !streq(tok, mod->name))
			continue;

		tok = strtok_r(NULL, " \t", &saveptr);
		if (tok == NULL) {
			ERR(mod->ctx,
			"invalid line format at /proc/modules:%d\n", lineno);
			break;
		}

		value = strtol(tok, &endptr, 10);
		if (endptr == tok || *endptr != '\0') {
			ERR(mod->ctx,
			"invalid line format at /proc/modules:%d\n", lineno);
			break;
		}

		size = value;
		break;
	}
	fclose(fp);

done:
	close(dfd);
	return size;
}

/**
 * kmod_module_get_refcnt:
 * @mod: kmod module
 *
 * Get the ref count of this @mod, as returned by Linux Kernel, by reading
 * /sys filesystem.
 *
 * Returns: 0 on success or < 0 on failure.
 */
KMOD_EXPORT int kmod_module_get_refcnt(const struct kmod_module *mod)
{
	char path[PATH_MAX];
	long refcnt;
	int fd, err;

	if (mod == NULL)
		return -ENOENT;

	snprintf(path, sizeof(path), "/sys/module/%s/refcnt", mod->name);
	fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd < 0) {
		err = -errno;
		DBG(mod->ctx, "could not open '%s': %s\n",
			path, strerror(errno));
		return err;
	}

	err = read_str_long(fd, &refcnt, 10);
	close(fd);
	if (err < 0) {
		ERR(mod->ctx, "could not read integer from '%s': '%s'\n",
			path, strerror(-err));
		return err;
	}

	return (int)refcnt;
}

/**
 * kmod_module_get_holders:
 * @mod: kmod module
 *
 * Get a list of kmod modules that are holding this @mod, as returned by Linux
 * Kernel. After use, free the @list by calling kmod_module_unref_list().
 *
 * Returns: a new list of kmod modules on success or NULL on failure.
 */
KMOD_EXPORT struct kmod_list *kmod_module_get_holders(const struct kmod_module *mod)
{
	char dname[PATH_MAX];
	struct kmod_list *list = NULL;
	struct dirent *dent;
	DIR *d;

	if (mod == NULL || mod->ctx == NULL)
		return NULL;

	snprintf(dname, sizeof(dname), "/sys/module/%s/holders", mod->name);

	d = opendir(dname);
	if (d == NULL) {
		ERR(mod->ctx, "could not open '%s': %s\n",
						dname, strerror(errno));
		return NULL;
	}

	for (dent = readdir(d); dent != NULL; dent = readdir(d)) {
		struct kmod_module *holder;
		struct kmod_list *l;
		int err;

		if (dent->d_name[0] == '.') {
			if (dent->d_name[1] == '\0' ||
			    (dent->d_name[1] == '.' && dent->d_name[2] == '\0'))
				continue;
		}

		err = kmod_module_new_from_name(mod->ctx, dent->d_name,
						&holder);
		if (err < 0) {
			ERR(mod->ctx, "could not create module for '%s': %s\n",
				dent->d_name, strerror(-err));
			goto fail;
		}

		l = kmod_list_append(list, holder);
		if (l != NULL) {
			list = l;
		} else {
			ERR(mod->ctx, "out of memory\n");
			kmod_module_unref(holder);
			goto fail;
		}
	}

	closedir(d);
	return list;

fail:
	closedir(d);
	kmod_module_unref_list(list);
	return NULL;
}

struct kmod_module_section {
	unsigned long address;
	char name[];
};

static void kmod_module_section_free(struct kmod_module_section *section)
{
	free(section);
}

/**
 * kmod_module_get_sections:
 * @mod: kmod module
 *
 * Get a list of kmod sections of this @mod, as returned by Linux Kernel. The
 * structure contained in this list is internal to libkmod and their fields
 * can be obtained by calling kmod_module_section_get_name() and
 * kmod_module_section_get_address().
 *
 * After use, free the @list by calling kmod_module_section_free_list().
 *
 * Returns: a new list of kmod module sections on success or NULL on failure.
 */
KMOD_EXPORT struct kmod_list *kmod_module_get_sections(const struct kmod_module *mod)
{
	char dname[PATH_MAX];
	struct kmod_list *list = NULL;
	struct dirent *dent;
	DIR *d;
	int dfd;

	if (mod == NULL)
		return NULL;

	snprintf(dname, sizeof(dname), "/sys/module/%s/sections", mod->name);

	d = opendir(dname);
	if (d == NULL) {
		ERR(mod->ctx, "could not open '%s': %s\n",
			dname, strerror(errno));
		return NULL;
	}

	dfd = dirfd(d);

	for (dent = readdir(d); dent; dent = readdir(d)) {
		struct kmod_module_section *section;
		struct kmod_list *l;
		unsigned long address;
		size_t namesz;
		int fd, err;

		if (dent->d_name[0] == '.') {
			if (dent->d_name[1] == '\0' ||
			    (dent->d_name[1] == '.' && dent->d_name[2] == '\0'))
				continue;
		}

		fd = openat(dfd, dent->d_name, O_RDONLY|O_CLOEXEC);
		if (fd < 0) {
			ERR(mod->ctx, "could not open '%s/%s': %m\n",
							dname, dent->d_name);
			goto fail;
		}

		err = read_str_ulong(fd, &address, 16);
		close(fd);
		if (err < 0) {
			ERR(mod->ctx, "could not read long from '%s/%s': %m\n",
							dname, dent->d_name);
			goto fail;
		}

		namesz = strlen(dent->d_name) + 1;
		section = malloc(sizeof(*section) + namesz);

		if (section == NULL) {
			ERR(mod->ctx, "out of memory\n");
			goto fail;
		}

		section->address = address;
		memcpy(section->name, dent->d_name, namesz);

		l = kmod_list_append(list, section);
		if (l != NULL) {
			list = l;
		} else {
			ERR(mod->ctx, "out of memory\n");
			free(section);
			goto fail;
		}
	}

	closedir(d);
	return list;

fail:
	closedir(d);
	kmod_module_unref_list(list);
	return NULL;
}

/**
 * kmod_module_section_get_module_name:
 * @entry: a list entry representing a kmod module section
 *
 * Get the name of a kmod module section.
 *
 * After use, free the @list by calling kmod_module_section_free_list().
 *
 * Returns: the name of this kmod module section on success or NULL on
 * failure. The string is owned by the section, do not free it.
 */
KMOD_EXPORT const char *kmod_module_section_get_name(const struct kmod_list *entry)
{
	struct kmod_module_section *section;

	if (entry == NULL)
		return NULL;

	section = entry->data;
	return section->name;
}

/**
 * kmod_module_section_get_address:
 * @entry: a list entry representing a kmod module section
 *
 * Get the address of a kmod module section.
 *
 * After use, free the @list by calling kmod_module_section_free_list().
 *
 * Returns: the address of this kmod module section on success or ULONG_MAX
 * on failure.
 */
KMOD_EXPORT unsigned long kmod_module_section_get_address(const struct kmod_list *entry)
{
	struct kmod_module_section *section;

	if (entry == NULL)
		return (unsigned long)-1;

	section = entry->data;
	return section->address;
}

/**
 * kmod_module_section_free_list:
 * @list: kmod module section list
 *
 * Release the resources taken by @list
 */
KMOD_EXPORT void kmod_module_section_free_list(struct kmod_list *list)
{
	while (list) {
		kmod_module_section_free(list->data);
		list = kmod_list_remove(list);
	}
}

static struct kmod_elf *kmod_module_get_elf(const struct kmod_module *mod)
{
	if (mod->file == NULL) {
		const char *path = kmod_module_get_path(mod);

		if (path == NULL) {
			errno = ENOENT;
			return NULL;
		}

		((struct kmod_module *)mod)->file = kmod_file_open(mod->ctx,
									path);
		if (mod->file == NULL)
			return NULL;
	}

	return kmod_file_get_elf(mod->file);
}

struct kmod_module_info {
	char *key;
	char value[];
};

static struct kmod_module_info *kmod_module_info_new(const char *key, size_t keylen, const char *value, size_t valuelen)
{
	struct kmod_module_info *info;

	info = malloc(sizeof(struct kmod_module_info) + keylen + valuelen + 2);
	if (info == NULL)
		return NULL;

	info->key = (char *)info + sizeof(struct kmod_module_info)
		    + valuelen + 1;
	memcpy(info->key, key, keylen);
	info->key[keylen] = '\0';
	memcpy(info->value, value, valuelen);
	info->value[valuelen] = '\0';
	return info;
}

static void kmod_module_info_free(struct kmod_module_info *info)
{
	free(info);
}

static struct kmod_list *kmod_module_info_append(struct kmod_list **list, const char *key, size_t keylen, const char *value, size_t valuelen)
{
	struct kmod_module_info *info;
	struct kmod_list *n;

	info = kmod_module_info_new(key, keylen, value, valuelen);
	if (info == NULL)
		return NULL;
	n = kmod_list_append(*list, info);
	if (n != NULL)
		*list = n;
	else
		kmod_module_info_free(info);
	return n;
}

/**
 * kmod_module_get_info:
 * @mod: kmod module
 * @list: where to return list of module information. Use
 *        kmod_module_info_get_key() and
 *        kmod_module_info_get_value(). Release this list with
 *        kmod_module_info_free_list()
 *
 * Get a list of entries in ELF section ".modinfo", these contain
 * alias, license, depends, vermagic and other keys with respective
 * values. If the module is signed (CONFIG_MODULE_SIG), information
 * about the module signature is included as well: signer,
 * sig_key and sig_hashalgo.
 *
 * After use, free the @list by calling kmod_module_info_free_list().
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT int kmod_module_get_info(const struct kmod_module *mod, struct kmod_list **list)
{
	struct kmod_elf *elf;
	char **strings;
	int i, count, ret = -ENOMEM;
	struct kmod_signature_info sig_info;

	if (mod == NULL || list == NULL)
		return -ENOENT;

	assert(*list == NULL);

	elf = kmod_module_get_elf(mod);
	if (elf == NULL)
		return -errno;

	count = kmod_elf_get_strings(elf, ".modinfo", &strings);
	if (count < 0)
		return count;

	for (i = 0; i < count; i++) {
		struct kmod_list *n;
		const char *key, *value;
		size_t keylen, valuelen;

		key = strings[i];
		value = strchr(key, '=');
		if (value == NULL) {
			keylen = strlen(key);
			valuelen = 0;
			value = key;
		} else {
			keylen = value - key;
			value++;
			valuelen = strlen(value);
		}

		n = kmod_module_info_append(list, key, keylen, value, valuelen);
		if (n == NULL)
			goto list_error;
	}

	if (kmod_module_signature_info(mod->file, &sig_info)) {
		struct kmod_list *n;
		char *key_hex;

		n = kmod_module_info_append(list, "signer", strlen("signer"),
				sig_info.signer, sig_info.signer_len);
		if (n == NULL)
			goto list_error;
		count++;

		/* Display the key id as 01:12:DE:AD:BE:EF:... */
		key_hex = malloc(sig_info.key_id_len * 3);
		if (key_hex == NULL)
			goto list_error;
		for (i = 0; i < (int)sig_info.key_id_len; i++) {
			sprintf(key_hex + i * 3, "%02X",
					(unsigned char)sig_info.key_id[i]);
			if (i < (int)sig_info.key_id_len - 1)
				key_hex[i * 3 + 2] = ':';
		}
		n = kmod_module_info_append(list, "sig_key", strlen("sig_key"),
				key_hex, sig_info.key_id_len * 3 - 1);
		free(key_hex);
		if (n == NULL)
			goto list_error;
		count++;

		n = kmod_module_info_append(list,
				"sig_hashalgo", strlen("sig_hashalgo"),
				sig_info.hash_algo, strlen(sig_info.hash_algo));
		if (n == NULL)
			goto list_error;
		count++;

		/*
		 * Omit sig_info.id_type and sig_info.algo for now, as these
		 * are currently constant.
		 */
	}
	ret = count;

list_error:
	if (ret < 0) {
		kmod_module_info_free_list(*list);
		*list = NULL;
	}
	free(strings);
	return ret;
}

/**
 * kmod_module_info_get_key:
 * @entry: a list entry representing a kmod module info
 *
 * Get the key of a kmod module info.
 *
 * Returns: the key of this kmod module info on success or NULL on
 * failure. The string is owned by the info, do not free it.
 */
KMOD_EXPORT const char *kmod_module_info_get_key(const struct kmod_list *entry)
{
	struct kmod_module_info *info;

	if (entry == NULL)
		return NULL;

	info = entry->data;
	return info->key;
}

/**
 * kmod_module_info_get_value:
 * @entry: a list entry representing a kmod module info
 *
 * Get the value of a kmod module info.
 *
 * Returns: the value of this kmod module info on success or NULL on
 * failure. The string is owned by the info, do not free it.
 */
KMOD_EXPORT const char *kmod_module_info_get_value(const struct kmod_list *entry)
{
	struct kmod_module_info *info;

	if (entry == NULL)
		return NULL;

	info = entry->data;
	return info->value;
}

/**
 * kmod_module_info_free_list:
 * @list: kmod module info list
 *
 * Release the resources taken by @list
 */
KMOD_EXPORT void kmod_module_info_free_list(struct kmod_list *list)
{
	while (list) {
		kmod_module_info_free(list->data);
		list = kmod_list_remove(list);
	}
}

struct kmod_module_version {
	uint64_t crc;
	char symbol[];
};

static struct kmod_module_version *kmod_module_versions_new(uint64_t crc, const char *symbol)
{
	struct kmod_module_version *mv;
	size_t symbollen = strlen(symbol) + 1;

	mv = malloc(sizeof(struct kmod_module_version) + symbollen);
	if (mv == NULL)
		return NULL;

	mv->crc = crc;
	memcpy(mv->symbol, symbol, symbollen);
	return mv;
}

static void kmod_module_version_free(struct kmod_module_version *version)
{
	free(version);
}

/**
 * kmod_module_get_versions:
 * @mod: kmod module
 * @list: where to return list of module versions. Use
 *        kmod_module_version_get_symbol() and
 *        kmod_module_version_get_crc(). Release this list with
 *        kmod_module_versions_free_list()
 *
 * Get a list of entries in ELF section "__versions".
 *
 * After use, free the @list by calling kmod_module_versions_free_list().
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT int kmod_module_get_versions(const struct kmod_module *mod, struct kmod_list **list)
{
	struct kmod_elf *elf;
	struct kmod_modversion *versions;
	int i, count, ret = 0;

	if (mod == NULL || list == NULL)
		return -ENOENT;

	assert(*list == NULL);

	elf = kmod_module_get_elf(mod);
	if (elf == NULL)
		return -errno;

	count = kmod_elf_get_modversions(elf, &versions);
	if (count < 0)
		return count;

	for (i = 0; i < count; i++) {
		struct kmod_module_version *mv;
		struct kmod_list *n;

		mv = kmod_module_versions_new(versions[i].crc, versions[i].symbol);
		if (mv == NULL) {
			ret = -errno;
			kmod_module_versions_free_list(*list);
			*list = NULL;
			goto list_error;
		}

		n = kmod_list_append(*list, mv);
		if (n != NULL)
			*list = n;
		else {
			kmod_module_version_free(mv);
			kmod_module_versions_free_list(*list);
			*list = NULL;
			ret = -ENOMEM;
			goto list_error;
		}
	}
	ret = count;

list_error:
	free(versions);
	return ret;
}

/**
 * kmod_module_version_get_symbol:
 * @entry: a list entry representing a kmod module versions
 *
 * Get the symbol of a kmod module versions.
 *
 * Returns: the symbol of this kmod module versions on success or NULL
 * on failure. The string is owned by the versions, do not free it.
 */
KMOD_EXPORT const char *kmod_module_version_get_symbol(const struct kmod_list *entry)
{
	struct kmod_module_version *version;

	if (entry == NULL)
		return NULL;

	version = entry->data;
	return version->symbol;
}

/**
 * kmod_module_version_get_crc:
 * @entry: a list entry representing a kmod module version
 *
 * Get the crc of a kmod module version.
 *
 * Returns: the crc of this kmod module version on success or NULL on
 * failure. The string is owned by the version, do not free it.
 */
KMOD_EXPORT uint64_t kmod_module_version_get_crc(const struct kmod_list *entry)
{
	struct kmod_module_version *version;

	if (entry == NULL)
		return 0;

	version = entry->data;
	return version->crc;
}

/**
 * kmod_module_versions_free_list:
 * @list: kmod module versions list
 *
 * Release the resources taken by @list
 */
KMOD_EXPORT void kmod_module_versions_free_list(struct kmod_list *list)
{
	while (list) {
		kmod_module_version_free(list->data);
		list = kmod_list_remove(list);
	}
}

struct kmod_module_symbol {
	uint64_t crc;
	char symbol[];
};

static struct kmod_module_symbol *kmod_module_symbols_new(uint64_t crc, const char *symbol)
{
	struct kmod_module_symbol *mv;
	size_t symbollen = strlen(symbol) + 1;

	mv = malloc(sizeof(struct kmod_module_symbol) + symbollen);
	if (mv == NULL)
		return NULL;

	mv->crc = crc;
	memcpy(mv->symbol, symbol, symbollen);
	return mv;
}

static void kmod_module_symbol_free(struct kmod_module_symbol *symbol)
{
	free(symbol);
}

/**
 * kmod_module_get_symbols:
 * @mod: kmod module
 * @list: where to return list of module symbols. Use
 *        kmod_module_symbol_get_symbol() and
 *        kmod_module_symbol_get_crc(). Release this list with
 *        kmod_module_symbols_free_list()
 *
 * Get a list of entries in ELF section ".symtab" or "__ksymtab_strings".
 *
 * After use, free the @list by calling kmod_module_symbols_free_list().
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT int kmod_module_get_symbols(const struct kmod_module *mod, struct kmod_list **list)
{
	struct kmod_elf *elf;
	struct kmod_modversion *symbols;
	int i, count, ret = 0;

	if (mod == NULL || list == NULL)
		return -ENOENT;

	assert(*list == NULL);

	elf = kmod_module_get_elf(mod);
	if (elf == NULL)
		return -errno;

	count = kmod_elf_get_symbols(elf, &symbols);
	if (count < 0)
		return count;

	for (i = 0; i < count; i++) {
		struct kmod_module_symbol *mv;
		struct kmod_list *n;

		mv = kmod_module_symbols_new(symbols[i].crc, symbols[i].symbol);
		if (mv == NULL) {
			ret = -errno;
			kmod_module_symbols_free_list(*list);
			*list = NULL;
			goto list_error;
		}

		n = kmod_list_append(*list, mv);
		if (n != NULL)
			*list = n;
		else {
			kmod_module_symbol_free(mv);
			kmod_module_symbols_free_list(*list);
			*list = NULL;
			ret = -ENOMEM;
			goto list_error;
		}
	}
	ret = count;

list_error:
	free(symbols);
	return ret;
}

/**
 * kmod_module_symbol_get_symbol:
 * @entry: a list entry representing a kmod module symbols
 *
 * Get the symbol of a kmod module symbols.
 *
 * Returns: the symbol of this kmod module symbols on success or NULL
 * on failure. The string is owned by the symbols, do not free it.
 */
KMOD_EXPORT const char *kmod_module_symbol_get_symbol(const struct kmod_list *entry)
{
	struct kmod_module_symbol *symbol;

	if (entry == NULL)
		return NULL;

	symbol = entry->data;
	return symbol->symbol;
}

/**
 * kmod_module_symbol_get_crc:
 * @entry: a list entry representing a kmod module symbol
 *
 * Get the crc of a kmod module symbol.
 *
 * Returns: the crc of this kmod module symbol on success or NULL on
 * failure. The string is owned by the symbol, do not free it.
 */
KMOD_EXPORT uint64_t kmod_module_symbol_get_crc(const struct kmod_list *entry)
{
	struct kmod_module_symbol *symbol;

	if (entry == NULL)
		return 0;

	symbol = entry->data;
	return symbol->crc;
}

/**
 * kmod_module_symbols_free_list:
 * @list: kmod module symbols list
 *
 * Release the resources taken by @list
 */
KMOD_EXPORT void kmod_module_symbols_free_list(struct kmod_list *list)
{
	while (list) {
		kmod_module_symbol_free(list->data);
		list = kmod_list_remove(list);
	}
}

struct kmod_module_dependency_symbol {
	uint64_t crc;
	uint8_t bind;
	char symbol[];
};

static struct kmod_module_dependency_symbol *kmod_module_dependency_symbols_new(uint64_t crc, uint8_t bind, const char *symbol)
{
	struct kmod_module_dependency_symbol *mv;
	size_t symbollen = strlen(symbol) + 1;

	mv = malloc(sizeof(struct kmod_module_dependency_symbol) + symbollen);
	if (mv == NULL)
		return NULL;

	mv->crc = crc;
	mv->bind = bind;
	memcpy(mv->symbol, symbol, symbollen);
	return mv;
}

static void kmod_module_dependency_symbol_free(struct kmod_module_dependency_symbol *dependency_symbol)
{
	free(dependency_symbol);
}

/**
 * kmod_module_get_dependency_symbols:
 * @mod: kmod module
 * @list: where to return list of module dependency_symbols. Use
 *        kmod_module_dependency_symbol_get_symbol() and
 *        kmod_module_dependency_symbol_get_crc(). Release this list with
 *        kmod_module_dependency_symbols_free_list()
 *
 * Get a list of entries in ELF section ".symtab" or "__ksymtab_strings".
 *
 * After use, free the @list by calling
 * kmod_module_dependency_symbols_free_list().
 *
 * Returns: 0 on success or < 0 otherwise.
 */
KMOD_EXPORT int kmod_module_get_dependency_symbols(const struct kmod_module *mod, struct kmod_list **list)
{
	struct kmod_elf *elf;
	struct kmod_modversion *symbols;
	int i, count, ret = 0;

	if (mod == NULL || list == NULL)
		return -ENOENT;

	assert(*list == NULL);

	elf = kmod_module_get_elf(mod);
	if (elf == NULL)
		return -errno;

	count = kmod_elf_get_dependency_symbols(elf, &symbols);
	if (count < 0)
		return count;

	for (i = 0; i < count; i++) {
		struct kmod_module_dependency_symbol *mv;
		struct kmod_list *n;

		mv = kmod_module_dependency_symbols_new(symbols[i].crc,
							symbols[i].bind,
							symbols[i].symbol);
		if (mv == NULL) {
			ret = -errno;
			kmod_module_dependency_symbols_free_list(*list);
			*list = NULL;
			goto list_error;
		}

		n = kmod_list_append(*list, mv);
		if (n != NULL)
			*list = n;
		else {
			kmod_module_dependency_symbol_free(mv);
			kmod_module_dependency_symbols_free_list(*list);
			*list = NULL;
			ret = -ENOMEM;
			goto list_error;
		}
	}
	ret = count;

list_error:
	free(symbols);
	return ret;
}

/**
 * kmod_module_dependency_symbol_get_symbol:
 * @entry: a list entry representing a kmod module dependency_symbols
 *
 * Get the dependency symbol of a kmod module
 *
 * Returns: the symbol of this kmod module dependency_symbols on success or NULL
 * on failure. The string is owned by the dependency_symbols, do not free it.
 */
KMOD_EXPORT const char *kmod_module_dependency_symbol_get_symbol(const struct kmod_list *entry)
{
	struct kmod_module_dependency_symbol *dependency_symbol;

	if (entry == NULL)
		return NULL;

	dependency_symbol = entry->data;
	return dependency_symbol->symbol;
}

/**
 * kmod_module_dependency_symbol_get_crc:
 * @entry: a list entry representing a kmod module dependency_symbol
 *
 * Get the crc of a kmod module dependency_symbol.
 *
 * Returns: the crc of this kmod module dependency_symbol on success or NULL on
 * failure. The string is owned by the dependency_symbol, do not free it.
 */
KMOD_EXPORT uint64_t kmod_module_dependency_symbol_get_crc(const struct kmod_list *entry)
{
	struct kmod_module_dependency_symbol *dependency_symbol;

	if (entry == NULL)
		return 0;

	dependency_symbol = entry->data;
	return dependency_symbol->crc;
}

/**
 * kmod_module_dependency_symbol_get_bind:
 * @entry: a list entry representing a kmod module dependency_symbol
 *
 * Get the bind type of a kmod module dependency_symbol.
 *
 * Returns: the bind of this kmod module dependency_symbol on success
 * or < 0 on failure.
 */
KMOD_EXPORT int kmod_module_dependency_symbol_get_bind(const struct kmod_list *entry)
{
	struct kmod_module_dependency_symbol *dependency_symbol;

	if (entry == NULL)
		return 0;

	dependency_symbol = entry->data;
	return dependency_symbol->bind;
}

/**
 * kmod_module_dependency_symbols_free_list:
 * @list: kmod module dependency_symbols list
 *
 * Release the resources taken by @list
 */
KMOD_EXPORT void kmod_module_dependency_symbols_free_list(struct kmod_list *list)
{
	while (list) {
		kmod_module_dependency_symbol_free(list->data);
		list = kmod_list_remove(list);
	}
}
