#include "../vidhrdw/spacefb.cpp"

/***************************************************************************

Space Firebird memory map (preliminary)

  Memory Map figured out by Chris Hardy (chrish@kcbbs.gen.nz), Paul Johnson and Andy Clark
  MAME driver by Chris Hardy

  Schematics scanned and provided by James Twine
  Thanks to Gary Walton for lending me his REAL Space Firebird

  The way the sprites are drawn ARE correct. They are identical to the real
  pcb.

TODO
	- Add "Red" flash when you die.
	- Add Starfield. It is NOT a Galaxians starfield
	-

0000-3FFF ROM		Code
8000-83FF RAM		Sprite RAM
C000-C7FF RAM		Game ram

IO Ports

IN:
Port 0 - Player 1 I/O
Port 1 - Player 2 I/O
Port 2 - P1/P2 Start Test and Service
Port 3 - Dipswitch


OUT:
Port 0 - RV,VREF and CREF
Port 1 - Comms to the Sound card (-> 8212)
    bit 0 = discrete sound
    bit 1 = INT to 8035
    bit 2 = T1 input to 8035
    bit 3 = PB4 input to 8035
    bit 4 = PB5 input to 8035
    bit 5 = T0 input to 8035
    bit 6 = discrete sound
    bit 7 = discrete sound

Port 2 - Video contrast values (used by sound board only)
Port 3 - Unused


IN:
Port 0

   bit 0 = Player 1 Right
   bit 1 = Player 1 Left
   bit 2 = unused
   bit 3 = unused
   bit 4 = Player 1 Escape
   bit 5 = unused
   bit 6 = unused
   bit 7 = Player 1 Fire

Port 1

   bit 0 = Player 2 Right
   bit 1 = Player 2 Left
   bit 2 = unused
   bit 3 = unused
   bit 4 = Player 2 Escape
   bit 5 = unused
   bit 6 = unused
   bit 7 = Player 2 Fire

Port 2

   bit 0 = unused
   bit 1 = unused
   bit 2 = Start 1 Player game
   bit 3 = Start 2 Players game
   bit 4 = unused
   bit 5 = unused
   bit 6 = Test switch
   bit 7 = Coin and Service switch

Port 3

   bit 0 = Dipswitch 1
   bit 1 = Dipswitch 2
   bit 2 = Dipswitch 3
   bit 3 = Dipswitch 4
   bit 4 = Dipswitch 5
   bit 5 = Dipswitch 6
   bit 6 = unused (Debug switch - Code jumps to $3800 on reset if on)
   bit 7 = unused

OUT:
Port 0 - Video

   bit 0 = Screen flip. (RV)
   bit 1 = unused
   bit 2 = unused
   bit 3 = unused
   bit 4 = unused
   bit 5 = Char/Sprite Bank switch (VREF)
   bit 6 = Turns on Bit 2 of the color PROM. Used to change the bird colors. (CREF)
   bit 7 = unused

Port 1
	8 bits gets "sent" to a 8212 chip

Port 2 - Video control

These are passed to the sound board and are used to produce a
red flash effect when you die.

   bit 0 = CONT R
   bit 1 = CONT G
   bit 2 = CONT B
   bit 3 = ALRD
   bit 4 = ALBU
   bit 5 = unused
   bit 6 = unused
   bit 7 = ALBA


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/i8039/i8039.h"


void spacefb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void spacefb_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

WRITE_HANDLER( spacefb_video_control_w );
WRITE_HANDLER( spacefb_port_2_w );


static int spacefb_interrupt(void)
{
	if (cpu_getiloops() != 0) return (0x00cf);		/* RST 08h */
	else return (0x00d7);		/* RST 10h */
}


unsigned char spacefb_sound_latch;

static READ_HANDLER( spacefb_sh_p2_r )
{
    return ((spacefb_sound_latch & 0x18) << 1);
}

static READ_HANDLER( spacefb_sh_t0_r )
{
    return spacefb_sound_latch & 0x20;
}

static READ_HANDLER( spacefb_sh_t1_r )
{
    return spacefb_sound_latch & 0x04;
}

static WRITE_HANDLER( spacefb_port_1_w )
{
    spacefb_sound_latch = data;
    if (!(data & 0x02)) cpu_cause_interrupt(1,I8039_EXT_INT);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x83ff, MRA_RAM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x83ff, MWA_RAM, &videoram, &videoram_size },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r }, /* IN 0 */
	{ 0x01, 0x01, input_port_1_r }, /* IN 1 */
	{ 0x02, 0x02, input_port_2_r }, /* Coin - Start */
	{ 0x03, 0x03, input_port_3_r }, /* DSW0 */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, spacefb_video_control_w },
	{ 0x01, 0x01, spacefb_port_1_w },
	{ 0x02, 0x02, spacefb_port_2_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x03ff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x03ff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport_sound[] =
{
	{ I8039_p2, I8039_p2, spacefb_sh_p2_r },
	{ I8039_t0, I8039_t0, spacefb_sh_t0_r },
	{ I8039_t1, I8039_t1, spacefb_sh_t1_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport_sound[] =
{
	{ I8039_p1, I8039_p1, DAC_0_data_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( spacefb )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START      /* Coin - Start */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Test ? */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x10, "8000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END


/* Same as Space Firebird, except for the difficulty switch */
INPUT_PORTS_START( spacedem )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START      /* Coin - Start */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Test ? */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x10, "8000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout spritelayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 256*8*8 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
/*
 * The bullests are stored in a 256x4bit PROM but the .bin file is
 * 256*8bit
 */

static struct GfxLayout bulletlayout =
{
	4,4,	/* 4*4 characters */
	64,		/* 64 characters */
	1,		/* 1 bits per pixel */
	{ 0 },
	{ 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8 },
	4*8	/* every char takes 4 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout, 0, 8 },
	{ REGION_GFX2, 0, &bulletlayout, 0, 8 },
	{ -1 } /* end of array */
};


static struct DACinterface dac_interface =
{
	1,
	{ 100 }
};

static struct MachineDriver machine_driver_spacefb =
{
	/* basic machine hardware */
	{
        {
            CPU_Z80,
            4000000,    /* 4 Mhz? */
            readmem,writemem,readport,writeport,
            spacefb_interrupt,2 /* two int's per frame */
        },
		{
            CPU_I8035 | CPU_AUDIO_CPU,
            6000000/15,
			readmem_sound,writemem_sound,readport_sound,writeport_sound,
            ignore_interrupt,0
        }
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
    3,
	0,

	/* video hardware */
	/* there is no real character graphics, only 8*8 and 4*4 sprites */
  	264, 256, { 0, 263, 16, 247 },
	gfxdecodeinfo,

	32,32,
	spacefb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	spacefb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
            &dac_interface
        }
	}
};



ROM_START( spacefb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "5e.cpu",       0x0000, 0x0800, 0x2d406678 )         /* Code */
	ROM_LOAD( "5f.cpu",       0x0800, 0x0800, 0x89f0c34a )
	ROM_LOAD( "5h.cpu",       0x1000, 0x0800, 0xc4bcac3e )
	ROM_LOAD( "5i.cpu",       0x1800, 0x0800, 0x61c00a65 )
	ROM_LOAD( "5j.cpu",       0x2000, 0x0800, 0x598420b9 )
	ROM_LOAD( "5k.cpu",       0x2800, 0x0800, 0x1713300c )
	ROM_LOAD( "5m.cpu",       0x3000, 0x0800, 0x6286f534 )
	ROM_LOAD( "5n.cpu",       0x3800, 0x0800, 0x1c9f91ee )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
    ROM_LOAD( "ic20.snd",     0x0000, 0x0400, 0x1c8670b3 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5k.vid",       0x0000, 0x0800, 0x236e1ff7 )
	ROM_LOAD( "6k.vid",       0x0800, 0x0800, 0xbf901a4e )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4i.vid",       0x0000, 0x0100, 0x528e8533 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "mb7051.3n",    0x0000, 0x0020, 0x465d07af )
ROM_END

ROM_START( spacefbg )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "tst-c.5e",     0x0000, 0x0800, 0x07949110 )         /* Code */
	ROM_LOAD( "tst-c.5f",     0x0800, 0x0800, 0xce591929 )
	ROM_LOAD( "tst-c.5h",     0x1000, 0x0800, 0x55d34ea5 )
	ROM_LOAD( "tst-c.5i",     0x1800, 0x0800, 0xa11e2881 )
	ROM_LOAD( "tst-c.5j",     0x2000, 0x0800, 0xa6aff352 )
	ROM_LOAD( "tst-c.5k",     0x2800, 0x0800, 0xf4213603 )
	ROM_LOAD( "5m.cpu",       0x3000, 0x0800, 0x6286f534 )
	ROM_LOAD( "5n.cpu",       0x3800, 0x0800, 0x1c9f91ee )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
    ROM_LOAD( "ic20.snd",     0x0000, 0x0400, 0x1c8670b3 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tst-v.5k",     0x0000, 0x0800, 0xbacc780d )
	ROM_LOAD( "tst-v.6k",     0x0800, 0x0800, 0x1645ff26 )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4i.vid",       0x0000, 0x0100, 0x528e8533 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "mb7051.3n",    0x0000, 0x0020, 0x465d07af )
ROM_END

ROM_START( spacebrd )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "sb5e.cpu",     0x0000, 0x0800, 0x232d66b8 )         /* Code */
	ROM_LOAD( "sb5f.cpu",     0x0800, 0x0800, 0x99504327 )
	ROM_LOAD( "sb5h.cpu",     0x1000, 0x0800, 0x49a26fe5 )
	ROM_LOAD( "sb5i.cpu",     0x1800, 0x0800, 0xc23025da )
	ROM_LOAD( "sb5j.cpu",     0x2000, 0x0800, 0x5e97baf0 )
	ROM_LOAD( "5k.cpu",       0x2800, 0x0800, 0x1713300c )
	ROM_LOAD( "sb5m.cpu",     0x3000, 0x0800, 0x4cbe92fc )
	ROM_LOAD( "sb5n.cpu",     0x3800, 0x0800, 0x1a798fbf )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
    ROM_LOAD( "ic20.snd",     0x0000, 0x0400, 0x1c8670b3 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5k.vid",       0x0000, 0x0800, 0x236e1ff7 )
	ROM_LOAD( "6k.vid",       0x0800, 0x0800, 0xbf901a4e )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4i.vid",       0x0000, 0x0100, 0x528e8533 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "spcbird.clr",  0x0000, 0x0020, 0x25c79518 )
ROM_END

/* only a few bytes are different between this and spacebrd above */
ROM_START( spacefbb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "fc51",         0x0000, 0x0800, 0x5657bd2f )         /* Code */
	ROM_LOAD( "fc52",         0x0800, 0x0800, 0x303b0294 )
	ROM_LOAD( "sb5h.cpu",     0x1000, 0x0800, 0x49a26fe5 )
	ROM_LOAD( "sb5i.cpu",     0x1800, 0x0800, 0xc23025da )
	ROM_LOAD( "fc55",         0x2000, 0x0800, 0x946bee5d )
	ROM_LOAD( "5k.cpu",       0x2800, 0x0800, 0x1713300c )
	ROM_LOAD( "sb5m.cpu",     0x3000, 0x0800, 0x4cbe92fc )
	ROM_LOAD( "sb5n.cpu",     0x3800, 0x0800, 0x1a798fbf )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
    ROM_LOAD( "fb.snd",       0x0000, 0x0400, 0xf7a59492 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fc59",         0x0000, 0x0800, 0xa00ad16c )
	ROM_LOAD( "6k.vid",       0x0800, 0x0800, 0xbf901a4e )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4i.vid",       0x0000, 0x0100, 0x528e8533 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "mb7051.3n",    0x0000, 0x0020, 0x465d07af )
ROM_END

ROM_START( spacedem )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "sd5e.cpu",     0x0000, 0x0800, 0xbe4b9cbb )         /* Code */
	ROM_LOAD( "sd5f.cpu",     0x0800, 0x0800, 0x0814f964 )
	ROM_LOAD( "sd5h.cpu",     0x1000, 0x0800, 0xebfff682 )
	ROM_LOAD( "sd5i.cpu",     0x1800, 0x0800, 0xdd7e1378 )
	ROM_LOAD( "sd5j.cpu",     0x2000, 0x0800, 0x98334fda )
	ROM_LOAD( "sd5k.cpu",     0x2800, 0x0800, 0xba4933b2 )
	ROM_LOAD( "sd5m.cpu",     0x3000, 0x0800, 0x14d3c656 )
	ROM_LOAD( "sd5n.cpu",     0x3800, 0x0800, 0x7e0e41b0 )

	ROM_REGION( 0x1000, REGION_CPU2 )	/* sound */
    ROM_LOAD( "ic20.snd",     0x0000, 0x0400, 0x00000000 )  /* This ROM wasn't in the set. Using Space Firebird's */

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sd5k.vid",     0x0000, 0x0800, 0x55758e4d )
	ROM_LOAD( "sd6k.vid",     0x0800, 0x0800, 0x3fcbb20c )

	ROM_REGION( 0x0100, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4i.vid",       0x0000, 0x0100, 0x00000000 )  /* This ROM wasn't in the set. Using Space Firebird's */

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "mb7051.3n",    0x0000, 0x0020, 0x00000000 )  /* This ROM wasn't in the set. Using Space Firebird's */
ROM_END


GAMEX( 1980, spacefb,  0,       spacefb, spacefb,  0, ROT90, "Nintendo", "Space Firebird (Nintendo)", GAME_IMPERFECT_COLORS | GAME_IMPERFECT_SOUND )
GAMEX( 1980, spacefbg, spacefb, spacefb, spacefb,  0, ROT90, "Gremlin", "Space Firebird (Gremlin)", GAME_IMPERFECT_COLORS | GAME_IMPERFECT_SOUND )
GAMEX( 1980, spacebrd, spacefb, spacefb, spacefb,  0, ROT90, "bootleg", "Space Bird (bootleg)", GAME_IMPERFECT_COLORS | GAME_IMPERFECT_SOUND )
GAMEX( 1980, spacefbb, spacefb, spacefb, spacefb,  0, ROT90, "bootleg", "Space Firebird (bootleg)", GAME_IMPERFECT_COLORS | GAME_IMPERFECT_SOUND )
GAMEX( 1980, spacedem, spacefb, spacefb, spacedem, 0, ROT90, "Nintendo / Fortrek", "Space Demon", GAME_IMPERFECT_COLORS | GAME_IMPERFECT_SOUND )
