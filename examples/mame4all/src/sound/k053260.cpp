/*********************************************************

	Konami 053260 PCM Sound Chip

*********************************************************/

#include "driver.h"
#include "k053260.h"
#include "osinline.h"

#define LOG 0

#define BASE_SHIFT	16

#define INTERPOLATE_SAMPLES 0

static struct K053260_channel_def {
	unsigned long		rate;
	unsigned long		size;
	unsigned long		start;
	unsigned long		bank;
	unsigned long		volume;
	int					play;
	unsigned long		pan;
	unsigned long		pos;
#if INTERPOLATE_SAMPLES
	int					steps;
	int					stepcount;
#endif
	int					loop;
	int					ppcm; /* packed PCM ( 4 bit signed ) */
	int					ppcm_data;
} K053260_channel[4];

static struct K053260_chip_def {
	const struct K053260_interface	*intf;
	int								channel;
	int								mode;
	int								regs[0x30];
	unsigned char					*rom;
	int								rom_size;
	void							*timer; /* SH1 int timer */
} K053260_chip;

static unsigned long *delta_table;

static void InitDeltaTable( void ) {
	int		i;
	float	base = ( float )Machine->sample_rate;
	float	max = (float)K053260_chip.intf->clock; /* hz */
	unsigned long val;

	for( i = 0; i < 0x1000; i++ ) {
		float v = ( float )( 0x1000 - i );
		float target = max / v;
		float fixed = ( float )( 1 << BASE_SHIFT );

		if ( target && base ) {
			target = fixed / ( base / target );
			val = ( unsigned long )target;
			if ( val == 0 )
				val = 1;
		} else
			val = 1;

		delta_table[i] = val;
	}
}

static void K053260_reset( void ) {
	int i;

	for( i = 0; i < 4; i++ ) {
		K053260_channel[i].rate = 0;
		K053260_channel[i].size = 0;
		K053260_channel[i].start = 0;
		K053260_channel[i].bank = 0;
		K053260_channel[i].volume = 0;
		K053260_channel[i].play = 0;
		K053260_channel[i].pan = 0;
		K053260_channel[i].pos = 0;
		K053260_channel[i].loop = 0;
		K053260_channel[i].ppcm = 0;
		K053260_channel[i].ppcm_data = 0;
	}
}

INLINE int limit( int val, int max, int min ) {
	if ( val > max )
		val = max;
	else if ( val < min )
		val = min;

	return val;
}

#define MAXOUT 0x7fff
#define MINOUT -0x8000

void K053260_update( int param, INT16 **buffer, int length ) {
	static long dpcmcnv[] = { 0, 1, 4, 9, 16, 25, 36, 49, -64, -49, -36, -25, -16, -9, -4, -1 };

	int i, j, lvol[4], rvol[4], play[4], loop[4], ppcm_data[4], ppcm[4];
	unsigned char *rom[4];
	unsigned long delta[4], end[4], pos[4];
	int dataL, dataR;
	signed char d;
#if INTERPOLATE_SAMPLES
	int steps[4], stepcount[4];
#endif
#ifdef clip_short
	clip_short_pre();
	int val;
#endif
	/* precache some values */
	for ( i = 0; i < 4; i++ ) {
		rom[i]= &K053260_chip.rom[K053260_channel[i].start + ( K053260_channel[i].bank << 16 )];
		delta[i] = delta_table[K053260_channel[i].rate];
		lvol[i] = K053260_channel[i].volume * K053260_channel[i].pan;
		rvol[i] = K053260_channel[i].volume * ( 8 - K053260_channel[i].pan );
		end[i] = K053260_channel[i].size;
		pos[i] = K053260_channel[i].pos;
		play[i] = K053260_channel[i].play;
		loop[i] = K053260_channel[i].loop;
		ppcm[i] = K053260_channel[i].ppcm;
		ppcm_data[i] = K053260_channel[i].ppcm_data;
#if INTERPOLATE_SAMPLES
		steps[i] = K053260_channel[i].steps;
		stepcount[i] = K053260_channel[i].stepcount;
#endif
		if ( ppcm[i] ) {
			delta[i] /= 2;
#if INTERPOLATE_SAMPLES
			steps[i] *= 2;
#endif
		}
	}

		for ( j = 0; j < length; j++ ) {

			dataL = dataR = 0;

			for ( i = 0; i < 4; i++ ) {
				/* see if the voice is on */
				if ( play[i] ) {
					/* see if we're done */
					if ( ( pos[i] >> BASE_SHIFT ) >= end[i] ) {

						ppcm_data[i] = 0;

						if ( loop[i] )
							pos[i] = 0;
						else {
							play[i] = 0;
							continue;
						}
					}

					if ( ppcm[i] ) { /* Packed PCM */
						/* we only update the signal if we're starting or a real sound sample has gone by */
						/* this is all due to the dynamic sample rate convertion */
						if ( pos[i] == 0 || ( ( pos[i] ^ ( pos[i] - delta[i] ) ) & 0x8000 ) == 0x8000 ) {
							int newdata;
							if ( pos[i] & 0x8000 )
								newdata = rom[i][pos[i] >> BASE_SHIFT] & 0x0f;
							else
								newdata = ( ( rom[i][pos[i] >> BASE_SHIFT] ) >> 4 ) & 0x0f;

							ppcm_data[i] = ( ( ppcm_data[i] * 62 ) >> 6 ) + dpcmcnv[newdata];

							if ( ppcm_data[i] > 127 )
								ppcm_data[i] = 127;
							else
								if ( ppcm_data[i] < -128 )
									ppcm_data[i] = -128;
						}

						d = ppcm_data[i];

//						d /= 2;

#if INTERPOLATE_SAMPLES
						if ( steps[i] ) {
							if ( ( pos[i] >> BASE_SHIFT ) < ( end[i] - 1 ) ) {
								signed char diff;
								int next_d;

								if ( pos[i] & 0x8000 )
									next_d = ( ( ( rom[i][(pos[i] >> BASE_SHIFT)+1] ) >> 4 ) & 0x0f ) * 0x11;
								else
									next_d = ( rom[i][( pos[i] >> BASE_SHIFT)] & 0x0f ) * 0x11;

								diff = next_d;
								diff /= 2;
								diff -= d;

								diff /= steps[i];

								d += ( diff * stepcount[i]++ );

								if ( stepcount[i] >= steps[i] )
									stepcount[i] = 0;
							}
						}
#endif
						pos[i] += delta[i];
					} else { /* PCM */
						d = rom[i][pos[i] >> BASE_SHIFT];

#if INTERPOLATE_SAMPLES
						if ( steps[i] ) {
							if ( ( pos[i] >> BASE_SHIFT ) < ( end[i] - 1 ) ) {
								signed char diff = rom[i][(pos[i] >> BASE_SHIFT) + 1];
								diff -= d;
								diff /= steps[i];

								d += ( diff * stepcount[i]++ );

								if ( stepcount[i] >= steps[i] )
									stepcount[i] = 0;
							}
						}
#endif

						pos[i] += delta[i];
					}

					if ( K053260_chip.mode & 2 ) {
						dataL += ( d * lvol[i] ) >> 2;
						dataR += ( d * rvol[i] ) >> 2;
					}
				}
			}

#ifndef clip_short
			buffer[1][j] = limit( dataL, MAXOUT, MINOUT );
			buffer[0][j] = limit( dataR, MAXOUT, MINOUT );
#else
			val=dataL;
			clip_short(val);
			buffer[1][j] = val;
			val=dataR;
			clip_short(val);
			buffer[0][j] = val;
#endif
		}

	/* update the regs now */
	for ( i = 0; i < 4; i++ ) {
		K053260_channel[i].pos = pos[i];
		K053260_channel[i].play = play[i];
		K053260_channel[i].ppcm_data = ppcm_data[i];
#if INTERPOLATE_SAMPLES
		K053260_channel[i].stepcount = stepcount[i];
#endif
	}
}

int K053260_sh_start(const struct MachineSound *msound) {
	const char *names[2];
	char ch_names[2][40];
	int i;

	/* Initialize our chip structure */
	K053260_chip.intf = (const struct K053260_interface *)msound->sound_interface;
	K053260_chip.mode = 0;
	K053260_chip.rom = memory_region(K053260_chip.intf->region);
	K053260_chip.rom_size = memory_region_length(K053260_chip.intf->region) - 1;

	K053260_reset();

	for ( i = 0; i < 0x30; i++ )
		K053260_chip.regs[i] = 0;

	delta_table = ( unsigned long * )malloc( 0x1000 * sizeof( unsigned long ) );

	if ( delta_table == 0 )
		return -1;

	for ( i = 0; i < 2; i++ ) {
		names[i] = ch_names[i];
		sprintf(ch_names[i],"%s Ch %d",sound_name(msound),i);
	}

	K053260_chip.channel = stream_init_multi( 2, names,
						K053260_chip.intf->mixing_level, Machine->sample_rate,
						0, K053260_update );

	InitDeltaTable();

	/* setup SH1 timer if necessary */
	if ( K053260_chip.intf->irq )
		K053260_chip.timer = timer_pulse( TIME_IN_HZ( ( K053260_chip.intf->clock / 32 ) ), 0, K053260_chip.intf->irq );
	else
		K053260_chip.timer = 0;

    return 0;
}

void K053260_sh_stop( void ) {
	if ( delta_table )
		free( delta_table );

	delta_table = 0;

	if ( K053260_chip.timer )
		timer_remove( K053260_chip.timer );

	K053260_chip.timer = 0;
}

INLINE void check_bounds( int channel ) {
	int channel_start = ( K053260_channel[channel].bank << 16 ) + K053260_channel[channel].start;
	int channel_end = channel_start + K053260_channel[channel].size - 1;

	if ( channel_start > K053260_chip.rom_size ) {
		logerror("K53260: Attempting to start playing past the end of the rom ( start = %06x, end = %06x ).\n", channel_start, channel_end );

		K053260_channel[channel].play = 0;

		return;
	}

	if ( channel_end > K053260_chip.rom_size ) {
		logerror("K53260: Attempting to play past the end of the rom ( start = %06x, end = %06x ).\n", channel_start, channel_end );

		K053260_channel[channel].size = K053260_chip.rom_size - channel_start;
	}
#if LOG
	logerror("K053260: Sample Start = %06x, Sample End = %06x, Sample rate = %04lx, PPCM = %s\n", channel_start, channel_end, K053260_channel[channel].rate, K053260_channel[channel].ppcm ? "yes" : "no" );
#endif
}

WRITE_HANDLER( K053260_w )
{
	int i, t;
	int r = offset;
	int v = data;

	if ( r > 0x2f ) {
		//logerror("K053260: Writing past registers\n" );
		return;
	}

#ifndef MAME_FASTSOUND
	if ( Machine->sample_rate != 0 )
		stream_update( K053260_chip.channel, 0 );
#else
    {
        extern int fast_sound;
        if ((!fast_sound) && ( Machine->sample_rate != 0 ))
		    stream_update( K053260_chip.channel, 0 );
    }
#endif

	/* before we update the regs, we need to check for a latched reg */
	if ( r == 0x28 ) {
		t = K053260_chip.regs[r] ^ v;

		for ( i = 0; i < 4; i++ ) {
			if ( t & ( 1 << i ) ) {
				if ( v & ( 1 << i ) ) {
					K053260_channel[i].play = 1;
					K053260_channel[i].pos = 0;
					K053260_channel[i].ppcm_data = 0;
					check_bounds( i );
#if INTERPOLATE_SAMPLES
					if ( delta_table[K053260_channel[i].rate] < ( 1 << BASE_SHIFT ) )
						K053260_channel[i].steps = ( 1 << BASE_SHIFT ) / delta_table[K053260_channel[i].rate];
					else
						K053260_channel[i].steps = 0;
					K053260_channel[i].stepcount = 0;
#endif
				} else
					K053260_channel[i].play = 0;
			}
		}

		K053260_chip.regs[r] = v;
		return;
	}

	/* update regs */
	K053260_chip.regs[r] = v;

	/* communication registers */
	if ( r < 8 )
		return;

	/* channel setup */
	if ( r < 0x28 ) {
		int channel = ( r - 8 ) / 8;

		switch ( ( r - 8 ) & 0x07 ) {
			case 0: /* sample rate low */
				K053260_channel[channel].rate &= 0x0f00;
				K053260_channel[channel].rate |= v;
			break;

			case 1: /* sample rate high */
				K053260_channel[channel].rate &= 0x00ff;
				K053260_channel[channel].rate |= ( v & 0x0f ) << 8;
			break;

			case 2: /* size low */
				K053260_channel[channel].size &= 0xff00;
				K053260_channel[channel].size |= v;
			break;

			case 3: /* size high */
				K053260_channel[channel].size &= 0x00ff;
				K053260_channel[channel].size |= v << 8;
			break;

			case 4: /* start low */
				K053260_channel[channel].start &= 0xff00;
				K053260_channel[channel].start |= v;
			break;

			case 5: /* start high */
				K053260_channel[channel].start &= 0x00ff;
				K053260_channel[channel].start |= v << 8;
			break;

			case 6: /* bank */
				K053260_channel[channel].bank = v & 0xff;
			break;

			case 7: /* volume is 7 bits. Convert to 8 bits now. */
				K053260_channel[channel].volume = ( ( v & 0x7f ) << 1 ) | ( v & 1 );
			break;
		}

		return;
	}

	switch( r ) {
		case 0x2a: /* loop, ppcm */
			for ( i = 0; i < 4; i++ )
				K053260_channel[i].loop = ( v & ( 1 << i ) ) != 0;

			for ( i = 4; i < 8; i++ )
				K053260_channel[i-4].ppcm = ( v & ( 1 << i ) ) != 0;
		break;

		case 0x2c: /* pan */
			K053260_channel[0].pan = v & 7;
			K053260_channel[1].pan = ( v >> 3 ) & 7;
		break;

		case 0x2d: /* more pan */
			K053260_channel[2].pan = v & 7;
			K053260_channel[3].pan = ( v >> 3 ) & 7;
		break;

		case 0x2f: /* control */
			K053260_chip.mode = v & 7;
			/* bit 0 = read ROM */
			/* bit 1 = enable sound output */
			/* bit 2 = unknown */
		break;
	}
}

READ_HANDLER( K053260_r )
{
	switch ( offset ) {
		case 0x29: /* channel status */
			{
				int i, status = 0;

				for ( i = 0; i < 4; i++ )
					status |= K053260_channel[i].play << i;

				return status;
			}
		break;

		case 0x2e: /* read rom */
			if ( K053260_chip.mode & 1 ) {
				unsigned long offs = K053260_channel[0].start + ( K053260_channel[0].pos >> BASE_SHIFT ) + ( K053260_channel[0].bank << 16 );

				K053260_channel[0].pos += ( 1 << 16 );

				if ( offs > K053260_chip.rom_size ) {
					logerror("K53260: Attempting to read past rom size on rom Read Mode.\n" );

					return 0;
				}

				return K053260_chip.rom[offs];
			}
		break;
	}

	return K053260_chip.regs[offset];
}
