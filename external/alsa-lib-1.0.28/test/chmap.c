/*
 * channel mapping API test program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include "../include/asoundlib.h"

static void usage(void)
{
	printf("usage: chmap [options] query\n"
	       "       chmap [options] get\n"
	       "       chmap [options] set CH0 CH1 CH2...\n"
	       "options:\n"
	       "  -D device     Specify PCM device to handle\n"
	       "  -s stream     Specify PCM stream direction (playback/capture)\n"
	       "  -f format     PCM format\n"
	       "  -c channels   Channels\n"
	       "  -r rate       Sample rate\n");
}

static void print_channels(const snd_pcm_chmap_t *map)
{
	char tmp[128];
	if (snd_pcm_chmap_print(map, sizeof(tmp), tmp) > 0)
		printf("  %s\n", tmp);
}

static int query_chmaps(snd_pcm_t *pcm)
{
	snd_pcm_chmap_query_t **maps = snd_pcm_query_chmaps(pcm);
	snd_pcm_chmap_query_t **p, *v;

	if (!maps) {
		printf("Cannot query maps\n");
		return 1;
	}
	for (p = maps; (v = *p) != NULL; p++) {
		printf("Type = %s, Channels = %d\n",
		       snd_pcm_chmap_type_name(v->type),
		       v->map.channels);
		print_channels(&v->map);
	}
	snd_pcm_free_chmaps(maps);
	return 0;
}

static int setup_pcm(snd_pcm_t *pcm, int format, int channels, int rate)
{
	snd_pcm_hw_params_t *params;

	snd_pcm_hw_params_alloca(&params);
	if (snd_pcm_hw_params_any(pcm, params) < 0) {
		printf("Cannot init hw_params\n");
		return -1;
	}
	if (format != SND_PCM_FORMAT_UNKNOWN) {
		if (snd_pcm_hw_params_set_format(pcm, params, format) < 0) {
			printf("Cannot set format %s\n",
			       snd_pcm_format_name(format));
			return -1;
		}
	}
	if (channels > 0) {
		if (snd_pcm_hw_params_set_channels(pcm, params, channels) < 0) {
			printf("Cannot set channels %d\n", channels);
			return -1;
		}
	}
	if (rate > 0) {
		if (snd_pcm_hw_params_set_rate_near(pcm, params, (unsigned int *)&rate, 0) < 0) {
			printf("Cannot set rate %d\n", rate);
			return -1;
		}
	}
	if (snd_pcm_hw_params(pcm, params) < 0) {
		printf("Cannot set hw_params\n");
		return -1;
	}
	return 0;
}

static int get_chmap(snd_pcm_t *pcm, int format, int channels, int rate)
{
	snd_pcm_chmap_t *map;

	if (setup_pcm(pcm, format, channels, rate))
		return 1;
	map = snd_pcm_get_chmap(pcm);
	if (!map) {
		printf("Cannot get chmap\n");
		return 1;
	}
	printf("Channels = %d\n", map->channels);
	print_channels(map);
	free(map);
	return 0;
}

static int set_chmap(snd_pcm_t *pcm, int format, int channels, int rate,
		     int nargs, char **arg)
{
	int i;
	snd_pcm_chmap_t *map;

	if (channels && channels != nargs) {
		printf("Inconsistent channels %d vs %d\n", channels, nargs);
		return 1;
	}
	if (!channels) {
		if (!nargs) {
			printf("No channels are given\n");
			return 1;
		}
		channels = nargs;
	}
	if (setup_pcm(pcm, format, channels, rate))
		return 1;
	map = malloc(sizeof(int) * (channels + 1));
	if (!map) {
		printf("cannot malloc\n");
		return 1;
	}
	map->channels = channels;
	for (i = 0; i < channels; i++) {
		int val = snd_pcm_chmap_from_string(arg[i]);
		if (val < 0)
			val = SND_CHMAP_UNKNOWN;
		map->pos[i] = val;
	}
	if (snd_pcm_set_chmap(pcm, map) < 0) {
		printf("Cannot set chmap\n");
		return 1;
	}
	free(map);

	map = snd_pcm_get_chmap(pcm);
	if (!map) {
		printf("Cannot get chmap\n");
		return 1;
	}
	printf("Get channels = %d\n", map->channels);
	print_channels(map);
	free(map);
	return 0;
}

int main(int argc, char **argv)
{
	char *device = NULL;
	int stream = SND_PCM_STREAM_PLAYBACK;
	int format = SND_PCM_FORMAT_UNKNOWN;
	int channels = 0;
	int rate = 0;
	snd_pcm_t *pcm;
	int c;

	while ((c = getopt(argc, argv, "D:s:f:c:r:")) != -1) {
		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			if (*optarg == 'c' || *optarg == 'C')
				stream = SND_PCM_STREAM_CAPTURE;
			else
				stream = SND_PCM_STREAM_PLAYBACK;
			break;
		case 'f':
			format = snd_pcm_format_value(optarg);
			break;
		case 'c':
			channels = atoi(optarg);
			break;
		case 'r':
			rate = atoi(optarg);
			break;
		default:
			usage();
			return 1;
		}
	}

	if (argc <= optind) {
		usage();
		return 1;
	}

	if (!device) {
		printf("No device is specified\n");
		return 1;
	}

	if (snd_pcm_open(&pcm, device, stream, SND_PCM_NONBLOCK) < 0) {
		printf("Cannot open PCM stream %s for %s\n", device,
		       snd_pcm_stream_name(stream));
		return 1;
	}

	switch (*argv[optind]) {
	case 'q':
		return query_chmaps(pcm);
	case 'g':
		return get_chmap(pcm, format, channels, rate);
	case 's':
		return set_chmap(pcm, format, channels, rate,
				 argc - optind - 1, argv + optind + 1);
	}
	usage();
	return 1;
}
