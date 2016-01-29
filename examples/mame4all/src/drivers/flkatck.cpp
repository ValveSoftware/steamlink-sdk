#include "../vidhrdw/flkatck.cpp"

/***************************************************************************

Flak Attack/MX5000 (Konami GX669)

Driver by:
	Manuel Abadia <manu@teleline.es>

TO DO:
	-What does 0x900X do? (Z80)

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "cpu/m6809/m6809.h"
#include "vidhrdw/generic.h"

/* from vidhrdw/flkatck.c */
int flkatck_vh_start(void);
void flkatck_vh_screenrefresh(struct osd_bitmap *bitmap,int fullrefresh);
WRITE_HANDLER( flkatck_k007121_w );
WRITE_HANDLER( flkatck_k007121_regs_w );

extern unsigned char *k007121_ram;
extern int flkatck_irq_enabled;

/***************************************************************************/

static void flkatck_init_machine( void )
{
	unsigned char *RAM = memory_region(REGION_SOUND1);
	int bank_A, bank_B;

	bank_A = 0x20000 * 0;
	bank_B = 0x20000 * 1;
	K007232_bankswitch(0,RAM + bank_A,RAM + bank_B);
}

static int flkatck_interrupt( void )
{
	if (flkatck_irq_enabled)
		return HD6309_INT_IRQ;
	else
		return ignore_interrupt();
}

static WRITE_HANDLER( flkatck_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int bankaddress = 0;

	/* bits 3-4: coin counters */
	coin_counter_w(0,data & 0x08);
	coin_counter_w(1,data & 0x10);

	/* bits 0-1: bank # */
	bankaddress += 0x10000 + (data & 0x03)*0x2000;
	if ((data & 0x03) != 0x03)	/* for safety */
		cpu_setbank(1,&RAM[bankaddress]);
}

static READ_HANDLER( flkatck_ls138_r )
{
	int data = 0;

	switch ((offset & 0x1c) >> 2){
		case 0x00:	/* inputs + DIPSW #3 + coinsw */
			if (offset & 0x02)
				data = readinputport(2 + (offset & 0x01));
			else
				data = readinputport(4 + (offset & 0x01));
			break;
		case 0x01:	/* DIPSW #1 & DIPSW #2 */
			if (offset & 0x02)
				data = readinputport(1 - (offset & 0x01));
			break;
	}

	return data;
}

static WRITE_HANDLER( flkatck_ls138_w )
{
	switch ((offset & 0x1c) >> 2){
		case 0x04:	/* bankswitch */
			flkatck_bankswitch_w(0, data);
			break;
		case 0x05:	/* sound code number */
			soundlatch_w(0, data);
			break;
		case 0x06:	/* Cause interrupt on audio CPU */
			cpu_cause_interrupt(1,Z80_IRQ_INT);
			break;
		case 0x07:	/* watchdog reset */
			watchdog_reset_w(0, data);
			break;
	}
}

static struct MemoryReadAddress flkatck_readmem[] =
{
	{ 0x0400, 0x041f, flkatck_ls138_r },			/* inputs + DIPS */
	{ 0x0800, 0x0bff, MRA_RAM },		/* palette */
	{ 0x1000, 0x1fff, MRA_RAM },					/* RAM */
	{ 0x2000, 0x3fff, MRA_RAM },		/* Video RAM (007121) */
	{ 0x4000, 0x5fff, MRA_BANK1 },					/* banked ROM */
	{ 0x6000, 0xffff, MRA_ROM },					/* ROM */
	{ -1 }
};

static struct MemoryWriteAddress flkatck_writemem[] =
{
	{ 0x0000, 0x0007, flkatck_k007121_regs_w },		/* 007121 registers */
	{ 0x0400, 0x041f, flkatck_ls138_w },			/* bankswitch + counters + sound command */
	{ 0x0800, 0x0bff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },/* palette */
	{ 0x1000, 0x1fff, MWA_RAM },					/* RAM */
	{ 0x2000, 0x3fff, flkatck_k007121_w, &k007121_ram },			/* Video RAM (007121) */
	{ 0x4000, 0x5fff, MWA_BANK1 },					/* banked ROM */
	{ 0x6000, 0xffff, MWA_ROM },					/* ROM */
	{ -1 }
};

static struct MemoryReadAddress flkatck_readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },				/* ROM */
	{ 0x8000, 0x87ff, MRA_RAM },				/* RAM */
	{ 0x9000, 0x9000, MRA_RAM },				/* ??? */
	{ 0x9001, 0x9001, MRA_RAM },				/* ??? */
	{ 0x9004, 0x9004, MRA_RAM },				/* ??? */
	{ 0xa000, 0xa000, soundlatch_r },			/* soundlatch_r */
	{ 0xb000, 0xb00d, K007232_read_port_0_r },	/* 007232 registers */
	{ 0xc001, 0xc001, YM2151_status_port_0_r },	/* YM2151 */
	{ -1 }
};

static struct MemoryWriteAddress flkatck_writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },					/* ROM */
	{ 0x8000, 0x87ff, MWA_RAM },					/* RAM */
	{ 0x9000, 0x9000, MWA_RAM },					/* ??? */
	{ 0x9001, 0x9001, MWA_RAM },					/* ??? */
	{ 0x9006, 0x9006, MWA_RAM },					/* ??? */
	{ 0xb000, 0xb00d, K007232_write_port_0_w },		/* 007232 registers */
	{ 0xc000, 0xc000, YM2151_register_port_0_w },	/* YM2151 */
	{ 0xc001, 0xc001, YM2151_data_port_0_w },		/* YM2151 */
	{ -1 }
};


INPUT_PORTS_START( flkatck )
	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
	//PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x03, "1" )
	PORT_DIPSETTING(	0x02, "2" )
	PORT_DIPSETTING(	0x01, "3" )
	PORT_DIPSETTING(	0x00, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x18, "30000 70000" )
	PORT_DIPSETTING(	0x10, "40000 80000" )
	PORT_DIPSETTING(	0x08, "30000" )
	PORT_DIPSETTING(	0x00, "40000" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x60, "Easy" )
	PORT_DIPSETTING(	0x40, "Normal" )
	PORT_DIPSETTING(	0x20, "Difficult" )
	PORT_DIPSETTING(	0x00, "Very difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	/* DSW #3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Upright Controls" )
	PORT_DIPSETTING(    0x02, "Single" )
	PORT_DIPSETTING(    0x00, "Dual" )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* COINSW & START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )	/* service */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

static struct GfxLayout gfxlayout =
{
	8,8,
	0x80000/32,
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &gfxlayout, 0, 32 },
	{ -1 } /* end of array */
};

static struct YM2151interface ym2151_interface =
{
	1,
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ 0 },
};

static void volume_callback0(int v)
{
	K007232_set_volume(0,0,(v >> 4) * 0x11,0);
	K007232_set_volume(0,1,0,(v & 0x0f) * 0x11);
}

static struct K007232_interface k007232_interface =
{
	1,			/* number of chips */
	{ REGION_SOUND1 },		/* memory region */
	{ K007232_VOL(50,MIXER_PAN_CENTER,50,MIXER_PAN_CENTER) },	/* volume */
	{ volume_callback0 }	/* external port callback */
};


static struct MachineDriver machine_driver_flkatck =
{
	{
		{
			CPU_HD6309,	/* HD63C09EP */
			3000000,	/* 24/8 MHz*/
			flkatck_readmem,flkatck_writemem,0,0,
			flkatck_interrupt,1
		},
		{
			CPU_Z80,	/* NEC D780C-1 */
			3579545,	/* 3.579545 MHz */
			flkatck_readmem_sound, flkatck_writemem_sound,0,0,
			ignore_interrupt,0	/* IRQs triggered by the 6309 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	10,
	flkatck_init_machine,

	/* video hardware */
	37*8, 32*8, { 0*8, 35*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	512, 512,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	flkatck_vh_start,
	0,
	flkatck_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K007232,
			&k007232_interface
		}
	}
};



ROM_START( mx5000 )
	ROM_REGION( 0x18000, REGION_CPU1 )		/* 6309 code */
	ROM_LOAD( "r01",          0x010000, 0x006000, 0x79b226fc )/* banked ROM */
	ROM_CONTINUE(             0x006000, 0x00a000 )			/* fixed ROM */

	ROM_REGION( 0x10000, REGION_CPU2 )		/* 64k for the SOUND CPU */
	ROM_LOAD( "m02.bin",		0x000000, 0x008000, 0x7e11e6b9 )

    ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mask4m.bin",		0x000000, 0x080000, 0xff1d718b )/* tiles + sprites */

	ROM_REGION( 0x040000, REGION_SOUND1 )	/* 007232 data (chip 1) */
	ROM_LOAD( "mask2m.bin",		0x000000, 0x040000, 0x6d1ea61c )
ROM_END

ROM_START( flkatck )
	ROM_REGION( 0x18000, REGION_CPU1 )		/* 6309 code */
	ROM_LOAD( "gx669_p1.16c", 0x010000, 0x006000, 0xc5cd2807 )/* banked ROM */
	ROM_CONTINUE(             0x006000, 0x00a000 )			/* fixed ROM */

	ROM_REGION( 0x10000, REGION_CPU2 )		/* 64k for the SOUND CPU */
	ROM_LOAD( "m02.bin",		0x000000, 0x008000, 0x7e11e6b9 )

    ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mask4m.bin",		0x000000, 0x080000, 0xff1d718b )/* tiles + sprites */

	ROM_REGION( 0x040000, REGION_SOUND1 )	/* 007232 data (chip 1) */
	ROM_LOAD( "mask2m.bin",		0x000000, 0x040000, 0x6d1ea61c )
ROM_END



GAME( 1987, mx5000,  0,      flkatck, flkatck, 0, ROT90, "Konami", "MX5000" )
GAME( 1987, flkatck, mx5000, flkatck, flkatck, 0, ROT90, "Konami", "Flak Attack (Japan)" )
