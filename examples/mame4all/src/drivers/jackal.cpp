#include "../machine/jackal.cpp"
#include "../vidhrdw/jackal.cpp"

/***************************************************************************

  jackal.c

  Written by Kenneth Lin (kenneth_lin@ai.vancouver.bc.ca)

Notes:
- The high score table colors are wrong, are there proms missing?
- The sprite drawing code could use a rewrite
- Sprite lag
- After game over, the first main logo screen is one line out of sync after
  the scrolling is completed.
- One of the bootleg Top Gunner gfx ROMs seems to be bad.


1st CPU memory map
NB. "x", "y" refers to non rotated screen.
ie.

  x --> increase
y  -------------------------------------------------------------
| | 0                                                           |
| | 0                                                           |
V |                                                             |
  |                                                             |
i |                                                             |
n |I                                                            |
c |H                                                            |
r |                                                             |
e |                                                             |
a | 0                                                           |
s | 0                                                           |
e |                                                             |
  |                                                             |
   -------------------------------------------------------------

0000-001f  Work area (IO interface?)

	0000:	x whole screen scroll
	0001:	y whole screen scroll
	0002 ?	store
	0003 ?	store
	0004 ?  Bit 2 controls NMI, bit 4 controls IRQ, bit 5 controls FIRQ
	0010:	DSW1
	0011:	Player 1 controls
	0012:	Player 2 controls
	0013:	Coin + selftest
	0018:	DSW2
	0019 ?	Write value frequently (watchdog reset ?)
	001C:	Memory bank selection 0x08=ALT SPRITE/0x10=ALT RAM/0x20=ALT ROM

0020-005f  Banked ZRAM (Scroll registers?)

	0020 - 002F: 16 horizontal / vertical strips based on memory $0002

0060-1fff  Banked COMMON RAM

	Using sprite buffer 1 (Base $3000) as example.
	Add 0x800 for sprite buffer 2 (Base $3800).

	($001C = 0x??, memory block size = 0x08, 65 entries)
	0060-0067: 0060, 0061, 0062, 0063, 0064 -> 0311D, 0311E, 0311F, 03120, 03121
	0068-006F: 0068, 0069, 006A, 006B, 006C -> 03122, 03123, 03124, 03125, 03126
	.
	.
	0198-019F: 0198, 0199, 019A, 019B, 019C -> 031E0, 031E1, 031E2, 031E3, 031E4
	01A0-01A7: 01A0, 01A1, 01A2, 01A3, 01A4 -> 031E5, 031E6, 031E7, 031E8, 031E9

	($001C = 0x28, memory block size = 0x28, 49 entries)
	01A8-01CF: 01AE, 01AF, 01AA, 01AC, 01A9 -> 13000, 13001, 13002, 13003, 13004
	01D0-01F7: 01D6, 01D7, 01D2, 01D4, 01D1 -> 13005, 13006, 13007, 13008, 13009
	01F8-021F: 01FE, 01FF, 01FA, 01FC, 01F9 -> 1300A, 1300B, 1300C, 1300D, 1300E
	.
	.
	0900-0927: 0906, 0907, 0902, 0904, 0901 -> 130EB, 130EC, 130ED, 130EE, 130EF
	0928-094F: 092E, 092F, 092A, 092C, 0929 -> 130F0, 130F1, 130F2, 130F3, 130F4

	($001C = 0x??, memory block size = 0x28, 57 entries)
	0950-0977: 0956, 0957, 0952, 0953, 0954 -> 03000, 03001, 03002, 03003, 03004
	0978-099F: 097E, 097F, 097A, 097C, 0979 -> 03005, 03006, 03007, 03008, 03009
	.
	.
	11E8-120F: 11EE, 11EF, 11EA, 11EC, 11E9 -> 03113, 03114, 03115, 03116, 03117
	1210-1237: 1216, 1217, 1212, 1214, 1211 -> 03118, 03119, 0311A, 0311B, 0311C

	12F8-132F: High score table

	1340-1343: High score

	1C11:	SOUND RAM status from 2nd CPU during self test
	1C12:	COLOR RAM status from 2nd CPU during self test
	1C13:	ROM1 status from 2nd CPU during self test

2000-23ff  ColorRAM (Extended VideoRAM)
2400-27ff  VideoRAM
2800-2bff  RAM
2C00-2fff  ?
3000-34ff  Sprite buffer #1
3500-37ff  (Unused)
3800-3cff  Sprite buffer #2
3D00-3fff  (Unused)
4000-bfff  Banked ROM
C000-ffff  ROM

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"


extern unsigned char *jackal_videoctrl;

void jackal_init_machine(void);
int jackal_vh_start(void);
void jackal_vh_stop(void);

READ_HANDLER( jackal_zram_r );
READ_HANDLER( jackal_commonram_r );
READ_HANDLER( jackal_commonram1_r );
READ_HANDLER( jackal_voram_r );
READ_HANDLER( jackal_spriteram_r );

WRITE_HANDLER( jackal_rambank_w );
WRITE_HANDLER( jackal_zram_w );
WRITE_HANDLER( jackal_commonram_w );
WRITE_HANDLER( jackal_commonram1_w );
WRITE_HANDLER( jackal_voram_w );
WRITE_HANDLER( jackal_spriteram_w );

void jackal_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void jackal_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static READ_HANDLER( rotary_0_r )
{
	return (1 << (readinputport(6) * 8 / 256)) ^ 0xff;
}

static READ_HANDLER( rotary_1_r )
{
	return (1 << (readinputport(7) * 8 / 256)) ^ 0xff;
}

static unsigned char intenable;

WRITE_HANDLER( jackal_interrupt_enable_w )
{
	intenable = data;
}

int jackal_interrupt(void)
{
	if (intenable & 0x02) return nmi_interrupt();
	if (intenable & 0x08) return M6809_INT_IRQ;
	if (intenable & 0x10) return M6809_INT_FIRQ;
	return ignore_interrupt();
}


static struct MemoryReadAddress jackal_readmem[] =
{
	{ 0x0010, 0x0010, input_port_0_r },
	{ 0x0011, 0x0011, input_port_1_r },
	{ 0x0012, 0x0012, input_port_2_r },
	{ 0x0013, 0x0013, input_port_3_r },
	{ 0x0014, 0x0014, rotary_0_r },
	{ 0x0015, 0x0015, rotary_1_r },
	{ 0x0018, 0x0018, input_port_4_r },
	{ 0x0020, 0x005f, jackal_zram_r },	/* MAIN   Z RAM,SUB    Z RAM */
	{ 0x0060, 0x1fff, jackal_commonram_r },	/* M COMMON RAM,S COMMON RAM */
	{ 0x2000, 0x2fff, jackal_voram_r },	/* MAIN V O RAM,SUB  V O RAM */
	{ 0x3000, 0x3fff, jackal_spriteram_r },	/* MAIN V O RAM,SUB  V O RAM */
	{ 0x4000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress jackal_writemem[] =
{
	{ 0x0000, 0x0003, MWA_RAM, &jackal_videoctrl },	/* scroll + other things */
	{ 0x0004, 0x0004, jackal_interrupt_enable_w },
	{ 0x0019, 0x0019, MWA_NOP },	/* possibly watchdog reset */
	{ 0x001c, 0x001c, jackal_rambank_w },
	{ 0x0020, 0x005f, jackal_zram_w },
	{ 0x0060, 0x1fff, jackal_commonram_w },
	{ 0x2000, 0x2fff, jackal_voram_w },
	{ 0x3000, 0x3fff, jackal_spriteram_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress jackal_sound_readmem[] =
{
	{ 0x2001, 0x2001, YM2151_status_port_0_r },
	{ 0x4000, 0x43ff, MRA_RAM },		/* COLOR RAM (Self test only check 0x4000-0x423f */
	{ 0x6000, 0x605f, MRA_RAM },		/* SOUND RAM (Self test check 0x6000-605f, 0x7c00-0x7fff */
	{ 0x6060, 0x7fff, jackal_commonram1_r }, /* COMMON RAM */
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress jackal_sound_writemem[] =
{
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x4000, 0x43ff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },
	{ 0x6000, 0x605f, MWA_RAM },
	{ 0x6060, 0x7fff, jackal_commonram1_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( jackal )
	PORT_START	/* DSW1 */
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
	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* IN1 */
	/* note that button 3 for player 1 and 2 are exchanged */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "30000 150000" )
	PORT_DIPSETTING(    0x10, "50000 200000" )
	PORT_DIPSETTING(    0x08, "30000" )
	PORT_DIPSETTING(    0x00, "50000" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Mode" )
	PORT_DIPSETTING(    0x04, "Mono" )
	PORT_DIPSETTING(    0x00, "Stereo" )
	PORT_DIPNAME( 0x08, 0x00, "Sound Adj" )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )

	/* the rotary controls work in topgunbl only */
	PORT_START	/* player 1 8-way rotary control - converted in rotary_0_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL, 25, 10, 0, 0, JOYCODE_1_BUTTON5, JOYCODE_1_BUTTON6, 0, 0 )

	PORT_START	/* player 2 8-way rotary control - converted in rotary_1_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_PLAYER2, 25, 10, 0, 0, JOYCODE_2_BUTTON5, JOYCODE_2_BUTTON6, 0, 0 )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	8,	/* 8 bits per pixel (!) */
	{ 0, 1, 2, 3, 0x20000*8+0, 0x20000*8+1, 0x20000*8+2, 0x20000*8+3 },
	{ 0*4, 1*4, 0x40000*8+0*4, 0x40000*8+1*4, 2*4, 3*4, 0x40000*8+2*4, 0x40000*8+3*4 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	1024,	/* 1024 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },	/* the four bitplanes are packed in one nibble */
	{ 0*4, 1*4, 0x40000*8+0*4, 0x40000*8+1*4, 2*4, 3*4, 0x40000*8+2*4, 0x40000*8+3*4,
			16*8+0*4, 16*8+1*4, 16*8+0x40000*8+0*4, 16*8+0x40000*8+1*4, 16*8+2*4, 16*8+3*4, 16*8+0x40000*8+2*4, 16*8+0x40000*8+3*4 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout spritelayout8 =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },	/* the four bitplanes are packed in one nibble */
	{ 0*4, 1*4, 0x40000*8+0*4, 0x40000*8+1*4, 2*4, 3*4, 0x40000*8+2*4, 0x40000*8+3*4 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout topgunbl_charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	8,	/* 8 bits per pixel (!) */
	{ 0, 1, 2, 3, 0x20000*8+0, 0x20000*8+1, 0x20000*8+2, 0x20000*8+3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout topgunbl_spritelayout =
{
	16,16,	/* 16*16 sprites */
	1024,	/* 1024 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4,
			32*8+2*4, 32*8+3*4, 32*8+0*4, 32*8+1*4, 32*8+6*4, 32*8+7*4, 32*8+4*4, 32*8+5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
	128*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout topgunbl_spritelayout8 =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxDecodeInfo jackal_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &charlayout,          256,  1 },	/* colors 256-511 */
	{ REGION_GFX1, 0x10000, &spritelayout,        512, 16 },	/* colors   0- 15 with lookup */
	{ REGION_GFX1, 0x30000, &spritelayout,  512+16*16, 16 },	/* colors  16- 31 with lookup */
	{ REGION_GFX1, 0x10000, &spritelayout8,       512, 16 },  /* to handle 8x8 sprites */
	{ REGION_GFX1, 0x30000, &spritelayout8, 512+16*16, 16 },  /* to handle 8x8 sprites */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo topgunbl_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &topgunbl_charlayout,          256,  1 },	/* colors 256-511 */
	{ REGION_GFX1, 0x40000, &topgunbl_spritelayout,        512, 16 },	/* colors   0- 15 with lookup */
	{ REGION_GFX1, 0x60000, &topgunbl_spritelayout,  512+16*16, 16 },	/* colors  16- 31 with lookup */
	{ REGION_GFX1, 0x40000, &topgunbl_spritelayout8,       512, 16 },	/* to handle 8x8 sprites */
	{ REGION_GFX1, 0x60000, &topgunbl_spritelayout8, 512+16*16, 16 },	/* to handle 8x8 sprites */
	{ -1 } /* end of array */
};



static struct YM2151interface ym2151_interface =
{
	1,
	3580000,
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) },
	{ 0 },
};


static struct MachineDriver machine_driver_jackal =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			2000000,	/* 2 MHz???? */
			jackal_readmem,jackal_writemem,0,0,
			jackal_interrupt,1
		},
		{
			CPU_M6809,
			2000000,	/* 2 MHz???? */
			jackal_sound_readmem,jackal_sound_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,	/* 10 CPU slices per frame - seems enough to keep the CPUs in sync */
	jackal_init_machine,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 2*8, 30*8-1 },
	jackal_gfxdecodeinfo,
	512, 512+16*16+16*16,
	jackal_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	jackal_vh_start,
	jackal_vh_stop,
	jackal_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
};

/* identical but different gfxdecode */
static struct MachineDriver machine_driver_topgunbl =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			2000000,	/* 2 MHz???? */
			jackal_readmem,jackal_writemem,0,0,
			jackal_interrupt,1
		},
		{
			CPU_M6809,
			2000000,	/* 2 MHz???? */
			jackal_sound_readmem,jackal_sound_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,	/* 10 CPU slices per frame - seems enough to keep the CPUs in sync */
	jackal_init_machine,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 2*8, 30*8-1 },
	topgunbl_gfxdecodeinfo,
	512, 512,
	jackal_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	jackal_vh_start,
	jackal_vh_stop,
	jackal_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
};




/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( jackal )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* Banked 64k for 1st CPU */
	ROM_LOAD( "j-v02.rom",    0x04000, 0x8000, 0x0b7e0584 )
	ROM_CONTINUE(             0x14000, 0x8000 )
	ROM_LOAD( "j-v03.rom",    0x0c000, 0x4000, 0x3e0dfb83 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 2nd cpu (Graphics & Sound)*/
	ROM_LOAD( "631t01.bin",   0x8000, 0x8000, 0xb189af6a )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "631t04.bin",   0x00000, 0x20000, 0x457f42f0 )
	ROM_LOAD( "631t06.bin",   0x20000, 0x20000, 0x2d10e56e )
	ROM_LOAD( "631t05.bin",   0x40000, 0x20000, 0x732b3fc1 )
	ROM_LOAD( "631t07.bin",   0x60000, 0x20000, 0x4961c397 )

	ROM_REGION( 0x0200, REGION_PROMS )	/* color lookup tables */
	ROM_LOAD( "631r08.bpr",   0x0000, 0x0100, 0x7553a172 )
	ROM_LOAD( "631r09.bpr",   0x0100, 0x0100, 0xa74dd86c )
ROM_END

ROM_START( topgunr )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* Banked 64k for 1st CPU */
	ROM_LOAD( "tgnr15d.bin",  0x04000, 0x8000, 0xf7e28426 )
	ROM_CONTINUE(             0x14000, 0x8000 )
	ROM_LOAD( "tgnr16d.bin",  0x0c000, 0x4000, 0xc086844e )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 2nd cpu (Graphics & Sound)*/
	ROM_LOAD( "631t01.bin",   0x8000, 0x8000, 0xb189af6a )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tgnr7h.bin",   0x00000, 0x20000, 0x50122a12 )
	ROM_LOAD( "tgnr12h.bin",  0x20000, 0x20000, 0x37dbbdb0 )
	ROM_LOAD( "tgnr8h.bin",   0x40000, 0x20000, 0x6943b1a4 )
	ROM_LOAD( "tgnr13h.bin",  0x60000, 0x20000, 0x22effcc8 )

	ROM_REGION( 0x0200, REGION_PROMS )	/* color lookup tables */
	ROM_LOAD( "631r08.bpr",   0x0000, 0x0100, 0x7553a172 )
	ROM_LOAD( "631r09.bpr",   0x0100, 0x0100, 0xa74dd86c )
ROM_END

ROM_START( jackalj )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* Banked 64k for 1st CPU */
	ROM_LOAD( "631t02.bin",   0x04000, 0x8000, 0x14db6b1a )
	ROM_CONTINUE(             0x14000, 0x8000 )
	ROM_LOAD( "631t03.bin",   0x0c000, 0x4000, 0xfd5f9624 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 2nd cpu (Graphics & Sound)*/
	ROM_LOAD( "631t01.bin",   0x8000, 0x8000, 0xb189af6a )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "631t04.bin",   0x00000, 0x20000, 0x457f42f0 )
	ROM_LOAD( "631t06.bin",   0x20000, 0x20000, 0x2d10e56e )
	ROM_LOAD( "631t05.bin",   0x40000, 0x20000, 0x732b3fc1 )
	ROM_LOAD( "631t07.bin",   0x60000, 0x20000, 0x4961c397 )

	ROM_REGION( 0x0200, REGION_PROMS )	/* color lookup tables */
	ROM_LOAD( "631r08.bpr",   0x0000, 0x0100, 0x7553a172 )
	ROM_LOAD( "631r09.bpr",   0x0100, 0x0100, 0xa74dd86c )
ROM_END

ROM_START( topgunbl )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* Banked 64k for 1st CPU */
	ROM_LOAD( "t-3.c5",       0x04000, 0x8000, 0x7826ad38 )
	ROM_LOAD( "t-4.c4",       0x14000, 0x8000, 0x976c8431 )
	ROM_LOAD( "t-2.c6",       0x0c000, 0x4000, 0xd53172e5 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 2nd cpu (Graphics & Sound)*/
	ROM_LOAD( "t-1.c14",      0x8000, 0x8000, 0x54aa2d29 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "t-17.n12",     0x00000, 0x08000, 0xe8875110 )
	ROM_LOAD( "t-18.n13",     0x08000, 0x08000, 0xcf14471d )
	ROM_LOAD( "t-19.n14",     0x10000, 0x08000, 0x46ee5dd2 )
	ROM_LOAD( "t-20.n15",     0x18000, 0x08000, 0x3f472344 )
	ROM_LOAD( "t-13.n8",      0x20000, 0x08000, 0x5d669abb )
	ROM_LOAD( "t-14.n9",      0x28000, 0x08000, 0xf349369b )
	ROM_LOAD( "t-15.n10",     0x30000, 0x08000, 0x7c5a91dd )
	ROM_LOAD( "t-16.n11",     0x38000, 0x08000, 0x5ec46d8e )
	ROM_LOAD( "t-6.n1",       0x40000, 0x08000, 0x539cc48c )
	ROM_LOAD( "t-5.m1",       0x48000, 0x08000, 0x2dd9a5e9 )
	ROM_LOAD( "t-7.n2",       0x50000, 0x08000, 0x0ecd31b1 )
	ROM_LOAD( "t-8.n3",       0x58000, 0x08000, 0xf946ada7 )
	ROM_LOAD( "t-9.n4",       0x60000, 0x08000, 0x8269caca )
	ROM_LOAD( "t-10.n5",      0x68000, 0x08000, 0x25393e4f )
	ROM_LOAD( "t-11.n6",      0x70000, 0x08000, 0x7895c22d )
	ROM_LOAD( "t-12.n7",      0x78000, 0x08000, 0x15606dfc )

	ROM_REGION( 0x0200, REGION_PROMS )	/* color lookup tables */
	ROM_LOAD( "631r08.bpr",   0x0000, 0x0100, 0x7553a172 )
	ROM_LOAD( "631r09.bpr",   0x0100, 0x0100, 0xa74dd86c )
ROM_END



GAMEX( 1986, jackal,   0,      jackal,   jackal, 0, ROT90, "Konami", "Jackal (World)", GAME_IMPERFECT_COLORS | GAME_NO_COCKTAIL )
GAMEX( 1986, topgunr,  jackal, jackal,   jackal, 0, ROT90, "Konami", "Top Gunner (US)", GAME_IMPERFECT_COLORS | GAME_NO_COCKTAIL )
GAMEX( 1986, jackalj,  jackal, jackal,   jackal, 0, ROT90, "Konami", "Tokushu Butai Jackal (Japan)", GAME_IMPERFECT_COLORS | GAME_NO_COCKTAIL )
GAMEX( 1987, topgunbl, jackal, topgunbl, jackal, 0, ROT90, "bootleg", "Top Gunner (bootleg)", GAME_IMPERFECT_COLORS | GAME_NO_COCKTAIL )
