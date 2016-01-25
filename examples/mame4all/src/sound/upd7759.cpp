/************************************************************

 NEC uPD7759 ADPCM Speech Processor
 by: Juergen Buchmueller, Mike Balfour and Howie Cohen


 Description:
 The uPD7759 is a speech processing LSI that, with an external
 ROM, utilizes ADPCM to produce speech.  The uPD7759 can
 directly address up to 1Mbits of external data ROM, or the
 host CPU can control the speech data transfer.  Three sample
 frequencies are selectable - 5, 6, or 8 kHz.  The external
 ROM can store a maximum of 256 different messages and up to
 50 seconds of speech.

 The uPD7759 should always be hooked up to a 640 kHz clock.

 TODO:
 1) find bugs
 2) fix bugs
 3) Bankswitching and frequency selection may not be 100%

NOTES:

There are 2 types of upd7759 sound roms, master and slave.

A master rom has a header at the beginning of the rom
for example : 15 5A A5 69 55 (this is the POW header)

-the 1st byte (15) is the number of samples stored in the rom
 (actually the number of samples minus 1 - NS)
-the next 4 bytes seems standard in every upd7759 rom used in
master mode (5A A5 69 55)
-after that there is table of (sample offsets)/2 we use this to
calculate the sample table on the fly. Then the samples start,
each sample has a short header with sample rate info in it.
A master rom can have up to 256 samples , and there should be
only one rom per upd7759.

a slave rom has no header... but each sample starts with
FF 00 00 00 00 10 (followed by a few other bytes that are
usually the same but not always)

Clock rates:
in master mode the clock rate should always be 64000000
sample frequencies are selectable - 5, 6, or 8 kHz. This
info is coded in the uPD7759 roms, it selects the sample
frequency for you.

slave mode is still some what of a mystery.  Everything
we know about slave mode (and 1/2 of everything in master
mode) is from guesswork. As far as we know the clock rate
is the same for all samples in slave mode on a per game basis
so valid clock rates are: 40000000, 48000000, 64000000

Differances between master/slave mode.
(very basic explanation based on my understanding)

Master mode: the sound cpu sends a sample number to the upd7759
it then sends a trigger command to it, and the upd7759 plays the
sample directly from the rom (like an adpcm chip)

Slave mode:  the sound cpu sends data to the upd7759 to select
the bank for the sound data, each bank is 0x4000 in size, and
the sample offset. The sound cpu then sends the data for the sample
a byte(?) at a time (dac / cvsd like) which the upd7759 plays till
it reaches the header of the next sample (FF 00 00 00 00)

Changes:
05/99	HJB
	Tried to figure better index_shift and diff_lookutp tables and
	also adjusted sample value range. It seems the 4 bits of the
	ADPCM data are signed (0x0f == -1)! Also a signal and step
	width fall off seems to be closer to the real thing.
	Reduced work load by adding a wrap around buffer for slave
	mode data that is stuffed by the sound CPU.
	Finally removed (now obsolete) 8 bit sample support.

 *************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "driver.h"
#include "upd7759.h"
#include "streams.h"

#define VERBOSE 0

#if VERBOSE
#define LOG(n,x)  if((n)>=VERBOSE) logerror x
#else
#define LOG(n,x)
#endif

/* number of samples stuffed into the rom */
static unsigned char numsam;

/* playback rate for the streams interface */
/* BASE_CLOCK or a multiple (if oversampling is active) */
static int emulation_rate;

static int base_rate;
/* define the output rate */
#define CLOCK_DIVIDER	80

#define OVERSAMPLING	0	/* 1 use oversampling, 0 don't */

/* signal fall off factor */
#define FALL_OFF(n) 	((n)-(((n)+7)/8))

#define SIGNAL_BITS 	15	/* signal range */
#define SIGNAL_MAX		(0x7fff >> (15-SIGNAL_BITS))
#define SIGNAL_MIN		-SIGNAL_MAX

#define STEP_MAX		32
#define STEP_MIN		0

#define DATA_MAX		512

struct UPD7759sample
{
	unsigned int offset;	/* offset in that region */
	unsigned int length;    /* length of the sample */
	unsigned int freq;		/* play back freq of sample */
};


/* struct describing a single playing ADPCM voice */
struct UPD7759voice
{
	int playing;            /* 1 if we are actively playing */
	unsigned char *base;    /* pointer to the base memory location */
	int mask;               /* mask to keep us within the buffer */
	int sample; 			/* current sample number (sample data in slave mode) */
	int freq;				/* current sample playback freq */
	int count;              /* total samples to play */
	int signal;             /* current ADPCM signal */
#if OVERSAMPLING
	int old_signal; 		/* last ADPCM signal */
#endif
    int step;               /* current ADPCM step */
	int counter;			/* sample counter */
	void *timer;			/* timer used in slave mode */
	int data[DATA_MAX]; 	/* data array used in slave mode */
	unsigned head;			/* head of data array used in slave mode */
	unsigned tail;			/* tail of data array used in slave mode */
	unsigned available;
};

/* global pointer to the current interface */
static const struct UPD7759_interface *upd7759_intf;

/* array of ADPCM voices */
static struct UPD7759voice updadpcm[MAX_UPD7759];

#if OVERSAMPLING
/* oversampling factor, ie. playback_rate / BASE_CLOCK */
static int oversampling;
#endif
/* array of channels returned by streams.c */
static int channel[MAX_UPD7759];

/* stores the current sample number */
static int sampnum[MAX_UPD7759];

/* step size index shift table */
#define INDEX_SHIFT_MAX 16
static int index_shift[INDEX_SHIFT_MAX] = {
	0,	 1,  2,  3,  6,  7, 10, 15,
	0,  15, 10,  7,  6,  3,  2,  1
};

/* lookup table for the precomputed difference */
static int diff_lookup[(STEP_MAX+1)*16];

static void UPD7759_update (int chip, INT16 *buffer, int left);

/*
 *   Compute the difference table
 */

static void ComputeTables (void)
{
	/* nibble to bit map */
    static int nbl2bit[16][4] = {
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1},
	};
    int step, nib;

	/* loop over all possible steps */
	for (step = 0; step <= STEP_MAX; step++)
	{
        /* compute the step value */
		int stepval = 6 * (step+1) * (step+1);
		LOG(1,("step %2d:", step));
		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
				 stepval/2 * nbl2bit[nib][2] +
				 stepval/4 * nbl2bit[nib][3] +
				 stepval/8);
			LOG(1,(" %+6d", diff_lookup[step*16 + nib]));
        }
		LOG(1,("\n"));
    }
}



static int find_sample(int num, int sample_num,struct UPD7759sample *sample)
{
	int j;
	int nextoff = 0;
	unsigned char *memrom;
	unsigned char *header;   /* upd7759 has a 4 byte what we assume is an identifier (bytes 1-4)*/
	unsigned char *data;


	memrom = memory_region(upd7759_intf->region[num]);

	numsam = (unsigned int)memrom[0]; /* get number of samples from sound rom */
	header = &(memrom[1]);

	if (memcmp (header, "\x5A\xA5\x69\x55",4) == 0)
	{
		LOG(1,("uPD7759 header verified\n"));
	}
	else
	{
		LOG(1,("uPD7759 header verification failed\n"));
	}

	LOG(1,("Number of samples in UPD7759 rom = %d\n",numsam));

	/* move the header pointer to the start of the sample offsets */
	header = &(memrom[5]);


	if (sample_num > numsam) return 0;	/* sample out of range */


	nextoff = 2 * sample_num;
	sample->offset = ((((unsigned int)(header[nextoff]))<<8)+(header[nextoff+1]))*2;
	data = &memory_region(upd7759_intf->region[num])[sample->offset];
	/* guesswork, probably wrong */
	j = 0;
	if (!data[j]) j++;
	if ((data[j] & 0xf0) != 0x50) j++;

	// Added and Modified by Takahiro Nogi. 1999/10/28
#if 0	// original
	switch (data[j])
	{
		case 0x53: sample->freq = 8000; break;
		case 0x59: sample->freq = 6000; break;
		case 0x5f: sample->freq = 5000; break;
		default:
			sample->freq = 5000;
	}
#else	// modified by Takahiro Nogi. 1999/10/28
	switch (data[j] & 0x1f)
	{
		case 0x13: sample->freq = 8000; break;
		case 0x19: sample->freq = 6000; break;
		case 0x1f: sample->freq = 5000; break;
		default:				// ???
			sample->freq = 5000;
	}
#endif

	if (sample_num == numsam)
	{
		sample->length = 0x20000 - sample->offset;
	}
	else
		sample->length = ((((unsigned int)(header[nextoff+2]))<<8)+(header[nextoff+3]))*2 -
							((((unsigned int)(header[nextoff]))<<8)+(header[nextoff+1]))*2;

	data = &memory_region(upd7759_intf->region[num])[sample->offset];
	/*
	logerror("play sample %3d, offset $%06x, length %5d, freq = %4d [data $%02x $%02x $%02x]\n",
		sample_num,
		sample->offset,
		sample->length,
		sample->freq,
		data[0],data[1],data[2]);
    */
	return 1;
}


/*
 *   Start emulation of several ADPCM output streams
 */


int UPD7759_sh_start (const struct MachineSound *msound)
{
	int i;
	const struct UPD7759_interface *intf = (const struct UPD7759_interface *)msound->sound_interface;

	if( Machine->sample_rate == 0 )
		return 0;

    /* compute the difference tables */
	ComputeTables ();

    /* copy the interface pointer to a global */
	upd7759_intf = intf;
	base_rate = intf->clock_rate / CLOCK_DIVIDER;

#if OVERSAMPLING
	oversampling = (Machine->sample_rate / base_rate);
	if (!oversampling) oversampling = 1;
	emulation_rate = base_rate * oversampling;
#else
	emulation_rate = base_rate;
#endif


	memset(updadpcm,0,sizeof(updadpcm));
	for (i = 0; i < intf->num; i++)
	{
		char name[20];

		updadpcm[i].mask = 0xffffffff;
		updadpcm[i].signal = 0;
		updadpcm[i].step = 0;
		updadpcm[i].counter = emulation_rate / 2;

		sprintf(name,"uPD7759 #%d",i);

		channel[i] = stream_init(name,intf->volume[i],emulation_rate,i,UPD7759_update);
	}
	return 0;
}


/*
 *   Stop emulation of several UPD7759 output streams
 */

void UPD7759_sh_stop (void)
{
}


/*
 *   Update emulation of an uPD7759 output stream
 */
static void UPD7759_update (int chip, INT16 *buffer, int left)
{
	struct UPD7759voice *voice = &updadpcm[chip];
	int i;

	/* see if there's actually any need to generate samples */
	LOG(3,("UPD7759_update %d (%d)\n", left, voice->available));

    if (left > 0)
	{
        /* if this voice is active */
		if (voice->playing)
		{
			voice->available -= left;
			if( upd7759_intf->mode == UPD7759_SLAVE_MODE )
			{
				while( left-- > 0 )
				{
					*buffer++ = voice->data[voice->tail];
#if OVERSAMPLE
					if( (voice->counter++ % OVERSAMPLE) == 0 )
#endif
                    voice->tail = (voice->tail + 1) % DATA_MAX;
				}
			}
			else
			{
				unsigned char *base = voice->base;
                int val;
#if OVERSAMPLING
				int i, delta;
#endif

                while( left > 0 )
				{
					/* compute the new amplitude and update the current voice->step */
					val = base[(voice->sample / 2) & voice->mask] >> (((voice->sample & 1) << 2) ^ 4);
					voice->step = FALL_OFF(voice->step) + index_shift[val & (INDEX_SHIFT_MAX-1)];
					if (voice->step > STEP_MAX) voice->step = STEP_MAX;
					else if (voice->step < STEP_MIN) voice->step = STEP_MIN;
					voice->signal = FALL_OFF(voice->signal) + diff_lookup[voice->step * 16 + (val & 15)];
					if (voice->signal > SIGNAL_MAX) voice->signal = SIGNAL_MAX;
					else if (voice->signal < SIGNAL_MIN) voice->signal = SIGNAL_MIN;
#if OVERSAMPLING
					i = 0;
					delta = voice->signal - voice->old_signal;
					while (voice->counter > 0 && left > 0)
					{
						*sample++ = voice->old_signal + delta * i / oversampling;
						if (++i == oversampling) i = 0;
						voice->counter -= voice->freq;
						left--;
					}
					voice->old_signal = voice->signal;
#else
					while (voice->counter > 0 && left > 0)
					{
						*buffer++ = voice->signal;
						voice->counter -= voice->freq;
						left--;
					}
#endif
					voice->counter += emulation_rate;

					/* next! */
					if( ++voice->sample > voice->count )
					{
						while (left-- > 0)
						{
							*buffer++ = voice->signal;
							voice->signal = FALL_OFF(voice->signal);
						}
						voice->playing = 0;
						break;
					}
				}
            }
		}
		else
		{
			/* voice is not playing */
			for (i = 0; i < left; i++)
				*buffer++ = voice->signal;
		}
	}
}

/************************************************************
 UPD7759_message_w

 Store the inputs to I0-I7 externally to the uPD7759.

 I0-I7 input the message number of the message to be
 reproduced. The inputs are latched at the rising edge of the
 !ST input. Unused pins should be grounded.

 In slave mode it seems like the ADPCM data is stuffed
 here from an external source (eg. Z80 NMI code).
 *************************************************************/

void UPD7759_message_w (int num, int data)
{
	struct UPD7759voice *voice = updadpcm + num;

	/* bail if we're not playing anything */
	if (Machine->sample_rate == 0)
		return;

	/* range check the numbers */
	if( num >= upd7759_intf->num )
	{
		LOG(1,("error: UPD7759_SNDSELECT() called with channel = %d, but only %d channels allocated\n", num, upd7759_intf->num));
		return;
	}

	if (upd7759_intf->mode == UPD7759_SLAVE_MODE)
	{
		int offset = -1;

		//LOG(1,("upd7759_message_w $%02x\n", data));
		//logerror("upd7759_message_w $%2x\n",data);

        switch (data) {

			case 0x00: 							/* roms 0x10000 & 0x20000 in size */
			case 0x38: offset = 0x10000; break; /* roms 0x8000 in size */

			case 0x01: 							/* roms 0x10000 & 0x20000 in size */
			case 0x39: offset = 0x14000; break; /* roms 0x8000 in size */

			case 0x02: 							/* roms 0x10000 & 0x20000 in size */
			case 0x34: offset = 0x18000; break; /* roms 0x8000 in size */

			case 0x03: 							/* roms 0x10000 & 0x20000 in size */
			case 0x35: offset = 0x1c000; break; /* roms 0x8000 in size */

			case 0x04:							/* roms 0x10000 & 0x20000 in size */
			case 0x2c: offset = 0x20000; break; /* roms 0x8000 in size */

			case 0x05: 							/* roms 0x10000 & 0x20000 in size */
			case 0x2d: offset = 0x24000; break; /* roms 0x8000 in size */

			case 0x06:							/* roms 0x10000 & 0x20000 in size */
			case 0x1c: offset = 0x28000; break;	/* roms 0x8000 in size in size */

			case 0x07: 							/* roms 0x10000 & 0x20000 in size */
			case 0x1d: offset = 0x2c000; break;	/* roms 0x8000 in size */

			case 0x08: offset = 0x30000; break; /* roms 0x10000 & 0x20000 in size */
			case 0x09: offset = 0x34000; break; /* roms 0x10000 & 0x20000 in size */
			case 0x0a: offset = 0x38000; break; /* roms 0x10000 & 0x20000 in size */
			case 0x0b: offset = 0x3c000; break; /* roms 0x10000 & 0x20000 in size */
			case 0x0c: offset = 0x40000; break; /* roms 0x10000 & 0x20000 in size */
			case 0x0d: offset = 0x44000; break; /* roms 0x10000 & 0x20000 in size */
			case 0x0e: offset = 0x48000; break; /* roms 0x10000 & 0x20000 in size */
			case 0x0f: offset = 0x4c000; break; /* roms 0x10000 & 0x20000 in size */

			default:

				//LOG(1,("upd7759_message_w unhandled $%02x\n", data));
				//logerror("upd7759_message_w unhandled $%02x\n", data);
				if ((data & 0xc0) == 0xc0)
				{
					if (voice->timer)
					{
						timer_remove(voice->timer);
						voice->timer = 0;
					}
					voice->playing = 0;
				}
        }
		if (offset > 0)
		{
			voice->base = &memory_region(upd7759_intf->region[num])[offset];
			//LOG(1,("upd7759_message_w set base $%08x\n", offset));
			//logerror("upd7759_message_w set base $%08x\n", offset);
        }
	}
	else
	{

		LOG(1,("uPD7759 calling sample : %d\n", data));
		sampnum[num] = data;

    }
}

/************************************************************
 UPD7759_dac

 Called by the timer interrupt at twice the sample rate.
 The first time the external irq callback is called, the
 second time the ADPCM msb is converted and the resulting
 signal is sent to the DAC.
 ************************************************************/
static void UPD7759_dac(int num)
{
	static int dac_msb = 0;
	struct UPD7759voice *voice = updadpcm + num;

	dac_msb ^= 1;
	if( dac_msb )
	{
		LOG(3,("UPD7759_dac:    $%x ", voice->sample & 15));
        /* convert lower nibble */
		voice->step = FALL_OFF(voice->step) + index_shift[voice->sample & (INDEX_SHIFT_MAX-1)];
        if (voice->step > STEP_MAX) voice->step = STEP_MAX;
        else if (voice->step < STEP_MIN) voice->step = STEP_MIN;
		voice->signal = FALL_OFF(voice->signal) + diff_lookup[voice->step * 16 + (voice->sample & 15)];
		if (voice->signal > SIGNAL_MAX) voice->signal = SIGNAL_MAX;
		else if (voice->signal < SIGNAL_MIN) voice->signal = SIGNAL_MIN;
		LOG(3,("step: %3d signal: %+5d\n", voice->step, voice->signal));
		voice->head = (voice->head + 1) % DATA_MAX;
		voice->data[voice->head] = voice->signal;
		voice->available++;
    }
	else
	{
		if( upd7759_intf->irqcallback[num] )
			(*upd7759_intf->irqcallback[num])(num);
    }
}

/************************************************************
 UPD7759_start_w

 !ST pin:
 Setting the !ST input low while !CS is low will start
 speech reproduction of the message in the speech ROM locations
 addressed by the contents of I0-I7. If the device is in
 standby mode, standby mode will be released.
 NOTE: While !BUSY is low, another !ST will not be accepted.
 *************************************************************/

void UPD7759_start_w (int num, int data)
{
	struct UPD7759voice *voice = updadpcm + num;

	/* bail if we're not playing anything */
	if (Machine->sample_rate == 0)
		return;

	/* range check the numbers */
	if( num >= upd7759_intf->num )
	{
		LOG(1,("error: UPD7759_play_stop() called with channel = %d, but only %d channels allocated\n", num, upd7759_intf->num));
		return;
	}

	/* handle the slave mode */
	if (upd7759_intf->mode == UPD7759_SLAVE_MODE)
	{
		if (voice->playing)
		{
            /* if the chip is busy this should be the ADPCM data */
			data &= 0xff;	/* be sure to use 8 bits value only */
			LOG(3,("UPD7759_data_w: $%x ", (data >> 4) & 15));

            /* detect end of a sample by inspection of the last 5 bytes */
			/* FF 00 00 00 00 is the start of the next sample */
			if( voice->count > 5 && voice->sample == 0xff && data == 0x00 )
			{
                /* remove an old timer */
                if (voice->timer)
                {
                    timer_remove(voice->timer);
                    voice->timer = 0;
				}
                /* stop playing this sample */
				voice->playing = 0;
				return;
            }

			/* save the data written in voice->sample */
			voice->sample = data;
			voice->count++;

            /* conversion of the ADPCM data to a new signal value */
			voice->step = FALL_OFF(voice->step) + index_shift[(voice->sample >> 4) & (INDEX_SHIFT_MAX-1)];
            if (voice->step > STEP_MAX) voice->step = STEP_MAX;
            else if (voice->step < STEP_MIN) voice->step = STEP_MIN;
			voice->signal = FALL_OFF(voice->signal) + diff_lookup[voice->step * 16 + ((voice->sample >> 4) & 15)];
            if (voice->signal > SIGNAL_MAX) voice->signal = SIGNAL_MAX;
            else if (voice->signal < SIGNAL_MIN) voice->signal = SIGNAL_MIN;
			LOG(3,("step: %3d signal: %+5d\n", voice->step, voice->signal));
			voice->head = (voice->head + 1) % DATA_MAX;
			voice->data[voice->head] = voice->signal;
			voice->available++;
		}
		else
		{
			LOG(2,("UPD7759_start_w: $%02x\n", data));
            /* remove an old timer */
			if (voice->timer)
			{
                timer_remove(voice->timer);
                voice->timer = 0;
            }
			/* bring the chip in sync with the CPU */
			stream_update(channel[num], 0);
            /* start a new timer */
			voice->timer = timer_pulse( TIME_IN_HZ(base_rate), num, UPD7759_dac );
			voice->signal = 0;
			voice->step = 0;	/* reset the step width */
			voice->count = 0;	/* reset count for the detection of an sample ending */
			voice->playing = 1; /* this voice is now playing */
            voice->tail = 0;
			voice->head = 0;
			voice->available = 0;
        }
	}
	else
	{
		struct UPD7759sample sample;

		/* if !ST is high, do nothing */ /* EHC - 13/08/99 */
		if (data > 0)
			return;

		/* bail if the chip is busy */
		if (voice->playing)
			return;

		LOG(2,("UPD7759_start_w: %d\n", data));

		/* find a match */
		if (find_sample(num,sampnum[num],&sample))
		{
			/* update the  voice */
			stream_update(channel[num], 0);
			voice->freq = sample.freq;
			/* set up the voice to play this sample */
			voice->playing = 1;
			voice->base = &memory_region(upd7759_intf->region[num])[sample.offset];
			voice->sample = 0;
			/* sample length needs to be doubled (counting nibbles) */
			voice->count = sample.length * 2;

			/* also reset the chip parameters */
			voice->step = 0;
			voice->counter = emulation_rate / 2;

			return;
		}

		LOG(1,("warning: UPD7759_playing_w() called with invalid number = %08x\n",data));
	}
}

/************************************************************
 UPD7759_data_r

 External read data from the UPD7759 memory region based
 on voice->base. Used in slave mode to retrieve data to
 stuff into UPD7759_message_w.
 *************************************************************/

int UPD7759_data_r(int num, int offs)
{
	struct UPD7759voice *voice = updadpcm + num;

    /* If there's no sample rate, do nothing */
    if (Machine->sample_rate == 0)
		return 0x00;

    /* range check the numbers */
	if( num >= upd7759_intf->num )
	{
		LOG(1,("error: UPD7759_data_r() called with channel = %d, but only %d channels allocated\n", num, upd7759_intf->num));
		return 0x00;
    }

	if ( voice->base == NULL )
	{
		LOG(1,("error: UPD7759_data_r() called with channel = %d, but updadpcm[%d].base == NULL\n", num, num));
		return 0x00;
	}

#if VERBOSE
    if (!(offs&0xff)) LOG(1, ("UPD7759#%d sample offset = $%04x\n", num, offs));
#endif

	return voice->base[offs];
}

/************************************************************
 UPD7759_busy_r

 !BUSY pin:
 !BUSY outputs the status of the uPD7759. It goes low during
 speech decode and output operations. When !ST is received,
 !BUSY goes low. While !BUSY is low, another !ST will not be
 accepted. In standby mode, !BUSY becomes high impedance. This
 is an active low output.
 *************************************************************/

int UPD7759_busy_r (int num)
{
	struct UPD7759voice *voice = updadpcm + num;

	/* If there's no sample rate, return not busy */
	if ( Machine->sample_rate == 0 )
		return 1;

	/* range check the numbers */
	if( num >= upd7759_intf->num )
	{
		LOG(1,("error: UPD7759_busy_r() called with channel = %d, but only %d channels allocated\n", num, upd7759_intf->num));
		return 1;
	}

	/* bring the chip in sync with the CPU */
#ifndef MAME_FASTSOUND
	stream_update(channel[num], 0);
#else
    {
        extern int fast_sound;
        if (!fast_sound) stream_update(channel[num], 0);
    }
#endif
	if ( voice->playing == 0 )
	{
		LOG(1,("uPD7759 not busy\n"));
		return 1;
	}
	else
	{
		LOG(1,("uPD7759 busy\n"));
		return 0;
	}

	return 1;
}

/************************************************************
 UPD7759_reset_w

 !RESET pin:
 The !RESET input initialized the chip. Use !RESET following
 power-up to abort speech reproduction or to release standby
 mode. !RESET must remain low at least 12 oscillator clocks.
 At power-up or when recovering from standby mode, !RESET
 must remain low at least 12 more clocks after clock
 oscillation stabilizes.
 *************************************************************/

void UPD7759_reset_w (int num, int data)
{
	struct UPD7759voice *voice = updadpcm + num;

	/* If there's no sample rate, do nothing */
	if( Machine->sample_rate == 0 )
		return;

	/* range check the numbers */
	if( num >= upd7759_intf->num )
	{
		LOG(1,("error: UPD7759_reset_w() called with channel = %d, but only %d channels allocated\n", num, upd7759_intf->num));
		return;
	}

	/* if !RESET is high, do nothing */
	if (data > 0)
		return;

	/* mark the uPD7759 as NOT PLAYING */
	/* (Note: do we need to do anything else?) */
	voice->playing = 0;
}


/* helper functions to be used as memory read handler function pointers */
WRITE_HANDLER( UPD7759_0_message_w )	{ UPD7759_message_w(0,data); }
WRITE_HANDLER( UPD7759_0_start_w )	{ UPD7759_start_w(0,data); }
READ_HANDLER( UPD7759_0_busy_r )	{ return UPD7759_busy_r(0); }
READ_HANDLER( UPD7759_0_data_r )	{ return UPD7759_data_r(0,offset); }
READ_HANDLER( UPD7759_1_data_r )	{ return UPD7759_data_r(1,offset); }
