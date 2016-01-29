#include "../vidhrdw/skykid.cpp"

/***************************************************************************

Dragon Buster (c) Namco 1984
Sky Kid	(c) Namco 1985

Driver by Manuel Abadia <manu@teleline.es>

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6800/m6800.h"
#include "cpu/m6809/m6809.h"

static unsigned char *sharedram;
extern unsigned char *skykid_textram, *spriteram, *skykid_videoram;

/* from vidhrdw/skykid.c */
int skykid_vh_start( void );
READ_HANDLER( skykid_videoram_r );
WRITE_HANDLER( skykid_videoram_w );
WRITE_HANDLER( skykid_scroll_x_w );
WRITE_HANDLER( skykid_scroll_y_w );
WRITE_HANDLER( skykid_flipscreen_w );
void skykid_vh_screenrefresh( struct osd_bitmap *bitmap,int full_refresh );
void drgnbstr_vh_screenrefresh( struct osd_bitmap *bitmap,int full_refresh );
void skykid_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);


static int irq_disabled = 1;
static int inputport_selected;

static int skykid_interrupt( void )
{
	if (!irq_disabled)
		return M6809_INT_IRQ;
	else
		return ignore_interrupt();
}

static WRITE_HANDLER( skykid_irq_ctrl_w )
{
	irq_disabled = offset;
}

static WRITE_HANDLER( inputport_select_w )
{
	if ((data & 0xe0) == 0x60)
		inputport_selected = data & 0x07;
	else if ((data & 0xe0) == 0xc0)
	{
		coin_lockout_global_w(0,~data & 1);
		coin_counter_w(0,data & 2);
		coin_counter_w(1,data & 4);
	}
}

#define reverse_bitstrm(data) ((data & 0x01) << 4) | ((data & 0x02) << 2) | (data & 0x04) \
							| ((data & 0x08) >> 2) | ((data & 0x10) >> 4)

static READ_HANDLER( inputport_r )
{
	int data = 0;

	switch (inputport_selected){
		case 0x00:	/* DSW B (bits 0-4) */
			data = ~(reverse_bitstrm(readinputport(1) & 0x1f)); break;
		case 0x01:	/* DSW B (bits 5-7), DSW A (bits 0-1) */
			data = ~(reverse_bitstrm((((readinputport(1) & 0xe0) >> 5) | ((readinputport(0) & 0x03) << 3)))); break;
		case 0x02:	/* DSW A (bits 2-6) */
			data = ~(reverse_bitstrm(((readinputport(0) & 0x7c) >> 2))); break;
		case 0x03:	/* DSW A (bit 7), DSW C (bits 0-3) */
			data = ~(reverse_bitstrm((((readinputport(0) & 0x80) >> 7) | ((readinputport(2) & 0x0f) << 1)))); break;
		case 0x04:	/* coins, start */
			data = ~(readinputport(3)); break;
		case 0x05:	/* 2P controls */
			data = ~(readinputport(5)); break;
		case 0x06:	/* 1P controls */
			data = ~(readinputport(4)); break;
		default:
			data = 0xff;
	}

	return data;
}

static WRITE_HANDLER( skykid_lamps_w )
{
	osd_led_w(0, (data & 0x08) >> 3);
	osd_led_w(1, (data & 0x10) >> 4);
}

static WRITE_HANDLER( skykid_halt_mcu_w )
{
	if (offset == 0){
		cpu_set_reset_line(1,PULSE_LINE);
		cpu_set_halt_line( 1, CLEAR_LINE );
	}
	else{
		cpu_set_halt_line( 1, ASSERT_LINE );
	}
}

READ_HANDLER( skykid_sharedram_r )
{
	return sharedram[offset];
}
WRITE_HANDLER( skykid_sharedram_w )
{
	sharedram[offset] = data;
}

WRITE_HANDLER( skykid_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	bankaddress = 0x10000 + (offset ? 0 : 0x2000);
	cpu_setbank(1,&RAM[bankaddress]);
}


static struct MemoryReadAddress skykid_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_BANK1 },				/* banked ROM */
	{ 0x2000, 0x2fff, skykid_videoram_r },		/* Video RAM (background) */
	{ 0x4000, 0x47ff, MRA_RAM },				/* video RAM (text layer) */
	{ 0x4800, 0x5fff, MRA_RAM },				/* RAM + Sprite RAM */
	{ 0x6800, 0x68ff, namcos1_wavedata_r },		/* PSG device, shared RAM */
	{ 0x6800, 0x6bff, skykid_sharedram_r },		/* shared RAM with the MCU */
	{ 0x7800, 0x7800, watchdog_reset_r },		/* watchdog reset */
	{ 0x8000, 0xffff, MRA_ROM },				/* ROM */
	{ -1 }
};

static struct MemoryWriteAddress skykid_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },				/* banked ROM */
	{ 0x2000, 0x2fff, skykid_videoram_w, &skykid_videoram },/* Video RAM (background) */
	{ 0x4000, 0x47ff, MWA_RAM, &skykid_textram },/* video RAM (text layer) */
	{ 0x4800, 0x5fff, MWA_RAM },				/* RAM + Sprite RAM */
	{ 0x6000, 0x60ff, skykid_scroll_y_w },		/* Y scroll register map */
	{ 0x6200, 0x63ff, skykid_scroll_x_w },		/* X scroll register map */
	{ 0x6800, 0x68ff, namcos1_wavedata_w, &namco_wavedata },/* PSG device, shared RAM */
	{ 0x6800, 0x6bff, skykid_sharedram_w, &sharedram },	/* shared RAM with the MCU */
	{ 0x7000, 0x7800, skykid_irq_ctrl_w },		/* IRQ control */
	{ 0x8000, 0x8800, skykid_halt_mcu_w },		/* MCU control */
	{ 0x9000, 0x9800, skykid_bankswitch_w },	/* Bankswitch control */
	{ 0xa000, 0xa001, skykid_flipscreen_w },	/* flip screen */
	{ 0x8000, 0xffff, MWA_ROM },				/* ROM */
	{ -1 }
};

static struct MemoryReadAddress mcu_readmem[] =
{
	{ 0x0000, 0x001f, hd63701_internal_registers_r },/* internal registers */
	{ 0x0080, 0x00ff, MRA_RAM },					/* built in RAM */
	{ 0x1000, 0x10ff, namcos1_wavedata_r },			/* PSG device, shared RAM */
	{ 0x1100, 0x113f, MRA_RAM },					/* PSG device */
	{ 0x1000, 0x13ff, skykid_sharedram_r },			/* shared RAM with the 6809 */
	{ 0x8000, 0xbfff, MRA_ROM },					/* MCU external ROM */
	{ 0xc000, 0xc800, MRA_RAM },					/* RAM */
	{ 0xf000, 0xffff, MRA_ROM },					/* MCU internal ROM */
	{ -1 }
};

static struct MemoryWriteAddress mcu_writemem[] =
{
	{ 0x0000, 0x001f, hd63701_internal_registers_w },/* internal registers */
	{ 0x0080, 0x00ff, MWA_RAM },					/* built in RAM */
	{ 0x1000, 0x10ff, namcos1_wavedata_w },			/* PSG device, shared RAM */
	{ 0x1100, 0x113f, namcos1_sound_w, &namco_soundregs },/* PSG device */
	{ 0x1000, 0x13ff, skykid_sharedram_w },			/* shared RAM with the 6809 */
	{ 0x2000, 0x2000, MWA_NOP },					/* ??? */
	{ 0x4000, 0x4000, MWA_NOP },					/* ??? */
	{ 0x6000, 0x6000, MWA_NOP },					/* ??? */
	{ 0x8000, 0xbfff, MWA_ROM },					/* MCU external ROM */
	{ 0xc000, 0xc800, MWA_RAM },					/* RAM */
	{ 0xf000, 0xffff, MWA_ROM },					/* MCU internal ROM */
	{ -1 }
};

static struct IOReadPort mcu_readport[] =
{
	{ HD63701_PORT1, HD63701_PORT1, inputport_r },			/* input ports read */
	{ -1 }	/* end of table */
};

static struct IOWritePort mcu_writeport[] =
{
	{ HD63701_PORT1, HD63701_PORT1, inputport_select_w },	/* input port select */
	{ HD63701_PORT2, HD63701_PORT2, skykid_lamps_w },		/* lamps */
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( skykid )
	PORT_START	/* DSW A */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Round Skip" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Freeze screen" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30k, 90k" )
	PORT_DIPSETTING(    0x04, "20k, 80k" )
	PORT_DIPSETTING(    0x08, "30k every 90k" )
	PORT_DIPSETTING(    0x0c, "20k every 80k" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* DSW C */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN 0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN 1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN 2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( drgnbstr )
	PORT_START	/* DSW A */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Round Skip" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Freeze screen" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSW C */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN 0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN 1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN 2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END


static struct GfxLayout text_layout =
{
	8,8,		/* 8*8 characters */
	512,		/* 512 characters */
	2,			/* 2 bits per pixel */
	{ 0, 4 },	/* the bitplanes are packed in the same byte */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8		/* every char takes 16 consecutive bytes */
};

static struct GfxLayout tile_layout =
{
	8,8,		/* 8*8 characters */
	512,		/* 512 characters */
	2,			/* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8		/* every char takes 16 consecutive bytes */
};

static struct GfxLayout sprite_layout1 =
{
	16,16,       	/* 16*16 sprites */
	128,           	/* 128 sprites */
	3,              /* 3 bits per pixel */
	{ 0x4000*8+4, 0, 4 },
	{ 0, 1, 2, 3, 8*8, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8    /* every sprite takes 64 bytes */
};

static struct GfxLayout sprite_layout2 =
{
	16,16,       	/* 16*16 sprites */
	128,           	/* 128 sprites */
	3,              /* 3 bits per pixel */
	{ 0x4000*8, 0x2000*8, 0x2000*8+4 },
	{ 0, 1, 2, 3, 8*8, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8    /* every sprite takes 64 bytes */
};

static struct GfxLayout sprite_layout3 =
{
	16,16,       	/* 16*16 sprites */
	128,           	/* 128 sprites */
	3,              /* 3 bits per pixel */
	{ 0x8000*8, 0x6000*8, 0x6000*8+4 },
	{ 0, 1, 2, 3, 8*8, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8    /* every sprite takes 64 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &text_layout,		0, 64 },
	{ REGION_GFX2, 0, &tile_layout,		64*4, 128 },
	{ REGION_GFX3, 0, &sprite_layout1,	64*4+128*4, 64 },
	{ REGION_GFX3, 0, &sprite_layout2,	64*4+128*4, 64 },
	{ REGION_GFX3, 0, &sprite_layout3,	64*4+128*4, 64 },
	{-1 }
};


static struct namco_interface namco_interface =
{
	49152000/2048, 		/* 24000 Hz */
	8,					/* number of voices */
	100,				/* playback volume */
	-1,					/* memory region */
	0					/* stereo */
};

static struct MachineDriver machine_driver_skykid =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			49152000/32,	/* ??? */
			skykid_readmem,skykid_writemem,0,0,
			skykid_interrupt,1
		},
		{
			CPU_HD63701,	/* or compatible 6808 with extra instructions */
			49152000/32,	/* ??? */
			mcu_readmem,mcu_writemem,mcu_readport,mcu_writeport,
			interrupt,1
		}
	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,
	100,	/* we need heavy synch */
	0,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	256, 64*4+128*4+64*8,
	skykid_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	skykid_vh_start,
	0,
	skykid_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NAMCO,
			&namco_interface
		}
	}
};


ROM_START( skykid )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* 6809 code */
	ROM_LOAD( "sk2-6c.bin",   0x08000, 0x4000, 0xea8a5822 )
	ROM_LOAD( "sk1-6b.bin",   0x0c000, 0x4000, 0x7abe6c6c )
	ROM_LOAD( "sk3-6d.bin",   0x10000, 0x4000, 0x314b8765 )	/* banked ROM */

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* MCU code */
	ROM_LOAD( "sk4-3c.bin",   0x8000, 0x2000, 0xa460d0e0 )	/* subprogram for the MCU */
	ROM_LOAD( "sk1-mcu.bin",  0xf000, 0x1000, 0x6ef08fb3 )	/* MCU internal code */
															/* Using Pacland code (probably similar) */

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk6-6l.bin",   0x00000, 0x2000, 0x58b731b9 )	/* chars */

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk5-7e.bin",   0x00000, 0x2000, 0xc33a498e )

	ROM_REGION( 0x0a000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sk9-10n.bin",  0x00000, 0x4000, 0x44bb7375 )	/* sprites */
	ROM_LOAD( "sk7-10m.bin",  0x04000, 0x4000, 0x3454671d )
	/* empty space to decode the sprites as 3bpp */

	ROM_REGION( 0x0700, REGION_PROMS )
	ROM_LOAD( "sk1-2n.bin",   0x0000, 0x0100, 0x0218e726 )	/* red component */
	ROM_LOAD( "sk2-2p.bin",   0x0100, 0x0100, 0xfc0d5b85 )	/* green component */
	ROM_LOAD( "sk3-2r.bin",   0x0200, 0x0100, 0xd06b620b )	/* blue component */
	ROM_LOAD( "sk-5n.bin",    0x0300, 0x0200, 0xc697ac72 )	/* tiles lookup table */
	ROM_LOAD( "sk-6n.bin",    0x0500, 0x0200, 0x161514a4 )	/* sprites lookup table */
ROM_END


ROM_START( drgnbstr )
	ROM_REGION( 0x14000, REGION_CPU1 ) /* 6809 code */
	ROM_LOAD( "6c.bin",		0x08000, 0x04000, 0x0f11cd17 )
	ROM_LOAD( "6b.bin",		0x0c000, 0x04000, 0x1c7c1821 )
	ROM_LOAD( "6d.bin",		0x10000, 0x04000, 0x6da169ae )	/* banked ROM */

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* MCU code */
	ROM_LOAD( "3c.bin",		0x8000, 0x02000, 0x8a0b1fc1 )	/* subprogram for the MCU */
	ROM_LOAD( "pl1-mcu.bin",0xf000,	0x01000, 0x6ef08fb3 )	/* The MCU internal code is missing */
															/* Using Pacland code (probably similar) */

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "6l.bin",		0x00000, 0x2000, 0xc080b66c )	/* tiles */

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "7e.bin",		0x00000, 0x2000, 0x28129aed )

	ROM_REGION( 0x0a000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "10n.bin",	0x00000, 0x4000, 0x11942c61 )	/* sprites */
	ROM_LOAD( "10m.bin",	0x04000, 0x4000, 0xcc130fe2 )
		/* empty space to decode the sprites as 3bpp */

	ROM_REGION( 0x0700, REGION_PROMS )
	ROM_LOAD( "2n.bin",		0x00000, 0x0100, 0x3f8cce97 )	/* red component */
	ROM_LOAD( "2p.bin",		0x00100, 0x0100, 0xafe32436 )	/* green component */
	ROM_LOAD( "2r.bin",		0x00200, 0x0100, 0xc95ff576 )	/* blue component */
	ROM_LOAD( "db1-4.5n",	0x00300, 0x0200, 0xb2180c21 )	/* tiles lookup table */
	ROM_LOAD( "db1-5.6n",	0x00500, 0x0200, 0x5e2b3f74 )	/* sprites lookup table */
ROM_END



GAME( 1985, skykid,   0, skykid, skykid,   0, ROT0, "Namco", "Sky Kid" )
GAME( 1984, drgnbstr, 0, skykid, drgnbstr, 0, ROT0, "Namco", "Dragon Buster" )
