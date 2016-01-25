#include "../vidhrdw/leprechn.cpp"

/***************************************************************************

 Leprechaun memory map (preliminary)

 driver by Zsolt Vasvari

 Hold down F2 while pressing F3 to enter test mode. Hit Advance (F1) to
 cycle through test and hit F2 to execute.


 Main CPU

 0000-03ff RAM
 8000-ffff ROM

 2000-200f, 2800-280f and 3000-300f might be some kind of programmable I/O
 controller. I'm not knowledgable enough about them to be able to tell for sure.
 I based the observation of the locations being written/read. They seem to
 follow a similar pattern across all 3 areas. Anyone with schematics?

 I/O Read

 2000 Video RAM Read Back
 200d ???
 3002-3003 ???
 2801 Input Port Read

 I/O Write

 2000 Graphics Command Write
 2001 Graphics Data Write
 2002-2003 ???
 200c-200e ???
 2800 Input Port Select
 2802-2803 ???
 280c ???
 3001 Sound Command Write
 3002-3003 ???
 300c ???


 Sound CPU

 0000-01ff RAM
 f000-ffff ROM

 I/O Read

 0800 Sound Command Read
 0804-0805 ???
 080c ???
 a001 ???

 I/O Write

 0801-0803 ???
 0806 ???
 081e ???
 a000 AY8910 Control Port
 a002 AY8910 Write Port

 ***************************************************************************/

#include "driver.h"
#include "cpu/m6502/m6502.h"


int  leprechn_vh_start(void);
void leprechn_vh_stop(void);
void leprechn_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( leprechn_graphics_command_w );
READ_HANDLER( leprechn_graphics_data_r );
WRITE_HANDLER( leprechn_graphics_data_w );



static UINT8 input_port_select;

static WRITE_HANDLER( leprechn_input_port_select_w )
{
    input_port_select = data;
}

static READ_HANDLER( leprechn_input_port_r )
{
    switch (input_port_select)
    {
    case 0x01:
        return input_port_0_r(0);
    case 0x02:
        return input_port_2_r(0);
    case 0x04:
        return input_port_3_r(0);
    case 0x08:
        return input_port_1_r(0);
    case 0x40:
        return input_port_5_r(0);
    case 0x80:
        return input_port_4_r(0);
    }

    return 0xff;
}

static READ_HANDLER( leprechn_200d_r )
{
    // Maybe a VSYNC line?
    return 0x02;
}

static READ_HANDLER( leprechn_0805_r )
{
    return 0xc0;
}

static WRITE_HANDLER( leprechn_sh_w )
{
    soundlatch_w(offset,data);
    cpu_cause_interrupt(1,M6502_INT_IRQ);
}



static struct MemoryReadAddress readmem[] =
{
    { 0x0000, 0x03ff, MRA_RAM},
    { 0x2000, 0x2000, leprechn_graphics_data_r},
    { 0x200d, 0x200d, leprechn_200d_r },
    { 0x2801, 0x2801, leprechn_input_port_r },
    { 0x3002, 0x3003, MRA_RAM},
    { 0x8000, 0xffff, MRA_ROM},
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
    { 0x0000, 0x03ff, MWA_RAM},
    { 0x2000, 0x2000, leprechn_graphics_command_w},
    { 0x2001, 0x2001, leprechn_graphics_data_w},
    { 0x2002, 0x2003, MWA_NOP },  // ???
    { 0x200c, 0x200e, MWA_NOP },  // ???
    { 0x2800, 0x2800, leprechn_input_port_select_w},
    { 0x2802, 0x2803, MWA_NOP },  // ???
    { 0x280c, 0x280c, MWA_NOP },  // ???
    { 0x3001, 0x3001, leprechn_sh_w },
    { 0x3002, 0x3003, MWA_RAM},   // ???
    { 0x300c, 0x300c, MWA_NOP },  // ???
    { 0x8000, 0xffff, MWA_ROM},
    { -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
    { 0x0000, 0x01ff, MRA_RAM},
    { 0x0800, 0x0800, soundlatch_r},
    { 0x0804, 0x0804, MRA_RAM},   // ???
    { 0x0805, 0x0805, leprechn_0805_r},   // ???
    { 0x080c, 0x080c, MRA_RAM},   // ???
    { 0xa001, 0xa001, MRA_RAM},   // ???
    { 0xf000, 0xffff, MRA_ROM},
    { -1 }  /* end of table */
};


static struct MemoryWriteAddress sound_writemem[] =
{
    { 0x0000, 0x01ff, MWA_RAM},
    { 0x0801, 0x0803, MWA_RAM},   // ???
    { 0x0806, 0x0806, MWA_RAM},   // ???
    { 0x081e, 0x081e, MWA_RAM},   // ???
    { 0xa000, 0xa000, AY8910_control_port_0_w },
    { 0xa002, 0xa002, AY8910_write_port_0_w },
    { 0xf000, 0xffff, MWA_ROM},
    { -1 }  /* end of table */
};

INPUT_PORTS_START( leprechn )
    // All of these ports are read indirectly through 2800/2801
    PORT_START      /* Input Port 0 */
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT ) // This is called "Slam" in the game
    PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
    PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "Advance", KEYCODE_F1, IP_JOY_NONE )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )
    PORT_BIT( 0x23, IP_ACTIVE_LOW, IPT_UNUSED )

    PORT_START      /* Input Port 1 */
    PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )

    PORT_START      /* Input Port 2 */
    PORT_BIT( 0x5f, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

    PORT_START      /* Input Port 3 */
    PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )

    PORT_START      /* DSW #1 */
    PORT_DIPNAME( 0x09, 0x09, DEF_STR( Coin_B ) )
    PORT_DIPSETTING(    0x09, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )
    PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( 1C_7C ) )
    PORT_DIPNAME( 0x22, 0x22, "Max Credits" )
    PORT_DIPSETTING(    0x22, "10" )
    PORT_DIPSETTING(    0x20, "20" )
    PORT_DIPSETTING(    0x02, "30" )
    PORT_DIPSETTING(    0x00, "40" )
    PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) )
    PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
    PORT_DIPNAME( 0x10, 0x10, DEF_STR( Free_Play ) )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_A ) )
    PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )

    PORT_START      /* DSW #2 */
    PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
    PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0x08, "3" )
    PORT_DIPSETTING(    0x00, "4" )
    PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Bonus_Life ) )
    PORT_DIPSETTING(    0x40, "30000" )
    PORT_DIPSETTING(    0x80, "60000" )
    PORT_DIPSETTING(    0x00, "90000" )
    PORT_DIPSETTING(    0xc0, "None" )
    PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



/* RGBI palette. Is it correct? */
static unsigned char palette[16 * 3] =
{
    0x00, 0x00, 0x00,
    0xff, 0x00, 0x00,
    0x00, 0xff, 0x00,
    0xff, 0xff, 0x00,
    0x00, 0x00, 0xff,
    0xff, 0x00, 0xff,
    0x00, 0xff, 0xff,
    0xff, 0xff, 0xff,
    0x40, 0x40, 0x40,
    0xff, 0x40, 0x40,
    0x40, 0xff, 0x40,
    0xff, 0xff, 0x40,
    0x40, 0x40, 0xff,
    0xff, 0x40, 0xff,
    0x40, 0xff, 0xff,
    0xff, 0xff, 0xff
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
    memcpy(game_palette,palette,sizeof(palette));
}



static struct AY8910interface ay8910_interface =
{
    1,      /* 1 chip */
    14318000/8,     /* ? */
    { 50 },
    { 0 },
    { 0 },
    { 0 },
    { 0 }
};


static struct MachineDriver machine_driver_leprechn =
{
    /* basic machine hardware */
    {
        // A good test to verify that the relative CPU speeds of the main
        // and sound are correct, is when you finish a level, the sound
        // should stop before the display switches to the name of the
        // next level
        {
            CPU_M6502,
            1250000,    /* 1.25 Mhz ??? */
            readmem,writemem,0,0,
            interrupt,1
        },
        {
            CPU_M6502 | CPU_AUDIO_CPU,
            1500000,    /* 1.5 Mhz ??? */
            sound_readmem,sound_writemem,0,0,
            ignore_interrupt,1      /* interrupts are triggered by the main CPU */
        }
    },
    57, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
    1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
    0,

    /* video hardware */
    256, 256, { 0, 256-1, 0, 256-1 },
    0,
    sizeof(palette) / sizeof(palette[0]) / 3, 0,
    init_palette,

    VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
    0,
    leprechn_vh_start,
    leprechn_vh_stop,
    leprechn_vh_screenrefresh,

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

ROM_START( leprechn )
	ROM_REGION( 0x10000, REGION_CPU1 )  /* 64k for the main CPU */
	ROM_LOAD( "lep1",         0x8000, 0x1000, 0x2c4a46ca )
	ROM_LOAD( "lep2",         0x9000, 0x1000, 0x6ed26b3e )
	ROM_LOAD( "lep3",         0xa000, 0x1000, 0xa2eaa016 )
	ROM_LOAD( "lep4",         0xb000, 0x1000, 0x6c12a065 )
	ROM_LOAD( "lep5",         0xc000, 0x1000, 0x21ddb539 )
	ROM_LOAD( "lep6",         0xd000, 0x1000, 0x03c34dce )
	ROM_LOAD( "lep7",         0xe000, 0x1000, 0x7e06d56d )
	ROM_LOAD( "lep8",         0xf000, 0x1000, 0x097ede60 )

	ROM_REGION( 0x10000, REGION_CPU2 )  /* 64k for the audio CPU */
	ROM_LOAD( "lepsound",     0xf000, 0x1000, 0x6651e294 )
ROM_END

ROM_START( potogold )
	ROM_REGION( 0x10000, REGION_CPU1 )  /* 64k for the main CPU */
	ROM_LOAD( "pog.pg1",      0x8000, 0x1000, 0x9f1dbda6 )
	ROM_LOAD( "pog.pg2",      0x9000, 0x1000, 0xa70e3811 )
	ROM_LOAD( "pog.pg3",      0xa000, 0x1000, 0x81cfb516 )
	ROM_LOAD( "pog.pg4",      0xb000, 0x1000, 0xd61b1f33 )
	ROM_LOAD( "pog.pg5",      0xc000, 0x1000, 0xeee7597e )
	ROM_LOAD( "pog.pg6",      0xd000, 0x1000, 0x25e682bc )
	ROM_LOAD( "pog.pg7",      0xe000, 0x1000, 0x84399f54 )
	ROM_LOAD( "pog.pg8",      0xf000, 0x1000, 0x9e995a1a )

	ROM_REGION( 0x10000, REGION_CPU2 )  /* 64k for the audio CPU */
	ROM_LOAD( "pog.snd",      0xf000, 0x1000, 0xec61f0a4 )
ROM_END



GAME( 1982, leprechn, 0,        leprechn, leprechn, 0, ROT0, "Tong Electronic", "Leprechaun" )
GAME( 1982, potogold, leprechn, leprechn, leprechn, 0, ROT0, "GamePlan", "Pot of Gold" )
