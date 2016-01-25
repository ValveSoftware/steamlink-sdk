#include "../vidhrdw/skyfox.cpp"

/***************************************************************************

						-= Sky Fox / Exerizer =-

				driver by	Luca Elia (eliavit@unina.it)


CPU  :	Z80A x 2
Sound:	YM2203C x 2
Other:	2 HM6116LP-3 (one on each board)
		1 KM6264L-15 (on bottom board)

To Do:	The background rendering is entirely guesswork

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"

/* Variables defined in vidhrdw: */

extern int skyfox_bg_pos, skyfox_bg_ctrl;


/* Functions defined in vidhrdw: */

READ_HANDLER ( skyfox_vregs_r );
WRITE_HANDLER( skyfox_vregs_w );

void skyfox_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

void skyfox_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


/***************************************************************************


								Main CPU


***************************************************************************/


/***************************************************************************
								Sky Fox
***************************************************************************/

static struct MemoryReadAddress skyfox_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM				},	// ROM
	{ 0xc000, 0xdfff, MRA_RAM				},	// RAM
	{ 0xe000, 0xe000, input_port_0_r		},	// Input Ports
	{ 0xe001, 0xe001, input_port_1_r		},	//
	{ 0xe002, 0xe002, input_port_2_r		},	//
	{ 0xf001, 0xf001, input_port_3_r		},	//
//	{ 0xff00, 0xff07, skyfox_vregs_r		},	// fake to read the vregs
	{ -1 }
};

static struct MemoryWriteAddress skyfox_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM								},	// ROM
	{ 0xc000, 0xcfff, MWA_RAM								},	// RAM
	{ 0xd000, 0xd3ff, MWA_RAM, &spriteram, &spriteram_size	},	// Sprites
	{ 0xd400, 0xdfff, MWA_RAM								},	// RAM?
	{ 0xe008, 0xe00f, skyfox_vregs_w						},	// Video Regs
	{ -1 }
};


static struct IOReadPort skyfox_readport[] =
{
	{ -1 }
};

static struct IOWritePort skyfox_writeport[] =
{
	{ -1 }
};






/***************************************************************************


								Sound CPU


***************************************************************************/


/***************************************************************************
								Sky Fox
***************************************************************************/


static struct MemoryReadAddress skyfox_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM				},	// ROM
	{ 0x8000, 0x87ff, MRA_RAM				},	// RAM
	{ 0xa001, 0xa001, YM2203_read_port_0_r 	},	// YM2203 #1
//	{ 0xc001, 0xc001, YM2203_read_port_1_r 	},	// YM2203 #2
	{ 0xb000, 0xb000, soundlatch_r			},	// From Main CPU
	{ -1 }
};

static struct MemoryWriteAddress skyfox_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM					},	// ROM
	{ 0x8000, 0x87ff, MWA_RAM					},	// RAM
//	{ 0x9000, 0x9001, MWA_NOP					},	// ??
	{ 0xa000, 0xa000, YM2203_control_port_0_w 	},	// YM2203 #1
	{ 0xa001, 0xa001, YM2203_write_port_0_w 	},	//
//	{ 0xb000, 0xb001, MWA_NOP					},	// ??
	{ 0xc000, 0xc000, YM2203_control_port_1_w 	},	// YM2203 #2
	{ 0xc001, 0xc001, YM2203_write_port_1_w 	},	//
	{ -1 }
};

static struct IOReadPort skyfox_sound_readport[] =
{
	{ -1 }
};
static struct IOWritePort skyfox_sound_writeport[] =
{
	{ -1 }
};





/***************************************************************************


								Input Ports


***************************************************************************/

INPUT_PORTS_START( skyfox )

	PORT_START	// IN0 - Player 1
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON2        )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_BUTTON1        )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_START1         )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_START2         )


	PORT_START	// IN1 - DSW
	PORT_DIPNAME( 0x01, 0x01, "Unknown 1-0" )		// rest unused?
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 1-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 1-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20K" )
	PORT_DIPSETTING(    0x08, "30K" )
	PORT_DIPSETTING(    0x10, "40K" )
	PORT_DIPSETTING(    0x18, "50K" )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 1-5*" )		// used
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )


	PORT_START	// IN2 - Coins + DSW + Vblank
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_VBLANK  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_COIN1   )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, "1 Coin/1 Credit  2C/1C" )	// coin A & B
	PORT_DIPSETTING(    0x04, "1 Coin/2 Credits 3C/1C" )
	PORT_DIPSETTING(    0x08, "1 Coin/3 Credits 4C/1C" )
	PORT_DIPSETTING(    0x0c, "1 Coin/4 Credits 5C/1C" )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )


	PORT_START	// IN3 - DSW
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x03, "4" )
	PORT_DIPSETTING(    0x04, "5" )
//	PORT_DIPSETTING(    0x05, "5" )
//	PORT_DIPSETTING(    0x06, "5" )
	PORT_BITX( 0x07, 0x07, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )


	PORT_START	// IN4 - Fake input port, polled every VBLANK to generate an NMI upon coin insertion
	PORT_BIT_IMPULSE(  0x01, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE(  0x02, IP_ACTIVE_LOW, IPT_COIN2, 1 )


INPUT_PORTS_END




/***************************************************************************


								Graphics Layouts


***************************************************************************/

/* 8x8x8 tiles (note that the tiles in the ROMs are 32x32x8, but
   we cut them in 8x8x8 ones in the init function, in order to
   support 8x8, 16x16 and 32x32 sprites. */

static struct GfxLayout layout_8x8x8 =
{
	8,8,
	RGN_FRAC(1,1),
	8,
	{0,1,2,3,4,5,6,7},
	{0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8},
	{0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64},
	8*8*8
};

/***************************************************************************
								Sky Fox
***************************************************************************/

static struct GfxDecodeInfo skyfox_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_8x8x8,   0, 1 }, // [0] Sprites
	{ -1 }
};






/***************************************************************************


								Machine Drivers


***************************************************************************/


/***************************************************************************
								Sky Fox
***************************************************************************/

/* Check for coin insertion once a frame (polling a fake input port).
   Generate an NMI in case. Scroll the background too. */

static int skyfox_interrupt(void)
{
	/* Scroll the bg */
	skyfox_bg_pos += (skyfox_bg_ctrl >> 1) & 0x7;	// maybe..

	/* Check coin 1 & 2 */
	if ((readinputport(4) & 3) == 3)	return ignore_interrupt();
	else								return nmi_interrupt();
}

static struct YM2203interface skyfox_ym2203_interface =
{
	2,
	1748000,		/* ? same as sound cpu ? */
	{ YM2203_VOL(80,80), YM2203_VOL(80,80) },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};

static struct MachineDriver machine_driver_skyfox =
{
	{
		{
			CPU_Z80,
			4000000,
			skyfox_readmem,skyfox_writemem, skyfox_readport,skyfox_writeport,
			skyfox_interrupt, 1		/* NMI caused by coin insertion */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			1748000,
			skyfox_sound_readmem,skyfox_sound_writemem, skyfox_sound_readport,skyfox_sound_writeport,
			ignore_interrupt, 1		/* No interrupts */
		}
	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,	// we're using IPT_VBLANK
	1,
	0,

	/* video hardware */
	512, 256, { 0+0x60, 320-1+0x60, 0+16, 256-1-16 },	// from $30*2 to $CC*2+8
	skyfox_gfxdecodeinfo,
	256+256, 256,		/* 256 static colors (+256 for the background??) */
	skyfox_vh_convert_color_prom,
	VIDEO_TYPE_RASTER,
	0,
	0,	/* No need for a vh_start function for now */
	0,
	skyfox_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&skyfox_ym2203_interface
		}
	}
};






/***************************************************************************


								ROMs Loading


***************************************************************************/



/***************************************************************************

									Sky Fox


c042	:	Lives
c044-5	:	Score (BCD)
c048-9	:	Power (BCD)

***************************************************************************/

/***************************************************************************

								Exerizer [Bootleg]

malcor

Location     Type     File ID    Checksum
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
TB 5E       27C256      1-J        F302      [  background  ]
TB 5N       27C256      1-I        F5E3      [    sound     ]
LB          27C256      1-A        A53E      [  program 1   ]
LB          27C256      1-B        382C      [  program 2   ]
LB          27C512      1-C        CDAC      [     GFX      ]
LB          27C512      1-D        9C7A      [     GFX      ]
LB          27C512      1-E        D808      [     GFX      ]
LB          27C512      1-F        F87E      [     GFX      ]
LB          27C512      1-G        9709      [     GFX      ]
LB          27C512      1-H        DFDA      [     GFX      ]
TB          82S129      1.BPR      0972      [ video blue   ]
TB          82S129      2.BPR      0972      [ video red    ]
TB          82S129      3.BPR      0972      [ video green  ]

Lower board ROM locations:

---=======------=======----
|    CN2          CN1     |
|                     1-A |
|                         |
|                     1-B |
|                         |
|                         |
|              1 1 1 1 1 1|
|              H G F E D C|
---------------------------

Notes  -  This archive is of a bootleg copy,
       -  Japanese program revision
       -  Although the colour PROMs have the same checksums,
          they are not the same.

Main processor  - Z80  4MHz
Sound processor - Z80  1.748MHz
                - YM2203C x2

***************************************************************************/


ROM_START( skyfox )
	ROM_REGION( 0x10000, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "skyfox1.bin", 0x00000, 0x8000, 0xb4d4bb6f )
	ROM_LOAD( "skyfox2.bin", 0x08000, 0x8000, 0xe15e0263 )	// identical halves

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "skyfox9.bin", 0x00000, 0x8000, 0x0b283bf5 )

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "skyfox3.bin", 0x00000, 0x10000, 0x3a17a929 )
	ROM_LOAD( "skyfox4.bin", 0x10000, 0x10000, 0x358053bb )
	ROM_LOAD( "skyfox5.bin", 0x20000, 0x10000, 0xc1215a6e )
	ROM_LOAD( "skyfox6.bin", 0x30000, 0x10000, 0xcc37e15d )
	ROM_LOAD( "skyfox7.bin", 0x40000, 0x10000, 0xfa2ab5b4 )
	ROM_LOAD( "skyfox8.bin", 0x50000, 0x10000, 0x0e3edc49 )

	ROM_REGION( 0x08000, REGION_GFX2 )	/* Background - do not dispose */
	ROM_LOAD( "skyfox10.bin", 0x0000, 0x8000, 0x19f58f9c )

	ROM_REGION( 0x300, REGION_PROMS )	/* Color Proms */
	ROM_LOAD( "sfoxrprm.bin", 0x000, 0x100, 0x79913c7f )	// R
	ROM_LOAD( "sfoxgprm.bin", 0x100, 0x100, 0xfb73d434 )	// G
	ROM_LOAD( "sfoxbprm.bin", 0x200, 0x100, 0x60d2ab41 )	// B
ROM_END

ROM_START( exerizrb )
	ROM_REGION( 0x10000, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "1-a",         0x00000, 0x8000, 0x5df72a5d )
	ROM_LOAD( "skyfox2.bin", 0x08000, 0x8000, 0xe15e0263 )	// 1-b

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "skyfox9.bin", 0x00000, 0x8000, 0x0b283bf5 )	// 1-i

	ROM_REGION( 0x60000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "1-c",         0x00000, 0x10000, 0x450e9381 )
	ROM_LOAD( "skyfox4.bin", 0x10000, 0x10000, 0x358053bb )	// 1-d
	ROM_LOAD( "1-e",         0x20000, 0x10000, 0x50a38c60 )
	ROM_LOAD( "skyfox6.bin", 0x30000, 0x10000, 0xcc37e15d )	// 1-f
	ROM_LOAD( "1-g",         0x40000, 0x10000, 0xc9bbfe5c )
	ROM_LOAD( "skyfox8.bin", 0x50000, 0x10000, 0x0e3edc49 )	// 1-h

	ROM_REGION( 0x08000, REGION_GFX2 )	/* Background - do not dispose */
	ROM_LOAD( "skyfox10.bin", 0x0000, 0x8000, 0x19f58f9c )	// 1-j

	ROM_REGION( 0x300, REGION_PROMS )	/* Color Proms */
	ROM_LOAD( "sfoxrprm.bin", 0x000, 0x100, 0x79913c7f )	// 2-bpr
	ROM_LOAD( "sfoxgprm.bin", 0x100, 0x100, 0xfb73d434 )	// 3-bpr
	ROM_LOAD( "sfoxbprm.bin", 0x200, 0x100, 0x60d2ab41 )	// 1-bpr
ROM_END




/* Untangle the graphics: cut each 32x32x8 tile in 16 8x8x8 tiles */
void init_skyfox(void)
{
	unsigned char *RAM = memory_region(REGION_GFX1);
	unsigned char *end = RAM + memory_region_length(REGION_GFX1);
	unsigned char buf[32*32];

	while (RAM < end)
	{
		int i;
		for (i=0;i<(32*32);i++)
			buf[i] = RAM[(i%8) + ((i/8)%8)*32 + ((i/64)%4)*8 + (i/256)*256];

		memcpy(RAM,buf,32*32);
		RAM += 32*32;
	}
}



GAME( 1987, skyfox,   0,      skyfox, skyfox, skyfox, ROT90_16BIT, "Jaleco (Nichibutsu USA License)", "Sky Fox"  )
GAME( 1987, exerizrb, skyfox, skyfox, skyfox, skyfox, ROT90_16BIT, "Jaleco", "Exerizer (Japan) (bootleg)" )
