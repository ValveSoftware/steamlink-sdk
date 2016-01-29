#include "../vidhrdw/cloud9.cpp"

/***************************************************************************

  Cloud9 (prototype) driver.

  This hardware is yet another variant of the Centipede/Millipede hardware,
  but as you can see there are some significant deviations...

  0000			R/W		X index into the bitmap
  0001			R/W		Y index into the bitmap
  0002			R/W		Current bitmap pixel value
  0003-05FF		R/W		RAM
  0600-3FFF		R/W		Bitmap RAM bank 0 (and bank 1 ?)
  5000-5073		R/W		Motion Object RAM
  5400			W		Watchdog
  5480			W		IRQ Acknowledge
  5500-557F		W		Color RAM (9 bits, 4 banks, LSB of Blue is addr&$40)

  5580			W		Auto-increment X bitmap index (~D7)
  5581			W		Auto-increment Y bitmap index (~D7)
  5584			W		VRAM Both Banks - (D7) seems to allow writing to both banks
  5585			W		Invert screen?
  5586			W		VRAM Bank select?
  5587			W		Color bank select

  5600			W		Coin Counter 1 (D7)
  5601			W		Coin Counter 2 (D7)
  5602			W		Start1 LED (~D7)
  5603			W		Start2 LED (~D7)

  5680			W		Force Write to EAROM?
  5700			W		EAROM Off?
  5780			W		EAROM On?

  5800			R		IN0 (D7=Vblank, D6=Right Coin, D5=Left Coin, D4=Aux, D3=Self Test)
  5801			R		IN1 (D7=Start1, D6=Start2, D5=Fire, D4=Zap)
  5900			R		Trackball Vert
  5901			R		Trackball Horiz

  5A00-5A0F		R/W		Pokey 1
  5B00-5B0F		R/W		Pokey 2
  5C00-5CFF		W		EAROM
  6000-FFFF		R		Program ROM



If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

WRITE_HANDLER( cloud9_paletteram_w );
READ_HANDLER( cloud9_bitmap_regs_r );
WRITE_HANDLER( cloud9_bitmap_regs_w );
WRITE_HANDLER( cloud9_bitmap_w );
extern void cloud9_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char *cloud9_vram2;
extern unsigned char *cloud9_bitmap_regs;
extern unsigned char *cloud9_auto_inc_x;
extern unsigned char *cloud9_auto_inc_y;
extern unsigned char *cloud9_both_banks;
extern unsigned char *cloud9_vram_bank;
extern unsigned char *cloud9_color_bank;


static unsigned char *nvram;
static size_t nvram_size;

static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
			memset(nvram,0,nvram_size);
	}
}


static WRITE_HANDLER( cloud9_led_w )
{
//	osd_led_w(offset,~data >> 7);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0002, cloud9_bitmap_regs_r },
	{ 0x0003, 0x05ff, MRA_RAM },
	{ 0x0600, 0x3fff, MRA_RAM },
	{ 0x5500, 0x557f, MRA_RAM },
	{ 0x5800, 0x5800, input_port_0_r },
	{ 0x5801, 0x5801, input_port_1_r },
	{ 0x5900, 0x5900, input_port_2_r },
	{ 0x5901, 0x5901, input_port_3_r },
	{ 0x5a00, 0x5a0f, pokey1_r },
	{ 0x5b00, 0x5b0f, pokey2_r },
	{ 0x5c00, 0x5cff, MRA_RAM },	/* EAROM */
	{ 0x6000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0002, cloud9_bitmap_regs_w, &cloud9_bitmap_regs },
	{ 0x0003, 0x05ff, MWA_RAM },
	{ 0x0600, 0x3fff, cloud9_bitmap_w, &videoram, &videoram_size },
	{ 0x5000, 0x50ff, MWA_RAM, &spriteram },
	{ 0x5400, 0x5400, watchdog_reset_w },
	{ 0x5480, 0x5480, MWA_NOP },	/* IRQ Ack */
	{ 0x5500, 0x557f, cloud9_paletteram_w, &paletteram },
	{ 0x5580, 0x5580, MWA_RAM, &cloud9_auto_inc_x },
	{ 0x5581, 0x5581, MWA_RAM, &cloud9_auto_inc_y },
	{ 0x5584, 0x5584, MWA_RAM, &cloud9_both_banks },
	{ 0x5586, 0x5586, MWA_RAM, &cloud9_vram_bank },
	{ 0x5587, 0x5587, MWA_RAM, &cloud9_color_bank },
	{ 0x5600, 0x5601, coin_counter_w },
	{ 0x5602, 0x5603, cloud9_led_w },
	{ 0x5a00, 0x5a0f, pokey1_w },
	{ 0x5b00, 0x5b0f, pokey2_w },
	{ 0x5c00, 0x5cff, MWA_RAM, &nvram, &nvram_size },
	{ 0x6000, 0xffff, MWA_ROM },
	{ 0x10600,0x13fff, MWA_RAM, &cloud9_vram2 },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( cloud9 )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x07, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE( 0x08, IP_ACTIVE_LOW )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START      /* IN1 */
	PORT_BIT ( 0x0F, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* IN2 */
	PORT_ANALOG( 0xff, 0x7f, IPT_TRACKBALL_Y | IPF_REVERSE, 30, 30, 0, 0 )

	PORT_START      /* IN3 */
	PORT_ANALOG( 0xff, 0x7f, IPT_TRACKBALL_X, 30, 30, 0, 0 )

	PORT_START	/* IN4 */ /* DSW1 */
	PORT_BIT ( 0xFF, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN5 */ /* DSW2 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x06, 0x04, DEF_STR( Coinage ) )
	PORT_DIPSETTING (   0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (   0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (   0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x18,  0x00, "Right Coin" )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x08, "*4" )
	PORT_DIPSETTING (   0x10, "*5" )
	PORT_DIPSETTING (   0x18, "*6" )
	PORT_DIPNAME(0x20,  0x00, "Middle Coin" )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x20, "*2" )
	PORT_DIPNAME(0xC0,  0x00, "Bonus Coins" )
	PORT_DIPSETTING (   0xC0, "4 coins + 2 coins" )
	PORT_DIPSETTING (   0x80, "4 coins + 1 coin" )
	PORT_DIPSETTING (   0x40, "2 coins + 1 coin" )
	PORT_DIPSETTING (   0x00, "None" )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	128,	/* 128 characters */
	4,	/* 4 bits per pixel */
	{ 0x3000*8, 0x2000*8, 0x1000*8, 0 },	/* the four bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 8 consecutive bytes, then skip 8 */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	4,	/* 4 bits per pixel */
	{ 0x3000*8, 0x2000*8, 0x1000*8, 0x0000*8 },	/* the four bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8,
			16*8, 18*8, 20*8, 22*8, 24*8, 26*8, 28*8, 30*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0800, &charlayout,   0, 4 },
	{ REGION_GFX1, 0x0808, &charlayout,   0, 4 },
	{ REGION_GFX1, 0x0000, &spritelayout, 0, 4 },
	{ -1 } /* end of array */
};



static struct POKEYinterface pokey_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz??? */
	{ 50, 50 },
	/* The 8 pot handlers */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	/* The allpot handler */
	{ input_port_4_r, input_port_5_r },
};


static struct MachineDriver machine_driver_cloud9 =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			12096000/8,	/* 1.512 Mhz?? */
			readmem,writemem,0,0,
			interrupt,4
		}
	},
	60, 1460,	/* frames per second, vblank duration??? */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 32*8-1 },
	gfxdecodeinfo,
	64, 64,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	generic_bitmapped_vh_start,
	generic_bitmapped_vh_stop,
	cloud9_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	},

	nvram_handler
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( cloud9 )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* 64k for code + extra VRAM space */
	ROM_LOAD( "c9_6000.bin", 0x6000, 0x2000, 0xb5d95d98 )
	ROM_LOAD( "c9_8000.bin", 0x8000, 0x2000, 0x49af8f22 )
	ROM_LOAD( "c9_a000.bin", 0xa000, 0x2000, 0x7cf404a6 )
	ROM_LOAD( "c9_c000.bin", 0xc000, 0x2000, 0x26a4d7df )
	ROM_LOAD( "c9_e000.bin", 0xe000, 0x2000, 0x6e663bce )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c9_gfx0.bin", 0x0000, 0x1000, 0xd01a8019 )
	ROM_LOAD( "c9_gfx1.bin", 0x1000, 0x1000, 0x514ac009 )
	ROM_LOAD( "c9_gfx2.bin", 0x2000, 0x1000, 0x930c1ade )
	ROM_LOAD( "c9_gfx3.bin", 0x3000, 0x1000, 0x27e9b88d )
ROM_END



GAMEX( 1983, cloud9, 0, cloud9, cloud9, 0, ROT0, "Atari", "Cloud 9 (prototype)", GAME_NO_COCKTAIL )
