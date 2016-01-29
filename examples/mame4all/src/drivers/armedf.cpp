#include "../vidhrdw/armedf.cpp"

/**********************************************************************

Armed Formation
(c)1988 Nichibutsu

Terra Force
(c)1987 Nichibutsu

Kodure Ookami
(c)1987 Nichibutsu

Crazy Climber 2
(c)1988 Nichibutsu

68000 + Z80

***********************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"

extern void armedf_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
extern void cclimbr2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
extern int terraf_vh_start(void);
extern int armedf_vh_start(void);
extern int kodure_vh_start(void);
extern void armedf_vh_stop(void);

WRITE_HANDLER( armedf_bg_videoram_w );
READ_HANDLER( armedf_bg_videoram_r );
WRITE_HANDLER( armedf_fg_videoram_w );
READ_HANDLER( armedf_fg_videoram_r );
READ_HANDLER( armedf_text_videoram_r );
WRITE_HANDLER( armedf_text_videoram_w );
READ_HANDLER( terraf_text_videoram_r );
WRITE_HANDLER( terraf_text_videoram_w );

extern UINT16 armedf_vreg;
extern UINT16 terraf_scroll_msb;
extern UINT16 armedf_bg_scrollx,armedf_bg_scrolly;
extern unsigned char *armedf_bg_videoram;
extern UINT16 armedf_fg_scrollx,armedf_fg_scrolly;
extern unsigned char *armedf_fg_videoram;

static WRITE_HANDLER( io_w ){
	switch (offset/2){
		case 0x0:
		armedf_vreg = COMBINE_WORD(armedf_vreg,data);
		/* bits 0 and 1 of armedf_vreg are coin counters */
		break;

		case 0x1:
		armedf_bg_scrollx = COMBINE_WORD(armedf_bg_scrollx, data);
		break;

		case 0x2:
		armedf_bg_scrolly = COMBINE_WORD(armedf_bg_scrolly, data);
		break;

		case 0x3:
		armedf_fg_scrollx = COMBINE_WORD(armedf_fg_scrollx, data);
		break;

		case 0x4:
		{
			static int flipflop = 0; /* hack? - needed for Terra Force */
			flipflop = 1-flipflop;
			if( !flipflop ){
				armedf_fg_scrolly = COMBINE_WORD(armedf_fg_scrolly, data);
			}
			else {
				terraf_scroll_msb = COMBINE_WORD(terraf_scroll_msb, data);
			}
		}
		break;

		case 0x5:
		soundlatch_w(offset,((data & 0x7f) << 1) | 1);
		break;

		default:
		//logerror("IO OUTPUT [%x] <- %08x\n", offset,data );
		break;
	}
}

static WRITE_HANDLER( kodure_io_w ){
	switch (offset/2){
		case 0x0:
		armedf_vreg = COMBINE_WORD(armedf_vreg,data);
		/* bits 0 and 1 of armedf_vreg are coin counters */

		/* This is a temporary condition specification. */
		if (!(armedf_vreg & 0x0080))
		{
			int i;
			for (i = 0; i < 0x2000; i++)
			{
				terraf_text_videoram_w(i, ' ');
			}
		}
		break;

		case 0x1:
		armedf_bg_scrollx = COMBINE_WORD(armedf_bg_scrollx, data);
		break;

		case 0x2:
		armedf_bg_scrolly = COMBINE_WORD(armedf_bg_scrolly, data);
		break;

		case 0x5:
		soundlatch_w(offset,((data & 0x7f) << 1) | 1);
		break;

		default:
		//logerror("IO OUTPUT [%x] <- %08x\n", offset,data );
		break;
	}
}

static READ_HANDLER( io_r ){
	switch (offset) {
		case 0: /* Input */
		return input_port_0_r(offset) + 256*input_port_2_r(offset);

		case 2: /* Input */
		return input_port_1_r(offset) + 256*input_port_3_r(offset);

		case 4: /* DIPSWITCH */
		return input_port_4_r(offset);

		case 6: /* DIPSWITCH */
		return input_port_5_r(offset);

		default: return (0xffff);
	}
}

/* the scroll registers are memory mapped in kodure, I/O ports in the others */
static WRITE_HANDLER( kodure_videoreg_w )
{
	switch (offset)
	{
		case	0x16:
			armedf_fg_scrolly = (data & 0x00ff);
			break;
		case	0x18:
			armedf_fg_scrolly |= ((data & 0x00ff) << 8);
			break;
		case	0x1a:
			armedf_fg_scrollx = (data & 0x00ff);
			break;
		case	0x1c:
			armedf_fg_scrollx |= ((data & 0x00ff) << 8);
			break;
		default:
			break;
	}
}

static struct MemoryReadAddress terraf_readmem[] = {
	{ 0x000000, 0x04ffff, MRA_ROM },
	{ 0x060000, 0x063fff, MRA_BANK1 }, /* sprites */
	{ 0x064000, 0x064fff, paletteram_word_r },

	{ 0x068000, 0x069fff, terraf_text_videoram_r },
	{ 0x06a000, 0x06a9ff, MRA_BANK2 },

	{ 0x06C000, 0x06C9ff, MRA_BANK3 },
	{ 0x070000, 0x070fff, armedf_fg_videoram_r },
	{ 0x074000, 0x074fff, armedf_bg_videoram_r },
	{ 0x078000, 0x07800f, io_r },
	{ -1 }
};

static struct MemoryWriteAddress terraf_writemem[] = {
	{ 0x000000, 0x04ffff, MWA_ROM },
	{ 0x060000, 0x063fff, MWA_BANK1, &spriteram },
	{ 0x064000, 0x064fff, paletteram_xxxxRRRRGGGGBBBB_word_w, &paletteram },

	{ 0x068000, 0x069fff, terraf_text_videoram_w, &videoram },
	{ 0x06a000, 0x06a9ff, MWA_BANK2 },

	{ 0x06C000, 0x06C9ff, MWA_BANK3 },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_bg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_fg_videoram },
	{ 0x07c000, 0x07c00f, io_w },
	{ 0x0c0000, 0x0c0001, MWA_NOP }, /* watchdog? */
	{ -1 }
};

static struct MemoryReadAddress kodure_readmem[] = {
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x060000, 0x063fff, MRA_BANK1 }, /* sprites */
	{ 0x064000, 0x064fff, paletteram_word_r },

	{ 0x068000, 0x069fff, terraf_text_videoram_r },
	{ 0x06a000, 0x06a9ff, MRA_BANK2 },

	{ 0x06C000, 0x06C9ff, MRA_BANK3 },
	{ 0x070000, 0x070fff, armedf_fg_videoram_r },
	{ 0x074000, 0x074fff, armedf_bg_videoram_r },
	{ 0x078000, 0x07800f, io_r },
	{ -1 }
};

static struct MemoryWriteAddress kodure_writemem[] = {
	{ 0x000000, 0x05ffff, MWA_ROM },
	{ 0x060000, 0x063fff, MWA_BANK1, &spriteram },
	{ 0x064000, 0x064fff, paletteram_xxxxRRRRGGGGBBBB_word_w, &paletteram },

	{ 0x068000, 0x06803f, kodure_videoreg_w },
	{ 0x068000, 0x069fff, terraf_text_videoram_w, &videoram },
	{ 0x06a000, 0x06a9ff, MWA_BANK2 },

	{ 0x06C000, 0x06C9ff, MWA_BANK3 },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_bg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_fg_videoram },
	{ 0x07c000, 0x07c00f, kodure_io_w },
	{ 0x0c0000, 0x0c0001, MWA_NOP }, /* watchdog? */

	{ 0xffd000, 0xffd001, MWA_NOP }, /* ? */
	{ -1 }
};

static struct MemoryReadAddress armedf_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x060000, 0x060fff, MRA_RAM },
	{ 0x061000, 0x065fff, MRA_RAM },
	{ 0x066000, 0x066fff, armedf_bg_videoram_r },
	{ 0x067000, 0x067fff, armedf_fg_videoram_r },
	{ 0x068000, 0x0682ff, MRA_RAM },
	{ 0x068300, 0x069fff, armedf_text_videoram_r },
	{ 0x06a000, 0x06afff, MRA_RAM },
	{ 0x06b000, 0x06bfff, MRA_RAM },
	{ 0x06c000, 0x06c00f, io_r },
	{ 0x06c010, 0x06c7ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress armedf_writemem[] ={
	{ 0x000000, 0x05ffff, MWA_ROM },
	{ 0x060000, 0x060fff, MWA_RAM, &spriteram },
	{ 0x061000, 0x065fff, MWA_RAM },
	{ 0x066000, 0x066fff, armedf_bg_videoram_w, &armedf_bg_videoram },
	{ 0x067000, 0x067fff, armedf_fg_videoram_w, &armedf_fg_videoram },
	{ 0x068000, 0x0682ff, MWA_RAM },
	{ 0x068300, 0x069fff, armedf_text_videoram_w, &videoram },
	{ 0x068d00, 0x068fff, MWA_RAM },
	{ 0x06a000, 0x06afff, paletteram_xxxxRRRRGGGGBBBB_word_w, &paletteram },
	{ 0x06b000, 0x06bfff, MWA_RAM },
	{ 0x06c000, 0x06c7ff, MWA_RAM },
	{ 0x06d000, 0x06d00f, io_w }, /* Output */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress cclimbr2_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x060000, 0x063fff, MRA_BANK1 }, /* sprites */
	{ 0x064000, 0x064fff, paletteram_word_r },

	{ 0x068000, 0x069fff, terraf_text_videoram_r },
	{ 0x06a000, 0x06a9ff, MRA_BANK2 },

	{ 0x06C000, 0x06C9ff, MRA_BANK3 },
	{ 0x070000, 0x070fff, armedf_fg_videoram_r },
	{ 0x074000, 0x074fff, armedf_bg_videoram_r },
	{ 0x078000, 0x07800f, io_r },
	{ -1 }
};

static struct MemoryWriteAddress cclimbr2_writemem[] ={
	{ 0x000000, 0x05ffff, MWA_ROM },
	{ 0x060000, 0x063fff, MWA_BANK1, &spriteram },
	{ 0x064000, 0x064fff, paletteram_xxxxRRRRGGGGBBBB_word_w, &paletteram },

	{ 0x068000, 0x069fff, terraf_text_videoram_w, &videoram },
	{ 0x06a000, 0x06a9ff, MWA_BANK2 },

	{ 0x06C000, 0x06C9ff, MWA_BANK3 },
	{ 0x06ca00, 0x06cbff, MWA_RAM },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_bg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_fg_videoram },
	{ 0x07c000, 0x07c00f, io_w },
	{ -1 }
};

static struct MemoryReadAddress soundreadmem[] ={
	{ 0x0000, 0xf7ff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress soundwritemem[] ={
	{ 0x0000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress cclimbr2_soundreadmem[] ={
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress cclimbr2_soundwritemem[] ={
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};

static READ_HANDLER( soundlatch_clear_r )
{
	soundlatch_clear_w(0,0);
	return 0;
}

static struct IOReadPort readport[] =
{
	{ 0x4, 0x4, soundlatch_clear_r },
	{ 0x6, 0x6, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x0, 0x0, YM3812_control_port_0_w },
	{ 0x1, 0x1, YM3812_write_port_0_w },
  	{ 0x2, 0x2, DAC_0_signed_data_w },
  	{ 0x3, 0x3, DAC_1_signed_data_w },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( armedf )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Coin, Start */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Test Mode */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )     /* Tilt */
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3")
	PORT_DIPSETTING(    0x02, "4")
	PORT_DIPSETTING(    0x01, "5")
	PORT_DIPSETTING(    0x00, "6")
	PORT_DIPNAME( 0x04, 0x04, "First Bonus" )
	PORT_DIPSETTING(    0x04, "20k")
	PORT_DIPSETTING(    0x00, "40k")
	PORT_DIPNAME( 0x08, 0x08, "Second Bonus" )
	PORT_DIPSETTING(    0x08, "60k")
	PORT_DIPSETTING(    0x00, "80k")
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy")
	PORT_DIPSETTING(    0x80, "Normal")
	PORT_DIPSETTING(    0x40, "Hard")
	PORT_DIPSETTING(    0x00, "Very Hard")

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, "Flip Screen ?" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( terraf )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Coin, Start */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Test Mode */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )     /* Tilt */
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3")
	PORT_DIPSETTING(    0x02, "4")
	PORT_DIPSETTING(    0x01, "5")
	PORT_DIPSETTING(    0x00, "6")
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20k")
	PORT_DIPSETTING(    0x08, "50k")
	PORT_DIPSETTING(    0x04, "60k")
	PORT_DIPSETTING(    0x00, "90k")
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy")
	PORT_DIPSETTING(    0x80, "Normal")
	PORT_DIPSETTING(    0x40, "Hard")
	PORT_DIPSETTING(    0x00, "Very Hard")

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Allow Continue" )
	PORT_DIPSETTING(    0xc0, "0" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "5" )
	PORT_DIPSETTING(    0x00, "Unlimited" )
INPUT_PORTS_END

INPUT_PORTS_START( kodure )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Coin, Start */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Test Mode */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )     /* Tilt */
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3")
	PORT_DIPSETTING(    0x02, "4")
	PORT_DIPSETTING(    0x01, "5")
	PORT_DIPSETTING(    0x00, "6")
	PORT_DIPNAME( 0x04, 0x04, "1st Bonus Life" )
	PORT_DIPSETTING(    0x04, "None")
	PORT_DIPSETTING(    0x00, "50k")
	PORT_DIPNAME( 0x08, 0x08, "2nd Bonus Life" )
	PORT_DIPSETTING(    0x08, "60k")
	PORT_DIPSETTING(    0x00, "90k")
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy")
	PORT_DIPSETTING(    0x00, "Hard")
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( cclimbr2 )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* Coin, Start */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Test Mode */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )     /* Tilt */
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3")
	PORT_DIPSETTING(    0x02, "4")
	PORT_DIPSETTING(    0x01, "5")
	PORT_DIPSETTING(    0x00, "6")
	PORT_DIPNAME( 0x04, 0x04, "First Bonus" )
	PORT_DIPSETTING(    0x04, "30k")
	PORT_DIPSETTING(    0x00, "60k")
	PORT_DIPNAME( 0x08, 0x08, "Second Bonus" )
	PORT_DIPSETTING(    0x08, "70k Only")
	PORT_DIPSETTING(    0x00, "Nothing")
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy")
	PORT_DIPSETTING(    0x00, "Hard")
#if 0
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#endif

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, "Continue Play" )
	PORT_DIPSETTING(    0x10, "3")
	PORT_DIPSETTING(    0x00, "0")
#if 0
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#endif
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#if 0
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#endif
INPUT_PORTS_END

static struct GfxLayout char_layout = {
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout tile_layout = {
	16,16,	/* 16*16 chars */
	2048,
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* plane offset */
	{ 4, 0, 12, 8, 20, 16, 28, 24,
		32+4, 32+0, 32+12, 32+8, 32+20, 32+16, 32+28, 32+24, },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
		8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8
};

static struct GfxLayout tile_layout2 = {
	16,16,	/* 16*16 chars */
	1024,
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* plane offset */
	{ 4, 0, 12, 8, 20, 16, 28, 24,
		32+4, 32+0, 32+12, 32+8, 32+20, 32+16, 32+28, 32+24, },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
		8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8
};

static struct GfxLayout sprite_layout = {
	16,16,	/* 16*16 chars */
	2048,
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* plane offset */
	{ 4, 0, 4+0x20000*8, 0+0x20000*8, 12, 8, 12+0x20000*8, 8+0x20000*8,
		20, 16, 20+0x20000*8, 16+0x20000*8, 28, 24, 28+0x20000*8, 24+0x20000*8 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
		8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &char_layout,		 0*16,	32 },
	{ REGION_GFX2, 0, &tile_layout,		64*16,	32 },
	{ REGION_GFX3, 0, &tile_layout2,	96*16,	32 },
	{ REGION_GFX4, 0, &sprite_layout,	32*16,	32 },
	{ -1 } /* end of array */
};

static struct YM3812interface ym3812_interface =
{
	1,				/* 1 chip (no more supported) */
	4000000,        /* 4 MHz */
	{ 255 }         /* (not supported) */
};

int armedf_interrupt(void){
	return (1);
}

int cclimbr2_interrupt(void){
	return (2);
}

static struct DACinterface dac_interface =
{
	2,	/* 2 channels */
	{ 100,100 },
};

static struct DACinterface cclimbr2_dac_interface =
{
	2,	/* 2 channels */
	{ 40, 40 },
};

static struct MachineDriver machine_driver_terraf =
{
	{
		{
			CPU_M68000,
			8000000, /* 8 Mhz?? */
			terraf_readmem,terraf_writemem,0,0,
			armedf_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3072000,	/* 3.072 Mhz???? */
			soundreadmem,soundwritemem,readport,writeport,
			interrupt,128
		},
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	38*8, 32*8, { 0*8, 38*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	2048,2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	terraf_vh_start,
	armedf_vh_stop,
	armedf_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
		   SOUND_YM3812,
		   &ym3812_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_kodure =
{
	{
		{
			CPU_M68000,
			8000000, /* 8 Mhz?? */
			kodure_readmem, kodure_writemem, 0, 0,
			armedf_interrupt, 1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3072000,	/* 3.072 Mhz???? */
			soundreadmem, soundwritemem, readport, writeport,
			interrupt, 128
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	38*8, 32*8, { 0*8, 38*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	2048,2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	kodure_vh_start,
	armedf_vh_stop,
	armedf_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_armedf =
{
	{
		{
			CPU_M68000,
			8000000, /* 8 Mhz?? */
			armedf_readmem,armedf_writemem,0,0,
			armedf_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3072000,	/* 3.072 Mhz???? */
			soundreadmem,soundwritemem,readport,writeport,
			interrupt,128
		},
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	38*8, 32*8, { 0*8, 38*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	2048,2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	armedf_vh_start,
	armedf_vh_stop,
	armedf_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
		   SOUND_YM3812,
		   &ym3812_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_cclimbr2 =
{
	{
		{
			CPU_M68000,
			8000000, /* 8 Mhz?? */
			cclimbr2_readmem,cclimbr2_writemem,0,0,
			cclimbr2_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3072000,	/* 3.072 Mhz???? */
			cclimbr2_soundreadmem,cclimbr2_soundwritemem,readport,writeport,
			interrupt,128
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,						// nogi
	0,

	/* video hardware */
	38*8, 32*8, { 1*8, 37*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	2048,2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	terraf_vh_start,
	armedf_vh_stop,
	cclimbr2_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
		   SOUND_YM3812,
		   &ym3812_interface
		},
		{
			SOUND_DAC,
			&cclimbr2_dac_interface
		}
	}
};



ROM_START( terraf )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 64K*8 for 68000 code */
	ROM_LOAD_EVEN( "terrafor.014", 0x00000, 0x10000, 0x8e5f557f )
	ROM_LOAD_ODD(  "terrafor.011", 0x00000, 0x10000, 0x5320162a )
	ROM_LOAD_EVEN( "terrafor.013", 0x20000, 0x10000, 0xa86951e0 )
	ROM_LOAD_ODD(  "terrafor.010", 0x20000, 0x10000, 0x58b5f43b )
	ROM_LOAD_EVEN( "terrafor.012", 0x40000, 0x08000, 0x4f0e1d76 )
	ROM_LOAD_ODD(  "terrafor.009", 0x40000, 0x08000, 0xd1014280 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Z80 code (sound) */
	ROM_LOAD( "terrafor.001", 0x00000, 0x10000, 0xeb6b4138 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.008", 0x00000, 0x08000, 0xbc6f7cbc ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.006", 0x00000, 0x10000, 0x25d23dfd ) /* foreground tiles */
	ROM_LOAD( "terrafor.007", 0x10000, 0x10000, 0xb9b0fe27 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.004", 0x00000, 0x10000, 0x2144d8e0 ) /* background tiles */
	ROM_LOAD( "terrafor.005", 0x10000, 0x10000, 0x744f5c9e )

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.003", 0x00000, 0x10000, 0xd74085a1 ) /* sprites */
	ROM_LOAD( "terrafor.002", 0x20000, 0x10000, 0x148aa0c5 )

	ROM_REGION( 0x4000, REGION_GFX5 )	/* data for mcu/blitter? */
	ROM_LOAD( "tf.10",        0x0000, 0x4000, 0xac705812 )	/* TEST DATA ? */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "tf.clr",       0x0000, 0x0100, 0x81244757 )	/* ??? */
ROM_END

ROM_START( terrafu )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 64K*8 for 68000 code */
	ROM_LOAD_EVEN( "tf.8",         0x00000, 0x10000, 0xfea6dd64 )
	ROM_LOAD_ODD(  "tf.3",         0x00000, 0x10000, 0x02f9d05a )
	ROM_LOAD_EVEN( "tf.7",         0x20000, 0x10000, 0xfde8de7e )
	ROM_LOAD_ODD(  "tf.2",         0x20000, 0x10000, 0xdb987414 )
	ROM_LOAD_EVEN( "tf.6",         0x40000, 0x08000, 0xb91e9ba3 )
	ROM_LOAD_ODD(  "tf.1",         0x40000, 0x08000, 0xd6e22375 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Z80 code (sound) */
	ROM_LOAD( "terrafor.001", 0x00000, 0x10000, 0xeb6b4138 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.008", 0x00000, 0x08000, 0xbc6f7cbc ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.006", 0x00000, 0x10000, 0x25d23dfd ) /* foreground tiles */
	ROM_LOAD( "terrafor.007", 0x10000, 0x10000, 0xb9b0fe27 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.004", 0x00000, 0x10000, 0x2144d8e0 ) /* background tiles */
	ROM_LOAD( "terrafor.005", 0x10000, 0x10000, 0x744f5c9e )

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "terrafor.003", 0x00000, 0x10000, 0xd74085a1 ) /* sprites */
	ROM_LOAD( "terrafor.002", 0x20000, 0x10000, 0x148aa0c5 )

	ROM_REGION( 0x4000, REGION_GFX5 )	/* data for mcu/blitter? */
	ROM_LOAD( "tf.10",        0x0000, 0x4000, 0xac705812 )	/* TEST DATA ? */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "tf.clr",       0x0000, 0x0100, 0x81244757 )	/* ??? */
ROM_END

ROM_START( kodure )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 64K*8 for 68000 code */
	ROM_LOAD_EVEN( "kodure8.6e", 0x00000, 0x10000, 0x6bbfb1e6 )
	ROM_LOAD_ODD(  "kodure3.6h", 0x00000, 0x10000, 0xf9178ec8 )
	ROM_LOAD_EVEN( "kodure7.5e", 0x20000, 0x10000, 0xa7ee09bb )
	ROM_LOAD_ODD(  "kodure2.5h", 0x20000, 0x10000, 0x236d820f )
	ROM_LOAD_EVEN( "kodure6.3e", 0x40000, 0x10000, 0x9120e728 )
	ROM_LOAD_ODD(  "kodure1.3h", 0x40000, 0x10000, 0x345fe7a5 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Z80 code (sound) */
	ROM_LOAD( "kodure11.17k", 0x00000, 0x10000, 0xdba51e2d )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "kodure9.11e", 0x00000, 0x08000, 0xe041356e )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "kodure5.15h", 0x00000, 0x20000, 0x0b510258 )	/* foreground tiles */
	ROM_LOAD( "kodure4.14h", 0x20000, 0x10000, 0xfb8e13e6 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "kodure14.8a", 0x00000, 0x10000, 0x94a9c3d0 )	/* background tiles */

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "kodure12.8d", 0x00000, 0x20000, 0x15f4021d )	/* sprites */
	ROM_LOAD( "kodure13.9d", 0x20000, 0x20000, 0xb3b6c753 )

	ROM_REGION( 0x4000, REGION_GFX5 )	/* data for mcu/blitter? */
	ROM_LOAD( "kodure10.11c", 0x0000, 0x4000, 0xf48be21d )	/* TEST DATA ? */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "tf.11j", 0x0000, 0x0100, 0x81244757 )	/* ??? */
ROM_END

ROM_START( armedf )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "af_06.rom", 0x00000, 0x10000, 0xc5326603 )
	ROM_LOAD_ODD(  "af_01.rom", 0x00000, 0x10000, 0x458e9542 )
	ROM_LOAD_EVEN( "af_07.rom", 0x20000, 0x10000, 0xcc8517f5 )
	ROM_LOAD_ODD(  "af_02.rom", 0x20000, 0x10000, 0x214ef220 )
	ROM_LOAD_EVEN( "af_08.rom", 0x40000, 0x10000, 0xd1d43600 )
	ROM_LOAD_ODD(  "af_03.rom", 0x40000, 0x10000, 0xbbe1fe2d )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Z80 code (sound) */
	ROM_LOAD( "af_10.rom", 0x00000, 0x10000, 0xc5eacb87 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "af_09.rom", 0x00000, 0x08000, 0x7025e92d ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "af_04.rom", 0x00000, 0x10000, 0x44d3af4f ) /* foreground tiles */
	ROM_LOAD( "af_05.rom", 0x10000, 0x10000, 0x92076cab )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "af_14.rom", 0x00000, 0x10000, 0x8c5dc5a7 ) /* background tiles */
	ROM_LOAD( "af_13.rom", 0x10000, 0x10000, 0x136a58a3 )

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "af_11.rom", 0x00000, 0x20000, 0xb46c473c ) /* sprites */
	ROM_LOAD( "af_12.rom", 0x20000, 0x20000, 0x23cb6bfe )
ROM_END

ROM_START( cclimbr2 )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 64K*8 for 68000 code */
	ROM_LOAD_EVEN( "4.bin", 0x00000, 0x10000, 0x7922ea14 )
	ROM_LOAD_ODD(  "1.bin", 0x00000, 0x10000, 0x2ac7ed67 )
	ROM_LOAD_EVEN( "6.bin", 0x20000, 0x10000, 0x7905c992 )
	ROM_LOAD_ODD(  "5.bin", 0x20000, 0x10000, 0x47be6c1e )
	ROM_LOAD_EVEN( "3.bin", 0x40000, 0x10000, 0x1fb110d6 )
	ROM_LOAD_ODD(  "2.bin", 0x40000, 0x10000, 0x0024c15b )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Z80 code (sound) */
	ROM_LOAD( "11.bin", 0x00000, 0x04000, 0xfe0175be )
	ROM_LOAD( "12.bin", 0x04000, 0x08000, 0x5ddf18f2 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "10.bin", 0x00000, 0x08000, 0x7f475266 ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "7.bin",  0x00000, 0x10000, 0xcbdd3906 ) /* foreground tiles */
	ROM_LOAD( "8.bin",  0x10000, 0x10000, 0xb2a613c0 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "17.bin", 0x00000, 0x10000, 0xe24bb2d7 ) /* background tiles */
	ROM_LOAD( "18.bin", 0x10000, 0x10000, 0x56834554 )

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15.bin", 0x00000, 0x10000, 0x4bf838be ) /* sprites */
	ROM_LOAD( "16.bin", 0x10000, 0x10000, 0x21a265c5 )
	ROM_LOAD( "13.bin", 0x20000, 0x10000, 0x6b6ec999 )
	ROM_LOAD( "14.bin", 0x30000, 0x10000, 0xf426a4ad )

	ROM_REGION( 0x4000, REGION_GFX5 )	/* data for mcu/blitter? */
	ROM_LOAD( "9.bin",  0x0000, 0x4000, 0x740d260f )	// DATA ?
ROM_END



GAMEX( 1987, terraf,   0,      terraf,   terraf,   0, ROT0,   "Nichibutsu", "Terra Force", GAME_NO_COCKTAIL )
GAMEX( 1987, terrafu,  terraf, terraf,   terraf,   0, ROT0,   "Nichibutsu USA", "Terra Force (US)", GAME_NO_COCKTAIL )
GAMEX( 1987, kodure,   0,      kodure,   kodure,   0, ROT0,   "Nichibutsu", "Kodure Ookami (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1988, armedf,   0,      armedf,   armedf,   0, ROT270, "Nichibutsu", "Armed Formation", GAME_NO_COCKTAIL )
GAMEX( 1988, cclimbr2, 0,      cclimbr2, cclimbr2, 0, ROT0,   "Nichibutsu", "Crazy Climber 2 (Japan)", GAME_NO_COCKTAIL )
