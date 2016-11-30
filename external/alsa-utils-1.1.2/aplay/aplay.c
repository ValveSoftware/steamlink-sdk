/*
 *  aplay.c - plays and records
 *
 *      CREATIVE LABS CHANNEL-files
 *      Microsoft WAVE-files
 *      SPARC AUDIO .AU-files
 *      Raw Data
 *
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Based on vplay program by Michael Beck
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

#define _GNU_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <termios.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <endian.h>
#include "aconfig.h"
#include "gettext.h"
#include "formats.h"
#include "version.h"

#ifdef SND_CHMAP_API_VERSION
#define CONFIG_SUPPORT_CHMAP	1
#endif

#ifndef LLONG_MAX
#define LLONG_MAX    9223372036854775807LL
#endif

#ifndef le16toh
#include <asm/byteorder.h>
#define le16toh(x) __le16_to_cpu(x)
#define be16toh(x) __be16_to_cpu(x)
#define le32toh(x) __le32_to_cpu(x)
#define be32toh(x) __be32_to_cpu(x)
#endif

#define DEFAULT_FORMAT		SND_PCM_FORMAT_U8
#define DEFAULT_SPEED 		8000

#define FORMAT_DEFAULT		-1
#define FORMAT_RAW		0
#define FORMAT_VOC		1
#define FORMAT_WAVE		2
#define FORMAT_AU		3

/* global data */

static snd_pcm_sframes_t (*readi_func)(snd_pcm_t *handle, void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writei_func)(snd_pcm_t *handle, const void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*readn_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writen_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);

enum {
	VUMETER_NONE,
	VUMETER_MONO,
	VUMETER_STEREO
};

static char *command;
static snd_pcm_t *handle;
static struct {
	snd_pcm_format_t format;
	unsigned int channels;
	unsigned int rate;
} hwparams, rhwparams;
static int timelimit = 0;
static int quiet_mode = 0;
static int file_type = FORMAT_DEFAULT;
static int open_mode = 0;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int mmap_flag = 0;
static int interleaved = 1;
static int nonblock = 0;
static volatile sig_atomic_t in_aborting = 0;
static u_char *audiobuf = NULL;
static snd_pcm_uframes_t chunk_size = 0;
static unsigned period_time = 0;
static unsigned buffer_time = 0;
static snd_pcm_uframes_t period_frames = 0;
static snd_pcm_uframes_t buffer_frames = 0;
static int avail_min = -1;
static int start_delay = 0;
static int stop_delay = 0;
static int monotonic = 0;
static int interactive = 0;
static int can_pause = 0;
static int fatal_errors = 0;
static int verbose = 0;
static int vumeter = VUMETER_NONE;
static int buffer_pos = 0;
static size_t significant_bits_per_sample, bits_per_sample, bits_per_frame;
static size_t chunk_bytes;
static int test_position = 0;
static int test_coef = 8;
static int test_nowait = 0;
static snd_output_t *log;
static long long max_file_size = 0;
static int max_file_time = 0;
static int use_strftime = 0;
volatile static int recycle_capture_file = 0;
static long term_c_lflag = -1;
static int dump_hw_params = 0;

static int fd = -1;
static off64_t pbrec_count = LLONG_MAX, fdcount;
static int vocmajor, vocminor;

static char *pidfile_name = NULL;
FILE *pidf = NULL;
static int pidfile_written = 0;

#ifdef CONFIG_SUPPORT_CHMAP
static snd_pcm_chmap_t *channel_map = NULL; /* chmap to override */
static unsigned int *hw_map = NULL; /* chmap to follow */
#endif

/* needed prototypes */

static void done_stdin(void);

static void playback(char *filename);
static void capture(char *filename);
static void playbackv(char **filenames, unsigned int count);
static void capturev(char **filenames, unsigned int count);

static void begin_voc(int fd, size_t count);
static void end_voc(int fd);
static void begin_wave(int fd, size_t count);
static void end_wave(int fd);
static void begin_au(int fd, size_t count);
static void end_au(int fd);

static const struct fmt_capture {
	void (*start) (int fd, size_t count);
	void (*end) (int fd);
	char *what;
	long long max_filesize;
} fmt_rec_table[] = {
	{	NULL,		NULL,		N_("raw data"),		LLONG_MAX },
	{	begin_voc,	end_voc,	N_("VOC"),		16000000LL },
	/* FIXME: can WAV handle exactly 2GB or less than it? */
	{	begin_wave,	end_wave,	N_("WAVE"),		2147483648LL },
	{	begin_au,	end_au,		N_("Sparc Audio"),	LLONG_MAX }
};

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define error(...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	putc('\n', stderr); \
} while (0)
#else
#define error(args...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, ##args); \
	putc('\n', stderr); \
} while (0)
#endif	

static void usage(char *command)
{
	snd_pcm_format_t k;
	printf(
_("Usage: %s [OPTION]... [FILE]...\n"
"\n"
"-h, --help              help\n"
"    --version           print current version\n"
"-l, --list-devices      list all soundcards and digital audio devices\n"
"-L, --list-pcms         list device names\n"
"-D, --device=NAME       select PCM by name\n"
"-q, --quiet             quiet mode\n"
"-t, --file-type TYPE    file type (voc, wav, raw or au)\n"
"-c, --channels=#        channels\n"
"-f, --format=FORMAT     sample format (case insensitive)\n"
"-r, --rate=#            sample rate\n"
"-d, --duration=#        interrupt after # seconds\n"
"-M, --mmap              mmap stream\n"
"-N, --nonblock          nonblocking mode\n"
"-F, --period-time=#     distance between interrupts is # microseconds\n"
"-B, --buffer-time=#     buffer duration is # microseconds\n"
"    --period-size=#     distance between interrupts is # frames\n"
"    --buffer-size=#     buffer duration is # frames\n"
"-A, --avail-min=#       min available space for wakeup is # microseconds\n"
"-R, --start-delay=#     delay for automatic PCM start is # microseconds \n"
"                        (relative to buffer size if <= 0)\n"
"-T, --stop-delay=#      delay for automatic PCM stop is # microseconds from xrun\n"
"-v, --verbose           show PCM structure and setup (accumulative)\n"
"-V, --vumeter=TYPE      enable VU meter (TYPE: mono or stereo)\n"
"-I, --separate-channels one file for each channel\n"
"-i, --interactive       allow interactive operation from stdin\n"
"-m, --chmap=ch1,ch2,..  Give the channel map to override or follow\n"
"    --disable-resample  disable automatic rate resample\n"
"    --disable-channels  disable automatic channel conversions\n"
"    --disable-format    disable automatic format conversions\n"
"    --disable-softvol   disable software volume control (softvol)\n"
"    --test-position     test ring buffer position\n"
"    --test-coef=#       test coefficient for ring buffer position (default 8)\n"
"                        expression for validation is: coef * (buffer_size / 2)\n"
"    --test-nowait       do not wait for ring buffer - eats whole CPU\n"
"    --max-file-time=#   start another output file when the old file has recorded\n"
"                        for this many seconds\n"
"    --process-id-file   write the process ID here\n"
"    --use-strftime      apply the strftime facility to the output file name\n"
"    --dump-hw-params    dump hw_params of the device\n"
"    --fatal-errors      treat all errors as fatal\n"
  )
		, command);
	printf(_("Recognized sample formats are:"));
	for (k = 0; k <= SND_PCM_FORMAT_LAST; ++k) {
		const char *s = snd_pcm_format_name(k);
		if (s)
			printf(" %s", s);
	}
	printf(_("\nSome of these may not be available on selected hardware\n"));
	printf(_("The available format shortcuts are:\n"));
	printf(_("-f cd (16 bit little endian, 44100, stereo)\n"));
	printf(_("-f cdr (16 bit big endian, 44100, stereo)\n"));
	printf(_("-f dat (16 bit little endian, 48000, stereo)\n"));
}

static void device_list(void)
{
	snd_ctl_t *handle;
	int card, err, dev, idx;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);

	card = -1;
	if (snd_card_next(&card) < 0 || card < 0) {
		error(_("no soundcards found..."));
		return;
	}
	printf(_("**** List of %s Hardware Devices ****\n"),
	       snd_pcm_stream_name(stream));
	while (card >= 0) {
		char name[32];
		sprintf(name, "hw:%d", card);
		if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
			error("control open (%i): %s", card, snd_strerror(err));
			goto next_card;
		}
		if ((err = snd_ctl_card_info(handle, info)) < 0) {
			error("control hardware info (%i): %s", card, snd_strerror(err));
			snd_ctl_close(handle);
			goto next_card;
		}
		dev = -1;
		while (1) {
			unsigned int count;
			if (snd_ctl_pcm_next_device(handle, &dev)<0)
				error("snd_ctl_pcm_next_device");
			if (dev < 0)
				break;
			snd_pcm_info_set_device(pcminfo, dev);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, stream);
			if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
				if (err != -ENOENT)
					error("control digital audio info (%i): %s", card, snd_strerror(err));
				continue;
			}
			printf(_("card %i: %s [%s], device %i: %s [%s]\n"),
				card, snd_ctl_card_info_get_id(info), snd_ctl_card_info_get_name(info),
				dev,
				snd_pcm_info_get_id(pcminfo),
				snd_pcm_info_get_name(pcminfo));
			count = snd_pcm_info_get_subdevices_count(pcminfo);
			printf( _("  Subdevices: %i/%i\n"),
				snd_pcm_info_get_subdevices_avail(pcminfo), count);
			for (idx = 0; idx < (int)count; idx++) {
				snd_pcm_info_set_subdevice(pcminfo, idx);
				if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
					error("control digital audio playback info (%i): %s", card, snd_strerror(err));
				} else {
					printf(_("  Subdevice #%i: %s\n"),
						idx, snd_pcm_info_get_subdevice_name(pcminfo));
				}
			}
		}
		snd_ctl_close(handle);
	next_card:
		if (snd_card_next(&card) < 0) {
			error("snd_card_next");
			break;
		}
	}
}

static void pcm_list(void)
{
	void **hints, **n;
	char *name, *descr, *descr1, *io;
	const char *filter;

	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return;
	n = hints;
	filter = stream == SND_PCM_STREAM_CAPTURE ? "Input" : "Output";
	while (*n != NULL) {
		name = snd_device_name_get_hint(*n, "NAME");
		descr = snd_device_name_get_hint(*n, "DESC");
		io = snd_device_name_get_hint(*n, "IOID");
		if (io != NULL && strcmp(io, filter) != 0)
			goto __end;
		printf("%s\n", name);
		if ((descr1 = descr) != NULL) {
			printf("    ");
			while (*descr1) {
				if (*descr1 == '\n')
					printf("\n    ");
				else
					putchar(*descr1);
				descr1++;
			}
			putchar('\n');
		}
	      __end:
	      	if (name != NULL)
	      		free(name);
		if (descr != NULL)
			free(descr);
		if (io != NULL)
			free(io);
		n++;
	}
	snd_device_name_free_hint(hints);
}

static void version(void)
{
	printf("%s: version " SND_UTIL_VERSION_STR " by Jaroslav Kysela <perex@perex.cz>\n", command);
}

/*
 *	Subroutine to clean up before exit.
 */
static void prg_exit(int code) 
{
	done_stdin();
	if (handle)
		snd_pcm_close(handle);
	if (pidfile_written)
		remove (pidfile_name);
	exit(code);
}

static void signal_handler(int sig)
{
	if (in_aborting)
		return;

	in_aborting = 1;
	if (verbose==2)
		putchar('\n');
	if (!quiet_mode)
		fprintf(stderr, _("Aborted by signal %s...\n"), strsignal(sig));
	if (handle)
		snd_pcm_abort(handle);
	if (sig == SIGABRT) {
		/* do not call snd_pcm_close() and abort immediately */
		handle = NULL;
		prg_exit(EXIT_FAILURE);
	}
	signal(sig, SIG_DFL);
}

/* call on SIGUSR1 signal. */
static void signal_handler_recycle (int sig)
{
	/* flag the capture loop to start a new output file */
	recycle_capture_file = 1;
}

enum {
	OPT_VERSION = 1,
	OPT_PERIOD_SIZE,
	OPT_BUFFER_SIZE,
	OPT_DISABLE_RESAMPLE,
	OPT_DISABLE_CHANNELS,
	OPT_DISABLE_FORMAT,
	OPT_DISABLE_SOFTVOL,
	OPT_TEST_POSITION,
	OPT_TEST_COEF,
	OPT_TEST_NOWAIT,
	OPT_MAX_FILE_TIME,
	OPT_PROCESS_ID_FILE,
	OPT_USE_STRFTIME,
	OPT_DUMP_HWPARAMS,
	OPT_FATAL_ERRORS,
};

static long parse_long(const char *str, int *err)
{
	long val;
	char *endptr;

	errno = 0;
	val = strtol(str, &endptr, 0);

	if (errno != 0 || *endptr != '\0')
		*err = -1;
	else
		*err = 0;

	return val;
}

int main(int argc, char *argv[])
{
	int option_index;
	static const char short_options[] = "hnlLD:qt:c:f:r:d:MNF:A:R:T:B:vV:IPCi"
#ifdef CONFIG_SUPPORT_CHMAP
		"m:"
#endif
		;
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, OPT_VERSION},
		{"list-devnames", 0, 0, 'n'},
		{"list-devices", 0, 0, 'l'},
		{"list-pcms", 0, 0, 'L'},
		{"device", 1, 0, 'D'},
		{"quiet", 0, 0, 'q'},
		{"file-type", 1, 0, 't'},
		{"channels", 1, 0, 'c'},
		{"format", 1, 0, 'f'},
		{"rate", 1, 0, 'r'},
		{"duration", 1, 0 ,'d'},
		{"mmap", 0, 0, 'M'},
		{"nonblock", 0, 0, 'N'},
		{"period-time", 1, 0, 'F'},
		{"period-size", 1, 0, OPT_PERIOD_SIZE},
		{"avail-min", 1, 0, 'A'},
		{"start-delay", 1, 0, 'R'},
		{"stop-delay", 1, 0, 'T'},
		{"buffer-time", 1, 0, 'B'},
		{"buffer-size", 1, 0, OPT_BUFFER_SIZE},
		{"verbose", 0, 0, 'v'},
		{"vumeter", 1, 0, 'V'},
		{"separate-channels", 0, 0, 'I'},
		{"playback", 0, 0, 'P'},
		{"capture", 0, 0, 'C'},
		{"disable-resample", 0, 0, OPT_DISABLE_RESAMPLE},
		{"disable-channels", 0, 0, OPT_DISABLE_CHANNELS},
		{"disable-format", 0, 0, OPT_DISABLE_FORMAT},
		{"disable-softvol", 0, 0, OPT_DISABLE_SOFTVOL},
		{"test-position", 0, 0, OPT_TEST_POSITION},
		{"test-coef", 1, 0, OPT_TEST_COEF},
		{"test-nowait", 0, 0, OPT_TEST_NOWAIT},
		{"max-file-time", 1, 0, OPT_MAX_FILE_TIME},
		{"process-id-file", 1, 0, OPT_PROCESS_ID_FILE},
		{"use-strftime", 0, 0, OPT_USE_STRFTIME},
		{"interactive", 0, 0, 'i'},
		{"dump-hw-params", 0, 0, OPT_DUMP_HWPARAMS},
		{"fatal-errors", 0, 0, OPT_FATAL_ERRORS},
#ifdef CONFIG_SUPPORT_CHMAP
		{"chmap", 1, 0, 'm'},
#endif
		{0, 0, 0, 0}
	};
	char *pcm_name = "default";
	int tmp, err, c;
	int do_device_list = 0, do_pcm_list = 0;
	snd_pcm_info_t *info;
	FILE *direction;

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	textdomain(PACKAGE);
#endif

	snd_pcm_info_alloca(&info);

	err = snd_output_stdio_attach(&log, stderr, 0);
	assert(err >= 0);

	command = argv[0];
	file_type = FORMAT_DEFAULT;
	if (strstr(argv[0], "arecord")) {
		stream = SND_PCM_STREAM_CAPTURE;
		file_type = FORMAT_WAVE;
		command = "arecord";
		start_delay = 1;
		direction = stdout;
	} else if (strstr(argv[0], "aplay")) {
		stream = SND_PCM_STREAM_PLAYBACK;
		command = "aplay";
		direction = stdin;
	} else {
		error(_("command should be named either arecord or aplay"));
		return 1;
	}

	if (isatty(fileno(direction)) && (argc == 1)) {
		usage(command);
		return 1;
	}

	chunk_size = -1;
	rhwparams.format = DEFAULT_FORMAT;
	rhwparams.rate = DEFAULT_SPEED;
	rhwparams.channels = 1;

	while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			usage(command);
			return 0;
		case OPT_VERSION:
			version();
			return 0;
		case 'l':
			do_device_list = 1;
			break;
		case 'L':
			do_pcm_list = 1;
			break;
		case 'D':
			pcm_name = optarg;
			break;
		case 'q':
			quiet_mode = 1;
			break;
		case 't':
			if (strcasecmp(optarg, "raw") == 0)
				file_type = FORMAT_RAW;
			else if (strcasecmp(optarg, "voc") == 0)
				file_type = FORMAT_VOC;
			else if (strcasecmp(optarg, "wav") == 0)
				file_type = FORMAT_WAVE;
			else if (strcasecmp(optarg, "au") == 0 || strcasecmp(optarg, "sparc") == 0)
				file_type = FORMAT_AU;
			else {
				error(_("unrecognized file format %s"), optarg);
				return 1;
			}
			break;
		case 'c':
			rhwparams.channels = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid channels argument '%s'"), optarg);
				return 1;
			}
			if (rhwparams.channels < 1 || rhwparams.channels > 256) {
				error(_("value %i for channels is invalid"), rhwparams.channels);
				return 1;
			}
			break;
		case 'f':
			if (strcasecmp(optarg, "cd") == 0 || strcasecmp(optarg, "cdr") == 0) {
				if (strcasecmp(optarg, "cdr") == 0)
					rhwparams.format = SND_PCM_FORMAT_S16_BE;
				else
					rhwparams.format = file_type == FORMAT_AU ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_S16_LE;
				rhwparams.rate = 44100;
				rhwparams.channels = 2;
			} else if (strcasecmp(optarg, "dat") == 0) {
				rhwparams.format = file_type == FORMAT_AU ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_S16_LE;
				rhwparams.rate = 48000;
				rhwparams.channels = 2;
			} else {
				rhwparams.format = snd_pcm_format_value(optarg);
				if (rhwparams.format == SND_PCM_FORMAT_UNKNOWN) {
					error(_("wrong extended format '%s'"), optarg);
					prg_exit(EXIT_FAILURE);
				}
			}
			break;
		case 'r':
			tmp = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid rate argument '%s'"), optarg);
				return 1;
			}
			if (tmp < 300)
				tmp *= 1000;
			rhwparams.rate = tmp;
			if (tmp < 2000 || tmp > 192000) {
				error(_("bad speed value %i"), tmp);
				return 1;
			}
			break;
		case 'd':
			timelimit = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid duration argument '%s'"), optarg);
				return 1;
			}
			break;
		case 'N':
			nonblock = 1;
			open_mode |= SND_PCM_NONBLOCK;
			break;
		case 'F':
			period_time = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid period time argument '%s'"), optarg);
				return 1;
			}
			break;
		case 'B':
			buffer_time = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid buffer time argument '%s'"), optarg);
				return 1;
			}
			break;
		case OPT_PERIOD_SIZE:
			period_frames = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid period size argument '%s'"), optarg);
				return 1;
			}
			break;
		case OPT_BUFFER_SIZE:
			buffer_frames = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid buffer size argument '%s'"), optarg);
				return 1;
			}
			break;
		case 'A':
			avail_min = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid min available space argument '%s'"), optarg);
				return 1;
			}
			break;
		case 'R':
			start_delay = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid start delay argument '%s'"), optarg);
				return 1;
			}
			break;
		case 'T':
			stop_delay = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid stop delay argument '%s'"), optarg);
				return 1;
			}
			break;
		case 'v':
			verbose++;
			if (verbose > 1 && !vumeter)
				vumeter = VUMETER_MONO;
			break;
		case 'V':
			if (*optarg == 's')
				vumeter = VUMETER_STEREO;
			else if (*optarg == 'm')
				vumeter = VUMETER_MONO;
			else
				vumeter = VUMETER_NONE;
			break;
		case 'M':
			mmap_flag = 1;
			break;
		case 'I':
			interleaved = 0;
			break;
		case 'P':
			stream = SND_PCM_STREAM_PLAYBACK;
			command = "aplay";
			break;
		case 'C':
			stream = SND_PCM_STREAM_CAPTURE;
			command = "arecord";
			start_delay = 1;
			if (file_type == FORMAT_DEFAULT)
				file_type = FORMAT_WAVE;
			break;
		case 'i':
			interactive = 1;
			break;
		case OPT_DISABLE_RESAMPLE:
			open_mode |= SND_PCM_NO_AUTO_RESAMPLE;
			break;
		case OPT_DISABLE_CHANNELS:
			open_mode |= SND_PCM_NO_AUTO_CHANNELS;
			break;
		case OPT_DISABLE_FORMAT:
			open_mode |= SND_PCM_NO_AUTO_FORMAT;
			break;
		case OPT_DISABLE_SOFTVOL:
			open_mode |= SND_PCM_NO_SOFTVOL;
			break;
		case OPT_TEST_POSITION:
			test_position = 1;
			break;
		case OPT_TEST_COEF:
			test_coef = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid test coef argument '%s'"), optarg);
				return 1;
			}
			if (test_coef < 1)
				test_coef = 1;
			break;
		case OPT_TEST_NOWAIT:
			test_nowait = 1;
			break;
		case OPT_MAX_FILE_TIME:
			max_file_time = parse_long(optarg, &err);
			if (err < 0) {
				error(_("invalid max file time argument '%s'"), optarg);
				return 1;
			}
			break;
		case OPT_PROCESS_ID_FILE:
			pidfile_name = optarg;
			break;
		case OPT_USE_STRFTIME:
			use_strftime = 1;
			break;
		case OPT_DUMP_HWPARAMS:
			dump_hw_params = 1;
			break;
		case OPT_FATAL_ERRORS:
			fatal_errors = 1;
			break;
#ifdef CONFIG_SUPPORT_CHMAP
		case 'm':
			channel_map = snd_pcm_chmap_parse_string(optarg);
			if (!channel_map) {
				fprintf(stderr, _("Unable to parse channel map string: %s\n"), optarg);
				return 1;
			}
			break;
#endif
		default:
			fprintf(stderr, _("Try `%s --help' for more information.\n"), command);
			return 1;
		}
	}

	if (do_device_list) {
		if (do_pcm_list) pcm_list();
		device_list();
		goto __end;
	} else if (do_pcm_list) {
		pcm_list();
		goto __end;
	}

	err = snd_pcm_open(&handle, pcm_name, stream, open_mode);
	if (err < 0) {
		error(_("audio open error: %s"), snd_strerror(err));
		return 1;
	}

	if ((err = snd_pcm_info(handle, info)) < 0) {
		error(_("info error: %s"), snd_strerror(err));
		return 1;
	}

	if (nonblock) {
		err = snd_pcm_nonblock(handle, 1);
		if (err < 0) {
			error(_("nonblock setting error: %s"), snd_strerror(err));
			return 1;
		}
	}

	chunk_size = 1024;
	hwparams = rhwparams;

	audiobuf = (u_char *)malloc(1024);
	if (audiobuf == NULL) {
		error(_("not enough memory"));
		return 1;
	}

	if (mmap_flag) {
		writei_func = snd_pcm_mmap_writei;
		readi_func = snd_pcm_mmap_readi;
		writen_func = snd_pcm_mmap_writen;
		readn_func = snd_pcm_mmap_readn;
	} else {
		writei_func = snd_pcm_writei;
		readi_func = snd_pcm_readi;
		writen_func = snd_pcm_writen;
		readn_func = snd_pcm_readn;
	}

	if (pidfile_name) {
		errno = 0;
		pidf = fopen (pidfile_name, "w");
		if (pidf) {
			(void)fprintf (pidf, "%d\n", getpid());
			fclose(pidf);
			pidfile_written = 1;
		} else {
			error(_("Cannot create process ID file %s: %s"), 
				pidfile_name, strerror (errno));
			return 1;
		}
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGUSR1, signal_handler_recycle);
	if (interleaved) {
		if (optind > argc - 1) {
			if (stream == SND_PCM_STREAM_PLAYBACK)
				playback(NULL);
			else
				capture(NULL);
		} else {
			while (optind <= argc - 1) {
				if (stream == SND_PCM_STREAM_PLAYBACK)
					playback(argv[optind++]);
				else
					capture(argv[optind++]);
			}
		}
	} else {
		if (stream == SND_PCM_STREAM_PLAYBACK)
			playbackv(&argv[optind], argc - optind);
		else
			capturev(&argv[optind], argc - optind);
	}
	if (verbose==2)
		putchar('\n');
	snd_pcm_close(handle);
	handle = NULL;
	free(audiobuf);
      __end:
	snd_output_close(log);
	snd_config_update_free_global();
	prg_exit(EXIT_SUCCESS);
	/* avoid warning */
	return EXIT_SUCCESS;
}

/*
 * Safe read (for pipes)
 */
 
static ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t result = 0, res;

	while (count > 0 && !in_aborting) {
		if ((res = read(fd, buf, count)) == 0)
			break;
		if (res < 0)
			return result > 0 ? result : res;
		count -= res;
		result += res;
		buf = (char *)buf + res;
	}
	return result;
}

/*
 * Test, if it is a .VOC file and return >=0 if ok (this is the length of rest)
 *                                       < 0 if not 
 */
static int test_vocfile(void *buffer)
{
	VocHeader *vp = buffer;

	if (!memcmp(vp->magic, VOC_MAGIC_STRING, 20)) {
		vocminor = LE_SHORT(vp->version) & 0xFF;
		vocmajor = LE_SHORT(vp->version) / 256;
		if (LE_SHORT(vp->version) != (0x1233 - LE_SHORT(vp->coded_ver)))
			return -2;	/* coded version mismatch */
		return LE_SHORT(vp->headerlen) - sizeof(VocHeader);	/* 0 mostly */
	}
	return -1;		/* magic string fail */
}

/*
 * helper for test_wavefile
 */

static size_t test_wavefile_read(int fd, u_char *buffer, size_t *size, size_t reqsize, int line)
{
	if (*size >= reqsize)
		return *size;
	if ((size_t)safe_read(fd, buffer + *size, reqsize - *size) != reqsize - *size) {
		error(_("read error (called from line %i)"), line);
		prg_exit(EXIT_FAILURE);
	}
	return *size = reqsize;
}

#define check_wavefile_space(buffer, len, blimit) \
	if (len > blimit) { \
		blimit = len; \
		if ((buffer = realloc(buffer, blimit)) == NULL) { \
			error(_("not enough memory"));		  \
			prg_exit(EXIT_FAILURE);  \
		} \
	}

/*
 * test, if it's a .WAV file, > 0 if ok (and set the speed, stereo etc.)
 *                            == 0 if not
 * Value returned is bytes to be discarded.
 */
static ssize_t test_wavefile(int fd, u_char *_buffer, size_t size)
{
	WaveHeader *h = (WaveHeader *)_buffer;
	u_char *buffer = NULL;
	size_t blimit = 0;
	WaveFmtBody *f;
	WaveChunkHeader *c;
	u_int type, len;
	unsigned short format, channels;
	int big_endian, native_format;

	if (size < sizeof(WaveHeader))
		return -1;
	if (h->magic == WAV_RIFF)
		big_endian = 0;
	else if (h->magic == WAV_RIFX)
		big_endian = 1;
	else
		return -1;
	if (h->type != WAV_WAVE)
		return -1;

	if (size > sizeof(WaveHeader)) {
		check_wavefile_space(buffer, size - sizeof(WaveHeader), blimit);
		memcpy(buffer, _buffer + sizeof(WaveHeader), size - sizeof(WaveHeader));
	}
	size -= sizeof(WaveHeader);
	while (1) {
		check_wavefile_space(buffer, sizeof(WaveChunkHeader), blimit);
		test_wavefile_read(fd, buffer, &size, sizeof(WaveChunkHeader), __LINE__);
		c = (WaveChunkHeader*)buffer;
		type = c->type;
		len = TO_CPU_INT(c->length, big_endian);
		len += len % 2;
		if (size > sizeof(WaveChunkHeader))
			memmove(buffer, buffer + sizeof(WaveChunkHeader), size - sizeof(WaveChunkHeader));
		size -= sizeof(WaveChunkHeader);
		if (type == WAV_FMT)
			break;
		check_wavefile_space(buffer, len, blimit);
		test_wavefile_read(fd, buffer, &size, len, __LINE__);
		if (size > len)
			memmove(buffer, buffer + len, size - len);
		size -= len;
	}

	if (len < sizeof(WaveFmtBody)) {
		error(_("unknown length of 'fmt ' chunk (read %u, should be %u at least)"),
		      len, (u_int)sizeof(WaveFmtBody));
		prg_exit(EXIT_FAILURE);
	}
	check_wavefile_space(buffer, len, blimit);
	test_wavefile_read(fd, buffer, &size, len, __LINE__);
	f = (WaveFmtBody*) buffer;
	format = TO_CPU_SHORT(f->format, big_endian);
	if (format == WAV_FMT_EXTENSIBLE) {
		WaveFmtExtensibleBody *fe = (WaveFmtExtensibleBody*)buffer;
		if (len < sizeof(WaveFmtExtensibleBody)) {
			error(_("unknown length of extensible 'fmt ' chunk (read %u, should be %u at least)"),
					len, (u_int)sizeof(WaveFmtExtensibleBody));
			prg_exit(EXIT_FAILURE);
		}
		if (memcmp(fe->guid_tag, WAV_GUID_TAG, 14) != 0) {
			error(_("wrong format tag in extensible 'fmt ' chunk"));
			prg_exit(EXIT_FAILURE);
		}
		format = TO_CPU_SHORT(fe->guid_format, big_endian);
	}
	if (format != WAV_FMT_PCM &&
	    format != WAV_FMT_IEEE_FLOAT) {
                error(_("can't play WAVE-file format 0x%04x which is not PCM or FLOAT encoded"), format);
		prg_exit(EXIT_FAILURE);
	}
	channels = TO_CPU_SHORT(f->channels, big_endian);
	if (channels < 1) {
		error(_("can't play WAVE-files with %d tracks"), channels);
		prg_exit(EXIT_FAILURE);
	}
	hwparams.channels = channels;
	switch (TO_CPU_SHORT(f->bit_p_spl, big_endian)) {
	case 8:
		if (hwparams.format != DEFAULT_FORMAT &&
		    hwparams.format != SND_PCM_FORMAT_U8)
			fprintf(stderr, _("Warning: format is changed to U8\n"));
		hwparams.format = SND_PCM_FORMAT_U8;
		break;
	case 16:
		if (big_endian)
			native_format = SND_PCM_FORMAT_S16_BE;
		else
			native_format = SND_PCM_FORMAT_S16_LE;
		if (hwparams.format != DEFAULT_FORMAT &&
		    hwparams.format != native_format)
			fprintf(stderr, _("Warning: format is changed to %s\n"),
				snd_pcm_format_name(native_format));
		hwparams.format = native_format;
		break;
	case 24:
		switch (TO_CPU_SHORT(f->byte_p_spl, big_endian) / hwparams.channels) {
		case 3:
			if (big_endian)
				native_format = SND_PCM_FORMAT_S24_3BE;
			else
				native_format = SND_PCM_FORMAT_S24_3LE;
			if (hwparams.format != DEFAULT_FORMAT &&
			    hwparams.format != native_format)
				fprintf(stderr, _("Warning: format is changed to %s\n"),
					snd_pcm_format_name(native_format));
			hwparams.format = native_format;
			break;
		case 4:
			if (big_endian)
				native_format = SND_PCM_FORMAT_S24_BE;
			else
				native_format = SND_PCM_FORMAT_S24_LE;
			if (hwparams.format != DEFAULT_FORMAT &&
			    hwparams.format != native_format)
				fprintf(stderr, _("Warning: format is changed to %s\n"),
					snd_pcm_format_name(native_format));
			hwparams.format = native_format;
			break;
		default:
			error(_(" can't play WAVE-files with sample %d bits in %d bytes wide (%d channels)"),
			      TO_CPU_SHORT(f->bit_p_spl, big_endian),
			      TO_CPU_SHORT(f->byte_p_spl, big_endian),
			      hwparams.channels);
			prg_exit(EXIT_FAILURE);
		}
		break;
	case 32:
		if (format == WAV_FMT_PCM) {
			if (big_endian)
				native_format = SND_PCM_FORMAT_S32_BE;
			else
				native_format = SND_PCM_FORMAT_S32_LE;
                        hwparams.format = native_format;
		} else if (format == WAV_FMT_IEEE_FLOAT) {
			if (big_endian)
				native_format = SND_PCM_FORMAT_FLOAT_BE;
			else
				native_format = SND_PCM_FORMAT_FLOAT_LE;
			hwparams.format = native_format;
		}
		break;
	default:
		error(_(" can't play WAVE-files with sample %d bits wide"),
		      TO_CPU_SHORT(f->bit_p_spl, big_endian));
		prg_exit(EXIT_FAILURE);
	}
	hwparams.rate = TO_CPU_INT(f->sample_fq, big_endian);
	
	if (size > len)
		memmove(buffer, buffer + len, size - len);
	size -= len;
	
	while (1) {
		u_int type, len;

		check_wavefile_space(buffer, sizeof(WaveChunkHeader), blimit);
		test_wavefile_read(fd, buffer, &size, sizeof(WaveChunkHeader), __LINE__);
		c = (WaveChunkHeader*)buffer;
		type = c->type;
		len = TO_CPU_INT(c->length, big_endian);
		if (size > sizeof(WaveChunkHeader))
			memmove(buffer, buffer + sizeof(WaveChunkHeader), size - sizeof(WaveChunkHeader));
		size -= sizeof(WaveChunkHeader);
		if (type == WAV_DATA) {
			if (len < pbrec_count && len < 0x7ffffffe)
				pbrec_count = len;
			if (size > 0)
				memcpy(_buffer, buffer, size);
			free(buffer);
			return size;
		}
		len += len % 2;
		check_wavefile_space(buffer, len, blimit);
		test_wavefile_read(fd, buffer, &size, len, __LINE__);
		if (size > len)
			memmove(buffer, buffer + len, size - len);
		size -= len;
	}

	/* shouldn't be reached */
	return -1;
}

/*

 */

static int test_au(int fd, void *buffer)
{
	AuHeader *ap = buffer;

	if (ap->magic != AU_MAGIC)
		return -1;
	if (BE_INT(ap->hdr_size) > 128 || BE_INT(ap->hdr_size) < 24)
		return -1;
	pbrec_count = BE_INT(ap->data_size);
	switch (BE_INT(ap->encoding)) {
	case AU_FMT_ULAW:
		if (hwparams.format != DEFAULT_FORMAT &&
		    hwparams.format != SND_PCM_FORMAT_MU_LAW)
			fprintf(stderr, _("Warning: format is changed to MU_LAW\n"));
		hwparams.format = SND_PCM_FORMAT_MU_LAW;
		break;
	case AU_FMT_LIN8:
		if (hwparams.format != DEFAULT_FORMAT &&
		    hwparams.format != SND_PCM_FORMAT_U8)
			fprintf(stderr, _("Warning: format is changed to U8\n"));
		hwparams.format = SND_PCM_FORMAT_U8;
		break;
	case AU_FMT_LIN16:
		if (hwparams.format != DEFAULT_FORMAT &&
		    hwparams.format != SND_PCM_FORMAT_S16_BE)
			fprintf(stderr, _("Warning: format is changed to S16_BE\n"));
		hwparams.format = SND_PCM_FORMAT_S16_BE;
		break;
	default:
		return -1;
	}
	hwparams.rate = BE_INT(ap->sample_rate);
	if (hwparams.rate < 2000 || hwparams.rate > 256000)
		return -1;
	hwparams.channels = BE_INT(ap->channels);
	if (hwparams.channels < 1 || hwparams.channels > 256)
		return -1;
	if ((size_t)safe_read(fd, buffer + sizeof(AuHeader), BE_INT(ap->hdr_size) - sizeof(AuHeader)) != BE_INT(ap->hdr_size) - sizeof(AuHeader)) {
		error(_("read error"));
		prg_exit(EXIT_FAILURE);
	}
	return 0;
}

static void show_available_sample_formats(snd_pcm_hw_params_t* params)
{
	snd_pcm_format_t format;

	fprintf(stderr, "Available formats:\n");
	for (format = 0; format <= SND_PCM_FORMAT_LAST; format++) {
		if (snd_pcm_hw_params_test_format(handle, params, format) == 0)
			fprintf(stderr, "- %s\n", snd_pcm_format_name(format));
	}
}

#ifdef CONFIG_SUPPORT_CHMAP
static int setup_chmap(void)
{
	snd_pcm_chmap_t *chmap = channel_map;
	char mapped[hwparams.channels];
	snd_pcm_chmap_t *hw_chmap;
	unsigned int ch, i;
	int err;

	if (!chmap)
		return 0;

	if (chmap->channels != hwparams.channels) {
		error(_("Channel numbers don't match between hw_params and channel map"));
		return -1;
	}
	err = snd_pcm_set_chmap(handle, chmap);
	if (!err)
		return 0;

	hw_chmap = snd_pcm_get_chmap(handle);
	if (!hw_chmap) {
		fprintf(stderr, _("Warning: unable to get channel map\n"));
		return 0;
	}

	if (hw_chmap->channels == chmap->channels &&
	    !memcmp(hw_chmap, chmap, 4 * (chmap->channels + 1))) {
		/* maps are identical, so no need to convert */
		free(hw_chmap);
		return 0;
	}

	hw_map = calloc(hwparams.channels, sizeof(int));
	if (!hw_map) {
		error(_("not enough memory"));
		return -1;
	}

	memset(mapped, 0, sizeof(mapped));
	for (ch = 0; ch < hw_chmap->channels; ch++) {
		if (chmap->pos[ch] == hw_chmap->pos[ch]) {
			mapped[ch] = 1;
			hw_map[ch] = ch;
			continue;
		}
		for (i = 0; i < hw_chmap->channels; i++) {
			if (!mapped[i] && chmap->pos[ch] == hw_chmap->pos[i]) {
				mapped[i] = 1;
				hw_map[ch] = i;
				break;
			}
		}
		if (i >= hw_chmap->channels) {
			char buf[256];
			error(_("Channel %d doesn't match with hw_parmas"), ch);
			snd_pcm_chmap_print(hw_chmap, sizeof(buf), buf);
			fprintf(stderr, "hardware chmap = %s\n", buf);
			return -1;
		}
	}
	free(hw_chmap);
	return 0;
}
#else
#define setup_chmap()	0
#endif

static void set_params(void)
{
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_uframes_t buffer_size;
	int err;
	size_t n;
	unsigned int rate;
	snd_pcm_uframes_t start_threshold, stop_threshold;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_sw_params_alloca(&swparams);
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		error(_("Broken configuration for this PCM: no configurations available"));
		prg_exit(EXIT_FAILURE);
	}
	if (dump_hw_params) {
		fprintf(stderr, _("HW Params of device \"%s\":\n"),
			snd_pcm_name(handle));
		fprintf(stderr, "--------------------\n");
		snd_pcm_hw_params_dump(params, log);
		fprintf(stderr, "--------------------\n");
	}
	if (mmap_flag) {
		snd_pcm_access_mask_t *mask = alloca(snd_pcm_access_mask_sizeof());
		snd_pcm_access_mask_none(mask);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
		snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
		err = snd_pcm_hw_params_set_access_mask(handle, params, mask);
	} else if (interleaved)
		err = snd_pcm_hw_params_set_access(handle, params,
						   SND_PCM_ACCESS_RW_INTERLEAVED);
	else
		err = snd_pcm_hw_params_set_access(handle, params,
						   SND_PCM_ACCESS_RW_NONINTERLEAVED);
	if (err < 0) {
		error(_("Access type not available"));
		prg_exit(EXIT_FAILURE);
	}
	err = snd_pcm_hw_params_set_format(handle, params, hwparams.format);
	if (err < 0) {
		error(_("Sample format non available"));
		show_available_sample_formats(params);
		prg_exit(EXIT_FAILURE);
	}
	err = snd_pcm_hw_params_set_channels(handle, params, hwparams.channels);
	if (err < 0) {
		error(_("Channels count non available"));
		prg_exit(EXIT_FAILURE);
	}

#if 0
	err = snd_pcm_hw_params_set_periods_min(handle, params, 2);
	assert(err >= 0);
#endif
	rate = hwparams.rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &hwparams.rate, 0);
	assert(err >= 0);
	if ((float)rate * 1.05 < hwparams.rate || (float)rate * 0.95 > hwparams.rate) {
		if (!quiet_mode) {
			char plugex[64];
			const char *pcmname = snd_pcm_name(handle);
			fprintf(stderr, _("Warning: rate is not accurate (requested = %iHz, got = %iHz)\n"), rate, hwparams.rate);
			if (! pcmname || strchr(snd_pcm_name(handle), ':'))
				*plugex = 0;
			else
				snprintf(plugex, sizeof(plugex), "(-Dplug:%s)",
					 snd_pcm_name(handle));
			fprintf(stderr, _("         please, try the plug plugin %s\n"),
				plugex);
		}
	}
	rate = hwparams.rate;
	if (buffer_time == 0 && buffer_frames == 0) {
		err = snd_pcm_hw_params_get_buffer_time_max(params,
							    &buffer_time, 0);
		assert(err >= 0);
		if (buffer_time > 500000)
			buffer_time = 500000;
	}
	if (period_time == 0 && period_frames == 0) {
		if (buffer_time > 0)
			period_time = buffer_time / 4;
		else
			period_frames = buffer_frames / 4;
	}
	if (period_time > 0)
		err = snd_pcm_hw_params_set_period_time_near(handle, params,
							     &period_time, 0);
	else
		err = snd_pcm_hw_params_set_period_size_near(handle, params,
							     &period_frames, 0);
	assert(err >= 0);
	if (buffer_time > 0) {
		err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
							     &buffer_time, 0);
	} else {
		err = snd_pcm_hw_params_set_buffer_size_near(handle, params,
							     &buffer_frames);
	}
	assert(err >= 0);
	monotonic = snd_pcm_hw_params_is_monotonic(params);
	can_pause = snd_pcm_hw_params_can_pause(params);
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		error(_("Unable to install hw params:"));
		snd_pcm_hw_params_dump(params, log);
		prg_exit(EXIT_FAILURE);
	}
	snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	if (chunk_size == buffer_size) {
		error(_("Can't use period equal to buffer size (%lu == %lu)"),
		      chunk_size, buffer_size);
		prg_exit(EXIT_FAILURE);
	}
	snd_pcm_sw_params_current(handle, swparams);
	if (avail_min < 0)
		n = chunk_size;
	else
		n = (double) rate * avail_min / 1000000;
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);

	/* round up to closest transfer boundary */
	n = buffer_size;
	if (start_delay <= 0) {
		start_threshold = n + (double) rate * start_delay / 1000000;
	} else
		start_threshold = (double) rate * start_delay / 1000000;
	if (start_threshold < 1)
		start_threshold = 1;
	if (start_threshold > n)
		start_threshold = n;
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
	assert(err >= 0);
	if (stop_delay <= 0) 
		stop_threshold = buffer_size + (double) rate * stop_delay / 1000000;
	else
		stop_threshold = (double) rate * stop_delay / 1000000;
	err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);
	assert(err >= 0);

	if (snd_pcm_sw_params(handle, swparams) < 0) {
		error(_("unable to install sw params:"));
		snd_pcm_sw_params_dump(swparams, log);
		prg_exit(EXIT_FAILURE);
	}

	if (setup_chmap())
		prg_exit(EXIT_FAILURE);

	if (verbose)
		snd_pcm_dump(handle, log);

	bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
	significant_bits_per_sample = snd_pcm_format_width(hwparams.format);
	bits_per_frame = bits_per_sample * hwparams.channels;
	chunk_bytes = chunk_size * bits_per_frame / 8;
	audiobuf = realloc(audiobuf, chunk_bytes);
	if (audiobuf == NULL) {
		error(_("not enough memory"));
		prg_exit(EXIT_FAILURE);
	}
	// fprintf(stderr, "real chunk_size = %i, frags = %i, total = %i\n", chunk_size, setup.buf.block.frags, setup.buf.block.frags * chunk_size);

	/* stereo VU-meter isn't always available... */
	if (vumeter == VUMETER_STEREO) {
		if (hwparams.channels != 2 || !interleaved || verbose > 2)
			vumeter = VUMETER_MONO;
	}

	/* show mmap buffer arragment */
	if (mmap_flag && verbose) {
		const snd_pcm_channel_area_t *areas;
		snd_pcm_uframes_t offset, size = chunk_size;
		int i;
		err = snd_pcm_mmap_begin(handle, &areas, &offset, &size);
		if (err < 0) {
			error(_("snd_pcm_mmap_begin problem: %s"), snd_strerror(err));
			prg_exit(EXIT_FAILURE);
		}
		for (i = 0; i < hwparams.channels; i++)
			fprintf(stderr, "mmap_area[%i] = %p,%u,%u (%u)\n", i, areas[i].addr, areas[i].first, areas[i].step, snd_pcm_format_physical_width(hwparams.format));
		/* not required, but for sure */
		snd_pcm_mmap_commit(handle, offset, 0);
	}

	buffer_frames = buffer_size;	/* for position test */
}

static void init_stdin(void)
{
	struct termios term;
	long flags;

	if (!interactive)
		return;
	if (!isatty(fileno(stdin))) {
		interactive = 0;
		return;
	}
	tcgetattr(fileno(stdin), &term);
	term_c_lflag = term.c_lflag;
	if (fd == fileno(stdin))
		return;
	flags = fcntl(fileno(stdin), F_GETFL);
	if (flags < 0 || fcntl(fileno(stdin), F_SETFL, flags|O_NONBLOCK) < 0)
		fprintf(stderr, _("stdin O_NONBLOCK flag setup failed\n"));
	term.c_lflag &= ~ICANON;
	tcsetattr(fileno(stdin), TCSANOW, &term);
}

static void done_stdin(void)
{
	struct termios term;

	if (!interactive)
		return;
	if (fd == fileno(stdin) || term_c_lflag == -1)
		return;
	tcgetattr(fileno(stdin), &term);
	term.c_lflag = term_c_lflag;
	tcsetattr(fileno(stdin), TCSANOW, &term);
}

static void do_pause(void)
{
	int err;
	unsigned char b;

	if (!can_pause) {
		fprintf(stderr, _("\rPAUSE command ignored (no hw support)\n"));
		return;
	}
	err = snd_pcm_pause(handle, 1);
	if (err < 0) {
		error(_("pause push error: %s"), snd_strerror(err));
		return;
	}
	while (1) {
		while (read(fileno(stdin), &b, 1) != 1);
		if (b == ' ' || b == '\r') {
			while (read(fileno(stdin), &b, 1) == 1);
			err = snd_pcm_pause(handle, 0);
			if (err < 0)
				error(_("pause release error: %s"), snd_strerror(err));
			return;
		}
	}
}

static void check_stdin(void)
{
	unsigned char b;

	if (!interactive)
		return;
	if (fd != fileno(stdin)) {
		while (read(fileno(stdin), &b, 1) == 1) {
			if (b == ' ' || b == '\r') {
				while (read(fileno(stdin), &b, 1) == 1);
				fprintf(stderr, _("\r=== PAUSE ===                                                            "));
				fflush(stderr);
			do_pause();
				fprintf(stderr, "                                                                          \r");
				fflush(stderr);
			}
		}
	}
}

#ifndef timersub
#define	timersub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while (0)
#endif

#ifndef timermsub
#define	timermsub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
	if ((result)->tv_nsec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_nsec += 1000000000L; \
	} \
} while (0)
#endif

/* I/O error handler */
static void xrun(void)
{
	snd_pcm_status_t *status;
	int res;
	
	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(handle, status))<0) {
		error(_("status error: %s"), snd_strerror(res));
		prg_exit(EXIT_FAILURE);
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		if (fatal_errors) {
			error(_("fatal %s: %s"),
					stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
					snd_strerror(res));
			prg_exit(EXIT_FAILURE);
		}
		if (monotonic) {
#ifdef HAVE_CLOCK_GETTIME
			struct timespec now, diff, tstamp;
			clock_gettime(CLOCK_MONOTONIC, &now);
			snd_pcm_status_get_trigger_htstamp(status, &tstamp);
			timermsub(&now, &tstamp, &diff);
			fprintf(stderr, _("%s!!! (at least %.3f ms long)\n"),
				stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
				diff.tv_sec * 1000 + diff.tv_nsec / 1000000.0);
#else
			fprintf(stderr, "%s !!!\n", _("underrun"));
#endif
		} else {
			struct timeval now, diff, tstamp;
			gettimeofday(&now, 0);
			snd_pcm_status_get_trigger_tstamp(status, &tstamp);
			timersub(&now, &tstamp, &diff);
			fprintf(stderr, _("%s!!! (at least %.3f ms long)\n"),
				stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
				diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
		}
		if (verbose) {
			fprintf(stderr, _("Status:\n"));
			snd_pcm_status_dump(status, log);
		}
		if ((res = snd_pcm_prepare(handle))<0) {
			error(_("xrun: prepare error: %s"), snd_strerror(res));
			prg_exit(EXIT_FAILURE);
		}
		return;		/* ok, data should be accepted again */
	} if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
		if (verbose) {
			fprintf(stderr, _("Status(DRAINING):\n"));
			snd_pcm_status_dump(status, log);
		}
		if (stream == SND_PCM_STREAM_CAPTURE) {
			fprintf(stderr, _("capture stream format change? attempting recover...\n"));
			if ((res = snd_pcm_prepare(handle))<0) {
				error(_("xrun(DRAINING): prepare error: %s"), snd_strerror(res));
				prg_exit(EXIT_FAILURE);
			}
			return;
		}
	}
	if (verbose) {
		fprintf(stderr, _("Status(R/W):\n"));
		snd_pcm_status_dump(status, log);
	}
	error(_("read/write error, state = %s"), snd_pcm_state_name(snd_pcm_status_get_state(status)));
	prg_exit(EXIT_FAILURE);
}

/* I/O suspend handler */
static void suspend(void)
{
	int res;

	if (!quiet_mode)
		fprintf(stderr, _("Suspended. Trying resume. ")); fflush(stderr);
	while ((res = snd_pcm_resume(handle)) == -EAGAIN)
		sleep(1);	/* wait until suspend flag is released */
	if (res < 0) {
		if (!quiet_mode)
			fprintf(stderr, _("Failed. Restarting stream. ")); fflush(stderr);
		if ((res = snd_pcm_prepare(handle)) < 0) {
			error(_("suspend: prepare error: %s"), snd_strerror(res));
			prg_exit(EXIT_FAILURE);
		}
	}
	if (!quiet_mode)
		fprintf(stderr, _("Done.\n"));
}

static void print_vu_meter_mono(int perc, int maxperc)
{
	const int bar_length = 50;
	char line[80];
	int val;

	for (val = 0; val <= perc * bar_length / 100 && val < bar_length; val++)
		line[val] = '#';
	for (; val <= maxperc * bar_length / 100 && val < bar_length; val++)
		line[val] = ' ';
	line[val] = '+';
	for (++val; val <= bar_length; val++)
		line[val] = ' ';
	if (maxperc > 99)
		sprintf(line + val, "| MAX");
	else
		sprintf(line + val, "| %02i%%", maxperc);
	fputs(line, stderr);
	if (perc > 100)
		fprintf(stderr, _(" !clip  "));
}

static void print_vu_meter_stereo(int *perc, int *maxperc)
{
	const int bar_length = 35;
	char line[80];
	int c;

	memset(line, ' ', sizeof(line) - 1);
	line[bar_length + 3] = '|';

	for (c = 0; c < 2; c++) {
		int p = perc[c] * bar_length / 100;
		char tmp[4];
		if (p > bar_length)
			p = bar_length;
		if (c)
			memset(line + bar_length + 6 + 1, '#', p);
		else
			memset(line + bar_length - p - 1, '#', p);
		p = maxperc[c] * bar_length / 100;
		if (p > bar_length)
			p = bar_length;
		if (c)
			line[bar_length + 6 + 1 + p] = '+';
		else
			line[bar_length - p - 1] = '+';
		if (maxperc[c] > 99)
			sprintf(tmp, "MAX");
		else
			sprintf(tmp, "%02d%%", maxperc[c]);
		if (c)
			memcpy(line + bar_length + 3 + 1, tmp, 3);
		else
			memcpy(line + bar_length, tmp, 3);
	}
	line[bar_length * 2 + 6 + 2] = 0;
	fputs(line, stderr);
}

static void print_vu_meter(signed int *perc, signed int *maxperc)
{
	if (vumeter == VUMETER_STEREO)
		print_vu_meter_stereo(perc, maxperc);
	else
		print_vu_meter_mono(*perc, *maxperc);
}

/* peak handler */
static void compute_max_peak(u_char *data, size_t count)
{
	signed int val, max, perc[2], max_peak[2];
	static	int	run = 0;
	size_t ocount = count;
	int	format_little_endian = snd_pcm_format_little_endian(hwparams.format);	
	int ichans, c;

	if (vumeter == VUMETER_STEREO)
		ichans = 2;
	else
		ichans = 1;

	memset(max_peak, 0, sizeof(max_peak));
	switch (bits_per_sample) {
	case 8: {
		signed char *valp = (signed char *)data;
		signed char mask = snd_pcm_format_silence(hwparams.format);
		c = 0;
		while (count-- > 0) {
			val = *valp++ ^ mask;
			val = abs(val);
			if (max_peak[c] < val)
				max_peak[c] = val;
			if (vumeter == VUMETER_STEREO)
				c = !c;
		}
		break;
	}
	case 16: {
		signed short *valp = (signed short *)data;
		signed short mask = snd_pcm_format_silence_16(hwparams.format);
		signed short sval;

		count /= 2;
		c = 0;
		while (count-- > 0) {
			if (format_little_endian)
				sval = le16toh(*valp);
			else
				sval = be16toh(*valp);
			sval = abs(sval) ^ mask;
			if (max_peak[c] < sval)
				max_peak[c] = sval;
			valp++;
			if (vumeter == VUMETER_STEREO)
				c = !c;
		}
		break;
	}
	case 24: {
		unsigned char *valp = data;
		signed int mask = snd_pcm_format_silence_32(hwparams.format);

		count /= 3;
		c = 0;
		while (count-- > 0) {
			if (format_little_endian) {
				val = valp[0] | (valp[1]<<8) | (valp[2]<<16);
			} else {
				val = (valp[0]<<16) | (valp[1]<<8) | valp[2];
			}
			/* Correct signed bit in 32-bit value */
			if (val & (1<<(bits_per_sample-1))) {
				val |= 0xff<<24;	/* Negate upper bits too */
			}
			val = abs(val) ^ mask;
			if (max_peak[c] < val)
				max_peak[c] = val;
			valp += 3;
			if (vumeter == VUMETER_STEREO)
				c = !c;
		}
		break;
	}
	case 32: {
		signed int *valp = (signed int *)data;
		signed int mask = snd_pcm_format_silence_32(hwparams.format);

		count /= 4;
		c = 0;
		while (count-- > 0) {
			if (format_little_endian)
				val = le32toh(*valp);
			else
				val = be32toh(*valp);
			val = abs(val) ^ mask;
			if (max_peak[c] < val)
				max_peak[c] = val;
			valp++;
			if (vumeter == VUMETER_STEREO)
				c = !c;
		}
		break;
	}
	default:
		if (run == 0) {
			fprintf(stderr, _("Unsupported bit size %d.\n"), (int)bits_per_sample);
			run = 1;
		}
		return;
	}
	max = 1 << (significant_bits_per_sample-1);
	if (max <= 0)
		max = 0x7fffffff;

	for (c = 0; c < ichans; c++) {
		if (bits_per_sample > 16)
			perc[c] = max_peak[c] / (max / 100);
		else
			perc[c] = max_peak[c] * 100 / max;
	}

	if (interleaved && verbose <= 2) {
		static int maxperc[2];
		static time_t t=0;
		const time_t tt=time(NULL);
		if(tt>t) {
			t=tt;
			maxperc[0] = 0;
			maxperc[1] = 0;
		}
		for (c = 0; c < ichans; c++)
			if (perc[c] > maxperc[c])
				maxperc[c] = perc[c];

		putc('\r', stderr);
		print_vu_meter(perc, maxperc);
		fflush(stderr);
	}
	else if(verbose==3) {
		fprintf(stderr, _("Max peak (%li samples): 0x%08x "), (long)ocount, max_peak[0]);
		for (val = 0; val < 20; val++)
			if (val <= perc[0] / 5)
				putc('#', stderr);
			else
				putc(' ', stderr);
		fprintf(stderr, " %i%%\n", perc[0]);
		fflush(stderr);
	}
}

static void do_test_position(void)
{
	static long counter = 0;
	static time_t tmr = -1;
	time_t now;
	static float availsum, delaysum, samples;
	static snd_pcm_sframes_t maxavail, maxdelay;
	static snd_pcm_sframes_t minavail, mindelay;
	static snd_pcm_sframes_t badavail = 0, baddelay = 0;
	snd_pcm_sframes_t outofrange;
	snd_pcm_sframes_t avail, delay;
	int err;

	err = snd_pcm_avail_delay(handle, &avail, &delay);
	if (err < 0)
		return;
	outofrange = (test_coef * (snd_pcm_sframes_t)buffer_frames) / 2;
	if (avail > outofrange || avail < -outofrange ||
	    delay > outofrange || delay < -outofrange) {
	  badavail = avail; baddelay = delay;
	  availsum = delaysum = samples = 0;
	  maxavail = maxdelay = 0;
	  minavail = mindelay = buffer_frames * 16;
	  fprintf(stderr, _("Suspicious buffer position (%li total): "
	  	"avail = %li, delay = %li, buffer = %li\n"),
	  	++counter, (long)avail, (long)delay, (long)buffer_frames);
	} else if (verbose) {
		time(&now);
		if (tmr == (time_t) -1) {
			tmr = now;
			availsum = delaysum = samples = 0;
			maxavail = maxdelay = 0;
			minavail = mindelay = buffer_frames * 16;
		}
		if (avail > maxavail)
			maxavail = avail;
		if (delay > maxdelay)
			maxdelay = delay;
		if (avail < minavail)
			minavail = avail;
		if (delay < mindelay)
			mindelay = delay;
		availsum += avail;
		delaysum += delay;
		samples++;
		if (avail != 0 && now != tmr) {
			fprintf(stderr, "BUFPOS: avg%li/%li "
				"min%li/%li max%li/%li (%li) (%li:%li/%li)\n",
				(long)(availsum / samples),
				(long)(delaysum / samples),
				(long)minavail, (long)mindelay,
				(long)maxavail, (long)maxdelay,
				(long)buffer_frames,
				counter, badavail, baddelay);
			tmr = now;
		}
	}
}

/*
 */
#ifdef CONFIG_SUPPORT_CHMAP
static u_char *remap_data(u_char *data, size_t count)
{
	static u_char *tmp, *src, *dst;
	static size_t tmp_size;
	size_t sample_bytes = bits_per_sample / 8;
	size_t step = bits_per_frame / 8;
	size_t chunk_bytes;
	unsigned int ch, i;

	if (!hw_map)
		return data;

	chunk_bytes = count * bits_per_frame / 8;
	if (tmp_size < chunk_bytes) {
		free(tmp);
		tmp = malloc(chunk_bytes);
		if (!tmp) {
			error(_("not enough memory"));
			exit(1);
		}
		tmp_size = count;
	}

	src = data;
	dst = tmp;
	for (i = 0; i < count; i++) {
		for (ch = 0; ch < hwparams.channels; ch++) {
			memcpy(dst, src + sample_bytes * hw_map[ch],
			       sample_bytes);
			dst += sample_bytes;
		}
		src += step;
	}
	return tmp;
}

static u_char **remap_datav(u_char **data, size_t count)
{
	static u_char **tmp;
	unsigned int ch;

	if (!hw_map)
		return data;

	if (!tmp) {
		tmp = malloc(sizeof(*tmp) * hwparams.channels);
		if (!tmp) {
			error(_("not enough memory"));
			exit(1);
		}
		for (ch = 0; ch < hwparams.channels; ch++)
			tmp[ch] = data[hw_map[ch]];
	}
	return tmp;
}
#else
#define remap_data(data, count)		(data)
#define remap_datav(data, count)	(data)
#endif

/*
 *  write function
 */

static ssize_t pcm_write(u_char *data, size_t count)
{
	ssize_t r;
	ssize_t result = 0;

	if (count < chunk_size) {
		snd_pcm_format_set_silence(hwparams.format, data + count * bits_per_frame / 8, (chunk_size - count) * hwparams.channels);
		count = chunk_size;
	}
	data = remap_data(data, count);
	while (count > 0 && !in_aborting) {
		if (test_position)
			do_test_position();
		check_stdin();
		r = writei_func(handle, data, count);
		if (test_position)
			do_test_position();
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			if (!test_nowait)
				snd_pcm_wait(handle, 100);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r == -ESTRPIPE) {
			suspend();
		} else if (r < 0) {
			error(_("write error: %s"), snd_strerror(r));
			prg_exit(EXIT_FAILURE);
		}
		if (r > 0) {
			if (vumeter)
				compute_max_peak(data, r * hwparams.channels);
			result += r;
			count -= r;
			data += r * bits_per_frame / 8;
		}
	}
	return result;
}

static ssize_t pcm_writev(u_char **data, unsigned int channels, size_t count)
{
	ssize_t r;
	size_t result = 0;

	if (count != chunk_size) {
		unsigned int channel;
		size_t offset = count;
		size_t remaining = chunk_size - count;
		for (channel = 0; channel < channels; channel++)
			snd_pcm_format_set_silence(hwparams.format, data[channel] + offset * bits_per_sample / 8, remaining);
		count = chunk_size;
	}
	data = remap_datav(data, count);
	while (count > 0 && !in_aborting) {
		unsigned int channel;
		void *bufs[channels];
		size_t offset = result;
		for (channel = 0; channel < channels; channel++)
			bufs[channel] = data[channel] + offset * bits_per_sample / 8;
		if (test_position)
			do_test_position();
		check_stdin();
		r = writen_func(handle, bufs, count);
		if (test_position)
			do_test_position();
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			if (!test_nowait)
				snd_pcm_wait(handle, 100);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r == -ESTRPIPE) {
			suspend();
		} else if (r < 0) {
			error(_("writev error: %s"), snd_strerror(r));
			prg_exit(EXIT_FAILURE);
		}
		if (r > 0) {
			if (vumeter) {
				for (channel = 0; channel < channels; channel++)
					compute_max_peak(data[channel], r);
			}
			result += r;
			count -= r;
		}
	}
	return result;
}

/*
 *  read function
 */

static ssize_t pcm_read(u_char *data, size_t rcount)
{
	ssize_t r;
	size_t result = 0;
	size_t count = rcount;

	if (count != chunk_size) {
		count = chunk_size;
	}

	while (count > 0 && !in_aborting) {
		if (test_position)
			do_test_position();
		check_stdin();
		r = readi_func(handle, data, count);
		if (test_position)
			do_test_position();
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			if (!test_nowait)
				snd_pcm_wait(handle, 100);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r == -ESTRPIPE) {
			suspend();
		} else if (r < 0) {
			error(_("read error: %s"), snd_strerror(r));
			prg_exit(EXIT_FAILURE);
		}
		if (r > 0) {
			if (vumeter)
				compute_max_peak(data, r * hwparams.channels);
			result += r;
			count -= r;
			data += r * bits_per_frame / 8;
		}
	}
	return rcount;
}

static ssize_t pcm_readv(u_char **data, unsigned int channels, size_t rcount)
{
	ssize_t r;
	size_t result = 0;
	size_t count = rcount;

	if (count != chunk_size) {
		count = chunk_size;
	}

	while (count > 0 && !in_aborting) {
		unsigned int channel;
		void *bufs[channels];
		size_t offset = result;
		for (channel = 0; channel < channels; channel++)
			bufs[channel] = data[channel] + offset * bits_per_sample / 8;
		if (test_position)
			do_test_position();
		check_stdin();
		r = readn_func(handle, bufs, count);
		if (test_position)
			do_test_position();
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			if (!test_nowait)
				snd_pcm_wait(handle, 100);
		} else if (r == -EPIPE) {
			xrun();
		} else if (r == -ESTRPIPE) {
			suspend();
		} else if (r < 0) {
			error(_("readv error: %s"), snd_strerror(r));
			prg_exit(EXIT_FAILURE);
		}
		if (r > 0) {
			if (vumeter) {
				for (channel = 0; channel < channels; channel++)
					compute_max_peak(data[channel], r);
			}
			result += r;
			count -= r;
		}
	}
	return rcount;
}

/*
 *  ok, let's play a .voc file
 */

static ssize_t voc_pcm_write(u_char *data, size_t count)
{
	ssize_t result = count, r;
	size_t size;

	while (count > 0) {
		size = count;
		if (size > chunk_bytes - buffer_pos)
			size = chunk_bytes - buffer_pos;
		memcpy(audiobuf + buffer_pos, data, size);
		data += size;
		count -= size;
		buffer_pos += size;
		if ((size_t)buffer_pos == chunk_bytes) {
			if ((size_t)(r = pcm_write(audiobuf, chunk_size)) != chunk_size)
				return r;
			buffer_pos = 0;
		}
	}
	return result;
}

static void voc_write_silence(unsigned x)
{
	unsigned l;
	u_char *buf;

	buf = (u_char *) malloc(chunk_bytes);
	if (buf == NULL) {
		error(_("can't allocate buffer for silence"));
		return;		/* not fatal error */
	}
	snd_pcm_format_set_silence(hwparams.format, buf, chunk_size * hwparams.channels);
	while (x > 0 && !in_aborting) {
		l = x;
		if (l > chunk_size)
			l = chunk_size;
		if (voc_pcm_write(buf, l) != (ssize_t)l) {
			error(_("write error"));
			prg_exit(EXIT_FAILURE);
		}
		x -= l;
	}
	free(buf);
}

static void voc_pcm_flush(void)
{
	if (buffer_pos > 0) {
		size_t b;
		if (snd_pcm_format_set_silence(hwparams.format, audiobuf + buffer_pos, chunk_bytes - buffer_pos * 8 / bits_per_sample) < 0)
			fprintf(stderr, _("voc_pcm_flush - silence error"));
		b = chunk_size;
		if (pcm_write(audiobuf, b) != (ssize_t)b)
			error(_("voc_pcm_flush error"));
	}
	snd_pcm_nonblock(handle, 0);
	snd_pcm_drain(handle);
	snd_pcm_nonblock(handle, nonblock);
}

static void voc_play(int fd, int ofs, char *name)
{
	int l;
	VocBlockType *bp;
	VocVoiceData *vd;
	VocExtBlock *eb;
	size_t nextblock, in_buffer;
	u_char *data, *buf;
	char was_extended = 0, output = 0;
	u_short *sp, repeat = 0;
	off64_t filepos = 0;

#define COUNT(x)	nextblock -= x; in_buffer -= x; data += x
#define COUNT1(x)	in_buffer -= x; data += x

	data = buf = (u_char *)malloc(64 * 1024);
	buffer_pos = 0;
	if (data == NULL) {
		error(_("malloc error"));
		prg_exit(EXIT_FAILURE);
	}
	if (!quiet_mode) {
		fprintf(stderr, _("Playing Creative Labs Channel file '%s'...\n"), name);
	}
	/* first we waste the rest of header, ugly but we don't need seek */
	while (ofs > (ssize_t)chunk_bytes) {
		if ((size_t)safe_read(fd, buf, chunk_bytes) != chunk_bytes) {
			error(_("read error"));
			prg_exit(EXIT_FAILURE);
		}
		ofs -= chunk_bytes;
	}
	if (ofs) {
		if (safe_read(fd, buf, ofs) != ofs) {
			error(_("read error"));
			prg_exit(EXIT_FAILURE);
		}
	}
	hwparams.format = DEFAULT_FORMAT;
	hwparams.channels = 1;
	hwparams.rate = DEFAULT_SPEED;
	set_params();

	in_buffer = nextblock = 0;
	while (!in_aborting) {
	      Fill_the_buffer:	/* need this for repeat */
		if (in_buffer < 32) {
			/* move the rest of buffer to pos 0 and fill the buf up */
			if (in_buffer)
				memcpy(buf, data, in_buffer);
			data = buf;
			if ((l = safe_read(fd, buf + in_buffer, chunk_bytes - in_buffer)) > 0)
				in_buffer += l;
			else if (!in_buffer) {
				/* the file is truncated, so simulate 'Terminator' 
				   and reduce the datablock for safe landing */
				nextblock = buf[0] = 0;
				if (l == -1) {
					perror(name);
					prg_exit(EXIT_FAILURE);
				}
			}
		}
		while (!nextblock) {	/* this is a new block */
			if (in_buffer < sizeof(VocBlockType))
				goto __end;
			bp = (VocBlockType *) data;
			COUNT1(sizeof(VocBlockType));
			nextblock = VOC_DATALEN(bp);
			if (output && !quiet_mode)
				fprintf(stderr, "\n");	/* write /n after ASCII-out */
			output = 0;
			switch (bp->type) {
			case 0:
#if 0
				d_printf("Terminator\n");
#endif
				return;		/* VOC-file stop */
			case 1:
				vd = (VocVoiceData *) data;
				COUNT1(sizeof(VocVoiceData));
				/* we need a SYNC, before we can set new SPEED, STEREO ... */

				if (!was_extended) {
					hwparams.rate = (int) (vd->tc);
					hwparams.rate = 1000000 / (256 - hwparams.rate);
#if 0
					d_printf("Channel data %d Hz\n", dsp_speed);
#endif
					if (vd->pack) {		/* /dev/dsp can't it */
						error(_("can't play packed .voc files"));
						return;
					}
					if (hwparams.channels == 2)		/* if we are in Stereo-Mode, switch back */
						hwparams.channels = 1;
				} else {	/* there was extended block */
					hwparams.channels = 2;
					was_extended = 0;
				}
				set_params();
				break;
			case 2:	/* nothing to do, pure data */
#if 0
				d_printf("Channel continuation\n");
#endif
				break;
			case 3:	/* a silence block, no data, only a count */
				sp = (u_short *) data;
				COUNT1(sizeof(u_short));
				hwparams.rate = (int) (*data);
				COUNT1(1);
				hwparams.rate = 1000000 / (256 - hwparams.rate);
				set_params();
#if 0
				{
					size_t silence;
					silence = (((size_t) * sp) * 1000) / hwparams.rate;
					d_printf("Silence for %d ms\n", (int) silence);
				}
#endif
				voc_write_silence(*sp);
				break;
			case 4:	/* a marker for syncronisation, no effect */
				sp = (u_short *) data;
				COUNT1(sizeof(u_short));
#if 0
				d_printf("Marker %d\n", *sp);
#endif
				break;
			case 5:	/* ASCII text, we copy to stderr */
				output = 1;
#if 0
				d_printf("ASCII - text :\n");
#endif
				break;
			case 6:	/* repeat marker, says repeatcount */
				/* my specs don't say it: maybe this can be recursive, but
				   I don't think somebody use it */
				repeat = *(u_short *) data;
				COUNT1(sizeof(u_short));
#if 0
				d_printf("Repeat loop %d times\n", repeat);
#endif
				if (filepos >= 0) {	/* if < 0, one seek fails, why test another */
					if ((filepos = lseek64(fd, 0, 1)) < 0) {
						error(_("can't play loops; %s isn't seekable\n"), name);
						repeat = 0;
					} else {
						filepos -= in_buffer;	/* set filepos after repeat */
					}
				} else {
					repeat = 0;
				}
				break;
			case 7:	/* ok, lets repeat that be rewinding tape */
				if (repeat) {
					if (repeat != 0xFFFF) {
#if 0
						d_printf("Repeat loop %d\n", repeat);
#endif
						--repeat;
					}
#if 0
					else
						d_printf("Neverending loop\n");
#endif
					lseek64(fd, filepos, 0);
					in_buffer = 0;	/* clear the buffer */
					goto Fill_the_buffer;
				}
#if 0
				else
					d_printf("End repeat loop\n");
#endif
				break;
			case 8:	/* the extension to play Stereo, I have SB 1.0 :-( */
				was_extended = 1;
				eb = (VocExtBlock *) data;
				COUNT1(sizeof(VocExtBlock));
				hwparams.rate = (int) (eb->tc);
				hwparams.rate = 256000000L / (65536 - hwparams.rate);
				hwparams.channels = eb->mode == VOC_MODE_STEREO ? 2 : 1;
				if (hwparams.channels == 2)
					hwparams.rate = hwparams.rate >> 1;
				if (eb->pack) {		/* /dev/dsp can't it */
					error(_("can't play packed .voc files"));
					return;
				}
#if 0
				d_printf("Extended block %s %d Hz\n",
					 (eb->mode ? "Stereo" : "Mono"), dsp_speed);
#endif
				break;
			default:
				error(_("unknown blocktype %d. terminate."), bp->type);
				return;
			}	/* switch (bp->type) */
		}		/* while (! nextblock)  */
		/* put nextblock data bytes to dsp */
		l = in_buffer;
		if (nextblock < (size_t)l)
			l = nextblock;
		if (l) {
			if (output && !quiet_mode) {
				if (write(2, data, l) != l) {	/* to stderr */
					error(_("write error"));
					prg_exit(EXIT_FAILURE);
				}
			} else {
				if (voc_pcm_write(data, l) != l) {
					error(_("write error"));
					prg_exit(EXIT_FAILURE);
				}
			}
			COUNT(l);
		}
	}			/* while(1) */
      __end:
        voc_pcm_flush();
        free(buf);
}
/* that was a big one, perhaps somebody split it :-) */

/* setting the globals for playing raw data */
static void init_raw_data(void)
{
	hwparams = rhwparams;
}

/* calculate the data count to read from/to dsp */
static off64_t calc_count(void)
{
	off64_t count;

	if (timelimit == 0) {
		count = pbrec_count;
	} else {
		count = snd_pcm_format_size(hwparams.format, hwparams.rate * hwparams.channels);
		count *= (off64_t)timelimit;
	}
	return count < pbrec_count ? count : pbrec_count;
}

/* write a .VOC-header */
static void begin_voc(int fd, size_t cnt)
{
	VocHeader vh;
	VocBlockType bt;
	VocVoiceData vd;
	VocExtBlock eb;

	memcpy(vh.magic, VOC_MAGIC_STRING, 20);
	vh.headerlen = LE_SHORT(sizeof(VocHeader));
	vh.version = LE_SHORT(VOC_ACTUAL_VERSION);
	vh.coded_ver = LE_SHORT(0x1233 - VOC_ACTUAL_VERSION);

	if (write(fd, &vh, sizeof(VocHeader)) != sizeof(VocHeader)) {
		error(_("write error"));
		prg_exit(EXIT_FAILURE);
	}
	if (hwparams.channels > 1) {
		/* write an extended block */
		bt.type = 8;
		bt.datalen = 4;
		bt.datalen_m = bt.datalen_h = 0;
		if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
			error(_("write error"));
			prg_exit(EXIT_FAILURE);
		}
		eb.tc = LE_SHORT(65536 - 256000000L / (hwparams.rate << 1));
		eb.pack = 0;
		eb.mode = 1;
		if (write(fd, &eb, sizeof(VocExtBlock)) != sizeof(VocExtBlock)) {
			error(_("write error"));
			prg_exit(EXIT_FAILURE);
		}
	}
	bt.type = 1;
	cnt += sizeof(VocVoiceData);	/* Channel_data block follows */
	bt.datalen = (u_char) (cnt & 0xFF);
	bt.datalen_m = (u_char) ((cnt & 0xFF00) >> 8);
	bt.datalen_h = (u_char) ((cnt & 0xFF0000) >> 16);
	if (write(fd, &bt, sizeof(VocBlockType)) != sizeof(VocBlockType)) {
		error(_("write error"));
		prg_exit(EXIT_FAILURE);
	}
	vd.tc = (u_char) (256 - (1000000 / hwparams.rate));
	vd.pack = 0;
	if (write(fd, &vd, sizeof(VocVoiceData)) != sizeof(VocVoiceData)) {
		error(_("write error"));
		prg_exit(EXIT_FAILURE);
	}
}

/* write a WAVE-header */
static void begin_wave(int fd, size_t cnt)
{
	WaveHeader h;
	WaveFmtBody f;
	WaveChunkHeader cf, cd;
	int bits;
	u_int tmp;
	u_short tmp2;

	/* WAVE cannot handle greater than 32bit (signed?) int */
	if (cnt == (size_t)-2)
		cnt = 0x7fffff00;

	bits = 8;
	switch ((unsigned long) hwparams.format) {
	case SND_PCM_FORMAT_U8:
		bits = 8;
		break;
	case SND_PCM_FORMAT_S16_LE:
		bits = 16;
		break;
	case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_FLOAT_LE:
		bits = 32;
		break;
	case SND_PCM_FORMAT_S24_LE:
	case SND_PCM_FORMAT_S24_3LE:
		bits = 24;
		break;
	default:
		error(_("Wave doesn't support %s format..."), snd_pcm_format_name(hwparams.format));
		prg_exit(EXIT_FAILURE);
	}
	h.magic = WAV_RIFF;
	tmp = cnt + sizeof(WaveHeader) + sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + sizeof(WaveChunkHeader) - 8;
	h.length = LE_INT(tmp);
	h.type = WAV_WAVE;

	cf.type = WAV_FMT;
	cf.length = LE_INT(16);

        if (hwparams.format == SND_PCM_FORMAT_FLOAT_LE)
                f.format = LE_SHORT(WAV_FMT_IEEE_FLOAT);
        else
                f.format = LE_SHORT(WAV_FMT_PCM);
	f.channels = LE_SHORT(hwparams.channels);
	f.sample_fq = LE_INT(hwparams.rate);
#if 0
	tmp2 = (samplesize == 8) ? 1 : 2;
	f.byte_p_spl = LE_SHORT(tmp2);
	tmp = dsp_speed * hwparams.channels * (u_int) tmp2;
#else
	tmp2 = hwparams.channels * snd_pcm_format_physical_width(hwparams.format) / 8;
	f.byte_p_spl = LE_SHORT(tmp2);
	tmp = (u_int) tmp2 * hwparams.rate;
#endif
	f.byte_p_sec = LE_INT(tmp);
	f.bit_p_spl = LE_SHORT(bits);

	cd.type = WAV_DATA;
	cd.length = LE_INT(cnt);

	if (write(fd, &h, sizeof(WaveHeader)) != sizeof(WaveHeader) ||
	    write(fd, &cf, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader) ||
	    write(fd, &f, sizeof(WaveFmtBody)) != sizeof(WaveFmtBody) ||
	    write(fd, &cd, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader)) {
		error(_("write error"));
		prg_exit(EXIT_FAILURE);
	}
}

/* write a Au-header */
static void begin_au(int fd, size_t cnt)
{
	AuHeader ah;

	ah.magic = AU_MAGIC;
	ah.hdr_size = BE_INT(24);
	ah.data_size = BE_INT(cnt);
	switch ((unsigned long) hwparams.format) {
	case SND_PCM_FORMAT_MU_LAW:
		ah.encoding = BE_INT(AU_FMT_ULAW);
		break;
	case SND_PCM_FORMAT_U8:
		ah.encoding = BE_INT(AU_FMT_LIN8);
		break;
	case SND_PCM_FORMAT_S16_BE:
		ah.encoding = BE_INT(AU_FMT_LIN16);
		break;
	default:
		error(_("Sparc Audio doesn't support %s format..."), snd_pcm_format_name(hwparams.format));
		prg_exit(EXIT_FAILURE);
	}
	ah.sample_rate = BE_INT(hwparams.rate);
	ah.channels = BE_INT(hwparams.channels);
	if (write(fd, &ah, sizeof(AuHeader)) != sizeof(AuHeader)) {
		error(_("write error"));
		prg_exit(EXIT_FAILURE);
	}
}

/* closing .VOC */
static void end_voc(int fd)
{
	off64_t length_seek;
	VocBlockType bt;
	size_t cnt;
	char dummy = 0;		/* Write a Terminator */

	if (write(fd, &dummy, 1) != 1) {
		error(_("write error"));
		prg_exit(EXIT_FAILURE);
	}
	length_seek = sizeof(VocHeader);
	if (hwparams.channels > 1)
		length_seek += sizeof(VocBlockType) + sizeof(VocExtBlock);
	bt.type = 1;
	cnt = fdcount;
	cnt += sizeof(VocVoiceData);	/* Channel_data block follows */
	if (cnt > 0x00ffffff)
		cnt = 0x00ffffff;
	bt.datalen = (u_char) (cnt & 0xFF);
	bt.datalen_m = (u_char) ((cnt & 0xFF00) >> 8);
	bt.datalen_h = (u_char) ((cnt & 0xFF0000) >> 16);
	if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
		write(fd, &bt, sizeof(VocBlockType));
	if (fd != 1)
		close(fd);
}

static void end_wave(int fd)
{				/* only close output */
	WaveChunkHeader cd;
	off64_t length_seek;
	off64_t filelen;
	u_int rifflen;
	
	length_seek = sizeof(WaveHeader) +
		      sizeof(WaveChunkHeader) +
		      sizeof(WaveFmtBody);
	cd.type = WAV_DATA;
	cd.length = fdcount > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(fdcount);
	filelen = fdcount + 2*sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + 4;
	rifflen = filelen > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(filelen);
	if (lseek64(fd, 4, SEEK_SET) == 4)
		write(fd, &rifflen, 4);
	if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
		write(fd, &cd, sizeof(WaveChunkHeader));
	if (fd != 1)
		close(fd);
}

static void end_au(int fd)
{				/* only close output */
	AuHeader ah;
	off64_t length_seek;
	
	length_seek = (char *)&ah.data_size - (char *)&ah;
	ah.data_size = fdcount > 0xffffffff ? 0xffffffff : BE_INT(fdcount);
	if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
		write(fd, &ah.data_size, sizeof(ah.data_size));
	if (fd != 1)
		close(fd);
}

static void header(int rtype, char *name)
{
	if (!quiet_mode) {
		if (! name)
			name = (stream == SND_PCM_STREAM_PLAYBACK) ? "stdout" : "stdin";
		fprintf(stderr, "%s %s '%s' : ",
			(stream == SND_PCM_STREAM_PLAYBACK) ? _("Playing") : _("Recording"),
			gettext(fmt_rec_table[rtype].what),
			name);
		fprintf(stderr, "%s, ", snd_pcm_format_description(hwparams.format));
		fprintf(stderr, _("Rate %d Hz, "), hwparams.rate);
		if (hwparams.channels == 1)
			fprintf(stderr, _("Mono"));
		else if (hwparams.channels == 2)
			fprintf(stderr, _("Stereo"));
		else
			fprintf(stderr, _("Channels %i"), hwparams.channels);
		fprintf(stderr, "\n");
	}
}

/* playing raw data */

static void playback_go(int fd, size_t loaded, off64_t count, int rtype, char *name)
{
	int l, r;
	off64_t written = 0;
	off64_t c;

	header(rtype, name);
	set_params();

	while (loaded > chunk_bytes && written < count && !in_aborting) {
		if (pcm_write(audiobuf + written, chunk_size) <= 0)
			return;
		written += chunk_bytes;
		loaded -= chunk_bytes;
	}
	if (written > 0 && loaded > 0)
		memmove(audiobuf, audiobuf + written, loaded);

	l = loaded;
	while (written < count && !in_aborting) {
		do {
			c = count - written;
			if (c > chunk_bytes)
				c = chunk_bytes;
			c -= l;

			if (c == 0)
				break;
			r = safe_read(fd, audiobuf + l, c);
			if (r < 0) {
				perror(name);
				prg_exit(EXIT_FAILURE);
			}
			fdcount += r;
			if (r == 0)
				break;
			l += r;
		} while ((size_t)l < chunk_bytes);
		l = l * 8 / bits_per_frame;
		r = pcm_write(audiobuf, l);
		if (r != l)
			break;
		r = r * bits_per_frame / 8;
		written += r;
		l = 0;
	}
	snd_pcm_nonblock(handle, 0);
	snd_pcm_drain(handle);
	snd_pcm_nonblock(handle, nonblock);
}


/*
 *  let's play or capture it (capture_type says VOC/WAVE/raw)
 */

static void playback(char *name)
{
	int ofs;
	size_t dta;
	ssize_t dtawave;

	pbrec_count = LLONG_MAX;
	fdcount = 0;
	if (!name || !strcmp(name, "-")) {
		fd = fileno(stdin);
		name = "stdin";
	} else {
		init_stdin();
		if ((fd = open(name, O_RDONLY, 0)) == -1) {
			perror(name);
			prg_exit(EXIT_FAILURE);
		}
	}
	/* read the file header */
	dta = sizeof(AuHeader);
	if ((size_t)safe_read(fd, audiobuf, dta) != dta) {
		error(_("read error"));
		prg_exit(EXIT_FAILURE);
	}
	if (test_au(fd, audiobuf) >= 0) {
		rhwparams.format = hwparams.format;
		pbrec_count = calc_count();
		playback_go(fd, 0, pbrec_count, FORMAT_AU, name);
		goto __end;
	}
	dta = sizeof(VocHeader);
	if ((size_t)safe_read(fd, audiobuf + sizeof(AuHeader),
		 dta - sizeof(AuHeader)) != dta - sizeof(AuHeader)) {
		error(_("read error"));
		prg_exit(EXIT_FAILURE);;
	}
	if ((ofs = test_vocfile(audiobuf)) >= 0) {
		pbrec_count = calc_count();
		voc_play(fd, ofs, name);
		goto __end;
	}
	/* read bytes for WAVE-header */
	if ((dtawave = test_wavefile(fd, audiobuf, dta)) >= 0) {
		pbrec_count = calc_count();
		playback_go(fd, dtawave, pbrec_count, FORMAT_WAVE, name);
	} else {
		/* should be raw data */
		init_raw_data();
		pbrec_count = calc_count();
		playback_go(fd, dta, pbrec_count, FORMAT_RAW, name);
	}
      __end:
	if (fd != 0)
		close(fd);
}

/**
 * mystrftime
 *
 *   Variant of strftime(3) that supports additional format
 *   specifiers in the format string.
 *
 * Parameters:
 *
 *   s	  - destination string
 *   max	- max number of bytes to write
 *   userformat - format string
 *   tm	 - time information
 *   filenumber - the number of the file, starting at 1
 *
 * Returns: number of bytes written to the string s
 */
size_t mystrftime(char *s, size_t max, const char *userformat,
		  const struct tm *tm, const int filenumber)
{
	char formatstring[PATH_MAX] = "";
	char tempstring[PATH_MAX] = "";
	char *format, *tempstr;
	const char *pos_userformat;

	format = formatstring;

	/* if mystrftime is called with userformat = NULL we return a zero length string */
	if (userformat == NULL) {
		*s = '\0';
		return 0;
	}

	for (pos_userformat = userformat; *pos_userformat; ++pos_userformat) {
		if (*pos_userformat == '%') {
			tempstr = tempstring;
			tempstr[0] = '\0';
			switch (*++pos_userformat) {

				case '\0': // end of string
					--pos_userformat;
					break;

				case 'v': // file number 
					sprintf(tempstr, "%02d", filenumber);
					break;

				default: // All other codes will be handled by strftime
					*format++ = '%';
					*format++ = *pos_userformat;
					continue;
			}

			/* If a format specifier was found and used, copy the result. */
			if (tempstr[0]) {
				while ((*format = *tempstr++) != '\0')
					++format;
				continue;
			}
		}

		/* For any other character than % we simply copy the character */
		*format++ = *pos_userformat;
	}

	*format = '\0';
	format = formatstring;
	return strftime(s, max, format, tm);
}

static int new_capture_file(char *name, char *namebuf, size_t namelen,
			    int filecount)
{
	char *s;
	char buf[PATH_MAX+1];
	time_t t;
	struct tm *tmp;

	if (use_strftime) {
		t = time(NULL);
		tmp = localtime(&t);
		if (tmp == NULL) {
			perror("localtime");
			prg_exit(EXIT_FAILURE);
		}
		if (mystrftime(namebuf, namelen, name, tmp, filecount+1) == 0) {
			fprintf(stderr, "mystrftime returned 0");
			prg_exit(EXIT_FAILURE);
		}
		return filecount;
	}

	/* get a copy of the original filename */
	strncpy(buf, name, sizeof(buf));

	/* separate extension from filename */
	s = buf + strlen(buf);
	while (s > buf && *s != '.' && *s != '/')
		--s;
	if (*s == '.')
		*s++ = 0;
	else if (*s == '/')
		s = buf + strlen(buf);

	/* upon first jump to this if block rename the first file */
	if (filecount == 1) {
		if (*s)
			snprintf(namebuf, namelen, "%s-01.%s", buf, s);
		else
			snprintf(namebuf, namelen, "%s-01", buf);
		remove(namebuf);
		rename(name, namebuf);
		filecount = 2;
	}

	/* name of the current file */
	if (*s)
		snprintf(namebuf, namelen, "%s-%02i.%s", buf, filecount, s);
	else
		snprintf(namebuf, namelen, "%s-%02i", buf, filecount);

	return filecount;
}

/**
 * create_path
 *
 *   This function creates a file path, like mkdir -p. 
 *
 * Parameters:
 *
 *   path - the path to create
 *
 * Returns: 0 on success, -1 on failure
 * On failure, a message has been printed to stderr.
 */
int create_path(const char *path)
{
	char *start;
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

	if (path[0] == '/')
		start = strchr(path + 1, '/');
	else
		start = strchr(path, '/');

	while (start) {
		char *buffer = strdup(path);
		buffer[start-path] = 0x00;

		if (mkdir(buffer, mode) == -1 && errno != EEXIST) {
			fprintf(stderr, "Problem creating directory %s", buffer);
			perror(" ");
			free(buffer);
			return -1;
		}
		free(buffer);
		start = strchr(start + 1, '/');
	}
	return 0;
}

static int safe_open(const char *name)
{
	int fd;

	fd = open(name, O_WRONLY | O_CREAT, 0644);
	if (fd == -1) {
		if (errno != ENOENT || !use_strftime)
			return -1;
		if (create_path(name) == 0)
			fd = open(name, O_WRONLY | O_CREAT, 0644);
	}
	return fd;
}

static void capture(char *orig_name)
{
	int tostdout=0;		/* boolean which describes output stream */
	int filecount=0;	/* number of files written */
	char *name = orig_name;	/* current filename */
	char namebuf[PATH_MAX+1];
	off64_t count, rest;		/* number of bytes to capture */
	struct stat statbuf;

	/* get number of bytes to capture */
	count = calc_count();
	if (count == 0)
		count = LLONG_MAX;
	/* compute the number of bytes per file */
	max_file_size = max_file_time *
		snd_pcm_format_size(hwparams.format,
				    hwparams.rate * hwparams.channels);
	/* WAVE-file should be even (I'm not sure), but wasting one byte
	   isn't a problem (this can only be in 8 bit mono) */
	if (count < LLONG_MAX)
		count += count % 2;
	else
		count -= count % 2;

	/* display verbose output to console */
	header(file_type, name);

	/* setup sound hardware */
	set_params();

	/* write to stdout? */
	if (!name || !strcmp(name, "-")) {
		fd = fileno(stdout);
		name = "stdout";
		tostdout=1;
		if (count > fmt_rec_table[file_type].max_filesize)
			count = fmt_rec_table[file_type].max_filesize;
	}
	init_stdin();

	do {
		/* open a file to write */
		if(!tostdout) {
			/* upon the second file we start the numbering scheme */
			if (filecount || use_strftime) {
				filecount = new_capture_file(orig_name, namebuf,
							     sizeof(namebuf),
							     filecount);
				name = namebuf;
			}
			
			/* open a new file */
			if (!lstat(name, &statbuf)) {
				if (S_ISREG(statbuf.st_mode))
					remove(name);
			}
			fd = safe_open(name);
			if (fd < 0) {
				perror(name);
				prg_exit(EXIT_FAILURE);
			}
			filecount++;
		}

		rest = count;
		if (rest > fmt_rec_table[file_type].max_filesize)
			rest = fmt_rec_table[file_type].max_filesize;
		if (max_file_size && (rest > max_file_size)) 
			rest = max_file_size;

		/* setup sample header */
		if (fmt_rec_table[file_type].start)
			fmt_rec_table[file_type].start(fd, rest);

		/* capture */
		fdcount = 0;
		while (rest > 0 && recycle_capture_file == 0 && !in_aborting) {
			size_t c = (rest <= (off64_t)chunk_bytes) ?
				(size_t)rest : chunk_bytes;
			size_t f = c * 8 / bits_per_frame;
			if (pcm_read(audiobuf, f) != f) {
				in_aborting = 1;
				break;
			}
			if (write(fd, audiobuf, c) != c) {
				perror(name);
				in_aborting = 1;
				break;
			}
			count -= c;
			rest -= c;
			fdcount += c;
		}

		/* re-enable SIGUSR1 signal */
		if (recycle_capture_file) {
			recycle_capture_file = 0;
			signal(SIGUSR1, signal_handler_recycle);
		}

		/* finish sample container */
		if (fmt_rec_table[file_type].end && !tostdout) {
			fmt_rec_table[file_type].end(fd);
			fd = -1;
		}

		if (in_aborting)
			prg_exit(EXIT_FAILURE);

		/* repeat the loop when format is raw without timelimit or
		 * requested counts of data are recorded
		 */
	} while ((file_type == FORMAT_RAW && !timelimit) || count > 0);
}

static void playbackv_go(int* fds, unsigned int channels, size_t loaded, off64_t count, int rtype, char **names)
{
	int r;
	size_t vsize;

	unsigned int channel;
	u_char *bufs[channels];

	header(rtype, names[0]);
	set_params();

	vsize = chunk_bytes / channels;

	// Not yet implemented
	assert(loaded == 0);

	for (channel = 0; channel < channels; ++channel)
		bufs[channel] = audiobuf + vsize * channel;

	while (count > 0 && !in_aborting) {
		size_t c = 0;
		size_t expected = count / channels;
		if (expected > vsize)
			expected = vsize;
		do {
			r = safe_read(fds[0], bufs[0], expected);
			if (r < 0) {
				perror(names[channel]);
				prg_exit(EXIT_FAILURE);
			}
			for (channel = 1; channel < channels; ++channel) {
				if (safe_read(fds[channel], bufs[channel], r) != r) {
					perror(names[channel]);
					prg_exit(EXIT_FAILURE);
				}
			}
			if (r == 0)
				break;
			c += r;
		} while (c < expected);
		c = c * 8 / bits_per_sample;
		r = pcm_writev(bufs, channels, c);
		if ((size_t)r != c)
			break;
		r = r * bits_per_frame / 8;
		count -= r;
	}
	snd_pcm_nonblock(handle, 0);
	snd_pcm_drain(handle);
	snd_pcm_nonblock(handle, nonblock);
}

static void capturev_go(int* fds, unsigned int channels, off64_t count, int rtype, char **names)
{
	size_t c;
	ssize_t r;
	unsigned int channel;
	size_t vsize;
	u_char *bufs[channels];

	header(rtype, names[0]);
	set_params();

	vsize = chunk_bytes / channels;

	for (channel = 0; channel < channels; ++channel)
		bufs[channel] = audiobuf + vsize * channel;

	while (count > 0 && !in_aborting) {
		size_t rv;
		c = count;
		if (c > chunk_bytes)
			c = chunk_bytes;
		c = c * 8 / bits_per_frame;
		if ((size_t)(r = pcm_readv(bufs, channels, c)) != c)
			break;
		rv = r * bits_per_sample / 8;
		for (channel = 0; channel < channels; ++channel) {
			if ((size_t)write(fds[channel], bufs[channel], rv) != rv) {
				perror(names[channel]);
				prg_exit(EXIT_FAILURE);
			}
		}
		r = r * bits_per_frame / 8;
		count -= r;
		fdcount += r;
	}
}

static void playbackv(char **names, unsigned int count)
{
	int ret = 0;
	unsigned int channel;
	unsigned int channels = rhwparams.channels;
	int alloced = 0;
	int fds[channels];
	for (channel = 0; channel < channels; ++channel)
		fds[channel] = -1;

	if (count == 1 && channels > 1) {
		size_t len = strlen(names[0]);
		char format[1024];
		memcpy(format, names[0], len);
		strcpy(format + len, ".%d");
		len += 4;
		names = malloc(sizeof(*names) * channels);
		for (channel = 0; channel < channels; ++channel) {
			names[channel] = malloc(len);
			sprintf(names[channel], format, channel);
		}
		alloced = 1;
	} else if (count != channels) {
		error(_("You need to specify %d files"), channels);
		prg_exit(EXIT_FAILURE);
	}

	for (channel = 0; channel < channels; ++channel) {
		fds[channel] = open(names[channel], O_RDONLY, 0);
		if (fds[channel] < 0) {
			perror(names[channel]);
			ret = EXIT_FAILURE;
			goto __end;
		}
	}
	/* should be raw data */
	init_raw_data();
	pbrec_count = calc_count();
	playbackv_go(fds, channels, 0, pbrec_count, FORMAT_RAW, names);

      __end:
	for (channel = 0; channel < channels; ++channel) {
		if (fds[channel] >= 0)
			close(fds[channel]);
		if (alloced)
			free(names[channel]);
	}
	if (alloced)
		free(names);
	if (ret)
		prg_exit(ret);
}

static void capturev(char **names, unsigned int count)
{
	int ret = 0;
	unsigned int channel;
	unsigned int channels = rhwparams.channels;
	int alloced = 0;
	int fds[channels];
	for (channel = 0; channel < channels; ++channel)
		fds[channel] = -1;

	if (count == 1) {
		size_t len = strlen(names[0]);
		char format[1024];
		memcpy(format, names[0], len);
		strcpy(format + len, ".%d");
		len += 4;
		names = malloc(sizeof(*names) * channels);
		for (channel = 0; channel < channels; ++channel) {
			names[channel] = malloc(len);
			sprintf(names[channel], format, channel);
		}
		alloced = 1;
	} else if (count != channels) {
		error(_("You need to specify %d files"), channels);
		prg_exit(EXIT_FAILURE);
	}

	for (channel = 0; channel < channels; ++channel) {
		fds[channel] = open(names[channel], O_WRONLY + O_CREAT, 0644);
		if (fds[channel] < 0) {
			perror(names[channel]);
			ret = EXIT_FAILURE;
			goto __end;
		}
	}
	/* should be raw data */
	init_raw_data();
	pbrec_count = calc_count();
	capturev_go(fds, channels, pbrec_count, FORMAT_RAW, names);

      __end:
	for (channel = 0; channel < channels; ++channel) {
		if (fds[channel] >= 0)
			close(fds[channel]);
		if (alloced)
			free(names[channel]);
	}
	if (alloced)
		free(names);
	if (ret)
		prg_exit(ret);
}
