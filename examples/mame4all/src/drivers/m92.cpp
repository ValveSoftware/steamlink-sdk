#include "../vidhrdw/m92.cpp"

/*****************************************************************************

	Irem M92 system games:

	Blademaster	(World)						(c) 1991 Irem Corp
	Gunforce (World)				M92-A	(c) 1991 Irem Corp
	Gunforce (USA)					M92-A	(c) 1991 Irem America Corp
	Lethal Thunder (World)					(c) 1991 Irem Corp
	Thunder Blaster (Japan)					(c) 1991 Irem Corp
	Hook (World)							(c) 1992 Irem Corp
	Hook (USA)								(c) 1992 Irem America Corp
	Mystic Riders (World)					(c) 1992 Irem Corp
	Gun Hohki (Japan)						(c) 1992 Irem Corp
	Undercover Cops	(World)					(c) 1992 Irem Corp
	Undercover Cops	(Japan)					(c) 1992 Irem Corp
	R-Type Leo (Japan)						(c) 1992 Irem Corp
	Major Title 2 (World)			M92-F	(c) 1992 Irem Corp
	The Irem Skins Game (USA Set 1)	M92-F	(c) 1992 Irem America Corp
	The Irem Skins Game (USA Set 2)	M92-F	(c) 1992 Irem America Corp
	In The Hunt	(World)				M92-E	(c) 1993 Irem Corp
	In The Hunt	(USA)				M92-E	(c) 1993 Irem Corp
	Kaitei Daisensou (Japan)		M92-E	(c) 1993 Irem Corp
	Ninja Baseball Batman (USA)				(c) 1993 Irem America Corp
	Yakyuu Kakutou League-Man (Japan)		(c) 1993 Irem Corp
	Perfect Soldiers (Japan)		M92-G	(c) 1993 Irem Corp

	Once decrypted, Irem M97 games will be very close to this hardware.

System notes:
	Each game has an encrypted sound cpu (see m97.c), the sound cpu and
	the sprite chip are on the game board rather than the main board and
	can differ between games.

	Irem Skins Game has an eeprom and ticket payout(?).
	R-Type Leo & Lethal Thunder have a memory card.

	Many games use raster IRQ's for special video effects, eg,
		* Scrolling water in Undercover Cops
		* Score display in R-Type Leo

	These are slow to emulate, and can be turned on/off by pressing
	F1 - they are on by default.


Glitch list!

	Gunforce:
		Animated water sometimes doesn't appear on level 5 (but it
		always appears if you cheat and jump straight to the level).
		Almost certainly a core bug.

	R-Type Leo:
		Title screen is incorrect, it uses mask sprites but I can't find
		how the effect is turned on/off
		Crashes fixed via a kludge - cpu bug.

	Irem Skins:
		Mask sprite on title screen?
		Sprites & tiles written with bad colour values - cpu bug?
		Eeprom load/save not yet implemented - when done, MT2EEP should
		  be removed from the ROM definition.

	Blade Master:
		Sprite list register isn't updated in service mode.

	Ninja Baseball:
		Very picky about interrupts..
		Doesn't work!

	Emulation by Bryan McPhail, mish@tendril.co.uk
	Thanks to Chris Hardy and Olli Bergmann too!


Irem Custom V33 CPU:

Gunforce						Nanao 	08J27261A 011 9106KK701
Quiz F-1 1,2 Finish          	Nanao	08J27291A4 014 9147KK700
Skins Game						Nanao 	08J27291A7
R-Type Leo						Irem 	D800001A1
In The Hunt						Irem 	D8000011A1
Gun Hohki									  16?
Risky Challenge/Gussun Oyoyo 			D8000019A1
Shisensho II                 			D8000020A1 023 9320NK700
Perfect Soldiers						D8000022A1

Nanao Custom parts:

GA20	Sample player
GA21	Tilemaps } Used in combination
GA22	Tilemaps }
GA23	Sprites (Gun Force)
GA30 	Tilemaps
GA31	Tilemaps
GA32	Sprites (Fire Barrel)

*****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "m92.h"

static int m92_irq_vectorbase,m92_vblank;
static unsigned char *m92_eeprom,*m92_ram;

#define M92_IRQ_0 ((m92_irq_vectorbase+0)/4)  /* VBL interrupt*/
#define M92_IRQ_1 ((m92_irq_vectorbase+4)/4)  /* End of VBL interrupt?? */
#define M92_IRQ_2 ((m92_irq_vectorbase+8)/4)  /* Raster interrupt */
#define M92_IRQ_3 ((m92_irq_vectorbase+12)/4) /* Sprite buffer complete interrupt? */

/* From vidhrdw/m92.c */
WRITE_HANDLER( m92_spritecontrol_w );
WRITE_HANDLER( m92_spritebuffer_w );
READ_HANDLER( m92_vram_r );
WRITE_HANDLER( m92_vram_w );
WRITE_HANDLER( m92_pf1_control_w );
WRITE_HANDLER( m92_pf2_control_w );
WRITE_HANDLER( m92_pf3_control_w );
WRITE_HANDLER( m92_master_control_w );
int m92_vh_start(void);
void m92_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void m92_vh_raster_partial_refresh(struct osd_bitmap *bitmap,int start_line,int end_line);

extern int m92_raster_irq_position,m92_spritechip,m92_raster_machine,m92_raster_enable;
extern unsigned char *m92_vram_data,*m92_spritecontrol;

extern int m92_game_kludge;

/*****************************************************************************/

static READ_HANDLER( status_port_r )
{
//logerror("%06x: status %04x\n",cpu_get_pc(),offset);

/*

	Gunforce waits on bit 1 (word) going high on bootup, just after
	setting up interrupt controller.

	Gunforce reads bit 2 (word) on every coin insert, doesn't seem to
	do much though..

	Ninja Batman polls this.

	R-Type Leo reads it now & again..

*/
	if (m92_game_kludge==1)
		m92_ram[0x53]=1; /* FIXES RTYPE LEO! */

	return 0xff;
}

static READ_HANDLER( m92_eeprom_r )
{
	unsigned char *RAM = memory_region(REGION_USER1);
//	logerror("%05x: EEPROM RE %04x\n",cpu_get_pc(),offset);

	return RAM[offset/2];
}

static WRITE_HANDLER( m92_eeprom_w )
{
	unsigned char *RAM = memory_region(REGION_USER1);
//	logerror("%05x: EEPROM WR %04x\n",cpu_get_pc(),offset);
	RAM[offset/2]=data;
}

static WRITE_HANDLER( m92_coincounter_w )
{
	if (offset==0) {
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);

		if (m92_game_kludge==2) {
			unsigned char *RAM = memory_region(REGION_CPU1);
			RAM[0x1840]=0x90; /* For Leagueman */
			RAM[0x1841]=0x90;
			RAM[0x830]=0x90;
			RAM[0x831]=0x90;
		}

		/* Bit 0x8 is Motor(?!), used in Hook, In The Hunt, UCops */
		/* Bit 0x8 is Memcard related in RTypeLeo */
		/* Bit 0x40 set in Blade Master test mode input check */
	}
#if 0
	if (/*offset==0 &&*/ data!=0) {
		char t[16];
		sprintf(t,"%02x",data);
		usrintf_showmessage(t);
	}
#endif
}

static WRITE_HANDLER( m92_unknown_w )
{
#if 0
	static int d[2];
	d[offset]=data;
	if (1/*offset==1*/) {
		char t[16];
		sprintf(t,"%02x",d[0] | (d[1]<<8));
		usrintf_showmessage(t);
	}
#endif
}

static WRITE_HANDLER( m92_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	//logerror("%04x: Bank %04x (%02x)\n",cpu_get_pc(),data,offset);
	if (offset==1) return; /* Unused top byte */

	cpu_setbank(1,&RAM[0x100000 + ((data&0x7)*0x10000)]);
}

static READ_HANDLER( m92_port_4_r )
{
	if (m92_vblank) return readinputport(4) | 0;
	return readinputport(4) | 0x80;
}

static WRITE_HANDLER( m92_soundlatch_w )
{
	if (offset==0) soundlatch_w(0,data);
	/* Interrupt second V33 */
}

/*****************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x00000, 0x9ffff, MRA_ROM },
	{ 0xa0000, 0xbffff, MRA_BANK1 },
	{ 0xc0000, 0xcffff, MRA_BANK2 }, /* Mirror of rom:  Used by In The Hunt as protection */
	{ 0xd0000, 0xdffff, m92_vram_r },
	{ 0xe0000, 0xeffff, MRA_RAM },
	{ 0xf0000, 0xf3fff, m92_eeprom_r }, /* Eeprom, Major Title 2 only */
	{ 0xf8000, 0xf87ff, MRA_RAM },
	{ 0xf8800, 0xf8fff, paletteram_r },
	{ 0xffff0, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x00000, 0xbffff, MWA_ROM },
	{ 0xd0000, 0xdffff, m92_vram_w, &m92_vram_data },
	{ 0xe0000, 0xeffff, MWA_RAM, &m92_ram }, /* System ram */
	{ 0xf0000, 0xf3fff, m92_eeprom_w, &m92_eeprom }, /* Eeprom, Major Title 2 only */
	{ 0xf8000, 0xf87ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf8800, 0xf8fff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },
	{ 0xf9000, 0xf900f, m92_spritecontrol_w, &m92_spritecontrol },
	{ 0xf9800, 0xf9801, m92_spritebuffer_w },
	{ 0xffff0, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress lethalth_readmem[] =
{
	{ 0x00000, 0x7ffff, MRA_ROM },
	{ 0x80000, 0x8ffff, m92_vram_r },
	{ 0xe0000, 0xeffff, MRA_RAM },
	{ 0xf8000, 0xf87ff, MRA_RAM },
	{ 0xf8800, 0xf8fff, paletteram_r },
	{ 0xffff0, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress lethalth_writemem[] =
{
	{ 0x00000, 0x7ffff, MWA_ROM },
	{ 0x80000, 0x8ffff, m92_vram_w, &m92_vram_data },
	{ 0xe0000, 0xeffff, MWA_RAM, &m92_ram }, /* System ram */
	{ 0xf8000, 0xf87ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf8800, 0xf8fff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },
	{ 0xf9000, 0xf900f, m92_spritecontrol_w, &m92_spritecontrol },
	{ 0xf9800, 0xf9801, m92_spritebuffer_w },
	{ 0xffff0, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r }, /* Player 1 */
	{ 0x01, 0x01, input_port_1_r }, /* Player 2 */
	{ 0x02, 0x02, m92_port_4_r },   /* Coins & VBL */
	{ 0x03, 0x03, input_port_7_r }, /* Dip 3 */
	{ 0x04, 0x04, input_port_6_r }, /* Dip 2 */
	{ 0x05, 0x05, input_port_5_r }, /* Dip 1 */
	{ 0x06, 0x06, input_port_2_r }, /* Player 3 */
	{ 0x07, 0x07, input_port_3_r }, /* Player 4 */
	{ 0x08, 0x09, status_port_r },  /* ? */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x01, m92_soundlatch_w },
	{ 0x02, 0x03, m92_coincounter_w },
	{ 0x20, 0x21, m92_bankswitch_w },
	{ 0x40, 0x43, MWA_NOP }, /* Interrupt controller, only written to at bootup */
	{ 0x80, 0x87, m92_pf1_control_w },
	{ 0x88, 0x8f, m92_pf2_control_w },
	{ 0x90, 0x97, m92_pf3_control_w },
	{ 0x98, 0x9f, m92_master_control_w },
	{ 0xc0, 0xc1, m92_unknown_w },
	{ -1 }	/* end of table */
};

/******************************************************************************/

#if 0
static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x00000, 0x1ffff, MRA_ROM },
	{ 0xffff0, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x00000, 0x1ffff, MWA_ROM },
	{ 0xffff0, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};
#endif

/******************************************************************************/

INPUT_PORTS_START( bmaster )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( gunforce )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "15000 35000 75000 120000" )
	PORT_DIPSETTING(    0x10, "20000 40000 90000 150000" )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_UNUSED	/* Game manual only mentions 2 dips */
INPUT_PORTS_END

INPUT_PORTS_START( lethalth )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( hook )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_PLAYER3_2BUTTON_JOYSTICK
	PORT_PLAYER4_2BUTTON_JOYSTICK
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Any Button Starts" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Number of Players" )
	PORT_DIPSETTING(    0x02, "2 Players" )
	PORT_DIPSETTING(    0x00, "4 Players" )
	PORT_DIPNAME( 0x04, 0x04, "Coin Slots" )
	PORT_DIPSETTING(    0x04, "Common Coins" )
	PORT_DIPSETTING(    0x00, "Separate Coins" )
	PORT_DIPNAME( 0x08, 0x08, "Coin Mode" )
	PORT_DIPSETTING(    0x08, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
/* Coin Mode 1, todo Mode 2 */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x10, "2 Start/1 Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
INPUT_PORTS_END

INPUT_PORTS_START( majtitl2 )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( mysticri )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( uccops )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_PLAYER3_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( rtypeleo )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* Buy in/coin mode?? */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_UNUSED	/* Game manual only mentions 2 dips */
INPUT_PORTS_END

INPUT_PORTS_START( inthunt )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Any Button starts game" ) /* ? */
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Number of Players" )
	PORT_DIPSETTING(    0x02, "2 Players" )
	PORT_DIPSETTING(    0x00, "4 Players" )
	PORT_DIPNAME( 0x04, 0x04, "Coin Slots" )
	PORT_DIPSETTING(    0x04, "Common Coins" )
	PORT_DIPSETTING(    0x00, "Seperate Coins" )
	PORT_DIPNAME( 0x08, 0x08, "Coin Mode" )
	PORT_DIPSETTING(    0x08, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
/* Coin Mode 1, todo Mode 2 */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x10, "2 Start/1 Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
INPUT_PORTS_END

INPUT_PORTS_START( nbbatman )
	PORT_PLAYER1_2BUTTON_JOYSTICK
	PORT_PLAYER2_2BUTTON_JOYSTICK
	PORT_UNUSED
	PORT_UNUSED
	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( psoldier )
	PORT_START
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )

	PORT_UNUSED

	PORT_START /* Extra connector for kick buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )

	PORT_COINS_VBLANK
	PORT_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/***************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	65536,	/* 65536 characters */
	4,	/* 4 bits per pixel */
	{ 0x180000*8, 0x100000*8, 0x80000*8, 0x000000*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,
	0x8000,
	4,
	{ 0x300000*8, 0x200000*8, 0x100000*8, 0x000000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8
};

static struct GfxLayout spritelayout2 =
{
	16,16,
	0x10000,
	4,
	{ 0x600000*8, 0x400000*8, 0x200000*8, 0x000000*8 },
	{ 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 64 },
	{ REGION_GFX2, 0, &spritelayout, 0, 64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo2[] =
{
	{ REGION_GFX1, 0, &charlayout,    0, 64 },
	{ REGION_GFX2, 0, &spritelayout2, 0, 64 },
	{ -1 } /* end of array */
};

/***************************************************************************/

static int m92_interrupt(void)
{
	m92_vblank=m92_raster_machine=0;
	if (osd_skip_this_frame()==0)
		m92_vh_raster_partial_refresh(Machine->scrbitmap,0,248);

	return M92_IRQ_0; /* VBL */
}

static int m92_raster_interrupt(void)
{
	static int last_line=0;
	int line = 256 - cpu_getiloops();
	m92_raster_machine=1;

	/* Raster interrupt */
	if (m92_raster_enable && line==m92_raster_irq_position) {
		if (osd_skip_this_frame()==0)
			m92_vh_raster_partial_refresh(Machine->scrbitmap,last_line,line);
		last_line=line+1;

		return M92_IRQ_2;
	}

	/* Redraw screen, then set vblank and trigger the VBL interrupt */
	if (line==248) {
		if (osd_skip_this_frame()==0)
			m92_vh_raster_partial_refresh(Machine->scrbitmap,last_line,248);
		last_line=248;
		m92_vblank=1;
		return M92_IRQ_0;
	}

	/* End of vblank */
	if (line==255) {
		m92_vblank=last_line=0;
		return 0;
	}

	if (m92_game_kludge==2 && line==250) /* Leagueman */
		return M92_IRQ_1;

	return 0;
}

/* I believe the spritechip interrupts the main CPU once sprite buffering
is complete, there isn't a whole lot of evidence for this but it's an idea..*/
void m92_sprite_interrupt(void)
{
	cpu_cause_interrupt(0,M92_IRQ_3);
}

static struct MachineDriver machine_driver_raster =
{
	/* basic machine hardware */
	{
		{
			CPU_V33,	/* NEC V33 */
			18000000,	/* 18 MHz clock */
			readmem,writemem,readport,writeport,
			m92_raster_interrupt,256 /* 8 prelines, 240 visible lines, 8 for vblank? */
		},
#if 0
		{
			CPU_V33 | CPU_AUDIO_CPU,
			14318180,	/* 14.31818 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
#endif
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	512, 512, { 80, 511-112, 128+8, 511-128-8 }, /* 320 x 240 */

	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	0,
	m92_vh_start,
	0,
	m92_vh_screenrefresh,

	/* sound hardware */
#if 0
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
#endif
};

static struct MachineDriver machine_driver_nonraster =
{
	/* basic machine hardware */
	{
		{
			CPU_V33,	/* NEC V33 */
			18000000,	/* 18 MHz clock */
			readmem,writemem,readport,writeport,
			m92_interrupt,1
		},
#if 0
		{
			CPU_V33 | CPU_AUDIO_CPU,
			14318180,	/* 14.31818 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
#endif
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	512, 512, { 80, 511-112, 128+8, 511-128-8 }, /* 320 x 240 */

	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	0,
	m92_vh_start,
	0,
	m92_vh_screenrefresh,

	/* sound hardware */
#if 0
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
#endif
};

static struct MachineDriver machine_driver_lethalth =
{
	/* basic machine hardware */
	{
		{
			CPU_V33,	/* NEC V33 */
			18000000,	/* 18 MHz clock */
			lethalth_readmem,lethalth_writemem,readport,writeport,
			m92_interrupt,1
		},
#if 0
		{
			CPU_V33 | CPU_AUDIO_CPU,
			14318180,	/* 14.31818 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
#endif
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	512, 512, { 80, 511-112, 128+8, 511-128-8 }, /* 320 x 240 */

	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	0,
	m92_vh_start,
	0,
	m92_vh_screenrefresh,

	/* sound hardware */
#if 0
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
#endif
};

static struct MachineDriver machine_driver_psoldier =
{
	/* basic machine hardware */
	{
		{
			CPU_V33,	/* NEC V33 */
			18000000,	/* 18 MHz clock */
			readmem,writemem,readport,writeport,
			m92_interrupt,1
		},
#if 0
		{
			CPU_V33 | CPU_AUDIO_CPU,
			14318180,	/* 14.31818 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
#endif
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	512, 512, { 80, 511-112, 128+8, 511-128-8 }, /* 320 x 240 */

	gfxdecodeinfo2,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	0,
	m92_vh_start,
	0,
	m92_vh_screenrefresh,

	/* sound hardware */
#if 0
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
#endif
};

/***************************************************************************/

ROM_START( bmaster )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "bm_d-h0.rom",  0x000000, 0x40000, 0x49b257c7 )
	ROM_LOAD_V20_ODD ( "bm_d-l0.rom",  0x000000, 0x40000, 0xa873523e )
	ROM_LOAD_V20_EVEN( "bm_d-h1.rom",  0x080000, 0x10000, 0x082b7158 )
	ROM_LOAD_V20_ODD ( "bm_d-l1.rom",  0x080000, 0x10000, 0x6ff0c04e )

	ROM_REGION( 0x100000, REGION_CPU2 )
	ROM_LOAD_V20_EVEN( "bm_d-sh0.rom",  0x000000, 0x10000, 0x9f7c075b )
	ROM_LOAD_V20_ODD ( "bm_d-sl0.rom",  0x000000, 0x10000, 0x1fa87c89 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "bm_c0.rom",       0x000000, 0x40000, 0x2cc966b8 )
	ROM_LOAD( "bm_c1.rom",       0x080000, 0x40000, 0x46df773e )
	ROM_LOAD( "bm_c2.rom",       0x100000, 0x40000, 0x05b867bd )
	ROM_LOAD( "bm_c3.rom",       0x180000, 0x40000, 0x0a2227a4 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "bm_000.rom",      0x000000, 0x80000, 0x339fc9f3 )
	ROM_LOAD( "bm_010.rom",      0x100000, 0x80000, 0x6a14377d )
	ROM_LOAD( "bm_020.rom",      0x200000, 0x80000, 0x31532198 )
	ROM_LOAD( "bm_030.rom",      0x300000, 0x80000, 0xd1a041d3 )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "bm_da.rom",       0x000000, 0x80000, 0x62ce5798 )
ROM_END

ROM_START( skingame )
	ROM_REGION( 0x180000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "is-h0-d",  0x000000, 0x40000, 0x80940abb )
	ROM_LOAD_V20_ODD ( "is-l0-d",  0x000000, 0x40000, 0xb84beed6 )
	ROM_LOAD_V20_EVEN( "is-h1",    0x100000, 0x40000, 0x9ba8e1f2 )
	ROM_LOAD_V20_ODD ( "is-l1",    0x100000, 0x40000, 0xe4e00626 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD_V20_EVEN( "mt2sh0",  0x000000, 0x10000, 0x1ecbea43 )
	ROM_LOAD_V20_ODD ( "mt2sl0",  0x000000, 0x10000, 0x8fd5b531 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "c0",       0x000000, 0x40000, 0x7e61e4b5 )
	ROM_LOAD( "c1",       0x080000, 0x40000, 0x0a667564 )
	ROM_LOAD( "c2",       0x100000, 0x40000, 0x5eb44312 )
	ROM_LOAD( "c3",       0x180000, 0x40000, 0xf2866294 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "k30",      0x000000, 0x100000, 0x8c9a2678 )
	ROM_LOAD( "k31",      0x100000, 0x100000, 0x5455df78 )
	ROM_LOAD( "k32",      0x200000, 0x100000, 0x3a258c41 )
	ROM_LOAD( "k33",      0x300000, 0x100000, 0xc1e91a14 )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "da",        0x000000, 0x80000, 0x713b9e9f )

	ROM_REGION( 0x4000, REGION_USER1 )	/* EEPROM */
	ROM_LOAD( "mt2eep",       0x000000, 0x800, 0x208af971 )
ROM_END

ROM_START( majtitl2 )
	ROM_REGION( 0x180000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "mt2-ho-b.5m",0x000000, 0x40000, 0xb163b12e )
	ROM_LOAD_V20_ODD ( "mt2-lo-b.5f",0x000000, 0x40000, 0x6f3b5d9d )
	ROM_LOAD_V20_EVEN( "is-h1",      0x100000, 0x40000, 0x9ba8e1f2 )
	ROM_LOAD_V20_ODD ( "is-l1",      0x100000, 0x40000, 0xe4e00626 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD_V20_EVEN( "mt2sh0",  0x000000, 0x10000, 0x1ecbea43 )
	ROM_LOAD_V20_ODD ( "mt2sl0",  0x000000, 0x10000, 0x8fd5b531 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "c0",       0x000000, 0x40000, 0x7e61e4b5 )
	ROM_LOAD( "c1",       0x080000, 0x40000, 0x0a667564 )
	ROM_LOAD( "c2",       0x100000, 0x40000, 0x5eb44312 )
	ROM_LOAD( "c3",       0x180000, 0x40000, 0xf2866294 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "k30",      0x000000, 0x100000, 0x8c9a2678 )
	ROM_LOAD( "k31",      0x100000, 0x100000, 0x5455df78 )
	ROM_LOAD( "k32",      0x200000, 0x100000, 0x3a258c41 )
	ROM_LOAD( "k33",      0x300000, 0x100000, 0xc1e91a14 )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "da",        0x000000, 0x80000, 0x713b9e9f )

	ROM_REGION( 0x4000, REGION_USER1 )	/* EEPROM */
	ROM_LOAD( "mt2eep",       0x000000, 0x800, 0x208af971 )
ROM_END

ROM_START( skingam2 )
	ROM_REGION( 0x180000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "mt2h0a", 0x000000, 0x40000, 0x7c6dbbc7 )
	ROM_LOAD_V20_ODD ( "mt2l0a", 0x000000, 0x40000, 0x9de5f689 )
	ROM_LOAD_V20_EVEN( "is-h1",  0x100000, 0x40000, 0x9ba8e1f2 )
	ROM_LOAD_V20_ODD ( "is-l1",  0x100000, 0x40000, 0xe4e00626 )

	ROM_REGION( 0x100000, REGION_CPU2 )
	ROM_LOAD_V20_EVEN( "mt2sh0",  0x000000, 0x10000, 0x1ecbea43 )
	ROM_LOAD_V20_ODD ( "mt2sl0",  0x000000, 0x10000, 0x8fd5b531 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "c0",       0x000000, 0x40000, 0x7e61e4b5 )
	ROM_LOAD( "c1",       0x080000, 0x40000, 0x0a667564 )
	ROM_LOAD( "c2",       0x100000, 0x40000, 0x5eb44312 )
	ROM_LOAD( "c3",       0x180000, 0x40000, 0xf2866294 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "k30",      0x000000, 0x100000, 0x8c9a2678 )
	ROM_LOAD( "k31",      0x100000, 0x100000, 0x5455df78 )
	ROM_LOAD( "k32",      0x200000, 0x100000, 0x3a258c41 )
	ROM_LOAD( "k33",      0x300000, 0x100000, 0xc1e91a14 )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "da",        0x000000, 0x80000, 0x713b9e9f )

	ROM_REGION( 0x4000, REGION_USER1 )	/* EEPROM */
	ROM_LOAD( "mt2eep",       0x000000, 0x800, 0x208af971 )
ROM_END

ROM_START( gunforce )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "gf_h0-c.rom",  0x000000, 0x20000, 0xc09bb634 )
	ROM_LOAD_V20_ODD ( "gf_l0-c.rom",  0x000000, 0x20000, 0x1bef6f7d )
	ROM_LOAD_V20_EVEN( "gf_h1-c.rom",  0x040000, 0x20000, 0xc84188b7 )
	ROM_LOAD_V20_ODD ( "gf_l1-c.rom",  0x040000, 0x20000, 0xb189f72a )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "gf_sh0.rom",0x000000, 0x010000, 0x3f8f16e0 )
	ROM_LOAD_V20_ODD ( "gf_sl0.rom",0x000000, 0x010000, 0xdb0b13a3 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "gf_c0.rom",       0x000000, 0x40000, 0xb3b74979 )
	ROM_LOAD( "gf_c1.rom",       0x080000, 0x40000, 0xf5c8590a )
	ROM_LOAD( "gf_c2.rom",       0x100000, 0x40000, 0x30f9fb64 )
	ROM_LOAD( "gf_c3.rom",       0x180000, 0x40000, 0x87b3e621 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "gf_000.rom",      0x000000, 0x40000, 0x209e8e8d )
	ROM_LOAD( "gf_010.rom",      0x100000, 0x40000, 0x6e6e7808 )
	ROM_LOAD( "gf_020.rom",      0x200000, 0x40000, 0x6f5c3cb0 )
	ROM_LOAD( "gf_030.rom",      0x300000, 0x40000, 0x18978a9f )

	ROM_REGION( 0x20000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "gf-da.rom",	 0x000000, 0x020000, 0x933ba935 )
ROM_END

ROM_START( gunforcu )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "gf_h0-d.5m",  0x000000, 0x20000, 0xa6db7b5c )
	ROM_LOAD_V20_ODD ( "gf_l0-d.5f",  0x000000, 0x20000, 0x82cf55f6 )
	ROM_LOAD_V20_EVEN( "gf_h1-d.5l",  0x040000, 0x20000, 0x08a3736c )
	ROM_LOAD_V20_ODD ( "gf_l1-d.5j",  0x040000, 0x20000, 0x435f524f )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "gf_sh0.rom",0x000000, 0x010000, 0x3f8f16e0 )
	ROM_LOAD_V20_ODD ( "gf_sl0.rom",0x000000, 0x010000, 0xdb0b13a3 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "gf_c0.rom",       0x000000, 0x40000, 0xb3b74979 )
	ROM_LOAD( "gf_c1.rom",       0x080000, 0x40000, 0xf5c8590a )
	ROM_LOAD( "gf_c2.rom",       0x100000, 0x40000, 0x30f9fb64 )
	ROM_LOAD( "gf_c3.rom",       0x180000, 0x40000, 0x87b3e621 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "gf_000.rom",      0x000000, 0x40000, 0x209e8e8d )
	ROM_LOAD( "gf_010.rom",      0x100000, 0x40000, 0x6e6e7808 )
	ROM_LOAD( "gf_020.rom",      0x200000, 0x40000, 0x6f5c3cb0 )
	ROM_LOAD( "gf_030.rom",      0x300000, 0x40000, 0x18978a9f )

	ROM_REGION( 0x20000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "gf-da.rom",	 0x000000, 0x020000, 0x933ba935 )
ROM_END

ROM_START( inthunt )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ith-h0-d.rom",0x000000, 0x040000, 0x52f8e7a6 )
	ROM_LOAD_V20_ODD ( "ith-l0-d.rom",0x000000, 0x040000, 0x5db79eb7 )
	ROM_LOAD_V20_EVEN( "ith-h1-b.rom",0x080000, 0x020000, 0xfc2899df )
	ROM_LOAD_V20_ODD ( "ith-l1-b.rom",0x080000, 0x020000, 0x955a605a )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* Irem D8000011A1 */
	ROM_LOAD_V20_EVEN( "ith-sh0.rom",0x000000, 0x010000, 0x209c8b7f )
	ROM_LOAD_V20_ODD ( "ith-sl0.rom",0x000000, 0x010000, 0x18472d65 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "ith_ic26.rom",0x000000, 0x080000, 0x4c1818cf )
	ROM_LOAD( "ith_ic25.rom",0x080000, 0x080000, 0x91145bae )
	ROM_LOAD( "ith_ic24.rom",0x100000, 0x080000, 0xfc03fe3b )
	ROM_LOAD( "ith_ic23.rom",0x180000, 0x080000, 0xee156a0a )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "ith_ic34.rom",0x000000, 0x100000, 0xa019766e )
	ROM_LOAD( "ith_ic35.rom",0x100000, 0x100000, 0x3fca3073 )
	ROM_LOAD( "ith_ic36.rom",0x200000, 0x100000, 0x20d1b28b )
	ROM_LOAD( "ith_ic37.rom",0x300000, 0x100000, 0x90b6fd4b )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "ith_ic9.rom" ,0x000000, 0x080000, 0x318ee71a )
ROM_END

ROM_START( inthuntu )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ithhoc.bin",0x000000, 0x040000, 0x563dcec0 )
	ROM_LOAD_V20_ODD ( "ithloc.bin",0x000000, 0x040000, 0x1638c705 )
	ROM_LOAD_V20_EVEN( "ithh1a.bin",0x080000, 0x020000, 0x0253065f )
	ROM_LOAD_V20_ODD ( "ithl1a.bin",0x080000, 0x020000, 0xa57d688d )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* Irem D8000011A1 */
	ROM_LOAD_V20_EVEN( "ith-sh0.rom",0x000000, 0x010000, 0x209c8b7f )
	ROM_LOAD_V20_ODD ( "ith-sl0.rom",0x000000, 0x010000, 0x18472d65 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "ith_ic26.rom",0x000000, 0x080000, 0x4c1818cf )
	ROM_LOAD( "ith_ic25.rom",0x080000, 0x080000, 0x91145bae )
	ROM_LOAD( "ith_ic24.rom",0x100000, 0x080000, 0xfc03fe3b )
	ROM_LOAD( "ith_ic23.rom",0x180000, 0x080000, 0xee156a0a )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "ith_ic34.rom",0x000000, 0x100000, 0xa019766e )
	ROM_LOAD( "ith_ic35.rom",0x100000, 0x100000, 0x3fca3073 )
	ROM_LOAD( "ith_ic36.rom",0x200000, 0x100000, 0x20d1b28b )
	ROM_LOAD( "ith_ic37.rom",0x300000, 0x100000, 0x90b6fd4b )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "ith_ic9.rom" ,0x000000, 0x080000, 0x318ee71a )
ROM_END

ROM_START( kaiteids )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ith-h0j.bin",0x000000, 0x040000, 0xdc1dec36 )
	ROM_LOAD_V20_ODD ( "ith-l0j.bin",0x000000, 0x040000, 0x8835d704 )
	ROM_LOAD_V20_EVEN( "ith-h1j.bin",0x080000, 0x020000, 0x5a7b212d )
	ROM_LOAD_V20_ODD ( "ith-l1j.bin",0x080000, 0x020000, 0x4c084494 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* Irem D8000011A1 */
	ROM_LOAD_V20_EVEN( "ith-sh0.rom",0x000000, 0x010000, 0x209c8b7f )
	ROM_LOAD_V20_ODD ( "ith-sl0.rom",0x000000, 0x010000, 0x18472d65 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "ith_ic26.rom",0x000000, 0x080000, 0x4c1818cf )
	ROM_LOAD( "ith_ic25.rom",0x080000, 0x080000, 0x91145bae )
	ROM_LOAD( "ith_ic24.rom",0x100000, 0x080000, 0xfc03fe3b )
	ROM_LOAD( "ith_ic23.rom",0x180000, 0x080000, 0xee156a0a )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "ith_ic34.rom",0x000000, 0x100000, 0xa019766e )
	ROM_LOAD( "ith_ic35.rom",0x100000, 0x100000, 0x3fca3073 )
	ROM_LOAD( "ith_ic36.rom",0x200000, 0x100000, 0x20d1b28b )
	ROM_LOAD( "ith_ic37.rom",0x300000, 0x100000, 0x90b6fd4b )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "ith_ic9.rom" ,0x000000, 0x080000, 0x318ee71a )
ROM_END

ROM_START( hook )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "h-h0-d.rom",0x000000, 0x040000, 0x40189ff6 )
	ROM_LOAD_V20_ODD ( "h-l0-d.rom",0x000000, 0x040000, 0x14567690 )
	ROM_LOAD_V20_EVEN( "h-h1.rom",  0x080000, 0x020000, 0x264ba1f0 )
	ROM_LOAD_V20_ODD ( "h-l1.rom",  0x080000, 0x020000, 0xf9913731 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "h-sh0.rom",0x000000, 0x010000, 0x86a4e56e )
	ROM_LOAD_V20_ODD ( "h-sl0.rom",0x000000, 0x010000, 0x10fd9676 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "hook-c0.rom",0x000000, 0x040000, 0xdec63dcf )
	ROM_LOAD( "hook-c1.rom",0x080000, 0x040000, 0xe4eb0b92 )
	ROM_LOAD( "hook-c2.rom",0x100000, 0x040000, 0xa52b320b )
	ROM_LOAD( "hook-c3.rom",0x180000, 0x040000, 0x7ef67731 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "hook-000.rom",0x000000, 0x100000, 0xccceac30 )
	ROM_LOAD( "hook-010.rom",0x100000, 0x100000, 0x8ac8da67 )
	ROM_LOAD( "hook-020.rom",0x200000, 0x100000, 0x8847af9a )
	ROM_LOAD( "hook-030.rom",0x300000, 0x100000, 0x239e877e )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "hook-da.rom" ,0x000000, 0x080000, 0x88cd0212 )
ROM_END

ROM_START( hooku )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "h0-c.3h",0x000000, 0x040000, 0x84cc239e )
	ROM_LOAD_V20_ODD ( "l0-c.5h",0x000000, 0x040000, 0x45e194fe )
	ROM_LOAD_V20_EVEN( "h-h1.rom",  0x080000, 0x020000, 0x264ba1f0 )
	ROM_LOAD_V20_ODD ( "h-l1.rom",  0x080000, 0x020000, 0xf9913731 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "h-sh0.rom",0x000000, 0x010000, 0x86a4e56e )
	ROM_LOAD_V20_ODD ( "h-sl0.rom",0x000000, 0x010000, 0x10fd9676 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "hook-c0.rom",0x000000, 0x040000, 0xdec63dcf )
	ROM_LOAD( "hook-c1.rom",0x080000, 0x040000, 0xe4eb0b92 )
	ROM_LOAD( "hook-c2.rom",0x100000, 0x040000, 0xa52b320b )
	ROM_LOAD( "hook-c3.rom",0x180000, 0x040000, 0x7ef67731 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "hook-000.rom",0x000000, 0x100000, 0xccceac30 )
	ROM_LOAD( "hook-010.rom",0x100000, 0x100000, 0x8ac8da67 )
	ROM_LOAD( "hook-020.rom",0x200000, 0x100000, 0x8847af9a )
	ROM_LOAD( "hook-030.rom",0x300000, 0x100000, 0x239e877e )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "hook-da.rom" ,0x000000, 0x080000, 0x88cd0212 )
ROM_END

ROM_START( rtypeleo )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "rtl-h0-d.bin", 0x000000, 0x040000, 0x3dbac89f )
	ROM_LOAD_V20_ODD ( "rtl-l0-d.bin", 0x000000, 0x040000, 0xf85a2537 )
	ROM_LOAD_V20_EVEN( "rtl-h1-d.bin", 0x080000, 0x020000, 0x352ff444 )
	ROM_LOAD_V20_ODD ( "rtl-l1-d.bin", 0x080000, 0x020000, 0xfd34ea46 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "rtl-sh0a.bin",0x000000, 0x010000, 0xe518b4e3 )
	ROM_LOAD_V20_ODD ( "rtl-sl0a.bin",0x000000, 0x010000, 0x896f0d36 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "rtl-c0.bin", 0x000000, 0x080000, 0xfb588d7c )
	ROM_LOAD( "rtl-c1.bin", 0x080000, 0x080000, 0xe5541bff )
	ROM_LOAD( "rtl-c2.bin", 0x100000, 0x080000, 0xfaa9ae27 )
	ROM_LOAD( "rtl-c3.bin", 0x180000, 0x080000, 0x3a2343f6 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "rtl-000.bin",0x000000, 0x100000, 0x82a06870 )
	ROM_LOAD( "rtl-010.bin",0x100000, 0x100000, 0x417e7a56 )
	ROM_LOAD( "rtl-020.bin",0x200000, 0x100000, 0xf9a3f3a1 )
	ROM_LOAD( "rtl-030.bin",0x300000, 0x100000, 0x03528d95 )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "rtl-da.bin" ,0x000000, 0x080000, 0xdbebd1ff )
ROM_END

ROM_START( mysticri )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "mr-h0-b.bin",  0x000000, 0x040000, 0xd529f887 )
	ROM_LOAD_V20_ODD ( "mr-l0-b.bin",  0x000000, 0x040000, 0xa457ab44 )
	ROM_LOAD_V20_EVEN( "mr-h1-b.bin",  0x080000, 0x010000, 0xe17649b9 )
	ROM_LOAD_V20_ODD ( "mr-l1-b.bin",  0x080000, 0x010000, 0xa87c62b4 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "mr-sh0.bin",0x000000, 0x010000, 0x50d335e4 )
	ROM_LOAD_V20_ODD ( "mr-sl0.bin",0x000000, 0x010000, 0x0fa32721 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "mr-c0.bin", 0x000000, 0x040000, 0x872a8fad )
	ROM_LOAD( "mr-c1.bin", 0x080000, 0x040000, 0xd2ffb27a )
	ROM_LOAD( "mr-c2.bin", 0x100000, 0x040000, 0x62bff287 )
	ROM_LOAD( "mr-c3.bin", 0x180000, 0x040000, 0xd0da62ab )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "mr-o00.bin", 0x000000, 0x080000, 0xa0f9ce16 )
	ROM_LOAD( "mr-o10.bin", 0x100000, 0x080000, 0x4e70a9e9 )
	ROM_LOAD( "mr-o20.bin", 0x200000, 0x080000, 0xb9c468fc )
	ROM_LOAD( "mr-o30.bin", 0x300000, 0x080000, 0xcc32433a )

	ROM_REGION( 0x40000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "mr-da.bin" ,0x000000, 0x040000, 0x1a11fc59 )
ROM_END

ROM_START( gunhohki )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "mr-h0.bin",  0x000000, 0x040000, 0x83352270 )
	ROM_LOAD_V20_ODD ( "mr-l0.bin",  0x000000, 0x040000, 0x9db308ae )
	ROM_LOAD_V20_EVEN( "mr-h1.bin",  0x080000, 0x010000, 0xc9532b60 )
	ROM_LOAD_V20_ODD ( "mr-l1.bin",  0x080000, 0x010000, 0x6349b520 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "mr-sh0.bin",0x000000, 0x010000, 0x50d335e4 )
	ROM_LOAD_V20_ODD ( "mr-sl0.bin",0x000000, 0x010000, 0x0fa32721 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "mr-c0.bin", 0x000000, 0x040000, 0x872a8fad )
	ROM_LOAD( "mr-c1.bin", 0x080000, 0x040000, 0xd2ffb27a )
	ROM_LOAD( "mr-c2.bin", 0x100000, 0x040000, 0x62bff287 )
	ROM_LOAD( "mr-c3.bin", 0x180000, 0x040000, 0xd0da62ab )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "mr-o00.bin", 0x000000, 0x080000, 0xa0f9ce16 )
	ROM_LOAD( "mr-o10.bin", 0x100000, 0x080000, 0x4e70a9e9 )
	ROM_LOAD( "mr-o20.bin", 0x200000, 0x080000, 0xb9c468fc )
	ROM_LOAD( "mr-o30.bin", 0x300000, 0x080000, 0xcc32433a )

	ROM_REGION( 0x40000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "mr-da.bin" ,0x000000, 0x040000, 0x1a11fc59 )
ROM_END

ROM_START( uccops )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "uc_h0.rom",  0x000000, 0x040000, 0x240aa5f7 )
	ROM_LOAD_V20_ODD ( "uc_l0.rom",  0x000000, 0x040000, 0xdf9a4826 )
	ROM_LOAD_V20_EVEN( "uc_h1.rom",  0x080000, 0x020000, 0x8d29bcd6 )
	ROM_LOAD_V20_ODD ( "uc_l1.rom",  0x080000, 0x020000, 0xa8a402d8 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "uc_sh0.rom", 0x000000, 0x010000, 0xdf90b198 )
	ROM_LOAD_V20_ODD ( "uc_sl0.rom", 0x000000, 0x010000, 0x96c11aac )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "uc_w38m.rom", 0x000000, 0x080000, 0x130a40e5 )
	ROM_LOAD( "uc_w39m.rom", 0x080000, 0x080000, 0xe42ca144 )
	ROM_LOAD( "uc_w40m.rom", 0x100000, 0x080000, 0xc2961648 )
	ROM_LOAD( "uc_w41m.rom", 0x180000, 0x080000, 0xf5334b80 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "uc_k16m.rom", 0x000000, 0x100000, 0x4a225f09 )
	ROM_LOAD( "uc_k17m.rom", 0x100000, 0x100000, 0xe4ed9a54 )
	ROM_LOAD( "uc_k18m.rom", 0x200000, 0x100000, 0xa626eb12 )
	ROM_LOAD( "uc_k19m.rom", 0x300000, 0x100000, 0x5df46549 )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "uc_w42.rom", 0x000000, 0x080000, 0xd17d3fd6 )
ROM_END

ROM_START( uccopsj )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "uca-h0.bin", 0x000000, 0x040000, 0x9e17cada )
	ROM_LOAD_V20_ODD ( "uca-l0.bin", 0x000000, 0x040000, 0x4a4e3208 )
	ROM_LOAD_V20_EVEN( "uca-h1.bin", 0x080000, 0x020000, 0x83f78dea )
	ROM_LOAD_V20_ODD ( "uca-l1.bin", 0x080000, 0x020000, 0x19628280 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD_V20_EVEN( "uca-sh0.bin", 0x000000, 0x010000, 0xf0ca1b03 )
	ROM_LOAD_V20_ODD ( "uca-sl0.bin", 0x000000, 0x010000, 0xd1661723 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "uca-c0.bin", 0x000000, 0x080000, 0x6a419a36 )
	ROM_LOAD( "uca-c1.bin", 0x080000, 0x080000, 0xd703ecc7 )
	ROM_LOAD( "uca-c2.bin", 0x100000, 0x080000, 0x96397ac6 )
	ROM_LOAD( "uca-c3.bin", 0x180000, 0x080000, 0x5d07d10d )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "uca-o3.bin", 0x000000, 0x100000, 0x97f7775e )
	ROM_LOAD( "uca-o2.bin", 0x100000, 0x100000, 0x5e0b1d65 )
	ROM_LOAD( "uca-o1.bin", 0x200000, 0x100000, 0xbdc224b3 )
	ROM_LOAD( "uca-o0.bin", 0x300000, 0x100000, 0x7526daec )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "uca-da.bin", 0x000000, 0x080000, 0x0b2855e9 )
ROM_END

ROM_START( lethalth )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "lt_d-h0.rom",  0x000000, 0x020000, 0x20c68935 )
	ROM_LOAD_V20_ODD ( "lt_d-l0.rom",  0x000000, 0x020000, 0xe1432fb3 )
	ROM_LOAD_V20_EVEN( "lt_d-h1.rom",  0x040000, 0x020000, 0xd7dd3d48 )
	ROM_LOAD_V20_ODD ( "lt_d-l1.rom",  0x040000, 0x020000, 0xb94b3bd8 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU */
	ROM_LOAD_V20_EVEN( "lt_d-sh0.rom",0x000000, 0x010000, 0xaf5b224f )
	ROM_LOAD_V20_ODD ( "lt_d-sl0.rom",0x000000, 0x010000, 0xcb3faac3 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "lt_7a.rom", 0x000000, 0x040000, 0xada0fd50 )
	ROM_LOAD( "lt_7b.rom", 0x080000, 0x040000, 0xd2596883 )
	ROM_LOAD( "lt_7d.rom", 0x100000, 0x040000, 0x2de637ef )
	ROM_LOAD( "lt_7h.rom", 0x180000, 0x040000, 0x9f6585cd )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "lt_7j.rom", 0x000000, 0x040000, 0xbaf8863e )
	ROM_LOAD( "lt_7l.rom", 0x100000, 0x040000, 0x40fd50af )
	ROM_LOAD( "lt_7s.rom", 0x200000, 0x040000, 0xc8e970df )
	ROM_LOAD( "lt_7y.rom", 0x300000, 0x040000, 0xf5436708 )

	ROM_REGION( 0x40000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "lt_8a.rom" ,0x000000, 0x040000, 0x357762a2 )
ROM_END

ROM_START( thndblst )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "lt_d-h0j.rom", 0x000000, 0x020000, 0xdc218a18 )
	ROM_LOAD_V20_ODD ( "lt_d-l0j.rom", 0x000000, 0x020000, 0xae9a3f81 )
	ROM_LOAD_V20_EVEN( "lt_d-h1.rom",  0x040000, 0x020000, 0xd7dd3d48 )
	ROM_LOAD_V20_ODD ( "lt_d-l1.rom",  0x040000, 0x020000, 0xb94b3bd8 )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU */
	ROM_LOAD_V20_EVEN( "lt_d-sh0.rom",0x000000, 0x010000, 0xaf5b224f )
	ROM_LOAD_V20_ODD ( "lt_d-sl0.rom",0x000000, 0x010000, 0xcb3faac3 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "lt_7a.rom", 0x000000, 0x040000, 0xada0fd50 )
	ROM_LOAD( "lt_7b.rom", 0x080000, 0x040000, 0xd2596883 )
	ROM_LOAD( "lt_7d.rom", 0x100000, 0x040000, 0x2de637ef )
	ROM_LOAD( "lt_7h.rom", 0x180000, 0x040000, 0x9f6585cd )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "lt_7j.rom", 0x000000, 0x040000, 0xbaf8863e )
	ROM_LOAD( "lt_7l.rom", 0x100000, 0x040000, 0x40fd50af )
	ROM_LOAD( "lt_7s.rom", 0x200000, 0x040000, 0xc8e970df )
	ROM_LOAD( "lt_7y.rom", 0x300000, 0x040000, 0xf5436708 )

	ROM_REGION( 0x40000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "lt_8a.rom" ,0x000000, 0x040000, 0x357762a2 )
ROM_END

ROM_START( nbbatman )
	ROM_REGION( 0x180000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "a1-h0-a.34",  0x000000, 0x040000, 0x24a9b794 )
	ROM_LOAD_V20_ODD ( "a1-l0-a.31",  0x000000, 0x040000, 0x846d7716 )
	ROM_LOAD_V20_EVEN( "a1-h1-.33",   0x100000, 0x040000, 0x3ce2aab5 )
	ROM_LOAD_V20_ODD ( "a1-l1-.32",   0x100000, 0x040000, 0x116d9bcc )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU */
	ROM_LOAD_V20_EVEN( "a1-sh0-.14",0x000000, 0x010000, 0xb7fae3e6 )
	ROM_LOAD_V20_ODD ( "a1-sl0-.17",0x000000, 0x010000, 0xb26d54fc )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "lh534k0c.9",  0x000000, 0x080000, 0x314a0c6d )
	ROM_LOAD( "lh534k0e.10", 0x080000, 0x080000, 0xdc31675b )
	ROM_LOAD( "lh534k0f.11", 0x100000, 0x080000, 0xe15d8bfb )
	ROM_LOAD( "lh534k0g.12", 0x180000, 0x080000, 0x888d71a3 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "lh538393.42", 0x000000, 0x100000, 0x26cdd224 )
	ROM_LOAD( "lh538394.43", 0x100000, 0x100000, 0x4bbe94fa )
	ROM_LOAD( "lh538395.44", 0x200000, 0x100000, 0x2a533b5e )
	ROM_LOAD( "lh538396.45", 0x300000, 0x100000, 0x863a66fa )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "lh534k0k.8" ,0x000000, 0x080000, 0x735e6380 )
ROM_END

ROM_START( leaguemn )
	ROM_REGION( 0x180000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "lma1-h0.34",  0x000000, 0x040000, 0x47c54204 )
	ROM_LOAD_V20_ODD ( "lma1-l0.31",  0x000000, 0x040000, 0x1d062c82 )
	ROM_LOAD_V20_EVEN( "a1-h1-.33",   0x100000, 0x040000, 0x3ce2aab5 )
	ROM_LOAD_V20_ODD ( "a1-l1-.32",   0x100000, 0x040000, 0x116d9bcc )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU */
	ROM_LOAD_V20_EVEN( "a1-sh0-.14",0x000000, 0x010000, 0xb7fae3e6 )
	ROM_LOAD_V20_ODD ( "a1-sl0-.17",0x000000, 0x010000, 0xb26d54fc )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "lh534k0c.9",  0x000000, 0x080000, 0x314a0c6d )
	ROM_LOAD( "lh534k0e.10", 0x080000, 0x080000, 0xdc31675b )
	ROM_LOAD( "lh534k0f.11", 0x100000, 0x080000, 0xe15d8bfb )
	ROM_LOAD( "lh534k0g.12", 0x180000, 0x080000, 0x888d71a3 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD( "lh538393.42", 0x000000, 0x100000, 0x26cdd224 )
	ROM_LOAD( "lh538394.43", 0x100000, 0x100000, 0x4bbe94fa )
	ROM_LOAD( "lh538395.44", 0x200000, 0x100000, 0x2a533b5e )
	ROM_LOAD( "lh538396.45", 0x300000, 0x100000, 0x863a66fa )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "lh534k0k.8" ,0x000000, 0x080000, 0x735e6380 )
ROM_END

ROM_START( psoldier )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "f3_h0d.h0",  0x000000, 0x040000, 0x38f131fd )
	ROM_LOAD_V20_ODD ( "f3_l0d.l0",  0x000000, 0x040000, 0x1662969c )
	ROM_LOAD_V20_EVEN( "f3_h1.h1",   0x080000, 0x040000, 0xc8d1947c )
	ROM_LOAD_V20_ODD ( "f3_l1.l1",   0x080000, 0x040000, 0x7b9492fc )

	ROM_REGION( 0x100000, REGION_CPU2 )	/* 1MB for the audio CPU */
	ROM_LOAD_V20_EVEN( "f3_sh0.sh0",0x000000, 0x010000, 0x90b55e5e )
	ROM_LOAD_V20_ODD ( "f3_sl0.sl0",0x000000, 0x010000, 0x77c16d57 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE ) /* Tiles */
	ROM_LOAD( "f3_w50.c0",  0x000000, 0x040000, 0x47e788ee )
	ROM_LOAD( "f3_w51.c1",  0x080000, 0x040000, 0x8e535e3f )
	ROM_LOAD( "f3_w52.c2",  0x100000, 0x040000, 0xa6eb2e56 )
	ROM_LOAD( "f3_w53.c3",  0x180000, 0x040000, 0x2f992807 )

	ROM_REGION( 0x800000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* Sprites */
	ROM_LOAD_GFX_EVEN( "f3_w37.000", 0x000000, 0x100000, 0xfd4cda03 )
	ROM_LOAD_GFX_ODD ( "f3_w38.001", 0x000000, 0x100000, 0x755bab10 )
	ROM_LOAD_GFX_EVEN( "f3_w39.010", 0x200000, 0x100000, 0xb21ced92 )
	ROM_LOAD_GFX_ODD ( "f3_w40.011", 0x200000, 0x100000, 0x2e906889 )
	ROM_LOAD_GFX_EVEN( "f3_w41.020", 0x400000, 0x100000, 0x02455d10 )
	ROM_LOAD_GFX_ODD ( "f3_w42.021", 0x400000, 0x100000, 0x124589b9 )
	ROM_LOAD_GFX_EVEN( "f3_w43.030", 0x600000, 0x100000, 0xdae7327a )
	ROM_LOAD_GFX_ODD ( "f3_w44.031", 0x600000, 0x100000, 0xd0fc84ac )

	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "f3_w95.da" ,0x000000, 0x080000, 0xf7ca432b )
ROM_END

/***************************************************************************/

static READ_HANDLER( lethalth_cycle_r )
{
	if (cpu_get_pc()==0x1f4 && m92_ram[0x1e]==2 && offset==0)
		cpu_spinuntil_int();

	return m92_ram[0x1e + offset];
}

static READ_HANDLER( hook_cycle_r )
{
	if (cpu_get_pc()==0x55ba && m92_ram[0x12]==0 && m92_ram[0x13]==0 && offset==0)
		cpu_spinuntil_int();

	return m92_ram[0x12 + offset];
}

static READ_HANDLER( psoldier_cycle_r )
{
	int a=m92_ram[0]+(m92_ram[1]<<8);
	int b=m92_ram[0x1aec]+(m92_ram[0x1aed]<<8);
	int c=m92_ram[0x1aea]+(m92_ram[0x1aeb]<<8);

	if (cpu_get_pc()==0x2dae && b!=a && c!=a && offset==0)
		cpu_spinuntil_int();

	return m92_ram[0x1aec + offset];
}

static READ_HANDLER( inthunt_cycle_r )
{
	int d=cpu_geticount();
	int line = 256 - cpu_getiloops();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000 && line<247) {
		if (cpu_get_pc()==0x858 && m92_ram[0x25f]==0 && offset==1) {
			/* Adjust in-game counter, based on cycles left to run */
			int old;

			old=m92_ram[0xb892]+(m92_ram[0xb893]<<8);
			old=(old+d/82)&0xffff; /* 82 cycles per increment */
			m92_ram[0xb892]=old&0xff;
			m92_ram[0xb893]=old>>8;

			cpu_spinuntil_int();
		}
	}

	return m92_ram[0x25e + offset];
}

static READ_HANDLER( uccops_cycle_r )
{
	int a=m92_ram[0x3f28]+(m92_ram[0x3f29]<<8);
	int b=m92_ram[0x3a00]+(m92_ram[0x3a01]<<8);
	int c=m92_ram[0x3a02]+(m92_ram[0x3a03]<<8);
	int d=cpu_geticount();
	int line = 256 - cpu_getiloops();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000 && line<247) {
		if ((cpu_get_pc()==0x900ff || cpu_get_pc()==0x90103) && b==c && offset==1) {
			cpu_spinuntil_int();
			/* Update internal counter based on cycles left to run */
			a=(a+d/127)&0xffff; /* 127 cycles per loop increment */
			m92_ram[0x3f28]=a&0xff;
			m92_ram[0x3f29]=a>>8;
		}
	}

	return m92_ram[0x3a02 + offset];
}

static READ_HANDLER( rtypeleo_cycle_r )
{
	if (cpu_get_pc()==0x307a3 && offset==0 && m92_ram[0x32]==2 && m92_ram[0x33]==0)
		cpu_spinuntil_int();

	return m92_ram[0x32 + offset];
}

static READ_HANDLER( gunforce_cycle_r )
{
	int a=m92_ram[0x6542]+(m92_ram[0x6543]<<8);
	int b=m92_ram[0x61d0]+(m92_ram[0x61d1]<<8);
	int d=cpu_geticount();
	int line = 256 - cpu_getiloops();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000 && line<247) {
		if (cpu_get_pc()==0x40a && ((b&0x8000)==0) && offset==1) {
			cpu_spinuntil_int();
			/* Update internal counter based on cycles left to run */
			a=(a+d/80)&0xffff; /* 80 cycles per loop increment */
			m92_ram[0x6542]=a&0xff;
			m92_ram[0x6543]=a>>8;
		}
	}

	return m92_ram[0x61d0 + offset];
}

/***************************************************************************/

static void m92_startup(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	memcpy(RAM+0xffff0,RAM+0x7fff0,0x10); /* Start vector */
	cpu_setbank(1,&RAM[0xa0000]); /* Initial bank */

	/* Mirror used by In The Hunt for protection */
	memcpy(RAM+0xc0000,RAM+0x00000,0x10000);
	cpu_setbank(2,&RAM[0xc0000]);

	RAM = memory_region(REGION_CPU2);
	memcpy(RAM+0xffff0,RAM+0x1fff0,0x10); /* Sound cpu Start vector */

	m92_irq_vectorbase=0x80;
	m92_raster_enable=1;
	m92_spritechip=0;
	m92_game_kludge=0;
}

static void init_m92(void)
{
/*	m92_sound_decrypt();*/	/* Someday.. */
	m92_startup();
}

static void init_bmaster(void)
{
	init_m92();
}

static void init_gunforce(void)
{
	install_mem_read_handler(0, 0xe61d0, 0xe61d1, gunforce_cycle_r);
	init_m92();
}

static void init_hook(void)
{
	install_mem_read_handler(0, 0xe0012, 0xe0013, hook_cycle_r);
	init_m92();
}

static void init_mysticri(void)
{
	init_m92();
	m92_spritechip=1;
}

static void init_uccops(void)
{
	install_mem_read_handler(0, 0xe3a02, 0xe3a03, uccops_cycle_r);
	init_m92();
	m92_spritechip=1;
}

static void init_rtypeleo(void)
{
	install_mem_read_handler(0, 0xe0032, 0xe0033, rtypeleo_cycle_r);
	init_m92();
	m92_game_kludge=1;
	m92_spritechip=1;
	m92_irq_vectorbase=0x20; /* M92-A-B motherboard */
}

static void init_majtitl2(void)
{
	init_m92();
}

static void init_inthunt(void)
{
	install_mem_read_handler(0, 0xe025e, 0xe025f, inthunt_cycle_r);
	init_m92();
}

static void init_lethalth(void)
{
	install_mem_read_handler(0, 0xe001e, 0xe001f, lethalth_cycle_r);
	init_m92();
	m92_spritechip=1;
	m92_irq_vectorbase=0x20; /* M92-A-B motherboard */

	/* This game sets the raster IRQ position, but the interrupt routine
		is just an iret, no need to emulate it */
	m92_raster_enable=0;
}

static void init_nbbatman(void)
{
	init_m92();
	m92_game_kludge=2;
	m92_spritechip=1;
}

static void init_psoldier(void)
{
	install_mem_read_handler(0, 0xe1aec, 0xe1aed, psoldier_cycle_r);
	init_m92();
	m92_spritechip=1;
	m92_irq_vectorbase=0x20; /* M92-A-B motherboard */
}

/***************************************************************************/

/* The 'nonraster' machine is for games that don't use the raster interrupt feature - slightly faster to emulate */
GAMEX( 1991, bmaster,  0,        nonraster, bmaster,  bmaster,  ROT0,       "Irem",         "Blade Master (World)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1991, lethalth, 0,        lethalth,  lethalth, lethalth, ROT270,     "Irem",         "Lethal Thunder (World)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1991, thndblst, lethalth, lethalth,  lethalth, lethalth, ROT270,     "Irem",         "Thunder Blaster (Japan)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1991, gunforce, 0,        raster,    gunforce, gunforce, ROT0,       "Irem",         "Gunforce - Battle Fire Engulfed Terror Island (World)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1991, gunforcu, gunforce, raster,    gunforce, gunforce, ROT0,       "Irem America", "Gunforce - Battle Fire Engulfed Terror Island (US)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, hook,     0,        nonraster, hook,     hook,     ROT0,       "Irem",         "Hook (World)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, hooku,    hook,     nonraster, hook,     hook,     ROT0,       "Irem America", "Hook (US)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, mysticri, 0,        nonraster, mysticri, mysticri, ROT0,       "Irem",         "Mystic Riders (World)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, gunhohki, mysticri, nonraster, mysticri, mysticri, ROT0,       "Irem",         "Gun Hohki (Japan)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, uccops,   0,        raster,    uccops,   uccops,   ROT0_16BIT, "Irem",         "Undercover Cops (World)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, uccopsj,  uccops,   raster,    uccops,   uccops,   ROT0_16BIT, "Irem",         "Undercover Cops (Japan)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, rtypeleo, 0,        raster,    rtypeleo, rtypeleo, ROT0_16BIT, "Irem",         "R-Type Leo (Japan)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, majtitl2, 0,        raster,    majtitl2, majtitl2, ROT0_16BIT, "Irem",         "Major Title 2 (World)", GAME_NO_SOUND | GAME_IMPERFECT_COLORS | GAME_NO_COCKTAIL )
GAMEX( 1992, skingame, majtitl2, raster,    majtitl2, majtitl2, ROT0_16BIT, "Irem America", "The Irem Skins Game (US set 1)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1992, skingam2, majtitl2, raster,    majtitl2, majtitl2, ROT0_16BIT, "Irem America", "The Irem Skins Game (US set 2)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1993, inthunt,  0,        raster,    inthunt,  inthunt,  ROT0_16BIT, "Irem",         "In The Hunt (World)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1993, inthuntu, inthunt,  raster,    inthunt,  inthunt,  ROT0_16BIT, "Irem America", "In The Hunt (US)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1993, kaiteids, inthunt,  raster,    inthunt,  inthunt,  ROT0_16BIT, "Irem",         "Kaitei Daisensou (Japan)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1993, nbbatman, 0,        raster,    nbbatman, nbbatman, ROT0,       "Irem America", "Ninja Baseball Batman (US)", GAME_NO_SOUND | GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1993, leaguemn, nbbatman, raster,    nbbatman, nbbatman, ROT0,       "Irem",         "Yakyuu Kakutou League-Man (Japan)", GAME_NO_SOUND | GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1993, psoldier, 0,        psoldier,  psoldier, psoldier, ROT0_16BIT, "Irem",         "Perfect Soldiers (Japan)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
