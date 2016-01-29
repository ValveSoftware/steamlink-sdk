#include "../vidhrdw/rpunch.cpp"

/***************************************************************************

	Rabbit Punch / Rabio Lepus

    driver by Aaron Giles

****************************************************************************

	Memory map

****************************************************************************

	========================================================================
	MAIN CPU
	========================================================================
	000000-03FFFF   R     xxxxxxxx xxxxxxxx   Program ROM
	040000-04FFFF   R/W   xxxxxxxx xxxxxxxx   Bitmap RAM (512x256 pixels)
	                R/W   xxxx---- --------      (leftmost pixel)
	                R/W   ----xxxx --------      (2nd pixel)
	                R/W   -------- xxxx----      (3rd pixel)
	                R/W   -------- ----xxxx      (rightmost pixel)
	060000-060FFF   R/W   xxxxxxxx xxxxxxxx   Sprite RAM (512 entries x 4 words)
	                R/W   -------x xxxxxxxx      (0: horizontal position)
	                R/W   xxx----- --------      (1: color index)
	                R/W   ---x---- --------      (1: horizontal flip)
	                R/W   -----xxx xxxxxxxx      (1: image number)
	                R/W   -------x xxxxxxxx      (2: Y position)
	                R/W   -------- --------      (3: not used)
	080000-081FFF   R/W   xxxxxxxx xxxxxxxx   Background 1 RAM (64x64 tiles)
	                R/W   xxx----- --------      (color index)
	                R/W   ---xxxxx xxxxxxxx      (image number)
	082000-083FFF   R/W   xxxxxxxx xxxxxxxx   Background 2 RAM (64x64 tiles)
	                R/W   xxx----- --------      (color index)
	                R/W   ---xxxxx xxxxxxxx      (image number)
	0A0000-0A01FF   R/W   -xxxxxxx xxxxxxxx   Background 1 palette RAM (256 entries)
	                R/W   -xxxxx-- --------      (red component)
	                R/W   ------xx xxx-----      (green component)
	                R/W   -------- ---xxxxx      (blue component)
	0A0200-0A03FF   R/W   -xxxxxxx xxxxxxxx   Background 2 palette RAM (256 entries)
	0A0400-0A05FF   R/W   -xxxxxxx xxxxxxxx   Bitmap palette RAM (256 entries)
	0A0600-0A07FF   R/W   -xxxxxxx xxxxxxxx   Sprite palette RAM (256 entries)
	0C0000            W   -------x xxxxxxxx   Background 1 vertical scroll
	0C0002            W   -------x xxxxxxxx   Background 1 horizontal scroll
	0C0004            W   -------x xxxxxxxx   Background 2 vertical scroll
	0C0006            W   -------x xxxxxxxx   Background 2 horizontal scroll
	0C0008            W   -------- ????????   Video controller data (CRTC)
	0C000C            W   ---xxx-- -xxxxxxx   Video flags
	                  W   ---x---- --------      (flip screen)
	                  W   ----x--- --------      (background 2 image bank)
	                  W   -----x-- --------      (background 1 image bank)
	                  W   -------- -x------      (sprite palette bank)
	                  W   -------- --x-----      (background 2 palette bank)
	                  W   -------- ---x----      (background 1 palette bank)
	                  W   -------- ----xxxx      (bitmap palette bank)
	0C000E            W   -------- xxxxxxxx   Sound communications
	0C0010            W   -------- --xxxxxx   Sprite bias (???)
	0C0012            W   -------- --xxxxxx   Bitmap bias (???)
	0C0018          R     -xxxx-xx --xxxxxx   Player 1 input port
	                R     -x------ --------      (2 player start)
	                R     --x----- --------      (1 player start)
	                R     ---x---- --------      (coin 1)
	                R     ----x--- --------      (coin 2)
	                R     ------x- --------      (test switch)
	                R     -------x --------      (service coin)
	                R     -------- --x-----      (punch button)
	                R     -------- ---x----      (missile button)
	                R     -------- ----xxxx      (joystick right/left/down/up)
	0C001A          R     -xxxx-xx --xxxxxx   Player 2 input port
	0C001C          R     xxxxxxxx xxxxxxxx   DIP switches
	                R     x------- --------      (flip screen)
	                R     -x------ --------      (continues allowed)
	                R     --x----- --------      (demo sounds)
	                R     ---x---- --------      (extended play)
	                R     ----x--- --------      (laser control)
	                R     -----x-- --------      (number of lives)
	                R     ------xx --------      (difficulty)
	                R     -------- xxxx----      (coinage 2)
	                R     -------- ----xxxx      (coinage 1)
	0C001E          R     -------- -------x   Sound busy flag
	0C0028            W   -------- ????????   Video controller register select (CRTC)
	0FC000-0FFFFF   R/W   xxxxxxxx xxxxxxxx   Program RAM
	========================================================================
	Interrupts:
		IRQ1 = VBLANK
	========================================================================


	========================================================================
	SOUND CPU
	========================================================================
	0000-EFFF   R     xxxxxxxx   Program ROM
	F000-F001   R/W   xxxxxxxx   YM2151 communications
	F200        R     xxxxxxxx   Sound command input
	F400          W   x------x   UPD7759 control
	              W   x-------      (/RESET line)
	              W   -------x      (ROM bank select)
	F600          W   xxxxxxxx   UPD7759 data/trigger
	F800-FFFF   R/W   xxxxxxxx   Program RAM
	========================================================================
	Interrupts:
		IRQ = YM2151 IRQ or'ed with the sound command latch flag
	========================================================================

***************************************************************************/


#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "vidhrdw/generic.h"
#include <math.h>



#define MASTER_CLOCK		16000000
#define UPD_CLOCK			640000


/* video driver data & functions */
int rpunch_vh_start(void);
void rpunch_vh_stop(void);
void rpunch_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);

extern UINT8 *rpunch_bitmapram;
extern size_t rpunch_bitmapram_size;
extern int rpunch_sprite_palette;

static UINT8 sound_data;
static UINT8 sound_busy;
static UINT8 ym2151_irq;
static UINT8 upd_rom_bank;

WRITE_HANDLER(rpunch_bitmap_w);
WRITE_HANDLER(rpunch_videoram_w);
WRITE_HANDLER(rpunch_videoreg_w);
WRITE_HANDLER(rpunch_scrollreg_w);
WRITE_HANDLER(rpunch_ins_w);
WRITE_HANDLER(rpunch_crtc_data_w);
WRITE_HANDLER(rpunch_crtc_register_w);



/*************************************
 *
 *	Interrupt handling
 *
 *************************************/

static void ym2151_irq_gen(int state)
{
	ym2151_irq = state;
	cpu_set_irq_line(1, 0, (ym2151_irq | sound_busy) ? ASSERT_LINE : CLEAR_LINE);
}


static void init_machine(void)
{
	memcpy(memory_region(REGION_SOUND1), memory_region(REGION_SOUND1) + 0x20000, 0x20000);
}



/*************************************
 *
 *	Input ports
 *
 *************************************/

READ_HANDLER(common_port_r)
{
	return readinputport(offset / 2) | readinputport(2);
}



/*************************************
 *
 *	Sound I/O
 *
 *************************************/

void sound_command_w_callback(int data)
{
	sound_busy = 1;
	sound_data = data;
	cpu_set_irq_line(1, 0, (ym2151_irq | sound_busy) ? ASSERT_LINE : CLEAR_LINE);
}


WRITE_HANDLER(sound_command_w)
{
	if (!(data & 0x00ff0000))
		timer_set(TIME_NOW, data & 0xff, sound_command_w_callback);
}


READ_HANDLER(sound_command_r)
{
	sound_busy = 0;
	cpu_set_irq_line(1, 0, (ym2151_irq | sound_busy) ? ASSERT_LINE : CLEAR_LINE);
	return sound_data;
}


READ_HANDLER(sound_busy_r)
{
	return sound_busy;
}



/*************************************
 *
 *	UPD7759 controller
 *
 *************************************/

WRITE_HANDLER(upd_control_w)
{
	if ((data & 1) != upd_rom_bank)
	{
		upd_rom_bank = data & 1;
		memcpy(memory_region(REGION_SOUND1), memory_region(REGION_SOUND1) + 0x20000 * (upd_rom_bank + 1), 0x20000);
	}
	UPD7759_reset_w(0, data >> 7);
}


WRITE_HANDLER(upd_data_w)
{
	UPD7759_message_w(0, data);
	UPD7759_start_w(0, 0);
	UPD7759_start_w(0, 1);
}



/*************************************
 *
 *	Mirroring (20bit -> 24bit)
 *
 *************************************/

static READ_HANDLER(mirror_r)
{
	return cpu_readmem24bew_word(offset & 0xfffff);
}


static WRITE_HANDLER(mirror_w)
{
	if (!(data & 0xffff0000))
		cpu_writemem24bew_word(offset & 0xfffff, data);
	else if (!(data & 0xff000000))
		cpu_writemem24bew(offset & 0xfffff, (data >> 8) & 0xff);
	else
		cpu_writemem24bew((offset & 0xfffff) + 1, data & 0xff);
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x040000, 0x04ffff, MRA_BANK1 },
	{ 0x060000, 0x060fff, MRA_BANK2 },
	{ 0x080000, 0x083fff, MRA_BANK3 },
	{ 0x0c0018, 0x0c001b, common_port_r },
	{ 0x0c001c, 0x0c001d, input_port_3_r },
	{ 0x0c001e, 0x0c001f, sound_busy_r },
	{ 0x0a0000, 0x0a07ff, MRA_BANK4 },
	{ 0x0fc000, 0x0fffff, MRA_BANK5 },
	{ 0x100000, 0xffffff, mirror_r },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x040000, 0x04ffff, rpunch_bitmap_w, &rpunch_bitmapram, &rpunch_bitmapram_size },
	{ 0x060000, 0x060fff, MWA_BANK2, &spriteram, &spriteram_size },
	{ 0x080000, 0x083fff, rpunch_videoram_w, &videoram, &videoram_size },
	{ 0x0a0000, 0x0a07ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0x0c0000, 0x0c0007, rpunch_scrollreg_w },
	{ 0x0c0008, 0x0c0009, rpunch_crtc_data_w },
	{ 0x0c000c, 0x0c000d, rpunch_videoreg_w },
	{ 0x0c000e, 0x0c000f, sound_command_w },
	{ 0x0c0010, 0x0c0013, rpunch_ins_w },
	{ 0x0c0028, 0x0c0029, rpunch_crtc_register_w },
	{ 0x0fc000, 0x0fffff, MWA_BANK5 },
	{ 0x100000, 0xffffff, mirror_w },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf001, YM2151_status_port_0_r },
	{ 0xf200, 0xf200, sound_command_r },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf000, YM2151_register_port_0_w },
	{ 0xf001, 0xf001, YM2151_data_port_0_w },
	{ 0xf400, 0xf400, upd_control_w },
	{ 0xf600, 0xf600, upd_data_w },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( rpunch )
	PORT_START	/* c0018 lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001a lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c0018/c001a upper 8 bits */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x0200, IP_ACTIVE_HIGH )
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001c DIP switches */
	PORT_DIPNAME( 0x000f, 0x0000, DEF_STR( Coin_A ))
	PORT_DIPSETTING(      0x000d, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0005, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0003, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x00f0, 0x0000, DEF_STR( Coin_B ))
	PORT_DIPSETTING(      0x00d0, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x00c0, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0050, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0030, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0090, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x0300, 0x0000, DEF_STR( Difficulty ))
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0000, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0300, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Lives ))
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x0400, "3" )
	PORT_DIPNAME( 0x0800, 0x0000, "Laser" )
	PORT_DIPSETTING(      0x0000, "Manual" )
	PORT_DIPSETTING(      0x0800, "Semi-Automatic" )
	PORT_DIPNAME( 0x1000, 0x0000, "Extended Play" )
	PORT_DIPSETTING(      0x0000, "500000 points" )
	PORT_DIPSETTING(      0x1000, "None" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Demo_Sounds ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x0000, "Continues" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END


INPUT_PORTS_START( rabiolep )
	PORT_START	/* c0018 lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001a lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c0018/c001a upper 8 bits */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x0200, IP_ACTIVE_HIGH )
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001c DIP switches */
	PORT_DIPNAME( 0x000f, 0x0000, DEF_STR( Coin_A ))
	PORT_DIPSETTING(      0x000d, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0005, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0003, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x00f0, 0x0000, DEF_STR( Coin_B ))
	PORT_DIPSETTING(      0x00d0, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x00c0, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0050, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0030, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0090, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x0300, 0x0000, DEF_STR( Difficulty ))
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0000, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0300, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0000, DEF_STR( Lives ))
	PORT_DIPSETTING(      0x0400, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0800, 0x0000, "Laser" )
	PORT_DIPSETTING(      0x0800, "Manual" )
	PORT_DIPSETTING(      0x0000, "Semi-Automatic" )
	PORT_DIPNAME( 0x1000, 0x0000, "Extended Play" )
	PORT_DIPSETTING(      0x0000, "500000 points" )
	PORT_DIPSETTING(      0x1000, "300000 points" )
	PORT_DIPNAME( 0x2000, 0x0000, DEF_STR( Demo_Sounds ))
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x0000, "Continues" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END



INPUT_PORTS_START( svolley )
	PORT_START	/* c0018 lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001a lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c0018/c001a upper 8 bits */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0700, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001c DIP switches */
	PORT_DIPNAME( 0x000f, 0x0000, DEF_STR( Coin_A ))
	PORT_DIPSETTING(      0x000d, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0005, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0003, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x00f0, 0x0000, DEF_STR( Coin_B ))
	PORT_DIPSETTING(      0x00d0, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x00c0, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0050, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0030, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0090, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x0100, 0x0000, "Game Time")
	PORT_DIPSETTING(      0x0100, "2 min/1 min" )
	PORT_DIPSETTING(      0x0000, "3 min/1.5 min" )
	PORT_DIPNAME( 0x0600, 0x0000, "2P Starting Score" )
	PORT_DIPSETTING(      0x0600, "0-0" )
	PORT_DIPSETTING(      0x0400, "5-5" )
	PORT_DIPSETTING(      0x0000, "7-7" )
	PORT_DIPSETTING(      0x0200, "9-9" )
	PORT_DIPNAME( 0x1800, 0x0000, "1P Starting Score" )
	PORT_DIPSETTING(      0x1000, "9-11" )
	PORT_DIPSETTING(      0x1800, "10-10" )
	PORT_DIPSETTING(      0x0800, "10-11" )
	PORT_DIPSETTING(      0x0000, "11-11" )
	PORT_SERVICE( 0x2000, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x4000, 0x0000, DEF_STR( Demo_Sounds ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x4000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout bglayout =
{
	8,8,
	12288,
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	8*32
};


static struct GfxLayout splayout =
{
	16,32,
	1536,
	4,
	{ 0, 1, 2, 3 },
	{ 12, 8, 4, 0, 28, 24, 20, 16, 44, 40, 36, 32, 60, 56, 52, 48 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64,
			16*64, 17*64, 18*64, 19*64, 20*64, 21*64, 22*64, 23*64,
			24*64, 25*64, 26*64, 27*64, 28*64, 29*64, 30*64, 31*64 },
	8*256
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &bglayout,   0, 16 },
	{ REGION_GFX2, 0, &bglayout, 256, 16 },
	{ REGION_GFX3, 0, &splayout,   0, 16*4 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	MASTER_CLOCK/4,
	{ YM3012_VOL(50,MIXER_PAN_CENTER,50,MIXER_PAN_CENTER) },
	{ ym2151_irq_gen }
};


static struct UPD7759_interface upd7759_interface =
{
	1,			/* 1 chip */
	UPD_CLOCK,
	{ 50 },
	{ REGION_SOUND1 },
	UPD7759_STANDALONE_MODE,
	{ 0 }
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_rpunch =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			MASTER_CLOCK/2,
			readmem,writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_Z80,
			MASTER_CLOCK/4,
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	init_machine,

	/* video hardware */
	304, 224, { 8, 303-8, 0, 223-8 },
	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	rpunch_vh_start,
	rpunch_vh_stop,
	rpunch_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_UPD7759,
			&upd7759_interface
		}
	},
	0
};



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( rpunch )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD_EVEN( "rpunch.20", 0x00000, 0x08000, 0xa2028d59 )
	ROM_LOAD_ODD ( "rpunch.21", 0x00000, 0x08000, 0x1cdb13d3 )
	ROM_LOAD_EVEN( "rpunch.2",  0x10000, 0x08000, 0x9b9729bb )
	ROM_LOAD_ODD ( "rpunch.3",  0x10000, 0x08000, 0x5704a688 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "rpunch.92", 0x00000, 0x10000, 0x5e1870e3 )

	ROM_REGION( 0x60000, REGION_GFX1 )
	ROM_LOAD( "rl_c13.bin", 0x00000, 0x40000, 0x7c8403b0 )
	ROM_LOAD( "rl_c10.bin", 0x40000, 0x08000, 0x312eb260 )
	ROM_LOAD( "rl_c12.bin", 0x48000, 0x08000, 0xbea85219 )

	ROM_REGION( 0x60000, REGION_GFX2 )
	ROM_LOAD( "rl_a10.bin", 0x00000, 0x40000, 0xc2a77619 )
	ROM_LOAD( "rl_a13.bin", 0x40000, 0x08000, 0xa39c2c16 )
	ROM_LOAD( "rpunch.54",  0x48000, 0x08000, 0xe2969747 )

	ROM_REGION( 0x60000, REGION_GFX3 )
	ROM_LOAD_GFX_EVEN( "rl_4g.bin", 0x00000, 0x20000, 0xc5cb4b7a )
	ROM_LOAD_GFX_ODD ( "rl_4h.bin", 0x00000, 0x20000, 0x8a4d3c99 )
	ROM_LOAD_GFX_EVEN( "rl_1g.bin", 0x40000, 0x08000, 0x74d41b2e )
	ROM_LOAD_GFX_ODD ( "rl_1h.bin", 0x40000, 0x08000, 0x7dcb32bb )
	ROM_LOAD_GFX_EVEN( "rpunch.85", 0x50000, 0x08000, 0x60b88a2c )
	ROM_LOAD_GFX_ODD ( "rpunch.86", 0x50000, 0x08000, 0x91d204f6 )

	ROM_REGION( 0x60000, REGION_SOUND1 )
	ROM_LOAD( "rl_f18.bin", 0x20000, 0x20000, 0x47840673 )
//	ROM_LOAD( "rpunch.91", 0x00000, 0x0f000, 0x7512cc59 )
ROM_END


ROM_START( rabiolep )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD_EVEN( "rl_e2.bin", 0x00000, 0x08000, 0x7d936a12 )
	ROM_LOAD_ODD ( "rl_d2.bin", 0x00000, 0x08000, 0xd8d85429 )
	ROM_LOAD_EVEN( "rl_e4.bin", 0x10000, 0x08000, 0x5bfaee12 )
	ROM_LOAD_ODD ( "rl_d4.bin", 0x10000, 0x08000, 0xe64216bf )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "rl_f20.bin", 0x00000, 0x10000, 0xa6f50351 )

	ROM_REGION( 0x60000, REGION_GFX1 )
	ROM_LOAD( "rl_c13.bin", 0x00000, 0x40000, 0x7c8403b0 )
	ROM_LOAD( "rl_c10.bin", 0x40000, 0x08000, 0x312eb260 )
	ROM_LOAD( "rl_c12.bin", 0x48000, 0x08000, 0xbea85219 )

	ROM_REGION( 0x60000, REGION_GFX2 )
	ROM_LOAD( "rl_a10.bin", 0x00000, 0x40000, 0xc2a77619 )
	ROM_LOAD( "rl_a13.bin", 0x40000, 0x08000, 0xa39c2c16 )
	ROM_LOAD( "rl_a12.bin", 0x48000, 0x08000, 0x970b0e32 )

	ROM_REGION( 0x60000, REGION_GFX3 )
	ROM_LOAD_GFX_EVEN( "rl_4g.bin", 0x00000, 0x20000, 0xc5cb4b7a )
	ROM_LOAD_GFX_ODD ( "rl_4h.bin", 0x00000, 0x20000, 0x8a4d3c99 )
	ROM_LOAD_GFX_EVEN( "rl_1g.bin", 0x40000, 0x08000, 0x74d41b2e )
	ROM_LOAD_GFX_ODD ( "rl_1h.bin", 0x40000, 0x08000, 0x7dcb32bb )
	ROM_LOAD_GFX_EVEN( "rl_2g.bin", 0x50000, 0x08000, 0x744903b4 )
	ROM_LOAD_GFX_ODD ( "rl_2h.bin", 0x50000, 0x08000, 0x09649e75 )

	ROM_REGION( 0x60000, REGION_SOUND1 )
	ROM_LOAD( "rl_f18.bin", 0x20000, 0x20000, 0x47840673 )
ROM_END


ROM_START( svolley )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD_EVEN( "sps_13.bin", 0x00000, 0x10000, 0x2fbc5dcf )
	ROM_LOAD_ODD ( "sps_11.bin", 0x00000, 0x10000, 0x51b025c9 )
	ROM_LOAD_EVEN( "sps_14.bin", 0x20000, 0x08000, 0xe7630122 )
	ROM_LOAD_ODD ( "sps_12.bin", 0x20000, 0x08000, 0xb6b24910 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "sps_17.bin", 0x00000, 0x10000, 0x48b89688 )

	ROM_REGION( 0x60000, REGION_GFX1 )
	ROM_LOAD( "sps_02.bin", 0x00000, 0x10000, 0x1a0abe75 )
	ROM_LOAD( "sps_03.bin", 0x10000, 0x10000, 0x36279075 )
	ROM_LOAD( "sps_04.bin", 0x20000, 0x10000, 0x7cede7d9 )
	ROM_LOAD( "sps_01.bin", 0x30000, 0x08000, 0x6425e6d7 )
	ROM_LOAD( "sps_10.bin", 0x40000, 0x08000, 0xa12b1589 )

	ROM_REGION( 0x60000, REGION_GFX2 )
	ROM_LOAD( "sps_05.bin", 0x00000, 0x10000, 0xb0671d12 )
	ROM_LOAD( "sps_06.bin", 0x10000, 0x10000, 0xc231957e )
	ROM_LOAD( "sps_07.bin", 0x20000, 0x10000, 0x904b7709 )
	ROM_LOAD( "sps_08.bin", 0x30000, 0x10000, 0x5430ffac )
	ROM_LOAD( "sps_09.bin", 0x40000, 0x10000, 0x414a6278 )

	ROM_REGION( 0x60000, REGION_GFX3 )
	ROM_LOAD_GFX_EVEN( "sps_20.bin", 0x00000, 0x10000, 0xc9e7206d )
	ROM_LOAD_GFX_ODD ( "sps_23.bin", 0x00000, 0x10000, 0x7b15c805 )
	ROM_LOAD_GFX_EVEN( "sps_19.bin", 0x20000, 0x08000, 0x8ac2f232 )
	ROM_LOAD_GFX_ODD ( "sps_22.bin", 0x20000, 0x08000, 0xfcc754e3 )
	ROM_LOAD_GFX_EVEN( "sps_18.bin", 0x30000, 0x08000, 0x4d6c8f0c )
	ROM_LOAD_GFX_ODD ( "sps_21.bin", 0x30000, 0x08000, 0x9dd28b42 )

	ROM_REGION( 0x60000, REGION_SOUND1 )
	ROM_LOAD( "sps_16.bin", 0x20000, 0x20000, 0x456d0f36 )
	ROM_LOAD( "sps_15.bin", 0x40000, 0x10000, 0xf33f415f )
ROM_END


ROM_START( svolleyk )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD_EVEN( "a14.bin", 0x00000, 0x10000, 0xdbab3bf9 )
	ROM_LOAD_ODD ( "a11.bin", 0x00000, 0x10000, 0x92afd56f )
	ROM_LOAD_EVEN( "a15.bin", 0x20000, 0x08000, 0xd8f89c4a )
	ROM_LOAD_ODD ( "a12.bin", 0x20000, 0x08000, 0xde3dd5cb )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "sps_17.bin", 0x00000, 0x10000, 0x48b89688 )

	ROM_REGION( 0x60000, REGION_GFX1 )
	ROM_LOAD( "sps_02.bin", 0x00000, 0x10000, 0x1a0abe75 )
	ROM_LOAD( "sps_03.bin", 0x10000, 0x10000, 0x36279075 )
	ROM_LOAD( "sps_04.bin", 0x20000, 0x10000, 0x7cede7d9 )
	ROM_LOAD( "sps_01.bin", 0x30000, 0x08000, 0x6425e6d7 )
	ROM_LOAD( "sps_10.bin", 0x40000, 0x08000, 0xa12b1589 )

	ROM_REGION( 0x60000, REGION_GFX2 )
	ROM_LOAD( "sps_05.bin", 0x00000, 0x10000, 0xb0671d12 )
	ROM_LOAD( "sps_06.bin", 0x10000, 0x10000, 0xc231957e )
	ROM_LOAD( "sps_07.bin", 0x20000, 0x10000, 0x904b7709 )
	ROM_LOAD( "sps_08.bin", 0x30000, 0x10000, 0x5430ffac )
	ROM_LOAD( "sps_09.bin", 0x40000, 0x10000, 0x414a6278 )
	ROM_LOAD( "a09.bin",    0x50000, 0x08000, 0xdd92dfe1 )

	ROM_REGION( 0x60000, REGION_GFX3 )
	ROM_LOAD_GFX_EVEN( "sps_20.bin", 0x00000, 0x10000, 0xc9e7206d )
	ROM_LOAD_GFX_ODD ( "sps_23.bin", 0x00000, 0x10000, 0x7b15c805 )
	ROM_LOAD_GFX_EVEN( "sps_19.bin", 0x20000, 0x08000, 0x8ac2f232 )
	ROM_LOAD_GFX_ODD ( "sps_22.bin", 0x20000, 0x08000, 0xfcc754e3 )
	ROM_LOAD_GFX_EVEN( "sps_18.bin", 0x30000, 0x08000, 0x4d6c8f0c )
	ROM_LOAD_GFX_ODD ( "sps_21.bin", 0x30000, 0x08000, 0x9dd28b42 )

	ROM_REGION( 0x60000, REGION_SOUND1 )
	ROM_LOAD( "sps_16.bin", 0x20000, 0x20000, 0x456d0f36 )
	ROM_LOAD( "sps_15.bin", 0x40000, 0x10000, 0xf33f415f )
ROM_END



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void init_rabiolep(void)
{
	rpunch_sprite_palette = 0x300;

	/* clear out any unused regions of background gfx */
	memset(memory_region(REGION_GFX1) + 0x50000, 0xff, 0x10000);
	memset(memory_region(REGION_GFX2) + 0x50000, 0xff, 0x10000);
}


static void init_svolley(void)
{
	/* the main differences between Super Volleyball and Rabbit Punch are */
	/* the lack of direct-mapped bitmap, a smaller sprite range, and a */
	/* different palette base for sprites */
	rpunch_sprite_palette = 0x080;
	rpunch_bitmapram = NULL;
	spriteram_size = 0x1b0;

	/* clear out any unused regions of background gfx */
	memset(memory_region(REGION_GFX1) + 0x38000, 0xff, 0x08000);
	memset(memory_region(REGION_GFX1) + 0x48000, 0xff, 0x18000);
	memset(memory_region(REGION_GFX2) + 0x50000, 0xff, 0x10000);
}


static void init_svolleyk(void)
{
	/* the main differences between Super Volleyball and Rabbit Punch are */
	/* the lack of direct-mapped bitmap, a smaller sprite range, and a */
	/* different palette base for sprites */
	rpunch_sprite_palette = 0x080;
	rpunch_bitmapram = NULL;
	spriteram_size = 0x1b0;

	/* clear out any unused regions of background gfx */
	memset(memory_region(REGION_GFX1) + 0x38000, 0xff, 0x08000);
	memset(memory_region(REGION_GFX1) + 0x48000, 0xff, 0x18000);
	memset(memory_region(REGION_GFX2) + 0x58000, 0xff, 0x08000);
}



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1987, rabiolep, 0,        rpunch,   rabiolep, rabiolep, ROT0, "V-System Co.", "Rabio Lepus (Japan)" )
GAME( 1987, rpunch,   rabiolep, rpunch,   rpunch,   rabiolep, ROT0, "V-System Co. (Bally/Midway/Sente license)", "Rabbit Punch (US)" )
GAME( 1989, svolley,  0,        rpunch,   svolley,  svolley,  ROT0, "V-System Co.", "Super Volleyball (Japan)" )
GAME( 1989, svolleyk, svolley,  rpunch,   svolley,  svolleyk, ROT0, "V-System Co.", "Super Volleyball (Korea)" )
