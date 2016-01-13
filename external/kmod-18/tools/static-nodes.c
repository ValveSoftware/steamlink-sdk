/*
 * kmod-static-nodes - manage modules.devname
 *
 * Copyright (C) 2004-2012 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 * Copyright (C) 2013 Tom Gundersen <teg@jklm.no>
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
#include <stddef.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "libkmod-util.h"

#include "kmod.h"

struct static_nodes_format {
	const char *name;
	int (*write)(FILE *, char[], char[], char, unsigned int, unsigned int);
	const char *description;
};

static const struct static_nodes_format static_nodes_format_human;
static const struct static_nodes_format static_nodes_format_tmpfiles;
static const struct static_nodes_format static_nodes_format_devname;

static const struct static_nodes_format *static_nodes_formats[] = {
	&static_nodes_format_human,
	&static_nodes_format_tmpfiles,
	&static_nodes_format_devname,
};

static const char cmdopts_s[] = "o:f:h";
static const struct option cmdopts[] = {
	{ "output", required_argument, 0, 'o'},
	{ "format", required_argument, 0, 'f'},
	{ "help", no_argument, 0, 'h'},
	{ },
};

static int write_human(FILE *out, char modname[], char devname[], char type, unsigned int maj, unsigned int min)
{
	int ret;

	ret = fprintf(out,
			"Module: %s\n"
			"\tDevice node: /dev/%s\n"
			"\t\tType: %s device\n"
			"\t\tMajor: %u\n"
			"\t\tMinor: %u\n",
			modname, devname,
			(type == 'c') ? "character" : "block", maj, min);
	if (ret >= 0)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

static const struct static_nodes_format static_nodes_format_human = {
	.name = "human",
	.write = write_human,
	.description = "(default) a human readable format. Do not parse.",
};

static int write_tmpfiles(FILE *out, char modname[], char devname[], char type, unsigned int maj, unsigned int min)
{
	const char *dir;
	int ret;

	dir = strrchr(devname, '/');
	if (dir) {
		ret = fprintf(out, "d /dev/%.*s 0755 - - -\n",
			      (int)(dir - devname), devname);
		if (ret < 0)
			return EXIT_FAILURE;
	}

	ret = fprintf(out, "%c /dev/%s 0600 - - - %u:%u\n",
		      type, devname, maj, min);
	if (ret < 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static const struct static_nodes_format static_nodes_format_tmpfiles = {
	.name = "tmpfiles",
	.write = write_tmpfiles,
	.description = "the tmpfiles.d(5) format used by systemd-tmpfiles.",
};

static int write_devname(FILE *out, char modname[], char devname[], char type, unsigned int maj, unsigned int min)
{
	int ret;

	ret = fprintf(out, "%s %s %c%u:%u\n", modname, devname, type, maj, min);
	if (ret >= 0)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

static const struct static_nodes_format static_nodes_format_devname = {
	.name = "devname",
	.write = write_devname,
	.description = "the modules.devname format.",
};

static void help(void)
{
	size_t i;

	printf("Usage:\n"
	       "\t%s static-nodes [options]\n"
	       "\n"
	       "kmod static-nodes outputs the static-node information of the currently running kernel.\n"
	       "\n"
	       "Options:\n"
	       "\t-f, --format=FORMAT  choose format to use: see \"Formats\"\n"
	       "\t-o, --output=FILE    write output to file\n"
	       "\t-h, --help           show this help\n"
	       "\n"
	       "Formats:\n",
	 program_invocation_short_name);

	for (i = 0; i < ARRAY_SIZE(static_nodes_formats); i++) {
		if (static_nodes_formats[i]->description != NULL) {
			printf("\t%-12s %s\n", static_nodes_formats[i]->name,
			       static_nodes_formats[i]->description);
		}
	}
}

static int do_static_nodes(int argc, char *argv[])
{
	struct utsname kernel;
	char modules[PATH_MAX], buf[4096];
	const char *output = "/dev/stdout";
	FILE *in = NULL, *out = NULL;
	const struct static_nodes_format *format = &static_nodes_format_human;
	int r, ret = EXIT_SUCCESS;

	for (;;) {
		int c, idx = 0, valid;
		size_t i;

		c = getopt_long(argc, argv, cmdopts_s, cmdopts, &idx);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'o':
			output = optarg;
			break;
		case 'f':
			valid = 0;

			for (i = 0; i < ARRAY_SIZE(static_nodes_formats); i++) {
				if (streq(static_nodes_formats[i]->name, optarg)) {
					format = static_nodes_formats[i];
					valid = 1;
				}
			}

			if (!valid) {
				fprintf(stderr, "Unknown format: '%s'.\n",
					optarg);
				help();
				ret = EXIT_FAILURE;
				goto finish;
			}
			break;
		case 'h':
			help();
			goto finish;
		case '?':
			ret = EXIT_FAILURE;
			goto finish;
		default:
			fprintf(stderr, "Unexpected commandline option '%c'.\n",
				c);
			help();
			ret = EXIT_FAILURE;
			goto finish;
		}
	}

	if (uname(&kernel) < 0) {
		fputs("Error: uname failed!\n", stderr);
		ret = EXIT_FAILURE;
		goto finish;
	}

	snprintf(modules, sizeof(modules), "/lib/modules/%s/modules.devname", kernel.release);
	in = fopen(modules, "re");
	if (in == NULL) {
		if (errno == ENOENT) {
			fprintf(stderr, "Warning: /lib/modules/%s/modules.devname not found - ignoring\n",
				kernel.release);
			ret = EXIT_SUCCESS;
		} else {
			fprintf(stderr, "Error: could not open /lib/modules/%s/modules.devname - %m\n",
				kernel.release);
			ret = EXIT_FAILURE;
		}
		goto finish;
	}

	r = mkdir_parents(output, 0755);
	if (r < 0) {
		fprintf(stderr, "Error: could not create parent directory for %s - %m.\n", output);
		ret = EXIT_FAILURE;
		goto finish;
	}

	out = fopen(output, "we");
	if (out == NULL) {
		fprintf(stderr, "Error: could not create %s - %m\n", output);
		ret = EXIT_FAILURE;
		goto finish;
	}

	while (fgets(buf, sizeof(buf), in) != NULL) {
		char modname[PATH_MAX];
		char devname[PATH_MAX];
		char type;
		unsigned int maj, min;
		int matches;

		if (buf[0] == '#')
			continue;

		matches = sscanf(buf, "%s %s %c%u:%u", modname, devname,
				 &type, &maj, &min);
		if (matches != 5 || (type != 'c' && type != 'b')) {
			fprintf(stderr, "Error: invalid devname entry: %s", buf);
			ret = EXIT_FAILURE;
			continue;
		}

		format->write(out, modname, devname, type, maj, min);
	}

finish:
	if (in)
		fclose(in);
	if (out)
		fclose(out);
	return ret;
}

const struct kmod_cmd kmod_cmd_static_nodes = {
	.name = "static-nodes",
	.cmd = do_static_nodes,
	.help = "outputs the static-node information installed with the currently running kernel",
};
