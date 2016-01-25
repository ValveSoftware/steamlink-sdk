/***************************************************************************

	NAMCO sound driver.

	This driver handles the three known types of NAMCO wavetable sounds:

		- 3-voice mono (Pac-Man, Pengo, Dig Dug, etc)
		- 8-voice mono (Mappy, Dig Dug 2, etc)
		- 8-voice stereo (System 1)
		- 6-voice stereo (Pole Position 1, Pole Position 2)

***************************************************************************/

#include "driver.h"


/* 8 voices max */
#define MAX_VOICES 8


/* this structure defines the parameters for a channel */
typedef struct
{
	int frequency;
	int counter;
	int volume[2];
	int noise_sw;
	int noise_state;
	int noise_seed;
	int noise_counter;
	const unsigned char *wave;
} sound_channel;


/* globals available to everyone */
unsigned char *namco_soundregs;
unsigned char *namco_wavedata;

/* data about the sound system */
static sound_channel channel_list[MAX_VOICES];
static sound_channel *last_channel;

/* global sound parameters */
static const unsigned char *sound_prom;
static int samples_per_byte;
static int num_voices;
static int sound_enable;
static int stream;
static int namco_clock;
static int sample_rate;

/* mixer tables and internal buffers */
static INT16 *mixer_table;
static INT16 *mixer_lookup;
static short *mixer_buffer;
static short *mixer_buffer_2;



/* build a table to divide by the number of voices */
static int make_mixer_table(int voices)
{
	int count = voices * 128;
	int i;
	int gain = 16;


	/* allocate memory */
	mixer_table = (INT16*)malloc(256 * voices * sizeof(INT16));
	if (!mixer_table)
		return 1;

	/* find the middle of the table */
	mixer_lookup = mixer_table + (128 * voices);

	/* fill in the table - 16 bit case */
	for (i = 0; i < count; i++)
	{
		int val = i * gain * 16 / voices;
		if (val > 32767) val = 32767;
		mixer_lookup[ i] = val;
		mixer_lookup[-i] = -val;
	}

	return 0;
}


/* generate sound to the mix buffer in mono */
static void namco_update_mono(int ch, INT16 *buffer, int length)
{
	sound_channel *voice;
	short *mix;
	int i;

	/* if no sound, we're done */
	if (sound_enable == 0)
	{
		memset(buffer, 0, length * sizeof(INT16));
		return;
	}

	/* zap the contents of the mixer buffer */
	memset(mixer_buffer, 0, length * sizeof(short));

	/* loop over each voice and add its contribution */
	for (voice = channel_list; voice < last_channel; voice++)
	{
		int f = voice->frequency;
		int v = voice->volume[0];

		mix = mixer_buffer;

		if (voice->noise_sw)
		{
			/* only update if we have non-zero volume and frequency */
			if (v && (f & 0xff))
			{
				float fbase = (float)sample_rate / (float)namco_clock;
				int delta = (float)((f & 0xff) << 4) * fbase;
				int c = voice->noise_counter;

				/* add our contribution */
				for (i = 0; i < length; i++)
				{
					int noise_data;
					int cnt;

					if (voice->noise_state)	noise_data = 0x07;
					else noise_data = -0x07;
					*mix++ += noise_data * (v >> 1);

					c += delta;
					cnt = (c >> 12);
					c &= (1 << 12) - 1;
					for( ;cnt > 0; cnt--)
					{
						if ((voice->noise_seed + 1) & 2) voice->noise_state ^= 1;
						if (voice->noise_seed & 1) voice->noise_seed ^= 0x28000;
						voice->noise_seed >>= 1;
					}
				}

				/* update the counter for this voice */
				voice->noise_counter = c;
			}
		}
		else
		{
			/* only update if we have non-zero volume and frequency */
			if (v && f)
			{
				const unsigned char *w = voice->wave;
				int c = voice->counter;

				/* add our contribution */
				for (i = 0; i < length; i++)
				{
					int offs;

					c += f;
					offs = (c >> 15) & 0x1f;
					if (samples_per_byte == 1)	/* use only low 4 bits */
						*mix++ += ((w[offs] & 0x0f) - 8) * v;
					else	/* use full byte, first the high 4 bits, then the low 4 bits */
					{
						if (offs & 1)
							*mix++ += ((w[offs>>1] & 0x0f) - 8) * v;
						else
							*mix++ += (((w[offs>>1]>>4) & 0x0f) - 8) * v;
					}
				}

				/* update the counter for this voice */
				voice->counter = c;
			}
		}
	}

	/* mix it down */
	mix = mixer_buffer;
	for (i = 0; i < length; i++)
		*buffer++ = mixer_lookup[*mix++];
}


/* generate sound to the mix buffer in stereo */
static void namco_update_stereo(int ch, INT16 **buffer, int length)
{
	sound_channel *voice;
	short *lmix, *rmix;
	int i;

	/* if no sound, we're done */
	if (sound_enable == 0)
	{
		memset(buffer[0], 0, length * sizeof(INT16));
		memset(buffer[1], 0, length * sizeof(INT16));
		return;
	}

	/* zap the contents of the mixer buffer */
	memset(mixer_buffer, 0, length * sizeof(INT16));
	memset(mixer_buffer_2, 0, length * sizeof(INT16));

	/* loop over each voice and add its contribution */
	for (voice = channel_list; voice < last_channel; voice++)
	{
		int f = voice->frequency;
		int lv = voice->volume[0];
		int rv = voice->volume[1];

		lmix = mixer_buffer;
		rmix = mixer_buffer_2;

		if (voice->noise_sw)
		{
			/* only update if we have non-zero volume and frequency */
			if ((lv || rv) && (f & 0xff))
			{
				float fbase = (float)sample_rate / (float)namco_clock;
				int delta = (float)((f & 0xff) << 4) * fbase;
				int c = voice->noise_counter;

				/* add our contribution */
				for (i = 0; i < length; i++)
				{
					int noise_data;
					int cnt;

					if (voice->noise_state)	noise_data = 0x07;
					else noise_data = -0x07;
					*lmix++ += noise_data * (lv >> 1);
					*rmix++ += noise_data * (rv >> 1);

					c += delta;
					cnt = (c >> 12);
					c &= (1 << 12) - 1;
					for( ;cnt > 0; cnt--)
					{
						if ((voice->noise_seed + 1) & 2) voice->noise_state ^= 1;
						if (voice->noise_seed & 1) voice->noise_seed ^= 0x28000;
						voice->noise_seed >>= 1;
					}
				}

				/* update the counter for this voice */
				voice->noise_counter = c;
			}
		}
		else
		{
			/* only update if we have non-zero volume and frequency */
			if ((lv || rv) && f)
			{
				const unsigned char *w = voice->wave;
				int c = voice->counter;

				/* add our contribution */
				for (i = 0; i < length; i++)
				{
					int offs;

					c += f;
					offs = (c >> 15) & 0x1f;
					if (samples_per_byte == 1)	/* use only low 4 bits */
					{
						*lmix++ += ((w[offs] & 0x0f) - 8) * lv;
						*rmix++ += ((w[offs] & 0x0f) - 8) * rv;
					}
					else	/* use full byte, first the high 4 bits, then the low 4 bits */
					{
						if (offs & 1)
						{
							*lmix++ += ((w[offs>>1] & 0x0f) - 8) * lv;
							*rmix++ += ((w[offs>>1] & 0x0f) - 8) * rv;
						}
						else
						{
							*lmix++ += (((w[offs>>1]>>4) & 0x0f) - 8) * lv;
							*rmix++ += (((w[offs>>1]>>4) & 0x0f) - 8) * rv;
						}
					}
				}

				/* update the counter for this voice */
				voice->counter = c;
			}
		}
	}

	/* mix it down */
	lmix = mixer_buffer;
	rmix = mixer_buffer_2;
	{
		INT16 *dest1 = buffer[0];
		INT16 *dest2 = buffer[1];
		for (i = 0; i < length; i++)
		{
			*dest1++ = mixer_lookup[*lmix++];
			*dest2++ = mixer_lookup[*rmix++];
		}
	}
}


int namco_sh_start(const struct MachineSound *msound)
{
	const char *mono_name = "NAMCO sound";
	const char *stereo_names[] =
	{
		"NAMCO sound left",
		"NAMCO sound right"
	};
	sound_channel *voice;
	const struct namco_interface *intf = (const struct namco_interface *)msound->sound_interface;

	namco_clock = intf->samplerate;
	sample_rate = Machine->sample_rate;

	/* get stream channels */
	if (intf->stereo)
	{
		int vol[2];

		vol[0] = MIXER(intf->volume,MIXER_PAN_LEFT);
		vol[1] = MIXER(intf->volume,MIXER_PAN_RIGHT);
		stream = stream_init_multi(2, stereo_names, vol, intf->samplerate, 0, namco_update_stereo);
	}
	else
	{
		stream = stream_init(mono_name, intf->volume, intf->samplerate, 0, namco_update_mono);
	}

	/* allocate a pair of buffers to mix into - 1 second's worth should be more than enough */
	if ((mixer_buffer = (short int*)malloc(2 * sizeof(short) * intf->samplerate)) == 0)
		return 1;
	mixer_buffer_2 = mixer_buffer + intf->samplerate;

	/* build the mixer table */
	if (make_mixer_table(intf->voices))
	{
		free(mixer_buffer);
		return 1;
	}

	/* extract globals from the interface */
	num_voices = intf->voices;
	last_channel = channel_list + num_voices;

	if (intf->region == -1)
	{
		sound_prom = namco_wavedata;
		samples_per_byte = 2;	/* first 4 high bits, then low 4 bits */
	}
	else
	{
		sound_prom = memory_region(intf->region);
		samples_per_byte = 1;	/* use only low 4 bits */
	}

	/* start with sound enabled, many games don't have a sound enable register */
	sound_enable = 1;

	/* reset all the voices */
	for (voice = channel_list; voice < last_channel; voice++)
	{
		voice->frequency = 0;
		voice->volume[0] = voice->volume[1] = 0;
		voice->wave = &sound_prom[0];
		voice->counter = 0;
		voice->noise_sw = 0;
		voice->noise_state = 0;
		voice->noise_seed = 1;
		voice->noise_counter = 0;
	}

	return 0;
}


void namco_sh_stop(void)
{
	free(mixer_table);
	free(mixer_buffer);
}


/********************************************************************************/


WRITE_HANDLER( pengo_sound_enable_w )
{
	sound_enable = data;
}

WRITE_HANDLER( pengo_sound_w )
{
	sound_channel *voice;
	int base;

	/* update the streams */
	stream_update(stream, 0);

	/* set the register */
	namco_soundregs[offset] = data & 0x0f;

	/* recompute all the voice parameters */
	for (base = 0, voice = channel_list; voice < last_channel; voice++, base += 5)
	{
		voice->frequency = namco_soundregs[0x14 + base];	/* always 0 */
		voice->frequency = voice->frequency * 16 + namco_soundregs[0x13 + base];
		voice->frequency = voice->frequency * 16 + namco_soundregs[0x12 + base];
		voice->frequency = voice->frequency * 16 + namco_soundregs[0x11 + base];
		if (base == 0)	/* the first voice has extra frequency bits */
			voice->frequency = voice->frequency * 16 + namco_soundregs[0x10 + base];
		else
			voice->frequency = voice->frequency * 16;

		voice->volume[0] = namco_soundregs[0x15 + base] & 0x0f;
		voice->wave = &sound_prom[32 * (namco_soundregs[0x05 + base] & 7)];
	}
}


/********************************************************************************/

WRITE_HANDLER( polepos_sound_w )
{
	sound_channel *voice;
	int base;

	/* update the streams */
	stream_update(stream, 0);

	/* set the register */
	namco_soundregs[offset] = data;

	/* recompute all the voice parameters */
	for (base = 8, voice = channel_list; voice < last_channel; voice++, base += 4)
	{
		voice->frequency = namco_soundregs[0x01 + base];
		voice->frequency = voice->frequency * 256 + namco_soundregs[0x00 + base];

		/* the volume seems to vary between one of these five places */
		/* it's likely that only 3 or 4 are valid; for now, we just */
		/* take the maximum volume and that seems to do the trick */
		/* volume[0] = left speaker ?, volume[1] = right speaker ? */
		voice->volume[0] = voice->volume[1] = 0;
		// front speaker ?
		voice->volume[1] |= namco_soundregs[0x02 + base] & 0x0f;
		voice->volume[0] |= namco_soundregs[0x02 + base] >> 4;
		// rear speaker ?
		voice->volume[1] |= namco_soundregs[0x03 + base] & 0x0f;
		voice->volume[0] |= namco_soundregs[0x03 + base] >> 4;
		voice->volume[1] |= namco_soundregs[0x23 + base] >> 4;
		voice->wave = &sound_prom[32 * (namco_soundregs[0x23 + base] & 7)];
	}
}


/********************************************************************************/

WRITE_HANDLER( mappy_sound_enable_w )
{
	sound_enable = offset;
}

WRITE_HANDLER( mappy_sound_w )
{
	sound_channel *voice;
	int base;

	/* update the streams */
	stream_update(stream, 0);

	/* set the register */
	namco_soundregs[offset] = data;

	/* recompute all the voice parameters */
	for (base = 0, voice = channel_list; voice < last_channel; voice++, base += 8)
	{
		voice->frequency = namco_soundregs[0x06 + base] & 15;	/* high bits are from here */
		voice->frequency = voice->frequency * 256 + namco_soundregs[0x05 + base];
		voice->frequency = voice->frequency * 256 + namco_soundregs[0x04 + base];

		voice->volume[0] = namco_soundregs[0x03 + base] & 0x0f;
		voice->wave = &sound_prom[32 * ((namco_soundregs[0x06 + base] >> 4) & 7)];
	}
}


/********************************************************************************/

WRITE_HANDLER( namcos1_sound_w )
{
	sound_channel *voice;
	int base;
	static int nssw;

	/* verify the offset */
	if (offset > 63)
	{
		//logerror("NAMCOS1 sound: Attempting to write past the 64 registers segment\n");
		return;
	}

	/* update the streams */
	stream_update(stream,0);

	/* set the register */
	namco_soundregs[offset] = data;

	/* recompute all the voice parameters */
	for (base = 0, voice = channel_list; voice < last_channel; voice++, base += 8)
	{
		voice->frequency = namco_soundregs[0x01 + base] & 15;	/* high bits are from here */
		voice->frequency = voice->frequency * 256 + namco_soundregs[0x02 + base];
		voice->frequency = voice->frequency * 256 + namco_soundregs[0x03 + base];

		voice->volume[0] = namco_soundregs[0x00 + base] & 0x0f;
		voice->volume[1] = namco_soundregs[0x04 + base] & 0x0f;
		voice->wave = &sound_prom[32/samples_per_byte * ((namco_soundregs[0x01 + base] >> 4) & 15)];

		nssw = ((namco_soundregs[0x04 + base] & 0x80) >> 7);
		if ((voice + 1) < last_channel) (voice + 1)->noise_sw = nssw;
	}
	voice = channel_list;
	voice->noise_sw = nssw;
}

READ_HANDLER( namcos1_sound_r )
{
	return namco_soundregs[offset];
}

WRITE_HANDLER( namcos1_wavedata_w )
{
	/* update the streams */
	stream_update(stream,0);

	namco_wavedata[offset] = data;
}

READ_HANDLER( namcos1_wavedata_r )
{
	return namco_wavedata[offset];
}


/********************************************************************************/

WRITE_HANDLER( snkwave_w )
{
	static int freq0 = 0xff;
	sound_channel *voice = channel_list;
	if( offset==0 ) freq0 = data;
	if( offset==1 )
	{
		stream_update(stream, 0);
		if( data==0xff || freq0==0 )
		{
			voice->volume[0] = 0x0;
		}
		else
		{
			voice->volume[0] = 0x8;
			voice->frequency = (data<<16)/freq0;
		}
	}
}

