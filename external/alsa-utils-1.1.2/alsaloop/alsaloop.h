/*
 *  A simple PCM loopback utility
 *  Copyright (c) 2010 by Jaroslav Kysela <perex@perex.cz>
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

#include "aconfig.h"
#ifdef HAVE_SAMPLERATE_H
#define USE_SAMPLERATE
#include <samplerate.h>
#else
enum {
	SRC_SINC_BEST_QUALITY	= 0,
	SRC_SINC_MEDIUM_QUALITY	= 1,
	SRC_SINC_FASTEST	= 2,
	SRC_ZERO_ORDER_HOLD	= 3,
	SRC_LINEAR		= 4
};
#endif

#define MAX_ARGS	128
#define MAX_MIXERS	64

#if 0
#define FILE_PWRITE "/tmp/alsaloop.praw"
#define FILE_CWRITE "/tmp/alsaloop.craw"
#endif

#define WORKAROUND_SERIALOPEN	(1<<0)

typedef enum _sync_type {
	SYNC_TYPE_NONE = 0,
	SYNC_TYPE_SIMPLE,	/* add or remove samples */
	SYNC_TYPE_CAPTRATESHIFT,
	SYNC_TYPE_PLAYRATESHIFT,
	SYNC_TYPE_SAMPLERATE,
	SYNC_TYPE_AUTO,		/* order: CAPTRATESHIFT, PLAYRATESHIFT, */
				/*        SAMPLERATE, SIMPLE */
	SYNC_TYPE_LAST = SYNC_TYPE_AUTO
} sync_type_t;

typedef enum _slave_type {
	SLAVE_TYPE_AUTO = 0,
	SLAVE_TYPE_ON = 1,
	SLAVE_TYPE_OFF = 2,
	SLAVE_TYPE_LAST = SLAVE_TYPE_OFF
} slave_type_t;

struct loopback_control {
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *value;
};

struct loopback_mixer {
	unsigned int skip:1;
	struct loopback_control src;
	struct loopback_control dst;
	struct loopback_mixer *next;
};

struct loopback_ossmixer {
	unsigned int skip:1;
	const char *alsa_id;
	int alsa_index;
	const char *oss_id;
	struct loopback_ossmixer *next;
};

struct loopback_handle {
	struct loopback *loopback;
	char *device;
	char *ctldev;
	char *id;
	int card_number;
	snd_pcm_t *handle;
	snd_pcm_access_t access;
	snd_pcm_format_t format;
	unsigned int rate;
	unsigned int rate_req;
	unsigned int channels;
	unsigned int buffer_size;
	unsigned int period_size;
	snd_pcm_uframes_t avail_min;
	unsigned int buffer_size_req;
	unsigned int period_size_req;
	unsigned int frame_size;
	unsigned int resample:1;	/* do resample */
	unsigned int nblock:1;		/* do block (period size) transfers */
	unsigned int xrun_pending:1;
	unsigned int pollfd_count;
	/* I/O job */
	char *buf;			/* I/O buffer */
	snd_pcm_uframes_t buf_pos;	/* I/O position */
	snd_pcm_uframes_t buf_count;	/* filled samples */
	snd_pcm_uframes_t buf_size;	/* buffer size in frames */
	snd_pcm_uframes_t buf_over;	/* capture buffer overflow */
	/* statistics */
	snd_pcm_uframes_t max;
	unsigned long long counter;
	unsigned long sync_point;	/* in samples */
	snd_pcm_sframes_t last_delay;
	double pitch;
	snd_pcm_uframes_t total_queued;
	/* control */
	snd_ctl_t *ctl;
	unsigned int ctl_pollfd_count;
	snd_ctl_elem_value_t *ctl_notify;
	snd_ctl_elem_value_t *ctl_rate_shift;
	snd_ctl_elem_value_t *ctl_active;
	snd_ctl_elem_value_t *ctl_format;
	snd_ctl_elem_value_t *ctl_rate;
	snd_ctl_elem_value_t *ctl_channels;
};

struct loopback {
	char *id;
	struct loopback_handle *capt;
	struct loopback_handle *play;
	snd_pcm_uframes_t latency;	/* final latency in frames */
	unsigned int latency_req;	/* in frames */
	unsigned int latency_reqtime;	/* in us */
	unsigned long loop_time;	/* ~0 = unlimited (in seconds) */
	unsigned long long loop_limit;	/* ~0 = unlimited (in frames) */
	snd_output_t *output;
	snd_output_t *state;
	int pollfd_count;
	int active_pollfd_count;
	unsigned int linked:1;		/* linked streams */
	unsigned int reinit:1;
	unsigned int running:1;
	unsigned int stop_pending:1;
	snd_pcm_uframes_t stop_count;
	sync_type_t sync;		/* type of sync */
	slave_type_t slave;
	int thread;			/* thread number */
	unsigned int wake;
	/* statistics */
	double pitch;
	double pitch_delta;
	snd_pcm_sframes_t pitch_diff;
	snd_pcm_sframes_t pitch_diff_min;
	snd_pcm_sframes_t pitch_diff_max;
	unsigned int total_queued_count;
	snd_timestamp_t tstamp_start;
	snd_timestamp_t tstamp_end;
	/* xrun profiling */
	unsigned int xrun:1;		/* xrun profiling */
	snd_timestamp_t xrun_last_update;
	snd_timestamp_t xrun_last_wake0;
	snd_timestamp_t xrun_last_wake;
	snd_timestamp_t xrun_last_check0;
	snd_timestamp_t xrun_last_check;
	snd_pcm_sframes_t xrun_last_pdelay;
	snd_pcm_sframes_t xrun_last_cdelay;
	snd_pcm_uframes_t xrun_buf_pcount;
	snd_pcm_uframes_t xrun_buf_ccount;
	unsigned int xrun_out_frames;
	long xrun_max_proctime;
	double xrun_max_missing;
	/* control mixer */
	struct loopback_mixer *controls;
	struct loopback_ossmixer *oss_controls;
	/* sample rate */
	unsigned int use_samplerate:1;
#ifdef USE_SAMPLERATE
	unsigned int src_enable:1;
	int src_converter_type;
	SRC_STATE *src_state;
	SRC_DATA src_data;
	unsigned int src_out_frames;
#endif
#ifdef FILE_CWRITE
	FILE *cfile;
#endif
#ifdef FILE_PWRITE
	FILE *pfile;
#endif
};

extern int verbose;
extern int workarounds;
extern int use_syslog;

#define logit(priority, fmt, args...) do {		\
	if (use_syslog)					\
		syslog(priority, fmt, ##args);		\
	else						\
		fprintf(stderr, fmt, ##args);		\
} while (0)

int pcmjob_init(struct loopback *loop);
int pcmjob_done(struct loopback *loop);
int pcmjob_start(struct loopback *loop);
int pcmjob_stop(struct loopback *loop);
int pcmjob_pollfds_init(struct loopback *loop, struct pollfd *fds);
int pcmjob_pollfds_handle(struct loopback *loop, struct pollfd *fds);
void pcmjob_state(struct loopback *loop);

int control_parse_id(const char *str, snd_ctl_elem_id_t *id);
int control_id_match(snd_ctl_elem_id_t *id1, snd_ctl_elem_id_t *id2);
int control_init(struct loopback *loop);
int control_done(struct loopback *loop);
int control_event(struct loopback_handle *lhandle, snd_ctl_event_t *ev);
