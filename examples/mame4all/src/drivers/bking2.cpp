#include "../vidhrdw/bking2.cpp"

/***************************************************************************

Birdie King II Memory Map
-------------------------

0000-7fff ROM
8000-83ff Scratch RAM
8400-8fff (Scratch RAM again, address lines AB10, AB11 ignored)
9000-9fff Video RAM?
A000-Bfff Unused?


NOTE:  ROM DM03 is missing from all known ROM sets.  This is a color palette.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

void bking2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void bking2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( bking2_xld1_w );
WRITE_HANDLER( bking2_yld1_w );
WRITE_HANDLER( bking2_xld2_w );
WRITE_HANDLER( bking2_yld2_w );
WRITE_HANDLER( bking2_xld3_w );
WRITE_HANDLER( bking2_yld3_w );
WRITE_HANDLER( bking2_msk_w );
WRITE_HANDLER( bking2_cont1_w );
WRITE_HANDLER( bking2_cont2_w );
WRITE_HANDLER( bking2_cont3_w );
WRITE_HANDLER( bking2_hitclr_w );
READ_HANDLER( bking2_input_port_5_r );
READ_HANDLER( bking2_input_port_6_r );
READ_HANDLER( bking2_pos_r );


static int sndnmi_enable = 1;

static READ_HANDLER( bking2_sndnmi_disable_r )
{
	sndnmi_enable = 0;
	return 0;
}

static WRITE_HANDLER( bking2_sndnmi_enable_w )
{
	sndnmi_enable = 1;
}

static WRITE_HANDLER( bking2_soundlatch_w )
{
	int i,code;


	code = 0;
	for (i = 0;i < 8;i++)
		if (data & (1 << i)) code |= 0x80 >> i;

	soundlatch_w(offset,code);
	if (sndnmi_enable) cpu_cause_interrupt(1,Z80_NMI_INT);
}


static struct MemoryReadAddress readmem[] =
{
    { 0x0000, 0x7fff, MRA_ROM },
    { 0x8000, 0x83ff, MRA_RAM },
    { 0x9000, 0x97ff, MRA_RAM },
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
    { 0x0000, 0x7fff, MWA_ROM },
    { 0x8000, 0x83ff, MWA_RAM },
    { 0x9000, 0x97ff, videoram_w, &videoram, &videoram_size },
    { -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
    { 0x00, 0x00, input_port_0_r },
    { 0x01, 0x01, input_port_1_r },
    { 0x02, 0x02, input_port_2_r },
    { 0x03, 0x03, input_port_3_r },
    { 0x04, 0x04, input_port_4_r },
    { 0x05, 0x05, bking2_input_port_5_r },
    { 0x06, 0x06, bking2_input_port_6_r },
	{ 0x07, 0x1f, bking2_pos_r },
    { -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
    { 0x00, 0x00, bking2_xld1_w },
    { 0x01, 0x01, bking2_yld1_w },
    { 0x02, 0x02, bking2_xld2_w },
    { 0x03, 0x03, bking2_yld2_w },
    { 0x04, 0x04, bking2_xld3_w },
    { 0x05, 0x05, bking2_yld3_w },
    { 0x06, 0x06, bking2_msk_w },
    { 0x07, 0x07, watchdog_reset_w },
    { 0x08, 0x08, bking2_cont1_w },
    { 0x09, 0x09, bking2_cont2_w },
    { 0x0a, 0x0a, bking2_cont3_w },
    { 0x0b, 0x0b, bking2_soundlatch_w },
  //{ 0x0c, 0x0c, bking2_eport2_w },   this is not shown to be connected anywhere
    { 0x0d, 0x0d, bking2_hitclr_w },
    { -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
    { 0x0000, 0x1fff, MRA_ROM },
    { 0x4000, 0x43ff, MRA_RAM },
	{ 0x4401, 0x4401, AY8910_read_port_0_r },
	{ 0x4403, 0x4403, AY8910_read_port_1_r },
    { 0x4800, 0x4800, soundlatch_r },
    { 0x4802, 0x4802, bking2_sndnmi_disable_r },
    { 0xe000, 0xefff, MRA_ROM },   /* space for some other ROM???
									  It's checked if there is valid code there */
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
    { 0x0000, 0x1fff, MWA_ROM },
    { 0x4000, 0x43ff, MWA_RAM },
	{ 0x4400, 0x4400, AY8910_control_port_0_w },
	{ 0x4401, 0x4401, AY8910_write_port_0_w },
	{ 0x4402, 0x4402, AY8910_control_port_1_w },
	{ 0x4403, 0x4403, AY8910_write_port_1_w },
    { 0x4802, 0x4802, bking2_sndnmi_enable_w },
    { -1 }  /* end of table */
};

INPUT_PORTS_START( bking2 )
    PORT_START  /* IN0 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
    PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )

    PORT_START  /* IN1 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 ) /* Continue 1 */
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 ) /* Continue 2 */
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_TILT )
    PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED ) /* Not Connected */

    PORT_START  /* IN2 - DIP Switch A */
    PORT_DIPNAME( 0x01, 0x00, "Bonus Holes Awarded" )
    PORT_DIPSETTING(    0x00, "Fewer" )
    PORT_DIPSETTING(    0x01, "More" )
    PORT_DIPNAME( 0x02, 0x02, "Holes Awarded for Hole-in-One" )
    PORT_DIPSETTING(    0x00, "3" )
    PORT_DIPSETTING(    0x02, "9" )
    PORT_DIPNAME( 0x04, 0x04, DEF_STR(Free_Play) )
    PORT_DIPSETTING(    0x04, DEF_STR(Off))
    PORT_DIPSETTING(    0x00, DEF_STR(On))
    PORT_DIPNAME( 0x18, 0x18, "Holes (Lives)" )
    PORT_DIPSETTING(    0x18, "3" )
    PORT_DIPSETTING(    0x08, "4" )
    PORT_DIPSETTING(    0x10, "5" )
    PORT_DIPSETTING(    0x00, "9" )
    PORT_DIPNAME( 0x20, 0x20, DEF_STR(Unused) )
    PORT_DIPSETTING(    0x20, DEF_STR(Off))
    PORT_DIPSETTING(    0x00, DEF_STR(On))
    PORT_DIPNAME( 0x40, 0x40, DEF_STR(Flip_Screen) )
    PORT_DIPSETTING(    0x40, DEF_STR(Off))
    PORT_DIPSETTING(    0x00, DEF_STR(On))
    PORT_DIPNAME( 0x80, 0x00, DEF_STR(Cabinet) )
    PORT_DIPSETTING(    0x00, DEF_STR(Upright) )
    PORT_DIPSETTING(    0x80, DEF_STR(Cocktail) )


    PORT_START  /* IN3 - DIP Switch B */
    PORT_DIPNAME( 0x0f, 0x00, DEF_STR( Coin_A ) )
    PORT_DIPSETTING(    0x0f, DEF_STR( 9C_1C ) )
    PORT_DIPSETTING(    0x0e, DEF_STR( 8C_1C ) )
    PORT_DIPSETTING(    0x0d, DEF_STR( 7C_1C ) )
    PORT_DIPSETTING(    0x0c, DEF_STR( 6C_1C ) )
    PORT_DIPSETTING(    0x0b, DEF_STR( 5C_1C ) )
    PORT_DIPSETTING(    0x0a, DEF_STR( 4C_1C ) )
    PORT_DIPSETTING(    0x09, DEF_STR( 3C_1C ) )
    PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
    PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
    PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
    PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
    PORT_DIPSETTING(    0x06, DEF_STR( 1C_7C ) )
    PORT_DIPSETTING(    0x07, DEF_STR( 1C_8C ) )
    PORT_DIPNAME( 0xf0, 0x00, DEF_STR( Coin_B ) )
    PORT_DIPSETTING(    0xf0, DEF_STR( 9C_1C ) )
    PORT_DIPSETTING(    0xe0, DEF_STR( 8C_1C ) )
    PORT_DIPSETTING(    0xd0, DEF_STR( 7C_1C ) )
    PORT_DIPSETTING(    0xc0, DEF_STR( 6C_1C ) )
    PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
    PORT_DIPSETTING(    0xa0, DEF_STR( 4C_1C ) )
    PORT_DIPSETTING(    0x90, DEF_STR( 3C_1C ) )
    PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x20, DEF_STR( 1C_3C ) )
    PORT_DIPSETTING(    0x30, DEF_STR( 1C_4C ) )
    PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
    PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
    PORT_DIPSETTING(    0x60, DEF_STR( 1C_7C ) )
    PORT_DIPSETTING(    0x70, DEF_STR( 1C_8C ) )

    PORT_START  /* IN4 - DIP Switch C */
    PORT_DIPNAME( 0x01, 0x01, "Crow" )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x01, DEF_STR( On ) )
    PORT_DIPNAME( 0x06, 0x04, "Crow Flight Pattern" )
    PORT_DIPSETTING(    0x00, "1" )
    PORT_DIPSETTING(    0x02, "2" )
    PORT_DIPSETTING(    0x04, "3" )
    PORT_DIPSETTING(    0x06, "4" )
    PORT_DIPNAME( 0x08, 0x08, DEF_STR(Unused) )
    PORT_DIPSETTING(    0x00, DEF_STR(Off))
    PORT_DIPSETTING(    0x08, DEF_STR(On))
    PORT_DIPNAME( 0x10, 0x10, "Coinage Display" )
    PORT_DIPSETTING(    0x00, DEF_STR(Off))
    PORT_DIPSETTING(    0x10, DEF_STR(On))
    PORT_DIPNAME( 0x20, 0x20, "Year Display" )
    PORT_DIPSETTING(    0x00, DEF_STR(Off))
    PORT_DIPSETTING(    0x20, DEF_STR(On))
    PORT_DIPNAME( 0x40, 0x40, "Check" )
    PORT_DIPSETTING(    0x00, "Check" )
    PORT_DIPSETTING(    0x40, "Normal" )
    PORT_DIPNAME( 0x80, 0x80, "Coin Chutes" )
    PORT_DIPSETTING(    0x00, "1" )
    PORT_DIPSETTING(    0x80, "2" )

    PORT_START  /* IN5 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X, 25, 10, 0, 0 ) /* Sensitivity, clip, min, max */

    PORT_START  /* IN6 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE, 25, 10, 0, 0 ) /* Sensitivity, clip, min, max */

    PORT_START  /* IN7 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_COCKTAIL, 25, 10, 0, 0 ) /* Sensitivity, clip, min, max */

    PORT_START  /* IN8 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_COCKTAIL, 25, 10, 0, 0 ) /* Sensitivity, clip, min, max */
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	1024,   /* 1024 characters */
	3,      /* 3 bits per pixel */
	{ 0*1024*8*8, 1*1024*8*8, 2*1024*8*8 }, /* the bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 }, /* reverse layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

struct GfxLayout crowlayout =
{
	16,32,	/* 16*32 characters */
	16,		/* 16 characters */
	2,		/* 2 bits per pixel */
	{ 0, 4 },
	{ 3*32*8+3, 3*32*8+2, 3*32*8+1, 3*32*8+0,
	  2*32*8+3, 2*32*8+2, 2*32*8+1, 2*32*8+0,
	    32*8+3,   32*8+2,   32*8+1,   32*8+0,
		     3,        2,        1,        0 }, /* reverse layout */
	{ 31*8, 30*8, 29*8, 28*8, 27*8, 26*8, 25*8, 24*8,
	  23*8, 22*8, 21*8, 20*8, 19*8, 18*8, 17*8, 16*8,
	  15*8, 14*8, 13*8, 12*8, 11*8, 10*8,  9*8,  8*8,
	   7*8,  6*8,  5*8,  4*8,  3*8,  2*8,  1*8,  0*8 },
	128*8    /* every sprite takes 128 consecutive bytes */
};

struct GfxLayout balllayout =
{
	8,16,  /* 8*16 sprites */
	8,     /* 8 sprites */
	1,  /* 1 bit per pixel */
	{ 0 },   /* the two bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },   /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8    /* every sprite takes 16 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0,           4  }, /* playfield */
	{ REGION_GFX2, 0, &crowlayout, 4*8,         4  }, /* crow */
	{ REGION_GFX3, 0, &balllayout, 4*8+4*4,     4  }, /* ball 1 */
	{ REGION_GFX4, 0, &balllayout, 4*8+4*4+4*2, 4  }, /* ball 2 */
	{ -1 } /* end of array */
};


static WRITE_HANDLER( portb_w )
{
	/* don't know what this is... could be a filter */
	//if (data != 0x00) logerror("portB = %02x\n",data);
}

static struct AY8910interface ay8910_interface =
{
	2,      /* 2 chips */
	2000000,	/* 2 MHz */
	{ 25, 25 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, DAC_0_signed_data_w },
	{ 0, portb_w }
};

static struct DACinterface dac_interface =
{
	1,
	{ 25 }
};



static struct MachineDriver machine_driver_bking2 =
{
    /* basic machine hardware */
    {
        {
            CPU_Z80,
			4000000,	/* 4 Mhz */
            readmem,writemem,
            readport,writeport,
            interrupt,1
        },
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,	/* 3 Mhz */
			sound_readmem,sound_writemem,0,0,
			/* interrupts (from Jungle King hardware, might be wrong): */
			/* - no interrupts synced with vblank */
			/* - NMI triggered by the main CPU */
			/* - periodic IRQ, with frequency 6000000/(4*16*16*10*16) = 36.621 Hz, */
			/*   that is a period of 27306666.6666 ns */
			0,0,
			interrupt,27306667
		}
    },
    60, DEFAULT_60HZ_VBLANK_DURATION,   /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
    0,  /* init machine */

    /* video hardware */
    32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
    gfxdecodeinfo,
    512, 4*8+4*4+4*2+4*2,
    bking2_vh_convert_color_prom,

    VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
    0,  /* video hardware init */
    generic_vh_start,
    generic_vh_stop,
    bking2_vh_screenrefresh,

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

ROM_START( bking2 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "01.13f",       0x0000, 0x1000, 0x078ada3f )
	ROM_LOAD( "02.11f",       0x1000, 0x1000, 0xc37d110a )
	ROM_LOAD( "03.10f",       0x2000, 0x1000, 0x2ba5c681 )
	ROM_LOAD( "04.8f",        0x3000, 0x1000, 0x8fad54e8 )
	ROM_LOAD( "05.7f",        0x4000, 0x1000, 0xb4de6b58 )
	ROM_LOAD( "06.5f",        0x5000, 0x1000, 0x9ac43b87 )
	ROM_LOAD( "07.4f",        0x6000, 0x1000, 0xb3ed40b7 )
	ROM_LOAD( "08.2f",        0x7000, 0x1000, 0x8fddb2e8 )

	ROM_REGION( 0x10000, REGION_CPU2 )         /* Sound ROMs */
	ROM_LOAD( "15",           0x0000, 0x1000, 0xf045d0fe )
	ROM_LOAD( "16",           0x1000, 0x1000, 0x92d50410 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "14.5a",        0x0000, 0x1000, 0x52636a94 )
	ROM_LOAD( "13.7a",        0x1000, 0x1000, 0x6b9e0564 )
	ROM_LOAD( "12.8a",        0x2000, 0x1000, 0xc6d685d9 )
	ROM_LOAD( "11.10a",       0x3000, 0x1000, 0x2b949987 )
	ROM_LOAD( "10.11a",       0x4000, 0x1000, 0xeb96f948 )
	ROM_LOAD( "09.13a",       0x5000, 0x1000, 0x595e3dd4 )

	ROM_REGION( 0x0800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "17",           0x0000, 0x0800, 0xe5663f0b )	/* crow graphics */

	ROM_REGION( 0x0800, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "18",           0x0000, 0x0800, 0xfc9cec31 )	/* ball 1 graphics. Only the first 128 bytes used */

	ROM_REGION( 0x0800, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "19",           0x0000, 0x0800, 0xfc9cec31 )  /* ball 2 graphics. Only the first 128 bytes used */

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "82s141.2d",    0x0000, 0x0200, 0x61b7a9ff )	/* palette */
	/* Collision detection prom 32x1 (not currently used) */
	/* HIT0-1 go to A3-A4. Character image goes to A0-A2 */
	ROM_LOAD( "mb7051.2c",    0x0200, 0x0020, 0x4cb5bd32 )
ROM_END



GAME( 1983, bking2, 0, bking2, bking2, 0, ROT90, "Taito Corporation", "Birdie King 2" )
