/**********************************************************************************************
 *
 *   streaming ADPCM driver
 *   by Aaron Giles
 *
 *   Library to transcode from an ADPCM source to raw PCM.
 *   Written by Buffoni Mirko in 08/06/97
 *   References: various sources and documents.
 *
 *	 HJB 08/31/98
 *	 modified to use an automatically selected oversampling factor
 *	 for the current Machine->sample_rate
 *
 *   Mish 21/7/99
 *   Updated to allow multiple OKI chips with different sample rates
 *
 **********************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "driver.h"
#include "adpcm.h"


#define MAX_SAMPLE_CHUNK	10000

#define FRAC_BITS			14
#define FRAC_ONE			(1 << FRAC_BITS)
#define FRAC_MASK			(FRAC_ONE - 1)


/* struct describing a single playing ADPCM voice */
struct ADPCMVoice
{
	int stream;				/* which stream are we playing on? */
	UINT8 playing;			/* 1 if we are actively playing */

	UINT8 *region_base;		/* pointer to the base of the region */
	UINT8 *base;			/* pointer to the base memory location */
	UINT32 sample;			/* current sample number */
	UINT32 count;			/* total samples to play */

	UINT32 signal;			/* current ADPCM signal */
	UINT32 step;			/* current ADPCM step */
	UINT32 volume;			/* output volume */

	INT16 last_sample;		/* last sample output */
	INT16 curr_sample;		/* current sample target */
	UINT32 source_step;		/* step value for frequency conversion */
	UINT32 source_pos;		/* current fractional position */
};

/* array of ADPCM voices */
static UINT8 num_voices;
static struct ADPCMVoice adpcm[MAX_ADPCM];

/* global pointer to the current array of samples */
static struct ADPCMsample *sample_list;

/* step size index shift table */
static int index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

/* lookup table for the precomputed difference */
static int diff_lookup[49*16];

/* volume lookup table */
static UINT32 volume_table[16];



/**********************************************************************************************

     compute_tables -- compute the difference tables

***********************************************************************************************/

static void compute_tables(void)
{
	/* nibble to bit map */
	static int nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	int step, nib;

	/* loop over all possible steps */
	for (step = 0; step <= 48; step++)
	{
		/* compute the step value */
		int stepval = floor(16.0 * pow(11.0 / 10.0, (float)step));

		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
				 stepval/2 * nbl2bit[nib][2] +
				 stepval/4 * nbl2bit[nib][3] +
				 stepval/8);
		}
	}

	/* generate the OKI6295 volume table */
	for (step = 0; step < 16; step++)
	{
		float out = 256.0;
		int vol = step;

		/* 3dB per step */
		while (vol-- > 0)
			out /= 1.412537545;	/* = 10 ^ (3/20) = 3dB */
		volume_table[step] = (UINT32)out;
	}
}



/**********************************************************************************************

     generate_adpcm -- general ADPCM decoding routine

***********************************************************************************************/

static void generate_adpcm(struct ADPCMVoice *voice, INT16 *buffer, int samples)
{
	/* if this voice is active */
	if (voice->playing)
	{
		UINT8 *base = voice->base;
		int sample = voice->sample;
		int signal = voice->signal;
		int count = voice->count;
		int step = voice->step;
		int val;

		/* loop while we still have samples to generate */
		while (samples)
		{
			/* compute the new amplitude and update the current step */
			val = base[sample / 2] >> (((sample & 1) << 2) ^ 4);
			signal += diff_lookup[step * 16 + (val & 15)];

			/* clamp to the maximum */
			if (signal > 2047)
				signal = 2047;
			else if (signal < -2048)
				signal = -2048;

			/* adjust the step size and clamp */
			step += index_shift[val & 7];
			if (step > 48)
				step = 48;
			else if (step < 0)
				step = 0;

			/* output to the buffer, scaling by the volume */
			*buffer++ = signal * voice->volume / 16;
			samples--;

			/* next! */
			if (++sample > count)
			{
				voice->playing = 0;
				break;
			}
		}

		/* update the parameters */
		voice->sample = sample;
		voice->signal = signal;
		voice->step = step;
	}

	/* fill the rest with silence */
	while (samples--)
		*buffer++ = 0;
}



/**********************************************************************************************

     adpcm_update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

static void adpcm_update(int num, INT16 *buffer, int length)
{
	struct ADPCMVoice *voice = &adpcm[num];
	INT16 sample_data[MAX_SAMPLE_CHUNK], *curr_data = sample_data;
	INT16 prev = voice->last_sample, curr = voice->curr_sample;
	UINT32 final_pos;
	UINT32 new_samples;

	/* finish off the current sample */
	if (voice->source_pos > 0)
	{
		/* interpolate */
		while (length > 0 && voice->source_pos < FRAC_ONE)
		{
			*buffer++ = (((INT32)prev * (FRAC_ONE - voice->source_pos)) + ((INT32)curr * voice->source_pos)) >> FRAC_BITS;
			voice->source_pos += voice->source_step;
			length--;
		}

		/* if we're over, continue; otherwise, we're done */
		if (voice->source_pos >= FRAC_ONE)
			voice->source_pos -= FRAC_ONE;
		else
			return;
	}

	/* compute how many new samples we need */
	final_pos = voice->source_pos + length * voice->source_step;
	new_samples = (final_pos + FRAC_ONE - 1) >> FRAC_BITS;
	if (new_samples > MAX_SAMPLE_CHUNK)
		new_samples = MAX_SAMPLE_CHUNK;

	/* generate them into our buffer */
	generate_adpcm(voice, sample_data, new_samples);
	prev = curr;
	curr = *curr_data++;

	/* then sample-rate convert with linear interpolation */
	while (length > 0)
	{
		/* interpolate */
		while (length > 0 && voice->source_pos < FRAC_ONE)
		{
			*buffer++ = (((INT32)prev * (FRAC_ONE - voice->source_pos)) + ((INT32)curr * voice->source_pos)) >> FRAC_BITS;
			voice->source_pos += voice->source_step;
			length--;
		}

		/* if we're over, grab the next samples */
		if (voice->source_pos >= FRAC_ONE)
		{
			voice->source_pos -= FRAC_ONE;
			prev = curr;
			curr = *curr_data++;
		}
	}

	/* remember the last samples */
	voice->last_sample = prev;
	voice->curr_sample = curr;
}



/**********************************************************************************************

     ADPCM_sh_start -- start emulation of several ADPCM output streams

***********************************************************************************************/

int ADPCM_sh_start(const struct MachineSound *msound)
{
	const struct ADPCMinterface *intf = (const struct ADPCMinterface *)msound->sound_interface;
	char stream_name[40];
	int i;

	/* reset the ADPCM system */
	num_voices = intf->num;
	compute_tables();
	sample_list = 0;

	/* generate the sample table, if one is needed */
	if (intf->init)
	{
		/* allocate memory for it */
		sample_list = (struct ADPCMsample*)malloc(257 * sizeof(struct ADPCMsample));
		if (!sample_list)
			return 1;
		memset(sample_list, 0, 257 * sizeof(struct ADPCMsample));

		/* callback to initialize */
		(*intf->init)(intf, sample_list, 256);
	}

	/* initialize the voices */
	memset(adpcm, 0, sizeof(adpcm));
	for (i = 0; i < num_voices; i++)
	{
		/* generate the name and create the stream */
		sprintf(stream_name, "%s #%d", sound_name(msound), i);
		adpcm[i].stream = stream_init(stream_name, intf->mixing_level[i], Machine->sample_rate, i, adpcm_update);
		if (adpcm[i].stream == -1)
			return 1;

		/* initialize the rest of the structure */
		adpcm[i].region_base = memory_region(intf->region);
		adpcm[i].volume = 255;
		adpcm[i].signal = -2;
		if (Machine->sample_rate)
			adpcm[i].source_step = (UINT32)((float)intf->frequency * (float)FRAC_ONE / (float)Machine->sample_rate);
	}

	/* success */
	return 0;
}



/**********************************************************************************************

     ADPCM_sh_stop -- stop emulation of several ADPCM output streams

***********************************************************************************************/

void ADPCM_sh_stop(void)
{
	/* free the temporary table if we created it */
	if (sample_list)
	{
		free(sample_list);
		sample_list = 0;
	}
}



/**********************************************************************************************

     ADPCM_sh_update -- update ADPCM streams

***********************************************************************************************/

void ADPCM_sh_update(void)
{
}



/**********************************************************************************************

     ADPCM_trigger -- handle a write to the ADPCM data stream

***********************************************************************************************/

void ADPCM_trigger(int num, int which)
{
	struct ADPCMVoice *voice = &adpcm[num];
	struct ADPCMsample *sample;

	/* bail if we're not playing anything */
	if (Machine->sample_rate == 0)
		return;

	/* range check the numbers */
	if (num >= num_voices)
	{
		logerror("error: ADPCM_trigger() called with channel = %d, but only %d channels allocated\n", num, num_voices);
		return;
	}

	/* find a match */
	for (sample = sample_list; sample->length > 0; sample++)
		if (sample->num == which)
		{
			/* update the ADPCM voice */
			stream_update(voice->stream, 0);

			/* set up the voice to play this sample */
			voice->playing = 1;
			voice->base = &voice->region_base[sample->offset];
			voice->sample = 0;
			voice->count = sample->length;

			/* also reset the ADPCM parameters */
			voice->signal = -2;
			voice->step = 0;
			return;
		}

	logerror("warning: ADPCM_trigger() called with unknown trigger = %08x\n",which);
}



/**********************************************************************************************

     ADPCM_play -- play data from a specific offset for a specific length

***********************************************************************************************/

void ADPCM_play(int num, int offset, int length)
{
	struct ADPCMVoice *voice = &adpcm[num];

	/* bail if we're not playing anything */
	if (Machine->sample_rate == 0)
		return;

	/* range check the numbers */
	if (num >= num_voices)
	{
		logerror("error: ADPCM_trigger() called with channel = %d, but only %d channels allocated\n", num, num_voices);
		return;
	}

	/* update the ADPCM voice */
	stream_update(voice->stream, 0);

	/* set up the voice to play this sample */
	voice->playing = 1;
	voice->base = &voice->region_base[offset];
	voice->sample = 0;
	voice->count = length;

	/* also reset the ADPCM parameters */
	voice->signal = -2;
	voice->step = 0;
}



/**********************************************************************************************

     ADPCM_play -- stop playback on an ADPCM data channel

***********************************************************************************************/

void ADPCM_stop(int num)
{
	struct ADPCMVoice *voice = &adpcm[num];

	/* bail if we're not playing anything */
	if (Machine->sample_rate == 0)
		return;

	/* range check the numbers */
	if (num >= num_voices)
	{
		logerror("error: ADPCM_stop() called with channel = %d, but only %d channels allocated\n", num, num_voices);
		return;
	}

	/* update the ADPCM voice */
	stream_update(voice->stream, 0);

	/* stop playback */
	voice->playing = 0;
}



/**********************************************************************************************

     ADPCM_setvol -- change volume on an ADPCM data channel

***********************************************************************************************/

void ADPCM_setvol(int num, int vol)
{
	struct ADPCMVoice *voice = &adpcm[num];

	/* bail if we're not playing anything */
	if (Machine->sample_rate == 0)
		return;

	/* range check the numbers */
	if (num >= num_voices)
	{
		logerror("error: ADPCM_setvol() called with channel = %d, but only %d channels allocated\n", num, num_voices);
		return;
	}

	/* update the ADPCM voice */
	stream_update(voice->stream, 0);
	voice->volume = vol;
}



/**********************************************************************************************

     ADPCM_playing -- returns true if an ADPCM data channel is still playing

***********************************************************************************************/

int ADPCM_playing(int num)
{
	struct ADPCMVoice *voice = &adpcm[num];

	/* bail if we're not playing anything */
	if (Machine->sample_rate == 0)
		return 0;

	/* range check the numbers */
	if (num >= num_voices)
	{
		logerror("error: ADPCM_playing() called with channel = %d, but only %d channels allocated\n", num, num_voices);
		return 0;
	}

	/* update the ADPCM voice */
	stream_update(voice->stream, 0);
	return voice->playing;
}



/**********************************************************************************************
 *
 *	OKIM 6295 ADPCM chip:
 *
 *	Command bytes are sent:
 *
 *		1xxx xxxx = start of 2-byte command sequence, xxxxxxx is the sample number to trigger
 *		abcd vvvv = second half of command; one of the abcd bits is set to indicate which voice
 *		            the v bits seem to be volumed
 *
 *		0abc d000 = stop playing; one or more of the abcd bits is set to indicate which voice(s)
 *
 *	Status is read:
 *
 *		???? abcd = one bit per voice, set to 0 if nothing is playing, or 1 if it is active
 *
***********************************************************************************************/

static int okim6295_command[MAX_OKIM6295];
static int okim6295_base[MAX_OKIM6295][MAX_OKIM6295_VOICES];


/**********************************************************************************************

     OKIM6295_sh_start -- start emulation of an OKIM6295-compatible chip

***********************************************************************************************/

int OKIM6295_sh_start(const struct MachineSound *msound)
{
	const struct OKIM6295interface *intf = (const struct OKIM6295interface *)msound->sound_interface;
	char stream_name[40];
	int i;

	/* reset the ADPCM system */
	num_voices = intf->num * MAX_OKIM6295_VOICES;
	compute_tables();
	sample_list = 0;

	/* initialize the voices */
	memset(adpcm, 0, sizeof(adpcm));
	for (i = 0; i < num_voices; i++)
	{
		int chip = i / MAX_OKIM6295_VOICES;
		int voice = i % MAX_OKIM6295_VOICES;

		/* reset the OKI-specific parameters */
		okim6295_command[chip] = -1;
		okim6295_base[chip][voice] = 0;

		/* generate the name and create the stream */
		sprintf(stream_name, "%s #%d (voice %d)", sound_name(msound), chip, voice);
		adpcm[i].stream = stream_init(stream_name, intf->mixing_level[chip], Machine->sample_rate, i, adpcm_update);
		if (adpcm[i].stream == -1)
			return 1;

		/* initialize the rest of the structure */
		adpcm[i].region_base = memory_region(intf->region[chip]);
		adpcm[i].volume = 255;
		adpcm[i].signal = -2;
		if (Machine->sample_rate)
			adpcm[i].source_step = (UINT32)((float)intf->frequency[chip] * (float)FRAC_ONE / (float)Machine->sample_rate);
	}

	/* success */
	return 0;
}



/**********************************************************************************************

     OKIM6295_sh_stop -- stop emulation of an OKIM6295-compatible chip

***********************************************************************************************/

void OKIM6295_sh_stop(void)
{
}



/**********************************************************************************************

     OKIM6295_sh_update -- update emulation of an OKIM6295-compatible chip

***********************************************************************************************/

void OKIM6295_sh_update(void)
{
}



/**********************************************************************************************

     OKIM6295_set_bank_base -- set the base of the bank for a given voice on a given chip

***********************************************************************************************/

void OKIM6295_set_bank_base(int which, int channel, int base)
{
	struct ADPCMVoice *voice = &adpcm[which * MAX_OKIM6295_VOICES + channel];

	/* handle the all voice case */
	if (channel == ALL_VOICES)
	{
		int i;

		for (i = 0; i < MAX_OKIM6295_VOICES; i++)
			OKIM6295_set_bank_base(which, i, base);
		return;
	}

	/* update the stream and set the new base */
	stream_update(voice->stream, 0);
	okim6295_base[which][channel] = base;
}



/**********************************************************************************************

     OKIM6295_set_frequency -- dynamically adjusts the frequency of a given ADPCM voice

***********************************************************************************************/

void OKIM6295_set_frequency(int which, int channel, int frequency)
{
	struct ADPCMVoice *voice = &adpcm[which * MAX_OKIM6295_VOICES + channel];

	/* handle the all voice case */
	if (channel == ALL_VOICES)
	{
		int i;

		for (i = 0; i < MAX_OKIM6295_VOICES; i++)
			OKIM6295_set_frequency(which, i, frequency);
		return;
	}

	/* update the stream and set the new base */
	stream_update(voice->stream, 0);
	if (Machine->sample_rate)
		voice->source_step = (UINT32)((float)frequency * (float)FRAC_ONE / (float)Machine->sample_rate);
}


/**********************************************************************************************

     OKIM6295_status_r -- read the status port of an OKIM6295-compatible chip

***********************************************************************************************/

static int OKIM6295_status_r(int num)
{
	int i, result;

	/* range check the numbers */
	if (num >= num_voices / MAX_OKIM6295_VOICES)
	{
		logerror("error: OKIM6295_status_r() called with chip = %d, but only %d chips allocated\n",num, num_voices / MAX_OKIM6295_VOICES);
		return 0x0f;
	}

	/* set the bit to 1 if something is playing on a given channel */
	result = 0;
	for (i = 0; i < MAX_OKIM6295_VOICES; i++)
	{
		struct ADPCMVoice *voice = &adpcm[num * MAX_OKIM6295_VOICES + i];

		/* update the stream */
		stream_update(voice->stream, 0);

		/* set the bit if it's playing */
		if (voice->playing)
			result |= 1 << i;
	}

	return result;
}



/**********************************************************************************************

     OKIM6295_data_w -- write to the data port of an OKIM6295-compatible chip

***********************************************************************************************/

static void OKIM6295_data_w(int num, int data)
{
	/* range check the numbers */
	if (num >= num_voices / MAX_OKIM6295_VOICES)
	{
		logerror("error: OKIM6295_data_w() called with chip = %d, but only %d chips allocated\n", num, num_voices / MAX_OKIM6295_VOICES);
		return;
	}

	/* if a command is pending, process the second half */
	if (okim6295_command[num] != -1)
	{
		int temp = data >> 4, i, start, stop;
		unsigned char *base;

		/* determine which voice(s) (voice is set by a 1 bit in the upper 4 bits of the second byte) */
		for (i = 0; i < MAX_OKIM6295_VOICES; i++, temp >>= 1)
			if (temp & 1)
			{
				struct ADPCMVoice *voice = &adpcm[num * MAX_OKIM6295_VOICES + i];

				/* update the stream */
				stream_update(voice->stream, 0);

				/* determine the start/stop positions */
				base = &voice->region_base[okim6295_base[num][i] + okim6295_command[num] * 8];
				start = (base[0] << 16) + (base[1] << 8) + base[2];
				stop = (base[3] << 16) + (base[4] << 8) + base[5];

				/* set up the voice to play this sample */
				if (start < 0x40000 && stop < 0x40000)
				{
					voice->playing = 1;
					voice->base = &voice->region_base[okim6295_base[num][i] + start];
					voice->sample = 0;
					voice->count = 2 * (stop - start + 1);

					/* also reset the ADPCM parameters */
					voice->signal = -2;
					voice->step = 0;
					voice->volume = volume_table[data & 0x0f];
				}

				/* invalid samples go here */
				else
				{
					logerror("OKIM6295: requested to play invalid sample %02x\n",okim6295_command[num]);
					voice->playing = 0;
				}
			}

		/* reset the command */
		okim6295_command[num] = -1;
	}

	/* if this is the start of a command, remember the sample number for next time */
	else if (data & 0x80)
	{
		okim6295_command[num] = data & 0x7f;
	}

	/* otherwise, see if this is a silence command */
	else
	{
		int temp = data >> 3, i;

		/* determine which voice(s) (voice is set by a 1 bit in bits 3-6 of the command */
		for (i = 0; i < 4; i++, temp >>= 1)
			if (temp & 1)
			{
				struct ADPCMVoice *voice = &adpcm[num * MAX_OKIM6295_VOICES + i];

				/* update the stream, then turn it off */
				stream_update(voice->stream, 0);
				voice->playing = 0;
			}
	}
}



/**********************************************************************************************

     OKIM6295_status_0_r -- generic status read functions
     OKIM6295_status_1_r

***********************************************************************************************/

READ_HANDLER( OKIM6295_status_0_r )
{
	return OKIM6295_status_r(0);
}

READ_HANDLER( OKIM6295_status_1_r )
{
	return OKIM6295_status_r(1);
}



/**********************************************************************************************

     OKIM6295_data_0_w -- generic data write functions
     OKIM6295_data_1_w

***********************************************************************************************/

WRITE_HANDLER( OKIM6295_data_0_w )
{
	OKIM6295_data_w(0, data);
}

WRITE_HANDLER( OKIM6295_data_1_w )
{
	OKIM6295_data_w(1, data);
}
