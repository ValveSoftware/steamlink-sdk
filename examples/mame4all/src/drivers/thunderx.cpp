#include "../vidhrdw/thunderx.cpp"

/***************************************************************************

Super Contra / Thunder Cross

driver by Bryan McPhail, Manuel Abadia

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/konami/konami.h" /* for the callback and the firq irq definition */
#include "vidhrdw/konamiic.h"

static void scontra_init_machine(void);
static void thunderx_init_machine(void);
static void thunderx_banking(int lines);

extern int scontra_priority;
int scontra_vh_start(void);
void scontra_vh_stop(void);
void scontra_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/***************************************************************************/

static int scontra_interrupt( void )
{
	if (K052109_is_IRQ_enabled())
		return KONAMI_INT_IRQ;
	else
		return ignore_interrupt();
}

static int thunderx_interrupt( void )
{
	if (K052109_is_IRQ_enabled())
	{
		if (cpu_getiloops() == 0) return KONAMI_INT_IRQ;
		else if (cpu_getiloops() & 1) return KONAMI_INT_FIRQ;	/* ??? */
	}
	return ignore_interrupt();
}


static int palette_selected;
static int bank;
static unsigned char *ram,*unknownram;

static READ_HANDLER( scontra_bankedram_r )
{
	if (palette_selected)
		return paletteram_r(offset);
	else
		return ram[offset];
}

static WRITE_HANDLER( scontra_bankedram_w )
{
	if (palette_selected)
		paletteram_xBBBBBGGGGGRRRRR_swap_w(offset,data);
	else
		ram[offset] = data;
}

static READ_HANDLER( thunderx_bankedram_r )
{
	if ((bank & 0x01) == 0)
	{
		if (bank & 0x10)
			return unknownram[offset];
		else
			return paletteram_r(offset);
	}
	else
		return ram[offset];
}

static WRITE_HANDLER( thunderx_bankedram_w )
{
	if ((bank & 0x01) == 0)
	{
		if (bank & 0x10)
			unknownram[offset] = data;
		else
			paletteram_xBBBBBGGGGGRRRRR_swap_w(offset,data);
	}
	else
		ram[offset] = data;
}

static void calculate_collisions( void ) {
	unsigned char *ptr1 = &unknownram[0x10], *ptr2;
	int i, j;

	/* each sprite is defined as: flags height width xpos ypos */
	for( i = 0; i < 127; i++, ptr1 += 5 ) {
		int w,h;
		int	x,y;

		if ( ( ptr1[0] & 0x80 ) == 0x00 )
			continue;

		ptr2 = ptr1 + 5;
		w = 4; /* ? */
		h = 4; /* ? */
		x = ptr1[3];
		y = ptr1[4];

		for( j = i+1; j < 128; j++, ptr2 += 5 ) {
			int x1,y1;

			if ( ( ptr2[0] & 0x80 ) == 0x00 )
				continue;

			x1 = ptr2[3];
			y1 = ptr2[4];

			x1 -= x;

			if ( x1 < 0 )
				x1 = -x1;

			if ( x1 > w )
				continue;

			y1 -= y;

			if ( y1 < 0 )
				y1 = -y1;

			if ( y1 > h )
				continue;

/*
00 - 02 - our ships
02 - 40 - our bullets
42 - 16 - enemy bullets
58 - 60 - enemy ships
118 - ? - ?
*/
			if ( i > 117 )
				continue;

			if ( i < 42 ) { /* our ship & bullets */
				if ( j < 42 ) /* our ship & bullets */
					continue;
			} else { /* enemy ships & bullets */
				if ( j > 41 ) /* enemy ships & bullets */
					continue;
			}

			/* bullets dont collide eachother */
			if ( i > 1 && i < 42 )
				if ( j > 41 && j < 58 )
					continue;

			/* collision */
			if ( ptr1[0] & 0x20 )
				ptr1[0] |= 0x10;

			if ( ptr2[0] & 0x20 )
				ptr2[0] |= 0x10;
		}
	}
}

static WRITE_HANDLER( thunderx_1f98_w )
{
//logerror("%04x: write %02x to 1f98\n",cpu_get_pc(),data);
	/* bit 0 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line((data & 0x01) ? ASSERT_LINE : CLEAR_LINE);

	/* bit 1 unknown - used by Thunder Cross during test of RAM C8 (5800-5fff) */
	if ( data & 2 )
		calculate_collisions();
}

WRITE_HANDLER( scontra_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int offs;

//logerror("%04x: bank switch %02x\n",cpu_get_pc(),data);

	/* bits 0-3 ROM bank */
	offs = 0x10000 + (data & 0x0f)*0x2000;
	cpu_setbank( 1, &RAM[offs] );

	/* bit 4 select work RAM or palette RAM at 5800-5fff */
	palette_selected = ~data & 0x10;

	/* bits 5/6 coin counters */
	coin_counter_w(0,data & 0x20);
	coin_counter_w(1,data & 0x40);

	/* bit 7 controls layer priority */
	scontra_priority = data & 0x80;
}

static WRITE_HANDLER( thunderx_videobank_w )
{
//logerror("%04x: select video ram bank %02x\n",cpu_get_pc(),data);
	/* 0x01 = work RAM at 4000-5fff */
	/* 0x00 = palette at 5800-5fff */
	/* 0x10 = unknown RAM at 5800-5fff */
	bank = data;

	/* bits 1/2 coin counters */
	coin_counter_w(0,data & 0x02);
	coin_counter_w(1,data & 0x04);

	/* bit 3 controls layer priority (seems to be always 1) */
	scontra_priority = data & 0x08;
}

static WRITE_HANDLER( thunderx_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,0xff);
}

static WRITE_HANDLER( scontra_snd_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_SOUND1);
	/* b3-b2: bank for chanel B */
	/* b1-b0: bank for chanel A */

	int bank_A = 0x20000*(data & 0x03);
	int bank_B = 0x20000*((data >> 2) & 0x03);

	K007232_bankswitch(0,RAM + bank_A,RAM + bank_B);
}

/***************************************************************************/

static struct MemoryReadAddress scontra_readmem[] =
{
	{ 0x1f90, 0x1f90, input_port_0_r }, /* coin */
	{ 0x1f91, 0x1f91, input_port_1_r }, /* p1 */
	{ 0x1f92, 0x1f92, input_port_2_r }, /* p2 */
	{ 0x1f93, 0x1f93, input_port_5_r }, /* Dip 3 */
	{ 0x1f94, 0x1f94, input_port_3_r }, /* Dip 1 */
	{ 0x1f95, 0x1f95, input_port_4_r }, /* Dip 2 */

	{ 0x0000, 0x3fff, K052109_051960_r },
	{ 0x4000, 0x57ff, MRA_RAM },
	{ 0x5800, 0x5fff, scontra_bankedram_r },			/* palette + work RAM */
	{ 0x6000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress thunderx_readmem[] =
{
	{ 0x1f90, 0x1f90, input_port_0_r }, /* coin */
	{ 0x1f91, 0x1f91, input_port_1_r }, /* p1 */
	{ 0x1f92, 0x1f92, input_port_2_r }, /* p2 */
	{ 0x1f93, 0x1f93, input_port_5_r }, /* Dip 3 */
	{ 0x1f94, 0x1f94, input_port_3_r }, /* Dip 1 */
	{ 0x1f95, 0x1f95, input_port_4_r }, /* Dip 2 */

	{ 0x0000, 0x3fff, K052109_051960_r },
	{ 0x4000, 0x57ff, MRA_RAM },
	{ 0x5800, 0x5fff, thunderx_bankedram_r },			/* palette + work RAM + unknown RAM */
	{ 0x6000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress scontra_writemem[] =
{
	{ 0x1f80, 0x1f80, scontra_bankswitch_w },	/* bankswitch control + coin counters */
	{ 0x1f84, 0x1f84, soundlatch_w },
	{ 0x1f88, 0x1f88, thunderx_sh_irqtrigger_w },		/* cause interrupt on audio CPU */
	{ 0x1f8c, 0x1f8c, watchdog_reset_w },
	{ 0x1f98, 0x1f98, thunderx_1f98_w },

	{ 0x0000, 0x3fff, K052109_051960_w },		/* video RAM + sprite RAM */
	{ 0x4000, 0x57ff, MWA_RAM },
	{ 0x5800, 0x5fff, scontra_bankedram_w, &ram },			/* palette + work RAM */
	{ 0x6000, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress thunderx_writemem[] =
{
	{ 0x1f80, 0x1f80, thunderx_videobank_w },
	{ 0x1f84, 0x1f84, soundlatch_w },
	{ 0x1f88, 0x1f88, thunderx_sh_irqtrigger_w },		/* cause interrupt on audio CPU */
	{ 0x1f8c, 0x1f8c, watchdog_reset_w },
	{ 0x1f98, 0x1f98, thunderx_1f98_w },

	{ 0x0000, 0x3fff, K052109_051960_w },
	{ 0x4000, 0x57ff, MWA_RAM },
	{ 0x5800, 0x5fff, thunderx_bankedram_w, &ram },			/* palette + work RAM + unknown RAM */
	{ 0x6000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress scontra_readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },				/* ROM */
	{ 0x8000, 0x87ff, MRA_RAM },				/* RAM */
	{ 0xa000, 0xa000, soundlatch_r },			/* soundlatch_r */
	{ 0xb000, 0xb00d, K007232_read_port_0_r },	/* 007232 registers */
	{ 0xc001, 0xc001, YM2151_status_port_0_r },	/* YM2151 */
	{ -1 }
};

static struct MemoryWriteAddress scontra_writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },					/* ROM */
	{ 0x8000, 0x87ff, MWA_RAM },					/* RAM */
	{ 0xb000, 0xb00d, K007232_write_port_0_w },		/* 007232 registers */
	{ 0xc000, 0xc000, YM2151_register_port_0_w },	/* YM2151 */
	{ 0xc001, 0xc001, YM2151_data_port_0_w },		/* YM2151 */
	{ 0xf000, 0xf000, scontra_snd_bankswitch_w },	/* 007232 bank select */
	{ -1 }
};

static struct MemoryReadAddress thunderx_readmem_sound[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, soundlatch_r },
	{ 0xc001, 0xc001, YM2151_status_port_0_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress thunderx_writemem_sound[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xc000, 0xc000, YM2151_register_port_0_w },
	{ 0xc001, 0xc001, YM2151_data_port_0_w },
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( scontra )
	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
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
//	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x03, "2" )
	PORT_DIPSETTING(	0x02, "3" )
	PORT_DIPSETTING(	0x01, "5" )
	PORT_DIPSETTING(	0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )	/* test mode calls it cabinet type, */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )		/* but this is a 2 players game */
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x18, "30000 200000" )
	PORT_DIPSETTING(	0x10, "50000 300000" )
	PORT_DIPSETTING(	0x08, "30000" )
	PORT_DIPSETTING(	0x00, "50000" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x60, "Easy" )
	PORT_DIPSETTING(	0x40, "Normal" )
	PORT_DIPSETTING(	0x20, "Difficult" )
	PORT_DIPSETTING(	0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	/* DSW #3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, "Continue Limit" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( thunderx )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
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

	PORT_START
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
//	PORT_DIPSETTING(    0x00, "Invalid" )

 	PORT_START
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, "Award Bonus Life" )
	PORT_DIPSETTING(    0x04, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "30000 200000" )
	PORT_DIPSETTING(    0x10, "50000 300000" )
	PORT_DIPSETTING(    0x08, "30000" )
	PORT_DIPSETTING(    0x00, "50000" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END



/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ 0 },
};

static void volume_callback(int v)
{
	K007232_set_volume(0,0,(v >> 4) * 0x11,0);
	K007232_set_volume(0,1,0,(v & 0x0f) * 0x11);
}

static struct K007232_interface k007232_interface =
{
	1,		/* number of chips */
	{ REGION_SOUND1 },	/* memory regions */
	{ K007232_VOL(20,MIXER_PAN_CENTER,20,MIXER_PAN_CENTER) },	/* volume */
	{ volume_callback }	/* external port callback */
};



static struct MachineDriver machine_driver_scontra =
{
	{
		{
			CPU_KONAMI,	/* 052001 */
			3000000,	/* ? */
			scontra_readmem,scontra_writemem,0,0,
			scontra_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,		/* ? */
			scontra_readmem_sound,scontra_writemem_sound,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scontra_init_machine,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0, /* gfx decoded by konamiic.c */
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	scontra_vh_start,
	scontra_vh_stop,
	scontra_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
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

static struct MachineDriver machine_driver_thunderx =
{
	{
		{
			CPU_KONAMI,
			3000000,		/* ? */
			thunderx_readmem,thunderx_writemem,0,0,
			thunderx_interrupt,16	/* ???? */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,		/* ? */
			thunderx_readmem_sound,thunderx_writemem_sound,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	thunderx_init_machine,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0, /* gfx decoded by konamiic.c */
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	scontra_vh_start,
	scontra_vh_stop,
	scontra_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( scontra )
	ROM_REGION( 0x30800, REGION_CPU1 )	/* ROMs + banked RAM */
	ROM_LOAD( "e02.k11",     0x10000, 0x08000, 0xa61c0ead )	/* banked ROM */
	ROM_CONTINUE(            0x08000, 0x08000 )				/* fixed ROM */
	ROM_LOAD( "e03.k13",     0x20000, 0x10000, 0x00b02622 )	/* banked ROM */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the SOUND CPU */
	ROM_LOAD( "775-c01.bin", 0x00000, 0x08000, 0x0ced785a )

	ROM_REGION( 0x100000, REGION_GFX1 ) /* tiles */
	ROM_LOAD_GFX_EVEN( "775-a07a.bin", 0x00000, 0x20000, 0xe716bdf3 )	/* tiles */
	ROM_LOAD_GFX_ODD(  "775-a07e.bin", 0x00000, 0x20000, 0x0986e3a5 )
	ROM_LOAD_GFX_EVEN( "775-f07c.bin", 0x40000, 0x10000, 0xb0b30915 )
	ROM_LOAD_GFX_ODD(  "775-f07g.bin", 0x40000, 0x10000, 0xfbed827d )
	ROM_LOAD_GFX_EVEN( "775-f07d.bin", 0x60000, 0x10000, 0xf184be8e )
	ROM_LOAD_GFX_ODD(  "775-f07h.bin", 0x60000, 0x10000, 0x7b56c348 )
	ROM_LOAD_GFX_EVEN( "775-a08a.bin", 0x80000, 0x20000, 0x3ddd11a4 )
	ROM_LOAD_GFX_ODD(  "775-a08e.bin", 0x80000, 0x20000, 0x1007d963 )
	ROM_LOAD_GFX_EVEN( "775-f08c.bin", 0xc0000, 0x10000, 0x53abdaec )
	ROM_LOAD_GFX_ODD(  "775-f08g.bin", 0xc0000, 0x10000, 0x3df85a6e )
	ROM_LOAD_GFX_EVEN( "775-f08d.bin", 0xe0000, 0x10000, 0x102dcace )
	ROM_LOAD_GFX_ODD(  "775-f08h.bin", 0xe0000, 0x10000, 0xad9d7016 )

	ROM_REGION( 0x100000, REGION_GFX2 ) /* sprites */
	ROM_LOAD_GFX_EVEN( "775-a05a.bin", 0x00000, 0x10000, 0xa0767045 )	/* sprites */
	ROM_LOAD_GFX_ODD(  "775-a05e.bin", 0x00000, 0x10000, 0x2f656f08 )
	ROM_LOAD_GFX_EVEN( "775-a05b.bin", 0x20000, 0x10000, 0xab8ad4fd )
	ROM_LOAD_GFX_ODD(  "775-a05f.bin", 0x20000, 0x10000, 0x1c0eb1b6 )
	ROM_LOAD_GFX_EVEN( "775-f05c.bin", 0x40000, 0x10000, 0x5647761e )
	ROM_LOAD_GFX_ODD(  "775-f05g.bin", 0x40000, 0x10000, 0xa1692cca )
	ROM_LOAD_GFX_EVEN( "775-f05d.bin", 0x60000, 0x10000, 0xad676a6f )
	ROM_LOAD_GFX_ODD(  "775-f05h.bin", 0x60000, 0x10000, 0x3f925bcf )
	ROM_LOAD_GFX_EVEN( "775-a06a.bin", 0x80000, 0x10000, 0x77a34ad0 )
	ROM_LOAD_GFX_ODD(  "775-a06e.bin", 0x80000, 0x10000, 0x8a910c94 )
	ROM_LOAD_GFX_EVEN( "775-a06b.bin", 0xa0000, 0x10000, 0x563fb565 )
	ROM_LOAD_GFX_ODD(  "775-a06f.bin", 0xa0000, 0x10000, 0xe14995c0 )
	ROM_LOAD_GFX_EVEN( "775-f06c.bin", 0xc0000, 0x10000, 0x5ee6f3c1 )
	ROM_LOAD_GFX_ODD(  "775-f06g.bin", 0xc0000, 0x10000, 0x2645274d )
	ROM_LOAD_GFX_EVEN( "775-f06d.bin", 0xe0000, 0x10000, 0xc8b764fa )
	ROM_LOAD_GFX_ODD(  "775-f06h.bin", 0xe0000, 0x10000, 0xd6595f59 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* k007232 data */
	ROM_LOAD( "775-a04a.bin", 0x00000, 0x10000, 0x7efb2e0f )
	ROM_LOAD( "775-a04b.bin", 0x10000, 0x10000, 0xf41a2b33 )
	ROM_LOAD( "775-a04c.bin", 0x20000, 0x10000, 0xe4e58f14 )
	ROM_LOAD( "775-a04d.bin", 0x30000, 0x10000, 0xd46736f6 )
	ROM_LOAD( "775-f04e.bin", 0x40000, 0x10000, 0xfbf7e363 )
	ROM_LOAD( "775-f04f.bin", 0x50000, 0x10000, 0xb031ef2d )
	ROM_LOAD( "775-f04g.bin", 0x60000, 0x10000, 0xee107bbb )
	ROM_LOAD( "775-f04h.bin", 0x70000, 0x10000, 0xfb0fab46 )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "775a09.b19",   0x0000, 0x0100, 0x46d1e0df )	/* priority encoder (not used) */
ROM_END

ROM_START( scontraj )
	ROM_REGION( 0x30800, REGION_CPU1 )	/* ROMs + banked RAM */
	ROM_LOAD( "775-f02.bin", 0x10000, 0x08000, 0x8d5933a7 )	/* banked ROM */
	ROM_CONTINUE(            0x08000, 0x08000 )				/* fixed ROM */
	ROM_LOAD( "775-f03.bin", 0x20000, 0x10000, 0x1ef63d80 )	/* banked ROM */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the SOUND CPU */
	ROM_LOAD( "775-c01.bin", 0x00000, 0x08000, 0x0ced785a )

	ROM_REGION( 0x100000, REGION_GFX1 ) /* tiles */
	ROM_LOAD_GFX_EVEN( "775-a07a.bin", 0x00000, 0x20000, 0xe716bdf3 )	/* tiles */
	ROM_LOAD_GFX_ODD(  "775-a07e.bin", 0x00000, 0x20000, 0x0986e3a5 )
	ROM_LOAD_GFX_EVEN( "775-f07c.bin", 0x40000, 0x10000, 0xb0b30915 )
	ROM_LOAD_GFX_ODD(  "775-f07g.bin", 0x40000, 0x10000, 0xfbed827d )
	ROM_LOAD_GFX_EVEN( "775-f07d.bin", 0x60000, 0x10000, 0xf184be8e )
	ROM_LOAD_GFX_ODD(  "775-f07h.bin", 0x60000, 0x10000, 0x7b56c348 )
	ROM_LOAD_GFX_EVEN( "775-a08a.bin", 0x80000, 0x20000, 0x3ddd11a4 )
	ROM_LOAD_GFX_ODD(  "775-a08e.bin", 0x80000, 0x20000, 0x1007d963 )
	ROM_LOAD_GFX_EVEN( "775-f08c.bin", 0xc0000, 0x10000, 0x53abdaec )
	ROM_LOAD_GFX_ODD(  "775-f08g.bin", 0xc0000, 0x10000, 0x3df85a6e )
	ROM_LOAD_GFX_EVEN( "775-f08d.bin", 0xe0000, 0x10000, 0x102dcace )
	ROM_LOAD_GFX_ODD(  "775-f08h.bin", 0xe0000, 0x10000, 0xad9d7016 )

	ROM_REGION( 0x100000, REGION_GFX2 ) /* sprites */
	ROM_LOAD_GFX_EVEN( "775-a05a.bin", 0x00000, 0x10000, 0xa0767045 )	/* sprites */
	ROM_LOAD_GFX_ODD(  "775-a05e.bin", 0x00000, 0x10000, 0x2f656f08 )
	ROM_LOAD_GFX_EVEN( "775-a05b.bin", 0x20000, 0x10000, 0xab8ad4fd )
	ROM_LOAD_GFX_ODD(  "775-a05f.bin", 0x20000, 0x10000, 0x1c0eb1b6 )
	ROM_LOAD_GFX_EVEN( "775-f05c.bin", 0x40000, 0x10000, 0x5647761e )
	ROM_LOAD_GFX_ODD(  "775-f05g.bin", 0x40000, 0x10000, 0xa1692cca )
	ROM_LOAD_GFX_EVEN( "775-f05d.bin", 0x60000, 0x10000, 0xad676a6f )
	ROM_LOAD_GFX_ODD(  "775-f05h.bin", 0x60000, 0x10000, 0x3f925bcf )
	ROM_LOAD_GFX_EVEN( "775-a06a.bin", 0x80000, 0x10000, 0x77a34ad0 )
	ROM_LOAD_GFX_ODD(  "775-a06e.bin", 0x80000, 0x10000, 0x8a910c94 )
	ROM_LOAD_GFX_EVEN( "775-a06b.bin", 0xa0000, 0x10000, 0x563fb565 )
	ROM_LOAD_GFX_ODD(  "775-a06f.bin", 0xa0000, 0x10000, 0xe14995c0 )
	ROM_LOAD_GFX_EVEN( "775-f06c.bin", 0xc0000, 0x10000, 0x5ee6f3c1 )
	ROM_LOAD_GFX_ODD(  "775-f06g.bin", 0xc0000, 0x10000, 0x2645274d )
	ROM_LOAD_GFX_EVEN( "775-f06d.bin", 0xe0000, 0x10000, 0xc8b764fa )
	ROM_LOAD_GFX_ODD(  "775-f06h.bin", 0xe0000, 0x10000, 0xd6595f59 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* k007232 data */
	ROM_LOAD( "775-a04a.bin", 0x00000, 0x10000, 0x7efb2e0f )
	ROM_LOAD( "775-a04b.bin", 0x10000, 0x10000, 0xf41a2b33 )
	ROM_LOAD( "775-a04c.bin", 0x20000, 0x10000, 0xe4e58f14 )
	ROM_LOAD( "775-a04d.bin", 0x30000, 0x10000, 0xd46736f6 )
	ROM_LOAD( "775-f04e.bin", 0x40000, 0x10000, 0xfbf7e363 )
	ROM_LOAD( "775-f04f.bin", 0x50000, 0x10000, 0xb031ef2d )
	ROM_LOAD( "775-f04g.bin", 0x60000, 0x10000, 0xee107bbb )
	ROM_LOAD( "775-f04h.bin", 0x70000, 0x10000, 0xfb0fab46 )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "775a09.b19",   0x0000, 0x0100, 0x46d1e0df )	/* priority encoder (not used) */
ROM_END

ROM_START( thunderx )
	ROM_REGION( 0x29000, REGION_CPU1 )	/* ROMs + banked RAM */
	ROM_LOAD( "873k03.k15", 0x10000, 0x10000, 0x276817ad )
	ROM_LOAD( "873k02.k13", 0x20000, 0x08000, 0x80cc1c45 )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "873h01.f8",    0x0000, 0x8000, 0x990b7a7c )

	ROM_REGION( 0x80000, REGION_GFX1 )	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD_GFX_EVEN( "873c06a.f6",   0x00000, 0x10000, 0x0e340b67 ) /* Chars */
	ROM_LOAD_GFX_ODD ( "873c06c.f5",   0x00000, 0x10000, 0xef0e72cd )
	ROM_LOAD_GFX_EVEN( "873c06b.e6",   0x20000, 0x10000, 0x97ad202e )
	ROM_LOAD_GFX_ODD ( "873c06d.e5",   0x20000, 0x10000, 0x8393d42e )
	ROM_LOAD_GFX_EVEN( "873c07a.f4",   0x40000, 0x10000, 0xa8aab84f )
	ROM_LOAD_GFX_ODD ( "873c07c.f3",   0x40000, 0x10000, 0x2521009a )
	ROM_LOAD_GFX_EVEN( "873c07b.e4",   0x60000, 0x10000, 0x12a2b8ba )
	ROM_LOAD_GFX_ODD ( "873c07d.e3",   0x60000, 0x10000, 0xfae9f965 )

	ROM_REGION( 0x80000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "873c04a.f11",  0x00000, 0x10000, 0xf7740bf3 ) /* Sprites */
	ROM_LOAD_GFX_ODD ( "873c04c.f10",  0x00000, 0x10000, 0x5dacbd2b )
	ROM_LOAD_GFX_EVEN( "873c04b.e11",  0x20000, 0x10000, 0x9ac581da )
	ROM_LOAD_GFX_ODD ( "873c04d.e10",  0x20000, 0x10000, 0x44a4668c )
	ROM_LOAD_GFX_EVEN( "873c05a.f9",   0x40000, 0x10000, 0xd73e107d )
	ROM_LOAD_GFX_ODD ( "873c05c.f8",   0x40000, 0x10000, 0x59903200 )
	ROM_LOAD_GFX_EVEN( "873c05b.e9",   0x60000, 0x10000, 0x81059b99 )
	ROM_LOAD_GFX_ODD ( "873c05d.e8",   0x60000, 0x10000, 0x7fa3d7df )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "873a08.f20",   0x0000, 0x0100, 0xe2d09a1b )	/* priority encoder (not used) */
ROM_END

ROM_START( thnderxj )
	ROM_REGION( 0x29000, REGION_CPU1 )	/* ROMs + banked RAM */
	ROM_LOAD( "873-n03.k15", 0x10000, 0x10000, 0xa01e2e3e )
	ROM_LOAD( "873-n02.k13", 0x20000, 0x08000, 0x55afa2cc )
	ROM_CONTINUE(            0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "873-f01.f8",   0x0000, 0x8000, 0xea35ffa3 )

	ROM_REGION( 0x80000, REGION_GFX1 )	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD_GFX_EVEN( "873c06a.f6",   0x00000, 0x10000, 0x0e340b67 ) /* Chars */
	ROM_LOAD_GFX_ODD ( "873c06c.f5",   0x00000, 0x10000, 0xef0e72cd )
	ROM_LOAD_GFX_EVEN( "873c06b.e6",   0x20000, 0x10000, 0x97ad202e )
	ROM_LOAD_GFX_ODD ( "873c06d.e5",   0x20000, 0x10000, 0x8393d42e )
	ROM_LOAD_GFX_EVEN( "873c07a.f4",   0x40000, 0x10000, 0xa8aab84f )
	ROM_LOAD_GFX_ODD ( "873c07c.f3",   0x40000, 0x10000, 0x2521009a )
	ROM_LOAD_GFX_EVEN( "873c07b.e4",   0x60000, 0x10000, 0x12a2b8ba )
	ROM_LOAD_GFX_ODD ( "873c07d.e3",   0x60000, 0x10000, 0xfae9f965 )

	ROM_REGION( 0x80000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "873c04a.f11",  0x00000, 0x10000, 0xf7740bf3 ) /* Sprites */
	ROM_LOAD_GFX_ODD ( "873c04c.f10",  0x00000, 0x10000, 0x5dacbd2b )
	ROM_LOAD_GFX_EVEN( "873c04b.e11",  0x20000, 0x10000, 0x9ac581da )
	ROM_LOAD_GFX_ODD ( "873c04d.e10",  0x20000, 0x10000, 0x44a4668c )
	ROM_LOAD_GFX_EVEN( "873c05a.f9",   0x40000, 0x10000, 0xd73e107d )
	ROM_LOAD_GFX_ODD ( "873c05c.f8",   0x40000, 0x10000, 0x59903200 )
	ROM_LOAD_GFX_EVEN( "873c05b.e9",   0x60000, 0x10000, 0x81059b99 )
	ROM_LOAD_GFX_ODD ( "873c05d.e8",   0x60000, 0x10000, 0x7fa3d7df )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "873a08.f20",   0x0000, 0x0100, 0xe2d09a1b )	/* priority encoder (not used) */
ROM_END

/***************************************************************************/

static void thunderx_banking( int lines )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int offs;

//	logerror("thunderx %04x: bank select %02x\n", cpu_get_pc(), lines );

	offs = 0x10000 + (((lines & 0x0f) ^ 0x08) * 0x2000);
	if (offs >= 0x28000) offs -= 0x20000;
	cpu_setbank( 1, &RAM[offs] );
}

static void scontra_init_machine( void )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	paletteram = &RAM[0x30000];
}

static void thunderx_init_machine( void )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	konami_cpu_setlines_callback = thunderx_banking;
	cpu_setbank( 1, &RAM[0x10000] ); /* init the default bank */

	paletteram = &RAM[0x28000];
	unknownram = &RAM[0x28800];
}

static void init_scontra(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_2(REGION_GFX2);
}



GAME( 1988, scontra,  0,        scontra,  scontra,  scontra, ROT90, "Konami", "Super Contra" )
GAME( 1988, scontraj, scontra,  scontra,  scontra,  scontra, ROT90, "Konami", "Super Contra (Japan)" )
GAMEX(1988, thunderx, 0,        thunderx, thunderx, scontra, ROT0, "Konami", "Thunder Cross", GAME_NOT_WORKING )
GAMEX(1988, thnderxj, thunderx, thunderx, thunderx, scontra, ROT0, "Konami", "Thunder Cross (Japan)", GAME_NOT_WORKING )
