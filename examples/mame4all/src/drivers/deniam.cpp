#include "../vidhrdw/deniam.cpp"

/***************************************************************************

Deniam games

driver by Nicola Salmoria


http://deniam.co.kr/text/c_game01.htm

Title            System     Date
---------------- ---------- ----------
GO!GO!           deniam-16b 1995/10/11
Logic Pro        deniam-16b 1996/10/20
Karian Cross     deniam-16b 1997/04/17
LOTTERY GAME     deniam-16c 1997/05/21
Logic Pro 2      deniam-16c 1997/06/20
Propose          deniam-16c 1997/06/21

They call the hardware "deniam-16", but it's actually pretty much identical to
Sega System 16.


Notes:

- The logicpr2 OKIM6295 ROM has four banks, but the game seems to only use 0 and 1.
- logicpro dip switches might be wrong (using the logicpr2 ones)
- flip screen is not supported but these games don't use it (no flip screen dip
  and no cocktail mode)
- if it's like System 16, the top bit of palette ram should be an additional bit
  for Green. But is it ever not 0?
- with a more aggressive palette marking, logicpr2 should fit in 256 colors

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


extern unsigned char *deniam_videoram,*deniam_textram;

void init_logicpro(void);
void init_karianx(void);
READ_HANDLER( deniam_videoram_r );
WRITE_HANDLER( deniam_videoram_w );
READ_HANDLER( deniam_textram_r );
WRITE_HANDLER( deniam_textram_w );
WRITE_HANDLER( deniam_palette_w );
READ_HANDLER( deniam_coinctrl_r );
WRITE_HANDLER( deniam_coinctrl_w );
int deniam_vh_start(void);
void deniam_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);




static WRITE_HANDLER( sound_command_w )
{
	if ((data & 0xff000000) == 0)
	{
		soundlatch_w(offset,(data >> 8) & 0xff);
		cpu_cause_interrupt(1,Z80_NMI_INT);
	}
}

static WRITE_HANDLER( deniam16b_oki_rom_bank_w )
{
	OKIM6295_set_bank_base(0,ALL_VOICES,(data & 0x40) ? 0x40000 : 0x00000);
}

static WRITE_HANDLER( deniam16c_oki_rom_bank_w )
{
	if ((data & 0x00ff0000) == 0)
		OKIM6295_set_bank_base(0,ALL_VOICES,(data & 0x01) ? 0x40000 : 0x00000);
}

static void deniam_init_machine(void)
{
	/* logicpr2 does not reset the bank base on startup */
	OKIM6295_set_bank_base(0,ALL_VOICES,0x00000);
}

static WRITE_HANDLER( YM3812_control_port_0_halfword_swap_w )
{
	if ((data & 0xff000000) == 0)
		YM3812_control_port_0_w(0,(data >> 8) & 0xff);
}

static WRITE_HANDLER( YM3812_write_port_0_halfword_swap_w )
{
	if ((data & 0xff000000) == 0)
		YM3812_write_port_0_w(0,(data >> 8) & 0xff);
}

static WRITE_HANDLER( OKIM6295_data_0_halfword_w )
{
	if ((data & 0x00ff0000) == 0)
		OKIM6295_data_0_w(0,data & 0xff);
}



static struct MemoryReadAddress deniam16b_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },
	{ 0x400000, 0x40ffff, deniam_videoram_r },
	{ 0x410000, 0x410fff, deniam_textram_r },
	{ 0xc40002, 0xc40003, deniam_coinctrl_r },
	{ 0xc44000, 0xc44001, input_port_0_r },
	{ 0xc44002, 0xc44003, input_port_1_r },
	{ 0xc44004, 0xc44005, input_port_2_r },
	{ 0xc44006, 0xc44007, MRA_NOP },	/* unused? */
	{ 0xc4400a, 0xc4400b, input_port_3_r },
	{ 0xff0000, 0xffffff, MRA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress deniam16b_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },
	{ 0x400000, 0x40ffff, deniam_videoram_w, &deniam_videoram },
	{ 0x410000, 0x410fff, deniam_textram_w, &deniam_textram },
	{ 0x440000, 0x4407ff, MWA_BANK2, &spriteram, &spriteram_size },
	{ 0x840000, 0x840fff, deniam_palette_w, &paletteram },
	{ 0xc40000, 0xc40001, sound_command_w },
	{ 0xc40002, 0xc40003, deniam_coinctrl_w },
	{ 0xc40004, 0xc40005, MWA_NOP },	/* irq ack? */
	{ 0xff0000, 0xffffff, MWA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0xf7ff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x01, 0x01, soundlatch_r },
	{ 0x05, 0x05, OKIM6295_status_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x02, 0x02, YM3812_control_port_0_w },
	{ 0x03, 0x03, YM3812_write_port_0_w },
	{ 0x05, 0x05, OKIM6295_data_0_w },
	{ 0x07, 0x07, deniam16b_oki_rom_bank_w },
	{ -1 }	/* end of table */
};


/* identical to 16b, but handles sound directly */
static struct MemoryReadAddress deniam16c_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },
	{ 0x400000, 0x40ffff, deniam_videoram_r },
	{ 0x410000, 0x410fff, deniam_textram_r },
	{ 0xc40000, 0xc40001, OKIM6295_status_0_r },
	{ 0xc40002, 0xc40003, deniam_coinctrl_r },
	{ 0xc44000, 0xc44001, input_port_0_r },
	{ 0xc44002, 0xc44003, input_port_1_r },
	{ 0xc44004, 0xc44005, input_port_2_r },
	{ 0xc44006, 0xc44007, MRA_NOP },	/* unused? */
	{ 0xc4400a, 0xc4400b, input_port_3_r },
	{ 0xff0000, 0xffffff, MRA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress deniam16c_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },
	{ 0x400000, 0x40ffff, deniam_videoram_w, &deniam_videoram },
	{ 0x410000, 0x410fff, deniam_textram_w, &deniam_textram },
	{ 0x440000, 0x4407ff, MWA_BANK2, &spriteram, &spriteram_size },
	{ 0x840000, 0x840fff, deniam_palette_w, &paletteram },
	{ 0xc40000, 0xc40001, OKIM6295_data_0_halfword_w },
	{ 0xc40002, 0xc40003, deniam_coinctrl_w },
	{ 0xc40004, 0xc40005, MWA_NOP },	/* irq ack? */
	{ 0xc40006, 0xc40007, deniam16c_oki_rom_bank_w },
	{ 0xc40008, 0xc40009, YM3812_control_port_0_halfword_swap_w },
	{ 0xc4000a, 0xc4000b, YM3812_write_port_0_halfword_swap_w },
	{ 0xff0000, 0xffffff, MWA_BANK1 },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( karianx )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )

	PORT_START
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Very Easy" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x20, "Demo Music" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( logicpr2 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )

	PORT_START
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x18, 0x18, "Play Time" )
	PORT_DIPSETTING(    0x08, "Slow" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(2,3), RGN_FRAC(1,3), RGN_FRAC(0,3) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 128 },	/* colors 0-1023 */
												/* sprites use colors 1024-2047 */
	{ -1 } /* end of array */
};


static void irqhandler(int linestate)
{
	/* system 16c doesn't have the sound CPU */
	if (Machine->drv->cpu[1].cpu_type)
		cpu_set_irq_line(1,0,linestate);
}

static struct YM3812interface ym3812_interface =
{
	1,			/* 1 chip */
	25000000/8,	/* ??? */
	{ 30 },	/* volume */
	{ irqhandler },
};

static struct OKIM6295interface okim6295_interface =
{
	1,                  /* 1 chip */
	{ 8000 },           /* 8000Hz frequency */
	{ REGION_SOUND1 },	/* memory region */
	{ 100 }				/* volume */
};



static struct MachineDriver machine_driver_deniam16b =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			25000000/2,	/* ??? */
			deniam16b_readmem,deniam16b_writemem,0,0,
			m68_level4_irq,1
		},
		{
			CPU_Z80,
			25000000/4,	/* (makes logicpro music tempo correct) */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* NMI is caused by the main cpu */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	deniam_init_machine,

	/* video hardware */
	512, 256, { 24*8, 64*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	deniam_vh_start,
	0,
	deniam_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_deniam16c =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			25000000/2,	/* ??? */
			deniam16c_readmem,deniam16c_writemem,0,0,
			m68_level4_irq,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	deniam_init_machine,

	/* video hardware */
	512, 256, { 24*8, 64*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	deniam_vh_start,
	0,
	deniam_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( logicpro )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "logicpro.r4", 0x00000, 0x40000, 0xc506d484 )
	ROM_LOAD_ODD ( "logicpro.r3", 0x00000, 0x40000, 0xd5a4cf62 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "logicpro.r2", 0x0000, 0x10000, 0x000d624b )

	ROM_REGION( 0x180000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* chars */
	ROM_LOAD( "logicpro.r5", 0x000000, 0x080000, 0xdedf18c9 )
	ROM_LOAD( "logicpro.r6", 0x080000, 0x080000, 0x3ecbd1c2 )
	ROM_LOAD( "logicpro.r7", 0x100000, 0x080000, 0x47135521 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* sprites, used at run time */
	ROM_LOAD_GFX_EVEN( "logicpro.r9", 0x000000, 0x080000, 0xa98bc1d2 )
	ROM_LOAD_GFX_ODD ( "logicpro.r8", 0x000000, 0x080000, 0x1de46298 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* OKIM6295 samples */
	ROM_LOAD( "logicpro.r1", 0x0000, 0x080000, 0xa1fec4d4 )
ROM_END

ROM_START( karianx )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "even",        0x00000, 0x80000, 0xfd0ce238 )
	ROM_LOAD_ODD ( "odd",         0x00000, 0x80000, 0xbe173cdc )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "snd",         0x0000, 0x10000, 0xfedd3375 )

	ROM_REGION( 0x180000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* chars */
	ROM_LOAD( "bkg1",        0x000000, 0x080000, 0x5cb8558a )
	ROM_LOAD( "bkg2",        0x080000, 0x080000, 0x95ff297c )
	ROM_LOAD( "bkg3",        0x100000, 0x080000, 0x6c81f1b2 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* sprites, used at run time */
	ROM_LOAD_GFX_EVEN( "obj4",        0x000000, 0x080000, 0x5f8d75a9 )
	ROM_LOAD_GFX_ODD ( "obj1",        0x000000, 0x080000, 0x967ee97d )
	ROM_LOAD_GFX_EVEN( "obj5",        0x100000, 0x080000, 0xe9fc22f9 )
	ROM_LOAD_GFX_ODD ( "obj2",        0x100000, 0x080000, 0xd39eb04e )
	ROM_LOAD_GFX_EVEN( "obj6",        0x200000, 0x080000, 0xc1ec35a5 )
	ROM_LOAD_GFX_ODD ( "obj3",        0x200000, 0x080000, 0x6ac1ac87 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* OKIM6295 samples */
	ROM_LOAD( "voi",         0x0000, 0x080000, 0xc6506a80 )
ROM_END

ROM_START( logicpr2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "lp2-2",       0x00000, 0x80000, 0xcc1880bf )
	ROM_LOAD_ODD ( "lp2-1",       0x00000, 0x80000, 0x46d5e954 )

	ROM_REGION( 0x180000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* chars */
	ROM_LOAD( "log2-b01",    0x000000, 0x080000, 0xfe789e07 )
	ROM_LOAD( "log2-b02",    0x080000, 0x080000, 0x1e0c51cd )
	ROM_LOAD( "log2-b03",    0x100000, 0x080000, 0x916f2928 )

	ROM_REGION( 0x400000, REGION_GFX2 )	/* sprites, used at run time */
	ROM_LOAD_GFX_SWAP( "obj",         0x000000, 0x400000, 0xf221f305 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* OKIM6295 samples */
	ROM_LOAD( "log2-s01",    0x0000, 0x100000, 0x2875c435 )
ROM_END



GAME( 1996, logicpro, 0, deniam16b, logicpr2, logicpro, ROT0,       "Deniam", "Logic Pro" )
GAME( 1996, karianx,  0, deniam16b, karianx,  karianx,  ROT0,       "Deniam", "Karian Cross" )
GAMEX(1997, logicpr2, 0, deniam16c, logicpr2, logicpro, ROT0_16BIT, "Deniam", "Logic Pro 2 (Japan)", GAME_IMPERFECT_SOUND )
