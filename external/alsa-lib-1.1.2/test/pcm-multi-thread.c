/*
 * simple multi-thread stress test for PCM
 *
 * The main thread simply feeds or reads the sample data with the
 * random size continuously.  Meanwhile, the worker threads call some
 * update function depending on the given mode, and show the thread
 * number of the read value.
 *
 * The function for the worker thread is specified via -m option.
 * When the random mode ('r') is set, the update function is chosen
 * randomly in the loop.
 *
 * When the -v option is passed, this tries to show some obtained value
 * from the function.  Without -v, as default, it shows the thread number
 * (0-9).  In addition, it puts the mode suffix ('a' for avail, 'd' for
 * delay, etc) for the random mode, as well as the suffix '!' indicating
 * the error from the called function.
 */

#include <stdio.h>
#include <pthread.h>
#include <getopt.h>
#include "../include/asoundlib.h"

#define MAX_THREADS	10

enum {
	MODE_AVAIL_UPDATE,
	MODE_STATUS,
	MODE_HWSYNC,
	MODE_TIMESTAMP,
	MODE_DELAY,
	MODE_RANDOM
};

static char mode_suffix[] = {
	'a', 's', 'h', 't', 'd', 'r'
};

static const char *devname = "default";
static int stream = SND_PCM_STREAM_PLAYBACK;
static int num_threads = 1;
static int periodsize = 16 * 1024;
static int bufsize = 16 * 1024 * 4;
static int channels = 2;
static int rate = 48000;
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

static int running_mode = MODE_AVAIL_UPDATE;
static int show_value = 0;
static int quiet = 0;

static pthread_t peeper_threads[MAX_THREADS];
static int running = 1;
static snd_pcm_t *pcm;

static void *peeper(void *data)
{
	int thread_no = (long)data;
	snd_pcm_sframes_t val;
	snd_pcm_status_t *stat;
	snd_htimestamp_t tstamp;
	int mode = running_mode, err;

	snd_pcm_status_alloca(&stat);

	while (running) {
		if (running_mode == MODE_RANDOM)
			mode = rand() % MODE_RANDOM;
		switch (mode) {
		case MODE_AVAIL_UPDATE:
			val = snd_pcm_avail_update(pcm);
			err = 0;
			break;
		case MODE_STATUS:
			err = snd_pcm_status(pcm, stat);
			val = snd_pcm_status_get_avail(stat);
			break;
		case MODE_HWSYNC:
			err = snd_pcm_hwsync(pcm);
			break;
		case MODE_TIMESTAMP:
			err = snd_pcm_htimestamp(pcm, (snd_pcm_uframes_t *)&val,
						 &tstamp);
			break;
		default:
			err = snd_pcm_delay(pcm, &val);
			break;
		}

		if (quiet)
			continue;
		if (running_mode == MODE_RANDOM) {
			fprintf(stderr, "%d%c%s", thread_no, mode_suffix[mode],
				err ? "!" : "");
		} else {
			if (show_value && mode != MODE_HWSYNC)
				fprintf(stderr, "\r%d     ", (int)val);
			else
				fprintf(stderr, "%d%s", thread_no,
					err ? "!" : "");
		}
	}
	return NULL;
}

static void usage(void)
{
	fprintf(stderr, "usage: multi-thread [-options]\n");
	fprintf(stderr, "  -D str  Set device name\n");
	fprintf(stderr, "  -r val  Set sample rate\n");
	fprintf(stderr, "  -p val  Set period size (in frame)\n");
	fprintf(stderr, "  -b val  Set buffer size (in frame)\n");
	fprintf(stderr, "  -c val  Set number of channels\n");
	fprintf(stderr, "  -f str  Set PCM format\n");
	fprintf(stderr, "  -s str  Set stream direction (playback or capture)\n");
	fprintf(stderr, "  -t val  Set number of threads\n");
	fprintf(stderr, "  -m str  Running mode (avail, status, hwsync, timestamp, delay, random)\n");
	fprintf(stderr, "  -v      Show value\n");
	fprintf(stderr, "  -q      Quiet mode\n");
}

static int parse_options(int argc, char **argv)
{
	int c, i;

	while ((c = getopt(argc, argv, "D:r:f:p:b:s:t:m:vq")) >= 0) {
		switch (c) {
		case 'D':
			devname = optarg;
			break;
		case 'r':
			rate = atoi(optarg);
			break;
		case 'p':
			periodsize = atoi(optarg);
			break;
		case 'b':
			bufsize = atoi(optarg);
			break;
		case 'c':
			channels = atoi(optarg);
			break;
		case 'f':
			format = snd_pcm_format_value(optarg);
			break;
		case 's':
			if (*optarg == 'p' || *optarg == 'P')
				stream = SND_PCM_STREAM_PLAYBACK;
			else if (*optarg == 'c' || *optarg == 'C')
				stream = SND_PCM_STREAM_CAPTURE;
			else {
				fprintf(stderr, "invalid stream direction\n");
				return 1;
			}
			break;
		case 't':
			num_threads = atoi(optarg);
			if (num_threads < 1 || num_threads > MAX_THREADS) {
				fprintf(stderr, "invalid number of threads\n");
				return 1;
			}
			break;
		case 'm':
			for (i = 0; i <= MODE_RANDOM; i++)
				if (mode_suffix[i] == *optarg)
					break;
			if (i > MODE_RANDOM) {
				fprintf(stderr, "invalid mode type\n");
				return 1;
			}
			running_mode = i;
			break;
		case 'v':
			show_value = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage();
			return 1;
		}
	}
	return 0;
}

static int setup_params(void)
{
	snd_pcm_hw_params_t *hw;

	/* FIXME: more finer error checks */
	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(pcm, hw);
	snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(pcm, hw, format);
	snd_pcm_hw_params_set_channels(pcm, hw, channels);
	snd_pcm_hw_params_set_rate(pcm, hw, rate, 0);
	snd_pcm_hw_params_set_period_size(pcm, hw, periodsize, 0);
	snd_pcm_hw_params_set_buffer_size(pcm, hw, bufsize);
	if (snd_pcm_hw_params(pcm, hw) < 0) {
		fprintf(stderr, "snd_pcm_hw_params error\n");
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	char *buf;
	int i, err;

	if (parse_options(argc, argv))
		return 1;

	err = snd_pcm_open(&pcm, devname, stream, 0);
	if (err < 0) {
		fprintf(stderr, "cannot open pcm %s\n", devname);
		return 1;
	}

	if (setup_params())
		return 1;

	buf = calloc(1, snd_pcm_format_size(format, bufsize) * channels);
	if (!buf) {
		fprintf(stderr, "cannot alloc buffer\n");
		return 1;
	}

	for (i = 0; i < num_threads; i++) {
		if (pthread_create(&peeper_threads[i], NULL, peeper, (void *)(long)i)) {
			fprintf(stderr, "pthread_create error\n");
			return 1;
		}
	}

	if (stream == SND_PCM_STREAM_CAPTURE)
		snd_pcm_start(pcm);
	for (;;) {
		int size = rand() % (bufsize / 2);
		if (stream == SND_PCM_STREAM_PLAYBACK)
			err = snd_pcm_writei(pcm, buf, size);
		else
			err = snd_pcm_readi(pcm, buf, size);
		if (err < 0) {
			fprintf(stderr, "read/write error %d\n", err);
			err = snd_pcm_recover(pcm, err, 0);
			if (err < 0)
				break;
			if (stream == SND_PCM_STREAM_CAPTURE)
				snd_pcm_start(pcm);
		}
	}

	running = 0;
	for (i = 0; i < num_threads; i++)
		pthread_cancel(peeper_threads[i]);
	for (i = 0; i < num_threads; i++)
		pthread_join(peeper_threads[i], NULL);

	return 1;
}
