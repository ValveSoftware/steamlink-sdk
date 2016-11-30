/*
 *  A simple PCM loopback utility with adaptive sample rate support
 *
 *     Author: Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>
#include <syslog.h>
#include <signal.h>
#include "alsaloop.h"

struct loopback_thread {
	int threaded;
	pthread_t thread;
	int exitcode;
	struct loopback **loopbacks;
	int loopbacks_count;
	snd_output_t *output;
};

int quit = 0;
int verbose = 0;
int workarounds = 0;
int daemonize = 0;
int use_syslog = 0;
struct loopback **loopbacks = NULL;
int loopbacks_count = 0;
char **my_argv = NULL;
int my_argc = 0;
struct loopback_thread *threads;
int threads_count = 0;
pthread_t main_job;
int arg_default_xrun = 0;
int arg_default_wake = 0;

static void my_exit(struct loopback_thread *thread, int exitcode)
{
	int i;

	for (i = 0; i < thread->loopbacks_count; i++)
		pcmjob_done(thread->loopbacks[i]);
	if (thread->threaded) {
		thread->exitcode = exitcode;
		pthread_exit(0);
	}
	exit(exitcode);
}

static int create_loopback_handle(struct loopback_handle **_handle,
				  const char *device,
				  const char *ctldev,
				  const char *id)
{
	char idbuf[1024];
	struct loopback_handle *handle;

	handle = calloc(1, sizeof(*handle));
	if (handle == NULL)
		return -ENOMEM;
	if (device == NULL)
		device = "hw:0,0";
	handle->device = strdup(device);
	if (handle->device == NULL)
		return -ENOMEM;
	if (ctldev) {
		handle->ctldev = strdup(ctldev);
		if (handle->ctldev == NULL)
			return -ENOMEM;
	} else {
		handle->ctldev = NULL;
	}
	snprintf(idbuf, sizeof(idbuf)-1, "%s %s", id, device);
	idbuf[sizeof(idbuf)-1] = '\0';
	handle->id = strdup(idbuf);
	handle->access = SND_PCM_ACCESS_RW_INTERLEAVED;
	handle->format = SND_PCM_FORMAT_S16_LE;
	handle->rate = handle->rate_req = 48000;
	handle->channels = 2;
	handle->resample = 0;
	*_handle = handle;
	return 0;
}

static int create_loopback(struct loopback **_handle,
			   struct loopback_handle *play,
			   struct loopback_handle *capt,
			   snd_output_t *output)
{
	struct loopback *handle;

	handle = calloc(1, sizeof(*handle));
	if (handle == NULL)
		return -ENOMEM;
	handle->play = play;
	handle->capt = capt;
	play->loopback = handle;
	capt->loopback = handle;
	handle->latency_req = 0;
	handle->latency_reqtime = 10000;
	handle->loop_time = ~0UL;
	handle->loop_limit = ~0ULL;
	handle->output = output;
	handle->state = output;
#ifdef USE_SAMPLERATE
	handle->src_enable = 1;
	handle->src_converter_type = SRC_SINC_BEST_QUALITY;
#endif
	*_handle = handle;
	return 0;
}

static void set_loop_time(struct loopback *loop, unsigned long loop_time)
{
	loop->loop_time = loop_time;
	loop->loop_limit = loop->capt->rate * loop_time;
}

static void setscheduler(void)
{
	struct sched_param sched_param;

	if (sched_getparam(0, &sched_param) < 0) {
		logit(LOG_WARNING, "Scheduler getparam failed.\n");
		return;
	}
	sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
	if (!sched_setscheduler(0, SCHED_RR, &sched_param)) {
		if (verbose)
			logit(LOG_WARNING, "Scheduler set to Round Robin with priority %i\n", sched_param.sched_priority);
		return;
	}
	if (verbose)
		logit(LOG_INFO, "!!!Scheduler set to Round Robin with priority %i FAILED!\n", sched_param.sched_priority);
}

void help(void)
{
	int k;
	printf(
"Usage: alsaloop [OPTION]...\n\n"
"-h,--help      help\n"
"-g,--config    configuration file (one line = one job specified)\n"
"-d,--daemonize daemonize the main process and use syslog for errors\n"
"-P,--pdevice   playback device\n"
"-C,--cdevice   capture device\n"
"-X,--pctl      playback ctl device\n"
"-Y,--cctl      capture ctl device\n"
"-l,--latency   requested latency in frames\n"
"-t,--tlatency  requested latency in usec (1/1000000sec)\n"
"-f,--format    sample format\n"
"-c,--channels  channels\n"
"-r,--rate      rate\n"
"-n,--resample  resample in alsa-lib\n"
"-A,--samplerate use converter (0=sincbest,1=sincmedium,2=sincfastest,\n"
"                               3=zerohold,4=linear)\n"
"-B,--buffer    buffer size in frames\n"
"-E,--period    period size in frames\n"
"-s,--seconds   duration of loop in seconds\n"
"-b,--nblock    non-block mode (very early process wakeup)\n"
"-S,--sync      sync mode(0=none,1=simple,2=captshift,3=playshift,4=samplerate,\n"
"                         5=auto)\n"
"-a,--slave     stream parameters slave mode (0=auto, 1=on, 2=off)\n"
"-T,--thread    thread number (-1 = create unique)\n"
"-m,--mixer	redirect mixer, argument is:\n"
"		    SRC_SLAVE_ID(PLAYBACK)[@DST_SLAVE_ID(CAPTURE)]\n"
"-O,--ossmixer	rescan and redirect oss mixer, argument is:\n"
"		    ALSA_ID@OSS_ID  (for example: \"Master@VOLUME\")\n"
"-e,--effect    apply an effect (bandpass filter sweep)\n"
"-v,--verbose   verbose mode (more -v means more verbose)\n"
"-w,--workaround use workaround (serialopen)\n"
"-U,--xrun      xrun profiling\n"
"-W,--wake      process wake timeout in ms\n"
"-z,--syslog    use syslog for errors\n"
);
	printf("\nRecognized sample formats are:");
	for (k = 0; k < SND_PCM_FORMAT_LAST; ++k) {
		const char *s = snd_pcm_format_name(k);
		if (s)
			printf(" %s", s);
	}
	printf("\n\n");
	printf(
"Tip #1 (usable 500ms latency, good CPU usage, superb xrun prevention):\n"
"  alsaloop -t 500000\n"
"Tip #2 (superb 1ms latency, but heavy CPU usage):\n"
"  alsaloop -t 1000\n"
);
}

static long timediff(struct timeval t1, struct timeval t2)
{
	signed long l;

	t1.tv_sec -= t2.tv_sec;
	l = (signed long) t1.tv_usec - (signed long) t2.tv_usec;
	if (l < 0) {
		t1.tv_sec--;
		l = 1000000 + l;
		l %= 1000000;
	}
	return (t1.tv_sec * 1000000) + l;
}

static void add_loop(struct loopback *loop)
{
	loopbacks = realloc(loopbacks, (loopbacks_count + 1) *
						sizeof(struct loopback *));
	if (loopbacks == NULL) {
		logit(LOG_CRIT, "No enough memory\n");
		exit(EXIT_FAILURE);
	}
	loopbacks[loopbacks_count++] = loop;
}

static int init_mixer_control(struct loopback_control *control,
			      char *id)
{
	int err;

	err = snd_ctl_elem_id_malloc(&control->id);
	if (err < 0)
		return err;
	err = snd_ctl_elem_info_malloc(&control->info);
	if (err < 0)
		return err;
	err = snd_ctl_elem_value_malloc(&control->value);
	if (err < 0)
		return err;
	err = control_parse_id(id, control->id);
	if (err < 0)
		return err;
	return 0;
}

static int add_mixers(struct loopback *loop,
		      char **mixers,
		      int mixers_count)
{
	struct loopback_mixer *mixer, *last = NULL;
	char *str1;
	int err;

	while (mixers_count > 0) {
		mixer = calloc(1, sizeof(*mixer));
		if (mixer == NULL)
			return -ENOMEM;
		if (last)
			last->next = mixer;
		else
			loop->controls = mixer;
		last = mixer;
		str1 = strchr(*mixers, '@');
		if (str1)
			*str1 = '\0';
		err = init_mixer_control(&mixer->src, *mixers);
		if (err < 0) {
			logit(LOG_CRIT, "Wrong mixer control ID syntax '%s'\n", *mixers);
			return -EINVAL;
		}
		err = init_mixer_control(&mixer->dst, str1 ? str1 + 1 : *mixers);
		if (err < 0) {
			logit(LOG_CRIT, "Wrong mixer control ID syntax '%s'\n", str1 ? str1 + 1 : *mixers);
			return -EINVAL;
		}
		if (str1)
			*str1 = '@';
		mixers++;
		mixers_count--;
	}
	return 0;
}

static int add_oss_mixers(struct loopback *loop,
			  char **mixers,
			  int mixers_count)
{
	struct loopback_ossmixer *mixer, *last = NULL;
	char *str1, *str2;

	while (mixers_count > 0) {
		mixer = calloc(1, sizeof(*mixer));
		if (mixer == NULL)
			return -ENOMEM;
		if (last)
			last->next = mixer;
		else
			loop->oss_controls = mixer;
		last = mixer;
		str1 = strchr(*mixers, ',');
		if (str1)
			*str1 = '\0';
		str2 = strchr(str1 ? str1 + 1 : *mixers, '@');
		if (str2)
			*str2 = '\0';
		mixer->alsa_id = strdup(*mixers);
		if (str1)
			mixer->alsa_index = atoi(str1);
		mixer->oss_id = strdup(str2 ? str2 + 1 : *mixers);
		if (mixer->alsa_id == NULL || mixer->oss_id == NULL) {
			logit(LOG_CRIT, "Not enough memory");
			return -ENOMEM;
		}
		if (str1)
			*str1 = ',';
		if (str2)
			*str2 = ',';
		mixers++;
		mixers_count--;
	}
	return 0;
}

static void enable_syslog(void)
{
	if (!use_syslog) {
		use_syslog = 1;
		openlog("alsaloop", LOG_NDELAY|LOG_PID, LOG_DAEMON);
	}
}

static int parse_config_file(const char *file, snd_output_t *output);

static int parse_config(int argc, char *argv[], snd_output_t *output,
			int cmdline)
{
	struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"config", 1, NULL, 'g'},
		{"daemonize", 0, NULL, 'd'},
		{"pdevice", 1, NULL, 'P'},
		{"cdevice", 1, NULL, 'C'},
		{"pctl", 1, NULL, 'X'},
		{"cctl", 1, NULL, 'Y'},
		{"latency", 1, NULL, 'l'},
		{"tlatency", 1, NULL, 't'},
		{"format", 1, NULL, 'f'},
		{"channels", 1, NULL, 'c'},
		{"rate", 1, NULL, 'r'},
		{"buffer", 1, NULL, 'B'},
		{"period", 1, NULL, 'E'},
		{"seconds", 1, NULL, 's'},
		{"nblock", 0, NULL, 'b'},
		{"effect", 0, NULL, 'e'},
		{"verbose", 0, NULL, 'v'},
		{"resample", 0, NULL, 'n'},
		{"samplerate", 1, NULL, 'A'},
		{"sync", 1, NULL, 'S'},
		{"slave", 1, NULL, 'a'},
		{"thread", 1, NULL, 'T'},
		{"mixer", 1, NULL, 'm'},
		{"ossmixer", 1, NULL, 'O'},
		{"workaround", 1, NULL, 'w'},
		{"xrun", 0, NULL, 'U'},
		{"syslog", 0, NULL, 'z'},
		{NULL, 0, NULL, 0},
	};
	int err, morehelp;
	char *arg_config = NULL;
	char *arg_pdevice = NULL;
	char *arg_cdevice = NULL;
	char *arg_pctl = NULL;
	char *arg_cctl = NULL;
	unsigned int arg_latency_req = 0;
	unsigned int arg_latency_reqtime = 10000;
	snd_pcm_format_t arg_format = SND_PCM_FORMAT_S16_LE;
	unsigned int arg_channels = 2;
	unsigned int arg_rate = 48000;
	snd_pcm_uframes_t arg_buffer_size = 0;
	snd_pcm_uframes_t arg_period_size = 0;
	unsigned long arg_loop_time = ~0UL;
	int arg_nblock = 0;
	int arg_effect = 0;
	int arg_resample = 0;
#ifdef USE_SAMPLERATE
	int arg_samplerate = SRC_SINC_FASTEST + 1;
#endif
	int arg_sync = SYNC_TYPE_AUTO;
	int arg_slave = SLAVE_TYPE_AUTO;
	int arg_thread = 0;
	struct loopback *loop = NULL;
	char *arg_mixers[MAX_MIXERS];
	int arg_mixers_count = 0;
	char *arg_ossmixers[MAX_MIXERS];
	int arg_ossmixers_count = 0;
	int arg_xrun = arg_default_xrun;
	int arg_wake = arg_default_wake;

	morehelp = 0;
	while (1) {
		int c;
		if ((c = getopt_long(argc, argv,
				"hdg:P:C:X:Y:l:t:F:f:c:r:s:benvA:S:a:m:T:O:w:UW:z",
				long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			morehelp++;
			break;
		case 'g':
			arg_config = strdup(optarg);
			break;
		case 'd':
			daemonize = 1;
			enable_syslog();
			break;
		case 'P':
			arg_pdevice = strdup(optarg);
			break;
		case 'C':
			arg_cdevice = strdup(optarg);
			break;
		case 'X':
			arg_pctl = strdup(optarg);
			break;
		case 'Y':
			arg_cctl = strdup(optarg);
			break;
		case 'l':
			err = atoi(optarg);
			arg_latency_req = err >= 4 ? err : 4;
			break;
		case 't':
			err = atoi(optarg);
			arg_latency_reqtime = err >= 500 ? err : 500;
			break;
		case 'f':
			arg_format = snd_pcm_format_value(optarg);
			if (arg_format == SND_PCM_FORMAT_UNKNOWN) {
				logit(LOG_WARNING, "Unknown format, setting to default S16_LE\n");
				arg_format = SND_PCM_FORMAT_S16_LE;
			}
			break;
		case 'c':
			err = atoi(optarg);
			arg_channels = err >= 1 && err < 1024 ? err : 1;
			break;
		case 'r':
			err = atoi(optarg);
			arg_rate = err >= 4000 && err < 200000 ? err : 44100;
			break;
		case 'B':
			err = atoi(optarg);
			arg_buffer_size = err >= 32 && err < 200000 ? err : 0;
			break;
		case 'E':
			err = atoi(optarg);
			arg_period_size = err >= 32 && err < 200000 ? err : 0;
			break;
		case 's':
			err = atoi(optarg);
			arg_loop_time = err >= 1 && err <= 100000 ? err : 30;
			break;
		case 'b':
			arg_nblock = 1;
			break;
		case 'e':
			arg_effect = 1;
			break;
		case 'n':
			arg_resample = 1;
			break;
#ifdef USE_SAMPLERATE
		case 'A':
			if (strcasecmp(optarg, "sincbest") == 0)
				arg_samplerate = SRC_SINC_BEST_QUALITY;
			else if (strcasecmp(optarg, "sincmedium") == 0)
				arg_samplerate = SRC_SINC_MEDIUM_QUALITY;
			else if (strcasecmp(optarg, "sincfastest") == 0)
				arg_samplerate = SRC_SINC_FASTEST;
			else if (strcasecmp(optarg, "zerohold") == 0)
				arg_samplerate = SRC_ZERO_ORDER_HOLD;
			else if (strcasecmp(optarg, "linear") == 0)
				arg_samplerate = SRC_LINEAR;
			else
				arg_samplerate = atoi(optarg);
			if (arg_samplerate < 0 || arg_samplerate > SRC_LINEAR)
				arg_sync = SRC_SINC_FASTEST;
			arg_samplerate += 1;
			break;
#endif
		case 'S':
			if (strcasecmp(optarg, "samplerate") == 0)
				arg_sync = SYNC_TYPE_SAMPLERATE;
			else if (optarg[0] == 'n')
				arg_sync = SYNC_TYPE_NONE;
			else if (optarg[0] == 's')
				arg_sync = SYNC_TYPE_SIMPLE;
			else if (optarg[0] == 'c')
				arg_sync = SYNC_TYPE_CAPTRATESHIFT;
			else if (optarg[0] == 'p')
				arg_sync = SYNC_TYPE_PLAYRATESHIFT;
			else if (optarg[0] == 'r')
				arg_sync = SYNC_TYPE_SAMPLERATE;
			else
				arg_sync = atoi(optarg);
			if (arg_sync < 0 || arg_sync > SYNC_TYPE_LAST)
				arg_sync = SYNC_TYPE_AUTO;
			break;
		case 'a':
			if (optarg[0] == 'a')
				arg_slave = SLAVE_TYPE_AUTO;
			else if (strcasecmp(optarg, "on") == 0)
				arg_slave = SLAVE_TYPE_ON;
			else if (strcasecmp(optarg, "off") == 0)
				arg_slave = SLAVE_TYPE_OFF;
			else
				arg_slave = atoi(optarg);
			if (arg_slave < 0 || arg_slave > SLAVE_TYPE_LAST)
				arg_slave = SLAVE_TYPE_AUTO;
			break;
		case 'T':
			arg_thread = atoi(optarg);
			if (arg_thread < 0)
				arg_thread = 10000000 + loopbacks_count;
			break;
		case 'm':
			if (arg_mixers_count >= MAX_MIXERS) {
				logit(LOG_CRIT, "Maximum redirected mixer controls reached (max %i)\n", (int)MAX_MIXERS);
				exit(EXIT_FAILURE);
			}
			arg_mixers[arg_mixers_count++] = optarg;
			break;
		case 'O':
			if (arg_ossmixers_count >= MAX_MIXERS) {
				logit(LOG_CRIT, "Maximum redirected mixer controls reached (max %i)\n", (int)MAX_MIXERS);
				exit(EXIT_FAILURE);
			}
			arg_ossmixers[arg_ossmixers_count++] = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			if (strcasecmp(optarg, "serialopen") == 0)
				workarounds |= WORKAROUND_SERIALOPEN;
			break;
		case 'U':
			arg_xrun = 1;
			if (cmdline)
				arg_default_xrun = 1;
			break;
		case 'W':
			arg_wake = atoi(optarg);
			if (cmdline)
				arg_default_wake = arg_wake;
			break;
		case 'z':
			enable_syslog();
			break;
		}
	}

	if (morehelp) {
		help();
		exit(EXIT_SUCCESS);
	}
	if (arg_config == NULL) {
		struct loopback_handle *play;
		struct loopback_handle *capt;
		err = create_loopback_handle(&play, arg_pdevice, arg_pctl, "playback");
		if (err < 0) {
			logit(LOG_CRIT, "Unable to create playback handle.\n");
			exit(EXIT_FAILURE);
		}
		err = create_loopback_handle(&capt, arg_cdevice, arg_cctl, "capture");
		if (err < 0) {
			logit(LOG_CRIT, "Unable to create capture handle.\n");
			exit(EXIT_FAILURE);
		}
		err = create_loopback(&loop, play, capt, output);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to create loopback handle.\n");
			exit(EXIT_FAILURE);
		}
		play->format = capt->format = arg_format;
		play->rate = play->rate_req = capt->rate = capt->rate_req = arg_rate;
		play->channels = capt->channels = arg_channels;
		play->buffer_size_req = capt->buffer_size_req = arg_buffer_size;
		play->period_size_req = capt->period_size_req = arg_period_size;
		play->resample = capt->resample = arg_resample;
		play->nblock = capt->nblock = arg_nblock ? 1 : 0;
		loop->latency_req = arg_latency_req;
		loop->latency_reqtime = arg_latency_reqtime;
		loop->sync = arg_sync;
		loop->slave = arg_slave;
		loop->thread = arg_thread;
		loop->xrun = arg_xrun;
		loop->wake = arg_wake;
		err = add_mixers(loop, arg_mixers, arg_mixers_count);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to add mixer controls.\n");
			exit(EXIT_FAILURE);
		}
		err = add_oss_mixers(loop, arg_ossmixers, arg_ossmixers_count);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to add ossmixer controls.\n");
			exit(EXIT_FAILURE);
		}
#ifdef USE_SAMPLERATE
		loop->src_enable = arg_samplerate > 0;
		if (loop->src_enable)
			loop->src_converter_type = arg_samplerate - 1;
#endif
		set_loop_time(loop, arg_loop_time);
		add_loop(loop);
		return 0;
	}

	return parse_config_file(arg_config, output);
}

static int parse_config_file(const char *file, snd_output_t *output)
{
	FILE *fp;
	char line[2048], word[2048];
	char *str, *ptr;
	int argc, c, err = 0;
	char **argv;

	fp = fopen(file, "r");
	if (fp == NULL) {
		logit(LOG_CRIT, "Unable to open file '%s': %s\n", file, strerror(errno));
		return -EIO;
	}
	while (!feof(fp)) {
		if (fgets(line, sizeof(line)-1, fp) == NULL)
			break;
		line[sizeof(line)-1] = '\0';
		my_argv = realloc(my_argv, my_argc + MAX_ARGS * sizeof(char *));
		if (my_argv == NULL)
			return -ENOMEM;
		argv = my_argv + my_argc;
		argc = 0;
		argv[argc++] = strdup("<prog>");
		my_argc++;
		str = line;
		while (*str) {
			ptr = word;
			while (*str && (*str == ' ' || *str < ' '))
				str++;
			if (*str == '#')
				goto __next;
			if (*str == '\'' || *str == '\"') {
				c = *str++;
				while (*str && *str != c)
					*ptr++ = *str++;
				if (*str == c)
					str++;
			} else {
				while (*str && *str != ' ' && *str != '\t')
					*ptr++ = *str++;
			}
			if (ptr != word) {
				if (*(ptr-1) == '\n')
					ptr--;
				*ptr = '\0';
				if (argc >= MAX_ARGS) {
					logit(LOG_CRIT, "Too many arguments.");
					goto __error;
				}
				argv[argc++] = strdup(word);
				my_argc++;
			}
		}
		/* erase runtime variables for getopt */
		optarg = NULL;
		optind = opterr = 1;
		optopt = '?';

		err = parse_config(argc, argv, output, 0);
	      __next:
		if (err < 0)
			break;
		err = 0;
	}
      __error:
	fclose(fp);

	return err;
}

static void thread_job1(void *_data)
{
	struct loopback_thread *thread = _data;
	snd_output_t *output = thread->output;
	struct pollfd *pfds = NULL;
	int pfds_count = 0;
	int i, j, err, wake = 1000000;

	setscheduler();

	for (i = 0; i < thread->loopbacks_count; i++) {
		err = pcmjob_init(thread->loopbacks[i]);
		if (err < 0) {
			logit(LOG_CRIT, "Loopback initialization failure.\n");
			my_exit(thread, EXIT_FAILURE);
		}
	}
	for (i = 0; i < thread->loopbacks_count; i++) {
		err = pcmjob_start(thread->loopbacks[i]);
		if (err < 0) {
			logit(LOG_CRIT, "Loopback start failure.\n");
			my_exit(thread, EXIT_FAILURE);
		}
		pfds_count += thread->loopbacks[i]->pollfd_count;
		j = thread->loopbacks[i]->wake;
		if (j > 0 && j < wake)
			wake = j;
	}
	if (wake >= 1000000)
		wake = -1;
	pfds = calloc(pfds_count, sizeof(struct pollfd));
	if (pfds == NULL || pfds_count <= 0) {
		logit(LOG_CRIT, "Poll FDs allocation failed.\n");
		my_exit(thread, EXIT_FAILURE);
	}
	while (!quit) {
		struct timeval tv1, tv2;
		for (i = j = 0; i < thread->loopbacks_count; i++) {
			err = pcmjob_pollfds_init(thread->loopbacks[i], &pfds[j]);
			if (err < 0) {
				logit(LOG_CRIT, "Poll FD initialization failed.\n");
				my_exit(thread, EXIT_FAILURE);
			}
			j += err;
		}
		if (verbose > 10)
			gettimeofday(&tv1, NULL);
		err = poll(pfds, j, wake);
		if (err < 0)
			err = -errno;
		if (verbose > 10) {
			gettimeofday(&tv2, NULL);
			snd_output_printf(output, "pool took %lius\n", timediff(tv2, tv1));
		}
		if (err < 0) {
			if (err == -EINTR || err == -ERESTART)
				continue;
			logit(LOG_CRIT, "Poll failed: %s\n", strerror(-err));
			my_exit(thread, EXIT_FAILURE);
		}
		for (i = j = 0; i < thread->loopbacks_count; i++) {
			struct loopback *loop = thread->loopbacks[i];
			if (j < loop->active_pollfd_count) {
				err = pcmjob_pollfds_handle(loop, &pfds[j]);
				if (err < 0) {
					logit(LOG_CRIT, "pcmjob failed.\n");
					exit(EXIT_FAILURE);
				}
			}
			j += loop->active_pollfd_count;
		}
	}

	my_exit(thread, EXIT_SUCCESS);
}

static void thread_job(struct loopback_thread *thread)
{
	if (!thread->threaded) {
		thread_job1(thread);
		return;
	}
	pthread_create(&thread->thread, NULL, (void *) &thread_job1,
					      (void *) thread);
}

static void send_to_all(int sig)
{
	struct loopback_thread *thread;
	int i;

	for (i = 0; i < threads_count; i++) {
		thread = &threads[i];
		if (thread->threaded)
			pthread_kill(thread->thread, sig);
	}
}

static void signal_handler(int sig)
{
	quit = 1;
	send_to_all(SIGUSR2);
}

static void signal_handler_state(int sig)
{
	pthread_t self = pthread_self();
	struct loopback_thread *thread;
	int i, j;

	if (pthread_equal(main_job, self))
		send_to_all(SIGUSR1);
	for (i = 0; i < threads_count; i++) {
		thread = &threads[i];
		if (thread->thread == self) {
			for (j = 0; j < thread->loopbacks_count; j++)
				pcmjob_state(thread->loopbacks[j]);
		}
	}
	signal(sig, signal_handler_state);
}

static void signal_handler_ignore(int sig)
{
	signal(sig, signal_handler_ignore);
}

int main(int argc, char *argv[])
{
	snd_output_t *output;
	int i, j, k, l, err;

	err = snd_output_stdio_attach(&output, stdout, 0);
	if (err < 0) {
		logit(LOG_CRIT, "Output failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	err = parse_config(argc, argv, output, 1);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to parse arguments or configuration...\n");
		exit(EXIT_FAILURE);
	}
	while (my_argc > 0)
		free(my_argv[--my_argc]);
	free(my_argv);

	if (loopbacks_count <= 0) {
		logit(LOG_CRIT, "No loopback defined...\n");
		exit(EXIT_FAILURE);
	}

	if (daemonize) {
		if (daemon(0, 0) < 0) {
			logit(LOG_CRIT, "daemon() failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		i = fork();
		if (i < 0) {
			logit(LOG_CRIT, "fork() failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (i > 0) {
			/* wait(&i); */
			exit(EXIT_SUCCESS);
		}
	}

	/* we must sort thread IDs */
	j = -1;
	do {
		k = 0x7fffffff;
		for (i = 0; i < loopbacks_count; i++) {
			if (loopbacks[i]->thread < k &&
			    loopbacks[i]->thread > j)
				k = loopbacks[i]->thread;
		}
		j++;
		for (i = 0; i < loopbacks_count; i++) {
			if (loopbacks[i]->thread == k)
				loopbacks[i]->thread = j;
		}
	} while (k != 0x7fffffff);
	/* fix maximum thread id */
	for (i = 0, j = -1; i < loopbacks_count; i++) {
		if (loopbacks[i]->thread > j)
			j = loopbacks[i]->thread;
	}
	j += 1;
	threads = calloc(1, sizeof(struct loopback_thread) * j);
	if (threads == NULL) {
		logit(LOG_CRIT, "No enough memory\n");
		exit(EXIT_FAILURE);
	}
	/* sort all threads */
	for (k = 0; k < j; k++) {
		for (i = l = 0; i < loopbacks_count; i++)
			if (loopbacks[i]->thread == k)
				l++;
		threads[k].loopbacks = malloc(l * sizeof(struct loopback *));
		threads[k].loopbacks_count = l;
		threads[k].output = output;
		threads[k].threaded = j > 1;
		for (i = l = 0; i < loopbacks_count; i++)
			if (loopbacks[i]->thread == k)
				threads[k].loopbacks[l++] = loopbacks[i];
	}
	threads_count = j;
	main_job = pthread_self();
 
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGUSR1, signal_handler_state);
	signal(SIGUSR2, signal_handler_ignore);

	for (k = 0; k < threads_count; k++)
		thread_job(&threads[k]);

	if (j > 1) {
		for (k = 0; k < threads_count; k++)
			pthread_join(threads[k].thread, NULL);
	}

	if (use_syslog)
		closelog();
	exit(EXIT_SUCCESS);
}
