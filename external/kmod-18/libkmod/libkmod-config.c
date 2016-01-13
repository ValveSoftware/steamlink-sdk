/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "libkmod.h"
#include "libkmod-internal.h"

struct kmod_alias {
	char *name;
	char modname[];
};

struct kmod_options {
	char *options;
	char modname[];
};

struct kmod_command {
	char *command;
	char modname[];
};

struct kmod_softdep {
	char *name;
	const char **pre;
	const char **post;
	unsigned int n_pre;
	unsigned int n_post;
};

const char *kmod_blacklist_get_modname(const struct kmod_list *l)
{
	return l->data;
}

const char *kmod_alias_get_name(const struct kmod_list *l) {
	const struct kmod_alias *alias = l->data;
	return alias->name;
}

const char *kmod_alias_get_modname(const struct kmod_list *l) {
	const struct kmod_alias *alias = l->data;
	return alias->modname;
}

const char *kmod_option_get_options(const struct kmod_list *l) {
	const struct kmod_options *alias = l->data;
	return alias->options;
}

const char *kmod_option_get_modname(const struct kmod_list *l) {
	const struct kmod_options *alias = l->data;
	return alias->modname;
}

const char *kmod_command_get_command(const struct kmod_list *l) {
	const struct kmod_command *alias = l->data;
	return alias->command;
}

const char *kmod_command_get_modname(const struct kmod_list *l) {
	const struct kmod_command *alias = l->data;
	return alias->modname;
}

const char *kmod_softdep_get_name(const struct kmod_list *l) {
	const struct kmod_softdep *dep = l->data;
	return dep->name;
}

const char * const *kmod_softdep_get_pre(const struct kmod_list *l, unsigned int *count) {
	const struct kmod_softdep *dep = l->data;
	*count = dep->n_pre;
	return dep->pre;
}

const char * const *kmod_softdep_get_post(const struct kmod_list *l, unsigned int *count) {
	const struct kmod_softdep *dep = l->data;
	*count = dep->n_post;
	return dep->post;
}

/*
 * Replace dashes with underscores.
 * Dashes inside character range patterns (e.g. [0-9]) are left unchanged.
 */
static char *underscores(struct kmod_ctx *ctx, char *s)
{
	unsigned int i;

	if (!s)
		return NULL;

	for (i = 0; s[i]; i++) {
		switch (s[i]) {
		case '-':
			s[i] = '_';
			break;

		case ']':
			INFO(ctx, "Unmatched bracket in %s\n", s);
			break;

		case '[':
			i += strcspn(&s[i], "]");
			if (!s[i])
				INFO(ctx, "Unmatched bracket in %s\n", s);
			break;
		}
	}
	return s;
}

static int kmod_config_add_command(struct kmod_config *config,
						const char *modname,
						const char *command,
						const char *command_name,
						struct kmod_list **list)
{
	_cleanup_free_ struct kmod_command *cmd;
	struct kmod_list *l;
	size_t modnamelen = strlen(modname) + 1;
	size_t commandlen = strlen(command) + 1;

	DBG(config->ctx, "modname='%s' cmd='%s %s'\n", modname, command_name,
								command);

	cmd = malloc(sizeof(*cmd) + modnamelen + commandlen);
	if (!cmd)
		return -ENOMEM;

	cmd->command = sizeof(*cmd) + modnamelen + (char *)cmd;
	memcpy(cmd->modname, modname, modnamelen);
	memcpy(cmd->command, command, commandlen);

	l = kmod_list_append(*list, cmd);
	if (!l)
		return -ENOMEM;

	*list = l;
	cmd = NULL;
	return 0;
}

static void kmod_config_free_command(struct kmod_config *config,
					struct kmod_list *l,
					struct kmod_list **list)
{
	struct kmod_command *cmd = l->data;

	free(cmd);
	*list = kmod_list_remove(l);
}

static int kmod_config_add_options(struct kmod_config *config,
				const char *modname, const char *options)
{
	_cleanup_free_ struct kmod_options *opt;
	struct kmod_list *list;
	size_t modnamelen = strlen(modname) + 1;
	size_t optionslen = strlen(options) + 1;

	DBG(config->ctx, "modname='%s' options='%s'\n", modname, options);

	opt = malloc(sizeof(*opt) + modnamelen + optionslen);
	if (!opt)
		return -ENOMEM;

	opt->options = sizeof(*opt) + modnamelen + (char *)opt;

	memcpy(opt->modname, modname, modnamelen);
	memcpy(opt->options, options, optionslen);
	strchr_replace(opt->options, '\t', ' ');

	list = kmod_list_append(config->options, opt);
	if (!list)
		return -ENOMEM;

	opt = NULL;
	config->options = list;
	return 0;
}

static void kmod_config_free_options(struct kmod_config *config,
							struct kmod_list *l)
{
	struct kmod_options *opt = l->data;

	free(opt);

	config->options = kmod_list_remove(l);
}

static int kmod_config_add_alias(struct kmod_config *config,
					const char *name, const char *modname)
{
	_cleanup_free_ struct kmod_alias *alias;
	struct kmod_list *list;
	size_t namelen = strlen(name) + 1, modnamelen = strlen(modname) + 1;

	DBG(config->ctx, "name=%s modname=%s\n", name, modname);

	alias = malloc(sizeof(*alias) + namelen + modnamelen);
	if (!alias)
		return -ENOMEM;

	alias->name = sizeof(*alias) + modnamelen + (char *)alias;

	memcpy(alias->modname, modname, modnamelen);
	memcpy(alias->name, name, namelen);

	list = kmod_list_append(config->aliases, alias);
	if (!list)
		return -ENOMEM;

	alias = NULL;
	config->aliases = list;
	return 0;
}

static void kmod_config_free_alias(struct kmod_config *config,
							struct kmod_list *l)
{
	struct kmod_alias *alias = l->data;

	free(alias);

	config->aliases = kmod_list_remove(l);
}

static int kmod_config_add_blacklist(struct kmod_config *config,
							const char *modname)
{
	_cleanup_free_ char *p;
	struct kmod_list *list;

	DBG(config->ctx, "modname=%s\n", modname);

	p = strdup(modname);
	if (!p)
		return -ENOMEM;

	list = kmod_list_append(config->blacklists, p);
	if (!list)
		return -ENOMEM;

	p = NULL;
	config->blacklists = list;
	return 0;
}

static void kmod_config_free_blacklist(struct kmod_config *config,
							struct kmod_list *l)
{
	free(l->data);
	config->blacklists = kmod_list_remove(l);
}

static int kmod_config_add_softdep(struct kmod_config *config,
							const char *modname,
							const char *line)
{
	struct kmod_list *list;
	struct kmod_softdep *dep;
	const char *s, *p;
	char *itr;
	unsigned int n_pre = 0, n_post = 0;
	size_t modnamelen = strlen(modname) + 1;
	size_t buflen = 0;
	bool was_space = false;
	enum { S_NONE, S_PRE, S_POST } mode = S_NONE;

	DBG(config->ctx, "modname=%s\n", modname);

	/* analyze and count */
	for (p = s = line; ; s++) {
		size_t plen;

		if (*s != '\0') {
			if (!isspace(*s)) {
				was_space = false;
				continue;
			}

			if (was_space) {
				p = s + 1;
				continue;
			}
			was_space = true;

			if (p >= s)
				continue;
		}
		plen = s - p;

		if (plen == sizeof("pre:") - 1 &&
				memcmp(p, "pre:", sizeof("pre:") - 1) == 0)
			mode = S_PRE;
		else if (plen == sizeof("post:") - 1 &&
				memcmp(p, "post:", sizeof("post:") - 1) == 0)
			mode = S_POST;
		else if (*s != '\0' || (*s == '\0' && !was_space)) {
			if (mode == S_PRE) {
				buflen += plen + 1;
				n_pre++;
			} else if (mode == S_POST) {
				buflen += plen + 1;
				n_post++;
			}
		}
		p = s + 1;
		if (*s == '\0')
			break;
	}

	DBG(config->ctx, "%u pre, %u post\n", n_pre, n_post);

	dep = malloc(sizeof(struct kmod_softdep) + modnamelen +
		     n_pre * sizeof(const char *) +
		     n_post * sizeof(const char *) +
		     buflen);
	if (dep == NULL) {
		ERR(config->ctx, "out-of-memory modname=%s\n", modname);
		return -ENOMEM;
	}
	dep->n_pre = n_pre;
	dep->n_post = n_post;
	dep->pre = (const char **)((char *)dep + sizeof(struct kmod_softdep));
	dep->post = dep->pre + n_pre;
	dep->name = (char *)(dep->post + n_post);

	memcpy(dep->name, modname, modnamelen);

	/* copy strings */
	itr = dep->name + modnamelen;
	n_pre = 0;
	n_post = 0;
	mode = S_NONE;
	for (p = s = line; ; s++) {
		size_t plen;

		if (*s != '\0') {
			if (!isspace(*s)) {
				was_space = false;
				continue;
			}

			if (was_space) {
				p = s + 1;
				continue;
			}
			was_space = true;

			if (p >= s)
				continue;
		}
		plen = s - p;

		if (plen == sizeof("pre:") - 1 &&
				memcmp(p, "pre:", sizeof("pre:") - 1) == 0)
			mode = S_PRE;
		else if (plen == sizeof("post:") - 1 &&
				memcmp(p, "post:", sizeof("post:") - 1) == 0)
			mode = S_POST;
		else if (*s != '\0' || (*s == '\0' && !was_space)) {
			if (mode == S_PRE) {
				dep->pre[n_pre] = itr;
				memcpy(itr, p, plen);
				itr[plen] = '\0';
				itr += plen + 1;
				n_pre++;
			} else if (mode == S_POST) {
				dep->post[n_post] = itr;
				memcpy(itr, p, plen);
				itr[plen] = '\0';
				itr += plen + 1;
				n_post++;
			}
		}
		p = s + 1;
		if (*s == '\0')
			break;
	}

	list = kmod_list_append(config->softdeps, dep);
	if (list == NULL) {
		free(dep);
		return -ENOMEM;
	}
	config->softdeps = list;

	return 0;
}

static char *softdep_to_char(struct kmod_softdep *dep) {
	const size_t sz_preprefix = sizeof("pre: ") - 1;
	const size_t sz_postprefix = sizeof("post: ") - 1;
	size_t sz = 1; /* at least '\0' */
	size_t sz_pre, sz_post;
	const char *start, *end;
	char *s, *itr;

	/*
	 * Rely on the fact that dep->pre[] and dep->post[] are strv's that
	 * point to a contiguous buffer
	 */
	if (dep->n_pre > 0) {
		start = dep->pre[0];
		end = dep->pre[dep->n_pre - 1]
					+ strlen(dep->pre[dep->n_pre - 1]);
		sz_pre = end - start;
		sz += sz_pre + sz_preprefix;
	} else
		sz_pre = 0;

	if (dep->n_post > 0) {
		start = dep->post[0];
		end = dep->post[dep->n_post - 1]
					+ strlen(dep->post[dep->n_post - 1]);
		sz_post = end - start;
		sz += sz_post + sz_postprefix;
	} else
		sz_post = 0;

	itr = s = malloc(sz);
	if (s == NULL)
		return NULL;

	if (sz_pre) {
		char *p;

		memcpy(itr, "pre: ", sz_preprefix);
		itr += sz_preprefix;

		/* include last '\0' */
		memcpy(itr, dep->pre[0], sz_pre + 1);
		for (p = itr; p < itr + sz_pre; p++) {
			if (*p == '\0')
				*p = ' ';
		}
		itr = p;
	}

	if (sz_post) {
		char *p;

		memcpy(itr, "post: ", sz_postprefix);
		itr += sz_postprefix;

		/* include last '\0' */
		memcpy(itr, dep->post[0], sz_post + 1);
		for (p = itr; p < itr + sz_post; p++) {
			if (*p == '\0')
				*p = ' ';
		}
		itr = p;
	}

	*itr = '\0';

	return s;
}

static void kmod_config_free_softdep(struct kmod_config *config,
							struct kmod_list *l)
{
	free(l->data);
	config->softdeps = kmod_list_remove(l);
}

static void kcmdline_parse_result(struct kmod_config *config, char *modname,
						char *param, char *value)
{
	if (modname == NULL || param == NULL)
		return;

	DBG(config->ctx, "%s %s\n", modname, param);

	if (streq(modname, "modprobe") && !strncmp(param, "blacklist=", 10)) {
		for (;;) {
			char *t = strsep(&value, ",");
			if (t == NULL)
				break;

			kmod_config_add_blacklist(config, t);
		}
	} else {
		kmod_config_add_options(config,
				underscores(config->ctx, modname), param);
	}
}

static int kmod_config_parse_kcmdline(struct kmod_config *config)
{
	char buf[KCMD_LINE_SIZE];
	int fd, err;
	char *p, *modname,  *param = NULL, *value = NULL, is_module = 1;

	fd = open("/proc/cmdline", O_RDONLY|O_CLOEXEC);
	if (fd < 0) {
		err = -errno;
		DBG(config->ctx, "could not open '/proc/cmdline' for reading: %m\n");
		return err;
	}

	err = read_str_safe(fd, buf, sizeof(buf));
	close(fd);
	if (err < 0) {
		ERR(config->ctx, "could not read from '/proc/cmdline': %s\n",
							strerror(-err));
		return err;
	}

	for (p = buf, modname = buf; *p != '\0' && *p != '\n'; p++) {
		switch (*p) {
		case ' ':
			*p = '\0';
			if (is_module)
				kcmdline_parse_result(config, modname, param, value);
			param = value = NULL;
			modname = p + 1;
			is_module = 1;
			break;
		case '.':
			if (param == NULL) {
				*p = '\0';
				param = p + 1;
			}
			break;
		case '=':
			if (param != NULL)
				value = p + 1;
			else
				is_module = 0;
			break;
		}
	}

	*p = '\0';
	if (is_module)
		kcmdline_parse_result(config, modname, param, value);

	return 0;
}

/*
 * Take an fd and own it. It will be closed on return. filename is used only
 * for debug messages
 */
static int kmod_config_parse(struct kmod_config *config, int fd,
							const char *filename)
{
	struct kmod_ctx *ctx = config->ctx;
	char *line;
	FILE *fp;
	unsigned int linenum = 0;
	int err;

	fp = fdopen(fd, "r");
	if (fp == NULL) {
		err = -errno;
		ERR(config->ctx, "fd %d: %m\n", fd);
		close(fd);
		return err;
	}

	while ((line = getline_wrapped(fp, &linenum)) != NULL) {
		char *cmd, *saveptr;

		if (line[0] == '\0' || line[0] == '#')
			goto done_next;

		cmd = strtok_r(line, "\t ", &saveptr);
		if (cmd == NULL)
			goto done_next;

		if (streq(cmd, "alias")) {
			char *alias = strtok_r(NULL, "\t ", &saveptr);
			char *modname = strtok_r(NULL, "\t ", &saveptr);

			if (alias == NULL || modname == NULL)
				goto syntax_error;

			kmod_config_add_alias(config,
						underscores(ctx, alias),
						underscores(ctx, modname));
		} else if (streq(cmd, "blacklist")) {
			char *modname = strtok_r(NULL, "\t ", &saveptr);

			if (modname == NULL)
				goto syntax_error;

			kmod_config_add_blacklist(config,
						underscores(ctx, modname));
		} else if (streq(cmd, "options")) {
			char *modname = strtok_r(NULL, "\t ", &saveptr);
			char *options = strtok_r(NULL, "\0", &saveptr);

			if (modname == NULL || options == NULL)
				goto syntax_error;

			kmod_config_add_options(config,
						underscores(ctx, modname),
						options);
		} else if (streq(cmd, "install")) {
			char *modname = strtok_r(NULL, "\t ", &saveptr);
			char *installcmd = strtok_r(NULL, "\0", &saveptr);

			if (modname == NULL || installcmd == NULL)
				goto syntax_error;

			kmod_config_add_command(config,
					underscores(ctx, modname),
					installcmd,
					cmd, &config->install_commands);
		} else if (streq(cmd, "remove")) {
			char *modname = strtok_r(NULL, "\t ", &saveptr);
			char *removecmd = strtok_r(NULL, "\0", &saveptr);

			if (modname == NULL || removecmd == NULL)
				goto syntax_error;

			kmod_config_add_command(config,
					underscores(ctx, modname),
					removecmd,
					cmd, &config->remove_commands);
		} else if (streq(cmd, "softdep")) {
			char *modname = strtok_r(NULL, "\t ", &saveptr);
			char *softdeps = strtok_r(NULL, "\0", &saveptr);

			if (modname == NULL || softdeps == NULL)
				goto syntax_error;

			kmod_config_add_softdep(config,
					underscores(ctx, modname),
					softdeps);
		} else if (streq(cmd, "include")
				|| streq(cmd, "config")) {
			ERR(ctx, "%s: command %s is deprecated and not parsed anymore\n",
								filename, cmd);
		} else {
syntax_error:
			ERR(ctx, "%s line %u: ignoring bad line starting with '%s'\n",
						filename, linenum, cmd);
		}

done_next:
		free(line);
	}

	fclose(fp);

	return 0;
}

void kmod_config_free(struct kmod_config *config)
{
	while (config->aliases)
		kmod_config_free_alias(config, config->aliases);

	while (config->blacklists)
		kmod_config_free_blacklist(config, config->blacklists);

	while (config->options)
		kmod_config_free_options(config, config->options);

	while (config->install_commands) {
		kmod_config_free_command(config, config->install_commands,
						&config->install_commands);
	}

	while (config->remove_commands) {
		kmod_config_free_command(config, config->remove_commands,
						&config->remove_commands);
	}

	while (config->softdeps)
		kmod_config_free_softdep(config, config->softdeps);

	for (; config->paths != NULL;
				config->paths = kmod_list_remove(config->paths))
		free(config->paths->data);

	free(config);
}

static bool conf_files_filter_out(struct kmod_ctx *ctx, DIR *d,
					const char *path, const char *fn)
{
	size_t len = strlen(fn);
	struct stat st;

	if (fn[0] == '.')
		return true;

	if (len < 6 || (!streq(&fn[len - 5], ".conf")
				&& !streq(&fn[len - 6], ".alias")))
		return true;

	fstatat(dirfd(d), fn, &st, 0);

	if (S_ISDIR(st.st_mode)) {
		ERR(ctx, "Directories inside directories are not supported: "
							"%s/%s\n", path, fn);
		return true;
	}

	return false;
}

struct conf_file {
	const char *path;
	bool is_single;
	char name[];
};

static int conf_files_insert_sorted(struct kmod_ctx *ctx,
					struct kmod_list **list,
					const char *path, const char *name)
{
	struct kmod_list *lpos, *tmp;
	struct conf_file *cf;
	size_t namelen;
	int cmp = -1;
	bool is_single = false;

	if (name == NULL) {
		name = basename(path);
		is_single = true;
	}

	kmod_list_foreach(lpos, *list) {
		cf = lpos->data;

		if ((cmp = strcmp(name, cf->name)) <= 0)
			break;
	}

	if (cmp == 0) {
		DBG(ctx, "Ignoring duplicate config file: %s/%s\n", path,
									name);
		return -EEXIST;
	}

	namelen = strlen(name);
	cf = malloc(sizeof(*cf) + namelen + 1);
	if (cf == NULL)
		return -ENOMEM;

	memcpy(cf->name, name, namelen + 1);
	cf->path = path;
	cf->is_single = is_single;

	if (lpos == NULL)
		tmp = kmod_list_append(*list, cf);
	else if (lpos == *list)
		tmp = kmod_list_prepend(*list, cf);
	else
		tmp = kmod_list_insert_before(lpos, cf);

	if (tmp == NULL) {
		free(cf);
		return -ENOMEM;
	}

	if (lpos == NULL || lpos == *list)
		*list = tmp;

	return 0;
}

/*
 * Insert configuration files in @list, ignoring duplicates
 */
static int conf_files_list(struct kmod_ctx *ctx, struct kmod_list **list,
						const char *path,
						unsigned long long *path_stamp)
{
	DIR *d;
	int err;
	struct stat st;
	struct dirent *dent;

	if (stat(path, &st) != 0) {
		err = -errno;
		DBG(ctx, "could not stat '%s': %m\n", path);
		return err;
	}

	*path_stamp = stat_mstamp(&st);

	if (!S_ISDIR(st.st_mode)) {
		conf_files_insert_sorted(ctx, list, path, NULL);
		return 0;
	}

	d = opendir(path);
	if (d == NULL) {
		ERR(ctx, "opendir(%s): %m\n", path);
		return -EINVAL;
	}

	for (dent = readdir(d); dent != NULL; dent = readdir(d)) {
		if (conf_files_filter_out(ctx, d, path, dent->d_name))
			continue;

		conf_files_insert_sorted(ctx, list, path, dent->d_name);
	}

	closedir(d);
	return 0;
}

int kmod_config_new(struct kmod_ctx *ctx, struct kmod_config **p_config,
					const char * const *config_paths)
{
	struct kmod_config *config;
	struct kmod_list *list = NULL;
	struct kmod_list *path_list = NULL;
	size_t i;

	conf_files_insert_sorted(ctx, &list, kmod_get_dirname(ctx), "modules.softdep");

	for (i = 0; config_paths[i] != NULL; i++) {
		const char *path = config_paths[i];
		unsigned long long path_stamp = 0;
		size_t pathlen;
		struct kmod_list *tmp;
		struct kmod_config_path *cf;

		if (conf_files_list(ctx, &list, path, &path_stamp) < 0)
			continue;

		pathlen = strlen(path) + 1;
		cf = malloc(sizeof(*cf) + pathlen);
		if (cf == NULL)
			goto oom;

		cf->stamp = path_stamp;
		memcpy(cf->path, path, pathlen);

		tmp = kmod_list_append(path_list, cf);
		if (tmp == NULL)
			goto oom;
		path_list = tmp;
	}

	*p_config = config = calloc(1, sizeof(struct kmod_config));
	if (config == NULL)
		goto oom;

	config->paths = path_list;
	config->ctx = ctx;

	for (; list != NULL; list = kmod_list_remove(list)) {
		char fn[PATH_MAX];
		struct conf_file *cf = list->data;
		int fd;

		if (cf->is_single)
			strcpy(fn, cf->path);
		else
			snprintf(fn, sizeof(fn),"%s/%s", cf->path,
					cf->name);

		fd = open(fn, O_RDONLY|O_CLOEXEC);
		DBG(ctx, "parsing file '%s' fd=%d\n", fn, fd);

		if (fd >= 0)
			kmod_config_parse(config, fd, fn);

		free(cf);
	}

	kmod_config_parse_kcmdline(config);

	return 0;

oom:
	for (; list != NULL; list = kmod_list_remove(list))
		free(list->data);

	for (; path_list != NULL; path_list = kmod_list_remove(path_list))
		free(path_list->data);

	return -ENOMEM;
}

/**********************************************************************
 * struct kmod_config_iter functions
 **********************************************************************/

enum config_type {
	CONFIG_TYPE_BLACKLIST = 0,
	CONFIG_TYPE_INSTALL,
	CONFIG_TYPE_REMOVE,
	CONFIG_TYPE_ALIAS,
	CONFIG_TYPE_OPTION,
	CONFIG_TYPE_SOFTDEP,
};

struct kmod_config_iter {
	enum config_type type;
	bool intermediate;
	const struct kmod_list *list;
	const struct kmod_list *curr;
	void *data;
	const char *(*get_key)(const struct kmod_list *l);
	const char *(*get_value)(const struct kmod_list *l);
};

static const char *softdep_get_plain_softdep(const struct kmod_list *l)
{
	char *s = softdep_to_char(l->data);
	return s;
}

static struct kmod_config_iter *kmod_config_iter_new(const struct kmod_ctx* ctx,
							enum config_type type)
{
	struct kmod_config_iter *iter = calloc(1, sizeof(*iter));
	const struct kmod_config *config = kmod_get_config(ctx);

	if (iter == NULL)
		return NULL;

	iter->type = type;

	switch (type) {
	case CONFIG_TYPE_BLACKLIST:
		iter->list = config->blacklists;
		iter->get_key = kmod_blacklist_get_modname;
		break;
	case CONFIG_TYPE_INSTALL:
		iter->list = config->install_commands;
		iter->get_key = kmod_command_get_modname;
		iter->get_value = kmod_command_get_command;
		break;
	case CONFIG_TYPE_REMOVE:
		iter->list = config->remove_commands;
		iter->get_key = kmod_command_get_modname;
		iter->get_value = kmod_command_get_command;
		break;
	case CONFIG_TYPE_ALIAS:
		iter->list = config->aliases;
		iter->get_key = kmod_alias_get_name;
		iter->get_value = kmod_alias_get_modname;
		break;
	case CONFIG_TYPE_OPTION:
		iter->list = config->options;
		iter->get_key = kmod_option_get_modname;
		iter->get_value = kmod_option_get_options;
		break;
	case CONFIG_TYPE_SOFTDEP:
		iter->list = config->softdeps;
		iter->get_key = kmod_softdep_get_name;
		iter->get_value = softdep_get_plain_softdep;
		iter->intermediate = true;
		break;
	}

	return iter;
}

/**
 * SECTION:libkmod-config
 * @short_description: retrieve current libkmod configuration
 */

/**
 * kmod_config_get_blacklists:
 * @ctx: kmod library context
 *
 * Retrieve an iterator to deal with the blacklist maintained inside the
 * library. See kmod_config_iter_get_key(), kmod_config_iter_get_value() and
 * kmod_config_iter_next(). At least one call to kmod_config_iter_next() must
 * be made to initialize the iterator and check if it's valid.
 *
 * Returns: a new iterator over the blacklists or NULL on failure. Free it
 * with kmod_config_iter_free_iter().
 */
KMOD_EXPORT struct kmod_config_iter *kmod_config_get_blacklists(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;;

	return kmod_config_iter_new(ctx, CONFIG_TYPE_BLACKLIST);
}

/**
 * kmod_config_get_install_commands:
 * @ctx: kmod library context
 *
 * Retrieve an iterator to deal with the install commands maintained inside the
 * library. See kmod_config_iter_get_key(), kmod_config_iter_get_value() and
 * kmod_config_iter_next(). At least one call to kmod_config_iter_next() must
 * be made to initialize the iterator and check if it's valid.
 *
 * Returns: a new iterator over the install commands or NULL on failure. Free
 * it with kmod_config_iter_free_iter().
 */
KMOD_EXPORT struct kmod_config_iter *kmod_config_get_install_commands(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;;

	return kmod_config_iter_new(ctx, CONFIG_TYPE_INSTALL);
}

/**
 * kmod_config_get_remove_commands:
 * @ctx: kmod library context
 *
 * Retrieve an iterator to deal with the remove commands maintained inside the
 * library. See kmod_config_iter_get_key(), kmod_config_iter_get_value() and
 * kmod_config_iter_next(). At least one call to kmod_config_iter_next() must
 * be made to initialize the iterator and check if it's valid.
 *
 * Returns: a new iterator over the remove commands or NULL on failure. Free
 * it with kmod_config_iter_free_iter().
 */
KMOD_EXPORT struct kmod_config_iter *kmod_config_get_remove_commands(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;;

	return kmod_config_iter_new(ctx, CONFIG_TYPE_REMOVE);
}

/**
 * kmod_config_get_aliases:
 * @ctx: kmod library context
 *
 * Retrieve an iterator to deal with the aliases maintained inside the
 * library. See kmod_config_iter_get_key(), kmod_config_iter_get_value() and
 * kmod_config_iter_next(). At least one call to kmod_config_iter_next() must
 * be made to initialize the iterator and check if it's valid.
 *
 * Returns: a new iterator over the aliases or NULL on failure. Free it with
 * kmod_config_iter_free_iter().
 */
KMOD_EXPORT struct kmod_config_iter *kmod_config_get_aliases(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;;

	return kmod_config_iter_new(ctx, CONFIG_TYPE_ALIAS);
}

/**
 * kmod_config_get_options:
 * @ctx: kmod library context
 *
 * Retrieve an iterator to deal with the options maintained inside the
 * library. See kmod_config_iter_get_key(), kmod_config_iter_get_value() and
 * kmod_config_iter_next(). At least one call to kmod_config_iter_next() must
 * be made to initialize the iterator and check if it's valid.
 *
 * Returns: a new iterator over the options or NULL on failure. Free it with
 * kmod_config_iter_free_iter().
 */
KMOD_EXPORT struct kmod_config_iter *kmod_config_get_options(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;;

	return kmod_config_iter_new(ctx, CONFIG_TYPE_OPTION);
}

/**
 * kmod_config_get_softdeps:
 * @ctx: kmod library context
 *
 * Retrieve an iterator to deal with the softdeps maintained inside the
 * library. See kmod_config_iter_get_key(), kmod_config_iter_get_value() and
 * kmod_config_iter_next(). At least one call to kmod_config_iter_next() must
 * be made to initialize the iterator and check if it's valid.
 *
 * Returns: a new iterator over the softdeps or NULL on failure. Free it with
 * kmod_config_iter_free_iter().
 */
KMOD_EXPORT struct kmod_config_iter *kmod_config_get_softdeps(const struct kmod_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;;

	return kmod_config_iter_new(ctx, CONFIG_TYPE_SOFTDEP);
}

/**
 * kmod_config_iter_get_key:
 * @iter: iterator over a certain configuration
 *
 * When using a new allocated iterator, user must perform a call to
 * kmod_config_iter_next() to initialize iterator's position and check if it's
 * valid.
 *
 * Returns: the key of the current configuration pointed by @iter.
 */
KMOD_EXPORT const char *kmod_config_iter_get_key(const struct kmod_config_iter *iter)
{
	if (iter == NULL || iter->curr == NULL)
		return NULL;

	return iter->get_key(iter->curr);
}

/**
 * kmod_config_iter_get_value:
 * @iter: iterator over a certain configuration
 *
 * When using a new allocated iterator, user must perform a call to
 * kmod_config_iter_next() to initialize iterator's position and check if it's
 * valid.
 *
 * Returns: the value of the current configuration pointed by @iter.
 */
KMOD_EXPORT const char *kmod_config_iter_get_value(const struct kmod_config_iter *iter)
{
	const char *s;

	if (iter == NULL || iter->curr == NULL)
		return NULL;

	if (iter->get_value == NULL)
		return NULL;

	if (iter->intermediate) {
		struct kmod_config_iter *i = (struct kmod_config_iter *)iter;

		free(i->data);
		s = i->data = (void *) iter->get_value(iter->curr);
	} else
		s = iter->get_value(iter->curr);

	return s;
}

/**
 * kmod_config_iter_next:
 * @iter: iterator over a certain configuration
 *
 * Make @iter point to the next item of a certain configuration. It's an
 * automatically recycling iterator. When it reaches the end, false is
 * returned; then if user wants to iterate again, it's sufficient to call this
 * function once more.
 *
 * Returns: true if next position of @iter is valid or false if its end is
 * reached.
 */
KMOD_EXPORT bool kmod_config_iter_next(struct kmod_config_iter *iter)
{
	if (iter == NULL)
		return false;

	if (iter->curr == NULL) {
		iter->curr = iter->list;
		return iter->curr != NULL;
	}

	iter->curr = kmod_list_next(iter->list, iter->curr);

	return iter->curr != NULL;
}

/**
 * kmod_config_iter_free_iter:
 * @iter: iterator over a certain configuration
 *
 * Free resources used by the iterator.
 */
KMOD_EXPORT void kmod_config_iter_free_iter(struct kmod_config_iter *iter)
{
	free(iter->data);
	free(iter);
}
