#include "../vidhrdw/shangha3.cpp"

/***************************************************************************

Shanghai 3           (c)1993 Sunsoft     (68000     AY8910 OKI6295)
Hebereke no Popoon   (c)1994 Sunsoft     (68000 Z80 YM3438 OKI6295)
Blocken              (c)1994 KID / Visco (68000 Z80 YM3438 OKI6295)

These games use the custom blitter GA9201 KA01-0249 (120pin IC)

driver by Nicola Salmoria

TODO:
shangha3:
- The zoom used for the "100" floating score when you remove tiles is very
  rough.
heberpop:
- Unknown writes to sound ports 40/41
blocken:
- incomplete zoom support, and missing rotation support.

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"


extern unsigned char *shangha3_ram;
extern size_t shangha3_ram_size;
extern int shangha3_do_shadows;

WRITE_HANDLER( shangha3_flipscreen_w );
WRITE_HANDLER( shangha3_gfxlist_addr_w );
WRITE_HANDLER( shangha3_blitter_go_w );
int shangha3_vh_start(void);
void shangha3_vh_stop(void);
void shangha3_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



/* this looks like a simple protection check */
/*
write    read
78 78 -> 0
9b 10 -> 1
9b 20 -> 3
9b 40 -> 7
9b 80 -> f
08    -> e
10    -> c
20    -> 8
40    -> 0
*/
static READ_HANDLER( shangha3_prot_r )
{
	static int count;
	static int result[] = { 0x0,0x1,0x3,0x7,0xf,0xe,0xc,0x8,0x0};

//logerror("PC %04x: read 20004e\n",cpu_get_pc());

	return result[count++ % 9];
}
static WRITE_HANDLER( shangha3_prot_w )
{
//logerror("PC %04x: write %02x to 20004e\n",cpu_get_pc(),data);
}


static READ_HANDLER( heberpop_gfxrom_r )
{
	UINT8 *ROM = memory_region(REGION_GFX1);

	return ROM[offset] | (ROM[offset+1] << 8);
}



static WRITE_HANDLER( shangha3_coinctrl_w )
{
	if ((data & 0xff000000) == 0)
	{
		coin_lockout_w(0,~data & 0x0400);
		coin_lockout_w(1,~data & 0x0400);
		coin_counter_w(0,data & 0x0100);
		coin_counter_w(1,data & 0x0200);
	}
}

static WRITE_HANDLER( heberpop_coinctrl_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		/* the sound ROM bank is selected by the main CPU! */
		OKIM6295_set_bank_base(0,ALL_VOICES,(data & 0x08) ? 0x40000 : 0x00000);

		coin_lockout_w(0,~data & 0x04);
		coin_lockout_w(1,~data & 0x04);
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);
	}
}


static WRITE_HANDLER( heberpop_sound_command_w )
{
	soundlatch_w(0,data);
	cpu_cause_interrupt(1,0xff);	/* RST 38h */
}



static struct MemoryReadAddress shangha3_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x100fff, paletteram_word_r },
	{ 0x200000, 0x200001, input_port_0_r },
	{ 0x200002, 0x200003, input_port_1_r },
	{ 0x20001e, 0x20001f, AY8910_read_port_0_r },
	{ 0x20004e, 0x20004f, shangha3_prot_r },
	{ 0x20006e, 0x20006f, OKIM6295_status_0_r },
	{ 0x300000, 0x30ffff, MRA_BANK1 },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress shangha3_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x100fff, paletteram_RRRRRGGGGGBBBBBx_word_w, &paletteram },
	{ 0x200008, 0x200009, shangha3_blitter_go_w },
	{ 0x20000a, 0x20000b, MWA_NOP },	/* irq ack? */
	{ 0x20000c, 0x20000d, shangha3_coinctrl_w },
	{ 0x20002e, 0x20002f, AY8910_write_port_0_w },
	{ 0x20003e, 0x20003f, AY8910_control_port_0_w },
	{ 0x20004e, 0x20004f, shangha3_prot_w },
	{ 0x20006e, 0x20006f, OKIM6295_data_0_w },
	{ 0x300000, 0x30ffff, MWA_BANK1, &shangha3_ram, &shangha3_ram_size },	/* gfx & work ram */
	{ 0x340000, 0x340001, shangha3_flipscreen_w },
	{ 0x360000, 0x360001, shangha3_gfxlist_addr_w },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress heberpop_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },
	{ 0x100000, 0x100fff, paletteram_word_r },
	{ 0x200000, 0x200001, input_port_0_r },
	{ 0x200002, 0x200003, input_port_1_r },
	{ 0x200004, 0x200005, input_port_2_r },
	{ 0x300000, 0x30ffff, MRA_BANK1 },
	{ 0x800000, 0xb7ffff, heberpop_gfxrom_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress heberpop_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },
	{ 0x100000, 0x100fff, paletteram_RRRRRGGGGGBBBBBx_word_w, &paletteram },
	{ 0x200008, 0x200009, shangha3_blitter_go_w },
	{ 0x20000a, 0x20000b, MWA_NOP },	/* irq ack? */
	{ 0x20000c, 0x20000d, heberpop_coinctrl_w },
	{ 0x20000e, 0x20000f, heberpop_sound_command_w },
	{ 0x300000, 0x30ffff, MWA_BANK1, &shangha3_ram, &shangha3_ram_size },	/* gfx & work ram */
	{ 0x340000, 0x340001, shangha3_flipscreen_w },
	{ 0x360000, 0x360001, shangha3_gfxlist_addr_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress blocken_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },
	{ 0x100000, 0x100001, input_port_0_r },
	{ 0x100002, 0x100003, input_port_1_r },
	{ 0x100004, 0x100005, input_port_2_r },
	{ 0x200000, 0x200fff, paletteram_word_r },
	{ 0x300000, 0x30ffff, MRA_BANK1 },
	{ 0x800000, 0xb7ffff, heberpop_gfxrom_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress blocken_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },
	{ 0x100008, 0x100009, shangha3_blitter_go_w },
	{ 0x10000a, 0x10000b, MWA_NOP },	/* irq ack? */
	{ 0x10000c, 0x10000d, heberpop_coinctrl_w },
	{ 0x10000e, 0x10000f, heberpop_sound_command_w },
	{ 0x200000, 0x200fff, paletteram_RRRRRGGGGGBBBBBx_word_w, &paletteram },
	{ 0x300000, 0x30ffff, MWA_BANK1, &shangha3_ram, &shangha3_ram_size },	/* gfx & work ram */
	{ 0x340000, 0x340001, shangha3_flipscreen_w },
	{ 0x360000, 0x360001, shangha3_gfxlist_addr_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress heberpop_sound_readmem[] =
{
	{ 0x0000, 0xf7ff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress heberpop_sound_writemem[] =
{
	{ 0x0000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct IOReadPort heberpop_sound_readport[] =
{
	{ 0x00, 0x00, YM2612_status_port_0_A_r },
	{ 0x80, 0x80, OKIM6295_status_0_r },
	{ 0xc0, 0xc0, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort heberpop_sound_writeport[] =
{
	{ 0x00, 0x00, YM2612_control_port_0_A_w },
	{ 0x01, 0x01, YM2612_data_port_0_A_w },
	{ 0x02, 0x02, YM2612_control_port_0_B_w },
	{ 0x03, 0x03, YM2612_data_port_0_B_w },
	{ 0x80, 0x80, OKIM6295_data_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( shangha3 )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BITX(0x0020, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, "Base Time" )
	PORT_DIPSETTING(    0x04, "70 sec" )
	PORT_DIPSETTING(    0x0c, "80 sec" )
	PORT_DIPSETTING(    0x08, "90 sec" )
	PORT_DIPSETTING(    0x00, "100 sec" )
	PORT_DIPNAME( 0x30, 0x30, "Additional Time" )
	PORT_DIPSETTING(    0x10, "4 sec" )
	PORT_DIPSETTING(    0x30, "5 sec" )
	PORT_DIPSETTING(    0x20, "6 sec" )
	PORT_DIPSETTING(    0x00, "7 sec" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( heberpop )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_VBLANK )	/* vblank?? has to toggle */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_VBLANK )	/* vblank?? has to toggle */

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BITX(0x0020, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0003, 0x0003, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0002, "Very Easy" )
	PORT_DIPSETTING(      0x0001, "Easy" )
	PORT_DIPSETTING(      0x0003, "Normal" )
	PORT_DIPSETTING(      0x0000, "Hard" )
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Allow Diagonal Moves" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0700, 0x0700, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0600, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0700, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0300, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0500, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x3800, 0x3800, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x3800, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x1800, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x2800, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( blocken )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_VBLANK )	/* vblank?? has to toggle */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_VBLANK )	/* vblank?? has to toggle */

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_SERVICE )	/* keeping this pressed on boot generates "BAD DIPSW" */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_SERVICE( 0x0001, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0006, 0x0006, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0004, "Easy" )
	PORT_DIPSETTING(      0x0006, "Normal" )
	PORT_DIPSETTING(      0x0002, "Hard" )
	PORT_DIPSETTING(      0x0000, "Very Hard" )
	PORT_DIPNAME( 0x0008, 0x0008, "Game Type" )
	PORT_DIPSETTING(      0x0008, "A" )
	PORT_DIPSETTING(      0x0000, "B" )
	PORT_DIPNAME( 0x0030, 0x0030, "Players" )
	PORT_DIPSETTING(      0x0030, "1" )
	PORT_DIPSETTING(      0x0020, "2" )
	PORT_DIPSETTING(      0x0010, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0f00, 0x0f00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0500, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x0f00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0300, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x0700, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0e00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0600, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x0d00, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0b00, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0a00, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0900, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf000, 0xf000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0xf000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0xd000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0xb000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x9000, DEF_STR( 1C_7C ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4,
			9*4, 8*4, 11*4, 10*4, 13*4, 12*4, 15*4, 14*4 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 128 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,			/* 1 chip */
	2000000,	/* 2 MHz ??? */
	{ 40 },
	{ input_port_3_r },
	{ input_port_2_r },
	{ 0 },
	{ 0 }
};

static void irqhandler(int linestate)
{
	cpu_set_nmi_line(1,linestate);
}

static struct YM2612interface ym3438_interface =
{
	1,			/* 1 chip */
	8000000,	/* 8 MHz ?? */
	{ 40 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};

static struct OKIM6295interface okim6295_interface =
{
	1,                  /* 1 chip */
	{ 8000 },           /* 8000Hz frequency ??? */
	{ REGION_SOUND1 },	/* memory region */
	{ 40 }
};



static struct MachineDriver machine_driver_shangha3 =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz ??? */
			shangha3_readmem,shangha3_writemem,0,0,
			m68_level4_irq,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	24*16, 16*16, { 0*16, 24*16-1, 1*16, 15*16-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	shangha3_vh_start,
	shangha3_vh_stop,
	shangha3_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_heberpop =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz ??? */
			heberpop_readmem,heberpop_writemem,0,0,
			m68_level4_irq,1
		},
		{
			CPU_Z80,
			6000000,	/* 6 MHz ??? */
			heberpop_sound_readmem,heberpop_sound_writemem,heberpop_sound_readport,heberpop_sound_writeport,
			ignore_interrupt,0	/* IRQ triggered by main CPU */
								/* NMI triggered by YM3438 */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	24*16, 16*16, { 0*16, 24*16-1, 1*16, 15*16-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	shangha3_vh_start,
	shangha3_vh_stop,
	shangha3_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3438,
			&ym3438_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_blocken =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			16000000,	/* 16 MHz ??? */
			blocken_readmem,blocken_writemem,0,0,
			m68_level4_irq,1
		},
		{
			CPU_Z80,
			6000000,	/* 6 MHz ??? */
			heberpop_sound_readmem,heberpop_sound_writemem,heberpop_sound_readport,heberpop_sound_writeport,
			ignore_interrupt,0	/* IRQ triggered by main CPU */
								/* NMI triggered by YM3438 */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	24*16, 16*16, { 0*16, 24*16-1, 1*16, 15*16-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	shangha3_vh_start,
	shangha3_vh_stop,
	shangha3_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3438,
			&ym3438_interface
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

ROM_START( shangha3 )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "s3j_ic3.v11",  0x0000, 0x40000, 0xe98ce9c8 )
	ROM_LOAD_ODD ( "s3j_ic2.v11",  0x0000, 0x40000, 0x09174620 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "s3j_ic43.chr", 0x0000, 0x200000, 0x2dbf9d17 )

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* samples for M6295 */
	ROM_LOAD( "s3j_ic75.v10", 0x0000, 0x40000, 0xf0cdc86a )
ROM_END

ROM_START( heberpop )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "hbpic31.bin",  0x0000, 0x80000, 0xc430d264 )
	ROM_LOAD_ODD ( "hbpic32.bin",  0x0000, 0x80000, 0xbfa555a8 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "hbpic34.bin",  0x0000, 0x10000, 0x0cf056c6 )

	ROM_REGION( 0x380000, REGION_GFX1 )	/* don't dispose, read during tests */
	ROM_LOAD( "hbpic98.bin",  0x000000, 0x80000, 0xa599100a )
	ROM_LOAD( "hbpic99.bin",  0x080000, 0x80000, 0xfb8bb12f )
	ROM_LOAD( "hbpic100.bin", 0x100000, 0x80000, 0x05a0f765 )
	ROM_LOAD( "hbpic101.bin", 0x180000, 0x80000, 0x151ba025 )
	ROM_LOAD( "hbpic102.bin", 0x200000, 0x80000, 0x2b5e341a )
	ROM_LOAD( "hbpic103.bin", 0x280000, 0x80000, 0xefa0e745 )
	ROM_LOAD( "hbpic104.bin", 0x300000, 0x80000, 0xbb896bbb )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples for M6295 */
	ROM_LOAD( "hbpic53.bin",  0x0000, 0x80000, 0xa4483aa0 )
ROM_END

ROM_START( blocken )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "ic31j.bin",    0x0000, 0x20000, 0xec8de2a3 )
	ROM_LOAD_ODD ( "ic32j.bin",    0x0000, 0x20000, 0x79b96240 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "ic34.bin",     0x0000, 0x10000, 0x23e446ff )

	ROM_REGION( 0x380000, REGION_GFX1 )	/* don't dispose, read during tests */
	ROM_LOAD( "ic98j.bin",    0x000000, 0x80000, 0x35dda273 )
	ROM_LOAD( "ic99j.bin",    0x080000, 0x80000, 0xce43762b )
	/* 100000-1fffff empty */
	ROM_LOAD( "ic100j.bin",   0x200000, 0x80000, 0xa34786fd )
	/* 280000-37ffff empty */

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples for M6295 */
	ROM_LOAD( "ic53.bin",     0x0000, 0x80000, 0x86108c56 )
ROM_END



static void init_shangha3(void)
{
	shangha3_do_shadows = 1;
}
static void init_heberpop(void)
{
	shangha3_do_shadows = 0;
}

GAME( 1993, shangha3, 0, shangha3, shangha3, shangha3, ROT0_16BIT, "Sunsoft", "Shanghai III (Japan)" )
GAME( 1994, heberpop, 0, heberpop, heberpop, heberpop, ROT0_16BIT, "Sunsoft / Atlus", "Hebereke no Popoon (Japan)" )
GAME( 1994, blocken,  0, blocken,  blocken,  heberpop, ROT0_16BIT, "KID / Visco", "Blocken (Japan)" )
