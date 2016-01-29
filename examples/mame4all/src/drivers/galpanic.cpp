#include "../vidhrdw/galpanic.cpp"

/***************************************************************************

The current ROM set is strange because two ROMs overlap two others replacing
the program.

It's definitely a Kaneko boardset, but it could very well be they converted
some other game to run Gals Panic, because there's some ROMs piggybacked
on top of each other and some ROMs on a daughterboard plugged into smaller
sized ROM sockets. It's not a pirate version. The piggybacked ROMs even have
Kaneko stickers. The silkscreen on the board says PAMERA-4.

There is at least another version of the Gals Panic board. It's single board,
so no daughterboard. There are only 4 IC's socketed, the rest is soldered to
the board, and no piggybacked ROMs. Board number is MDK 321 V-0    EXPRO-02

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *galpanic_bgvideoram,*galpanic_fgvideoram;
extern size_t galpanic_fgvideoram_size;

void galpanic_init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom);
READ_HANDLER( galpanic_bgvideoram_r );
WRITE_HANDLER( galpanic_bgvideoram_w );
READ_HANDLER( galpanic_fgvideoram_r );
WRITE_HANDLER( galpanic_fgvideoram_w );
READ_HANDLER( galpanic_paletteram_r );
WRITE_HANDLER( galpanic_paletteram_w );
READ_HANDLER( galpanic_spriteram_r );
WRITE_HANDLER( galpanic_spriteram_w );
void galpanic_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);




int galpanic_interrupt(void)
{
	/* IRQ 3 drives the game, IRQ 5 updates the palette */
	if (cpu_getiloops() != 0) return 5;
	else return 3;
}

WRITE_HANDLER( galpanic_6295_bankswitch_w )
{
	static unsigned char bank[2];
	unsigned char *RAM = memory_region(REGION_SOUND1);


	COMBINE_WORD_MEM(bank,data);

	memcpy(&RAM[0x30000],&RAM[0x40000 + ((data >> 8) & 0x0f) * 0x10000],0x10000);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x3fffff, MRA_ROM },
	{ 0x400000, 0x400001, OKIM6295_status_0_r },
	{ 0x500000, 0x51ffff, galpanic_fgvideoram_r },
	{ 0x520000, 0x53ffff, galpanic_bgvideoram_r },
	{ 0x600000, 0x6007ff, galpanic_paletteram_r },
	{ 0x700000, 0x7047ff, galpanic_spriteram_r },
	{ 0x800000, 0x800001, input_port_0_r },
	{ 0x800002, 0x800003, input_port_1_r },
	{ 0x800004, 0x800005, input_port_2_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x3fffff, MWA_ROM },
	{ 0x400000, 0x400001, OKIM6295_data_0_w },
	{ 0x500000, 0x51ffff, galpanic_fgvideoram_w, &galpanic_fgvideoram, &galpanic_fgvideoram_size },
	{ 0x520000, 0x53ffff, galpanic_bgvideoram_w, &galpanic_bgvideoram },	/* + work RAM */
	{ 0x600000, 0x6007ff, galpanic_paletteram_w, &paletteram },	/* 1024 colors, but only 512 seem to be used */
	{ 0x700000, 0x7047ff, galpanic_spriteram_w, &spriteram, &spriteram_size },
	{ 0x900000, 0x900001, galpanic_6295_bankswitch_w },
	{ 0xa00000, 0xa00001, MWA_NOP },	/* ??? */
	{ 0xb00000, 0xb00001, MWA_NOP },	/* ??? */
	{ 0xc00000, 0xc00001, MWA_NOP },	/* ??? */
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( galpanic )
	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Unknown ) )	/* flip screen? */
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0004, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unknown ) )	/* might affect coinage according to manual, */
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )		/* but settings below don't match */
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )// BUTTON2 ) used in test mode
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0003, 0x0003, "Difficulty?" )
	PORT_DIPSETTING(      0x0002, "Easy?" )
	PORT_DIPSETTING(      0x0003, "Normal?" )
	PORT_DIPSETTING(      0x0001, "Hard?" )
	PORT_DIPSETTING(      0x0000, "Hardest?" )
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0020, "4" )
	PORT_DIPSETTING(      0x0000, "5" )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )	/* manual says demo sounds but has no effect */
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Test Mode" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )//BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	8192,	/* 8192 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
			64*4, 65*4, 66*4, 67*4, 68*4, 69*4, 70*4, 71*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,  256, 16 },
	{ -1 } /* end of array */
};



static struct OKIM6295interface okim6295_interface =
{
	1,                  /* 1 chip */
	{ 12000 },          /* 12000Hz frequency */
	{ REGION_SOUND1 },  /* memory region */
	{ 100 }
};



static struct MachineDriver machine_driver_galpanic =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,	/* 8 Mhz ??? */
			readmem,writemem,0,0,
			galpanic_interrupt,2
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0, 224-1 },
	gfxdecodeinfo,
	1024 + 32768, 1024,
	galpanic_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	generic_bitmapped_vh_start,
	generic_bitmapped_vh_stop,
	galpanic_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( galpanic )
	ROM_REGION( 0x400000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "pm110.4m2",    0x000000, 0x080000, 0xae6b17a8 )
	ROM_LOAD_ODD ( "pm109.4m1",    0x000000, 0x080000, 0xb85d792d )
	/* The above two ROMs contain valid 68000 code, but the game doesn't */
	/* work. I think there might be a protection (addressed at e00000). */
	/* The two following ROMs replace the code with a working version. */
	ROM_LOAD_EVEN( "pm112.6",      0x000000, 0x020000, 0x7b972b58 )
	ROM_LOAD_ODD ( "pm111.5",      0x000000, 0x020000, 0x4eb7298d )
	ROM_LOAD_ODD ( "pm004e.8",     0x100000, 0x080000, 0xd3af52bc )
	ROM_LOAD_EVEN( "pm005e.7",     0x100000, 0x080000, 0xd7ec650c )
	ROM_LOAD_ODD ( "pm000e.15",    0x200000, 0x080000, 0x5d220f3f )
	ROM_LOAD_EVEN( "pm001e.14",    0x200000, 0x080000, 0x90433eb1 )
	ROM_LOAD_ODD ( "pm002e.17",    0x300000, 0x080000, 0x713ee898 )
	ROM_LOAD_EVEN( "pm003e.16",    0x300000, 0x080000, 0x6bb060fd )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "pm006e.67",    0x000000, 0x100000, 0x57aec037 )

	ROM_REGION( 0x140000, REGION_SOUND1 )	/* 1024k for ADPCM samples - sound chip is OKIM6295 */
	/* 00000-2ffff is fixed, 30000-3ffff is bank switched from all the ROMs */
	ROM_LOAD( "pm008e.l",     0x00000, 0x80000, 0xd9379ba8 )
	ROM_RELOAD(               0x40000, 0x80000 )
	ROM_LOAD( "pm007e.u",     0xc0000, 0x80000, 0xc7ed7950 )
ROM_END



GAME( 1990, galpanic, 0, galpanic, galpanic, 0, ROT90_16BIT, "Kaneko", "Gals Panic" )
