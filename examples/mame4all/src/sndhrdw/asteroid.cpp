/*****************************************************************************
 *
 * A (partially wrong) try to emulate Asteroid's analog sound
 * It's getting better but is still imperfect :/
 * If you have ideas, corrections, suggestions contact Juergen
 * Buchmueller <pullmoll@t-online.de>
 *
 * Known issues (TODO):
 * - find out if/why the samples are 'damped', I don't see no
 *	 low pass filter in the sound output, but the samples sound
 *	 like there should be one. Maybe in the amplifier..
 * - better (accurate) way to emulate the low pass on the thrust sound?
 * - verify the thump_frequency calculation. It's only an approximation now
 * - the nastiest piece of the circuit is the saucer sound IMO
 *	 the circuits are almost equal, but there are some strange looking
 *	 things like the direct coupled op-amps and transistors and I can't
 *	 calculate the true resistance and thus voltage at the control
 *	 input (5) of the NE555s.
 * - saucer sound is not easy either and the calculations might be off, still.
 *
 *****************************************************************************/

#include <math.h>
#include "driver.h"

#define VMAX    32767
#define VMIN	0

#define SAUCEREN    0
#define SAUCRFIREEN 1
#define SAUCERSEL   2
#define THRUSTEN    3
#define SHIPFIREEN	4
#define LIFEEN		5

#define EXPITCH0	(1<<6)
#define EXPITCH1	(1<<7)
#define EXPAUDSHIFT 2
#define EXPAUDMASK	(0x0f<<EXPAUDSHIFT)

#define NE555_T1(Ra,Rb,C)	(VMAX*2/3/(0.639*((Ra)+(Rb))*(C)))
#define NE555_T2(Ra,Rb,C)	(VMAX*2/3/(0.639*(Rb)*(C)))
#define NE555_F(Ra,Rb,C)	(1.44/(((Ra)+2*(Rb))*(C)))
static int channel;
static int explosion_latch;
static int thump_latch;
static int sound_latch[8];

static int polynome;
static int thump_frequency;

static INT16 *discharge;
static INT16 vol_explosion[16];
#define EXP(charge,n) (charge ? 0x7fff - discharge[0x7fff-n] : discharge[n])

INLINE int explosion(int samplerate)
{
	static int counter, sample_counter;
	static int out;

	counter -= 12000;
	while( counter <= 0 )
	{
		counter += samplerate;
		if( ((polynome & 0x4000) == 0) == ((polynome & 0x0040) == 0) )
			polynome = (polynome << 1) | 1;
		else
			polynome <<= 1;
		if( ++sample_counter == 16 )
		{
			sample_counter = 0;
			if( explosion_latch & EXPITCH0 )
				sample_counter |= 2 + 8;
			else
				sample_counter |= 4;
			if( explosion_latch & EXPITCH1 )
				sample_counter |= 1 + 8;
		}
		/* ripple count output is high? */
		if( sample_counter == 15 )
			out = polynome & 1;
	}
	if( out )
		return vol_explosion[(explosion_latch & EXPAUDMASK) >> EXPAUDSHIFT];

    return 0;
}

INLINE int thrust(int samplerate)
{
	static int counter, out, amp;

    if( sound_latch[THRUSTEN] )
	{
		/* SHPSND filter */
		counter -= 110;
		while( counter <= 0 )
		{
			counter += samplerate;
			out = polynome & 1;
		}
		if( out )
		{
			if( amp < VMAX )
				amp += (VMAX - amp) * 32768 / 32 / samplerate + 1;
		}
		else
		{
			if( amp > VMIN )
				amp -= amp * 32768 / 32 / samplerate + 1;
		}
		return amp;
	}
	return 0;
}

INLINE int thump(int samplerate)
{
	static int counter, out;

    if( thump_latch & 0x10 )
	{
		counter -= thump_frequency;
		while( counter <= 0 )
		{
			counter += samplerate;
			out ^= 1;
		}
		if( out )
			return VMAX;
	}
	return 0;
}


INLINE int saucer(int samplerate)
{
	static int vco, vco_charge, vco_counter;
    static int out, counter;
	float v5;

    /* saucer sound enabled ? */
	if( sound_latch[SAUCEREN] )
	{
		/* NE555 setup as astable multivibrator:
		 * C = 10u, Ra = 5.6k, Rb = 10k
		 * or, with /SAUCERSEL being low:
		 * C = 10u, Ra = 5.6k, Rb = 6k (10k parallel with 15k)
		 */
		if( vco_charge )
		{
			if( sound_latch[SAUCERSEL] )
				vco_counter -= NE555_T1(5600,10000,10e-6);
			else
				vco_counter -= NE555_T1(5600,6000,10e-6);
			if( vco_counter <= 0 )
			{
				int steps = (-vco_counter / samplerate) + 1;
				vco_counter += steps * samplerate;
				if( (vco += steps) >= VMAX*2/3 )
				{
					vco = VMAX*2/3;
					vco_charge = 0;
				}
			}
		}
		else
		{
			if( sound_latch[SAUCERSEL] )
				vco_counter -= NE555_T2(5600,10000,10e-6);
			else
				vco_counter -= NE555_T2(5600,6000,10e-6);
			if( vco_counter <= 0 )
			{
				int steps = (-vco_counter / samplerate) + 1;
				vco_counter += steps * samplerate;
				if( (vco -= steps) <= VMAX*1/3 )
				{
					vco = VMIN*1/3;
					vco_charge = 1;
				}
			}
		}
		/*
		 * NE566 voltage controlled oscillator
		 * Co = 0.047u, Ro = 10k
		 * to = 2.4 * (Vcc - V5) / (Ro * Co * Vcc)
		 */
		if( sound_latch[SAUCERSEL] )
			v5 = 12.0 - 1.66 - 5.0 * EXP(vco_charge,vco) / 32768;
		else
			v5 = 11.3 - 1.66 - 5.0 * EXP(vco_charge,vco) / 32768;
		counter -= floor(2.4 * (12.0 - v5) / (10000 * 0.047e-6 * 12.0));
		while( counter <= 0 )
		{
			counter += samplerate;
			out ^= 1;
		}
		if( out )
			return VMAX;
	}
	return 0;
}

INLINE int saucerfire(int samplerate)
{
	static int vco, vco_counter;
	static int amp, amp_counter;
	static int out, counter;

    if( sound_latch[SAUCRFIREEN] )
	{
		if( vco < VMAX*12/5 )
		{
			/* charge C38 (10u) through R54 (10K) from 5V to 12V */
			#define C38_CHARGE_TIME (VMAX)
			vco_counter -= C38_CHARGE_TIME;
			while( vco_counter <= 0 )
			{
				vco_counter += samplerate;
				if( ++vco == VMAX*12/5 )
					break;
			}
		}
		if( amp > VMIN )
		{
			/* discharge C39 (10u) through R58 (10K) and diode CR6,
			 * but only during the time the output of the NE555 is low.
			 */
			if( out )
			{
				#define C39_DISCHARGE_TIME (int)(VMAX)
				amp_counter -= C39_DISCHARGE_TIME;
				while( amp_counter <= 0 )
				{
					amp_counter += samplerate;
					if( --amp == VMIN )
						break;
				}
			}
		}
		if( out )
		{
			/* C35 = 1u, Ra = 3.3k, Rb = 680
			 * discharge = 0.693 * 680 * 1e-6 = 4.7124e-4 -> 2122 Hz
			 */
			counter -= 2122;
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				out = 0;
			}
		}
		else
		{
			/* C35 = 1u, Ra = 3.3k, Rb = 680
			 * charge 0.693 * (3300+680) * 1e-6 = 2.75814e-3 -> 363Hz
			 */
			counter -= 363 * 2 * (VMAX*12/5-vco) / 32768;
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				out = 1;
			}
		}
        if( out )
			return amp;
	}
	else
	{
		/* charge C38 and C39 */
		amp = VMAX;
		vco = VMAX;
	}
	return 0;
}

INLINE int shipfire(int samplerate)
{
	static int vco, vco_counter;
	static int amp, amp_counter;
    static int out, counter;

    if( sound_latch[SHIPFIREEN] )
	{
		if( vco < VMAX*12/5 )
		{
			/* charge C47 (1u) through R52 (33K) and Q3 from 5V to 12V */
			#define C47_CHARGE_TIME (VMAX * 3)
			vco_counter -= C47_CHARGE_TIME;
			while( vco_counter <= 0 )
			{
				vco_counter += samplerate;
				if( ++vco == VMAX*12/5 )
					break;
			}
        }
		if( amp > VMIN )
		{
			/* discharge C48 (10u) through R66 (2.7K) and CR8,
			 * but only while the output of theNE555 is low.
			 */
			if( out )
			{
				#define C48_DISCHARGE_TIME (VMAX * 3)
				amp_counter -= C48_DISCHARGE_TIME;
				while( amp_counter <= 0 )
				{
					amp_counter += samplerate;
					if( --amp == VMIN )
						break;
				}
			}
		}

		if( out )
		{
			/* C50 = 1u, Ra = 3.3k, Rb = 680
			 * discharge = 0.693 * 680 * 1e-6 = 4.7124e-4 -> 2122 Hz
			 */
			counter -= 2122;
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				out = 0;
			}
		}
		else
		{
			/* C50 = 1u, Ra = R65 (3.3k), Rb = R61 (680)
			 * charge = 0.693 * (3300+680) * 1e-6) = 2.75814e-3 -> 363Hz
			 */
			counter -= 363 * 2 * (VMAX*12/5-vco) / 32768;
			if( counter <= 0 )
			{
				int n = -counter / samplerate + 1;
				counter += n * samplerate;
				out = 1;
			}
		}
		if( out )
			return amp;
	}
	else
	{
		/* charge C47 and C48 */
		amp = VMAX;
		vco = VMAX;
	}
	return 0;
}

INLINE int life(int samplerate)
{
	static int counter, out;
    if( sound_latch[LIFEEN] )
	{
		counter -= 3000;
		while( counter <= 0 )
		{
			counter += samplerate;
			out ^= 1;
		}
		if( out )
			return VMAX;
	}
	return 0;
}


static void asteroid_sound_update(int param, INT16 *buffer, int length)
{
	int samplerate = Machine->sample_rate;

    while( length-- > 0 )
	{
		int sum = 0;

		sum += explosion(samplerate) / 7;
		sum += thrust(samplerate) / 7;
		sum += thump(samplerate) / 7;
		sum += saucer(samplerate) / 7;
		sum += saucerfire(samplerate) / 7;
		sum += shipfire(samplerate) / 7;
		sum += life(samplerate) / 7;

        *buffer++ = sum;
	}
}

static void explosion_init(void)
{
	int i;

    for( i = 0; i < 16; i++ )
    {
        /* r0 = open, r1 = open */
        float r0 = 1.0/1e12, r1 = 1.0/1e12;

        /* R14 */
        if( i & 1 )
            r1 += 1.0/47000;
        else
            r0 += 1.0/47000;
        /* R15 */
        if( i & 2 )
            r1 += 1.0/22000;
        else
            r0 += 1.0/22000;
        /* R16 */
        if( i & 4 )
            r1 += 1.0/12000;
        else
            r0 += 1.0/12000;
        /* R17 */
        if( i & 8 )
            r1 += 1.0/5600;
        else
            r0 += 1.0/5600;
        r0 = 1.0/r0;
        r1 = 1.0/r1;
        vol_explosion[i] = VMAX * r0 / (r0 + r1);
    }

}

int asteroid_sh_start(const struct MachineSound *msound)
{
    int i;

	discharge = (INT16 *)malloc(32768 * sizeof(INT16));
	if( !discharge )
        return 1;

    for( i = 0; i < 0x8000; i++ )
		discharge[0x7fff-i] = (INT16) (0x7fff/exp(1.0*i/4096));

	/* initialize explosion volume lookup table */
	explosion_init();

    channel = stream_init("Custom", 100, Machine->sample_rate, 0, asteroid_sound_update);
    if( channel == -1 )
        return 1;

    return 0;
}

void asteroid_sh_stop(void)
{
	if( discharge )
		free(discharge);
	discharge = NULL;
}

void asteroid_sh_update(void)
{
	stream_update(channel, 0);
}


WRITE_HANDLER( asteroid_explode_w )
{
	if( data == explosion_latch )
		return;

    stream_update(channel, 0);
	explosion_latch = data;
}



WRITE_HANDLER( asteroid_thump_w )
{
	float r0 = 1/47000, r1 = 1/1e12;

    if( data == thump_latch )
		return;

    stream_update(channel, 0);
	thump_latch = data;

	if( thump_latch & 1 )
		r1 += 1.0/220000;
	else
		r0 += 1.0/220000;
	if( thump_latch & 2 )
		r1 += 1.0/100000;
	else
		r0 += 1.0/100000;
	if( thump_latch & 4 )
		r1 += 1.0/47000;
	else
		r0 += 1.0/47000;
	if( thump_latch & 8 )
		r1 += 1.0/22000;
	else
		r0 += 1.0/22000;

	/* NE555 setup as voltage controlled astable multivibrator
	 * C = 0.22u, Ra = 22k...???, Rb = 18k
	 * frequency = 1.44 / ((22k + 2*18k) * 0.22n) = 56Hz .. huh?
	 */
	thump_frequency = 56 + 56 * r0 / (r0 + r1);
}


WRITE_HANDLER( asteroid_sounds_w )
{
	data &= 0x80;
    if( data == sound_latch[offset] )
		return;

    stream_update(channel, 0);
	sound_latch[offset] = data;
}



static void astdelux_sound_update(int param, INT16 *buffer, int length)
{
	int samplerate = Machine->sample_rate;

    while( length-- > 0)
	{
		int sum = 0;

		sum += explosion(samplerate) / 2;
		sum += thrust(samplerate) / 2;

		*buffer++ = sum;
	}
}

int astdelux_sh_start(const struct MachineSound *msound)
{
	/* initialize explosion volume lookup table */
	explosion_init();

	channel = stream_init("Custom", 50, Machine->sample_rate, 0, astdelux_sound_update);
    if( channel == -1 )
        return 1;

    return 0;
}

void astdelux_sh_stop(void)
{
}

void astdelux_sh_update(void)
{
	stream_update(channel, 0);
}


WRITE_HANDLER( astdelux_sounds_w )
{
	data = ~data & 0x80;
	if( data == sound_latch[THRUSTEN] )
		return;
    stream_update(channel, 0);
	sound_latch[THRUSTEN] = data;
}


