#include "../vidhrdw/cninja.cpp"

/***************************************************************************

  Edward Randy      (c) 1990 Data East Corporation (World version)
  Edward Randy      (c) 1990 Data East Corporation (Japanese version)
  Caveman Ninja     (c) 1991 Data East Corporation (World version)
  Caveman Ninja     (c) 1991 Data East Corporation (USA version)
  Joe & Mac         (c) 1991 Data East Corporation (Japanese version)
  Stone Age         (Italian bootleg)

  Edward Randy runs on the same board as Caveman Ninja but one custom chip
  is different.  The chip controls protection and also seems to affect the
  memory map.


Sound (Original):
  Hu6280 CPU
  YM2151 (music)
  YM2203C (effects)
  2 Oki ADPCM chips

Sound (Bootleg):
  Z80 (Program ripped from Block Out!)
  YM2151
  Oki chip

Caveman Ninja Issues:
  End of level 2 is corrupt.

  Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/h6280/h6280.h"
#include "cpu/z80/z80.h"

/* Video emulation definitions */
extern unsigned char *cninja_pf1_rowscroll,*cninja_pf2_rowscroll;
extern unsigned char *cninja_pf3_rowscroll,*cninja_pf4_rowscroll;
extern unsigned char *cninja_pf1_data,*cninja_pf2_data;
extern unsigned char *cninja_pf3_data,*cninja_pf4_data;

int  cninja_vh_start(void);
int  edrandy_vh_start(void);
int  stoneage_vh_start(void);
void cninja_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

READ_HANDLER( cninja_pf3_rowscroll_r );
READ_HANDLER( cninja_pf1_data_r );

WRITE_HANDLER( cninja_pf1_data_w );
WRITE_HANDLER( cninja_pf2_data_w );
WRITE_HANDLER( cninja_pf3_data_w );
WRITE_HANDLER( cninja_pf4_data_w );
WRITE_HANDLER( cninja_control_0_w );
WRITE_HANDLER( cninja_control_1_w );

WRITE_HANDLER( cninja_pf1_rowscroll_w );
WRITE_HANDLER( cninja_pf2_rowscroll_w );
WRITE_HANDLER( cninja_pf3_rowscroll_w );
WRITE_HANDLER( cninja_pf4_rowscroll_w );

WRITE_HANDLER( cninja_palette_24bit_w );

static int loopback[0x100];
static unsigned char *cninja_ram;

/**********************************************************************************/

static WRITE_HANDLER( cninja_sound_w )
{
	soundlatch_w(0,data&0xff);
	cpu_cause_interrupt(1,H6280_INT_IRQ1);
}

static WRITE_HANDLER( stoneage_sound_w )
{
	soundlatch_w(0,data&0xff);
	cpu_cause_interrupt(1,Z80_NMI_INT);
}

static WRITE_HANDLER( cninja_loopback_w )
{
	COMBINE_WORD_MEM(&loopback[offset],data);
#if 0
	if ((offset>0x22 || offset<0x8) && (offset>0x94 || offset<0x80)
&& offset!=0x36 && offset!=0x9e && offset!=0x76 && offset!=0x58 && offset!=0x56
&& offset!=0x2c && offset!=0x34
&& (offset>0xb0 || offset<0xa0) /* in game prot writes */
)
logerror("Protection PC %06x: warning - write %04x to %04x\n",cpu_get_pc(),data,offset);
#endif
}

static READ_HANDLER( cninja_prot_r )
{
 	switch (offset) {
		case 0x80: /* Master level control */
			return READ_WORD(&loopback[0]);

		case 0xde: /* Restart position control */
			return READ_WORD(&loopback[2]);

		case 0xe6: /* The number of credits in the system. */
			return READ_WORD(&loopback[4]);

		case 0x86: /* End of game check.  See 0x1814 */
			return READ_WORD(&loopback[6]);

		/* Video registers */
		case 0x5a: /* Moved to 0x140000 on int */
			return READ_WORD(&loopback[0x10]);
		case 0x84: /* Moved to 0x14000a on int */
			return READ_WORD(&loopback[0x12]);
		case 0x20: /* Moved to 0x14000c on int */
			return READ_WORD(&loopback[0x14]);
		case 0x72: /* Moved to 0x14000e on int */
			return READ_WORD(&loopback[0x16]);
		case 0xdc: /* Moved to 0x150000 on int */
			return READ_WORD(&loopback[0x18]);
		case 0x6e: /* Moved to 0x15000a on int */
			return READ_WORD(&loopback[0x1a]); /* Not used on bootleg */
		case 0x6c: /* Moved to 0x15000c on int */
			return READ_WORD(&loopback[0x1c]);
		case 0x08: /* Moved to 0x15000e on int */
			return READ_WORD(&loopback[0x1e]);

		case 0x36: /* Dip switches */
			return (readinputport(3) + (readinputport(4) << 8));

		case 0x1c8: /* Coins */
			return readinputport(2);

		case 0x22c: /* Player 1 & 2 input ports */
			return (readinputport(0) + (readinputport(1) << 8));
	}
	//logerror("Protection PC %06x: warning - read unmapped memory address %04x\n",cpu_get_pc(),offset);
	return 0;
}

static READ_HANDLER( edrandy_prot_r )
{
 	switch (offset) {
		/* Video registers */
		case 0x32a: /* Moved to 0x140006 on int */
			return READ_WORD(&loopback[0x80]);
		case 0x380: /* Moved to 0x140008 on int */
			return READ_WORD(&loopback[0x84]);
		case 0x63a: /* Moved to 0x150002 on int */
			return READ_WORD(&loopback[0x88]);
		case 0x42a: /* Moved to 0x150004 on int */
			return READ_WORD(&loopback[0x8c]);
		case 0x030: /* Moved to 0x150006 on int */
			return READ_WORD(&loopback[0x90]);
		case 0x6b2: /* Moved to 0x150008 on int */
			return READ_WORD(&loopback[0x94]);




case 0x6c4: /* dma enable, bit 7 set, below bit 5 */
case 0x33e: return READ_WORD(&loopback[0x2c]); /* allows video registers */




		/* memcpy selectors, transfer occurs in interrupt */
		case 0x32e: return READ_WORD(&loopback[0x8]); /* src msb */
		case 0x6d8: return READ_WORD(&loopback[0xa]); /* src lsb */
		case 0x010: return READ_WORD(&loopback[0xc]); /* dst msb */
		case 0x07a: return READ_WORD(&loopback[0xe]); /* src lsb */

		case 0x37c: return READ_WORD(&loopback[0x10]); /* src msb */
		case 0x250: return READ_WORD(&loopback[0x12]);
		case 0x04e: return READ_WORD(&loopback[0x14]);
		case 0x5ba: return READ_WORD(&loopback[0x16]);
		case 0x5f4: return READ_WORD(&loopback[0x18]); /* length */

		case 0x38c: return READ_WORD(&loopback[0x1a]); /* src msb */
		case 0x02c: return READ_WORD(&loopback[0x1c]);
		case 0x1e6: return READ_WORD(&loopback[0x1e]);
		case 0x3e4: return READ_WORD(&loopback[0x20]);
		case 0x174: return READ_WORD(&loopback[0x22]); /* length */

		/* Player 1 & 2 controls, read in IRQ then written *back* to protection device */
		case 0x50: /* written to 9e byte */
			return readinputport(0);
		case 0x6f8: /* written to 76 byte */
			return readinputport(1);
		/* Controls are *really* read here! */
		case 0x6fa:
			return (READ_WORD(&loopback[0x9e])&0xff00) | ((READ_WORD(&loopback[0x76])>>8)&0xff);
		/* These two go to the low bytes of 9e and 76.. */
		case 0xc6: return 0;
		case 0x7bc: return 0;
		case 0x5c: /* After coin insert, high 0x8000 bit set starts game */
			return READ_WORD(&loopback[0x76]);
		case 0x3a6: /* Top byte OR'd with above, masked to 7 */
			return READ_WORD(&loopback[0x9e]);

//		case 0xac: /* Dip switches */

		case 0xc2: /* Dip switches */
			return (readinputport(3) + (readinputport(4) << 8));
		case 0x5d4: /* The state of the dips _last_ frame */
			return READ_WORD(&loopback[0x34]);

		case 0x76a: /* Coins */
			return readinputport(2);



case 0x156: /* Interrupt regulate */

//logerror("Int stop %04x\n",READ_WORD(&loopback[0x1a]));

cpu_spinuntil_int();
//return readinputport(2);

	/* 4058 or 4056? */
	return READ_WORD(&loopback[0x36])>>8;




#if 0

// 5d4 == 80 when coin is inserted

// 284 == LOOKUP TABLE IMPROTANT (0 4 8 etc
case 0x284: return 0;



case 0x2f6: /* btst 2, jsr 11a if set */
if (READ_WORD(&loopback[0x40])) return 0xffff;
return 0;//READ_WORD(&loopback[0x40]);//0;


//case 0x5d4:
//case 0xc2: return READ_WORD(&loopback[0x40]);//0; /* Screen flip related?! */




//case 0x33e:
//case 0x6c4:
//	case 0x6f8: /* Player 1 & 2 input ports */


// return value 36 is important


/*

6006 written to 0x40 at end of japan screen


*/



//case 0xc2:
//case 0x5c: /* 9cde */
//case 0x5d4:
//return READ_WORD(&loopback[0x40]);//0;

//case 0x6fa:





/* in game prot */
case 0x102: return READ_WORD(&loopback[0xa0]);
case 0x15a: return READ_WORD(&loopback[0xa2]);
case 0x566: return READ_WORD(&loopback[0xa4]);
case 0xd2: return READ_WORD(&loopback[0xa6]);
case 0x4a6: return READ_WORD(&loopback[0xa8]);
case 0x3dc: return READ_WORD(&loopback[0xb0]);
case 0x2a0: return READ_WORD(&loopback[0xb2]);
//case 0x392: return READ_WORD(&loopback[0xa0]);


		case 0x3b2: return READ_WORD(&loopback[0xd0]);

/* Enemy power related HIGH WORD*/
		case 0x440: return READ_WORD(&loopback[0xd2]);/* Enemy power related LOW WORD*/
			return 0;

//case 0x6fa:
//return rand()%0xffff;

//case 0x5d4: /* Top byte:  Player 1?  lsl.4 then mask 3 for fire buttons? */
//case 0x5d4:

		return 0xffff;//(readinputport(0) + (readinputport(1) << 8));
#endif
	}

//	logerror("Protection PC %06x: warning - read unmapped memory address %04x\n",cpu_get_pc(),offset);
	return 0;
}

#if 0
static WRITE_HANDLER( log_m_w )
{
	logerror("INTERRUPT %06x: warning - write address %04x\n",cpu_get_pc(),offset);

}
#endif

/**********************************************************************************/

static struct MemoryReadAddress cninja_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x144000, 0x144fff, cninja_pf1_data_r },
	{ 0x15c000, 0x15c7ff, cninja_pf3_rowscroll_r },
	{ 0x184000, 0x187fff, MRA_BANK1 },
	{ 0x190004, 0x190005, MRA_NOP }, /* Seemingly unused */
	{ 0x19c000, 0x19dfff, paletteram_word_r },
	{ 0x1a4000, 0x1a47ff, MRA_BANK2 }, /* Sprites */
	{ 0x1bc000, 0x1bcfff, cninja_prot_r }, /* Protection device */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cninja_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },

	{ 0x140000, 0x14000f, cninja_control_1_w },
	{ 0x144000, 0x144fff, cninja_pf1_data_w, &cninja_pf1_data },
	{ 0x146000, 0x146fff, cninja_pf4_data_w, &cninja_pf4_data },
	{ 0x14c000, 0x14c7ff, cninja_pf1_rowscroll_w, &cninja_pf1_rowscroll },
	{ 0x14e000, 0x14e7ff, cninja_pf4_rowscroll_w, &cninja_pf4_rowscroll },

	{ 0x150000, 0x15000f, cninja_control_0_w },
	{ 0x154000, 0x154fff, cninja_pf3_data_w, &cninja_pf3_data },
	{ 0x156000, 0x156fff, cninja_pf2_data_w, &cninja_pf2_data },
	{ 0x15c000, 0x15c7ff, cninja_pf3_rowscroll_w, &cninja_pf3_rowscroll },
	{ 0x15e000, 0x15e7ff, cninja_pf2_rowscroll_w, &cninja_pf2_rowscroll },

	{ 0x184000, 0x187fff, MWA_BANK1, &cninja_ram }, /* Main ram */
	{ 0x190000, 0x190007, MWA_NOP }, /* IRQ Ack + DMA flags? */
	{ 0x19c000, 0x19dfff, cninja_palette_24bit_w, &paletteram },
	{ 0x1a4000, 0x1a47ff, MWA_BANK2, &spriteram, &spriteram_size },
	{ 0x1b4000, 0x1b4001, buffer_spriteram_w }, /* DMA flag */
	{ 0x1bc000, 0x1bc0ff, cninja_loopback_w }, /* Protection writes */
	{ 0x308000, 0x308fff, MWA_NOP }, /* Bootleg only */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress edrandy_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },
	{ 0x144000, 0x144fff, cninja_pf1_data_r },
	{ 0x15c000, 0x15c7ff, cninja_pf3_rowscroll_r },
	{ 0x188000, 0x189fff, paletteram_word_r },
	{ 0x194000, 0x197fff, MRA_BANK1 },
	{ 0x198000, 0x198fff, edrandy_prot_r }, /* Protection device */
	{ 0x1bc000, 0x1bc7ff, MRA_BANK2 }, /* Sprites */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress edrandy_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },

	{ 0x140000, 0x14000f, cninja_control_1_w },
	{ 0x144000, 0x144fff, cninja_pf1_data_w, &cninja_pf1_data },
	{ 0x146000, 0x146fff, cninja_pf4_data_w, &cninja_pf4_data },
	{ 0x14c000, 0x14c7ff, cninja_pf1_rowscroll_w, &cninja_pf1_rowscroll },
	{ 0x14e000, 0x14e7ff, cninja_pf4_rowscroll_w, &cninja_pf4_rowscroll },

	{ 0x150000, 0x15000f, cninja_control_0_w },
	{ 0x154000, 0x154fff, cninja_pf3_data_w, &cninja_pf3_data },
	{ 0x156000, 0x156fff, cninja_pf2_data_w, &cninja_pf2_data },
	{ 0x15c000, 0x15c7ff, cninja_pf3_rowscroll_w, &cninja_pf3_rowscroll },
	{ 0x15e000, 0x15e7ff, cninja_pf2_rowscroll_w, &cninja_pf2_rowscroll },

	{ 0x188000, 0x189fff, cninja_palette_24bit_w, &paletteram },
	{ 0x194000, 0x197fff, MWA_BANK1, &cninja_ram }, /* Main ram */
	{ 0x198064, 0x198065, cninja_sound_w }, /* Soundlatch is amongst protection */
	{ 0x198000, 0x1980ff, cninja_loopback_w }, /* Protection writes */
	{ 0x1a4000, 0x1a4007, MWA_NOP }, /* IRQ Ack + DMA flags? */
	{ 0x1ac000, 0x1ac001, buffer_spriteram_w }, /* DMA flag */
	{ 0x1bc000, 0x1bc7ff, MWA_BANK2, &spriteram, &spriteram_size },
	{ -1 }  /* end of table */
};

/******************************************************************************/

static WRITE_HANDLER( YM2151_w )
{
	switch (offset) {
	case 0:
		YM2151_register_port_0_w(0,data);
		break;
	case 1:
		YM2151_data_port_0_w(0,data);
		break;
	}
}

static WRITE_HANDLER( YM2203_w )
{
	switch (offset) {
	case 0:
		YM2203_control_port_0_w(0,data);
		break;
	case 1:
		YM2203_write_port_0_w(0,data);
		break;
	}
}

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x100000, 0x100001, YM2203_status_port_0_r },
	{ 0x110000, 0x110001, YM2151_status_port_0_r },
	{ 0x120000, 0x120001, OKIM6295_status_0_r },
	{ 0x130000, 0x130001, OKIM6295_status_1_r },
	{ 0x140000, 0x140001, soundlatch_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x100000, 0x100001, YM2203_w },
	{ 0x110000, 0x110001, YM2151_w },
	{ 0x120000, 0x120001, OKIM6295_data_0_w },
	{ 0x130000, 0x130001, OKIM6295_data_1_w },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 },
	{ 0x1fec00, 0x1fec01, H6280_timer_w },
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
    { -1 }  /* end of table */
};

static struct MemoryReadAddress stoneage_s_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8801, 0x8801, YM2151_status_port_0_r },
	{ 0xa000, 0xa000, soundlatch_r },
	{ 0x9800, 0x9800, OKIM6295_status_0_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress stoneage_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8800, YM2151_register_port_0_w },
	{ 0x8801, 0x8801, YM2151_data_port_0_w },
	{ 0x9800, 0x9800, OKIM6295_data_0_w },
	{ -1 }  /* end of table */
};

/**********************************************************************************/

#define PORTS_COINS \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 ) \
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK ) \
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )

INPUT_PORTS_START( cninja )

	PORTS_COINS

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( cninjau )

	PORTS_COINS

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Credit(s) to Start" )
	PORT_DIPSETTING(    0x80, "1" )
	PORT_DIPSETTING(    0x00, "2" )
/* Also, if Coin A and B are on 1/1, 0x00 gives 2 to start, 1 to continue */

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/**********************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	4096,
	4,		/* 4 bits per pixel  */
	{ 0x08000*8, 0x18000*8, 0x00000*8, 0x10000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,
	0x4000,  /* A lotta sprites.. */
	4,
	{ 8, 0, 0x100000*8+8, 0x100000*8 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout spritelayout2 =
{
	16,16,
	0xa000,  /* A lotta sprites.. */
	4,
	{ 8, 0, 0x280000*8+8, 0x280000*8 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout tilelayout =
{
	16,16,
	4096,
	4,
	{ 0x40000*8+8, 0x40000*8, 8, 0,  },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

/* Areas 1536-2048 & 1024-1280 seem to be always empty */
static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,    0, 16 },	/* Characters 8x8 */
	{ REGION_GFX2, 0, &tilelayout,  512, 64 },	/* Tiles 16x16 (BASE 1280) */
	{ REGION_GFX3, 0, &tilelayout,  512, 64 },	/* Tiles 16x16 */
	{ REGION_GFX4, 0, &tilelayout,  256, 16 },	/* Tiles 16x16 */
	{ REGION_GFX5, 0, &spritelayout,768, 16 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo2[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 16 },	/* Characters 8x8 */
	{ REGION_GFX2, 0, &tilelayout,   512, 64 },	/* Tiles 16x16 (BASE 1280) */
	{ REGION_GFX3, 0, &tilelayout,   512, 64 },	/* Tiles 16x16 */
	{ REGION_GFX4, 0, &tilelayout,   256, 16 },	/* Tiles 16x16 */
	{ REGION_GFX5, 0, &spritelayout2,768, 16 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

/**********************************************************************************/

static struct YM2203interface ym2203_interface =
{
	1,
	32220000/8,	/* Accurate, audio section crystal is 32.220 MHz */
	{ YM2203_VOL(40,40) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static void sound_irq(int state)
{
	cpu_set_irq_line(1,1,state); /* IRQ 2 */
}

static void sound_irq2(int state)
{
	cpu_set_irq_line(1,0,state);
}

static WRITE_HANDLER( sound_bankswitch_w )
{
	/* the second OKIM6295 ROM is bank switched */
	OKIM6295_set_bank_base(1, ALL_VOICES, (data & 1) * 0x40000);
}

static struct YM2151interface ym2151_interface =
{
	1,
	32220000/9, /* Accurate, audio section crystal is 32.220 MHz */
	{ YM3012_VOL(45,MIXER_PAN_LEFT,45,MIXER_PAN_RIGHT) },
	{ sound_irq },
	{ sound_bankswitch_w }
};

static struct YM2151interface ym2151_interface2 =
{
	1,
	3579545,	/* 3.579545 Mhz (?) */
	{ YM3012_VOL(50,MIXER_PAN_CENTER,50,MIXER_PAN_CENTER) },
	{ sound_irq2 }
};

static struct OKIM6295interface okim6295_interface =
{
	2,              /* 2 chips */
	{ 7757, 15514 },/* Frequency */
	{ REGION_SOUND1, REGION_SOUND2 },       /* memory regions 3 & 4 */
	{ 50, 25 }		/* Note!  Keep chip 1 (voices) louder than chip 2 */
};

/**********************************************************************************/

static struct MachineDriver machine_driver_cninja =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,
			cninja_readmem,cninja_writemem,0,0,
			m68_level5_irq,1
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU,
			32220000/8,	/* Accurate */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	58, 529,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	cninja_vh_start,
	0,
	cninja_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0, /* Mono */
  	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
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

static struct MachineDriver machine_driver_stoneage =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,
			cninja_readmem,cninja_writemem,0,0,
			m68_level5_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,
			stoneage_s_readmem,stoneage_s_writemem,0,0,
			ignore_interrupt,0
		}
	},
	58, 529,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	stoneage_vh_start,
	0,
	cninja_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0, /* Mono */
	{
		{
			SOUND_YM2151,
			&ym2151_interface2
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_edrandy =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,
			edrandy_readmem,edrandy_writemem,0,0,
			m68_level5_irq,1
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU,
			32220000/8,	/* Accurate */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	58, 529,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo2,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	edrandy_vh_start,
	0,
	cninja_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0, /* Mono */
  	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
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

/**********************************************************************************/

ROM_START( cninja )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "gn02rev3.bin", 0x00000, 0x20000, 0x39aea12a )
	ROM_LOAD_ODD ( "gn05rev2.bin", 0x00000, 0x20000, 0x0f4360ef )
	ROM_LOAD_EVEN( "gn01rev2.bin", 0x40000, 0x20000, 0xf740ef7e )
	ROM_LOAD_ODD ( "gn04rev2.bin", 0x40000, 0x20000, 0xc98fcb62 )
	ROM_LOAD_EVEN( "gn-00.rom",    0x80000, 0x20000, 0x0b110b16 )
	ROM_LOAD_ODD ( "gn-03.rom",    0x80000, 0x20000, 0x1e28e697 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */
	ROM_LOAD( "gl-07.rom",  0x00000,  0x10000,  0xca8bef96 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gl-08.rom",  0x00000,  0x10000,  0x33a2b400 )	/* chars */
	ROM_LOAD( "gl-09.rom",  0x10000,  0x10000,  0x5a2d4752 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-00.rom", 0x000000, 0x80000,  0xa8f05d33 )	/* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-01.rom", 0x000000, 0x80000,  0x5b399eed )	/* tiles 2 */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-02.rom", 0x000000, 0x80000,  0xde89c69a )	/* tiles 3 */

	ROM_REGION( 0x200000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-03.rom", 0x000000, 0x80000,  0x2220eb9f )	/* sprites */
	ROM_LOAD( "mag-04.rom", 0x080000, 0x80000,  0x144b94cc )
	ROM_LOAD( "mag-05.rom", 0x100000, 0x80000,  0x56a53254 )
	ROM_LOAD( "mag-06.rom", 0x180000, 0x80000,  0x82d44749 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "gl-06.rom",  0x00000,  0x20000,  0xd92e519d )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* Extra Oki samples */
	ROM_LOAD( "mag-07.rom", 0x00000,  0x80000,  0x08eb5264 )	/* banked */
ROM_END

ROM_START( cninja0 )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "gn-02.rom", 0x00000, 0x20000, 0xccc59524 )
	ROM_LOAD_ODD ( "gn-05.rom", 0x00000, 0x20000, 0xa002cbe4 )
	ROM_LOAD_EVEN( "gn-01.rom", 0x40000, 0x20000, 0x18f0527c )
	ROM_LOAD_ODD ( "gn-04.rom", 0x40000, 0x20000, 0xea4b6d53 )
	ROM_LOAD_EVEN( "gn-00.rom", 0x80000, 0x20000, 0x0b110b16 )
	ROM_LOAD_ODD ( "gn-03.rom", 0x80000, 0x20000, 0x1e28e697 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */
	ROM_LOAD( "gl-07.rom",  0x00000,  0x10000,  0xca8bef96 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gl-08.rom",  0x00000,  0x10000,  0x33a2b400 )	/* chars */
	ROM_LOAD( "gl-09.rom",  0x10000,  0x10000,  0x5a2d4752 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-00.rom", 0x000000, 0x80000,  0xa8f05d33 )	/* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-01.rom", 0x000000, 0x80000,  0x5b399eed )	/* tiles 2 */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-02.rom", 0x000000, 0x80000,  0xde89c69a )	/* tiles 3 */

	ROM_REGION( 0x200000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-03.rom", 0x000000, 0x80000,  0x2220eb9f )	/* sprites */
	ROM_LOAD( "mag-04.rom", 0x080000, 0x80000,  0x144b94cc )
	ROM_LOAD( "mag-05.rom", 0x100000, 0x80000,  0x56a53254 )
	ROM_LOAD( "mag-06.rom", 0x180000, 0x80000,  0x82d44749 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "gl-06.rom",  0x00000,  0x20000,  0xd92e519d )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* Extra Oki samples */
	ROM_LOAD( "mag-07.rom", 0x00000,  0x80000,  0x08eb5264 )	/* banked */
ROM_END

ROM_START( cninjau )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "gm02-3.1k", 0x00000, 0x20000, 0xd931c3b1 )
	ROM_LOAD_ODD ( "gm05-2.3k", 0x00000, 0x20000, 0x7417d3fb )
	ROM_LOAD_EVEN( "gm01-2.1j", 0x40000, 0x20000, 0x72041f7e )
	ROM_LOAD_ODD ( "gm04-2.3j", 0x40000, 0x20000, 0x2104d005 )
	ROM_LOAD_EVEN( "gn-00.rom", 0x80000, 0x20000, 0x0b110b16 )
	ROM_LOAD_ODD ( "gn-03.rom", 0x80000, 0x20000, 0x1e28e697 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */
	ROM_LOAD( "gl-07.rom",  0x00000,  0x10000,  0xca8bef96 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gl-08.rom",  0x00000,  0x10000,  0x33a2b400 )	/* chars */
	ROM_LOAD( "gl-09.rom",  0x10000,  0x10000,  0x5a2d4752 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-00.rom", 0x000000, 0x80000,  0xa8f05d33 )	/* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-01.rom", 0x000000, 0x80000,  0x5b399eed )	/* tiles 2 */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-02.rom", 0x000000, 0x80000,  0xde89c69a )	/* tiles 3 */

	ROM_REGION( 0x200000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-03.rom", 0x000000, 0x80000,  0x2220eb9f )	/* sprites */
	ROM_LOAD( "mag-04.rom", 0x080000, 0x80000,  0x144b94cc )
	ROM_LOAD( "mag-05.rom", 0x100000, 0x80000,  0x56a53254 )
	ROM_LOAD( "mag-06.rom", 0x180000, 0x80000,  0x82d44749 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "gl-06.rom",  0x00000,  0x20000,  0xd92e519d )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* Extra Oki samples */
	ROM_LOAD( "mag-07.rom", 0x00000,  0x80000,  0x08eb5264 )	/* banked */
ROM_END

ROM_START( joemac )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "gl02-2.k1", 0x00000, 0x20000,  0x80da12e2 )
	ROM_LOAD_ODD ( "gl05-2.k3", 0x00000, 0x20000,  0xfe4dbbbb )
	ROM_LOAD_EVEN( "gl01-2.j1", 0x40000, 0x20000,  0x0b245307 )
	ROM_LOAD_ODD ( "gl04-2.j3", 0x40000, 0x20000,  0x1b331f61 )
	ROM_LOAD_EVEN( "gn-00.rom", 0x80000, 0x20000,  0x0b110b16 )
	ROM_LOAD_ODD ( "gn-03.rom", 0x80000, 0x20000,  0x1e28e697 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */
	ROM_LOAD( "gl-07.rom",  0x00000,  0x10000,  0xca8bef96 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gl-08.rom",  0x00000,  0x10000,  0x33a2b400 )	/* chars */
	ROM_LOAD( "gl-09.rom",  0x10000,  0x10000,  0x5a2d4752 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-00.rom", 0x000000, 0x80000,  0xa8f05d33 )	/* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-01.rom", 0x000000, 0x80000,  0x5b399eed )	/* tiles 2 */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-02.rom", 0x000000, 0x80000,  0xde89c69a )	/* tiles 3 */

	ROM_REGION( 0x200000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-03.rom", 0x000000, 0x80000,  0x2220eb9f )	/* sprites */
	ROM_LOAD( "mag-04.rom", 0x080000, 0x80000,  0x144b94cc )
	ROM_LOAD( "mag-05.rom", 0x100000, 0x80000,  0x56a53254 )
	ROM_LOAD( "mag-06.rom", 0x180000, 0x80000,  0x82d44749 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "gl-06.rom",  0x00000,  0x20000,  0xd92e519d )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* Extra Oki samples */
	ROM_LOAD( "mag-07.rom", 0x00000,  0x80000,  0x08eb5264 )	/* banked */
ROM_END

ROM_START( stoneage )
	ROM_REGION( 0xc0000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "sa_1_019.bin", 0x00000, 0x20000,  0x7fb8c44f )
	ROM_LOAD_ODD ( "sa_1_033.bin", 0x00000, 0x20000,  0x961c752b )
	ROM_LOAD_EVEN( "sa_1_018.bin", 0x40000, 0x20000,  0xa4043022 )
	ROM_LOAD_ODD ( "sa_1_032.bin", 0x40000, 0x20000,  0xf52a3286 )
	ROM_LOAD_EVEN( "sa_1_017.bin", 0x80000, 0x20000,  0x08d6397a )
	ROM_LOAD_ODD ( "sa_1_031.bin", 0x80000, 0x20000,  0x103079f5 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound CPU */
	ROM_LOAD( "sa_1_012.bin",  0x00000,  0x10000, 0x56058934 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gl-08.rom",  0x00000,  0x10000,  0x33a2b400 )	/* chars */
	ROM_LOAD( "gl-09.rom",  0x10000,  0x10000,  0x5a2d4752 )

	/* The bootleg graphics are stored in a different arrangement but
		seem to be the same as the original set */

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-00.rom", 0x000000, 0x80000,  0xa8f05d33 )	/* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-01.rom", 0x000000, 0x80000,  0x5b399eed )	/* tiles 2 */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-02.rom", 0x000000, 0x80000,  0xde89c69a )	/* tiles 3 */

	ROM_REGION( 0x200000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mag-03.rom", 0x000000, 0x80000,  0x2220eb9f )	/* sprites */
	ROM_LOAD( "mag-04.rom", 0x080000, 0x80000,  0x144b94cc )
	ROM_LOAD( "mag-05.rom", 0x100000, 0x80000,  0x56a53254 )
	ROM_LOAD( "mag-06.rom", 0x180000, 0x80000,  0x82d44749 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "sa_1_069.bin",  0x00000,  0x40000, 0x2188f3ca )

	/* No extra Oki samples in the bootleg */
ROM_END

ROM_START( edrandy )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "gg-00-2", 0x00000, 0x20000, 0xce1ba964 )
  	ROM_LOAD_ODD ( "gg-04-2", 0x00000, 0x20000, 0x24caed19 )
	ROM_LOAD_EVEN( "gg-01-2", 0x40000, 0x20000, 0x33677b80 )
 	ROM_LOAD_ODD ( "gg-05-2", 0x40000, 0x20000, 0x79a68ca6 )
	ROM_LOAD_EVEN( "ge-02",   0x80000, 0x20000, 0xc2969fbb )
	ROM_LOAD_ODD ( "ge-06",   0x80000, 0x20000, 0x5c2e6418 )
	ROM_LOAD_EVEN( "ge-03",   0xc0000, 0x20000, 0x5e7b19a8 )
	ROM_LOAD_ODD ( "ge-07",   0xc0000, 0x20000, 0x5eb819a1 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "ge-09",    0x00000, 0x10000, 0x9f94c60b )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gg-11",    0x000000, 0x10000, 0xee567448 )
  	ROM_LOAD( "gg-10",    0x010000, 0x10000, 0xb96c6cbe )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-00",   0x000000, 0x80000, 0x3735b22d ) /* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-01",   0x000000, 0x80000, 0x7bb13e1c ) /* tiles 2 */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-02",   0x000000, 0x80000, 0x6c76face ) /* tiles 3 */

	ROM_REGION( 0x500000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-03",   0x000000, 0x80000, 0xc0bff892 ) /* sprites */
	ROM_LOAD( "mad-04",   0x080000, 0x80000, 0x464f3eb9 )
	ROM_LOAD( "mad-07",   0x100000, 0x80000, 0xac03466e )
	ROM_LOAD( "mad-10",   0x180000, 0x80000, 0x42da8ef0 )
	ROM_LOAD( "mad-09",   0x200000, 0x80000, 0x930f4900 )
	ROM_LOAD( "mad-05",   0x280000, 0x80000, 0x3f2ccf95 )
	ROM_LOAD( "mad-06",   0x300000, 0x80000, 0x60871f77 )
	ROM_LOAD( "mad-08",   0x380000, 0x80000, 0x1b420ec8 )
	ROM_LOAD( "mad-11",   0x400000, 0x80000, 0x03c1f982 )
	ROM_LOAD( "mad-12",   0x480000, 0x80000, 0xa0bd62b6 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "ge-08",    0x00000, 0x20000, 0xdfe28c7b )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* Extra Oki samples */
	ROM_LOAD( "mad-13", 0x00000, 0x80000, 0x6ab28eba )	/* banked */
ROM_END

ROM_START( edrandyj )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "ge-00-2",   0x00000, 0x20000, 0xb3d2403c )
  	ROM_LOAD_ODD ( "ge-04-2",   0x00000, 0x20000, 0x8a9624d6 )
	ROM_LOAD_EVEN( "ge-01-2",   0x40000, 0x20000, 0x84360123 )
 	ROM_LOAD_ODD ( "ge-05-2",   0x40000, 0x20000, 0x0bf85d9d )
	ROM_LOAD_EVEN( "ge-02",     0x80000, 0x20000, 0xc2969fbb )
	ROM_LOAD_ODD ( "ge-06",     0x80000, 0x20000, 0x5c2e6418 )
	ROM_LOAD_EVEN( "ge-03",     0xc0000, 0x20000, 0x5e7b19a8 )
	ROM_LOAD_ODD ( "ge-07",     0xc0000, 0x20000, 0x5eb819a1 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "ge-09",    0x00000, 0x10000, 0x9f94c60b )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ge-10",    0x000000, 0x10000, 0x2528d795 )
  	ROM_LOAD( "ge-11",    0x010000, 0x10000, 0xe34a931e )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-00",   0x000000, 0x80000, 0x3735b22d ) /* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-01",   0x000000, 0x80000, 0x7bb13e1c ) /* tiles 2 */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-02",   0x000000, 0x80000, 0x6c76face ) /* tiles 3 */

	ROM_REGION( 0x500000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mad-03",   0x000000, 0x80000, 0xc0bff892 ) /* sprites */
	ROM_LOAD( "mad-04",   0x080000, 0x80000, 0x464f3eb9 )
	ROM_LOAD( "mad-07",   0x100000, 0x80000, 0xac03466e )
	ROM_LOAD( "mad-10",   0x180000, 0x80000, 0x42da8ef0 )
	ROM_LOAD( "mad-09",   0x200000, 0x80000, 0x930f4900 )
	ROM_LOAD( "mad-05",   0x280000, 0x80000, 0x3f2ccf95 )
	ROM_LOAD( "mad-06",   0x300000, 0x80000, 0x60871f77 )
	ROM_LOAD( "mad-08",   0x380000, 0x80000, 0x1b420ec8 )
	ROM_LOAD( "mad-11",   0x400000, 0x80000, 0x03c1f982 )
	ROM_LOAD( "mad-12",   0x480000, 0x80000, 0xa0bd62b6 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "ge-08",    0x00000, 0x20000, 0xdfe28c7b )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* Extra Oki samples */
	ROM_LOAD( "mad-13", 0x00000, 0x80000, 0x6ab28eba )	/* banked */
ROM_END

/**********************************************************************************/

static void cninja_patch(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int i;

	for (i=0; i<0x80000; i+=2) {
		int aword=READ_WORD(&RAM[i]);

		if (aword==0x66ff || aword==0x67ff) {
			int doublecheck=READ_WORD(&RAM[i-8]);

			/* Cmpi + btst controlling opcodes */
			if (doublecheck==0xc39 || doublecheck==0x839) {

				WRITE_WORD (&RAM[i],0x4E71);
		        WRITE_WORD (&RAM[i-2],0x4E71);
		        WRITE_WORD (&RAM[i-4],0x4E71);
		        WRITE_WORD (&RAM[i-6],0x4E71);
		        WRITE_WORD (&RAM[i-8],0x4E71);
			}
		}
	}
}

#if 0
static void edrandyj_patch(void)
{
//	unsigned char *RAM = memory_region(REGION_CPU1);

//	WRITE_WORD (&RAM[0x98cc],0x4E71);
//	WRITE_WORD (&RAM[0x98ce],0x4E71);

}
#endif

/**********************************************************************************/

static void init_cninja(void)
{
	install_mem_write_handler(0, 0x1bc0a8, 0x1bc0a9, cninja_sound_w);
	cninja_patch();
}

static void init_stoneage(void)
{
	install_mem_write_handler(0, 0x1bc0a8, 0x1bc0a9, stoneage_sound_w);
}

/**********************************************************************************/

GAME( 1991, cninja,   0,       cninja,   cninja,  cninja,   ROT0, "Data East Corporation", "Caveman Ninja (World revision 3)" )
GAME( 1991, cninja0,  cninja,  cninja,   cninja,  cninja,   ROT0, "Data East Corporation", "Caveman Ninja (World revision 0)" )
GAME( 1991, cninjau,  cninja,  cninja,   cninjau, cninja,   ROT0, "Data East Corporation", "Caveman Ninja (US)" )
GAME( 1991, joemac,   cninja,  cninja,   cninja,  cninja,   ROT0, "Data East Corporation", "Joe & Mac (Japan)" )
GAME( 1991, stoneage, cninja,  stoneage, cninja,  stoneage, ROT0, "bootleg", "Stoneage" )
GAMEX(1990, edrandy,  0,       edrandy,  cninja,  0,        ROT0, "Data East Corporation", "Edward Randy (World)", GAME_NOT_WORKING )
GAMEX(1990, edrandyj, edrandy, edrandy,  cninja,  0,        ROT0, "Data East Corporation", "Edward Randy (Japan)", GAME_NOT_WORKING )
