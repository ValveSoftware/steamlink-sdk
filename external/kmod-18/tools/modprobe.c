/*
 * kmod-modprobe - manage linux kernel modules using libkmod.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

#include "libkmod.h"
#include "libkmod-array.h"
#include "macro.h"

#include "kmod.h"

static int log_priority = LOG_CRIT;
static int use_syslog = 0;
#define LOG(...) log_printf(log_priority, __VA_ARGS__)

#define DEFAULT_VERBOSE LOG_WARNING
static int verbose = DEFAULT_VERBOSE;
static int do_show = 0;
static int dry_run = 0;
static int ignore_loaded = 0;
static int lookup_only = 0;
static int first_time = 0;
static int ignore_commands = 0;
static int use_blacklist = 0;
static int force = 0;
static int strip_modversion = 0;
static int strip_vermagic = 0;
static int remove_dependencies = 0;
static int quiet_inuse = 0;

static const char cmdopts_s[] = "arRibfDcnC:d:S:sqvVh";
static const struct option cmdopts[] = {
	{"all", no_argument, 0, 'a'},
	{"remove", no_argument, 0, 'r'},
	{"remove-dependencies", no_argument, 0, 5},
	{"resolve-alias", no_argument, 0, 'R'},
	{"first-time", no_argument, 0, 3},
	{"ignore-install", no_argument, 0, 'i'},
	{"ignore-remove", no_argument, 0, 'i'},
	{"use-blacklist", no_argument, 0, 'b'},
	{"force", no_argument, 0, 'f'},
	{"force-modversion", no_argument, 0, 2},
	{"force-vermagic", no_argument, 0, 1},

	{"show-depends", no_argument, 0, 'D'},
	{"showconfig", no_argument, 0, 'c'},
	{"show-config", no_argument, 0, 'c'},
	{"show-modversions", no_argument, 0, 4},
	{"dump-modversions", no_argument, 0, 4},

	{"dry-run", no_argument, 0, 'n'},
	{"show", no_argument, 0, 'n'},

	{"config", required_argument, 0, 'C'},
	{"dirname", required_argument, 0, 'd'},
	{"set-version", required_argument, 0, 'S'},

	{"syslog", no_argument, 0, 's'},
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{"version", no_argument, 0, 'V'},
	{"help", no_argument, 0, 'h'},
	{NULL, 0, 0, 0}
};

static void help(void)
{
	printf("Usage:\n"
		"\t%s [options] [-i] [-b] modulename\n"
		"\t%s [options] -a [-i] [-b] modulename [modulename...]\n"
		"\t%s [options] -r [-i] modulename\n"
		"\t%s [options] -r -a [-i] modulename [modulename...]\n"
		"\t%s [options] -c\n"
		"\t%s [options] --dump-modversions filename\n"
		"Management Options:\n"
		"\t-a, --all                   Consider every non-argument to\n"
		"\t                            be a module name to be inserted\n"
		"\t                            or removed (-r)\n"
		"\t-r, --remove                Remove modules instead of inserting\n"
		"\t    --remove-dependencies   Also remove modules depending on it\n"
		"\t-R, --resolve-alias         Only lookup and print alias and exit\n"
		"\t    --first-time            Fail if module already inserted or removed\n"
		"\t-i, --ignore-install        Ignore install commands\n"
		"\t-i, --ignore-remove         Ignore remove commands\n"
		"\t-b, --use-blacklist         Apply blacklist to resolved alias.\n"
		"\t-f, --force                 Force module insertion or removal.\n"
		"\t                            implies --force-modversions and\n"
		"\t                            --force-vermagic\n"
		"\t    --force-modversion      Ignore module's version\n"
		"\t    --force-vermagic        Ignore module's version magic\n"
		"\n"
		"Query Options:\n"
		"\t-D, --show-depends          Only print module dependencies and exit\n"
		"\t-c, --showconfig            Print out known configuration and exit\n"
		"\t-c, --show-config           Same as --showconfig\n"
		"\t    --show-modversions      Dump module symbol version and exit\n"
		"\t    --dump-modversions      Same as --show-modversions\n"
		"\n"
		"General Options:\n"
		"\t-n, --dry-run               Do not execute operations, just print out\n"
		"\t-n, --show                  Same as --dry-run\n"

		"\t-C, --config=FILE           Use FILE instead of default search paths\n"
		"\t-d, --dirname=DIR           Use DIR as filesystem root for /lib/modules\n"
		"\t-S, --set-version=VERSION   Use VERSION instead of `uname -r`\n"

		"\t-s, --syslog                print to syslog, not stderr\n"
		"\t-q, --quiet                 disable messages\n"
		"\t-v, --verbose               enables more messages\n"
		"\t-V, --version               show version\n"
		"\t-h, --help                  show this help\n",
		program_invocation_short_name, program_invocation_short_name,
		program_invocation_short_name, program_invocation_short_name,
		program_invocation_short_name, program_invocation_short_name);
}

_printf_format_(1, 2)
static inline void _show(const char *fmt, ...)
{
	va_list args;

	if (!do_show && verbose <= DEFAULT_VERBOSE)
		return;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fflush(stdout);
	va_end(args);
}
#define SHOW(...) _show(__VA_ARGS__)

static int show_config(struct kmod_ctx *ctx)
{
	struct config_iterators {
		const char *name;
		struct kmod_config_iter *(*get_iter)(const struct kmod_ctx *ctx);
	} ci[] = {
		{ "blacklist", kmod_config_get_blacklists },
		{ "install", kmod_config_get_install_commands },
		{ "remove", kmod_config_get_remove_commands },
		{ "alias", kmod_config_get_aliases },
		{ "options", kmod_config_get_options },
		{ "softdep", kmod_config_get_softdeps },
	};
	size_t i;

	for (i = 0;  i < ARRAY_SIZE(ci); i++) {
		struct kmod_config_iter *iter = ci[i].get_iter(ctx);

		if (iter == NULL)
			continue;

		while (kmod_config_iter_next(iter)) {
			const char *val;

			printf("%s %s", ci[i].name,
					kmod_config_iter_get_key(iter));
			val = kmod_config_iter_get_value(iter);
			if (val != NULL) {
				putchar(' ');
				puts(val);
			} else
				putchar('\n');
		}

		kmod_config_iter_free_iter(iter);
	}

	puts("\n# End of configuration files. Dumping indexes now:\n");
	fflush(stdout);

	kmod_dump_index(ctx, KMOD_INDEX_MODULES_ALIAS, STDOUT_FILENO);
	kmod_dump_index(ctx, KMOD_INDEX_MODULES_SYMBOL, STDOUT_FILENO);

	return 0;
}

static int show_modversions(struct kmod_ctx *ctx, const char *filename)
{
	struct kmod_list *l, *list = NULL;
	struct kmod_module *mod;
	int err = kmod_module_new_from_path(ctx, filename, &mod);
	if (err < 0) {
		LOG("Module %s not found.\n", filename);
		return err;
	}

	err = kmod_module_get_versions(mod, &list);
	if (err < 0) {
		LOG("could not get modversions of %s: %s\n",
			filename, strerror(-err));
		kmod_module_unref(mod);
		return err;
	}

	kmod_list_foreach(l, list) {
		const char *symbol = kmod_module_version_get_symbol(l);
		uint64_t crc = kmod_module_version_get_crc(l);
		printf("0x%08"PRIx64"\t%s\n", crc, symbol);
	}
	kmod_module_versions_free_list(list);
	kmod_module_unref(mod);
	return 0;
}

static int command_do(struct kmod_module *module, const char *type,
				const char *command, const char *cmdline_opts)
{
	const char *modname = kmod_module_get_name(module);
	char *p, *cmd = NULL;
	size_t cmdlen, cmdline_opts_len, varlen;
	int ret = 0;

	if (cmdline_opts == NULL)
		cmdline_opts = "";
	cmdline_opts_len = strlen(cmdline_opts);

	cmd = strdup(command);
	if (cmd == NULL)
		return -ENOMEM;
	cmdlen = strlen(cmd);
	varlen = sizeof("$CMDLINE_OPTS") - 1;
	while ((p = strstr(cmd, "$CMDLINE_OPTS")) != NULL) {
		size_t prefixlen = p - cmd;
		size_t suffixlen = cmdlen - prefixlen - varlen;
		size_t slen = cmdlen - varlen + cmdline_opts_len;
		char *suffix = p + varlen;
		char *s = malloc(slen + 1);
		if (s == NULL) {
			free(cmd);
			return -ENOMEM;
		}
		memcpy(s, cmd, p - cmd);
		memcpy(s + prefixlen, cmdline_opts, cmdline_opts_len);
		memcpy(s + prefixlen + cmdline_opts_len, suffix, suffixlen);
		s[slen] = '\0';

		free(cmd);
		cmd = s;
		cmdlen = slen;
	}

	SHOW("%s %s\n", type, cmd);
	if (dry_run)
		goto end;

	setenv("MODPROBE_MODULE", modname, 1);
	ret = system(cmd);
	unsetenv("MODPROBE_MODULE");
	if (ret == -1 || WEXITSTATUS(ret)) {
		LOG("Error running %s command for %s\n", type, modname);
		if (ret != -1)
			ret = -WEXITSTATUS(ret);
	}

end:
	free(cmd);
	return ret;
}

static int rmmod_do_remove_module(struct kmod_module *mod)
{
	const char *modname = kmod_module_get_name(mod);
	struct kmod_list *deps, *itr;
	int flags = 0, err;

	SHOW("rmmod %s\n", kmod_module_get_name(mod));

	if (dry_run)
		return 0;

	if (force)
		flags |= KMOD_REMOVE_FORCE;

	err = kmod_module_remove_module(mod, flags);
	if (err == -EEXIST) {
		if (!first_time)
			err = 0;
		else
			LOG("Module %s is not in kernel.\n", modname);
	}

	deps = kmod_module_get_dependencies(mod);
	if (deps != NULL) {
		kmod_list_foreach(itr, deps) {
			struct kmod_module *dep = kmod_module_get_module(itr);
			if (kmod_module_get_refcnt(dep) == 0)
				rmmod_do_remove_module(dep);
			kmod_module_unref(dep);
		}
		kmod_module_unref_list(deps);
	}

	return err;
}

static int rmmod_do_module(struct kmod_module *mod, bool do_dependencies);

static int rmmod_do_deps_list(struct kmod_list *list, bool stop_on_errors)
{
	struct kmod_list *l;

	kmod_list_foreach_reverse(l, list) {
		struct kmod_module *m = kmod_module_get_module(l);
		int r = rmmod_do_module(m, false);
		kmod_module_unref(m);

		if (r < 0 && stop_on_errors)
			return r;
	}

	return 0;
}

static int rmmod_do_module(struct kmod_module *mod, bool do_dependencies)
{
	const char *modname = kmod_module_get_name(mod);
	struct kmod_list *pre = NULL, *post = NULL;
	const char *cmd = NULL;
	int err;

	if (!ignore_commands) {
		err = kmod_module_get_softdeps(mod, &pre, &post);
		if (err < 0) {
			WRN("could not get softdeps of '%s': %s\n",
						modname, strerror(-err));
			return err;
		}

		cmd = kmod_module_get_remove_commands(mod);
	}

	if (cmd == NULL && !ignore_loaded) {
		int state = kmod_module_get_initstate(mod);

		if (state < 0) {
			if (first_time) {
				LOG("Module %s is not in kernel.\n", modname);
				err = -ENOENT;
			} else {
				err = 0;
			}
			goto error;
		} else if (state == KMOD_MODULE_BUILTIN) {
			LOG("Module %s is builtin.\n", modname);
			err = -ENOENT;
			goto error;
		}
	}

	rmmod_do_deps_list(post, false);

	if (do_dependencies && remove_dependencies) {
		struct kmod_list *deps = kmod_module_get_dependencies(mod);

		err = rmmod_do_deps_list(deps, true);
		if (err < 0)
			goto error;
	}

	if (!ignore_loaded && !cmd) {
		int usage = kmod_module_get_refcnt(mod);

		if (usage > 0) {
			if (!quiet_inuse)
				LOG("Module %s is in use.\n", modname);

			err = -EBUSY;
			goto error;
		}
	}

	if (cmd == NULL)
		err = rmmod_do_remove_module(mod);
	else
		err = command_do(mod, "remove", cmd, NULL);

	if (err < 0)
		goto error;

	rmmod_do_deps_list(pre, false);

error:
	kmod_module_unref_list(pre);
	kmod_module_unref_list(post);

	return err;
}

static int rmmod(struct kmod_ctx *ctx, const char *alias)
{
	struct kmod_list *l, *list = NULL;
	int err;

	err = kmod_module_new_from_lookup(ctx, alias, &list);
	if (err < 0)
		return err;

	if (list == NULL) {
		LOG("Module %s not found.\n", alias);
		err = -ENOENT;
	}

	kmod_list_foreach(l, list) {
		struct kmod_module *mod = kmod_module_get_module(l);
		err = rmmod_do_module(mod, true);
		kmod_module_unref(mod);
		if (err < 0)
			break;
	}

	kmod_module_unref_list(list);
	return err;
}

static int rmmod_all(struct kmod_ctx *ctx, char **args, int nargs)
{
	int i, err = 0;

	for (i = 0; i < nargs; i++) {
		int r = rmmod(ctx, args[i]);
		if (r < 0)
			err = r;
	}

	return err;
}

static int handle_failed_lookup(struct kmod_ctx *ctx, const char *alias)
{
	struct kmod_module *mod;
	int state, err;

	DBG("lookup failed - trying to check if it's builtin\n");

	err = kmod_module_new_from_name(ctx, alias, &mod);
	if (err < 0)
		return err;

	state = kmod_module_get_initstate(mod);
	kmod_module_unref(mod);

	if (state != KMOD_MODULE_BUILTIN) {
		LOG("Module %s not found.\n", alias);
		return -ENOENT;
	}

	if (first_time) {
		LOG("Module %s already in kernel (builtin).\n", alias);
		return -ENOENT;
	}

	SHOW("builtin %s\n", alias);
	return 0;
}

static void print_action(struct kmod_module *m, bool install,
							const char *options)
{
	const char *path;

	if (install) {
		printf("install %s %s\n", kmod_module_get_install_commands(m),
								options);
		return;
	}

	path = kmod_module_get_path(m);

	if (path == NULL) {
		/*
		 * Either a builtin module, or an alias, print only for
		 * builtin
		 */
		if (kmod_module_get_initstate(m) == KMOD_MODULE_BUILTIN)
			printf("builtin %s\n", kmod_module_get_name(m));
	} else
		printf("insmod %s %s\n", kmod_module_get_path(m), options);
}

static int insmod(struct kmod_ctx *ctx, const char *alias,
						const char *extra_options)
{
	struct kmod_list *l, *list = NULL;
	int err, flags = 0;

	void (*show)(struct kmod_module *m, bool install,
						const char *options) = NULL;

	err = kmod_module_new_from_lookup(ctx, alias, &list);
	if (err < 0)
		return err;

	if (list == NULL)
		return handle_failed_lookup(ctx, alias);

	if (strip_modversion || force)
		flags |= KMOD_PROBE_FORCE_MODVERSION;
	if (strip_vermagic || force)
		flags |= KMOD_PROBE_FORCE_VERMAGIC;
	if (ignore_commands)
		flags |= KMOD_PROBE_IGNORE_COMMAND;
	if (ignore_loaded)
		flags |= KMOD_PROBE_IGNORE_LOADED;
	if (dry_run)
		flags |= KMOD_PROBE_DRY_RUN;
	if (do_show || verbose > DEFAULT_VERBOSE)
		show = &print_action;

	flags |= KMOD_PROBE_APPLY_BLACKLIST_ALIAS_ONLY;

	if (use_blacklist)
		flags |= KMOD_PROBE_APPLY_BLACKLIST;
	if (first_time)
		flags |= KMOD_PROBE_FAIL_ON_LOADED;

	kmod_list_foreach(l, list) {
		struct kmod_module *mod = kmod_module_get_module(l);

		if (lookup_only)
			printf("%s\n", kmod_module_get_name(mod));
		else {
			err = kmod_module_probe_insert_module(mod, flags,
					extra_options, NULL, NULL, show);
		}

		if (err >= 0)
			/* ignore flag return values such as a mod being blacklisted */
			err = 0;
		else {
			switch (err) {
			case -EEXIST:
				ERR("could not insert '%s': Module already in kernel\n",
							kmod_module_get_name(mod));
				break;
			case -ENOENT:
				ERR("could not insert '%s': Unknown symbol in module, "
						"or unknown parameter (see dmesg)\n",
						kmod_module_get_name(mod));
				break;
			default:
				ERR("could not insert '%s': %s\n",
						kmod_module_get_name(mod),
						strerror(-err));
				break;
			}
		}

		kmod_module_unref(mod);
	}

	kmod_module_unref_list(list);
	return err;
}

static int insmod_all(struct kmod_ctx *ctx, char **args, int nargs)
{
	int i, err = 0;

	for (i = 0; i < nargs; i++) {
		int r = insmod(ctx, args[i], NULL);
		if (r < 0)
			err = r;
	}

	return err;
}

static void env_modprobe_options_append(const char *value)
{
	const char *old = getenv("MODPROBE_OPTIONS");
	char *env;

	if (old == NULL) {
		setenv("MODPROBE_OPTIONS", value, 1);
		return;
	}

	if (asprintf(&env, "%s %s", old, value) < 0) {
		ERR("could not append value to $MODPROBE_OPTIONS\n");
		return;
	}

	if (setenv("MODPROBE_OPTIONS", env, 1) < 0)
		ERR("could not setenv(MODPROBE_OPTIONS, \"%s\")\n", env);
	free(env);
}

static int options_from_array(char **args, int nargs, char **output)
{
	char *opts = NULL;
	size_t optslen = 0;
	int i, err = 0;

	for (i = 1; i < nargs; i++) {
		size_t len = strlen(args[i]);
		size_t qlen = 0;
		const char *value;
		void *tmp;

		value = strchr(args[i], '=');
		if (value) {
			value++;
			if (*value != '"' && *value != '\'') {
				if (strchr(value, ' '))
					qlen = 2;
			}
		}

		tmp = realloc(opts, optslen + len + qlen + 2);
		if (!tmp) {
			err = -errno;
			free(opts);
			opts = NULL;
			ERR("could not gather module options: out-of-memory\n");
			break;
		}
		opts = tmp;
		if (optslen > 0) {
			opts[optslen] = ' ';
			optslen++;
		}
		if (qlen == 0) {
			memcpy(opts + optslen, args[i], len + 1);
			optslen += len;
		} else {
			size_t keylen = value - args[i];
			size_t valuelen = len - keylen;
			memcpy(opts + optslen, args[i], keylen);
			optslen += keylen;
			opts[optslen] = '"';
			optslen++;
			memcpy(opts + optslen, value, valuelen);
			optslen += valuelen;
			opts[optslen] = '"';
			optslen++;
			opts[optslen] = '\0';
		}
	}

	*output = opts;
	return err;
}

static char **prepend_options_from_env(int *p_argc, char **orig_argv)
{
	const char *p, *env = getenv("MODPROBE_OPTIONS");
	char **new_argv, *str_start, *str_end, *str, *s, *quote;
	int i, argc = *p_argc;
	size_t envlen, space_count = 0;

	if (env == NULL)
		return orig_argv;

	for (p = env; *p != '\0'; p++) {
		if (*p == ' ')
			space_count++;
	}

	envlen = p - env;
	new_argv = malloc(sizeof(char *) * (argc + space_count + 3 + envlen));
	if (new_argv == NULL)
		return NULL;

	new_argv[0] = orig_argv[0];
	str_start = str = (char *) (new_argv + argc + space_count + 3);
	memcpy(str, env, envlen + 1);

	str_end = str_start + envlen;

	quote = NULL;
	for (i = 1, s = str; *s != '\0'; s++) {
		if (quote == NULL) {
			if (*s == ' ') {
				new_argv[i] = str;
				i++;
				*s = '\0';
				str = s + 1;
			} else if (*s == '"' || *s == '\'')
				quote = s;
		} else {
			if (*s == *quote) {
				if (quote == str) {
					new_argv[i] = str + 1;
					i++;
					*s = '\0';
					str = s + 1;
				} else {
					char *it;
					for (it = quote; it < s - 1; it++)
						it[0] = it[1];
					for (it = s - 1; it < str_end - 2; it++)
						it[0] = it[2];
					str_end -= 2;
					*str_end = '\0';
					s -= 2;
				}
				quote = NULL;
			}
		}
	}
	if (str < s) {
		new_argv[i] = str;
		i++;
	}

	memcpy(new_argv + i, orig_argv + 1, sizeof(char *) * (argc - 1));
	new_argv[i + argc] = NULL;
	*p_argc = i + argc - 1;

	return new_argv;
}

static int do_modprobe(int argc, char **orig_argv)
{
	struct kmod_ctx *ctx;
	char **args = NULL, **argv;
	const char **config_paths = NULL;
	int nargs = 0, n_config_paths = 0;
	char dirname_buf[PATH_MAX];
	const char *dirname = NULL;
	const char *root = NULL;
	const char *kversion = NULL;
	int use_all = 0;
	int do_remove = 0;
	int do_show_config = 0;
	int do_show_modversions = 0;
	int err;

	argv = prepend_options_from_env(&argc, orig_argv);
	if (argv == NULL) {
		ERR("Could not prepend options from command line\n");
		return EXIT_FAILURE;
	}

	for (;;) {
		int c, idx = 0;
		c = getopt_long(argc, argv, cmdopts_s, cmdopts, &idx);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
			log_priority = LOG_WARNING;
			use_all = 1;
			break;
		case 'r':
			do_remove = 1;
			break;
		case 5:
			remove_dependencies = 1;
			break;
		case 'R':
			lookup_only = 1;
			break;
		case 3:
			first_time = 1;
			break;
		case 'i':
			ignore_commands = 1;
			break;
		case 'b':
			use_blacklist = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 2:
			strip_modversion = 1;
			break;
		case 1:
			strip_vermagic = 1;
			break;
		case 'D':
			ignore_loaded = 1;
			dry_run = 1;
			do_show = 1;
			break;
		case 'c':
			do_show_config = 1;
			break;
		case 4:
			do_show_modversions = 1;
			break;
		case 'n':
			dry_run = 1;
			break;
		case 'C': {
			size_t bytes = sizeof(char *) * (n_config_paths + 2);
			void *tmp = realloc(config_paths, bytes);
			if (!tmp) {
				ERR("out-of-memory\n");
				err = -1;
				goto done;
			}
			config_paths = tmp;
			config_paths[n_config_paths] = optarg;
			n_config_paths++;
			config_paths[n_config_paths] = NULL;

			env_modprobe_options_append("-C");
			env_modprobe_options_append(optarg);
			break;
		}
		case 'd':
			root = optarg;
			break;
		case 'S':
			kversion = optarg;
			break;
		case 's':
			env_modprobe_options_append("-s");
			use_syslog = 1;
			break;
		case 'q':
			env_modprobe_options_append("-q");
			verbose = LOG_EMERG;
			break;
		case 'v':
			env_modprobe_options_append("-v");
			verbose++;
			break;
		case 'V':
			puts(PACKAGE " version " VERSION);
			err = 0;
			goto done;
		case 'h':
			help();
			err = 0;
			goto done;
		case '?':
			err = -1;
			goto done;
		default:
			ERR("unexpected getopt_long() value '%c'.\n", c);
			err = -1;
			goto done;
		}
	}

	args = argv + optind;
	nargs = argc - optind;

	log_open(use_syslog);

	if (!do_show_config) {
		if (nargs == 0) {
			ERR("missing parameters. See -h.\n");
			err = -1;
			goto done;
		}
	}

	if (root != NULL || kversion != NULL) {
		struct utsname u;
		if (root == NULL)
			root = "";
		if (kversion == NULL) {
			if (uname(&u) < 0) {
				ERR("uname() failed: %m\n");
				err = -1;
				goto done;
			}
			kversion = u.release;
		}
		snprintf(dirname_buf, sizeof(dirname_buf),
				"%s/lib/modules/%s", root,
				kversion);
		dirname = dirname_buf;
	}

	ctx = kmod_new(dirname, config_paths);
	if (!ctx) {
		ERR("kmod_new() failed!\n");
		err = -1;
		goto done;
	}

	log_setup_kmod_log(ctx, verbose);

	kmod_load_resources(ctx);

	if (do_show_config)
		err = show_config(ctx);
	else if (do_show_modversions)
		err = show_modversions(ctx, args[0]);
	else if (do_remove)
		err = rmmod_all(ctx, args, nargs);
	else if (use_all)
		err = insmod_all(ctx, args, nargs);
	else {
		char *opts;
		err = options_from_array(args, nargs, &opts);
		if (err == 0) {
			err = insmod(ctx, args[0], opts);
			free(opts);
		}
	}

	kmod_unref(ctx);

done:
	log_close();

	if (argv != orig_argv)
		free(argv);

	free(config_paths);

	return err >= 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

const struct kmod_cmd kmod_cmd_compat_modprobe = {
	.name = "modprobe",
	.cmd = do_modprobe,
	.help = "compat modprobe command",
};
