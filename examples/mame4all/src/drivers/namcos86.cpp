#include "../vidhrdw/namcos86.cpp"

/*******************************************************************
Rolling Thunder
(C) 1986 Namco

To Do:
-----
Remove sprite lag (watch the "bullets" signs on the walls during scrolling).
  Increasing vblank_duration does it but some sprites flicker.

Add correct dipswitches and potentially fix controls in Wonder Momo.

Notes:
-----
PCM roms sample tables:
At the beggining of each PCM sound ROM you can find a 2 byte
offset to the beggining of each sample in the rom. Since the
table is not in sequential order, it is possible that the order
of the table is actually the sound number. Each sample ends in
a 0xff mark.

*******************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "cpu/m6800/m6800.h"

extern unsigned char *rthunder_videoram1, *rthunder_videoram2, *spriteram, *dirtybuffer;

/*******************************************************************/

void namcos86_vh_convert_color_prom(unsigned char *palette,unsigned short *colortable,const unsigned char *color_prom);
int namcos86_vh_start(void);
void namcos86_vh_screenrefresh(struct osd_bitmap *bitmap,int fullrefresh);
READ_HANDLER( rthunder_videoram1_r );
WRITE_HANDLER( rthunder_videoram1_w );
READ_HANDLER( rthunder_videoram2_r );
WRITE_HANDLER( rthunder_videoram2_w );
WRITE_HANDLER( rthunder_scroll0_w );
WRITE_HANDLER( rthunder_scroll1_w );
WRITE_HANDLER( rthunder_scroll2_w );
WRITE_HANDLER( rthunder_scroll3_w );
WRITE_HANDLER( rthunder_backcolor_w );
WRITE_HANDLER( rthunder_tilebank_select_0_w );
WRITE_HANDLER( rthunder_tilebank_select_1_w );



/*******************************************************************/

/* Sampled voices (Modified and Added by Takahiro Nogi. 1999/09/26) */

/* signed/unsigned 8-bit conversion macros */
#define AUDIO_CONV(A) ((A)^0x80)

static int rt_totalsamples[7];
static int rt_decode_mode;


static int rt_decode_sample(const struct MachineSound *msound)
{
	struct GameSamples *samples;
	unsigned char *src, *scan, *dest, last=0;
	int size, n = 0, j;
	int decode_mode;

	j = memory_region_length(REGION_SOUND1);
	if (j == 0) return 0;	/* no samples in this game */
	else if (j == 0x80000)	/* genpeitd */
		rt_decode_mode = 1;
	else
		rt_decode_mode = 0;

	//logerror("pcm decode mode:%d\n", rt_decode_mode );
	if (rt_decode_mode != 0) {
		decode_mode = 6;
	} else {
		decode_mode = 4;
	}

	/* get amount of samples */
	for ( j = 0; j < decode_mode; j++ ) {
		src = memory_region(REGION_SOUND1)+ ( j * 0x10000 );
		rt_totalsamples[j] = ( ( src[0] << 8 ) + src[1] ) / 2;
		n += rt_totalsamples[j];
		//logerror("rt_totalsamples[%d]:%d\n", j, rt_totalsamples[j] );
	}

	/* calculate the amount of headers needed */
	size = sizeof( struct GameSamples ) + n * sizeof( struct GameSamples * );

	/* allocate */
	if ( ( Machine->samples = (struct GameSamples*)malloc( size ) ) == NULL )
		return 1;

	samples = Machine->samples;
	samples->total = n;

	for ( n = 0; n < samples->total; n++ ) {
		int indx, start, offs;

		if ( n < rt_totalsamples[0] ) {
			src = memory_region(REGION_SOUND1);
			indx = n;
		} else
			if ( ( n - rt_totalsamples[0] ) < rt_totalsamples[1] ) {
				src = memory_region(REGION_SOUND1)+0x10000;
				indx = n - rt_totalsamples[0];
			} else
				if ( ( n - ( rt_totalsamples[0] + rt_totalsamples[1] ) ) < rt_totalsamples[2] ) {
					src = memory_region(REGION_SOUND1)+0x20000;
					indx = n - ( rt_totalsamples[0] + rt_totalsamples[1] );
				} else
					if ( ( n - ( rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] ) ) < rt_totalsamples[3] ) {
						src = memory_region(REGION_SOUND1)+0x30000;
						indx = n - ( rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] );
					} else
						if ( ( n - ( rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] ) ) < rt_totalsamples[4] ) {
							src = memory_region(REGION_SOUND1)+0x40000;
							indx = n - ( rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] );
						} else
							if ( ( n - ( rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] + rt_totalsamples[4] ) ) < rt_totalsamples[5] ) {
								src = memory_region(REGION_SOUND1)+0x50000;
								indx = n - ( rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] + rt_totalsamples[4] );
							} else {
								src = memory_region(REGION_SOUND1)+0x60000;
								indx = n - ( rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] + rt_totalsamples[4] + rt_totalsamples[5] );
							}

		/* calculate header offset */
		offs = indx * 2;

		/* get sample start offset */
		start = ( src[offs] << 8 ) + src[offs+1];

		/* calculate the sample size */
		scan = &src[start];
		size = 0;

		while ( *scan != 0xff ) {
			if ( *scan == 0x00 ) { /* run length encoded data start tag */
				/* get RLE size */
				size += scan[1] + 1;
				scan += 2;
			} else {
				size++;
				scan++;
			}
		}

		/* allocate sample */
		if ( ( samples->sample[n] = (struct GameSample*)malloc( sizeof( struct GameSample ) + size * sizeof( unsigned char ) ) ) == NULL )
			return 1;

		/* fill up the sample info */
		samples->sample[n]->length = size;
		samples->sample[n]->smpfreq = 6000;	/* 6 kHz */
		samples->sample[n]->resolution = 8;	/* 8 bit */

		/* unpack sample */
		dest = (unsigned char *)samples->sample[n]->data;
		scan = &src[start];

		while ( *scan != 0xff ) {
			if ( *scan == 0x00 ) { /* run length encoded data start tag */
				int i;
				for ( i = 0; i <= scan[1]; i++ ) /* unpack RLE */
					*dest++ = last;

				scan += 2;
			} else {
				last = AUDIO_CONV( scan[0] );
				*dest++ = last;
				scan++;
			}
		}
	}

	return 0; /* no errors */
}


/* play voice sample (Modified and Added by Takahiro Nogi. 1999/09/26) */
static int voice[2];

static void namco_voice_play( int offset, int data, int ch ) {

	if ( voice[ch] == -1 )
		sample_stop( ch );
	else
		sample_start( ch, voice[ch], 0 );
}

static WRITE_HANDLER( namco_voice0_play_w ) {

	namco_voice_play(offset, data, 0);
}

static WRITE_HANDLER( namco_voice1_play_w ) {

	namco_voice_play(offset, data, 1);
}

/* select voice sample (Modified and Added by Takahiro Nogi. 1999/09/26) */
static void namco_voice_select( int offset, int data, int ch ) {

	//logerror("Voice %d mode: %d select: %02x\n", ch, rt_decode_mode, data );

	if ( data == 0 )
		sample_stop( ch );

	if (rt_decode_mode != 0) {
		switch ( data & 0xe0 ) {
			case 0x00:
			break;

			case 0x20:
				data &= 0x1f;
				data += rt_totalsamples[0];
			break;

			case 0x40:
				data &= 0x1f;
				data += rt_totalsamples[0] + rt_totalsamples[1];
			break;

			case 0x60:
				data &= 0x1f;
				data += rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2];
			break;

			case 0x80:
				data &= 0x1f;
				data += rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3];
			break;

			case 0xa0:
				data &= 0x1f;
				data += rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] + rt_totalsamples[4];
			break;

			case 0xc0:
				data &= 0x1f;
				data += rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] + rt_totalsamples[4] + rt_totalsamples[5];
			break;

			case 0xe0:
				data &= 0x1f;
				data += rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2] + rt_totalsamples[3] + rt_totalsamples[4] + rt_totalsamples[5] + rt_totalsamples[6];
			break;
		}
	} else {
		switch ( data & 0xc0 ) {
			case 0x00:
			break;

			case 0x40:
				data &= 0x3f;
				data += rt_totalsamples[0];
			break;

			case 0x80:
				data &= 0x3f;
				data += rt_totalsamples[0] + rt_totalsamples[1];
			break;

			case 0xc0:
				data &= 0x3f;
				data += rt_totalsamples[0] + rt_totalsamples[1] + rt_totalsamples[2];
			break;
		}
	}

	voice[ch] = data - 1;
}

static WRITE_HANDLER( namco_voice0_select_w ) {

	namco_voice_select(offset, data, 0);
}

static WRITE_HANDLER( namco_voice1_select_w ) {

	namco_voice_select(offset, data, 1);
}
/*******************************************************************/

/* shared memory area with the mcu */
static unsigned char *shared1;
static READ_HANDLER( shared1_r ) { return shared1[offset]; }
static WRITE_HANDLER( shared1_w ) { shared1[offset] = data; }



static WRITE_HANDLER( spriteram_w )
{
	spriteram[offset] = data;
}
static READ_HANDLER( spriteram_r )
{
	return spriteram[offset];
}

static WRITE_HANDLER( bankswitch1_w )
{
	unsigned char *base = memory_region(REGION_CPU1) + 0x10000;

	/* if the ROM expansion module is available, don't do anything. This avoids conflict */
	/* with bankswitch1_ext_w() in wndrmomo */
	if (memory_region(REGION_USER1)) return;

	cpu_setbank(1,base + ((data & 0x03) * 0x2000));
}

static WRITE_HANDLER( bankswitch1_ext_w )
{
	unsigned char *base = memory_region(REGION_USER1);

	if (base == 0) return;

	cpu_setbank(1,base + ((data & 0x1f) * 0x2000));
}

static WRITE_HANDLER( bankswitch2_w )
{
	unsigned char *base = memory_region(REGION_CPU2) + 0x10000;

	cpu_setbank(2,base + ((data & 0x03) * 0x2000));
}

/* Stubs to pass the correct Dip Switch setup to the MCU */
static READ_HANDLER( dsw0_r )
{
	int rhi, rlo;

	rhi = ( readinputport( 2 ) & 0x01 ) << 4;
	rhi |= ( readinputport( 2 ) & 0x04 ) << 3;
	rhi |= ( readinputport( 2 ) & 0x10 ) << 2;
	rhi |= ( readinputport( 2 ) & 0x40 ) << 1;

	rlo = ( readinputport( 3 ) & 0x01 );
	rlo |= ( readinputport( 3 ) & 0x04 ) >> 1;
	rlo |= ( readinputport( 3 ) & 0x10 ) >> 2;
	rlo |= ( readinputport( 3 ) & 0x40 ) >> 3;

	return ~( rhi | rlo ) & 0xff; /* Active Low */
}

static READ_HANDLER( dsw1_r )
{
	int rhi, rlo;

	rhi = ( readinputport( 2 ) & 0x02 ) << 3;
	rhi |= ( readinputport( 2 ) & 0x08 ) << 2;
	rhi |= ( readinputport( 2 ) & 0x20 ) << 1;
	rhi |= ( readinputport( 2 ) & 0x80 );

	rlo = ( readinputport( 3 ) & 0x02 ) >> 1;
	rlo |= ( readinputport( 3 ) & 0x08 ) >> 2;
	rlo |= ( readinputport( 3 ) & 0x20 ) >> 3;
	rlo |= ( readinputport( 3 ) & 0x80 ) >> 4;

	return ~( rhi | rlo ) & 0xff; /* Active Low */
}

static int int_enabled[2];

static WRITE_HANDLER( int_ack1_w )
{
	int_enabled[0] = 1;
}

static WRITE_HANDLER( int_ack2_w )
{
	int_enabled[1] = 1;
}

static int namco86_interrupt1(void)
{
	if (int_enabled[0])
	{
		int_enabled[0] = 0;
		return interrupt();
	}

	return ignore_interrupt();
}

static int namco86_interrupt2(void)
{
	if (int_enabled[1])
	{
		int_enabled[1] = 0;
		return interrupt();
	}

	return ignore_interrupt();
}

static WRITE_HANDLER( namcos86_coin_w )
{
	coin_lockout_global_w(0,data & 1);
	coin_counter_w(0,~data & 2);
	coin_counter_w(1,~data & 4);
}

static WRITE_HANDLER( namcos86_led_w )
{
	osd_led_w(0,data >> 3);
	osd_led_w(1,data >> 4);
}


/*******************************************************************/

static struct MemoryReadAddress readmem1[] =
{
	{ 0x0000, 0x1fff, rthunder_videoram1_r },
	{ 0x2000, 0x3fff, rthunder_videoram2_r },
	{ 0x4000, 0x40ff, namcos1_wavedata_r }, /* PSG device, shared RAM */
	{ 0x4100, 0x413f, namcos1_sound_r }, /* PSG device, shared RAM */
	{ 0x4000, 0x43ff, shared1_r },
	{ 0x4400, 0x5fff, spriteram_r },
	{ 0x6000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem1[] =
{
	{ 0x0000, 0x1fff, rthunder_videoram1_w, &rthunder_videoram1 },
	{ 0x2000, 0x3fff, rthunder_videoram2_w, &rthunder_videoram2 },

	{ 0x4000, 0x40ff, namcos1_wavedata_w, &namco_wavedata }, /* PSG device, shared RAM */
	{ 0x4100, 0x413f, namcos1_sound_w, &namco_soundregs }, /* PSG device, shared RAM */
	{ 0x4000, 0x43ff, shared1_w, &shared1 },

	{ 0x4400, 0x5fff, spriteram_w, &spriteram },

	{ 0x6000, 0x6000, namco_voice0_play_w },
	{ 0x6200, 0x6200, namco_voice0_select_w },
	{ 0x6400, 0x6400, namco_voice1_play_w },
	{ 0x6600, 0x6600, namco_voice1_select_w },
	{ 0x6800, 0x6800, bankswitch1_ext_w },
//	{ 0x6c00, 0x6c00, MWA_NOP }, /* ??? */
//	{ 0x6e00, 0x6e00, MWA_NOP }, /* ??? */

	{ 0x8000, 0x8000, watchdog_reset_w },
	{ 0x8400, 0x8400, int_ack1_w }, /* IRQ acknowledge */
	{ 0x8800, 0x8800, rthunder_tilebank_select_0_w },
	{ 0x8c00, 0x8c00, rthunder_tilebank_select_1_w },

	{ 0x9000, 0x9002, rthunder_scroll0_w },	/* scroll + priority */
	{ 0x9003, 0x9003, bankswitch1_w },
	{ 0x9004, 0x9006, rthunder_scroll1_w },	/* scroll + priority */

	{ 0x9400, 0x9402, rthunder_scroll2_w },	/* scroll + priority */
//	{ 0x9403, 0x9403 } sub CPU rom bank select would be here
	{ 0x9404, 0x9406, rthunder_scroll3_w },	/* scroll + priority */

	{ 0xa000, 0xa000, rthunder_backcolor_w },

	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};


#define CPU2_MEMORY(NAME,ADDR_SPRITE,ADDR_VIDEO1,ADDR_VIDEO2,ADDR_ROM,ADDR_BANK,ADDR_WDOG,ADDR_INT)	\
static struct MemoryReadAddress NAME##_readmem2[] =									\
{																					\
	{ ADDR_SPRITE+0x0000, ADDR_SPRITE+0x03ff, MRA_RAM },							\
	{ ADDR_SPRITE+0x0400, ADDR_SPRITE+0x1fff, spriteram_r },						\
	{ ADDR_VIDEO1+0x0000, ADDR_VIDEO1+0x1fff, rthunder_videoram1_r },				\
	{ ADDR_VIDEO2+0x0000, ADDR_VIDEO2+0x1fff, rthunder_videoram2_r },				\
	{ ADDR_ROM+0x0000, ADDR_ROM+0x1fff, MRA_BANK2 },								\
	{ 0x8000, 0xffff, MRA_ROM },													\
	{ -1 }																			\
};																					\
static struct MemoryWriteAddress NAME##_writemem2[] =								\
{																					\
	{ ADDR_SPRITE+0x0000, ADDR_SPRITE+0x03ff, MWA_RAM },							\
	{ ADDR_SPRITE+0x0400, ADDR_SPRITE+0x1fff, spriteram_w },						\
	{ ADDR_VIDEO1+0x0000, ADDR_VIDEO1+0x1fff, rthunder_videoram1_w },				\
	{ ADDR_VIDEO2+0x0000, ADDR_VIDEO2+0x1fff, rthunder_videoram2_w },				\
/*	{ ADDR_BANK+0x00, ADDR_BANK+0x02 } layer 2 scroll registers would be here */	\
	{ ADDR_BANK+0x03, ADDR_BANK+0x03, bankswitch2_w },								\
/*	{ ADDR_BANK+0x04, ADDR_BANK+0x06 } layer 3 scroll registers would be here */	\
	{ ADDR_WDOG, ADDR_WDOG, watchdog_reset_w },										\
	{ ADDR_INT, ADDR_INT, int_ack2_w },	/* IRQ acknowledge */						\
	{ ADDR_ROM+0x0000, ADDR_ROM+0x1fff, MWA_ROM },									\
	{ 0x8000, 0xffff, MWA_ROM },													\
	{ -1 }																			\
};

#define UNUSED 0x4000
/*                     SPRITE  VIDEO1  VIDEO2  ROM     BANK    WDOG    IRQACK */
CPU2_MEMORY( hopmappy, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, 0x9000, UNUSED )
CPU2_MEMORY( skykiddx, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, 0x9000, 0x9400 )
CPU2_MEMORY( roishtar, 0x0000, 0x6000, 0x4000, UNUSED, UNUSED, 0xa000, 0xb000 )
CPU2_MEMORY( genpeitd, 0x4000, 0x0000, 0x2000, UNUSED, UNUSED, 0xb000, 0x8800 )
CPU2_MEMORY( rthunder, 0x0000, 0x2000, 0x4000, 0x6000, 0xd800, 0x8000, 0x8800 )
CPU2_MEMORY( wndrmomo, 0x2000, 0x4000, 0x6000, UNUSED, UNUSED, 0xc000, 0xc800 )
#undef UNUSED


#define MCU_MEMORY(NAME,ADDR_LOWROM,ADDR_INPUT,ADDR_UNK1,ADDR_UNK2)			\
static struct MemoryReadAddress NAME##_mcu_readmem[] =						\
{																			\
	{ 0x0000, 0x001f, hd63701_internal_registers_r },						\
	{ 0x0080, 0x00ff, MRA_RAM },											\
	{ 0x1000, 0x10ff, namcos1_wavedata_r }, /* PSG device, shared RAM */	\
	{ 0x1100, 0x113f, namcos1_sound_r }, /* PSG device, shared RAM */		\
	{ 0x1000, 0x13ff, shared1_r },											\
	{ 0x1400, 0x1fff, MRA_RAM },											\
	{ ADDR_INPUT+0x00, ADDR_INPUT+0x01, YM2151_status_port_0_r },			\
	{ ADDR_INPUT+0x20, ADDR_INPUT+0x20, input_port_0_r },					\
	{ ADDR_INPUT+0x21, ADDR_INPUT+0x21, input_port_1_r },					\
	{ ADDR_INPUT+0x30, ADDR_INPUT+0x30, dsw0_r },							\
	{ ADDR_INPUT+0x31, ADDR_INPUT+0x31, dsw1_r },							\
	{ ADDR_LOWROM, ADDR_LOWROM+0x3fff, MRA_ROM },							\
	{ 0x8000, 0xbfff, MRA_ROM },											\
	{ 0xf000, 0xffff, MRA_ROM },											\
	{ -1 } /* end of table */												\
};																			\
static struct MemoryWriteAddress NAME##_mcu_writemem[] =					\
{																			\
	{ 0x0000, 0x001f, hd63701_internal_registers_w },						\
	{ 0x0080, 0x00ff, MWA_RAM },											\
	{ 0x1000, 0x10ff, namcos1_wavedata_w }, /* PSG device, shared RAM */	\
	{ 0x1100, 0x113f, namcos1_sound_w }, /* PSG device, shared RAM */		\
	{ 0x1000, 0x13ff, shared1_w },											\
	{ 0x1400, 0x1fff, MWA_RAM },											\
	{ ADDR_INPUT+0x00, ADDR_INPUT+0x00, YM2151_register_port_0_w },			\
	{ ADDR_INPUT+0x01, ADDR_INPUT+0x01, YM2151_data_port_0_w },				\
	{ ADDR_UNK1, ADDR_UNK1, MWA_NOP }, /* ??? written (not always) at end of interrupt */	\
	{ ADDR_UNK2, ADDR_UNK2, MWA_NOP }, /* ??? written (not always) at end of interrupt */	\
	{ ADDR_LOWROM, ADDR_LOWROM+0x3fff, MWA_ROM },							\
	{ 0x8000, 0xbfff, MWA_ROM },											\
	{ 0xf000, 0xffff, MWA_ROM },											\
	{ -1 } /* end of table */												\
};

#define UNUSED 0x4000
/*                    LOWROM   INPUT    UNK1    UNK2 */
MCU_MEMORY( hopmappy, UNUSED, 0x2000, 0x8000, 0x8800 )
MCU_MEMORY( skykiddx, UNUSED, 0x2000, 0x8000, 0x8800 )
MCU_MEMORY( roishtar, 0x0000, 0x6000, 0x8000, 0x9800 )
MCU_MEMORY( genpeitd, 0x4000, 0x2800, 0xa000, 0xa800 )
MCU_MEMORY( rthunder, 0x4000, 0x2000, 0xb000, 0xb800 )
MCU_MEMORY( wndrmomo, 0x4000, 0x3800, 0xc000, 0xc800 )
#undef UNUSED


static struct IOReadPort mcu_readport[] =
{
	{ HD63701_PORT1, HD63701_PORT1, input_port_4_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort mcu_writeport[] =
{
	{ HD63701_PORT1, HD63701_PORT1, namcos86_coin_w },
	{ HD63701_PORT2, HD63701_PORT2, namcos86_led_w },
	{ -1 }	/* end of table */
};


/*******************************************************************/

INPUT_PORTS_START( hopmappy )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 2 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 2 player 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX( 0x80, 0x80, IPT_SERVICE, "Service Switch", KEYCODE_F1, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 1 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 2 player 2 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x04, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BITX(    0x10, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Select", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x80, "Hard" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin lockout */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
INPUT_PORTS_END

INPUT_PORTS_START( skykiddx )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 2 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX( 0x80, 0x80, IPT_SERVICE, "Service Switch", KEYCODE_F1, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 1 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_BITX(    0x08, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Level Select", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "20000 80000" )
	PORT_DIPSETTING(    0x00, "30000 90000" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0xc0, "5" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin lockout */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
INPUT_PORTS_END

INPUT_PORTS_START( roishtar )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 2 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON6 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 1 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin lockout */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON5 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_8WAY )
INPUT_PORTS_END

INPUT_PORTS_START( genpeitd )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 2 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX( 0x80, 0x80, IPT_SERVICE, "Service Switch", KEYCODE_F1, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 1 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x30, "Hardest" )
	PORT_DIPNAME( 0xc0, 0x00, "Candle" )
	PORT_DIPSETTING(    0x40, "40" )
	PORT_DIPSETTING(    0x00, "50" )
	PORT_DIPSETTING(    0x80, "60" )
	PORT_DIPSETTING(    0xc0, "70" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin lockout */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( rthunder )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 2 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX( 0x80, 0x80, IPT_SERVICE, "Service Switch", KEYCODE_F1, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 1 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_BITX(    0x08, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x00, "Continues" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Upright 1 Player" )
/*	PORT_DIPSETTING(    0x04, "Upright 1 Player" ) */
	PORT_DIPSETTING(    0x02, "Upright 2 Players" )
	PORT_DIPSETTING(    0x06, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x08, "Level Select" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Normal" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPNAME( 0x20, 0x20, "Timer value" )
	PORT_DIPSETTING(    0x00, "120 secs" )
	PORT_DIPSETTING(    0x20, "150 secs" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "70k, 200k" )
	PORT_DIPSETTING(    0x40, "100k, 300k" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x80, "5" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin lockout */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( rthundro )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 2 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX( 0x80, 0x80, IPT_SERVICE, "Service Switch", KEYCODE_F1, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 1 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_BITX(    0x08, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0xc0, "5" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin lockout */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( wndrmomo )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 2 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BITX( 0x80, 0x80, IPT_SERVICE, "Service Switch", KEYCODE_F1, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 player 1 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Level Select" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Type A" )
	PORT_DIPSETTING(    0x02, "Type B" )
	PORT_DIPSETTING(    0x04, "Type C" )
//	PORT_DIPSETTING(    0x06, "Type A" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin lockout */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 1 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* OUT:coin counter 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_PLAYER2 )
INPUT_PORTS_END


/*******************************************************************/

#define TILELAYOUT(NUM) static struct GfxLayout tilelayout_##NUM =  \
{                                                                   \
	8,8,	/* 8*8 characters */                                    \
	NUM,	/* NUM characters */                                    \
	3,	/* 3 bits per pixel */                                      \
	{ 2*NUM*8*8, NUM*8*8, 0 },                                      \
	{ 0, 1, 2, 3, 4, 5, 6, 7 },                                     \
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },                     \
	8*8	/* every char takes 8 consecutive bytes */                  \
}

TILELAYOUT(1024);
TILELAYOUT(2048);
TILELAYOUT(4096);

#define SPRITELAYOUT(NUM) static struct GfxLayout spritelayout_##NUM =         \
{																			   \
	16,16,	/* 16*16 sprites */												   \
	NUM,	/* NUM sprites */												   \
	4,	/* 4 bitss per pixel */												   \
	{ 0, 1, 2, 3 },															   \
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,								   \
			8*4, 9*4, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4 },					   \
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,						   \
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },			   \
	16*64																	   \
}

SPRITELAYOUT(256);
SPRITELAYOUT(512);
SPRITELAYOUT(1024);


#define GFXDECODE(CHAR1,CHAR2,SPRITE)										\
static struct GfxDecodeInfo gfxdecodeinfo_##CHAR1##_##CHAR2##_##SPRITE[] =	\
{																			\
	{ REGION_GFX1, 0x00000,      &tilelayout_##CHAR1,    2048*0, 256 },		\
	{ REGION_GFX2, 0x00000,      &tilelayout_##CHAR2,    2048*0, 256 },		\
	{ REGION_GFX3, 0*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ REGION_GFX3, 1*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ REGION_GFX3, 2*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ REGION_GFX3, 3*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ REGION_GFX3, 4*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ REGION_GFX3, 5*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ REGION_GFX3, 6*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ REGION_GFX3, 7*128*SPRITE, &spritelayout_##SPRITE, 2048*1, 128 },		\
	{ -1 }																	\
};

GFXDECODE(1024,1024, 256)
GFXDECODE(2048,2048, 256)
GFXDECODE(2048,2048, 512)
GFXDECODE(4096,2048, 512)
GFXDECODE(4096,2048,1024)

/*******************************************************************/

static struct YM2151interface ym2151_interface =
{
	1,                      /* 1 chip */
	3579580,                /* 3.579580 MHz ? */
	{ YM3012_VOL(0,MIXER_PAN_CENTER,60,MIXER_PAN_CENTER) },	/* only right channel is connected */
	{ 0 },
	{ 0 }
};

static struct namco_interface namco_interface =
{
	49152000/2048, 		/* 24000Hz */
	8,		/* number of voices */
	50,     /* playback volume */
	-1,		/* memory region */
	0		/* stereo */
};

static struct Samplesinterface samples_interface =
{
	2,	/* 2 channels for voice effects */
	40	/* volume */
};

static struct CustomSound_interface custom_interface =
{
	rt_decode_sample,
	0,
	0
};


static void namco86_init_machine( void )
{
	unsigned char *base = memory_region(REGION_CPU1) + 0x10000;

	cpu_setbank(1,base);

	int_enabled[0] = int_enabled[1] = 1;
}


#define MACHINE_DRIVER(NAME,GFX)												\
static struct MachineDriver machine_driver_##NAME =								\
{																				\
	{																			\
		{																		\
			CPU_M6809,															\
			6000000/4,		/* ? */												\
			/*49152000/32, rthunder doesn't work with this */					\
			readmem1,writemem1,0,0,												\
			namco86_interrupt1,1												\
		},																		\
		{																		\
			CPU_M6809,															\
			49152000/32, 		/* ? */											\
			NAME##_readmem2,NAME##_writemem2,0,0,								\
			namco86_interrupt2,1												\
		},																		\
		{																		\
			CPU_HD63701,	/* or compatible 6808 with extra instructions */	\
			49152000/32, 		/* ? */											\
			NAME##_mcu_readmem,NAME##_mcu_writemem,mcu_readport,mcu_writeport,	\
			interrupt, 1	/* ??? */											\
		}																		\
	},																			\
	60, DEFAULT_60HZ_VBLANK_DURATION,											\
	100, /* cpu slices */														\
	namco86_init_machine, /* init machine */									\
																				\
	/* video hardware */														\
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },									\
	gfxdecodeinfo_##GFX,														\
	512,4096,																	\
	namcos86_vh_convert_color_prom,												\
																				\
	VIDEO_TYPE_RASTER,															\
	0,																			\
	namcos86_vh_start,															\
	0,																			\
	namcos86_vh_screenrefresh,													\
																				\
	/* sound hardware */														\
	0,0,0,0,																	\
	{																			\
		{																		\
			SOUND_YM2151,														\
			&ym2151_interface													\
		},																		\
		{																		\
			SOUND_NAMCO,														\
			&namco_interface													\
		},																		\
		{																		\
			SOUND_SAMPLES,														\
			&samples_interface													\
		},																		\
		{																		\
			SOUND_CUSTOM,	/* actually initializes the samples */				\
			&custom_interface													\
		}																		\
	}																			\
};


MACHINE_DRIVER( hopmappy, 1024_1024_256 )
MACHINE_DRIVER( skykiddx, 2048_2048_256 )
MACHINE_DRIVER( roishtar, 1024_1024_256 )
MACHINE_DRIVER( genpeitd, 4096_2048_1024 )
MACHINE_DRIVER( rthunder, 4096_2048_512 )
MACHINE_DRIVER( wndrmomo, 2048_2048_512 )


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( hopmappy )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "hm1",         0x08000, 0x8000, 0x1a83914e )
	/* 9d empty */

	/* the CPU1 ROM expansion board is not present in this game */

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "hm2",         0xc000, 0x4000, 0xc46cda65 )
	/* 12d empty */

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hm6",         0x00000, 0x04000, 0xfd0e8887 )	/* plane 1,2 */
	/* no plane 3 */

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hm5",         0x00000, 0x04000, 0x9c4f31ae )	/* plane 1,2 */
	/* no plane 3 */

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hm4",         0x00000, 0x8000, 0x78719c52 )
	/* 12k/l/m/p/r/t/u empty */

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "hm11.bpr",    0x0000, 0x0200, 0xcc801088 )	/* red & green components */
	ROM_LOAD( "hm12.bpr",    0x0200, 0x0200, 0xa1cb71c5 )	/* blue component */
	ROM_LOAD( "hm13.bpr",    0x0400, 0x0800, 0xe362d613 )	/* tiles colortable */
	ROM_LOAD( "hm14.bpr",    0x0c00, 0x0800, 0x678252b4 )	/* sprites colortable */
	ROM_LOAD( "hm15.bpr",    0x1400, 0x0020, 0x475bf500 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "hm3",         0x08000, 0x2000, 0x6496e1db )
	ROM_LOAD( "pl1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	/* the PCM expansion board is not present in this game */
ROM_END

ROM_START( skykiddx )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "sk3_1b.9c", 0x08000, 0x8000, 0x767b3514 )
	ROM_LOAD( "sk3_2.9d",  0x10000, 0x8000, 0x74b8f8e2 )

	/* the CPU1 ROM expansion board is not present in this game */

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "sk3_3.12c", 0x8000, 0x8000, 0x6d1084c4 )
	/* 12d empty */

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk3_9.7r",  0x00000, 0x08000, 0x48675b17 )	/* plane 1,2 */
	ROM_LOAD( "sk3_10.7s", 0x08000, 0x04000, 0x7418465a )	/* plane 3 */

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk3_7.4r",  0x00000, 0x08000, 0x4036b735 )	/* plane 1,2 */
	ROM_LOAD( "sk3_8.4s",  0x08000, 0x04000, 0x044bfd21 )	/* plane 3 */

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk3_5.12h",  0x00000, 0x8000, 0x5c7d4399 )
	ROM_LOAD( "sk3_6.12k",  0x08000, 0x8000, 0xc908a3b2 )
	/* 12l/m/p/r/t/u empty */

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "sk3-1.3r", 0x0000, 0x0200, 0x9e81dedd )	/* red & green components */
	ROM_LOAD( "sk3-2.3s", 0x0200, 0x0200, 0xcbfec4dd )	/* blue component */
	ROM_LOAD( "sk3-3.4v", 0x0400, 0x0800, 0x81714109 )	/* tiles colortable */
	ROM_LOAD( "sk3-4.5v", 0x0c00, 0x0800, 0x1bf25acc )	/* sprites colortable */
	ROM_LOAD( "sk3-5.6u", 0x1400, 0x0020, 0xe4130804 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "sk3_4.6b",    0x08000, 0x4000, 0xe6cae2d6 )
	ROM_LOAD( "rt1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	/* the PCM expansion board is not present in this game */
ROM_END

ROM_START( skykiddo )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "sk3-1.9c",  0x08000, 0x8000, 0x5722a291 )
	ROM_LOAD( "sk3_2.9d",  0x10000, 0x8000, 0x74b8f8e2 )

	/* the CPU1 ROM expansion board is not present in this game */

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "sk3_3.12c", 0x8000, 0x8000, 0x6d1084c4 )
	/* 12d empty */

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk3_9.7r",  0x00000, 0x08000, 0x48675b17 )	/* plane 1,2 */
	ROM_LOAD( "sk3_10.7s", 0x08000, 0x04000, 0x7418465a )	/* plane 3 */

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk3_7.4r",  0x00000, 0x08000, 0x4036b735 )	/* plane 1,2 */
	ROM_LOAD( "sk3_8.4s",  0x08000, 0x04000, 0x044bfd21 )	/* plane 3 */

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk3_5.12h",  0x00000, 0x8000, 0x5c7d4399 )
	ROM_LOAD( "sk3_6.12k",  0x08000, 0x8000, 0xc908a3b2 )
	/* 12l/m/p/r/t/u empty */

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "sk3-1.3r", 0x0000, 0x0200, 0x9e81dedd )	/* red & green components */
	ROM_LOAD( "sk3-2.3s", 0x0200, 0x0200, 0xcbfec4dd )	/* blue component */
	ROM_LOAD( "sk3-3.4v", 0x0400, 0x0800, 0x81714109 )	/* tiles colortable */
	ROM_LOAD( "sk3-4.5v", 0x0c00, 0x0800, 0x1bf25acc )	/* sprites colortable */
	ROM_LOAD( "sk3-5.6u", 0x1400, 0x0020, 0xe4130804 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "sk3_4.6b",    0x08000, 0x4000, 0xe6cae2d6 )
	ROM_LOAD( "rt1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	/* the PCM expansion board is not present in this game */
ROM_END

ROM_START( roishtar )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "ri1-1c.9c", 0x08000, 0x8000, 0x14acbacb )
	ROM_LOAD( "ri1-2.9d",  0x14000, 0x2000, 0xfcd58d91 )

	/* the CPU1 ROM expansion board is not present in this game */

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "ri1-3.12c", 0x8000, 0x8000, 0xa39829f7 )
	/* 12d empty */

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ri1-14.7r", 0x00000, 0x04000, 0xde8154b4 )	/* plane 1,2 */
	ROM_LOAD( "ri1-15.7s", 0x04000, 0x02000, 0x4298822b )	/* plane 3 */

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ri1-12.4r", 0x00000, 0x04000, 0x557e54d3 )	/* plane 1,2 */
	ROM_LOAD( "ri1-13.4s", 0x04000, 0x02000, 0x9ebe8e32 )	/* plane 3 */

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ri1-5.12h",  0x00000, 0x8000, 0x46b59239 )
	ROM_LOAD( "ri1-6.12k",  0x08000, 0x8000, 0x94d9ef48 )
	ROM_LOAD( "ri1-7.12l",  0x10000, 0x8000, 0xda802b59 )
	ROM_LOAD( "ri1-8.12m",  0x18000, 0x8000, 0x16b88b74 )
	ROM_LOAD( "ri1-9.12p",  0x20000, 0x8000, 0xf3de3c2a )
	ROM_LOAD( "ri1-10.12r", 0x28000, 0x8000, 0x6dacc70d )
	ROM_LOAD( "ri1-11.12t", 0x30000, 0x8000, 0xfb6bc533 )
	/* 12u empty */

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "ri1-1.3r", 0x0000, 0x0200, 0x29cd0400 )	/* red & green components */
	ROM_LOAD( "ri1-2.3s", 0x0200, 0x0200, 0x02fd278d )	/* blue component */
	ROM_LOAD( "ri1-3.4v", 0x0400, 0x0800, 0xcbd7e53f )	/* tiles colortable */
	ROM_LOAD( "ri1-4.5v", 0x0c00, 0x0800, 0x22921617 )	/* sprites colortable */
	ROM_LOAD( "ri1-5.6u", 0x1400, 0x0020, 0xe2188075 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "ri1-4.6b",    0x00000, 0x4000, 0x552172b8 )
	ROM_CONTINUE(            0x08000, 0x4000 )
	ROM_LOAD( "rt1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	/* the PCM expansion board is not present in this game */
ROM_END

ROM_START( genpeitd )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "gt1-1b.9c", 0x08000, 0x8000, 0x75396194 )
	/* 9d empty */

	ROM_REGION( 0x40000, REGION_USER1 ) /* bank switched data for CPU1 */
	ROM_LOAD( "gt1-10b.f1",  0x00000, 0x10000, 0x5721ad0d )
	/* h1 empty */
	/* k1 empty */
	/* m1 empty */

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "gt1-2.12c", 0xc000, 0x4000, 0x302f2cb6 )
	/* 12d empty */

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gt1-7.7r", 0x00000, 0x10000, 0xea77a211 )	/* plane 1,2 */
	ROM_LOAD( "gt1-6.7s", 0x10000, 0x08000, 0x1b128a2e )	/* plane 3 */

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gt1-5.4r", 0x00000, 0x08000, 0x44d58b06 )	/* plane 1,2 */
	ROM_LOAD( "gt1-4.4s", 0x08000, 0x04000, 0xdb8d45b0 )	/* plane 3 */

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gt1-11.12h",  0x00000, 0x20000, 0x3181a5fe )
	ROM_LOAD( "gt1-12.12k",  0x20000, 0x20000, 0x76b729ab )
	ROM_LOAD( "gt1-13.12l",  0x40000, 0x20000, 0xe332a36e )
	ROM_LOAD( "gt1-14.12m",  0x60000, 0x20000, 0xe5ffaef5 )
	ROM_LOAD( "gt1-15.12p",  0x80000, 0x20000, 0x198b6878 )
	ROM_LOAD( "gt1-16.12r",  0xa0000, 0x20000, 0x801e29c7 )
	ROM_LOAD( "gt1-8.12t",   0xc0000, 0x10000, 0xad7bc770 )
	ROM_LOAD( "gt1-9.12u",   0xe0000, 0x10000, 0xd95a5fd7 )

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "gt1-1.3r", 0x0000, 0x0200, 0x2f0ddddb )	/* red & green components */
	ROM_LOAD( "gt1-2.3s", 0x0200, 0x0200, 0x87d27025 )	/* blue component */
	ROM_LOAD( "gt1-3.4v", 0x0400, 0x0800, 0xc178de99 )	/* tiles colortable */
	ROM_LOAD( "gt1-4.5v", 0x0c00, 0x0800, 0x9f48ef17 )	/* sprites colortable */
	ROM_LOAD( "gt1-5.6u", 0x1400, 0x0020, 0xe4130804 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "gt1-3.6b",    0x04000, 0x8000, 0x315cd988 )
	ROM_LOAD( "rt1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	ROM_REGION( 0x80000, REGION_SOUND1 ) /* PCM samples for Hitachi CPU */
	ROM_LOAD( "gt1-17.f3",  0x00000, 0x20000, 0x26181ff8 )
	ROM_LOAD( "gt1-18.h3",  0x20000, 0x20000, 0x7ef9e5ea )
	ROM_LOAD( "gt1-19.k3",  0x40000, 0x20000, 0x38e11f6c )
	/* m3 empty */
ROM_END

ROM_START( rthunder )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "rt3-1b.9c",  0x8000, 0x8000, 0x7d252a1b )
	/* 9d empty */

	ROM_REGION( 0x40000, REGION_USER1 ) /* bank switched data for CPU1 */
	ROM_LOAD( "rt1-17.f1",  0x00000, 0x10000, 0x766af455 )
	ROM_LOAD( "rt1-18.h1",  0x10000, 0x10000, 0x3f9f2f5d )
	ROM_LOAD( "rt1-19.k1",  0x20000, 0x10000, 0xc16675e9 )
	ROM_LOAD( "rt1-20.m1",  0x30000, 0x10000, 0xc470681b )

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "rt3-2b.12c", 0x08000, 0x8000, 0xa7ea46ee )
	ROM_LOAD( "rt3-3.12d",  0x10000, 0x8000, 0xa13f601c )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rt1-7.7r",  0x00000, 0x10000, 0xa85efa39 )	/* plane 1,2 */
	ROM_LOAD( "rt1-8.7s",  0x10000, 0x08000, 0xf7a95820 )	/* plane 3 */

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rt1-5.4r",  0x00000, 0x08000, 0xd0fc470b )	/* plane 1,2 */
	ROM_LOAD( "rt1-6.4s",  0x08000, 0x04000, 0x6b57edb2 )	/* plane 3 */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rt1-9.12h",  0x00000, 0x10000, 0x8e070561 )
	ROM_LOAD( "rt1-10.12k", 0x10000, 0x10000, 0xcb8fb607 )
	ROM_LOAD( "rt1-11.12l", 0x20000, 0x10000, 0x2bdf5ed9 )
	ROM_LOAD( "rt1-12.12m", 0x30000, 0x10000, 0xe6c6c7dc )
	ROM_LOAD( "rt1-13.12p", 0x40000, 0x10000, 0x489686d7 )
	ROM_LOAD( "rt1-14.12r", 0x50000, 0x10000, 0x689e56a8 )
	ROM_LOAD( "rt1-15.12t", 0x60000, 0x10000, 0x1d8bf2ca )
	ROM_LOAD( "rt1-16.12u", 0x70000, 0x10000, 0x1bbcf37b )

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "mb7124e.3r", 0x0000, 0x0200, 0x8ef3bb9d )	/* red & green components */
	ROM_LOAD( "mb7116e.3s", 0x0200, 0x0200, 0x6510a8f2 )	/* blue component */
	ROM_LOAD( "mb7138h.4v", 0x0400, 0x0800, 0x95c7d944 )	/* tiles colortable */
	ROM_LOAD( "mb7138h.6v", 0x0c00, 0x0800, 0x1391fec9 )	/* sprites colortable */
	ROM_LOAD( "mb7112e.6u", 0x1400, 0x0020, 0xe4130804 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "rt1-4.6b",    0x04000, 0x8000, 0x00cf293f )
	ROM_LOAD( "rt1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* PCM samples for Hitachi CPU */
	ROM_LOAD( "rt1-21.f3",  0x00000, 0x10000, 0x454968f3 )
	ROM_LOAD( "rt1-22.h3",  0x10000, 0x10000, 0xfe963e72 )
	/* k3 empty */
	/* m3 empty */
ROM_END

ROM_START( rthundro )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "r1",         0x8000, 0x8000, 0x6f8c1252 )
	/* 9d empty */

	ROM_REGION( 0x40000, REGION_USER1 ) /* bank switched data for CPU1 */
	ROM_LOAD( "rt1-17.f1",  0x00000, 0x10000, 0x766af455 )
	ROM_LOAD( "rt1-18.h1",  0x10000, 0x10000, 0x3f9f2f5d )
	ROM_LOAD( "r19",        0x20000, 0x10000, 0xfe9343b0 )
	ROM_LOAD( "r20",        0x30000, 0x10000, 0xf8518d4f )

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "r2",        0x08000, 0x8000, 0xf22a03d8 )
	ROM_LOAD( "r3",        0x10000, 0x8000, 0xaaa82885 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rt1-7.7r",  0x00000, 0x10000, 0xa85efa39 )	/* plane 1,2 */
	ROM_LOAD( "rt1-8.7s",  0x10000, 0x08000, 0xf7a95820 )	/* plane 3 */

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rt1-5.4r",  0x00000, 0x08000, 0xd0fc470b )	/* plane 1,2 */
	ROM_LOAD( "rt1-6.4s",  0x08000, 0x04000, 0x6b57edb2 )	/* plane 3 */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rt1-9.12h",  0x00000, 0x10000, 0x8e070561 )
	ROM_LOAD( "rt1-10.12k", 0x10000, 0x10000, 0xcb8fb607 )
	ROM_LOAD( "rt1-11.12l", 0x20000, 0x10000, 0x2bdf5ed9 )
	ROM_LOAD( "rt1-12.12m", 0x30000, 0x10000, 0xe6c6c7dc )
	ROM_LOAD( "rt1-13.12p", 0x40000, 0x10000, 0x489686d7 )
	ROM_LOAD( "rt1-14.12r", 0x50000, 0x10000, 0x689e56a8 )
	ROM_LOAD( "rt1-15.12t", 0x60000, 0x10000, 0x1d8bf2ca )
	ROM_LOAD( "rt1-16.12u", 0x70000, 0x10000, 0x1bbcf37b )

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "mb7124e.3r", 0x0000, 0x0200, 0x8ef3bb9d )	/* red & green components */
	ROM_LOAD( "mb7116e.3s", 0x0200, 0x0200, 0x6510a8f2 )	/* blue component */
	ROM_LOAD( "mb7138h.4v", 0x0400, 0x0800, 0x95c7d944 )	/* tiles colortable */
	ROM_LOAD( "mb7138h.6v", 0x0c00, 0x0800, 0x1391fec9 )	/* sprites colortable */
	ROM_LOAD( "mb7112e.6u", 0x1400, 0x0020, 0xe4130804 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "r4",          0x04000, 0x8000, 0x0387464f )
	ROM_LOAD( "rt1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* PCM samples for Hitachi CPU */
	ROM_LOAD( "rt1-21.f3",  0x00000, 0x10000, 0x454968f3 )
	ROM_LOAD( "rt1-22.h3",  0x10000, 0x10000, 0xfe963e72 )
	/* k3 empty */
	/* m3 empty */
ROM_END

ROM_START( wndrmomo )
	ROM_REGION( 0x18000, REGION_CPU1 )
	ROM_LOAD( "wm1-1.9c", 0x8000, 0x8000, 0x34b50bf0 )
	/* 9d empty */

	ROM_REGION( 0x40000, REGION_USER1 ) /* bank switched data for CPU1 */
	ROM_LOAD( "wm1-16.f1", 0x00000, 0x10000, 0xe565f8f3 )
	/* h1 empty */
	/* k1 empty */
	/* m1 empty */

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "wm1-2.12c", 0x8000, 0x8000, 0x3181efd0 )
	/* 12d empty */

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "wm1-6.7r", 0x00000, 0x08000, 0x93955fbb )	/* plane 1,2 */
	ROM_LOAD( "wm1-7.7s", 0x08000, 0x04000, 0x7d662527 )	/* plane 3 */

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "wm1-4.4r", 0x00000, 0x08000, 0xbbe67836 )	/* plane 1,2 */
	ROM_LOAD( "wm1-5.4s", 0x08000, 0x04000, 0xa81b481f )	/* plane 3 */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "wm1-8.12h",  0x00000, 0x10000, 0x14f52e72 )
	ROM_LOAD( "wm1-9.12k",  0x10000, 0x10000, 0x16f8cdae )
	ROM_LOAD( "wm1-10.12l", 0x20000, 0x10000, 0xbfbc1896 )
	ROM_LOAD( "wm1-11.12m", 0x30000, 0x10000, 0xd775ddb2 )
	ROM_LOAD( "wm1-12.12p", 0x40000, 0x10000, 0xde64c12f )
	ROM_LOAD( "wm1-13.12r", 0x50000, 0x10000, 0xcfe589ad )
	ROM_LOAD( "wm1-14.12t", 0x60000, 0x10000, 0x2ae21a53 )
	ROM_LOAD( "wm1-15.12u", 0x70000, 0x10000, 0xb5c98be0 )

	ROM_REGION( 0x1420, REGION_PROMS )
	ROM_LOAD( "wm1-1.3r", 0x0000, 0x0200, 0x1af8ade8 )	/* red & green components */
	ROM_LOAD( "wm1-2.3s", 0x0200, 0x0200, 0x8694e213 )	/* blue component */
	ROM_LOAD( "wm1-3.4v", 0x0400, 0x0800, 0x2ffaf9a4 )	/* tiles colortable */
	ROM_LOAD( "wm1-4.5v", 0x0c00, 0x0800, 0xf4e83e0b )	/* sprites colortable */
	ROM_LOAD( "wm1-5.6u", 0x1400, 0x0020, 0xe4130804 )	/* tile address decoder (used at runtime) */

	ROM_REGION( 0x10000, REGION_CPU3 )
	ROM_LOAD( "wm1-3.6b",    0x04000, 0x8000, 0x55f01df7 )
	ROM_LOAD( "rt1-mcu.bin", 0x0f000, 0x1000, 0x6ef08fb3 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* PCM samples for Hitachi CPU */
	ROM_LOAD( "wm1-17.f3", 0x00000, 0x10000, 0xbea3c318 )
	ROM_LOAD( "wm1-18.h3", 0x10000, 0x10000, 0x6d73bcc5 )
	ROM_LOAD( "wm1-19.k3", 0x20000, 0x10000, 0xd288e912 )
	ROM_LOAD( "wm1-20.m3", 0x30000, 0x10000, 0x076a72cb )
ROM_END



static void init_namco86(void)
{
	int size;
	unsigned char *gfx;
	unsigned char *buffer;

	/* shuffle tile ROMs so regular gfx unpack routines can be used */
	gfx = memory_region(REGION_GFX1);
	size = memory_region_length(REGION_GFX1) * 2 / 3;
	buffer = (unsigned char*)malloc( size );

	if ( buffer )
	{
		unsigned char *dest1 = gfx;
		unsigned char *dest2 = gfx + ( size / 2 );
		unsigned char *mono = gfx + size;
		int i;

		memcpy( buffer, gfx, size );

		for ( i = 0; i < size; i += 2 )
		{
			unsigned char data1 = buffer[i];
			unsigned char data2 = buffer[i+1];
			*dest1++ = ( data1 << 4 ) | ( data2 & 0xf );
			*dest2++ = ( data1 & 0xf0 ) | ( data2 >> 4 );

			*mono ^= 0xff; mono++;
		}

		free( buffer );
	}

	gfx = memory_region(REGION_GFX2);
	size = memory_region_length(REGION_GFX2) * 2 / 3;
	buffer = (unsigned char*)malloc( size );

	if ( buffer )
	{
		unsigned char *dest1 = gfx;
		unsigned char *dest2 = gfx + ( size / 2 );
		unsigned char *mono = gfx + size;
		int i;

		memcpy( buffer, gfx, size );

		for ( i = 0; i < size; i += 2 )
		{
			unsigned char data1 = buffer[i];
			unsigned char data2 = buffer[i+1];
			*dest1++ = ( data1 << 4 ) | ( data2 & 0xf );
			*dest2++ = ( data1 & 0xf0 ) | ( data2 >> 4 );

			*mono ^= 0xff; mono++;
		}

		free( buffer );
	}
}



WRITE_HANDLER( roishtar_semaphore_w )
{
    rthunder_videoram1_w(0x7e24-0x6000+offset,data);

    if (data == 0x02)
	    cpu_spinuntil_int();
}

static void init_roishtar(void)
{
	/* install hook to avoid hang at game over */
    install_mem_write_handler(1, 0x7e24, 0x7e24, roishtar_semaphore_w);

	init_namco86();
}



GAME( 1986, hopmappy, 0,        hopmappy, hopmappy, namco86,  ROT0,   "Namco", "Hopping Mappy" )
GAME( 1986, skykiddx, 0,        skykiddx, skykiddx, namco86,  ROT180, "Namco", "Sky Kid Deluxe (set 1)" )
GAME( 1986, skykiddo, skykiddx, skykiddx, skykiddx, namco86,  ROT180, "Namco", "Sky Kid Deluxe (set 2)" )
GAME( 1986, roishtar, 0,        roishtar, roishtar, roishtar, ROT0,   "Namco", "The Return of Ishtar" )
GAME( 1986, genpeitd, 0,        genpeitd, genpeitd, namco86,  ROT0,   "Namco", "Genpei ToumaDen" )
GAME( 1986, rthunder, 0,        rthunder, rthunder, namco86,  ROT0,   "Namco", "Rolling Thunder (new version)" )
GAME( 1986, rthundro, rthunder, rthunder, rthundro, namco86,  ROT0,   "Namco", "Rolling Thunder (old version)" )
GAME( 1987, wndrmomo, 0,        wndrmomo, wndrmomo, namco86,  ROT0,   "Namco", "Wonder Momo" )
