#include "../vidhrdw/blktiger.cpp"

/***************************************************************************

  Black Tiger

  Driver provided by Paul Leaman

  Thanks to Ishmair for providing information about the screen
  layout on level 3.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"



extern unsigned char *blktiger_backgroundram;
extern unsigned char *blktiger_backgroundattribram;
extern size_t blktiger_backgroundram_size;
extern unsigned char *blktiger_palette_bank;
extern unsigned char *blktiger_screen_layout;

WRITE_HANDLER( blktiger_palette_bank_w );
WRITE_HANDLER( blktiger_screen_layout_w );

READ_HANDLER( blktiger_background_r );
WRITE_HANDLER( blktiger_background_w );
WRITE_HANDLER( blktiger_video_control_w );
WRITE_HANDLER( blktiger_video_enable_w );
WRITE_HANDLER( blktiger_scrollbank_w );
WRITE_HANDLER( blktiger_scrollx_w );
WRITE_HANDLER( blktiger_scrolly_w );

int blktiger_interrupt(void);

int blktiger_vh_start(void);
void blktiger_vh_stop(void);
void blktiger_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



/* this is a protection check. The game crashes (thru a jump to 0x8000) */
/* if a read from this address doesn't return the value it expects. */
static READ_HANDLER( blktiger_protection_r )
{
	int data = cpu_get_reg(Z80_DE) >> 8;
	//logerror("protection read, PC: %04x Result:%02x\n",cpu_get_pc(),data);
	return data;
}

static WRITE_HANDLER( blktiger_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankaddress = 0x10000 + (data & 0x0f) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xcfff, blktiger_background_r },
	{ 0xd000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, blktiger_background_w, &blktiger_backgroundram, &blktiger_backgroundram_size },
	{ 0xd000, 0xd3ff, videoram_w, &videoram, &videoram_size },
	{ 0xd400, 0xd7ff, colorram_w, &colorram },
	{ 0xd800, 0xdbff, paletteram_xxxxBBBBRRRRGGGG_split1_w, &paletteram },
	{ 0xdc00, 0xdfff, paletteram_xxxxBBBBRRRRGGGG_split2_w, &paletteram_2 },
	{ 0xe000, 0xfdff, MWA_RAM },
	{ 0xfe00, 0xffff, MWA_RAM, &spriteram, &spriteram_size },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ 0x02, 0x02, input_port_2_r },
	{ 0x03, 0x03, input_port_3_r },
	{ 0x04, 0x04, input_port_4_r },
	{ 0x05, 0x05, input_port_5_r },
	{ 0x07, 0x07, blktiger_protection_r },        /*DPS 980118*/
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, soundlatch_w },
	{ 0x01, 0x01, blktiger_bankswitch_w },
	{ 0x04, 0x04, blktiger_video_control_w },
	{ 0x06, 0x06, watchdog_reset_w },
	{ 0x07, 0x07, IOWP_NOP }, /* Software protection (7) */
	{ 0x08, 0x09, blktiger_scrollx_w },
	{ 0x0a, 0x0b, blktiger_scrolly_w },
	{ 0x0c, 0x0c, blktiger_video_enable_w },
	{ 0x0d, 0x0d, blktiger_scrollbank_w },   /* Scroll ram bank register */
	{ 0x0e, 0x0e, blktiger_screen_layout_w },/* Video scrolling layout */
#if 0
	{ 0x03, 0x03, IOWP_NOP }, /* Unknown port 3 */
#endif
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc800, soundlatch_r },
	{ 0xe000, 0xe000, YM2203_status_port_0_r },
	{ 0xe001, 0xe001, YM2203_read_port_0_r },
	{ 0xe002, 0xe002, YM2203_status_port_1_r },
	{ 0xe003, 0xe003, YM2203_read_port_1_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xe000, 0xe000, YM2203_control_port_0_w },
	{ 0xe001, 0xe001, YM2203_write_port_0_w },
	{ 0xe002, 0xe002, YM2203_control_port_1_w },
	{ 0xe003, 0xe003, YM2203_write_port_1_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( blktiger )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7")
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x1c, "1 (Easiest)")
	PORT_DIPSETTING(    0x18, "2" )
	PORT_DIPSETTING(    0x14, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x0c, "5 (Normal)" )
	PORT_DIPSETTING(    0x08, "6" )
	PORT_DIPSETTING(    0x04, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Freeze" )	/* could be VBLANK */
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	2048,   /* 2048 characters */
	2,	/* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	2048,   /* 2048 sprites */
	4,	/* 4 bits per pixel */
	{ 0x20000*8+4, 0x20000*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
	32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
	8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   768, 32 },	/* colors 768 - 895 */
	{ REGION_GFX2, 0, &spritelayout,   0, 16 },	/* colors 0 - 255 */
	{ REGION_GFX3, 0, &spritelayout, 512,  8 },	/* colors 512 - 639 */
	{ -1 } /* end of array */
};



/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	3579545,	/* 3.579 MHz ? (hand tuned) */
	{ YM2203_VOL(15,15), YM2203_VOL(15,15) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};



static struct MachineDriver machine_driver_blktiger =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz (?) */
			readmem,writemem,readport,writeport,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,	/* 3 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2203 */
		}
	},
	60, 1500,	/* frames per second, vblank duration - hand tuned to get rid of sprite lag */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	blktiger_vh_start,
	blktiger_vh_stop,
	blktiger_vh_screenrefresh,

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

ROM_START( blktiger )
	ROM_REGION( 0x50000, REGION_CPU1 )	/* 64k for code + banked ROMs images */
	ROM_LOAD( "blktiger.5e",  0x00000, 0x08000, 0xa8f98f22 )	/* CODE */
	ROM_LOAD( "blktiger.6e",  0x10000, 0x10000, 0x7bef96e8 )	/* 0+1 */
	ROM_LOAD( "blktiger.8e",  0x20000, 0x10000, 0x4089e157 )	/* 2+3 */
	ROM_LOAD( "blktiger.9e",  0x30000, 0x10000, 0xed6af6ec )	/* 4+5 */
	ROM_LOAD( "blktiger.10e", 0x40000, 0x10000, 0xae59b72e )	/* 6+7 */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "blktiger.1l",  0x0000, 0x8000, 0x2cf54274 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.2n",  0x00000, 0x08000, 0x70175d78 )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.5b",  0x00000, 0x10000, 0xc4524993 )	/* tiles */
	ROM_LOAD( "blktiger.4b",  0x10000, 0x10000, 0x7932c86f )
	ROM_LOAD( "blktiger.9b",  0x20000, 0x10000, 0xdc49593a )
	ROM_LOAD( "blktiger.8b",  0x30000, 0x10000, 0x7ed7a122 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.5a",  0x00000, 0x10000, 0xe2f17438 )	/* sprites */
	ROM_LOAD( "blktiger.4a",  0x10000, 0x10000, 0x5fccbd27 )
	ROM_LOAD( "blktiger.9a",  0x20000, 0x10000, 0xfc33ccc6 )
	ROM_LOAD( "blktiger.8a",  0x30000, 0x10000, 0xf449de01 )

	ROM_REGION( 0x0400, REGION_PROMS )	/* PROMs (function unknown) */
	ROM_LOAD( "mb7114e.8j",   0x0000, 0x0100, 0x29b459e5 )
	ROM_LOAD( "mb7114e.9j",   0x0100, 0x0100, 0x8b741e66 )
	ROM_LOAD( "mb7114e.11k",  0x0200, 0x0100, 0x27201c75 )
	ROM_LOAD( "mb7114e.11l",  0x0300, 0x0100, 0xe5490b68 )
ROM_END

ROM_START( bktigerb )
	ROM_REGION( 0x50000, REGION_CPU1 )	/* 64k for code + banked ROMs images */
	ROM_LOAD( "btiger1.f6",   0x00000, 0x08000, 0x9d8464e8 )	/* CODE */
	ROM_LOAD( "blktiger.6e",  0x10000, 0x10000, 0x7bef96e8 )	/* 0+1 */
	ROM_LOAD( "btiger3.j6",   0x20000, 0x10000, 0x52c56ed1 )	/* 2+3 */
	ROM_LOAD( "blktiger.9e",  0x30000, 0x10000, 0xed6af6ec )	/* 4+5 */
	ROM_LOAD( "blktiger.10e", 0x40000, 0x10000, 0xae59b72e )	/* 6+7 */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "blktiger.1l",  0x0000, 0x8000, 0x2cf54274 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.2n",  0x00000, 0x08000, 0x70175d78 )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.5b",  0x00000, 0x10000, 0xc4524993 )	/* tiles */
	ROM_LOAD( "blktiger.4b",  0x10000, 0x10000, 0x7932c86f )
	ROM_LOAD( "blktiger.9b",  0x20000, 0x10000, 0xdc49593a )
	ROM_LOAD( "blktiger.8b",  0x30000, 0x10000, 0x7ed7a122 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.5a",  0x00000, 0x10000, 0xe2f17438 )	/* sprites */
	ROM_LOAD( "blktiger.4a",  0x10000, 0x10000, 0x5fccbd27 )
	ROM_LOAD( "blktiger.9a",  0x20000, 0x10000, 0xfc33ccc6 )
	ROM_LOAD( "blktiger.8a",  0x30000, 0x10000, 0xf449de01 )

	ROM_REGION( 0x0400, REGION_PROMS )	/* PROMs (function unknown) */
	ROM_LOAD( "mb7114e.8j",   0x0000, 0x0100, 0x29b459e5 )
	ROM_LOAD( "mb7114e.9j",   0x0100, 0x0100, 0x8b741e66 )
	ROM_LOAD( "mb7114e.11k",  0x0200, 0x0100, 0x27201c75 )
	ROM_LOAD( "mb7114e.11l",  0x0300, 0x0100, 0xe5490b68 )
ROM_END

ROM_START( blkdrgon )
	ROM_REGION( 0x50000, REGION_CPU1 )	/* 64k for code + banked ROMs images */
	ROM_LOAD( "blkdrgon.5e",  0x00000, 0x08000, 0x27ccdfbc )	/* CODE */
	ROM_LOAD( "blkdrgon.6e",  0x10000, 0x10000, 0x7d39c26f )	/* 0+1 */
	ROM_LOAD( "blkdrgon.8e",  0x20000, 0x10000, 0xd1bf3757 )	/* 2+3 */
	ROM_LOAD( "blkdrgon.9e",  0x30000, 0x10000, 0x4d1d6680 )	/* 4+5 */
	ROM_LOAD( "blkdrgon.10e", 0x40000, 0x10000, 0xc8d0c45e )	/* 6+7 */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "blktiger.1l",  0x0000, 0x8000, 0x2cf54274 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blkdrgon.2n",  0x00000, 0x08000, 0x3821ab29 )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blkdrgon.5b",  0x00000, 0x10000, 0x22d0a4b0 )	/* tiles */
	ROM_LOAD( "blkdrgon.4b",  0x10000, 0x10000, 0xc8b5fc52 )
	ROM_LOAD( "blkdrgon.9b",  0x20000, 0x10000, 0x9498c378 )
	ROM_LOAD( "blkdrgon.8b",  0x30000, 0x10000, 0x5b0df8ce )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.5a",  0x00000, 0x10000, 0xe2f17438 )	/* sprites */
	ROM_LOAD( "blktiger.4a",  0x10000, 0x10000, 0x5fccbd27 )
	ROM_LOAD( "blktiger.9a",  0x20000, 0x10000, 0xfc33ccc6 )
	ROM_LOAD( "blktiger.8a",  0x30000, 0x10000, 0xf449de01 )

	ROM_REGION( 0x0400, REGION_PROMS )	/* PROMs (function unknown) */
	ROM_LOAD( "mb7114e.8j",   0x0000, 0x0100, 0x29b459e5 )
	ROM_LOAD( "mb7114e.9j",   0x0100, 0x0100, 0x8b741e66 )
	ROM_LOAD( "mb7114e.11k",  0x0200, 0x0100, 0x27201c75 )
	ROM_LOAD( "mb7114e.11l",  0x0300, 0x0100, 0xe5490b68 )
ROM_END

ROM_START( blkdrgnb )
	ROM_REGION( 0x50000, REGION_CPU1 )	/* 64k for code + banked ROMs images */
	ROM_LOAD( "j1-5e",        0x00000, 0x08000, 0x97e84412 )	/* CODE */
	ROM_LOAD( "blkdrgon.6e",  0x10000, 0x10000, 0x7d39c26f )	/* 0+1 */
	ROM_LOAD( "j3-8e",        0x20000, 0x10000, 0xf4cd0f39 )	/* 2+3 */
	ROM_LOAD( "blkdrgon.9e",  0x30000, 0x10000, 0x4d1d6680 )	/* 4+5 */
	ROM_LOAD( "blkdrgon.10e", 0x40000, 0x10000, 0xc8d0c45e )	/* 6+7 */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "blktiger.1l",  0x0000, 0x8000, 0x2cf54274 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "j15-2n",       0x00000, 0x08000, 0x852ad2b7 )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blkdrgon.5b",  0x00000, 0x10000, 0x22d0a4b0 )	/* tiles */
	ROM_LOAD( "j11-4b",       0x10000, 0x10000, 0x053ab15c )
	ROM_LOAD( "blkdrgon.9b",  0x20000, 0x10000, 0x9498c378 )
	ROM_LOAD( "j13-8b",       0x30000, 0x10000, 0x663d5afa )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "blktiger.5a",  0x00000, 0x10000, 0xe2f17438 )	/* sprites */
	ROM_LOAD( "blktiger.4a",  0x10000, 0x10000, 0x5fccbd27 )
	ROM_LOAD( "blktiger.9a",  0x20000, 0x10000, 0xfc33ccc6 )
	ROM_LOAD( "blktiger.8a",  0x30000, 0x10000, 0xf449de01 )

	ROM_REGION( 0x0400, REGION_PROMS )	/* PROMs (function unknown) */
	ROM_LOAD( "mb7114e.8j",   0x0000, 0x0100, 0x29b459e5 )
	ROM_LOAD( "mb7114e.9j",   0x0100, 0x0100, 0x8b741e66 )
	ROM_LOAD( "mb7114e.11k",  0x0200, 0x0100, 0x27201c75 )
	ROM_LOAD( "mb7114e.11l",  0x0300, 0x0100, 0xe5490b68 )
ROM_END



GAMEX( 1987, blktiger, 0,        blktiger, blktiger, 0, ROT0, "Capcom", "Black Tiger", GAME_NO_COCKTAIL )
GAMEX( 1987, bktigerb, blktiger, blktiger, blktiger, 0, ROT0, "bootleg", "Black Tiger (bootleg)", GAME_NO_COCKTAIL )
GAMEX( 1987, blkdrgon, blktiger, blktiger, blktiger, 0, ROT0, "Capcom", "Black Dragon", GAME_NO_COCKTAIL )
GAMEX( 1987, blkdrgnb, blktiger, blktiger, blktiger, 0, ROT0, "bootleg", "Black Dragon (bootleg)", GAME_NO_COCKTAIL )
