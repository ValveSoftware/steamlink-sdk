#include "../vidhrdw/blockout.cpp"

/***************************************************************************

Block Out

driver by Nicola Salmoria

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"



extern unsigned char *blockout_videoram;
extern unsigned char *blockout_frontvideoram;
extern unsigned char *blockout_frontcolor;

static unsigned char *ram_blockout; /* used by high scores */

WRITE_HANDLER( blockout_videoram_w );
READ_HANDLER( blockout_videoram_r );
WRITE_HANDLER( blockout_frontvideoram_w );
READ_HANDLER( blockout_frontvideoram_r );
WRITE_HANDLER( blockout_paletteram_w );
WRITE_HANDLER( blockout_frontcolor_w );
int blockout_vh_start(void);
void blockout_vh_stop(void);
void blockout_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


int blockout_interrupt(void)
{
	/* interrupt 6 is vblank */
	/* interrupt 5 reads coin inputs - might have to be triggered only */
	/* when a coin is inserted */
	return 6 - cpu_getiloops();
}

READ_HANDLER( blockout_input_r )
{
	switch (offset)
	{
		case 0:
			return input_port_0_r(offset);
		case 2:
			return input_port_1_r(offset);
		case 4:
			return input_port_2_r(offset);
		case 6:
			return input_port_3_r(offset);
		case 8:
			return input_port_4_r(offset);
		default:
//logerror("PC %06x - read input port %06x\n",cpu_get_pc(),0x100000+offset);
			return 0;
	}
}

WRITE_HANDLER( blockout_sound_command_w )
{
	switch (offset)
	{
		case 0:
			soundlatch_w(offset,data & 0xff);
			cpu_cause_interrupt(1,Z80_NMI_INT);
			break;
		case 2:
			/* don't know, maybe reset sound CPU */
			break;
	}
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x10000b, blockout_input_r },
	{ 0x180000, 0x1bffff, blockout_videoram_r },
	{ 0x1d4000, 0x1dffff, MRA_BANK1 },	/* work RAM */
	{ 0x1f4000, 0x1fffff, MRA_BANK2 },	/* work RAM */
	{ 0x200000, 0x207fff, blockout_frontvideoram_r },
	{ 0x208000, 0x21ffff, MRA_BANK3 },	/* ??? */
	{ 0x280200, 0x2805ff, paletteram_word_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100014, 0x100017, blockout_sound_command_w },
	{ 0x180000, 0x1bffff, blockout_videoram_w, &blockout_videoram },
	{ 0x1d4000, 0x1dffff, MWA_BANK1, &ram_blockout },	/* work RAM */
	{ 0x1f4000, 0x1fffff, MWA_BANK2 },	/* work RAM */
	{ 0x200000, 0x207fff, blockout_frontvideoram_w, &blockout_frontvideoram },
	{ 0x208000, 0x21ffff, MWA_BANK3 },	/* ??? */
	{ 0x280002, 0x280003, blockout_frontcolor_w },
	{ 0x280200, 0x2805ff, blockout_paletteram_w, &paletteram },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8801, 0x8801, YM2151_status_port_0_r },
	{ 0x9800, 0x9800, OKIM6295_status_0_r },
	{ 0xa000, 0xa000, soundlatch_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8800, YM2151_register_port_0_w },
	{ 0x8801, 0x8801, YM2151_data_port_0_w },
	{ 0x9800, 0x9800, OKIM6295_data_0_w },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( blockout )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* the following two are supposed to control Coin 2, but they don't work. */
	/* This happens on the original board too. */
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )	/* unused? */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )	/* unused? */
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "1 Coin to Continue" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )	/* unused? */
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )	/* unused? */
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x04, 0x04, "Rotate Buttons" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
INPUT_PORTS_END




/* handler called by the 2151 emulator when the internal timers cause an IRQ */
static void blockout_irq_handler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
	/* cpu_cause_interrupt(1,0xff); */
}

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 Mhz (?) */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ blockout_irq_handler }
};

static struct OKIM6295interface okim6295_interface =
{
	1,                  /* 1 chip */
	{ 8000 },           /* 8000Hz frequency */
	{ REGION_SOUND1 },	/* memory region */
	{ 50 }
};



static struct MachineDriver machine_driver_blockout =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,  
			readmem,writemem,0,0,
			blockout_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1	/* NMIs are triggered by the main CPU, IRQs by the YM2151 */
		}
	},
	58, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	320, 256, { 0, 319, 8, 247 },
	0,
	513, 0,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	blockout_vh_start,
	blockout_vh_stop,
	blockout_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
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

ROM_START( blockout )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 2*128k for 68000 code */
	ROM_LOAD_EVEN( "bo29a0-2.bin", 0x00000, 0x20000, 0xb0103427 )
	ROM_LOAD_ODD ( "bo29a1-2.bin", 0x00000, 0x20000, 0x5984d5a2 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "bo29e3-0.bin", 0x0000, 0x8000, 0x3ea01f78 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for ADPCM samples - sound chip is OKIM6295 */
	ROM_LOAD( "bo29e2-0.bin", 0x0000, 0x20000, 0x15c5a99d )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "mb7114h.25",   0x0000, 0x0100, 0xb25bbda7 )	/* unknown */
ROM_END

ROM_START( blckout2 )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 2*128k for 68000 code */
	ROM_LOAD_EVEN( "29a0",         0x00000, 0x20000, 0x605f931e )
	ROM_LOAD_ODD ( "29a1",         0x00000, 0x20000, 0x38f07000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "bo29e3-0.bin", 0x0000, 0x8000, 0x3ea01f78 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* 128k for ADPCM samples - sound chip is OKIM6295 */
	ROM_LOAD( "bo29e2-0.bin", 0x0000, 0x20000, 0x15c5a99d )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "mb7114h.25",   0x0000, 0x0100, 0xb25bbda7 )	/* unknown */
ROM_END



GAME( 1989, blockout, 0,        blockout, blockout, 0, ROT0, "Technos + California Dreams", "Block Out (set 1)" )
GAME( 1989, blckout2, blockout, blockout, blockout, 0, ROT0, "Technos + California Dreams", "Block Out (set 2)" )
