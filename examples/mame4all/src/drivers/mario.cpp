#include "../vidhrdw/mario.cpp"
#include "../sndhrdw/mario.cpp"

/***************************************************************************

Mario Bros memory map (preliminary):

driver by Mirko Buffoni


0000-5fff ROM
6000-6fff RAM
7000-73ff ?
7400-77ff Video RAM
f000-ffff ROM

read:
7c00      IN0
7c80      IN1
7f80      DSW

*
 * IN0 (bits NOT inverted)
 * bit 7 : TEST
 * bit 6 : START 2
 * bit 5 : START 1
 * bit 4 : JUMP player 1
 * bit 3 : ? DOWN player 1 ?
 * bit 2 : ? UP player 1 ?
 * bit 1 : LEFT player 1
 * bit 0 : RIGHT player 1
 *
*
 * IN1 (bits NOT inverted)
 * bit 7 : ?
 * bit 6 : COIN 2
 * bit 5 : COIN 1
 * bit 4 : JUMP player 2
 * bit 3 : ? DOWN player 2 ?
 * bit 2 : ? UP player 2 ?
 * bit 1 : LEFT player 2
 * bit 0 : RIGHT player 2
 *
*
 * DSW (bits NOT inverted)
 * bit 7 : \ difficulty
 * bit 6 : / 00 = easy  01 = medium  10 = hard  11 = hardest
 * bit 5 : \ bonus
 * bit 4 : / 00 = 20000  01 = 30000  10 = 40000  11 = none
 * bit 3 : \ coins per play
 * bit 2 : /
 * bit 1 : \ 00 = 3 lives  01 = 4 lives
 * bit 0 : / 10 = 5 lives  11 = 6 lives
 *

write:
7d00      vertical scroll (pow)
7d80      ?
7e00      sound
7e80-7e82 ?
7e83      sprite palette bank select
7e84      interrupt enable
7e85      ?
7f00-7f07 sound triggers


I/O ports

write:
00        ?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/i8039/i8039.h"

static int p[8] = { 0,0xf0,0,0,0,0,0,0 };
static int t[2] = { 0,0 };


extern unsigned char *mario_scrolly;

WRITE_HANDLER( mario_gfxbank_w );
WRITE_HANDLER( mario_palettebank_w );
int  mario_vh_start(void);
void mario_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void mario_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/*
 *  from sndhrdw/mario.c
 */
WRITE_HANDLER( mario_sh_w );
WRITE_HANDLER( mario_sh1_w );
WRITE_HANDLER( mario_sh2_w );
WRITE_HANDLER( mario_sh3_w );


#define ACTIVELOW_PORT_BIT(P,A,D)   ((P & (~(1 << A))) | ((D ^ 1) << A))
#define ACTIVEHIGH_PORT_BIT(P,A,D)   ((P & (~(1 << A))) | (D << A))


WRITE_HANDLER( mario_sh_getcoin_w )    { t[0] = data; }
WRITE_HANDLER( mario_sh_crab_w )       { p[1] = ACTIVEHIGH_PORT_BIT(p[1],0,data); }
WRITE_HANDLER( mario_sh_turtle_w )     { p[1] = ACTIVEHIGH_PORT_BIT(p[1],1,data); }
WRITE_HANDLER( mario_sh_fly_w )        { p[1] = ACTIVEHIGH_PORT_BIT(p[1],2,data); }
static WRITE_HANDLER( mario_sh_tuneselect_w ) { soundlatch_w(offset,data); }

static READ_HANDLER( mario_sh_p1_r )   { return p[1]; }
static READ_HANDLER( mario_sh_p2_r )   { return p[2]; }
static READ_HANDLER( mario_sh_t0_r )   { return t[0]; }
static READ_HANDLER( mario_sh_t1_r )   { return t[1]; }
static READ_HANDLER( mario_sh_tune_r ) { return soundlatch_r(offset); }

static WRITE_HANDLER( mario_sh_sound_w )
{
	DAC_data_w(0,data);
}
static WRITE_HANDLER( mario_sh_p1_w )
{
	p[1] = data;
}
static WRITE_HANDLER( mario_sh_p2_w )
{
	p[2] = data;
}
WRITE_HANDLER( masao_sh_irqtrigger_w )
{
	static int last;


	if (last == 1 && data == 0)
	{
		/* setting bit 0 high then low triggers IRQ on the sound CPU */
		cpu_cause_interrupt(1,0xff);
	}

	last = data;
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6fff, MRA_RAM },
	{ 0x7400, 0x77ff, MRA_RAM },	/* video RAM */
	{ 0x7c00, 0x7c00, input_port_0_r },	/* IN0 */
	{ 0x7c80, 0x7c80, input_port_1_r },	/* IN1 */
	{ 0x7f80, 0x7f80, input_port_2_r },	/* DSW */
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x7400, 0x77ff, videoram_w, &videoram, &videoram_size },
	{ 0x7c00, 0x7c00, mario_sh1_w }, /* Mario run sample */
	{ 0x7c80, 0x7c80, mario_sh2_w }, /* Luigi run sample */
	{ 0x7d00, 0x7d00, MWA_RAM, &mario_scrolly },
	{ 0x7e80, 0x7e80, mario_gfxbank_w },
	{ 0x7e83, 0x7e83, mario_palettebank_w },
	{ 0x7e84, 0x7e84, interrupt_enable_w },
	{ 0x7f00, 0x7f00, mario_sh_w },	/* death */
	{ 0x7f01, 0x7f01, mario_sh_getcoin_w },
	{ 0x7f03, 0x7f03, mario_sh_crab_w },
	{ 0x7f04, 0x7f04, mario_sh_turtle_w },
	{ 0x7f05, 0x7f05, mario_sh_fly_w },
	{ 0x7f00, 0x7f07, mario_sh3_w }, /* Misc discrete samples */
	{ 0x7e00, 0x7e00, mario_sh_tuneselect_w },
	{ 0x7000, 0x73ff, MWA_NOP },	/* ??? */
//	{ 0x7e85, 0x7e85, MWA_RAM },	/* Sets alternative 1 and 0 */
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress masao_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x7400, 0x77ff, videoram_w, &videoram, &videoram_size },
	{ 0x7d00, 0x7d00, MWA_RAM, &mario_scrolly },
	{ 0x7e00, 0x7e00, soundlatch_w },
	{ 0x7e80, 0x7e80, mario_gfxbank_w },
	{ 0x7e83, 0x7e83, mario_palettebank_w },
	{ 0x7e84, 0x7e84, interrupt_enable_w },
	{ 0x7000, 0x73ff, MWA_NOP },	/* ??? */
	{ 0x7f00, 0x7f00, masao_sh_irqtrigger_w },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct IOWritePort mario_writeport[] =
{
	{ 0x00,   0x00,   IOWP_NOP },  /* unknown... is this a trigger? */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ -1 }	/* end of table */
};
static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ -1 }	/* end of table */
};
static struct IOReadPort readport_sound[] =
{
	{ 0x00,     0xff,     mario_sh_tune_r },
	{ I8039_p1, I8039_p1, mario_sh_p1_r },
	{ I8039_p2, I8039_p2, mario_sh_p2_r },
	{ I8039_t0, I8039_t0, mario_sh_t0_r },
	{ I8039_t1, I8039_t1, mario_sh_t1_r },
	{ -1 }	/* end of table */
};
static struct IOWritePort writeport_sound[] =
{
	{ 0x00,     0xff,     mario_sh_sound_w },
	{ I8039_p1, I8039_p1, mario_sh_p1_w },
	{ I8039_p2, I8039_p2, mario_sh_p2_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( mario )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x10, "30000" )
	PORT_DIPSETTING(    0x20, "40000" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0xc0, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( mariojp )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )	/* doesn't work in game, but does in service mode */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x1c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x20, 0x20, "2 Players Game" )
	PORT_DIPSETTING(    0x00, "1 Credit" )
	PORT_DIPSETTING(    0x20, "2 Credits" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x40, "30000" )
	PORT_DIPSETTING(    0x80, "40000" )
	PORT_DIPSETTING(    0xc0, "None" )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 512*8*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};


static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	3,	/* 3 bits per pixel */
	{ 2*256*16*16, 256*16*16, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,		/* the two halves of the sprite are separated */
			256*16*8+0, 256*16*8+1, 256*16*8+2, 256*16*8+3, 256*16*8+4, 256*16*8+5, 256*16*8+6, 256*16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,      0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 16*4, 32 },
	{ -1 } /* end of array */
};



static struct DACinterface dac_interface =
{
	1,
	{ 100 }
};

static const char *mario_sample_names[] =
{
	"*mario",

	/* 7f01 - 7f07 sounds */
	"ice.wav",    /* 0x02 ice appears (formerly effect0.wav) */
	"coin.wav",   /* 0x06 coin appears (formerly effect1.wav) */
	"skid.wav",   /* 0x07 skid */

	/* 7c00 */
	"run.wav",        /* 03, 02, 01 - 0x1b */

	/* 7c80 */
	"luigirun.wav",   /* 03, 02, 01 - 0x1c */

    0	/* end of array */
};

static struct Samplesinterface samples_interface =
{
	3,	/* 3 channels */
	25,	/* volume */
	mario_sample_names
};

static struct AY8910interface ay8910_interface =
{
	1,      /* 1 chip */
	14318000/6,	/* ? */
	{ 50 },
	{ soundlatch_r },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MemoryReadAddress masao_sound_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x4000, 0x4000, AY8910_read_port_0_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress masao_sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x6000, 0x6000, AY8910_control_port_0_w },
	{ 0x4000, 0x4000, AY8910_write_port_0_w },
	{ -1 }  /* end of table */
};


static struct MachineDriver machine_driver_mario =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz (?) */
			readmem,writemem,0,mario_writeport,
			nmi_interrupt,1
		},
		{
			CPU_I8039 | CPU_AUDIO_CPU,
                        730000,         /* 730 khz */
			readmem_sound,writemem_sound,readport_sound,writeport_sound,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,16*4+32*8,
	mario_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	mario_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		},
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	}
};

static struct MachineDriver machine_driver_masao =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4.000 Mhz (?) */
			readmem,masao_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			24576000/16,	/* ???? */
			masao_sound_readmem,masao_sound_writemem,0,0,
			ignore_interrupt,1
		}

		},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,16*4+32*8,
	mario_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	mario_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( mario )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "mario.7f",     0x0000, 0x2000, 0xc0c6e014 )
	ROM_LOAD( "mario.7e",     0x2000, 0x2000, 0x116b3856 )
	ROM_LOAD( "mario.7d",     0x4000, 0x2000, 0xdcceb6c1 )
	ROM_LOAD( "mario.7c",     0xf000, 0x1000, 0x4a63d96b )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "tma1c-a.6k",   0x0000, 0x1000, 0x06b9ff85 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mario.3f",     0x0000, 0x1000, 0x28b0c42c )
	ROM_LOAD( "mario.3j",     0x1000, 0x1000, 0x0c8cc04d )

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mario.7m",     0x0000, 0x1000, 0x22b7372e )
	ROM_LOAD( "mario.7n",     0x1000, 0x1000, 0x4f3a1f47 )
	ROM_LOAD( "mario.7p",     0x2000, 0x1000, 0x56be6ccd )
	ROM_LOAD( "mario.7s",     0x3000, 0x1000, 0x56f1d613 )
	ROM_LOAD( "mario.7t",     0x4000, 0x1000, 0x641f0008 )
	ROM_LOAD( "mario.7u",     0x5000, 0x1000, 0x7baf5309 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "mario.4p",     0x0000, 0x0200, 0xafc9bd41 )
ROM_END

ROM_START( mariojp )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "tma1c-a1.7f",  0x0000, 0x2000, 0xb64b6330 )
	ROM_LOAD( "tma1c-a2.7e",  0x2000, 0x2000, 0x290c4977 )
	ROM_LOAD( "tma1c-a1.7d",  0x4000, 0x2000, 0xf8575f31 )
	ROM_LOAD( "tma1c-a2.7c",  0xf000, 0x1000, 0xa3c11e9e )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "tma1c-a.6k",   0x0000, 0x1000, 0x06b9ff85 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tma1v-a.3f",   0x0000, 0x1000, 0xadf49ee0 )
	ROM_LOAD( "tma1v-a.3j",   0x1000, 0x1000, 0xa5318f2d )

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tma1v-a.7m",   0x0000, 0x1000, 0x186762f8 )
	ROM_LOAD( "tma1v-a.7n",   0x1000, 0x1000, 0xe0e08bba )
	ROM_LOAD( "tma1v-a.7p",   0x2000, 0x1000, 0x7b27c8c1 )
	ROM_LOAD( "tma1v-a.7s",   0x3000, 0x1000, 0x912ba80a )
	ROM_LOAD( "tma1v-a.7t",   0x4000, 0x1000, 0x5cbb92a5 )
	ROM_LOAD( "tma1v-a.7u",   0x5000, 0x1000, 0x13afb9ed )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "mario.4p",     0x0000, 0x0200, 0xafc9bd41 )
ROM_END

ROM_START( masao )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "masao-4.rom",  0x0000, 0x2000, 0x07a75745 )
	ROM_LOAD( "masao-3.rom",  0x2000, 0x2000, 0x55c629b6 )
	ROM_LOAD( "masao-2.rom",  0x4000, 0x2000, 0x42e85240 )
	ROM_LOAD( "masao-1.rom",  0xf000, 0x1000, 0xb2817af9 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for sound */
	ROM_LOAD( "masao-5.rom",  0x0000, 0x1000, 0xbd437198 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "masao-6.rom",  0x0000, 0x1000, 0x1c9e0be2 )
	ROM_LOAD( "masao-7.rom",  0x1000, 0x1000, 0x747c1349 )

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tma1v-a.7m",   0x0000, 0x1000, 0x186762f8 )
	ROM_LOAD( "masao-9.rom",  0x1000, 0x1000, 0x50be3918 )
	ROM_LOAD( "mario.7p",     0x2000, 0x1000, 0x56be6ccd )
	ROM_LOAD( "tma1v-a.7s",   0x3000, 0x1000, 0x912ba80a )
	ROM_LOAD( "tma1v-a.7t",   0x4000, 0x1000, 0x5cbb92a5 )
	ROM_LOAD( "tma1v-a.7u",   0x5000, 0x1000, 0x13afb9ed )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "mario.4p",     0x0000, 0x0200, 0xafc9bd41 )
ROM_END



GAME( 1983, mario,   0,     mario, mario,   0, ROT180, "Nintendo of America", "Mario Bros. (US)" )
GAME( 1983, mariojp, mario, mario, mariojp, 0, ROT180, "Nintendo", "Mario Bros. (Japan)" )
GAME( 1983, masao,   mario, masao, mario,   0, ROT180, "bootleg", "Masao" )
