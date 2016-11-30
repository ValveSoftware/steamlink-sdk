/*
 * Copyright (C) 2013-2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define TEMP_RECORD_FILE_NAME		"/tmp/bat.wav.XXXXXX"
#define DEFAULT_DEV_NAME		"default"

#define OPT_BASE			300
#define OPT_LOG				(OPT_BASE + 1)
#define OPT_READFILE			(OPT_BASE + 2)
#define OPT_SAVEPLAY			(OPT_BASE + 3)
#define OPT_LOCAL			(OPT_BASE + 4)
#define OPT_STANDALONE			(OPT_BASE + 5)
#define OPT_ROUNDTRIPLATENCY		(OPT_BASE + 6)
#define OPT_SNRTHD_DB			(OPT_BASE + 7)
#define OPT_SNRTHD_PC			(OPT_BASE + 8)

#define COMPOSE(a, b, c, d)		((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define WAV_RIFF			COMPOSE('R', 'I', 'F', 'F')
#define WAV_WAVE			COMPOSE('W', 'A', 'V', 'E')
#define WAV_FMT				COMPOSE('f', 'm', 't', ' ')
#define WAV_DATA			COMPOSE('d', 'a', 't', 'a')
#define WAV_FORMAT_PCM			1	/* PCM WAVE file encoding */

#define MAX_CHANNELS			2
#define MIN_CHANNELS			1
#define MAX_PEAKS			10
#define MAX_FRAMES			(10 * 1024 * 1024)
/* Given in ms */
#define CAPTURE_DELAY			500
/* signal frequency should be less than samplerate * RATE_FACTOR */
#define RATE_FACTOR			0.4
/* valid range of samplerate: (1 - RATE_RANGE, 1 + RATE_RANGE) * samplerate */
#define RATE_RANGE			0.05
/* Given in us */
#define MAX_BUFFERTIME			500000
/* devide factor, was 4, changed to 8 to remove reduce capture overrun */
#define DIV_BUFFERTIME			8
/* margin to avoid sign inversion when generate sine wav */
#define RANGE_FACTOR			0.95
#define MAX_BUFFERSIZE			200000
#define MIN_BUFFERSIZE			32
#define MAX_PERIODSIZE			200000
#define MIN_PERIODSIZE			32
/* default period size for tinyalsa */
#define TINYALSA_PERIODSIZE			1024

#define LATENCY_TEST_NUMBER			5
#define LATENCY_TEST_TIME_LIMIT			25
#define DIV_BUFFERSIZE			2

#define EBATBASE			1000
#define ENOPEAK				(EBATBASE + 1)
#define EONLYDC				(EBATBASE + 2)
#define EBADPEAK			(EBATBASE + 3)

#define DC_THRESHOLD			7.01

/* tolerance of detected peak = max (DELTA_HZ, DELTA_RATE * target_freq).
 * If DELTA_RATE is too high, BAT may not be able to recognize negative result;
 * if too low, BAT may be too sensitive and results in uncecessary failure. */
#define DELTA_RATE			0.005
#define DELTA_HZ			1

#define FOUND_DC			(1<<1)
#define FOUND_WRONG_PEAK		(1<<0)

/* Truncate sample frames to (1 << N), for faster FFT analysis process. The
 * valid range of N is (SHIFT_MIN, SHIFT_MAX). When N increases, the analysis
 * will be more time-consuming, and the result will be more accurate. */
#define SHIFT_MAX			(sizeof(int) * 8 - 2)
#define SHIFT_MIN			8

/* Define SNR range in dB.
 * if the noise is equal to signal, SNR = 0.0dB;
 * if the noise is zero, SNR is limited by RIFF wav data width:
 *     8 bit  -->  20.0 * log10f (powf(2.0, 8.0))  = 48.16 dB
 *    16 bit  -->  20.0 * log10f (powf(2.0, 16.0)) = 96.33 dB
 *    24 bit  -->  20.0 * log10f (powf(2.0, 24.0)) = 144.49 dB
 *    32 bit  -->  20.0 * log10f (powf(2.0, 32.0)) = 192.66 dB
 * so define the SNR range (0.0, 200.0) dB, value out of range is invalid. */
#define SNR_DB_INVALID			-1.0
#define SNR_DB_MIN			0.0
#define SNR_DB_MAX			200.0

static inline bool snr_is_valid(float db)
{
	return (db > SNR_DB_MIN && db < SNR_DB_MAX);
}

struct wav_header {
	unsigned int magic; /* 'RIFF' */
	unsigned int length; /* file len */
	unsigned int type; /* 'WAVE' */
};

struct wav_chunk_header {
	unsigned int type; /* 'data' */
	unsigned int length; /* sample count */
};

struct wav_fmt {
	unsigned int magic; /* 'FMT '*/
	unsigned int fmt_size; /* 16 or 18 */
	unsigned short format; /* see WAV_FMT_* */
	unsigned short channels;
	unsigned int sample_rate; /* Frequency of sample */
	unsigned int bytes_p_second;
	unsigned short blocks_align; /* sample size; 1 or 2 bytes */
	unsigned short sample_length; /* 8, 12 or 16 bit */
};

struct chunk_fmt {
	unsigned short format; /* see WAV_FMT_* */
	unsigned short channels;
	unsigned int sample_rate; /* Frequency of sample */
	unsigned int bytes_p_second;
	unsigned short blocks_align; /* sample size; 1 or 2 bytes */
	unsigned short sample_length; /* 8, 12 or 16 bit */
};

struct wav_container {
	struct wav_header header;
	struct wav_fmt format;
	struct wav_chunk_header chunk;
};

struct bat;

enum _bat_pcm_format {
	BAT_PCM_FORMAT_UNKNOWN = -1,
	BAT_PCM_FORMAT_S16_LE = 0,
	BAT_PCM_FORMAT_S32_LE,
	BAT_PCM_FORMAT_U8,
	BAT_PCM_FORMAT_S24_3LE,
	BAT_PCM_FORMAT_MAX
};

enum _bat_op_mode {
	MODE_UNKNOWN = -1,
	MODE_SINGLE = 0,
	MODE_LOOPBACK,
	MODE_LAST
};

enum latency_state {
	LATENCY_STATE_COMPLETE_FAILURE = -1,
	LATENCY_STATE_COMPLETE_SUCCESS = 0,
	LATENCY_STATE_MEASURE_FOR_1_SECOND,
	LATENCY_STATE_PLAY_AND_LISTEN,
	LATENCY_STATE_WAITING,
};

struct pcm {
	unsigned int card_tiny;
	unsigned int device_tiny;
	char *device;
	char *file;
	enum _bat_op_mode mode;
	void *(*fct)(struct bat *);
};

struct sin_generator;

struct sin_generator {
	double state_real;
	double state_imag;
	double phasor_real;
	double phasor_imag;
	float frequency;
	float sample_rate;
	float magnitude;
};

struct roundtrip_latency {
	int number;
	enum latency_state state;
	float result[LATENCY_TEST_NUMBER];
	int final_result;
	int samples;
	float sum;
	int threshold;
	int error;
	bool is_capturing;
	bool is_playing;
	bool xrun_error;
};

struct noise_analyzer {
	int nsamples;			/* number of sample */
	float *source;			/* single-tone to be analyzed */
	float *target;			/* target single-tone as standard */
	float rms_tgt;			/* rms of target single-tone */
	float snr_db;			/* snr in dB */
};

struct bat {
	unsigned int rate;		/* sampling rate */
	int channels;			/* nb of channels */
	int frames;			/* nb of frames */
	int frame_size;			/* size of frame */
	int sample_size;		/* size of sample */
	enum _bat_pcm_format format;	/* PCM format */
	int buffer_size;		/* buffer size in frames */
	int period_size;		/* period size in frames */

	float sigma_k;			/* threshold for peak detection */
	float snr_thd_db;		/* threshold for noise detection (dB) */
	float target_freq[MAX_CHANNELS];

	int sinus_duration;		/* number of frames for playback */
	char *narg;			/* argument string of duration */
	char *logarg;			/* path name of log file */
	char *debugplay;		/* path name to store playback signal */
	bool standalone;		/* enable to bypass analysis */
	bool roundtriplatency;		/* enable round trip latency */

	struct pcm playback;
	struct pcm capture;
	struct roundtrip_latency latency;

	unsigned int periods_played;
	unsigned int periods_total;
	bool period_is_limited;

	FILE *fp;

	FILE *log;
	FILE *err;

	void (*convert_sample_to_float)(void *, float *, int);
	void (*convert_float_to_sample)(float *, void *, int, int);

	void *buf;			/* PCM Buffer */

	bool local;			/* true for internal test */
};

struct analyze {
	void *buf;
	float *in;
	float *out;
	float *mag;
};

void prepare_wav_info(struct wav_container *, struct bat *);
int read_wav_header(struct bat *, char *, FILE *, bool);
int write_wav_header(FILE *, struct wav_container *, struct bat *);
int update_wav_header(struct bat *, FILE *, int);
int generate_input_data(struct bat *, void *, int, int);
