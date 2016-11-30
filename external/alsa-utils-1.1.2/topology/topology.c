/*
  Copyright(c) 2014-2015 Intel Corporation
  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <getopt.h>
#include <assert.h>

#include <alsa/asoundlib.h>
#include <alsa/topology.h>
#include "gettext.h"

static snd_output_t *log;

static void usage(char *name)
{
	printf(
_("Usage: %s [OPTIONS]...\n"
"\n"
"-h, --help              help\n"
"-c, --compile=FILE      compile file\n"
"-v, --verbose=LEVEL     set verbosity level (0...1)\n"
"-o, --output=FILE       set output file\n"
), name);
}

int main(int argc, char *argv[])
{
	snd_tplg_t *snd_tplg;
	static const char short_options[] = "hc:v:o:";
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"verbose", 0, 0, 'v'},
		{"compile", 0, 0, 'c'},
		{"output", 0, 0, 'o'},
		{0, 0, 0, 0},
	};
	char *source_file = NULL, *output_file = NULL;
	int c, err, verbose = 0, option_index;

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	textdomain(PACKAGE);
#endif

	err = snd_output_stdio_attach(&log, stderr, 0);
	assert(err >= 0);

	while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 'c':
			source_file = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		default:
			fprintf(stderr, _("Try `%s --help' for more information.\n"), argv[0]);
			return 1;
		}
	}

	if (source_file == NULL || output_file == NULL) {
		usage(argv[0]);
		return 1;
	}

	snd_tplg = snd_tplg_new();
	if (snd_tplg == NULL) {
		fprintf(stderr, _("failed to create new topology context\n"));
		return 1;
	}

	snd_tplg_verbose(snd_tplg, verbose);

	err = snd_tplg_build_file(snd_tplg, source_file, output_file);
	if (err < 0) {
		fprintf(stderr, _("failed to compile context %s\n"), source_file);
		snd_tplg_free(snd_tplg);
		return 1;
	}

	snd_tplg_free(snd_tplg);
	return 0;
}

