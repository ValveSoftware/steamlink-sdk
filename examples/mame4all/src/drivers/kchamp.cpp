#include "../vidhrdw/kchamp.cpp"

/***************************************************************************

Karate Champ - (c) 1984 Data East

driver by Ernesto Corvi


Changes:
(1999/11/11 Takahiro Nogi)
Changed "karatedo" to "karatevs".
Supported "karatedo".
Corrected DIPSW settings.(kchamp/karatedo)

Currently supported sets:
Karate Champ - 1 Player Version (kchamp)
Karate Champ - VS Version (kchampvs)
Karate Champ - 1 Player Version Japanese (karatedo)
Karate Champ - VS Version Japanese (karatevs)

VS Version Info:
---------------
Memory Map:
Main CPU
0000-bfff ROM (encrypted)
c000-cfff RAM
d000-d3ff char videoram
d400-d7ff color videoram
d800-d8ff sprites
e000-ffff ROM (encrypted)

Sound CPU
0000-5fff ROM
6000-6300 RAM

IO Ports:
Main CPU
INPUT  00 = Player 1 Controls - ( ACTIVE LOW )
INPUT  40 = Player 2 Controls - ( ACTIVE LOW )
INPUT  80 = Coins and Start Buttons - ( ACTIVE LOW )
INPUT  C0 = Dip Switches - ( ACTIVE LOW )
OUTPUT 00 = Screen Flip? (There isnt a cocktail switch?) UNINMPLEMENTED
OUTPUT 01 = CPU Control
                bit 0 = external nmi enable
OUTPUT 02 = Sound Reset
OUTPUT 40 = Sound latch write

Sound CPU
INPUT  01 = Sound latch read
OUTPUT 00 = AY8910 #1 data write
OUTPUT 01 = AY8910 #1 control write
OUTPUT 02 = AY8910 #2 data write
OUTPUT 03 = AY8910 #2 control write
OUTPUT 04 = MSM5205 write
OUTPUT 05 = CPU Control
                bit 0 = MSM5205 trigger
                bit 1 = external nmi enable

1P Version Info:
---------------
Same as VS version but with a DAC instead of a MSM5205. Also some minor
IO ports and memory map changes. Dip switches differ too.

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


/* from vidhrdw */
extern void kchamp_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
extern void kchamp_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
extern int kchampvs_vh_start(void);
extern int kchamp1p_vh_start(void);


static int nmi_enable = 0;
static int sound_nmi_enable = 0;

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, MRA_RAM },
	{ 0xd000, 0xd3ff, videoram_r },
	{ 0xd400, 0xd7ff, colorram_r },
	{ 0xd800, 0xd8ff, spriteram_r },
	{ 0xd900, 0xdfff, MRA_RAM },
	{ 0xe000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd3ff, videoram_w, &videoram, &videoram_size },
	{ 0xd400, 0xd7ff, colorram_w, &colorram },
	{ 0xd800, 0xd8ff, spriteram_w, &spriteram, &spriteram_size },
	{ 0xd900, 0xdfff, MWA_RAM },
	{ 0xe000, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0xffff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0xffff, MWA_RAM },
	{ -1 }
};

static WRITE_HANDLER( control_w ) {
	nmi_enable = data & 1;
}

static WRITE_HANDLER( sound_reset_w ) {
	if ( !( data & 1 ) )
		cpu_set_reset_line(1,PULSE_LINE);
}

static WRITE_HANDLER( sound_control_w ) {
	MSM5205_reset_w( 0, !( data & 1 ) );
	sound_nmi_enable = ( ( data >> 1 ) & 1 );
}

static WRITE_HANDLER( sound_command_w ) {
	soundlatch_w( 0, data );
	cpu_cause_interrupt( 1, 0xff );
}

static int msm_data = 0;
static int msm_play_lo_nibble = 1;

static WRITE_HANDLER( sound_msm_w ) {
	msm_data = data;
	msm_play_lo_nibble = 1;
}

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r }, /* Player 1 controls - ACTIVE LOW */
	{ 0x40, 0x40, input_port_1_r }, /* Player 2 controls - ACTIVE LOW */
	{ 0x80, 0x80, input_port_2_r }, /* Coins & Start - ACTIVE LOW */
	{ 0xC0, 0xC0, input_port_3_r }, /* Dipswitch */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, MWA_NOP },
	{ 0x01, 0x01, control_w },
	{ 0x02, 0x02, sound_reset_w },
	{ 0x40, 0x40, sound_command_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x01, 0x01, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_write_port_0_w },
	{ 0x01, 0x01, AY8910_control_port_0_w },
	{ 0x02, 0x02, AY8910_write_port_1_w },
	{ 0x03, 0x03, AY8910_control_port_1_w },
	{ 0x04, 0x04, sound_msm_w },
	{ 0x05, 0x05, sound_control_w },
	{ -1 }	/* end of table */
};

/********************
* 1 Player Version  *
********************/

static struct MemoryReadAddress kc_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe3ff, videoram_r },
	{ 0xe400, 0xe7ff, colorram_r },
	{ 0xea00, 0xeaff, spriteram_r },
	{ 0xeb00, 0xffff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress kc_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe3ff, videoram_w, &videoram, &videoram_size },
	{ 0xe400, 0xe7ff, colorram_w, &colorram },
	{ 0xea00, 0xeaff, spriteram_w, &spriteram, &spriteram_size },
	{ 0xeb00, 0xffff, MWA_RAM },
	{ -1 }
};

static struct MemoryReadAddress kc_sound_readmem[] =
{
	{ 0x0000, 0xdfff, MRA_ROM },
	{ 0xe000, 0xe2ff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress kc_sound_writemem[] =
{
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe2ff, MWA_RAM },
	{ -1 }
};

static READ_HANDLER( sound_reset_r ) {
	cpu_set_reset_line(1,PULSE_LINE);
	return 0;
}

static WRITE_HANDLER( kc_sound_control_w ) {
	if ( offset == 0 )
		sound_nmi_enable = ( ( data >> 7 ) & 1 );
//	else
//		DAC_set_volume(0,( data == 1 ) ? 255 : 0,0);
}

static struct IOReadPort kc_readport[] =
{
	{ 0x90, 0x90, input_port_0_r }, /* Player 1 controls - ACTIVE LOW */
	{ 0x98, 0x98, input_port_1_r }, /* Player 2 controls - ACTIVE LOW */
	{ 0xa0, 0xa0, input_port_2_r }, /* Coins & Start - ACTIVE LOW */
	{ 0x80, 0x80, input_port_3_r }, /* Dipswitch */
	{ 0xa8, 0xa8, sound_reset_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort kc_writeport[] =
{
	{ 0x80, 0x80, MWA_NOP },
	{ 0x81, 0x81, control_w },
	{ 0xa8, 0xa8, sound_command_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort kc_sound_readport[] =
{
	{ 0x06, 0x06, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort kc_sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_write_port_0_w },
	{ 0x01, 0x01, AY8910_control_port_0_w },
	{ 0x02, 0x02, AY8910_write_port_1_w },
	{ 0x03, 0x03, AY8910_control_port_1_w },
	{ 0x04, 0x04, DAC_0_data_w },
	{ 0x05, 0x05, kc_sound_control_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( kchampvs )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_4WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_PLAYER2 | IPF_4WAY )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, "Easy" )
	PORT_DIPSETTING(    0x20, "Medium" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


/********************
* 1 Player Version  *
********************/

INPUT_PORTS_START( kchamp )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_4WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_PLAYER2 | IPF_4WAY )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )
INPUT_PORTS_END

static struct GfxLayout tilelayout =
{
	8,8,	/* tile size */
	256*8,	/* number of tiles */
	2,	/* bits per pixel */
	{ 0x4000*8, 0 }, /* plane offsets */
	{ 0,1,2,3,4,5,6,7 }, /* x offsets */
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 }, /* y offsets */
	8*8	/* offset to next tile */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* tile size */
	512,	/* number of tiles */
	2,	/* bits per pixel */
	{ 0xC000*8, 0 }, /* plane offsets */
	{ 0,1,2,3,4,5,6,7,
		0x2000*8+0,0x2000*8+1,0x2000*8+2,0x2000*8+3,
		0x2000*8+4,0x2000*8+5,0x2000*8+6,0x2000*8+7 }, /* x offsets */
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
	8*8,9*8,10*8,11*8,12*8,13*8,14*8, 15*8 }, /* y offsets */
	16*8	/* ofset to next tile */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &tilelayout,	32*4, 32 },
	{ REGION_GFX2, 0x08000, &spritelayout,	0, 16 },
	{ REGION_GFX2, 0x04000, &spritelayout,	0, 16 },
	{ REGION_GFX2, 0x00000, &spritelayout,	0, 16 },
	{ -1 }
};



static int kc_interrupt( void ) {

	if ( nmi_enable )
		return Z80_NMI_INT;

	return Z80_IGNORE_INT;
}

static void msmint( int data ) {

	static int counter = 0;

	if ( msm_play_lo_nibble )
		MSM5205_data_w( 0, msm_data & 0x0f );
	else
		MSM5205_data_w( 0, ( msm_data >> 4 ) & 0x0f );

	msm_play_lo_nibble ^= 1;

	if ( !( counter ^= 1 ) ) {
		if ( sound_nmi_enable ) {
			cpu_cause_interrupt( 1, Z80_NMI_INT );
		}
	}
}

static struct AY8910interface ay8910_interface =
{
	2, /* 2 chips */
	1500000,			/* 12 Mhz / 8 = 1.5 Mhz */
	{ 30, 30 },			// Modified by T.Nogi 1999/11/08
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MSM5205interface msm_interface =
{
	1,				/* 1 chip */
	375000,				/* 12Mhz / 16 / 2 */
	{ msmint },			/* interrupt function */
	{ MSM5205_S96_4B },		/* 1 / 96 = 3906.25Hz playback */
	{ 100 }
};

/********************
* 1 Player Version  *
********************/

static int sound_int( void ) {

	if ( sound_nmi_enable )
		return Z80_NMI_INT;

	return Z80_IGNORE_INT;
}

static struct DACinterface dac_interface =
{
	1,
	{ 50 }
};


static struct MachineDriver machine_driver_kchampvs =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3000000,	/* 12Mhz / 4 = 3.0 Mhz */
			readmem,writemem,readport,writeport,
			kc_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,	/* 12Mhz / 4 = 3.0 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt, 0
			/* irq's triggered from main cpu */
			/* nmi's from msm5205 */
		}
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,	/* Interleaving forced by interrupts */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, /* number of colors */
	256, /* color table length */
	kchamp_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	kchampvs_vh_start,
	generic_vh_stop,
	kchamp_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
		{
			SOUND_MSM5205,
			&msm_interface
		}
	}
};

/********************
* 1 Player Version  *
********************/

static struct MachineDriver machine_driver_kchamp =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3000000,	/* 12Mhz / 4 = 3.0 Mhz */
			kc_readmem, kc_writemem, kc_readport, kc_writeport,
			kc_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,	/* 12Mhz / 4 = 3.0 Mhz */
			kc_sound_readmem,kc_sound_writemem,kc_sound_readport,kc_sound_writeport,
			ignore_interrupt, 0,
			sound_int, 125 /* Hz */
			/* irq's triggered from main cpu */
			/* nmi's from 125 Hz clock */
		}
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,	/* Interleaving forced by interrupts */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, /* number of colors */
	256, /* color table length */
	kchamp_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	kchamp1p_vh_start,
	generic_vh_stop,
	kchamp_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( kchamp )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "b014.bin", 0x0000, 0x2000, 0x0000d1a0 )
	ROM_LOAD( "b015.bin", 0x2000, 0x2000, 0x03fae67e )
	ROM_LOAD( "b016.bin", 0x4000, 0x2000, 0x3b6e1d08 )
	ROM_LOAD( "b017.bin", 0x6000, 0x2000, 0xc1848d1a )
	ROM_LOAD( "b018.bin", 0x8000, 0x2000, 0xb824abc7 )
	ROM_LOAD( "b019.bin", 0xa000, 0x2000, 0x3b487a46 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "b026.bin", 0x0000, 0x2000, 0x999ed2c7 )
	ROM_LOAD( "b025.bin", 0x2000, 0x2000, 0x33171e07 ) /* adpcm */
	ROM_LOAD( "b024.bin", 0x4000, 0x2000, 0x910b48b9 ) /* adpcm */
	ROM_LOAD( "b023.bin", 0x6000, 0x2000, 0x47f66aac )
	ROM_LOAD( "b022.bin", 0x8000, 0x2000, 0x5928e749 )
	ROM_LOAD( "b021.bin", 0xa000, 0x2000, 0xca17e3ba )
	ROM_LOAD( "b020.bin", 0xc000, 0x2000, 0xada4f2cd )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b000.bin", 0x00000, 0x2000, 0xa4fa98a1 )  /* plane0 */ /* tiles */
	ROM_LOAD( "b001.bin", 0x04000, 0x2000, 0xfea09f7c )  /* plane1 */ /* tiles */

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b013.bin", 0x00000, 0x2000, 0xeaad4168 )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b004.bin", 0x02000, 0x2000, 0x10a47e2d )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, 0xb4842ea9 )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, 0x8cd166a5 )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, 0x4cbd3aa3 )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, 0x6be342a6 )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b007.bin", 0x0c000, 0x2000, 0xcb91d16b )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b010.bin", 0x0e000, 0x2000, 0x489c9c04 )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, 0x7346db8a )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, 0xb78714fc )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, 0xb2557102 )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, 0xc85aba0e )  /* bot, plane1 */ /* sprites */

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "br27", 0x0000, 0x0100, 0xf683c54a ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, 0x3ddbb6c4 ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, 0xba4a5651 ) /* blue */
ROM_END

ROM_START( karatedo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "be14", 0x0000, 0x2000, 0x44e60aa0 )
	ROM_LOAD( "be15", 0x2000, 0x2000, 0xa65e3793 )
	ROM_LOAD( "be16", 0x4000, 0x2000, 0x151d8872 )
	ROM_LOAD( "be17", 0x6000, 0x2000, 0x8f393b6a )
	ROM_LOAD( "be18", 0x8000, 0x2000, 0xa09046ad )
	ROM_LOAD( "be19", 0xa000, 0x2000, 0x0cdc4da9 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "be26", 0x0000, 0x2000, 0x999ab0a3 )
	ROM_LOAD( "be25", 0x2000, 0x2000, 0x253bf0da ) /* adpcm */
	ROM_LOAD( "be24", 0x4000, 0x2000, 0xe2c188af ) /* adpcm */
	ROM_LOAD( "be23", 0x6000, 0x2000, 0x25262de1 )
	ROM_LOAD( "be22", 0x8000, 0x2000, 0x38055c48 )
	ROM_LOAD( "be21", 0xa000, 0x2000, 0x5f0efbe7 )
	ROM_LOAD( "be20", 0xc000, 0x2000, 0xcbe8a533 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "be00",     0x00000, 0x2000, 0xcec020f2 )  /* plane0 */ /* tiles */
	ROM_LOAD( "be01",     0x04000, 0x2000, 0xcd96271c )  /* plane1 */ /* tiles */

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "be13",     0x00000, 0x2000, 0xfb358707 )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "be04",     0x02000, 0x2000, 0x48372bf8 )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, 0xb4842ea9 )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, 0x8cd166a5 )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, 0x4cbd3aa3 )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, 0x6be342a6 )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "be07",     0x0c000, 0x2000, 0x40f2b6fb )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "be10",     0x0e000, 0x2000, 0x325c0a97 )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, 0x7346db8a )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, 0xb78714fc )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, 0xb2557102 )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, 0xc85aba0e )  /* bot, plane1 */ /* sprites */

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "br27", 0x0000, 0x0100, 0xf683c54a ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, 0x3ddbb6c4 ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, 0xba4a5651 ) /* blue */
ROM_END

ROM_START( kchampvs )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "bs24", 0x0000, 0x2000, 0x829da69b )
	ROM_LOAD( "bs23", 0x2000, 0x2000, 0x091f810e )
	ROM_LOAD( "bs22", 0x4000, 0x2000, 0xd4df2a52 )
	ROM_LOAD( "bs21", 0x6000, 0x2000, 0x3d4ef0da )
	ROM_LOAD( "bs20", 0x8000, 0x2000, 0x623a467b )
	ROM_LOAD( "bs19", 0xa000, 0x2000, 0x43e196c4 )
	ROM_CONTINUE(     0xe000, 0x2000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "bs18", 0x0000, 0x2000, 0xeaa646eb )
	ROM_LOAD( "bs17", 0x2000, 0x2000, 0xd71031ad ) /* adpcm */
	ROM_LOAD( "bs16", 0x4000, 0x2000, 0x6f811c43 ) /* adpcm */

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bs12",     0x00000, 0x2000, 0x4c574ecd )
	ROM_LOAD( "bs13",     0x02000, 0x2000, 0x750b66af )
	ROM_LOAD( "bs14",     0x04000, 0x2000, 0x9ad6227c )
	ROM_LOAD( "bs15",     0x06000, 0x2000, 0x3b6d5de5 )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bs00",     0x00000, 0x2000, 0x51eda56c )
	ROM_LOAD( "bs06",     0x02000, 0x2000, 0x593264cf )
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, 0xb4842ea9 )  /* bs01 */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, 0x8cd166a5 )  /* bs07 */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, 0x4cbd3aa3 )  /* bs02 */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, 0x6be342a6 )  /* bs08 */
	ROM_LOAD( "bs03",     0x0c000, 0x2000, 0x8dcd271a )
	ROM_LOAD( "bs09",     0x0e000, 0x2000, 0x4ee1dba7 )
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, 0x7346db8a )  /* bs04 */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, 0xb78714fc )  /* bs10 */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, 0xb2557102 )  /* bs05 */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, 0xc85aba0e )  /* bs11 */

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "br27", 0x0000, 0x0100, 0xf683c54a ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, 0x3ddbb6c4 ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, 0xba4a5651 ) /* blue */
ROM_END

ROM_START( karatevs )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "br24", 0x0000, 0x2000, 0xea9cda49 )
	ROM_LOAD( "br23", 0x2000, 0x2000, 0x46074489 )
	ROM_LOAD( "br22", 0x4000, 0x2000, 0x294f67ba )
	ROM_LOAD( "br21", 0x6000, 0x2000, 0x934ea874 )
	ROM_LOAD( "br20", 0x8000, 0x2000, 0x97d7816a )
	ROM_LOAD( "br19", 0xa000, 0x2000, 0xdd2239d2 )
	ROM_CONTINUE(     0xe000, 0x2000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "br18", 0x0000, 0x2000, 0x00ccb8ea )
	ROM_LOAD( "bs17", 0x2000, 0x2000, 0xd71031ad ) /* adpcm */
	ROM_LOAD( "br16", 0x4000, 0x2000, 0x2512d961 ) /* adpcm */

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "br12",     0x00000, 0x2000, 0x9ed6f00d )
	ROM_LOAD( "bs13",     0x02000, 0x2000, 0x750b66af )
	ROM_LOAD( "br14",     0x04000, 0x2000, 0xfc399229 )
	ROM_LOAD( "bs15",     0x06000, 0x2000, 0x3b6d5de5 )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "br00",     0x00000, 0x2000, 0xc46a8b88 )
	ROM_LOAD( "br06",     0x02000, 0x2000, 0xcf8982ff )
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, 0xb4842ea9 )  /* bs01 */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, 0x8cd166a5 )  /* bs07 */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, 0x4cbd3aa3 )  /* bs02 */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, 0x6be342a6 )  /* bs08 */
	ROM_LOAD( "br03",     0x0c000, 0x2000, 0xbde8a52b )
	ROM_LOAD( "br09",     0x0e000, 0x2000, 0xe9a5f945 )
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, 0x7346db8a )  /* bs04 */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, 0xb78714fc )  /* bs10 */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, 0xb2557102 )  /* bs05 */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, 0xc85aba0e )  /* bs11 */

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "br27", 0x0000, 0x0100, 0xf683c54a ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, 0x3ddbb6c4 ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, 0xba4a5651 ) /* blue */
ROM_END



static void init_kchampvs( void )
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;
	int A;


	memory_set_opcode_base(0,rom+diff);

	for (A = 0;A < 0x10000;A++)
		rom[A+diff] = (rom[A] & 0x55) | ((rom[A] & 0x88) >> 2) | ((rom[A] & 0x22) << 2);

	/*
		Note that the first 4 opcodes that the program
		executes aren't encrypted for some obscure reason.
		The address for the 2nd opcode (a jump) is encrypted too.
		It's not clear what the 3rd and 4th opcode are supposed to do,
		they just write to a RAM location. This write might be what
		turns the encryption on, but this doesn't explain the
		encrypted address for the jump.
	 */
	rom[0+diff] = rom[0];	/* this is a jump */
	A = rom[1] + 256 * rom[2];
	rom[A+diff] = rom[A];	/* fix opcode on first jump address (again, a jump) */
	rom[A+1] ^= 0xee;		/* fix address of the second jump */
	A = rom[A+1] + 256 * rom[A+2];
	rom[A+diff] = rom[A];	/* fix third opcode (ld a,$xx) */
	A += 2;
	rom[A+diff] = rom[A];	/* fix fourth opcode (ld ($xxxx),a */
	/* and from here on, opcodes are encrypted */
}



GAMEX( 1984, kchamp,   0,      kchamp, kchamp,     0,        ROT90, "Data East USA", "Karate Champ (US)", GAME_NO_COCKTAIL )
GAMEX( 1984, karatedo, kchamp, kchamp, kchamp,     0,        ROT90, "Data East Corporation", "Karate Dou (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1984, kchampvs, kchamp, kchampvs, kchampvs, kchampvs, ROT90, "Data East USA", "Karate Champ (US VS version)", GAME_NO_COCKTAIL )
GAMEX( 1984, karatevs, kchamp, kchampvs, kchampvs, kchampvs, ROT90, "Data East Corporation", "Taisen Karate Dou (Japan VS version)", GAME_NO_COCKTAIL )
