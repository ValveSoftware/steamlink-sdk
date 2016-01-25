#include "../vidhrdw/goindol.cpp"

/***************************************************************************
  GOINDOL

  Driver provided by Jarek Parchanski (jpdev@friko6.onet.pl)
***************************************************************************/

#include "driver.h"

int  goindol_vh_start(void);
void goindol_vh_stop(void);
WRITE_HANDLER( goindol_fg_videoram_w );
WRITE_HANDLER( goindol_bg_videoram_w );
void goindol_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void goindol_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

extern unsigned char	*goindol_fg_scrollx;
extern unsigned char	*goindol_fg_scrolly;
extern unsigned char 	*goindol_fg_videoram;
extern unsigned char 	*goindol_bg_videoram;
extern unsigned char 	*goindol_spriteram1;
extern unsigned char 	*goindol_spriteram2;
extern size_t goindol_spriteram_size;
extern size_t goindol_fg_videoram_size;
extern size_t goindol_bg_videoram_size;
extern int 	 	goindol_char_bank;


WRITE_HANDLER( goindol_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	bankaddress = 0x10000 + ((data & 3) * 0x4000);
	cpu_setbank(1,&RAM[bankaddress]);

	goindol_char_bank = data & 0x10;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc800, MRA_NOP },
	{ 0xd000, 0xefff, MRA_RAM },
	{ 0xf000, 0xf000, input_port_3_r },
	{ 0xf800, 0xf800, input_port_4_r },
	{ 0xc834, 0xc834, input_port_1_r },
	{ 0xc820, 0xc820, input_port_2_r },
	{ 0xc830, 0xc830, input_port_0_r },
	{ 0xe000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
        { 0xc810, 0xc810, goindol_bankswitch_w },
	{ 0xc820, 0xd820, MWA_RAM, &goindol_fg_scrollx },
	{ 0xc830, 0xd830, MWA_RAM, &goindol_fg_scrolly },
	{ 0xc800, 0xc800, soundlatch_w },
	{ 0xd000, 0xd03f, MWA_RAM, &goindol_spriteram1, &goindol_spriteram_size },
	{ 0xd040, 0xd7ff, MWA_RAM },
	{ 0xd800, 0xdfff, goindol_bg_videoram_w, &goindol_bg_videoram, &goindol_bg_videoram_size },
	{ 0xe000, 0xe03f, MWA_RAM, &goindol_spriteram2 },
	{ 0xe040, 0xe7ff, MWA_RAM },
	{ 0xe800, 0xefff, goindol_fg_videoram_w, &goindol_fg_videoram, &goindol_fg_videoram_size },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd800, 0xd800, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xa000, 0xa000, YM2203_control_port_0_w },
	{ 0xa001, 0xa001, YM2203_write_port_0_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( goindol )

	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN2, 1 )

	PORT_START      /* IN2 - spinner */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL , 40, 10, 0, 0)

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x1c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x1c, "Easiest" )
	PORT_DIPSETTING(    0x18, "Very Very Easy" )
	PORT_DIPSETTING(    0x14, "Very Easy" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Difficult" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "30k and every 50k" )
	PORT_DIPSETTING(    0x05, "50k and every 100k" )
	PORT_DIPSETTING(    0x06, "50k and every 200k" )
	PORT_DIPSETTING(    0x07, "100k and every 200k" )
	PORT_DIPSETTING(    0x01, "10000 only" )
	PORT_DIPSETTING(    0x02, "30000 only" )
	PORT_DIPSETTING(    0x03, "50000 only" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END


INPUT_PORTS_START( homo )

	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN2, 1 )

	PORT_START      /* IN2 - spinner */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL , 40, 10, 0, 0)

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x1c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x1c, "Easiest" )
	PORT_DIPSETTING(    0x18, "Very Very Easy" )
	PORT_DIPSETTING(    0x14, "Very Easy" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Difficult" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "30k and every 50k" )
	PORT_DIPSETTING(    0x05, "50k and every 100k" )
	PORT_DIPSETTING(    0x06, "50k and every 200k" )
	PORT_DIPSETTING(    0x07, "100k and every 200k" )
	PORT_DIPSETTING(    0x01, "10000 only" )
	PORT_DIPSETTING(    0x02, "30000 only" )
	PORT_DIPSETTING(    0x03, "50000 only" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters   */
	4096,	/* 1024 characters  */
	3,	/* 2 bits per pixel */
	{  0, 0x8000*8, 0x10000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0, 8, 16, 24, 32, 40, 48, 56 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 32 },
	{ REGION_GFX2, 0, &charlayout, 0, 32 },
	{ -1 } /* end of array */
};


static struct YM2203interface ym2203_interface =
{
	1,		/* 1 chip */
	2000000,	/* 2 MHz (?) */
	{ YM2203_VOL(25,25) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_goindol =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,        /* 6 Mhz (?) */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* 4 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			interrupt,4
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,32*8+32*8,
	goindol_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	goindol_vh_start,
	goindol_vh_stop,
	goindol_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( goindol )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 2*64k for code */
	ROM_LOAD( "r1", 0x00000, 0x8000, 0x3111c61b ) /* Code 0000-7fff */
	ROM_LOAD( "r2", 0x10000, 0x8000, 0x1ff6e3a2 ) /* Paged data */
	ROM_LOAD( "r3", 0x18000, 0x8000, 0xe9eec24a ) /* Paged data */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "r10", 0x00000, 0x8000, 0x72e1add1 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r4", 0x00000, 0x8000, 0x1ab84225 ) /* Characters */
	ROM_LOAD( "r5", 0x08000, 0x8000, 0x4997d469 )
	ROM_LOAD( "r6", 0x10000, 0x8000, 0x752904b0 )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r7", 0x00000, 0x8000, 0x362f2a27 )
	ROM_LOAD( "r8", 0x08000, 0x8000, 0x9fc7946e )
	ROM_LOAD( "r9", 0x10000, 0x8000, 0xe6212fe4 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "am27s21.pr1", 0x0000, 0x0100, 0x361f0868 )	/* palette red bits   */
	ROM_LOAD( "am27s21.pr2", 0x0100, 0x0100, 0xe355da4d )	/* palette green bits */
	ROM_LOAD( "am27s21.pr3", 0x0200, 0x0100, 0x8534cfb5 )	/* palette blue bits  */
ROM_END

ROM_START( homo )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 2*64k for code */
	ROM_LOAD( "homo.01", 0x00000, 0x8000, 0x28c539ad ) /* Code 0000-7fff */
	ROM_LOAD( "r2", 0x10000, 0x8000, 0x1ff6e3a2 ) /* Paged data */
	ROM_LOAD( "r3", 0x18000, 0x8000, 0xe9eec24a ) /* Paged data */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "r10", 0x00000, 0x8000, 0x72e1add1 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r4", 0x00000, 0x8000, 0x1ab84225 ) /* Characters */
	ROM_LOAD( "r5", 0x08000, 0x8000, 0x4997d469 )
	ROM_LOAD( "r6", 0x10000, 0x8000, 0x752904b0 )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r7", 0x00000, 0x8000, 0x362f2a27 )
	ROM_LOAD( "r8", 0x08000, 0x8000, 0x9fc7946e )
	ROM_LOAD( "r9", 0x10000, 0x8000, 0xe6212fe4 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "am27s21.pr1", 0x0000, 0x0100, 0x361f0868 )	/* palette red bits   */
	ROM_LOAD( "am27s21.pr2", 0x0100, 0x0100, 0xe355da4d )	/* palette green bits */
	ROM_LOAD( "am27s21.pr3", 0x0200, 0x0100, 0x8534cfb5 )	/* palette blue bits  */
ROM_END



void init_goindol(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);


	/* I hope that's all patches to avoid protection */

	rom[0x04a7] = 0xc9;
	rom[0x0641] = 0xc9;
	rom[0x0831] = 0xc9;
	rom[0x0b30] = 0x00;
	rom[0x0c13] = 0xc9;
	rom[0x134e] = 0xc9;
	rom[0x172e] = 0xc9;
	rom[0x1785] = 0xc9;
	rom[0x17cc] = 0xc9;
	rom[0x1aa5] = 0x7b;
	rom[0x1aa6] = 0x17;
	rom[0x1bee] = 0xc9;
	rom[0x218c] = 0x00;
	rom[0x218d] = 0x00;
	rom[0x218e] = 0x00;
	rom[0x333d] = 0xc9;
	rom[0x3365] = 0x00;
}

void init_homo(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);


	rom[0x218c] = 0x00;
	rom[0x218d] = 0x00;
	rom[0x218e] = 0x00;
}



GAME( 1987, goindol, 0,       goindol, goindol, goindol, ROT90, "Sun a Electronics", "Goindol" )
GAME( 1987, homo,    goindol, goindol, homo,    homo,    ROT90, "bootleg", "Homo" )
