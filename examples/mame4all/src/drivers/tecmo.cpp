#include "../vidhrdw/tecmo.cpp"

/***************************************************************************

tecmo.c

driver by Nicola Salmoria


Silkworm memory map (preliminary)

0000-bfff ROM
c000-c1ff Background video RAM #2
c200-c3ff Background color RAM #2
c400-c5ff Background video RAM #1
c600-c7ff Background color RAM #1
c800-cbff Video RAM
cc00-cfff Color RAM
d000-dfff RAM
e000-e7ff Sprites
e800-efff Palette RAM, groups of 2 bytes, 4 bits per gun: xB RG
          e800-e9ff sprites
          ea00-ebff characters
          ec00-edff bg #1
          ee00-efff bg #2
f000-f7ff window for banked ROM

read:
f800      IN0 (heli) bit 0-3
f801      IN0 bit 4-7
f802      IN1 (jeep) bit 0-3
f803      IN1 bit 4-7
f806      DSWA bit 0-3
f807      DSWA bit 4-7
f808      DSWB bit 0-3
f809      DSWB bit 4-7
f80f      COIN

write:
f800-f801 bg #1 x scroll
f802      bg #1 y scroll
f803-f804 bg #2 x scroll
f805      bg #2 y scroll
f806      ????
f808      ROM bank selector
f809      ????
f80b      ????

***************************************************************************/
#include "driver.h"
#include "cpu/z80/z80.h"



extern int tecmo_video_type;
extern unsigned char *tecmo_txvideoram,*tecmo_fgvideoram,*tecmo_bgvideoram;
extern unsigned char *spriteram;
extern size_t spriteram_size;

WRITE_HANDLER( tecmo_txvideoram_w );
WRITE_HANDLER( tecmo_fgvideoram_w );
WRITE_HANDLER( tecmo_bgvideoram_w );
WRITE_HANDLER( tecmo_fgscroll_w );
WRITE_HANDLER( tecmo_bgscroll_w );
WRITE_HANDLER( tecmo_flipscreen_w );

int tecmo_vh_start(void);
void tecmo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



WRITE_HANDLER( tecmo_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankaddress = 0x10000 + ((data & 0xf8) << 8);
	cpu_setbank(1,&RAM[bankaddress]);
}

static WRITE_HANDLER( tecmo_sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,Z80_NMI_INT);
}

static int adpcm_pos,adpcm_end;

static WRITE_HANDLER( tecmo_adpcm_start_w )
{
	adpcm_pos = data << 8;
	MSM5205_reset_w(0,0);
}
static WRITE_HANDLER( tecmo_adpcm_end_w )
{
	adpcm_end = (data + 1) << 8;
}
static WRITE_HANDLER( tecmo_adpcm_vol_w )
{
	MSM5205_set_volume(0,(data & 0x0f) * 100 / 15);
}
static void tecmo_adpcm_int(int num)
{
	static int adpcm_data = -1;

	if (adpcm_pos >= adpcm_end ||
				adpcm_pos >= memory_region_length(REGION_SOUND1))
		MSM5205_reset_w(0,1);
	else if (adpcm_data != -1)
	{
		MSM5205_data_w(0,adpcm_data & 0x0f);
		adpcm_data = -1;
	}
	else
	{
		unsigned char *ROM = memory_region(REGION_SOUND1);

		adpcm_data = ROM[adpcm_pos++];
		MSM5205_data_w(0,adpcm_data >> 4);
	}
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xefff, MRA_RAM },
	{ 0xf000, 0xf7ff, MRA_BANK1 },
	{ 0xf800, 0xf800, input_port_0_r },
	{ 0xf801, 0xf801, input_port_1_r },
	{ 0xf802, 0xf802, input_port_2_r },
	{ 0xf803, 0xf803, input_port_3_r },
	{ 0xf804, 0xf804, input_port_4_r },
	{ 0xf805, 0xf805, input_port_5_r },
	{ 0xf806, 0xf806, input_port_6_r },
	{ 0xf807, 0xf807, input_port_7_r },
	{ 0xf808, 0xf808, input_port_8_r },
	{ 0xf809, 0xf809, input_port_9_r },
	{ 0xf80f, 0xf80f, input_port_10_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress rygar_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd7ff, tecmo_txvideoram_w, &tecmo_txvideoram },
	{ 0xd800, 0xdbff, tecmo_fgvideoram_w, &tecmo_fgvideoram },
	{ 0xdc00, 0xdfff, tecmo_bgvideoram_w, &tecmo_bgvideoram },
	{ 0xe000, 0xe7ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe800, 0xefff, paletteram_xxxxBBBBRRRRGGGG_swap_w, &paletteram },
	{ 0xf000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xf802, tecmo_fgscroll_w },
	{ 0xf803, 0xf805, tecmo_bgscroll_w },
	{ 0xf806, 0xf806, tecmo_sound_command_w },
	{ 0xf807, 0xf807, tecmo_flipscreen_w },
	{ 0xf808, 0xf808, tecmo_bankswitch_w },
	{ 0xf80b, 0xf80b, watchdog_reset_w },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress gemini_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd7ff, tecmo_txvideoram_w, &tecmo_txvideoram },
	{ 0xd800, 0xdbff, tecmo_fgvideoram_w, &tecmo_fgvideoram },
	{ 0xdc00, 0xdfff, tecmo_bgvideoram_w, &tecmo_bgvideoram },
	{ 0xe000, 0xe7ff, paletteram_xxxxBBBBRRRRGGGG_swap_w, &paletteram },
	{ 0xe800, 0xefff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xf802, tecmo_fgscroll_w },
	{ 0xf803, 0xf805, tecmo_bgscroll_w },
	{ 0xf806, 0xf806, tecmo_sound_command_w },
	{ 0xf807, 0xf807, tecmo_flipscreen_w },
	{ 0xf808, 0xf808, tecmo_bankswitch_w },
	{ 0xf80b, 0xf80b, watchdog_reset_w },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress silkworm_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc3ff, tecmo_bgvideoram_w, &tecmo_bgvideoram },
	{ 0xc400, 0xc7ff, tecmo_fgvideoram_w, &tecmo_fgvideoram },
	{ 0xc800, 0xcfff, tecmo_txvideoram_w, &tecmo_txvideoram },
	{ 0xd000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe7ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe800, 0xefff, paletteram_xxxxBBBBRRRRGGGG_swap_w, &paletteram },
	{ 0xf000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xf802, tecmo_fgscroll_w },
	{ 0xf803, 0xf805, tecmo_bgscroll_w },
	{ 0xf806, 0xf806, tecmo_sound_command_w },
	{ 0xf807, 0xf807, tecmo_flipscreen_w },
	{ 0xf808, 0xf808, tecmo_bankswitch_w },
	{ 0xf809, 0xf809, MWA_NOP },	/* ? */
	{ 0xf80b, 0xf80b, MWA_NOP },	/* ? if mapped to watchdog like in the others, causes reset */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress rygar_sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0xc000, 0xc000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress rygar_sound_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x47ff, MWA_RAM },
	{ 0x8000, 0x8000, YM3812_control_port_0_w },
	{ 0x8001, 0x8001, YM3812_write_port_0_w },
	{ 0xc000, 0xc000, tecmo_adpcm_start_w },
	{ 0xd000, 0xd000, tecmo_adpcm_end_w },
	{ 0xe000, 0xe000, tecmo_adpcm_vol_w },
	{ 0xf000, 0xf000, MWA_NOP },	/* NMI acknowledge */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress tecmo_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xc000, 0xc000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress tecmo_sound_writemem[] =
{
	{ 0x2000, 0x207f, MWA_RAM },	/* Silkworm set #2 has a custom CPU which */
									/* writes code to this area */
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xa000, 0xa000, YM3812_control_port_0_w },
	{ 0xa001, 0xa001, YM3812_write_port_0_w },
	{ 0xc000, 0xc000, tecmo_adpcm_start_w },
	{ 0xc400, 0xc400, tecmo_adpcm_end_w },
	{ 0xc800, 0xc800, tecmo_adpcm_vol_w },
	{ 0xcc00, 0xcc00, MWA_NOP },	/* NMI acknowledge */
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( rygar )
	PORT_START	/* IN0 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )

	PORT_START	/* IN1 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* IN3 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN4 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START	/* unused? */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSWA bit 0-3 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0C, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0C, DEF_STR( 1C_3C ) )

	PORT_START	/* DSWA bit 4-7 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )

	PORT_START	/* DSWB bit 0-3 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 200000 500000" )
	PORT_DIPSETTING(    0x01, "100000 300000 600000" )
	PORT_DIPSETTING(    0x02, "200000 500000" )
	PORT_DIPSETTING(    0x03, "100000" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )

	PORT_START	/* DSWB bit 4-7 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x01, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x03, "Hardest" )
	PORT_DIPNAME( 0x04, 0x00, "2P Can Start Anytime" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x08, 0x08, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Yes ) )

	PORT_START	/* unused? */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( gemini )
	PORT_START	/* IN0 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )

	PORT_START	/* IN1 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* IN3 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* unused? */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN4 bits 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN2 )

	PORT_START	/* DSWA bit 0-3 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x08, 0x00, "Final Round Continuation" )
	PORT_DIPSETTING(    0x00, "Round 6" )
	PORT_DIPSETTING(    0x08, "Round 7" )

	PORT_START	/* DSWA bit 4-7 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x08, 0x00, "Buy in During Final Round" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Yes ) )

	PORT_START	/* DSWB bit 0-3 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x0c, "Hardest" )

	PORT_START	/* DSWB bit 4-7 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 200000" )
	PORT_DIPSETTING(    0x01, "50000 300000" )
	PORT_DIPSETTING(    0x02, "100000 500000" )
	PORT_DIPSETTING(    0x03, "50000" )
	PORT_DIPSETTING(    0x04, "100000" )
	PORT_DIPSETTING(    0x05, "200000" )
	PORT_DIPSETTING(    0x06, "300000" )
	PORT_DIPSETTING(    0x07, "None" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* unused? */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( silkworm )
	PORT_START	/* IN0 bit 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )

	PORT_START	/* IN0 bit 4-7 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* unused? */

	PORT_START	/* IN1 bit 0-3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* IN1 bit 4-7 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* unused? */

	PORT_START	/* unused? */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* unused? */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSWA bit 0-3 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0C, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0C, DEF_STR( 1C_3C ) )

	PORT_START	/* DSWA bit 4-7 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )	/* unused? */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )

	PORT_START	/* DSWB bit 0-3 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 200000 500000" )
	PORT_DIPSETTING(    0x01, "100000 300000 800000" )
	PORT_DIPSETTING(    0x02, "50000 200000" )
	PORT_DIPSETTING(    0x03, "100000 300000" )
	PORT_DIPSETTING(    0x04, "50000" )
	PORT_DIPSETTING(    0x05, "100000" )
	PORT_DIPSETTING(    0x06, "200000" )
	PORT_DIPSETTING(    0x07, "None" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )	/* unused? */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )

	PORT_START	/* DSWB bit 4-7 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x05, "5" )
	/* 0x06 and 0x07 are the same as 0x00 */
	PORT_DIPNAME( 0x08, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )

	PORT_START	/* COIN */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN2 )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout tilelayout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
			32*8+0*4, 32*8+1*4, 32*8+2*4, 32*8+3*4, 32*8+4*4, 32*8+5*4, 32*8+6*4, 32*8+7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
	128*8
};

static struct GfxLayout spritelayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 256, 16 },	/* colors 256 - 511 */
	{ REGION_GFX2, 0, &spritelayout, 0, 16 },	/* colors   0 - 255 */
	{ REGION_GFX3, 0, &tilelayout, 512, 16 },	/* colors 512 - 767 */
	{ REGION_GFX4, 0, &tilelayout, 768, 16 },	/* colors 768 - 1023 */
	{ -1 } /* end of array */
};



static void irqhandler(int linestate)
{
	cpu_set_irq_line(1,0,linestate);
}

static struct YM3526interface ym3812_interface =
{
	1,			/* 1 chip */
	4000000,	/* 4 MHz */
	{ 80 },		/* volume */
	{ irqhandler }
};

static struct MSM5205interface msm5205_interface =
{
	1,					/* 1 chip             */
	400000,				/* 400KHz             */
	{ tecmo_adpcm_int },/* interrupt function */
	{ MSM5205_S48_4B },	/* 8KHz               */
	{ 80 }				/* volume */
};



#define MACHINE_DRIVER(NAME,CPU1_CLOCK,SOUND)					\
static struct MachineDriver machine_driver_##NAME =				\
{																\
	/* basic machine hardware */								\
	{															\
		{														\
			CPU_Z80,											\
			CPU1_CLOCK,											\
			readmem,NAME##_writemem,0,0,						\
			interrupt,1											\
		},														\
		{														\
			CPU_Z80 | CPU_AUDIO_CPU,							\
			4000000,	/* 4 MHz */								\
			SOUND##_sound_readmem,SOUND##_sound_writemem,0,0,	\
			ignore_interrupt,0	/* IRQs triggered by YM3526 */	\
								/* NMIs triggered by main CPU */\
		}														\
	},															\
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */	\
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */	\
	0,															\
																\
	/* video hardware */										\
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },					\
	gfxdecodeinfo,												\
	1024, 1024,													\
	0,															\
																\
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,					\
	0,															\
	tecmo_vh_start,												\
	0,															\
	tecmo_vh_screenrefresh,										\
																\
	/* sound hardware */										\
	0,0,0,0,													\
	{															\
		{														\
			SOUND_YM3812,										\
			&ym3812_interface									\
		},														\
		{														\
			SOUND_MSM5205,										\
			&msm5205_interface									\
		},														\
	}															\
};

MACHINE_DRIVER( rygar,    6000000,         rygar )
MACHINE_DRIVER( gemini,   6000000 /* ? */, tecmo )
MACHINE_DRIVER( silkworm, 6000000 /* ? */, tecmo )



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( rygar )
	ROM_REGION( 0x18000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "5.5p",         0x00000, 0x08000, 0x062cd55d ) /* code */
	ROM_LOAD( "cpu_5m.bin",   0x08000, 0x04000, 0x7ac5191b ) /* code */
	ROM_LOAD( "cpu_5j.bin",   0x10000, 0x08000, 0xed76d606 ) /* banked at f000-f7ff */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "cpu_4h.bin",   0x0000, 0x2000, 0xe4a2fa87 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu_8k.bin",   0x00000, 0x08000, 0x4d482fb6 )	/* characters */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6k.bin",   0x00000, 0x08000, 0xaba6db9e )	/* sprites */
	ROM_LOAD( "vid_6j.bin",   0x08000, 0x08000, 0xae1f2ed6 )	/* sprites */
	ROM_LOAD( "vid_6h.bin",   0x10000, 0x08000, 0x46d9e7df )	/* sprites */
	ROM_LOAD( "vid_6g.bin",   0x18000, 0x08000, 0x45839c9a )	/* sprites */

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6p.bin",   0x00000, 0x08000, 0x9eae5f8e )
	ROM_LOAD( "vid_6o.bin",   0x08000, 0x08000, 0x5a10a396 )
	ROM_LOAD( "vid_6n.bin",   0x10000, 0x08000, 0x7b12cf3f )
	ROM_LOAD( "vid_6l.bin",   0x18000, 0x08000, 0x3cea7eaa )

	ROM_REGION( 0x20000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6f.bin",   0x00000, 0x08000, 0x9840edd8 )
	ROM_LOAD( "vid_6e.bin",   0x08000, 0x08000, 0xff65e074 )
	ROM_LOAD( "vid_6c.bin",   0x10000, 0x08000, 0x89868c85 )
	ROM_LOAD( "vid_6b.bin",   0x18000, 0x08000, 0x35389a7b )

	ROM_REGION( 0x4000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "cpu_1f.bin",   0x0000, 0x4000, 0x3cc98c5a )
ROM_END

ROM_START( rygar2 )
	ROM_REGION( 0x18000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "cpu_5p.bin",   0x00000, 0x08000, 0xe79c054a ) /* code */
	ROM_LOAD( "cpu_5m.bin",   0x08000, 0x04000, 0x7ac5191b ) /* code */
	ROM_LOAD( "cpu_5j.bin",   0x10000, 0x08000, 0xed76d606 ) /* banked at f000-f7ff */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "cpu_4h.bin",   0x0000, 0x2000, 0xe4a2fa87 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu_8k.bin",   0x00000, 0x08000, 0x4d482fb6 )	/* characters */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6k.bin",   0x00000, 0x08000, 0xaba6db9e )	/* sprites */
	ROM_LOAD( "vid_6j.bin",   0x08000, 0x08000, 0xae1f2ed6 )	/* sprites */
	ROM_LOAD( "vid_6h.bin",   0x10000, 0x08000, 0x46d9e7df )	/* sprites */
	ROM_LOAD( "vid_6g.bin",   0x18000, 0x08000, 0x45839c9a )	/* sprites */

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6p.bin",   0x00000, 0x08000, 0x9eae5f8e )
	ROM_LOAD( "vid_6o.bin",   0x08000, 0x08000, 0x5a10a396 )
	ROM_LOAD( "vid_6n.bin",   0x10000, 0x08000, 0x7b12cf3f )
	ROM_LOAD( "vid_6l.bin",   0x18000, 0x08000, 0x3cea7eaa )

	ROM_REGION( 0x20000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6f.bin",   0x00000, 0x08000, 0x9840edd8 )
	ROM_LOAD( "vid_6e.bin",   0x08000, 0x08000, 0xff65e074 )
	ROM_LOAD( "vid_6c.bin",   0x10000, 0x08000, 0x89868c85 )
	ROM_LOAD( "vid_6b.bin",   0x18000, 0x08000, 0x35389a7b )

	ROM_REGION( 0x4000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "cpu_1f.bin",   0x0000, 0x4000, 0x3cc98c5a )
ROM_END

ROM_START( rygarj )
	ROM_REGION( 0x18000, REGION_CPU1 )	/* 64k for code */

	ROM_LOAD( "cpuj_5p.bin",  0x00000, 0x08000, 0xb39698ba ) /* code */
	ROM_LOAD( "cpuj_5m.bin",  0x08000, 0x04000, 0x3f180979 ) /* code */
	ROM_LOAD( "cpuj_5j.bin",  0x10000, 0x08000, 0x69e44e8f ) /* banked at f000-f7ff */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "cpu_4h.bin",   0x0000, 0x2000, 0xe4a2fa87 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpuj_8k.bin",  0x00000, 0x08000, 0x45047707 )	/* characters */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6k.bin",   0x00000, 0x08000, 0xaba6db9e )	/* sprites */
	ROM_LOAD( "vid_6j.bin",   0x08000, 0x08000, 0xae1f2ed6 )	/* sprites */
	ROM_LOAD( "vid_6h.bin",   0x10000, 0x08000, 0x46d9e7df )	/* sprites */
	ROM_LOAD( "vid_6g.bin",   0x18000, 0x08000, 0x45839c9a )	/* sprites */

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6p.bin",   0x00000, 0x08000, 0x9eae5f8e )
	ROM_LOAD( "vid_6o.bin",   0x08000, 0x08000, 0x5a10a396 )
	ROM_LOAD( "vid_6n.bin",   0x10000, 0x08000, 0x7b12cf3f )
	ROM_LOAD( "vid_6l.bin",   0x18000, 0x08000, 0x3cea7eaa )

	ROM_REGION( 0x20000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_6f.bin",   0x00000, 0x08000, 0x9840edd8 )
	ROM_LOAD( "vid_6e.bin",   0x08000, 0x08000, 0xff65e074 )
	ROM_LOAD( "vid_6c.bin",   0x10000, 0x08000, 0x89868c85 )
	ROM_LOAD( "vid_6b.bin",   0x18000, 0x08000, 0x35389a7b )

	ROM_REGION( 0x4000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "cpu_1f.bin",   0x0000, 0x4000, 0x3cc98c5a )
ROM_END

ROM_START( silkworm )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "silkworm.4",   0x00000, 0x10000, 0xa5277cce )	/* c000-ffff is not used */
	ROM_LOAD( "silkworm.5",   0x10000, 0x10000, 0xa6c7bb51 )	/* banked at f000-f7ff */

	ROM_REGION( 0x20000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "silkworm.3",   0x0000, 0x8000, 0xb589f587 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.2",   0x00000, 0x08000, 0xe80a1cd9 )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.6",   0x00000, 0x10000, 0x1138d159 )	/* sprites */
	ROM_LOAD( "silkworm.7",   0x10000, 0x10000, 0xd96214f7 )	/* sprites */
	ROM_LOAD( "silkworm.8",   0x20000, 0x10000, 0x0494b38e )	/* sprites */
	ROM_LOAD( "silkworm.9",   0x30000, 0x10000, 0x8ce3cdf5 )	/* sprites */

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.10",  0x00000, 0x10000, 0x8c7138bb )	/* tiles #1 */
	ROM_LOAD( "silkworm.11",  0x10000, 0x10000, 0x6c03c476 )	/* tiles #1 */
	ROM_LOAD( "silkworm.12",  0x20000, 0x10000, 0xbb0f568f )	/* tiles #1 */
	ROM_LOAD( "silkworm.13",  0x30000, 0x10000, 0x773ad0a4 )	/* tiles #1 */

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.14",  0x00000, 0x10000, 0x409df64b )	/* tiles #2 */
	ROM_LOAD( "silkworm.15",  0x10000, 0x10000, 0x6e4052c9 )	/* tiles #2 */
	ROM_LOAD( "silkworm.16",  0x20000, 0x10000, 0x9292ed63 )	/* tiles #2 */
	ROM_LOAD( "silkworm.17",  0x30000, 0x10000, 0x3fa4563d )	/* tiles #2 */

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "silkworm.1",   0x0000, 0x8000, 0x5b553644 )
ROM_END

ROM_START( silkwrm2 )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "r4",           0x00000, 0x10000, 0x6df3df22 )	/* c000-ffff is not used */
	ROM_LOAD( "silkworm.5",   0x10000, 0x10000, 0xa6c7bb51 )	/* banked at f000-f7ff */

	ROM_REGION( 0x20000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "r3",           0x0000, 0x8000, 0xb79848d0 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.2",   0x00000, 0x08000, 0xe80a1cd9 )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.6",   0x00000, 0x10000, 0x1138d159 )	/* sprites */
	ROM_LOAD( "silkworm.7",   0x10000, 0x10000, 0xd96214f7 )	/* sprites */
	ROM_LOAD( "silkworm.8",   0x20000, 0x10000, 0x0494b38e )	/* sprites */
	ROM_LOAD( "silkworm.9",   0x30000, 0x10000, 0x8ce3cdf5 )	/* sprites */

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.10",  0x00000, 0x10000, 0x8c7138bb )	/* tiles #1 */
	ROM_LOAD( "silkworm.11",  0x10000, 0x10000, 0x6c03c476 )	/* tiles #1 */
	ROM_LOAD( "silkworm.12",  0x20000, 0x10000, 0xbb0f568f )	/* tiles #1 */
	ROM_LOAD( "silkworm.13",  0x30000, 0x10000, 0x773ad0a4 )	/* tiles #1 */

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "silkworm.14",  0x00000, 0x10000, 0x409df64b )	/* tiles #2 */
	ROM_LOAD( "silkworm.15",  0x10000, 0x10000, 0x6e4052c9 )	/* tiles #2 */
	ROM_LOAD( "silkworm.16",  0x20000, 0x10000, 0x9292ed63 )	/* tiles #2 */
	ROM_LOAD( "silkworm.17",  0x30000, 0x10000, 0x3fa4563d )	/* tiles #2 */

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "silkworm.1",   0x0000, 0x8000, 0x5b553644 )
ROM_END

ROM_START( gemini )
	ROM_REGION( 0x20000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "gw04-5s.rom",  0x00000, 0x10000, 0xff9de855 )	/* c000-ffff is not used */
	ROM_LOAD( "gw05-6s.rom",  0x10000, 0x10000, 0x5a6947a9 )	/* banked at f000-f7ff */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "gw03-5h.rom",  0x0000, 0x8000, 0x9bc79596 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gw02-3h.rom",  0x00000, 0x08000, 0x7acc8d35 )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gw06-1c.rom",  0x00000, 0x10000, 0x4ea51631 )	/* sprites */
	ROM_LOAD( "gw07-1d.rom",  0x10000, 0x10000, 0xda42637e )	/* sprites */
	ROM_LOAD( "gw08-1f.rom",  0x20000, 0x10000, 0x0b4e8d70 )	/* sprites */
	ROM_LOAD( "gw09-1h.rom",  0x30000, 0x10000, 0xb65c5e4c )	/* sprites */

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gw10-1n.rom",  0x00000, 0x10000, 0x5e84cd4f )	/* tiles #1 */
	ROM_LOAD( "gw11-2na.rom", 0x10000, 0x10000, 0x08b458e1 )	/* tiles #1 */
	ROM_LOAD( "gw12-2nb.rom", 0x20000, 0x10000, 0x229c9714 )	/* tiles #1 */
	ROM_LOAD( "gw13-3n.rom",  0x30000, 0x10000, 0xc5dfaf47 )	/* tiles #1 */

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gw14-1r.rom",  0x00000, 0x10000, 0x9c10e5b5 )	/* tiles #2 */
	ROM_LOAD( "gw15-2ra.rom", 0x10000, 0x10000, 0x4cd18cfa )	/* tiles #2 */
	ROM_LOAD( "gw16-2rb.rom", 0x20000, 0x10000, 0xf911c7be )	/* tiles #2 */
	ROM_LOAD( "gw17-3r.rom",  0x30000, 0x10000, 0x79a9ce25 )	/* tiles #2 */

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "gw01-6a.rom",  0x0000, 0x8000, 0xd78afa05 )
ROM_END



/*
   video_type is used to distinguish Rygar, Silkworm and Gemini Wing.
   This is needed because there is a difference in the tile and sprite indexing.
*/
void init_rygar(void)    { tecmo_video_type = 0; }
void init_silkworm(void) { tecmo_video_type = 1; }
void init_gemini(void)   { tecmo_video_type = 2; }



GAME( 1986, rygar,    0,        rygar,    rygar,    rygar,    ROT0,  "Tecmo", "Rygar (US set 1)" )
GAME( 1986, rygar2,   rygar,    rygar,    rygar,    rygar,    ROT0,  "Tecmo", "Rygar (US set 2)" )
GAME( 1986, rygarj,   rygar,    rygar,    rygar,    rygar,    ROT0,  "Tecmo", "Argus no Senshi (Japan)" )
GAME( 1987, gemini,   0,        gemini,   gemini,   gemini,   ROT90, "Tecmo", "Gemini Wing" )
GAME( 1988, silkworm, 0,        silkworm, silkworm, silkworm, ROT0,  "Tecmo", "Silkworm (set 1)" )
GAME( 1988, silkwrm2, silkworm, silkworm, silkworm, silkworm, ROT0,  "Tecmo", "Silkworm (set 2)" )
