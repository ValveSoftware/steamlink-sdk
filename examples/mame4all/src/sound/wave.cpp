/***************************************************************************

	wave.c

	Functions to handle loading, creation, recording and playback
	of wave samples for IO_CASSETTE

****************************************************************************/
#include "driver.h"

/* from mame.c */
extern int bitmap_dirty;

/* Our private wave file structure */
struct wave_file {
	int channel;			/* channel for playback */
	void *file; 			/* osd file handle */
	int mode;				/* write mode? */
	int (*fill_wave)(INT16 *,int,UINT8*);
	void *timer;			/* timer (TIME_NEVER) for reading sample values */
	INT16 play_sample;		/* current sample value for playback */
	INT16 record_sample;	/* current sample value for playback */
	int display;			/* display tape status on screen */
	int offset; 			/* offset set by device_seek function */
	int playpos;			/* sample position for playback */
	int counter;			/* sample fraction counter for playback */
	int smpfreq;			/* sample frequency from the WAV header */
	int resolution; 		/* sample resolution in bits/sample (8 or 16) */
	int samples;			/* number of samples (length * resolution / 8) */
	int length; 			/* length in bytes */
	void *data; 			/* sample data */
	int mute;				/* mute if non-zero */
};

static struct Wave_interface *intf;
static struct wave_file wave[MAX_WAVE] = {{-1,},{-1,}};

#ifdef LSB_FIRST
#define intelLong(x) (x)
#else
#define intelLong(x) (((x << 24) | (((unsigned long) x) >> 24) | \
                       (( x & 0x0000ff00) << 8) | (( x & 0x00ff0000) >> 8)))
#endif

#define WAVE_OK    0
#define WAVE_ERR   1
#define WAVE_FMT   2

/*****************************************************************************
 * helper functions
 *****************************************************************************/
static int wave_read(int id)
{
	struct wave_file *w = &wave[id];
    UINT32 offset = 0;
	UINT32 filesize, temp32;
	UINT16 channels, bits, temp16;
	char buf[32];

	if( !w->file )
		return WAVE_ERR;

    /* read the core header and make sure it's a WAVE file */
	offset += osd_fread(w->file, buf, 4);
	if( offset < 4 )
	{
		logerror("WAVE read error at offs %d\n", offset);
		return WAVE_ERR;
	}
	if( memcmp (&buf[0], "RIFF", 4) != 0 )
	{
		logerror("WAVE header not 'RIFF'\n");
		return WAVE_FMT;
    }

	/* get the total size */
	offset += osd_fread(w->file, &temp32, 4);
	if( offset < 8 )
	{
		logerror("WAVE read error at offs %d\n", offset);
		return WAVE_ERR;
	}
	filesize = intelLong(temp32);
	logerror("WAVE filesize %u bytes\n", filesize);

	/* read the RIFF file type and make sure it's a WAVE file */
	offset += osd_fread(w->file, buf, 4);
	if( offset < 12 )
	{
		logerror("WAVE read error at offs %d\n", offset);
		return WAVE_ERR;
	}
	if( memcmp (&buf[0], "WAVE", 4) != 0 )
	{
		logerror("WAVE RIFF type not 'WAVE'\n");
		return WAVE_FMT;
	}

	/* seek until we find a format tag */
	while( 1 )
	{
		offset += osd_fread(w->file, buf, 4);
		offset += osd_fread(w->file, &temp32, 4);
		w->length = intelLong(temp32);
		if( memcmp(&buf[0], "fmt ", 4) == 0 )
			break;

		/* seek to the next block */
		osd_fseek(w->file, w->length, SEEK_CUR);
		offset += w->length;
		if( offset >= filesize )
		{
			logerror("WAVE no 'fmt ' tag found\n");
			return WAVE_ERR;
        }
	}

	/* read the format -- make sure it is PCM */
	offset += osd_fread_lsbfirst(w->file, &temp16, 2);
	if( temp16 != 1 )
	{
		logerror("WAVE format %d not supported (not = 1 PCM)\n", temp16);
			return WAVE_ERR;
    }
	logerror("WAVE format %d (PCM)\n", temp16);

	/* number of channels -- only mono is supported */
	offset += osd_fread_lsbfirst(w->file, &channels, 2);
	if( channels != 1 && channels != 2 )
	{
		logerror("WAVE channels %d not supported (only 1 mono or 2 stereo)\n", channels);
		return WAVE_ERR;
    }
	logerror("WAVE channels %d\n", channels);

	/* sample rate */
	offset += osd_fread(w->file, &temp32, 4);
	w->smpfreq = intelLong(temp32);
	logerror("WAVE sample rate %d Hz\n", w->smpfreq);

	/* bytes/second and block alignment are ignored */
	offset += osd_fread(w->file, buf, 6);

	/* bits/sample */
	offset += osd_fread_lsbfirst(w->file, &bits, 2);
	if( bits != 8 && bits != 16 )
	{
		logerror("WAVE %d bits/sample not supported (only 8/16)\n", bits);
		return WAVE_ERR;
    }
	logerror("WAVE bits/sample %d\n", bits);
	w->resolution = bits;

	/* seek past any extra data */
	osd_fseek(w->file, w->length - 16, SEEK_CUR);
	offset += w->length - 16;

	/* seek until we find a data tag */
	while( 1 )
	{
		offset += osd_fread(w->file, buf, 4);
		offset += osd_fread(w->file, &temp32, 4);
		w->length = intelLong(temp32);
		if( memcmp(&buf[0], "data", 4) == 0 )
			break;

		/* seek to the next block */
		osd_fseek(w->file, w->length, SEEK_CUR);
		offset += w->length;
		if( offset >= filesize )
		{
			logerror("WAVE not 'data' tag found\n");
			return WAVE_ERR;
        }
	}

	/* allocate the game sample */
	w->data = malloc(w->length);

	if( w->data == NULL )
	{
		logerror("WAVE failed to malloc %d bytes\n", w->length);
		return WAVE_ERR;
    }

	/* read the data in */
	if( w->resolution == 8 )
	{
		if( osd_fread(w->file, w->data, w->length) != w->length )
		{
			logerror("WAVE failed read %d data bytes\n", w->length);
			free(w->data);
			return WAVE_ERR;
		}
		if( channels == 2 )
		{
			UINT8 *src = w->data;
			INT8 *dst = w->data;
			logerror("WAVE mixing 8-bit unsigned stereo to 8-bit signed mono\n");
            /* convert stereo 8-bit data to mono signed samples */
			for( temp32 = 0; temp32 < w->length/2; temp32++ )
			{
				*dst = ((src[0] + src[1]) / 2) ^ 0x80;
				dst += 1;
				src += 2;
			}
			w->length /= 2;
            w->data = realloc(w->data, w->length);
			if( w->data == NULL )
			{
				logerror("WAVE failed to malloc %d bytes\n", w->length);
				return WAVE_ERR;
			}
        }
		else
		{
			UINT8 *src = w->data;
			INT8 *dst = w->data;
            logerror("WAVE converting 8-bit unsigned to 8-bit signed\n");
            /* convert 8-bit data to signed samples */
			for( temp32 = 0; temp32 < w->length; temp32++ )
				*dst++ = *src++ ^ 0x80;
		}
	}
	else
	{
		/* 16-bit data is fine as-is */
		if( osd_fread_lsbfirst(w->file, w->data, w->length) != w->length )
		{
			logerror("WAVE failed read %d data bytes\n", w->length);
			free(w->data);
			return WAVE_ERR;
        }
        if( channels == 2 )
        {
			INT16 *src = w->data;
			INT16 *dst = w->data;
            logerror("WAVE mixing 16-bit stereo to 16-bit mono\n");
            /* convert stereo 16-bit data to mono */
			for( temp32 = 0; temp32 < w->length/2; temp32++ )
			{
				*dst = ((INT32)src[0] + (INT32)src[1]) / 2;
				dst += 1;
				src += 2;
			}
			w->length /= 2;
			w->data = realloc(w->data, w->length);
			if( w->data == NULL )
			{
				logerror("WAVE failed to malloc %d bytes\n", w->length);
				return WAVE_ERR;
            }
        }
		else
		{
			logerror("WAVE using 16-bit signed samples as is\n");
        }
	}
	w->samples = w->length * 8 / w->resolution;
	logerror("WAVE %d samples - %d:%02d\n", w->samples, (w->samples/w->smpfreq)/60, (w->samples/w->smpfreq)%60);

	return WAVE_OK;
}

static int wave_write(int id)
{
	struct wave_file *w = &wave[id];
	UINT32 filesize, offset = 0, temp32;
	UINT16 temp16;

	if( !w->file )
        return WAVE_ERR;

    while( w->playpos < w->samples )
    {
		*((INT16 *)w->data + w->playpos) = 0;
		w->playpos++;
	}

    filesize =
		4 + 	/* 'RIFF' */
		4 + 	/* size of entire file */
		8 + 	/* 'WAVEfmt ' */
		20 +	/* WAVE tag  (including size -- 0x10 in dword) */
		4 + 	/* 'data' */
		4 + 	/* size of data */
		w->length;

    /* write the core header for a WAVE file */
	offset += osd_fwrite(w->file, "RIFF", 4);
    if( offset < 4 )
    {
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	temp32 = intelLong(filesize) - 8;
	offset += osd_fwrite(w->file, &temp32, 4);

	/* read the RIFF file type and make sure it's a WAVE file */
	offset += osd_fwrite(w->file, "WAVE", 4);
	if( offset < 12 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
	}

	/* write a format tag */
	offset += osd_fwrite(w->file, "fmt ", 4);
    if( offset < 12 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }
    /* size of the following 'fmt ' fields */
    offset += osd_fwrite(w->file, "\x10\x00\x00\x00", 4);
	if( offset < 16 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* format: PCM */
	temp16 = 1;
	offset += osd_fwrite_lsbfirst(w->file, &temp16, 2);
	if( offset < 18 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* channels: 1 (mono) */
	temp16 = 1;
    offset += osd_fwrite_lsbfirst(w->file, &temp16, 2);
	if( offset < 20 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* sample rate */
	temp32 = intelLong(w->smpfreq);
	offset += osd_fwrite(w->file, &temp32, 4);
	if( offset < 24 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* byte rate */
	temp32 = intelLong(w->smpfreq * w->resolution / 8);
	offset += osd_fwrite(w->file, &temp32, 4);
	if( offset < 28 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* block align (size of one `sample') */
	temp16 = w->resolution / 8;
	offset += osd_fwrite_lsbfirst(w->file, &temp16, 2);
	if( offset < 30 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* block align */
	temp16 = w->resolution;
	offset += osd_fwrite_lsbfirst(w->file, &temp16, 2);
	if( offset < 32 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* 'data' tag */
	offset += osd_fwrite(w->file, "data", 4);
	if( offset < 36 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	/* data size */
	temp32 = intelLong(w->length);
	offset += osd_fwrite(w->file, &temp32, 4);
	if( offset < 40 )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	if( osd_fwrite_lsbfirst(w->file, w->data, w->length) != w->length )
	{
		logerror("WAVE write error at offs %d\n", offset);
		return WAVE_ERR;
    }

	return WAVE_OK;
}

static void wave_display(int id)
{
    static int tapepos = 0;
    struct wave_file *w = &wave[id];

    if( abs(w->playpos - tapepos) > w->smpfreq / 4 )
	{
        char buf[32];
		int x, y, n, t0, t1;

        x = Machine->uixmin + id * Machine->uifontwidth * 16 + 1;
		y = Machine->uiymin + Machine->uiheight - 9;
		n = (w->playpos * 4 / w->smpfreq) & 3;
        t0 = w->playpos / w->smpfreq;
		t1 = w->samples / w->smpfreq;
		sprintf(buf, "%c%c %2d:%02d [%2d:%02d]", n*2+2,n*2+3, t0/60,t0%60, t1/60,t1%60);
		ui_text(Machine->scrbitmap,buf, x, y);
		tapepos = w->playpos;
    }
}


static void wave_sound_update(int id, INT16 *buffer, int length)
{
	struct wave_file *w = &wave[id];
	if( !w->timer )
	{
		while( length-- > 0 )
            *buffer++ = 0;
		return;
	}
	/* write mode? */
	if( w->mode )
	{
        if( w->resolution == 16 )
        {
			while( length-- > 0 )
			{
				w->counter -= w->smpfreq;
				while( w->counter <= 0 )
				{
					w->counter += Machine->sample_rate;
					*((INT16 *)w->data + w->playpos) = w->record_sample;
					if( ++w->playpos >= w->samples )
					{
						w->samples += w->smpfreq;
						w->length = w->samples * w->resolution / 8;
						w->data = realloc(w->data, w->length);
						if( !w->data )
						{
							logerror("WAVE realloc(%d) failed\n", w->length);
							timer_remove(w->timer);
							memset(w, 0, sizeof(struct wave_file));
						}
					}
					w->play_sample = w->record_sample;
				}
				*buffer++ = w->mute ? 0 : w->play_sample;
			}
		}
		else
		{
			while( length-- > 0 )
			{
				w->counter -= w->smpfreq;
				while( w->counter <= 0 )
				{
					w->counter += Machine->sample_rate;
					*((INT8 *)w->data + w->playpos) = w->record_sample / 256;
					if( ++w->playpos >= w->samples )
					{
						w->samples += w->smpfreq;
						w->length = w->samples * w->resolution / 8;
						w->data = realloc(w->data, w->length);
						if( !w->data )
						{
							logerror("WAVE realloc(%d) failed\n", w->length);
							timer_remove(w->timer);
							memset(w, 0, sizeof(struct wave_file));
						}
					}
					w->play_sample = w->record_sample;
				}
				*buffer++ = w->mute ? 0 : w->play_sample;
            }
        }
	}
	else
	{
		if( w->resolution == 16 )
		{
			while( length-- > 0 )
			{
				w->counter -= w->smpfreq;
				while( w->counter <= 0 )
				{
					w->counter += Machine->sample_rate;
					if( ++w->playpos >= w->samples )
						w->playpos = w->samples - 1;
					w->play_sample = *((INT16 *)w->data + w->playpos);
				}
				*buffer++ = w->mute ? 0 : w->play_sample;
            }
        }
		else
		{
			while( length-- > 0 )
			{
				w->counter -= w->smpfreq;
				while( w->counter <= 0 )
				{
					w->counter += Machine->sample_rate;
					if( ++w->playpos >= w->samples )
						w->playpos = w->samples - 1;
					w->play_sample = 256 * *((INT8 *)w->data + w->playpos);
				}
				*buffer++ = w->mute ? 0 : w->play_sample;
			}
		}
	}
	if( w->display )
		wave_display(id);
}

/*****************************************************************************
 * WaveSound interface
 *****************************************************************************/

int wave_sh_start(const struct MachineSound *msound)
{
	int i;

    intf = msound->sound_interface;

    for( i = 0; i < intf->num; i++ )
	{
		struct wave_file *w = &wave[i];
		char buf[32];

        if( intf->num > 1 )
			sprintf(buf, "Cassette #%d", i+1);
		else
			strcpy(buf, "Cassette");

        w->channel = stream_init(buf, intf->mixing_level[i], Machine->sample_rate, i, wave_sound_update);

        if( w->channel == -1 )
			return 1;
	}

    return 0;
}

void wave_sh_stop(void)
{
	int i;

    for( i = 0; i < intf->num; i++ )
		wave[i].channel = -1;
}

void wave_sh_update(void)
{
	int i;

	for( i = 0; i < intf->num; i++ )
	{
		if( wave[i].channel != -1 )
			stream_update(wave[i].channel, 0);
	}
}

/*****************************************************************************
 * IODevice interface functions
 *****************************************************************************/

/*
 * return info about a wave device
 */
const void *wave_info(int id, int whatinfo)
{
	return NULL;
}

/*
 * You can use this default handler if you don't want
 * to support your own file types with the fill_wave()
 * extension
 */
int wave_init(int id, const char *name)
{
	void *file;
	if( !name || strlen(name) == 0 )
		return INIT_OK;
	file = osd_fopen(Machine->gamedrv->name, name, OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_READ);
	if( file )
	{
		struct wave_args wa = {0,};
		wa.file = file;
		wa.display = 1;
		if( device_open(IO_CASSETTE,id,0,&wa) )
			return INIT_FAILED;
		return INIT_OK;
    }
	return INIT_FAILED;
}

void wave_exit(int id)
{
	wave_close(id);
}

int wave_status(int id, int newstatus)
{
	/* wave status has the following bitfields:
	 *
	 *	Bit 1:	Mute (1=mute 0=nomute)
	 *	Bit 0:	Motor (1=on 0=off)
	 *
	 *	Also, you can pass -1 to have it simply return the status
	 */
	struct wave_file *w = &wave[id];

	if( !w->file )
		return 0;

    if( newstatus != -1 )
	{
		w->mute = newstatus & 2;
		newstatus &= 1;

		if( newstatus && !w->timer )
		{
			w->timer = timer_set(TIME_NEVER, 0, NULL);
		}
		else
		if( !newstatus && w->timer )
		{
			if( w->timer )
				w->offset += (((float)timer_timeelapsed(w->timer))/(float)TIME_ONE_SEC) * w->smpfreq;
			timer_remove(w->timer);
			w->timer = NULL;
			bitmap_dirty = 1;
		}
	}
	return (w->timer ? 1 : 0) | (w->mute ? 2 : 0);
}

int wave_open(int id, int mode, void *args)
{
	struct wave_file *w = &wave[id];
    struct wave_args *wa = args;
	int result;

    /* wave already opened? */
	if( w->file )
		wave_close(id);

    w->file = wa->file;
	w->mode = mode;
	w->fill_wave = wa->fill_wave;
	w->smpfreq = wa->smpfreq;
	w->display = wa->display;

	if( w->mode )
	{
        w->resolution = 16;
		w->samples = w->smpfreq;
		w->length = w->samples * w->resolution / 8;
		w->data = malloc(w->length);
		if( !w->data )
		{
			logerror("WAVE malloc(%d) failed\n", w->length);
			memset(w, 0, sizeof(struct wave_file));
			return WAVE_ERR;
		}
		return INIT_OK;
    }
	else
	{
		result = wave_read(id);
		if( result == WAVE_OK )
		{
			/* return sample frequency in the user supplied structure */
			wa->smpfreq = w->smpfreq;
			w->offset = 0;
			return INIT_OK;
		}

		if( result == WAVE_FMT )
		{
			UINT8 *data;
			int bytes, pos, length;

			/* User supplied fill_wave function? */
			if( w->fill_wave == NULL )
			{
				logerror("WAVE no fill_wave callback, failing now\n");
				return WAVE_ERR;
			}

			logerror("WAVE creating wave using fill_wave() callback\n");

			/* sanity check: default chunk size is one byte */
			if( wa->chunk_size == 0 )
			{
				wa->chunk_size = 1;
				logerror("WAVE chunk_size defaults to %d\n", wa->chunk_size);
			}
			if( wa->smpfreq == 0 )
			{
				wa->smpfreq = 11025;
				logerror("WAVE smpfreq defaults to %d\n", w->smpfreq);
			}

			/* allocate a buffer for the binary data */
			data = malloc(wa->chunk_size);
			if( !data )
			{
				free(w->data);
				/* zap the wave structure */
				memset(&wave[id], 0, sizeof(struct wave_file));
				return WAVE_ERR;
			}

			/* determine number of samples */
			length =
				wa->header_samples +
				((osd_fsize(w->file) + wa->chunk_size - 1) / wa->chunk_size) * wa->chunk_samples +
				wa->trailer_samples;

			w->smpfreq = wa->smpfreq;
			w->resolution = 16;
			w->samples = length;
			w->length = length * 2;   /* 16 bits per sample */

			w->data = malloc(w->length);
			if( !w->data )
			{
				logerror("WAVE failed to malloc %d bytes\n", w->length);
				/* zap the wave structure */
				memset(&wave[id], 0, sizeof(struct wave_file));
				return WAVE_ERR;
			}
			logerror("WAVE creating max %d:%02d samples (%d) at %d Hz\n", (w->samples/w->smpfreq)/60, (w->samples/w->smpfreq)%60, w->samples, w->smpfreq);

			pos = 0;
			/* if there has to be a header */
			if( wa->header_samples > 0 )
			{
				length = (*w->fill_wave)((INT16 *)w->data + pos, w->samples - pos, CODE_HEADER);
				if( length < 0 )
				{
					logerror("WAVE conversion aborted at header\n");
					free(w->data);
					/* zap the wave structure */
					memset(&wave[id], 0, sizeof(struct wave_file));
					return WAVE_ERR;
				}
				logerror("WAVE header %d samples\n", length);
				pos += length;
			}

			/* convert the file data to samples */
			bytes = 0;
			osd_fseek(w->file, 0, SEEK_SET);
			while( pos < w->samples )
			{
				length = osd_fread(w->file, data, wa->chunk_size);
				if( length == 0 )
					break;
				bytes += length;
				length = (*w->fill_wave)((INT16 *)w->data + pos, w->samples - pos, data);
				if( length < 0 )
				{
					logerror("WAVE conversion aborted at %d bytes (%d samples)\n", bytes, pos);
					free(w->data);
					/* zap the wave structure */
					memset(&wave[id], 0, sizeof(struct wave_file));
					return WAVE_ERR;
				}
				pos += length;
			}
			logerror("WAVE converted %d data bytes to %d samples\n", bytes, pos);

			/* if there has to be a trailer */
			if( wa->trailer_samples )
			{
				if( pos < w->samples )
				{
					length = (*w->fill_wave)((INT16 *)w->data + pos, w->samples - pos, CODE_TRAILER);
					if( length < 0 )
					{
						logerror("WAVE conversion aborted at trailer\n");
						free(w->data);
						/* zap the wave structure */
						memset(&wave[id], 0, sizeof(struct wave_file));
						return WAVE_ERR;
					}
					logerror("WAVE trailer %d samples\n", length);
					pos += length;
				}
			}

			if( pos < w->samples )
			{
				/* what did the fill_wave() calls really fill into the buffer? */
				w->samples = pos;
				w->length = pos * 2;   /* 16 bits per sample */
				w->data = realloc(w->data, w->length);
				/* failure in the last step? how sad... */
				if( !w->data )
				{
					logerror("WAVE realloc(%d) failed\n", w->length);
					/* zap the wave structure */
					memset(&wave[id], 0, sizeof(struct wave_file));
					return WAVE_ERR;
				}
			}
			logerror("WAVE %d samples - %d:%02d\n", w->samples, (w->samples/w->smpfreq)/60, (w->samples/w->smpfreq)%60);
			/* hooray! :-) */
			return INIT_OK;
		}
	}
	return WAVE_ERR;
}

void wave_close(int id)
{
	struct wave_file *w = &wave[id];

    if( !w->file )
		return;

    if( w->timer )
	{
		if( w->channel != -1 )
			stream_update(w->channel, 0);
		w->samples = w->playpos;
		w->length = w->samples * w->resolution / 8;
		timer_remove(w->timer);
		w->timer = NULL;
	}

    if( w->mode )
	{
		wave_output(id,0);
		wave_write(id);
		w->mode = 0;
	}

    if( w->data )
		free(w->data);
    w->data = NULL;

	if (w->file) {
		osd_fclose(w->file);
		w->file = NULL;
	}
	w->offset = 0;
	w->playpos = 0;
	w->counter = 0;
	w->smpfreq = 0;
	w->resolution = 0;
	w->samples = 0;
	w->length = 0;
}

int wave_seek(int id, int offset, int whence)
{
	struct wave_file *w = &wave[id];
    UINT32 pos = 0;

	if( !w->file )
		return pos;

    switch( whence )
	{
	case SEEK_SET:
		w->offset = offset;
		break;
	case SEEK_END:
		w->offset = w->samples - 1;
		break;
	case SEEK_CUR:
		if( w->timer )
			pos = w->offset + ((((float)timer_timeelapsed(w->timer))/(float)TIME_ONE_SEC) * w->smpfreq);
		w->offset = pos + offset;
		if( w->offset < 0 )
			w->offset = 0;
		if( w->offset >= w->length )
			w->offset = w->length - 1;
	}
	w->playpos = w->offset;

    if( w->timer )
	{
		timer_remove(w->timer);
		w->timer = timer_set(TIME_NEVER, 0, NULL);
	}

    return w->offset;
}

int wave_tell(int id)
{
	struct wave_file *w = &wave[id];
    UINT32 pos = 0;
	if( w->timer )
		pos = w->offset + ((((float)timer_timeelapsed(w->timer))/(float)TIME_ONE_SEC) * w->smpfreq);
	if( pos >= w->samples )
		pos = w->samples -1;
    return pos;
}

int wave_input(int id)
{
	struct wave_file *w = &wave[id];
	UINT32 pos = 0;
    int level = 0;

	if( !w->file )
		return level;

    if( w->channel != -1 )
		stream_update(w->channel, 0);

    if( w->timer )
	{
		pos = w->offset + ((((float)timer_timeelapsed(w->timer))/(float)TIME_ONE_SEC) * w->smpfreq);
		if( pos >= w->samples )
			pos = w->samples - 1;
        w->playpos = pos;
		if( w->resolution == 16 )
			level = *((INT16 *)w->data + pos);
		else
			level = 256 * *((INT8 *)w->data + pos);
    }
	if( w->display )
		wave_display(id);
    return level;
}

void wave_output(int id, int data)
{
	struct wave_file *w = &wave[id];
	UINT32 pos = 0;

	if( !w->file )
		return;

    if( !w->mode )
		return;

	if( data == w->record_sample )
		return;

	if( w->channel != -1 )
		stream_update(w->channel, 0);

    if( w->timer )
    {
		pos = w->offset + ((((float)timer_timeelapsed(w->timer))/(float)TIME_ONE_SEC) * w->smpfreq);
        while( pos >= w->samples )
        {
            /* add one second of data */
            w->samples += w->smpfreq;
            w->length = w->samples * w->resolution / 8;
            w->data = realloc(w->data, w->length);
            if( !w->data )
            {
                logerror("WAVE realloc(%d) failed\n", w->length);
                memset(w, 0, sizeof(struct wave_file));
                return;
            }
        }
        while( w->playpos < pos )
        {
            *((INT16 *)w->data + w->playpos) = w->record_sample;
            w->playpos++;
        }
    }

    if( w->display )
        wave_display(id);

    w->record_sample = data;
}

int wave_input_chunk(int id, void *dst, int count)
{
	struct wave_file *w = &wave[id];
	UINT32 pos = 0;

	if( !w->file )
		return 0;

    if( w->timer )
	{
        pos = w->offset + ((((float)timer_timeelapsed(w->timer))/(float)TIME_ONE_SEC) * w->smpfreq);
		if( pos >= w->samples )
			pos = w->samples - 1;
	}

    if( pos + count >= w->samples )
		count = w->samples - pos - 1;

    if( count > 0 )
	{
		if( w->resolution == 16 )
			memcpy(dst, (INT16 *)w->data + pos, count * sizeof(INT16));
		else
			memcpy(dst, (INT8 *)w->data + pos, count * sizeof(INT8));
	}

    return count;
}

int wave_output_chunk(int id, void *src, int count)
{
	struct wave_file *w = &wave[id];
	UINT32 pos = 0;

	if( !w->file )
		return 0;

    if( w->timer )
	{
        pos = w->offset + ((((float)timer_timeelapsed(w->timer))/(float)TIME_ONE_SEC) * w->smpfreq);
		if( pos >= w->length )
			pos = w->length - 1;
	}

    if( pos + count >= w->length )
	{
		/* add space for new data */
		w->samples += count - pos;
		w->length = w->samples * w->resolution / 8;
		w->data = realloc(w->data, w->length);
		if( !w->data )
		{
			logerror("WAVE realloc(%d) failed\n", w->length);
			memset(w, 0, sizeof(struct wave_file));
			return 0;
		}
	}

    if( count > 0 )
	{
		if( w->resolution == 16 )
			memcpy((INT16 *)w->data + pos, src, count * sizeof(INT16));
		else
			memcpy((INT8 *)w->data + pos, src, count * sizeof(INT8));
	}

    return count;
}


