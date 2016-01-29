/**********************************************************************************************

     TMS5220 interface

     Written for MAME by Frank Palazzolo
     With help from Neill Corlett
     Additional tweaking by Aaron Giles

***********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "driver.h"
#include "tms5220.h"
#include "tms5220.cpp"

#define MAX_SAMPLE_CHUNK	10000

#define FRAC_BITS			14
#define FRAC_ONE			(1 << FRAC_BITS)
#define FRAC_MASK			(FRAC_ONE - 1)


/* the state of the streamed output */
static const struct TMS5220interface *intf;
static INT16 last_sample, curr_sample;
static UINT32 source_step;
static UINT32 source_pos;
static int stream;


/* static function prototypes */
static void tms5220_update(int ch, INT16 *buffer, int length);



/**********************************************************************************************

     tms5220_sh_start -- allocate buffers and reset the 5220

***********************************************************************************************/

int tms5220_sh_start(const struct MachineSound *msound)
{
    intf = (const struct TMS5220interface *)msound->sound_interface;

    /* reset the 5220 */
    tms5220_reset();
    tms5220_set_irq(intf->irq);

    /* set the initial frequency */
    stream = -1;
    tms5220_set_frequency(intf->baseclock);
    source_pos = 0;
    last_sample = curr_sample = 0;

	/* initialize a stream */
	stream = stream_init("TMS5220", intf->mixing_level, Machine->sample_rate, 0, tms5220_update);
	if (stream == -1)
		return 1;

    /* request a sound channel */
    return 0;
}



/**********************************************************************************************

     tms5220_sh_stop -- free buffers

***********************************************************************************************/

void tms5220_sh_stop(void)
{
}



/**********************************************************************************************

     tms5220_sh_update -- update the sound chip

***********************************************************************************************/

void tms5220_sh_update(void)
{
}



/**********************************************************************************************

     tms5220_data_w -- write data to the sound chip

***********************************************************************************************/

WRITE_HANDLER( tms5220_data_w )
{
    /* bring up to date first */
#ifndef MAME_FASTSOUND
    stream_update(stream, 0);
#else
    {
        extern int fast_sound;
        if (!fast_sound) stream_update(stream, 0);
    }
#endif
    tms5220_data_write(data);
}



/**********************************************************************************************

     tms5220_status_r -- read status from the sound chip

***********************************************************************************************/

READ_HANDLER( tms5220_status_r )
{
    /* bring up to date first */
#ifndef MAME_FASTSOUND
    stream_update(stream, -1);
#else
    {
        extern int fast_sound;
        if (!fast_sound) stream_update(stream, -1);
    }
#endif
    return tms5220_status_read();
}



/**********************************************************************************************

     tms5220_ready_r -- return the not ready status from the sound chip

***********************************************************************************************/

int tms5220_ready_r(void)
{
    /* bring up to date first */
#ifndef MAME_FASTSOUND
    stream_update(stream, -1);
#else
    {
        extern int fast_sound;
        if (!fast_sound) stream_update(stream, -1);
    }
#endif
    return tms5220_ready_read();
}



/**********************************************************************************************

     tms5220_int_r -- return the int status from the sound chip

***********************************************************************************************/

int tms5220_int_r(void)
{
    /* bring up to date first */
#ifndef MAME_FASTSOUND
    stream_update(stream, -1);
#else
    {
        extern int fast_sound;
        if (!fast_sound) stream_update(stream, -1);
    }
#endif
    return tms5220_int_read();
}



/**********************************************************************************************

     tms5220_update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

static void tms5220_update(int ch, INT16 *buffer, int length)
{
	INT16 sample_data[MAX_SAMPLE_CHUNK], *curr_data = sample_data;
	INT16 prev = last_sample, curr = curr_sample;
	UINT32 final_pos;
	UINT32 new_samples;

	/* finish off the current sample */
	if (source_pos > 0)
	{
		/* interpolate */
		while (length > 0 && source_pos < FRAC_ONE)
		{
			*buffer++ = (((INT32)prev * (FRAC_ONE - source_pos)) + ((INT32)curr * source_pos)) >> FRAC_BITS;
			source_pos += source_step;
			length--;
		}

		/* if we're over, continue; otherwise, we're done */
		if (source_pos >= FRAC_ONE)
			source_pos -= FRAC_ONE;
		else
		{
			tms5220_process(sample_data, 0);
			return;
		}
	}

	/* compute how many new samples we need */
	final_pos = source_pos + length * source_step;
	new_samples = (final_pos + FRAC_ONE - 1) >> FRAC_BITS;
	if (new_samples > MAX_SAMPLE_CHUNK)
		new_samples = MAX_SAMPLE_CHUNK;

	/* generate them into our buffer */
	tms5220_process(sample_data, new_samples);
	prev = curr;
	curr = *curr_data++;

	/* then sample-rate convert with linear interpolation */
	while (length > 0)
	{
		/* interpolate */
		while (length > 0 && source_pos < FRAC_ONE)
		{
			*buffer++ = (((INT32)prev * (FRAC_ONE - source_pos)) + ((INT32)curr * source_pos)) >> FRAC_BITS;
			source_pos += source_step;
			length--;
		}

		/* if we're over, grab the next samples */
		if (source_pos >= FRAC_ONE)
		{
			source_pos -= FRAC_ONE;
			prev = curr;
			curr = *curr_data++;
		}
	}

	/* remember the last samples */
	last_sample = prev;
	curr_sample = curr;
}



/**********************************************************************************************

     tms5220_set_frequency -- adjusts the playback frequency

***********************************************************************************************/

void tms5220_set_frequency(int frequency)
{
	/* skip if output frequency is zero */
	if (!Machine->sample_rate)
		return;

	/* update the stream and compute a new step size */
	if (stream != -1)
		stream_update(stream, 0);
	source_step = (UINT32)((float)(frequency / 80) * (float)FRAC_ONE / (float)Machine->sample_rate);
}
