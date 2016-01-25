#include "../vidhrdw/cave.cpp"

/***************************************************************************

							  -= Cave Games =-

				driver by	Luca Elia (eliavit@unina.it)


CPU:	MC68000
Sound:	YMZ280B
Other:	EEPROM

---------------------------------------------------------------------------
Game				Year	PCB		License				Issues / Notes
---------------------------------------------------------------------------
Donpachi       (J)	1995								- Not Dumped -
Dodonpachi     (J)	1997	ATC03D2	Atlus
Dangun Feveron (J)	1998	CV01    Nihon System Inc.
ESP Ra.De.     (J)	1998	ATC04	Atlus
Uo Poko        (J)	1998	CV02	Jaleco
Guwange        (J)	1999	ATC05	Atlus
---------------------------------------------------------------------------

	- Note: press F2 to enter service mode: there are no DSWs -

To Do:

- Alignment issues between sprites and layers: see uopoko!


***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"

/* Variables that vidhrdw has access to */

int cave_spritetype;


/* Variables defined in vidhrdw */

extern unsigned char *cave_videoregs;

extern unsigned char *cave_vram_0, *cave_vctrl_0;
extern unsigned char *cave_vram_1, *cave_vctrl_1;
extern unsigned char *cave_vram_2, *cave_vctrl_2;


/* Functions defined in vidhrdw */

WRITE_HANDLER( cave_vram_0_w );
WRITE_HANDLER( cave_vram_1_w );
WRITE_HANDLER( cave_vram_2_w );

WRITE_HANDLER( cave_vram_0_8x8_w );
WRITE_HANDLER( cave_vram_1_8x8_w );
WRITE_HANDLER( cave_vram_2_8x8_w );

int  dfeveron_vh_start(void);
int  ddonpach_vh_start(void);
int  esprade_vh_start(void);
int  guwange_vh_start(void);
int  uopoko_vh_start(void);


void ddonpach_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void dfeveron_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

void cave_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* Variables only used here */

static UINT8 vblank_irq;
static UINT8 sound_irq;
static UINT8 unknown_irq;



/***************************************************************************


						Misc Machine Emulation Routines


***************************************************************************/


/* Update the IRQ state based on all possible causes */
static void update_irq_state(void)
{
	if (vblank_irq || sound_irq || unknown_irq)
		cpu_set_irq_line(0, 1, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 1, CLEAR_LINE);
}


/* Called once/frame to generate the VBLANK interrupt */
static int cave_interrupt(void)
{
	vblank_irq = 1;
	update_irq_state();
	return ignore_interrupt();
}


/* Called by the YMZ280B to set the IRQ state */
static void sound_irq_gen(int state)
{
	sound_irq = (state != 0);
	update_irq_state();
}


/*	Level 1 irq routines:

	Game		|first read	| bit==0->routine +	|
				|offset:	| read this offset	|

	ddonpach	4,0			0 -> vblank + 4		1 -> rte	2 -> like 0		read sound
	dfeveron	0			0 -> vblank + 4		1 -> + 6	-				read sound
	uopoko		0			0 -> vblank + 4		1 -> + 6	-				read sound
	esprade		0			0 -> vblank + 4		1 -> rte	2 must be 0		read sound
	guwange		0			0 -> vblank + 6,4	1 -> + 6,4	2 must be 0		read sound
*/


/* Reads the cause of the interrupt and clears the state */

static READ_HANDLER( cave_irq_cause_r )
{
	int result = 0x0003;

	if (vblank_irq)		result ^= 0x01;
	if (unknown_irq)	result ^= 0x02;

	if (offset == 4)	vblank_irq = 0;
	if (offset == 6)	unknown_irq = 0;

	update_irq_state();

	return result;
}

/* Is ddonpach different? This is probably wrong but it works */

static READ_HANDLER( ddonpach_irq_cause_r )
{
	int result = 0x0007;

	if (vblank_irq)		result ^= 0x01;
//	if (unknown_irq)	result ^= 0x02;

	if (offset == 0)	vblank_irq = 0;
//	if (offset == 4)	unknown_irq = 0;

	update_irq_state();

	return result;
}




/* Handles writes to the YMZ280B */
static WRITE_HANDLER( cave_sound_w )
{
	if (!(data & 0x00ff0000))
	{
		if (offset & 2)
			YMZ280B_data_0_w(offset, data & 0xff);
		else
			YMZ280B_register_0_w(offset, data & 0xff);
	}
}


/* Handles reads from the YMZ280B */
static READ_HANDLER( cave_sound_r )
{
	return YMZ280B_status_0_r(offset);
}




/***************************************************************************

									EEPROM

***************************************************************************/


static struct EEPROM_interface eeprom_interface =
{
	6,				// address bits
	16,				// data bits
	"0110",			// read command
	"0101",			// write command
	0,				// erase command
	0,				// lock command
	"0100110000" 	// unlock command
};

WRITE_HANDLER( cave_eeprom_w )
{
	if ( (data & 0xFF000000) == 0 )  // even address
	{
		// latch the bit
		EEPROM_write_bit(data & 0x0800);

		// reset line asserted: reset.
		EEPROM_set_cs_line((data & 0x0200) ? CLEAR_LINE : ASSERT_LINE );

		// clock line asserted: write latch or select next bit to read
		EEPROM_set_clock_line((data & 0x0400) ? ASSERT_LINE : CLEAR_LINE );
	}
}

WRITE_HANDLER( guwange_eeprom_w )
{
	if ( (data & 0x00FF0000) == 0 )  // odd address
	{
		// latch the bit
		EEPROM_write_bit(data & 0x80);

		// reset line asserted: reset.
		EEPROM_set_cs_line((data & 0x20) ? CLEAR_LINE : ASSERT_LINE );

		// clock line asserted: write latch or select next bit to read
		EEPROM_set_clock_line((data & 0x40) ? ASSERT_LINE : CLEAR_LINE );
	}
}


void cave_nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&eeprom_interface);

		if (file) EEPROM_load(file);
		else usrintf_showmessage("You MUST initialize NVRAM in service mode");
	}
}



/***************************************************************************

								Inputs

***************************************************************************/

READ_HANDLER( cave_inputs_r )
{
	switch (offset)
	{
		case 0:
			return readinputport(0);

		case 2:
			return	readinputport(1) |
					( (EEPROM_read_bit() & 0x01) << 11 );

		default:	return 0;
	}
}


READ_HANDLER( guwange_inputs_r )
{
	switch (offset)
	{
		case 0:
			return readinputport(0);

		case 2:
			return	readinputport(1) |
					( (EEPROM_read_bit() & 0x01) << 7 );

		default:	return 0;
	}
}

/***************************************************************************


								Memory Maps


***************************************************************************/

/*
 Lines starting with an empty comment in the following MemoryReadAddress
 arrays are there for debug (e.g. the game does not read from those ranges
 AFAIK)
*/

/***************************************************************************
								Dangun Feveron
***************************************************************************/

static struct MemoryReadAddress dfeveron_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MRA_BANK1					},	// RAM
	{ 0x300002, 0x300003, cave_sound_r				},	// From sound
/**/{ 0x400000, 0x407fff, MRA_BANK3					},	// Sprites
/**/{ 0x408000, 0x40ffff, MRA_BANK4					},	// Sprites?
/**/{ 0x500000, 0x507fff, MRA_BANK5					},	// Layer 0 (size?)
/**/{ 0x600000, 0x607fff, MRA_BANK6					},	// Layer 1 (size?)
/**/{ 0x708000, 0x708fff, MRA_BANK7					},	// Palette
/**/{ 0x710000, 0x710fff, MRA_BANK8					},	// ?
	{ 0x800000, 0x800007, cave_irq_cause_r			},	// ?
/**/{ 0x900000, 0x900005, MRA_BANK10				},	// Layer 0 Control
/**/{ 0xa00000, 0xa00005, MRA_BANK11				},	// Layer 1 Control
	{ 0xb00000, 0xb00003, cave_inputs_r				},	// Inputs + EEPROM
/**/{ 0xc00000, 0xc00001, MRA_BANK12				},	//
	{ -1 }
};

static struct MemoryWriteAddress dfeveron_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM									},	// ROM
	{ 0x100000, 0x10ffff, MWA_BANK1									},	// RAM
	{ 0x300000, 0x300003, cave_sound_w								},	// To Sound
	{ 0x400000, 0x407fff, MWA_BANK3, &spriteram, &spriteram_size	},	// Sprites
	{ 0x408000, 0x40ffff, MWA_BANK4									},	// Sprites?
	{ 0x500000, 0x507fff, cave_vram_0_w, &cave_vram_0				},	// Layer 0 (size?)
	{ 0x600000, 0x607fff, cave_vram_1_w, &cave_vram_1				},	// Layer 1 (size?)
	{ 0x708000, 0x708fff, paletteram_xGGGGGRRRRRBBBBB_word_w, &paletteram },	// Palette
	{ 0x710c00, 0x710fff, MWA_BANK8									},	// ?
	{ 0x800000, 0x80007f, MWA_BANK9,  &cave_videoregs				},	// Video Regs?
	{ 0x900000, 0x900005, MWA_BANK10, &cave_vctrl_0					},	// Layer 0 Control
	{ 0xa00000, 0xa00005, MWA_BANK11, &cave_vctrl_1					},	// Layer 1 Control
	{ 0xc00000, 0xc00001, cave_eeprom_w								},	// EEPROM
	{ -1 }
};



/***************************************************************************
								Dodonpachi
***************************************************************************/

static struct MemoryReadAddress ddonpach_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MRA_BANK1					},	// RAM
	{ 0x300002, 0x300003, cave_sound_r				},	// From sound
/**/{ 0x400000, 0x407fff, MRA_BANK3					},	// Sprites
/**/{ 0x408000, 0x40ffff, MRA_BANK4					},	// Sprites?
/**/{ 0x500000, 0x507fff, MRA_BANK5					},	// Layer 0 (size?)
/**/{ 0x600000, 0x607fff, MRA_BANK6					},	// Layer 1 (size?)
/**/{ 0x700000, 0x70ffff, MRA_BANK7					},	// Layer 2 (size?)
	{ 0x800000, 0x800007, ddonpach_irq_cause_r		},	// ?
/**/{ 0x900000, 0x900005, MRA_BANK9					},	// Layer 0 Control
/**/{ 0xa00000, 0xa00005, MRA_BANK10				},	// Layer 1 Control
/**/{ 0xb00000, 0xb00005, MRA_BANK11				},	// Layer 2 Control
/**/{ 0xc00000, 0xc0ffff, MRA_BANK12				},	// Palette
	{ 0xd00000, 0xd00003, cave_inputs_r				},	// Inputs + EEPROM
/**/{ 0xe00000, 0xe00001, MRA_BANK13				},	//
	{ -1 }
};

static struct MemoryWriteAddress ddonpach_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM									},	// ROM
	{ 0x100000, 0x10ffff, MWA_BANK1									},	// RAM
	{ 0x300000, 0x300003, cave_sound_w								},	// To Sound
	{ 0x400000, 0x407fff, MWA_BANK3, &spriteram, &spriteram_size	},	// Sprites
	{ 0x408000, 0x40ffff, MWA_BANK4									},	// Sprites?
	{ 0x500000, 0x507fff, cave_vram_0_w, &cave_vram_0				},	// Layer 0 (size?)
	{ 0x600000, 0x607fff, cave_vram_1_w, &cave_vram_1				},	// Layer 1 (size?)
	{ 0x700000, 0x70ffff, cave_vram_2_8x8_w, &cave_vram_2			},	// Layer 2 (size?)
	{ 0x800000, 0x80007f, MWA_BANK8,  &cave_videoregs				},	// Video Regs?
	{ 0x900000, 0x900005, MWA_BANK9,  &cave_vctrl_0					},	// Layer 0 Control
	{ 0xa00000, 0xa00005, MWA_BANK10, &cave_vctrl_1					},	// Layer 1 Control
	{ 0xb00000, 0xb00005, MWA_BANK11, &cave_vctrl_2					},	// Layer 2 Control
	{ 0xc00000, 0xc0ffff, paletteram_xGGGGGRRRRRBBBBB_word_w, &paletteram },	// Palette
	{ 0xe00000, 0xe00001, cave_eeprom_w								},	// EEPROM
	{ -1 }
};



/***************************************************************************
									Esprade
***************************************************************************/

static struct MemoryReadAddress esprade_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MRA_BANK1					},	// RAM
	{ 0x300002, 0x300003, cave_sound_r				},	// From sound
/**/{ 0x400000, 0x407fff, MRA_BANK3					},	// Sprites
/**/{ 0x408000, 0x40ffff, MRA_BANK4					},	// Sprites?
/**/{ 0x500000, 0x507fff, MRA_BANK5					},	// Layer 0 (size?)
/**/{ 0x600000, 0x607fff, MRA_BANK6					},	// Layer 1 (size?)
/**/{ 0x700000, 0x707fff, MRA_BANK7					},	// Layer 2 (size?)
	{ 0x800000, 0x800007, cave_irq_cause_r			},	// ?
/**/{ 0x900000, 0x900005, MRA_BANK9					},	// Layer 0 Control
/**/{ 0xa00000, 0xa00005, MRA_BANK10				},	// Layer 1 Control
/**/{ 0xb00000, 0xb00005, MRA_BANK11				},	// Layer 2 Control
/**/{ 0xc00000, 0xc0ffff, MRA_BANK12				},	// Palette
	{ 0xd00000, 0xd00003, cave_inputs_r				},	// Inputs + EEPROM
/**/{ 0xe00000, 0xe00001, MRA_BANK13				},	//
	{ -1 }
};

static struct MemoryWriteAddress esprade_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM									},	// ROM
	{ 0x100000, 0x10ffff, MWA_BANK1									},	// RAM
	{ 0x300000, 0x300003, cave_sound_w								},	// To Sound
	{ 0x400000, 0x407fff, MWA_BANK3, &spriteram, &spriteram_size	},	// Sprites
	{ 0x408000, 0x40ffff, MWA_BANK4									},	// Sprites?
	{ 0x500000, 0x507fff, cave_vram_0_w, &cave_vram_0				},	// Layer 0 (size?)
	{ 0x600000, 0x607fff, cave_vram_1_w, &cave_vram_1				},	// Layer 1 (size?)
	{ 0x700000, 0x707fff, cave_vram_2_w, &cave_vram_2				},	// Layer 2 (size?)
	{ 0x800000, 0x80007f, MWA_BANK8,  &cave_videoregs				},	// Video Regs?
	{ 0x900000, 0x900005, MWA_BANK9,  &cave_vctrl_0					},	// Layer 0 Control
	{ 0xa00000, 0xa00005, MWA_BANK10, &cave_vctrl_1					},	// Layer 1 Control
	{ 0xb00000, 0xb00005, MWA_BANK11, &cave_vctrl_2					},	// Layer 2 Control
	{ 0xc00000, 0xc0ffff, paletteram_xGGGGGRRRRRBBBBB_word_w, &paletteram },	// Palette
	{ 0xe00000, 0xe00001, cave_eeprom_w								},	// EEPROM
	{ -1 }
};



/***************************************************************************
									Guwange
***************************************************************************/

static struct MemoryReadAddress guwange_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM					},	// ROM
	{ 0x200000, 0x20ffff, MRA_BANK1					},	// RAM
	{ 0x300000, 0x300007, cave_irq_cause_r			},	// ?
/**/{ 0x400000, 0x407fff, MRA_BANK3					},	// Sprites
/**/{ 0x408000, 0x40ffff, MRA_BANK4					},	// Sprites?
/**/{ 0x500000, 0x507fff, MRA_BANK5					},	// Layer 0 (size?)
/**/{ 0x600000, 0x607fff, MRA_BANK6					},	// Layer 1 (size?)
/**/{ 0x700000, 0x707fff, MRA_BANK7					},	// Layer 2 (size?)
	{ 0x800002, 0x800003, cave_sound_r				},	// From sound
/**/{ 0x900000, 0x900005, MRA_BANK9					},	// Layer 0 Control
/**/{ 0xa00000, 0xa00005, MRA_BANK10				},	// Layer 1 Control
/**/{ 0xb00000, 0xb00005, MRA_BANK11				},	// Layer 2 Control
/**/{ 0xc00000, 0xc0ffff, MRA_BANK12				},	// Palette
	{ 0xd00010, 0xd00013, guwange_inputs_r			},	// Inputs + EEPROM
	{ -1 }
};

static struct MemoryWriteAddress guwange_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM									},	// ROM
	{ 0x200000, 0x20ffff, MWA_BANK1									},	// RAM
	{ 0x300000, 0x30007f, MWA_BANK8,  &cave_videoregs				},	// Video Regs?
	{ 0x400000, 0x407fff, MWA_BANK3, &spriteram, &spriteram_size	},	// Sprites
	{ 0x408000, 0x40ffff, MWA_BANK4									},	// Sprites?
	{ 0x500000, 0x507fff, cave_vram_0_w, &cave_vram_0				},	// Layer 0 (size?)
	{ 0x600000, 0x607fff, cave_vram_1_w, &cave_vram_1				},	// Layer 1 (size?)
	{ 0x700000, 0x707fff, cave_vram_2_w, &cave_vram_2				},	// Layer 2 (size?)
	{ 0x800000, 0x800003, cave_sound_w								},	// To Sound
	{ 0x900000, 0x900005, MWA_BANK9,  &cave_vctrl_0					},	// Layer 0 Control
	{ 0xa00000, 0xa00005, MWA_BANK10, &cave_vctrl_1					},	// Layer 1 Control
	{ 0xb00000, 0xb00005, MWA_BANK11, &cave_vctrl_2					},	// Layer 2 Control
	{ 0xc00000, 0xc0ffff, paletteram_xGGGGGRRRRRBBBBB_word_w, &paletteram },	// Palette
	{ 0xd00010, 0xd00011, guwange_eeprom_w							},	// EEPROM
//	{ 0xd00012, 0xd00013, MWA_NOP	},	// ?
//	{ 0xd00014, 0xd00015, MWA_NOP	},	// ? $800068 in dfeveron ?
	{ -1 }
};



/***************************************************************************
									Uo Poko
***************************************************************************/

static struct MemoryReadAddress uopoko_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MRA_BANK1					},	// RAM
	{ 0x300002, 0x300003, cave_sound_r				},	// From sound
/**/{ 0x400000, 0x407fff, MRA_BANK3					},	// Sprites
/**/{ 0x408000, 0x40ffff, MRA_BANK4					},	// Sprites?
/**/{ 0x500000, 0x501fff, MRA_BANK5					},	// Layer 0 (size?)
	{ 0x600000, 0x600007, cave_irq_cause_r			},	// ?
/**/{ 0x700000, 0x700005, MRA_BANK7					},	// Layer 0 Control
/**/{ 0x800000, 0x80ffff, MRA_BANK8					},	// Palette
	{ 0x900000, 0x900003, cave_inputs_r				},	// Inputs + EEPROM
/**/{ 0xa00000, 0xa00001, MRA_BANK9					},	//
	{ -1 }
};

static struct MemoryWriteAddress uopoko_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM									},	// ROM
	{ 0x100000, 0x10ffff, MWA_BANK1									},	// RAM
	{ 0x300000, 0x300003, cave_sound_w								},	// To Sound
	{ 0x400000, 0x407fff, MWA_BANK3, &spriteram, &spriteram_size	},	// Sprites
	{ 0x408000, 0x40ffff, MWA_BANK4									},	// Sprites?
	{ 0x500000, 0x501fff, cave_vram_0_w, &cave_vram_0				},	// Layer 0 (size?)
	{ 0x600000, 0x60007f, MWA_BANK6,  &cave_videoregs				},	// Video Regs?
	{ 0x700000, 0x700005, MWA_BANK7,  &cave_vctrl_0					},	// Layer 0 Control
	{ 0x800000, 0x80ffff, paletteram_xGGGGGRRRRRBBBBB_word_w, &paletteram },	// Palette
	{ 0xa00000, 0xa00001, cave_eeprom_w								},	// EEPROM
	{ -1 }
};




/***************************************************************************


								Input Ports


***************************************************************************/

/*
	dfeveron config menu:
	101624.w -> 8,a6	preferences
	101626.w -> c,a6	(1:coin<<4|credit) <<8 | (2:coin<<4|credit)
*/

INPUT_PORTS_START( dfeveron )

	PORT_START	// IN0 - Player 1
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	 | IPF_PLAYER1 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START1  )

	PORT_BIT_IMPULSE(  0x0100, IP_ACTIVE_LOW, IPT_COIN1, 1)
	PORT_BITX( 0x0200, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )	// sw? exit service mode
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )	// sw? enter & exit service mode
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - Player 2
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	 | IPF_PLAYER2 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START2  )

	PORT_BIT_IMPULSE(  0x0100, IP_ACTIVE_LOW, IPT_COIN2, 1)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_SERVICE1)
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	//         0x0800  eeprom bit
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

INPUT_PORTS_END


INPUT_PORTS_START( guwange )

	PORT_START	// IN0 - Player 1 & 2
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	 | IPF_PLAYER1 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	 | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )

	PORT_START	// IN1 - Coins
	PORT_BIT_IMPULSE(  0x0001, IP_ACTIVE_LOW, IPT_COIN1, 1)
	PORT_BIT_IMPULSE(  0x0002, IP_ACTIVE_LOW, IPT_COIN2, 1)
	PORT_BITX( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
//             0x0080  eeprom bit

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

INPUT_PORTS_END





/***************************************************************************


							Graphics Layouts


***************************************************************************/


/* 8x8x8 tiles */
static struct GfxLayout layout_8x8_8bit =
{
	8,8,
	RGN_FRAC(1,1),
	8,
	{8,9,10,11, 0,1,2,3},
	{0*4,1*4,4*4,5*4,8*4,9*4,12*4,13*4},
	{0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64},
	8*8*8
};


/* 16x16x4 tiles */
static struct GfxLayout layout_4bit =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{0,1,2,3},
	{0*4,1*4,2*4,3*4,4*4,5*4,6*4,7*4,
	 64*4,65*4,66*4,67*4,68*4,69*4,70*4,71*4},
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32,
	 16*32,17*32,18*32,19*32,20*32,21*32,22*32,23*32},
	16*16*4
};

/* 16x16x8 tiles */
static struct GfxLayout layout_8bit =
{
	16,16,
	RGN_FRAC(1,1),
	8,
	{8,9,10,11, 0,1,2,3},
	{0*4,1*4,4*4,5*4,8*4,9*4,12*4,13*4,
	 128*4,129*4,132*4,133*4,136*4,137*4,140*4,141*4},
	{0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64,
	 16*64,17*64,18*64,19*64,20*64,21*64,22*64,23*64},
	16*16*8
};


#if 0
/* 16x16x4 Zooming Sprites - No need to decode them, but if we do,
   to have a look, we must remember the data has been converted from
   4 bits/pixel to 1 byte per pixel, for the sprite manager */

static struct GfxLayout layout_spritemanager =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{4,5,6,7},
	{0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
	 8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8},
	{0*128,1*128,2*128,3*128,4*128,5*128,6*128,7*128,
	 8*128,9*128,10*128,11*128,12*128,13*128,14*128,15*128 },
	16*16*8
};
#endif


/***************************************************************************
								Dangun Feveron
***************************************************************************/

static struct GfxDecodeInfo dfeveron_gfxdecodeinfo[] =
{
	/* There are only $800 colors here, the first half for sprites
	   the second half for tiles. We use $8000 virtual colors instead
	   for consistency with games having $8000 real colors.
	   A vh_init_palette function is provided as well, for sprites */

	{ REGION_GFX1, 0, &layout_4bit,          0x4400, 0x40 }, // [0] Tiles
	{ REGION_GFX2, 0, &layout_4bit,          0x4400, 0x40 }, // [1] Tiles
//	{ REGION_GFX3, 0, &layout_4bit,          0x4400, 0x40 }, // [2] Tiles
//	{ REGION_GFX4, 0, &layout_spritemanager, 0x0000, 0x40 }, // [3] 4 Bit Sprites
	{ -1 }
};


/***************************************************************************
								Dodonpachi
***************************************************************************/

static struct GfxDecodeInfo ddonpach_gfxdecodeinfo[] =
{
	/* Layers 1&2 are 4 bit deep and use the first 16 of every 256
	   colors for any given color code (a vh_init_palette function
	   is provided for these layers, filling the 8000-83ff entries
	   in the color table. Layer 3 uses the whole 256 for any given
	   color code and the 4000-7fff in the color table.       */

	{ REGION_GFX1, 0, &layout_4bit,          0x8000, 0x40 }, // [0] Tiles
	{ REGION_GFX2, 0, &layout_4bit,          0x8000, 0x40 }, // [1] Tiles
	{ REGION_GFX3, 0, &layout_8x8_8bit,      0x4000, 0x40 }, // [2] Tiles
//	{ REGION_GFX4, 0, &layout_spritemanager, 0x0000, 0x40 }, // [3] 4 Bit Sprites
	{ -1 }
};


/***************************************************************************
								Esprade
***************************************************************************/

static struct GfxDecodeInfo esprade_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_8bit,          0x4000, 0x40 }, // [0] Tiles
	{ REGION_GFX2, 0, &layout_8bit,          0x4000, 0x40 }, // [1] Tiles
	{ REGION_GFX3, 0, &layout_8bit,          0x4000, 0x40 }, // [2] Tiles
//	{ REGION_GFX4, 0, &layout_spritemanager, 0x0000, 0x40 }, // [3] 8 Bit Sprites
	{ -1 }
};



/***************************************************************************
								Uo Poko
***************************************************************************/

static struct GfxDecodeInfo uopoko_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_8bit,          0x4000, 0x40 }, // [0] Tiles
//	{ REGION_GFX4, 0, &layout_spritemanager, 0x0000, 0x40 }, // [1] 8 Bit Sprites
	{ -1 }
};




/***************************************************************************


								Machine Drivers


***************************************************************************/


static struct YMZ280Binterface ymz280b_intf =
{
	1,
	{ 16934400/*16000000*/ },
	{ REGION_SOUND1 },
	/*{ YM2203_VOL(50,50) },*/
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ sound_irq_gen }
};



/***************************************************************************
								Dangun Feveron
***************************************************************************/

static struct MachineDriver machine_driver_dfeveron =
{
	{
		{
			CPU_M68000,
			16000000,
			dfeveron_readmem, dfeveron_writemem,0,0,
			cave_interrupt, 1
		},
	},
	/*60*/15625/271.5,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	320, 240, { 0, 320-1, 0, 240-1 },
	dfeveron_gfxdecodeinfo,
	0x800, 0x8000,	/* $8000 palette entries for consistency with the other games */
	dfeveron_vh_init_palette,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	dfeveron_vh_start,
	0,
	cave_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_YMZ280B, &ymz280b_intf }
	},

	cave_nvram_handler
};



/***************************************************************************
								Dodonpachi
***************************************************************************/

static struct MachineDriver machine_driver_ddonpach =
{
	{
		{
			CPU_M68000,
			16000000,
			ddonpach_readmem, ddonpach_writemem,0,0,
			cave_interrupt, 1
		},
	},
	/*60*/15625/271.5,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	320, 240, { 0, 320-1, 0, 240-1 },
	ddonpach_gfxdecodeinfo,
	0x8000, 0x8000 + 0x40*16,	// $400 extra entries for layers 1&2
	ddonpach_vh_init_palette,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ddonpach_vh_start,
	0,
	cave_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_YMZ280B, &ymz280b_intf }
	},

	cave_nvram_handler
};



/***************************************************************************
								Esprade
***************************************************************************/


static struct MachineDriver machine_driver_esprade =
{
	{
		{
			CPU_M68000,
			16000000,
			esprade_readmem, esprade_writemem,0,0,
			cave_interrupt, 1
		},
	},
	/*60*/15625/271.5,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	320, 240, { 0, 320-1, 0, 240-1 },
	esprade_gfxdecodeinfo,
	0x8000, 0x8000,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	esprade_vh_start,
	0,
	cave_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_YMZ280B, &ymz280b_intf }
	},

	cave_nvram_handler
};



/***************************************************************************
								Guwange
***************************************************************************/

static struct MachineDriver machine_driver_guwange =
{
	{
		{
			CPU_M68000,
			16000000,
			guwange_readmem, guwange_writemem,0,0,
			cave_interrupt, 1
		},
	},
	/*60*/15625/271.5,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	320, 240, { 0, 320-1, 0, 240-1 },
	esprade_gfxdecodeinfo,
	0x8000, 0x8000,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	guwange_vh_start,
	0,
	cave_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_YMZ280B, &ymz280b_intf }
	},

	cave_nvram_handler
};



/***************************************************************************
								Uo Poko
***************************************************************************/

static struct MachineDriver machine_driver_uopoko =
{
	{
		{
			CPU_M68000,
			16000000,
			uopoko_readmem, uopoko_writemem,0,0,
			cave_interrupt, 1
		},
	},
	/*60*/15625/271.5,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	320, 240, { 0, 320-1, 0, 240-1 },
	uopoko_gfxdecodeinfo,
	0x8000, 0x8000,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	uopoko_vh_start,
	0,
	cave_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_YMZ280B, &ymz280b_intf }
	},

	cave_nvram_handler
};





/***************************************************************************


								ROMs Loading


***************************************************************************/

/* 4 bits -> 8 bits. Even and odd pixels are swapped */
static void unpack_sprites(void)
{
	const int region		=	REGION_GFX4;	// sprites

	const unsigned int len	=	memory_region_length(region);
	unsigned char *src		=	memory_region(region) + len / 2 - 1;
	unsigned char *dst		=	memory_region(region) + len - 1;

	while(dst > src)
	{
		unsigned char data = *src--;
		/* swap even and odd pixels */
		*dst-- = data >> 4;		*dst-- = data & 0xF;
	}
}


/* 4 bits -> 8 bits. Even and odd pixels and even and odd words, are swapped */
static void ddonpach_unpack_sprites(void)
{
	const int region		=	REGION_GFX4;	// sprites

	const unsigned int len	=	memory_region_length(region);
	unsigned char *src		=	memory_region(region) + len / 2 - 1;
	unsigned char *dst		=	memory_region(region) + len - 1;

	while(dst > src)
	{
		unsigned char data1= *src--;
		unsigned char data2= *src--;
		unsigned char data3= *src--;
		unsigned char data4= *src--;

		/* swap even and odd pixels, and even and odd words */
		*dst-- = data2 & 0xF;		*dst-- = data2 >> 4;
		*dst-- = data1 & 0xF;		*dst-- = data1 >> 4;
		*dst-- = data4 & 0xF;		*dst-- = data4 >> 4;
		*dst-- = data3 & 0xF;		*dst-- = data3 >> 4;
	}
}


/* 2 pages of 4 bits -> 8 bits */
static void esprade_unpack_sprites(void)
{
	const int region		=	REGION_GFX4;	// sprites

	unsigned char *src		=	memory_region(region);
	unsigned char *dst		=	memory_region(region) + memory_region_length(region);

	while(src < dst)
	{
		unsigned char data1 = src[0];
		unsigned char data2 = src[1];

		src[0] = (data1 & 0xf0) + (data2 & 0x0f);
		src[1] = ((data1 & 0x0f)<<4) + ((data2 & 0xf0)>>4);

		src += 2;
	}
}




/***************************************************************************

								Dangun Feveron

Board:	CV01
OSC:	28.0, 16.0, 16.9 MHz


***************************************************************************/

ROM_START( dfeveron )

	ROM_REGION( 0x100000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "cv01-u34.bin", 0x000000, 0x080000, 0xbe87f19d )
	ROM_LOAD_ODD ( "cv01-u33.bin", 0x000000, 0x080000, 0xe53a7db3 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "cv01-u50.bin", 0x000000, 0x200000, 0x7a344417 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "cv01-u49.bin", 0x000000, 0x200000, 0xd21cdda7 )

//	ROM_REGION( 0x200000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 3 */
//	empty

	ROM_REGION( 0x800000 * 2, REGION_GFX4 )		/* Sprites: * 2 , do not dispose */
	ROM_LOAD( "cv01-u25.bin", 0x000000, 0x400000, 0xa6f6a95d )
	ROM_LOAD( "cv01-u26.bin", 0x400000, 0x400000, 0x32edb62a )

	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_LOAD( "cv01-u19.bin", 0x000000, 0x400000, 0x5f5514da )

ROM_END


void init_dfeveron(void)
{
	unpack_sprites();
	cave_spritetype = 0;	// "normal" sprites
}




/***************************************************************************

								Dodonpachi (Japan)


PCB:	AT-C03 D2
CPU:	MC68000-16
Sound:	YMZ280B
OSC:	28.0000MHz
		16.0000MHz
		16.9MHz (16.9344MHz?)

***************************************************************************/

ROM_START( ddonpach )

	ROM_REGION( 0x100000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "u27.bin", 0x000000, 0x080000, 0x2432ff9b )
	ROM_LOAD_ODD ( "u26.bin", 0x000000, 0x080000, 0x4f3a914a )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u60.bin", 0x000000, 0x200000, 0x903096a7 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "u61.bin", 0x000000, 0x200000, 0xd89b7631 )

	ROM_REGION( 0x200000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 3 */
	ROM_LOAD( "u62.bin", 0x000000, 0x200000, 0x292bfb6b )

	ROM_REGION( 0x800000 * 2, REGION_GFX4 )		/* Sprites: * 2, do not dispose */
	ROM_LOAD( "u50.bin", 0x000000, 0x200000, 0x14b260ec )
	ROM_LOAD( "u51.bin", 0x200000, 0x200000, 0xe7ba8cce )
	ROM_LOAD( "u52.bin", 0x400000, 0x200000, 0x02492ee0 )
	ROM_LOAD( "u53.bin", 0x600000, 0x200000, 0xcb4c10f0 )

	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_LOAD( "u6.bin", 0x000000, 0x200000, 0x9dfdafaf )
	ROM_LOAD( "u7.bin", 0x200000, 0x200000, 0x795b17d5 )

ROM_END


void init_ddonpach(void)
{
	ddonpach_unpack_sprites();
	cave_spritetype = 1;	// "different" sprites (no zooming?)
}


/***************************************************************************

									Esprade

ATC04
OSC:	28.0, 16.0, 16.9 MHz

***************************************************************************/


ROM_START( esprade )

	ROM_REGION( 0x100000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "u42.bin", 0x000000, 0x080000, 0x0718c7e5 )
	ROM_LOAD_ODD ( "u41.bin", 0x000000, 0x080000, 0xdef30539 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u54.bin", 0x000000, 0x400000, 0xe7ca6936 )
	ROM_LOAD( "u55.bin", 0x400000, 0x400000, 0xf53bd94f )

	ROM_REGION( 0x800000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "u52.bin", 0x000000, 0x400000, 0xe7abe7b4 )
	ROM_LOAD( "u53.bin", 0x400000, 0x400000, 0x51a0f391 )

	ROM_REGION( 0x400000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 3 */
	ROM_LOAD( "u51.bin", 0x000000, 0x400000, 0x0b9b875c )

	ROM_REGION( 0x1000000, REGION_GFX4 )		/* Sprites (do not dispose) */
	ROM_LOAD_GFX_EVEN( "u63.bin", 0x000000, 0x400000, 0x2f2fe92c )
	ROM_LOAD_GFX_ODD ( "u64.bin", 0x000000, 0x400000, 0x491a3da4 )
	ROM_LOAD_GFX_EVEN( "u65.bin", 0x800000, 0x400000, 0x06563efe )
	ROM_LOAD_GFX_ODD ( "u66.bin", 0x800000, 0x400000, 0x7bbe4cfc )

	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_LOAD( "u19.bin", 0x000000, 0x400000, 0xf54b1cab )

ROM_END


void init_esprade(void)
{
	esprade_unpack_sprites();
	cave_spritetype = 0;	// "normal" sprites
}




/***************************************************************************

								Guwange (Japan)

PCB:	ATC05
CPU:	MC68000-16
Sound:	YMZ280B
OSC:	28.0000MHz
		16.0000MHz
		16.9MHz

***************************************************************************/

ROM_START( guwange )

	ROM_REGION( 0x100000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "gu-u0127.bin", 0x000000, 0x080000, 0xf86b5293 )
	ROM_LOAD_ODD ( "gu-u0129.bin", 0x000000, 0x080000, 0x6c0e3b93 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u101.bin", 0x000000, 0x800000, 0x0369491f )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "u10102.bin", 0x000000, 0x400000, 0xe28d6855 )

	ROM_REGION( 0x400000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 3 */
	ROM_LOAD( "u10103.bin", 0x000000, 0x400000, 0x0fe91b8e )

	ROM_REGION( 0x1800000, REGION_GFX4 )		/* Sprites (do not dispose) */
	ROM_LOAD_GFX_EVEN( "u083.bin", 0x0000000, 0x800000, 0xadc4b9c4 )
	ROM_LOAD_GFX_ODD(  "u082.bin", 0x0000000, 0x800000, 0x3d75876c )
	ROM_LOAD_GFX_EVEN( "u086.bin", 0x1000000, 0x400000, 0x188e4f81 )
	ROM_LOAD_GFX_ODD(  "u085.bin", 0x1000000, 0x400000, 0xa7d5659e )

	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_LOAD( "u0462.bin", 0x000000, 0x400000, 0xb3d75691 )

ROM_END




/***************************************************************************

									Uo Poko
Board: CV02
OSC:	28.0, 16.0, 16.9 MHz

***************************************************************************/

ROM_START( uopoko )

	ROM_REGION( 0x100000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "u26j.bin", 0x000000, 0x080000, 0xe7eec050 )
	ROM_LOAD_ODD ( "u25j.bin", 0x000000, 0x080000, 0x68cb6211 )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u49.bin", 0x000000, 0x400000, 0x12fb11bb )

//	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 2 */
//	empty

//	ROM_REGION( 0x200000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 3 */
//	empty

	ROM_REGION( 0x400000 * 2, REGION_GFX4 )		/* Sprites: * 2 , do not dispose */
	ROM_LOAD( "u33.bin", 0x000000, 0x400000, 0x5d142ad2 )

	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_LOAD( "u4.bin", 0x000000, 0x200000, 0xa2d0d755 )

ROM_END


void init_uopoko(void)
{
	unpack_sprites();
	cave_spritetype = 0;	// "normal" sprites
}




/***************************************************************************


								Game Drivers


***************************************************************************/

GAME( 1997, ddonpach, 0, ddonpach, dfeveron, ddonpach, ROT90_16BIT, "Atlus/Cave",                  "Dodonpachi (Japan)"     )
GAME( 1998, dfeveron, 0, dfeveron, dfeveron, dfeveron, ROT270_16BIT, "Cave (Nihon System license)", "Dangun Feveron (Japan)" )
GAME( 1998, esprade,  0, esprade,  dfeveron, esprade,  ROT270_16BIT, "Atlus/Cave",                  "ESP Ra.De. (Japan)"     )
GAME( 1998, uopoko,   0, uopoko,   dfeveron, uopoko,   ROT0_16BIT,   "Cave (Jaleco license)",       "Uo Poko (Japan)"        )
GAME( 1999, guwange,  0, guwange,  guwange,  esprade,  ROT270_16BIT, "Atlus/Cave",                  "Guwange (Japan)"        )
