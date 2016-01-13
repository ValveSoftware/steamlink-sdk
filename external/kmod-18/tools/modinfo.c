/*
 * kmod-modinfo - query kernel module information using libkmod.
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include "libkmod.h"
#include "libkmod-util.h"

#include "kmod.h"

static char separator = '\n';
static const char *field = NULL;

struct param {
	struct param *next;
	const char *name;
	const char *param;
	const char *type;
	int namelen;
	int paramlen;
	int typelen;
};

static struct param *add_param(const char *name, int namelen, const char *param, int paramlen, const char *type, int typelen, struct param **list)
{
	struct param *it;

	for (it = *list; it != NULL; it = it->next) {
		if (it->namelen == namelen &&
			memcmp(it->name, name, namelen) == 0)
			break;
	}

	if (it == NULL) {
		it = malloc(sizeof(struct param));
		if (it == NULL)
			return NULL;
		it->next = *list;
		*list = it;
		it->name = name;
		it->namelen = namelen;
		it->param = NULL;
		it->type = NULL;
		it->paramlen = 0;
		it->typelen = 0;
	}

	if (param != NULL) {
		it->param = param;
		it->paramlen = paramlen;
	}

	if (type != NULL) {
		it->type = type;
		it->typelen = typelen;
	}

	return it;
}

static int process_parm(const char *key, const char *value, struct param **params)
{
	const char *name, *param, *type;
	int namelen, paramlen, typelen;
	struct param *it;
	const char *colon = strchr(value, ':');
	if (colon == NULL) {
		ERR("Found invalid \"%s=%s\": missing ':'\n",
		    key, value);
		return 0;
	}

	name = value;
	namelen = colon - value;
	if (strcmp(key, "parm") == 0) {
		param = colon + 1;
		paramlen = strlen(param);
		type = NULL;
		typelen = 0;
	} else {
		param = NULL;
		paramlen = 0;
		type = colon + 1;
		typelen = strlen(type);
	}

	it = add_param(name, namelen, param, paramlen, type, typelen, params);
	if (it == NULL) {
		ERR("Out of memory!\n");
		return -ENOMEM;
	}

	return 0;
}

static int modinfo_params_do(const struct kmod_list *list)
{
	const struct kmod_list *l;
	struct param *params = NULL;
	int err = 0;

	kmod_list_foreach(l, list) {
		const char *key = kmod_module_info_get_key(l);
		const char *value = kmod_module_info_get_value(l);
		if (strcmp(key, "parm") != 0 &&
		    strcmp(key, "parmtype") != 0)
			continue;

		err = process_parm(key, value, &params);
		if (err < 0)
			goto end;
	}

	while (params != NULL) {
		struct param *p = params;
		params = p->next;

		if (p->param == NULL)
			printf("%.*s: (%.*s)%c",
			       p->namelen, p->name, p->typelen, p->type,
			       separator);
		else if (p->type != NULL)
			printf("%.*s:%.*s (%.*s)%c",
			       p->namelen, p->name,
			       p->paramlen, p->param,
			       p->typelen, p->type,
			       separator);
		else
			printf("%.*s:%.*s%c",
			       p->namelen, p->name,
			       p->paramlen, p->param,
			       separator);

		free(p);
	}

end:
	while (params != NULL) {
		void *tmp = params;
		params = params->next;
		free(tmp);
	}

	return err;
}

static int modinfo_do(struct kmod_module *mod)
{
	struct kmod_list *l, *list = NULL;
	struct param *params = NULL;
	int err;

	if (field != NULL && strcmp(field, "filename") == 0) {
		printf("%s%c", kmod_module_get_path(mod), separator);
		return 0;
	} else if (field == NULL) {
		printf("%-16s%s%c", "filename:",
		       kmod_module_get_path(mod), separator);
	}

	err = kmod_module_get_info(mod, &list);
	if (err < 0) {
		ERR("could not get modinfo from '%s': %s\n",
			kmod_module_get_name(mod), strerror(-err));
		return err;
	}

	if (field != NULL && strcmp(field, "parm") == 0) {
		err = modinfo_params_do(list);
		goto end;
	}

	kmod_list_foreach(l, list) {
		const char *key = kmod_module_info_get_key(l);
		const char *value = kmod_module_info_get_value(l);
		int keylen;

		if (field != NULL) {
			if (strcmp(field, key) != 0)
				continue;
			/* filtered output contains no key, just value */
			printf("%s%c", value, separator);
			continue;
		}

		if (strcmp(key, "parm") == 0 || strcmp(key, "parmtype") == 0) {
			err = process_parm(key, value, &params);
			if (err < 0)
				goto end;
			continue;
		}

		if (separator == '\0') {
			printf("%s=%s%c", key, value, separator);
			continue;
		}

		keylen = strlen(key);
		printf("%s:%-*s%s%c", key, 15 - keylen, "", value, separator);
	}

	if (field != NULL)
		goto end;

	while (params != NULL) {
		struct param *p = params;
		params = p->next;

		if (p->param == NULL)
			printf("%-16s%.*s:%.*s%c", "parm:",
			       p->namelen, p->name, p->typelen, p->type,
			       separator);
		else if (p->type != NULL)
			printf("%-16s%.*s:%.*s (%.*s)%c", "parm:",
			       p->namelen, p->name,
			       p->paramlen, p->param,
			       p->typelen, p->type,
			       separator);
		else
			printf("%-16s%.*s:%.*s%c",
			       "parm:",
			       p->namelen, p->name,
			       p->paramlen, p->param,
			       separator);

		free(p);
	}

end:
	while (params != NULL) {
		void *tmp = params;
		params = params->next;
		free(tmp);
	}
	kmod_module_info_free_list(list);

	return err;
}

static int modinfo_path_do(struct kmod_ctx *ctx, const char *path)
{
	struct kmod_module *mod;
	int err = kmod_module_new_from_path(ctx, path, &mod);
	if (err < 0) {
		ERR("Module file %s not found.\n", path);
		return err;
	}
	err = modinfo_do(mod);
	kmod_module_unref(mod);
	return err;
}

static int modinfo_alias_do(struct kmod_ctx *ctx, const char *alias)
{
	struct kmod_list *l, *filtered, *list = NULL;
	int err = kmod_module_new_from_lookup(ctx, alias, &list);
	if (err < 0) {
		ERR("Module alias %s not found.\n", alias);
		return err;
	}

	if (list == NULL) {
		ERR("Module %s not found.\n", alias);
		return -ENOENT;
	}

	err = kmod_module_apply_filter(ctx, KMOD_FILTER_BUILTIN, list, &filtered);
	kmod_module_unref_list(list);
	if (err < 0) {
		ERR("Failed to filter list: %m\n");
		return err;
	}

	if (filtered == NULL) {
		ERR("Module %s not found.\n", alias);
		return -ENOENT;
	}

	kmod_list_foreach(l, filtered) {
		struct kmod_module *mod = kmod_module_get_module(l);
		int r = modinfo_do(mod);
		kmod_module_unref(mod);
		if (r < 0)
			err = r;
	}
	kmod_module_unref_list(filtered);
	return err;
}

static const char cmdopts_s[] = "adlpn0F:k:b:Vh";
static const struct option cmdopts[] = {
	{"author", no_argument, 0, 'a'},
	{"description", no_argument, 0, 'd'},
	{"license", no_argument, 0, 'l'},
	{"parameters", no_argument, 0, 'p'},
	{"filename", no_argument, 0, 'n'},
	{"null", no_argument, 0, '0'},
	{"field", required_argument, 0, 'F'},
	{"set-version", required_argument, 0, 'k'},
	{"basedir", required_argument, 0, 'b'},
	{"version", no_argument, 0, 'V'},
	{"help", no_argument, 0, 'h'},
	{NULL, 0, 0, 0}
};

static void help(void)
{
	printf("Usage:\n"
		"\t%s [options] filename [args]\n"
		"Options:\n"
		"\t-a, --author                Print only 'author'\n"
		"\t-d, --description           Print only 'description'\n"
		"\t-l, --license               Print only 'license'\n"
		"\t-p, --parameters            Print only 'parm'\n"
		"\t-n, --filename              Print only 'filename'\n"
		"\t-0, --null                  Use \\0 instead of \\n\n"
		"\t-F, --field=FIELD           Print only provided FIELD\n"
		"\t-k, --set-version=VERSION   Use VERSION instead of `uname -r`\n"
		"\t-b, --basedir=DIR           Use DIR as filesystem root for /lib/modules\n"
		"\t-V, --version               Show version\n"
		"\t-h, --help                  Show this help\n",
		program_invocation_short_name);
}

static bool is_module_filename(const char *name)
{
	struct stat st;

	if (stat(name, &st) == 0 && S_ISREG(st.st_mode) &&
		path_ends_with_kmod_ext(name, strlen(name)))
			return true;

	return false;
}

static int do_modinfo(int argc, char *argv[])
{
	struct kmod_ctx *ctx;
	char dirname_buf[PATH_MAX];
	const char *dirname = NULL;
	const char *kversion = NULL;
	const char *root = NULL;
	const char *null_config = NULL;
	int i, err;

	for (;;) {
		int c, idx = 0;
		c = getopt_long(argc, argv, cmdopts_s, cmdopts, &idx);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
			field = "author";
			break;
		case 'd':
			field = "description";
			break;
		case 'l':
			field = "license";
			break;
		case 'p':
			field = "parm";
			break;
		case 'n':
			field = "filename";
			break;
		case '0':
			separator = '\0';
			break;
		case 'F':
			field = optarg;
			break;
		case 'k':
			kversion = optarg;
			break;
		case 'b':
			root = optarg;
			break;
		case 'h':
			help();
			return EXIT_SUCCESS;
		case 'V':
			puts(PACKAGE " version " VERSION);
			return EXIT_SUCCESS;
		case '?':
			return EXIT_FAILURE;
		default:
			ERR("unexpected getopt_long() value '%c'.\n", c);
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		ERR("missing module or filename.\n");
		return EXIT_FAILURE;
	}

	if (root != NULL || kversion != NULL) {
		struct utsname u;
		if (root == NULL)
			root = "";
		if (kversion == NULL) {
			if (uname(&u) < 0) {
				ERR("uname() failed: %m\n");
				return EXIT_FAILURE;
			}
			kversion = u.release;
		}
		snprintf(dirname_buf, sizeof(dirname_buf), "%s/lib/modules/%s",
			 root, kversion);
		dirname = dirname_buf;
	}

	ctx = kmod_new(dirname, &null_config);
	if (!ctx) {
		ERR("kmod_new() failed!\n");
		return EXIT_FAILURE;
	}

	err = 0;
	for (i = optind; i < argc; i++) {
		const char *name = argv[i];
		int r;

		if (is_module_filename(name))
			r = modinfo_path_do(ctx, name);
		else
			r = modinfo_alias_do(ctx, name);

		if (r < 0)
			err = r;
	}

	kmod_unref(ctx);
	return err >= 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

const struct kmod_cmd kmod_cmd_compat_modinfo = {
	.name = "modinfo",
	.cmd = do_modinfo,
	.help = "compat modinfo command",
};
