#include "../vidhrdw/cischeat.cpp"

/***************************************************************************

						Cisco Heat & F1 GrandPrix Star
							(c) 1990 & 1991 Jaleco


				    driver by Luca Elia (eliavit@unina.it)


----------------------------------------------------------------------
Hardware		Main 	Sub#1	Sub#2	Sound	Sound Chips
----------------------------------------------------------------------
[Cisco Heat]	68000	68000	68000	68000	YM2151 2xM6295

	BOARD #1 CH-9072 EB90001-20024
	BOARD #2 CH-9071 EB90001-20023
	BOARD #3 CH-9073 EB90001-20025

[F1 GP Star]	68000	68000	68000	68000	YM2151 2xM6295

	TB - Top board     (audio & I/O)       GP-9190A EB90015-20039-1
	MB - Middle board  (GFX)               GP-9189  EB90015-20038
	LB - Lower board   (CPU/GFX)           GP-9188A EB90015-20037-1

----------------------------------------------------------------------


----------------------------------------------------------------
Main CPU					[Cisco Heat]		[F1 GP Star]
----------------------------------------------------------------
ROM					R		000000-07ffff		<
							100000-17ffff		<
Work RAM			RW		0f0000-0fffff		<
Hardware Regs		RW		080000-087fff		<
Units Linking RAM	RW		088000-88ffff		<
Shared RAM #2		RW		090000-097fff		<
Shared RAM #1		RW		098000-09ffff		<
Scroll RAM 0		RW		0a0000-0a7fff		<
Scroll RAM 1		RW		0a8000-0affff		<
Scroll RAM 2		RW		0b0000-0b7fff		<
Palette RAM			RW		0b8000-0bffff		<
	Palette Scroll 0		0b9c00-0b9fff		0b9e00-0b9fff
	Palette Scroll 1		0bac00-0bafff		0bae00-0bafff
	Palette Road 0			0bb800-0bbfff		<
	Palette Road 1			0bc800-0bcfff		<
	Palette Sprites			0bd000-0bdfff		<
	Palette Scroll 2		0bec00-0befff0		bee00-0befff
----------------------------------------------------------------


----------------------------------------------------------------
Sub CPU's					[Cisco Heat]		[F1 GP Star]
----------------------------------------------------------------
ROM					R		000000-03ffff		<
							200000-23ffff		-
Work RAM			RW		0c0000-0c3fff		180000-183fff
Shared RAM			RW		040000-047fff		080000-087fff
Road RAM			RW		080000-0807ff		100000-1007ff
Whatchdog					100000-100001		200000-200001
----------------------------------------------------------------


----------------------------------------------------------------
Sound CPU					[Cisco Heat]		[F1 GP Star]
----------------------------------------------------------------
ROM					R		000000-03ffff		<
Work RAM			RW		0f0000-0fffff		0e0000-0fffff
M6295 #1 Banking	 W		040002-040003		040004-040005
M6295 #2 Banking	 W		040004-040005		040008-040009
Sound Latch			 W		060002-060003		060000-060001
Sound Latch			R		060004-060005		060000-060001
YM2151 Reg Sel		 W		080001-080001		<
YM2151 Data			 W		080003-080003		<
YM2151 Status		R		080003-080003		<
M6295 #1 Status		R		0a0001-0a0001		<
M6295 #1 Data		 W		0a0000-0a0003		<
M6295 #2 Status		R		0c0001-0c0001		<
M6295 #2 Data		 W		0c0000-0c0003		<
----------------------------------------------------------------


---------------------------------------------------------------------------
								  Game code
								[ Cisco Heat ]
---------------------------------------------------------------------------

f011a.w		*** stage - 1 ***
f0190.l		*** score / 10 (BCD) ***
f0280.w		*** time * 10 (seconds) ***

f61Xa.w		car X data

---------------------------------------------------------------------------
								  Game code
							[ F1 GrandPrix Star ]
---------------------------------------------------------------------------

Note: This game has some leftover code from Cisco Heat, it seems.

f9088.w		*** lap - 1 ***
fa008.w		($fa000 + $08) *** time (seconds) ***
fa2aa.l		($fa200 + $aa) speed << 16


---------------------------------------------------------------------------
							   Common Issues
---------------------------------------------------------------------------

- Some ROMs aren't used. I don't see big problems though.

- Screen control register (priorities, layers enabling etc.) - Where is it?

- In cischeat, at the start of some levels, you can see the empty scrolling
  layers as they are filled. In f1gpstar, I'm unsure whether they are
  correct in a few places: in the attract mode where the cars move
  horizontally, for example, the wheels don't follow for this reason, I
  think

- Sound communication not quite right: see Test Mode

---------------------------------------------------------------------------
								   To Do
---------------------------------------------------------------------------

- Use the Sprite Manager (when SPRITE_TYPE_UNPACK and SPRITE_TYPE_ZOOM
  will be usable together) for better looking rendering, better priority
  support and shadow sprites

- Use the Tilemap Manager for the road layers (when this kind of layers
  will be supported) for perfomance and better priority support.
  A line based zooming should be possible, for f1gpstar..

- Force feedback :)

***************************************************************************/

#include "driver.h"
#include "drivers/megasys1.h"

/* Variables only used here: */
static unsigned char *rom_1, *rom_2, *rom_3;
static unsigned char *sharedram1, *sharedram2;

/* Variables that vidhrdw has access to: */

/* Variables defined in vidhrdw: */
extern unsigned char *cischeat_roadram[2];

/* Functions defined in vidhrdw: */
READ_HANDLER( cischeat_vregs_r );
READ_HANDLER( f1gpstar_vregs_r );

WRITE_HANDLER( cischeat_vregs_w );
WRITE_HANDLER( f1gpstar_vregs_w );

int cischeat_vh_start(void);
int f1gpstar_vh_start(void);

void cischeat_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void f1gpstar_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


/*
**
**				CPU # 1 (Main)
**
*/

READ_HANDLER( sharedram1_r )			{return READ_WORD(&sharedram1[offset]);	}
READ_HANDLER( sharedram2_r )			{return READ_WORD(&sharedram2[offset]);	}

WRITE_HANDLER( sharedram1_w ) {WRITE_WORD(&sharedram1[offset],data);	}
WRITE_HANDLER( sharedram2_w ) {WRITE_WORD(&sharedram2[offset],data);	}

static READ_HANDLER( rom_1_r ) {return READ_WORD(&rom_1[offset]);}
static READ_HANDLER( rom_2_r ) {return READ_WORD(&rom_2[offset]);}
static READ_HANDLER( rom_3_r ) {return READ_WORD(&rom_3[offset]);}


/**************************************************************************
								[ Cisco Heat ]
**************************************************************************/


/*	CISCO HEAT
	[  Test  ]		[  Real  ]
	b9c00-b9fff		<				scroll 0
	bac00-bafff		<				scroll 1
	bb800-bbbff		bb800-bbfff		road 0
	bc800-bcbff		bc800-bcfff		road 1
	bd000-bd3ff		bd000-bdfff		sprites
	bec00-befff		<				text		*/

READ_HANDLER( cischeat_palette_r )
{
	return READ_WORD(&paletteram[offset]);
}

WRITE_HANDLER( cischeat_palette_w )
{
int newword,r,g,b;

	COMBINE_WORD_MEM(&paletteram[offset],data);
	newword = READ_WORD(&paletteram[offset]);
	r = ((newword >> 8) & 0xF0 ) | ((newword << 0) & 0x08);
	g = ((newword >> 4) & 0xF0 ) | ((newword << 1) & 0x08);
	b = ((newword >> 0) & 0xF0 ) | ((newword << 2) & 0x08);

	// Scroll 0
	if ( (offset >= 0x1c00) && (offset <= 0x1fff) ) { palette_change_color(0x000 + (offset - 0x1c00)/2, r,g,b ); return;}
	// Scroll 1
	if ( (offset >= 0x2c00) && (offset <= 0x2fff) ) { palette_change_color(0x200 + (offset - 0x2c00)/2, r,g,b ); return;}
	// Scroll 2
	if ( (offset >= 0x6c00) && (offset <= 0x6fff) ) { palette_change_color(0x400 + (offset - 0x6c00)/2, r,g,b ); return;}
	// Road 0
	if ( (offset >= 0x3800) && (offset <= 0x3fff) ) { palette_change_color(0x600 + (offset - 0x3800)/2, r,g,b ); return;}
	// Road 1
	if ( (offset >= 0x4800) && (offset <= 0x4fff) ) { palette_change_color(0xa00 + (offset - 0x4800)/2, r,g,b ); return;}
	// Sprites
	if ( (offset >= 0x5000) && (offset <= 0x5fff) ) { palette_change_color(0xe00 + (offset - 0x5000)/2, r,g,b ); return;}
}



static struct MemoryReadAddress cischeat_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM			},	// ROM
	{ 0x100000, 0x17ffff, rom_1_r			},	// ROM
	{ 0x0f0000, 0x0fffff, MRA_BANK1			},	// RAM
	{ 0x080000, 0x087fff, cischeat_vregs_r	},	// Vregs
	{ 0x088000, 0x088fff, MRA_BANK2			},	// Linking with other units

/* 	Only the first 0x800 bytes are tested but:
	CPU #0 PC 0000278c: warning - write 68c0 to unmapped memory address 0009c7fe
	CPU #0 PC 0000dd58: warning - read unmapped memory address 000945ac
	No mem access error from the other CPU's, though.. */

	/* this is the right order of sharedram's */
	{ 0x090000, 0x097fff, sharedram2_r		},	// Sharedram with sub CPU#2
	{ 0x098000, 0x09ffff, sharedram1_r		},	// Sharedram with sub CPU#1

	{ 0x0a0000, 0x0a7fff, megasys1_scrollram_0_r	},	// Scroll ram 0
	{ 0x0a8000, 0x0affff, megasys1_scrollram_1_r	},	// Scroll ram 1
	{ 0x0b0000, 0x0b7fff, megasys1_scrollram_2_r	},	// Scroll ram 2

	{ 0x0b8000, 0x0bffff, cischeat_palette_r	},	// Palettes

	{ -1 }
};

static struct MemoryWriteAddress cischeat_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM							},	// ROM
	{ 0x100000, 0x17ffff, MWA_ROM							},	// ROM
	{ 0x0f0000, 0x0fffff, MWA_BANK1, &megasys1_ram			},	// RAM
	{ 0x080000, 0x087fff, cischeat_vregs_w, &megasys1_vregs	},	// Vregs
	{ 0x088000, 0x088fff, MWA_BANK2							},	// Linking with other units

	{ 0x090000, 0x097fff, sharedram2_w, &sharedram2			},	// Sharedram with sub CPU#2
	{ 0x098000, 0x09ffff, sharedram1_w, &sharedram1			},	// Sharedram with sub CPU#1

	/* Only writes to the first 0x40000 bytes affect the tilemaps:             */
	/* either these games support larger tilemaps or have more ram than needed */
	{ 0x0a0000, 0x0a7fff, megasys1_scrollram_0_w, &megasys1_scrollram_0	},	// Scroll ram 0
	{ 0x0a8000, 0x0affff, megasys1_scrollram_1_w, &megasys1_scrollram_1	},	// Scroll ram 1
	{ 0x0b0000, 0x0b7fff, megasys1_scrollram_2_w, &megasys1_scrollram_2	},	// Scroll ram 2

	{ 0x0b8000, 0x0bffff, cischeat_palette_w, &paletteram	},	// Palettes

	{ -1 }
};





/**************************************************************************
							[ F1 GrandPrix Star ]
**************************************************************************/


READ_HANDLER( f1gpstar_palette_r )
{
	return READ_WORD(&paletteram[offset]);
}
WRITE_HANDLER( f1gpstar_palette_w )
{
int newword,r,g,b;

	COMBINE_WORD_MEM(&paletteram[offset],data);
	newword = READ_WORD(&paletteram[offset]);
	r = ((newword >> 8) & 0xF0 ) | ((newword << 0) & 0x08);
	g = ((newword >> 4) & 0xF0 ) | ((newword << 1) & 0x08);
	b = ((newword >> 0) & 0xF0 ) | ((newword << 2) & 0x08);

	// Scroll 0
	if ( (offset >= 0x1e00) && (offset <= 0x1fff) ) { palette_change_color(0x000 + (offset - 0x1e00)/2, r,g,b ); return;}
	// Scroll 1
	if ( (offset >= 0x2e00) && (offset <= 0x2fff) ) { palette_change_color(0x100 + (offset - 0x2e00)/2, r,g,b ); return;}
	// Scroll 2
	if ( (offset >= 0x6e00) && (offset <= 0x6fff) ) { palette_change_color(0x200 + (offset - 0x6e00)/2, r,g,b ); return;}
	// Road 0
	if ( (offset >= 0x3800) && (offset <= 0x3fff) ) { palette_change_color(0x300 + (offset - 0x3800)/2, r,g,b ); return;}
	// Road 1
	if ( (offset >= 0x4800) && (offset <= 0x4fff) ) { palette_change_color(0x700 + (offset - 0x4800)/2, r,g,b ); return;}
	// Sprites
	if ( (offset >= 0x5000) && (offset <= 0x5fff) ) { palette_change_color(0xb00 + (offset - 0x5000)/2, r,g,b ); return;}
}

/*	F1 GP Star tests:
	0A0000-0B8000
	0F0000-100000
	0B8000-0C0000
	090800-091000
	098800-099000
	0F8000-0F9000	*/

static struct MemoryReadAddress f1gpstar_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM			},	// ROM
	{ 0x100000, 0x17ffff, rom_1_r			},	// ROM
	{ 0x0f0000, 0x0fffff, MRA_BANK1			},	// RAM
	{ 0x080000, 0x087fff, f1gpstar_vregs_r	},	// Vregs
	{ 0x088000, 0x088fff, MRA_BANK2			},	// Linking with other units

	{ 0x090000, 0x097fff, sharedram2_r		},	// Sharedram with sub CPU#2
	{ 0x098000, 0x09ffff, sharedram1_r		},	// Sharedram with sub CPU#1

	{ 0x0a0000, 0x0a7fff, megasys1_scrollram_0_r	},	// Scroll ram 0
	{ 0x0a8000, 0x0affff, megasys1_scrollram_1_r	},	// Scroll ram 1
	{ 0x0b0000, 0x0b7fff, megasys1_scrollram_2_r	},	// Scroll ram 2

	{ 0x0b8000, 0x0bffff, f1gpstar_palette_r	},	// Palettes
	{ -1 }
};

static struct MemoryWriteAddress f1gpstar_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM							},
	{ 0x100000, 0x17ffff, MWA_ROM							},
	{ 0x0f0000, 0x0fffff, MWA_BANK1, &megasys1_ram			},	// RAM
	{ 0x080000, 0x087fff, f1gpstar_vregs_w, &megasys1_vregs	},	// Vregs
	{ 0x088000, 0x088fff, MWA_BANK2							},	// Linking with other units

	{ 0x090000, 0x097fff, sharedram2_w, &sharedram2			},	// Sharedram with sub CPU#2
	{ 0x098000, 0x09ffff, sharedram1_w, &sharedram1			},	// Sharedram with sub CPU#1

	/* Only writes to the first 0x40000 bytes affect the tilemaps:             */
	/* either these games support larger tilemaps or have more ram than needed */
	{ 0x0a0000, 0x0a7fff, megasys1_scrollram_0_w, &megasys1_scrollram_0	},	// Scroll ram 0
	{ 0x0a8000, 0x0affff, megasys1_scrollram_1_w, &megasys1_scrollram_1	},	// Scroll ram 1
	{ 0x0b0000, 0x0b7fff, megasys1_scrollram_2_w, &megasys1_scrollram_2	},	// Scroll ram 2

	{ 0x0b8000, 0x0bffff, f1gpstar_palette_w, &paletteram	},	// Palettes

	{ -1 }
};





/*
**
**				CPU # 2 (Road)
**
*/

/**************************************************************************
								[ Cisco Heat ]
**************************************************************************/

static struct MemoryReadAddress cischeat_readmem2[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM		},	// ROM
	{ 0x200000, 0x23ffff, rom_2_r		},	// ROM
	{ 0x0c0000, 0x0c3fff, MRA_BANK3		},	// RAM
	{ 0x040000, 0x047fff, sharedram1_r	},	// Shared RAM (with Main CPU)
	{ 0x080000, 0x0807ff, MRA_BANK4		},	// Road RAM
	{ -1 }
};
static struct MemoryWriteAddress cischeat_writemem2[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM							},	// ROM
	{ 0x200000, 0x23ffff, MWA_ROM							},	// ROM
	{ 0x0c0000, 0x0c3fff, MWA_BANK3							},	// RAM
	{ 0x040000, 0x047fff, sharedram1_w						},	// Shared RAM (with Main CPU)
	{ 0x080000, 0x0807ff, MWA_BANK4, &cischeat_roadram[0]	},	// Road RAM
	{ 0x100000, 0x100001, MWA_NOP							},	// watchdog
	{ -1 }
};




/**************************************************************************
							[ F1 GrandPrix Star ]
**************************************************************************/

static struct MemoryReadAddress f1gpstar_readmem2[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM		},	// ROM
	{ 0x180000, 0x183fff, MRA_BANK3		},	// RAM
	{ 0x080000, 0x0807ff, sharedram1_r	},	// Shared RAM (with Main CPU)
	{ 0x100000, 0x1007ff, MRA_BANK4		},	// Road RAM
	{ -1 }
};
static struct MemoryWriteAddress f1gpstar_writemem2[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM							},	// ROM
	{ 0x180000, 0x183fff, MWA_BANK3							},	// RAM
	{ 0x080000, 0x0807ff, sharedram1_w						},	// Shared RAM (with Main CPU)
	{ 0x100000, 0x1007ff, MWA_BANK4, &cischeat_roadram[0]	},	// Road RAM
	{ 0x200000, 0x200001, MWA_NOP							},	// watchdog
	{ -1 }
};


/*
**
**				CPU # 3 (Road)
**
*/

/**************************************************************************
								[ Cisco Heat ]
**************************************************************************/

static struct MemoryReadAddress cischeat_readmem3[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM		},	// ROM
	{ 0x200000, 0x23ffff, rom_3_r		},	// ROM
	{ 0x0c0000, 0x0c3fff, MRA_BANK5		},	// RAM
	{ 0x040000, 0x047fff, sharedram2_r	},	// Shared RAM (with Main CPU)
	{ 0x080000, 0x0807ff, MRA_BANK6		},	// Road RAM
	{ -1 }
};
static struct MemoryWriteAddress cischeat_writemem3[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM							},	// ROM
	{ 0x200000, 0x23ffff, MWA_ROM							},	// ROM
	{ 0x0c0000, 0x0c3fff, MWA_BANK5							},	// RAM
	{ 0x040000, 0x047fff, sharedram2_w						},	// Shared RAM (with Main CPU)
	{ 0x080000, 0x0807ff, MWA_BANK6, &cischeat_roadram[1]	},	// Road RAM
	{ 0x100000, 0x100001, MWA_NOP							},	// watchdog
	{ -1 }
};





/**************************************************************************
							[ F1 GrandPrix Star ]
**************************************************************************/

static struct MemoryReadAddress f1gpstar_readmem3[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM		},	// ROM
	{ 0x180000, 0x183fff, MRA_BANK5		},	// RAM
	{ 0x080000, 0x0807ff, sharedram2_r	},	// Shared RAM (with Main CPU)
	{ 0x100000, 0x1007ff, MRA_BANK6		},	// Road RAM
	{ -1 }
};
static struct MemoryWriteAddress f1gpstar_writemem3[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM							},	// ROM
	{ 0x180000, 0x183fff, MWA_BANK5							},	// RAM
	{ 0x080000, 0x0807ff, sharedram2_w						},	// Shared RAM (with Main CPU)
	{ 0x100000, 0x1007ff, MWA_BANK6, &cischeat_roadram[1]	},	// Road RAM
	{ 0x200000, 0x200001, MWA_NOP							},	// watchdog
	{ -1 }
};



/*
**
**				CPU # 4 (Sound)
**
*/

/* Both Games: music tempo driven by the YM2151 timers (status reg polled) */


/**************************************************************************
								[ Cisco Heat ]
**************************************************************************/

#define SOUNDBANK_W(_n_) \
WRITE_HANDLER( cischeat_soundbank_##_n_##_w ) \
{ \
	OKIM6295_set_bank_base(_n_, ALL_VOICES, 0x40000 * (data & 1) ); \
}

SOUNDBANK_W(0)
SOUNDBANK_W(1)

static struct MemoryReadAddress cischeat_sound_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM					},	// ROM
	{ 0x0f0000, 0x0fffff, MRA_BANK7					},	// RAM
	{ 0x060004, 0x060005, soundlatch_r				},	// From Main CPU
	{ 0x080002, 0x080003, YM2151_status_port_0_r	},
	{ 0x0a0000, 0x0a0001, OKIM6295_status_0_r		},
	{ 0x0c0000, 0x0c0001, OKIM6295_status_1_r		},
	{ -1 }
};
static struct MemoryWriteAddress cischeat_sound_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM						},	// ROM
	{ 0x0f0000, 0x0fffff, MWA_BANK7						},	// RAM
	{ 0x040002, 0x040003, cischeat_soundbank_0_w		},	// Sample Banking
	{ 0x040004, 0x040005, cischeat_soundbank_1_w		},	// Sample Banking
	{ 0x060002, 0x060003, ms_soundlatch2_w				},	// To Main CPU
	{ 0x080000, 0x080001, ms_YM2151_register_port_0_w	},
	{ 0x080002, 0x080003, ms_YM2151_data_port_0_w		},
	{ 0x0a0000, 0x0a0003, ms_OKIM6295_data_0_w			},
	{ 0x0c0000, 0x0c0003, ms_OKIM6295_data_1_w			},
	{ -1 }
};





/**************************************************************************
							[ F1 GrandPrix Star ]
**************************************************************************/

static struct MemoryReadAddress f1gpstar_sound_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM					},	// ROM
	{ 0x0e0000, 0x0fffff, MRA_BANK7					},	// RAM				(cischeat: f0000-fffff)
	{ 0x060000, 0x060001, soundlatch_r				},	// From Main CPU	(cischeat: 60004)
	{ 0x080002, 0x080003, YM2151_status_port_0_r	},
	{ 0x0a0000, 0x0a0001, OKIM6295_status_0_r		},
	{ 0x0c0000, 0x0c0001, OKIM6295_status_1_r		},
	{ -1 }
};
static struct MemoryWriteAddress f1gpstar_sound_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM						},	// ROM
	{ 0x0e0000, 0x0fffff, MWA_BANK7						},	// RAM				(cischeat: f0000-fffff)
	{ 0x040004, 0x040005, cischeat_soundbank_0_w		},	// Sample Banking	(cischeat: 40002)
	{ 0x040008, 0x040009, cischeat_soundbank_1_w		},	// Sample Banking	(cischeat: 40004)
	{ 0x060000, 0x060001, ms_soundlatch2_w				},	// To Main CPU		(cischeat: 60002)
	{ 0x080000, 0x080001, ms_YM2151_register_port_0_w	},
	{ 0x080002, 0x080003, ms_YM2151_data_port_0_w		},
	{ 0x0a0000, 0x0a0003, ms_OKIM6295_data_0_w			},
	{ 0x0c0000, 0x0c0003, ms_OKIM6295_data_1_w			},
	{ -1 }
};






/***************************************************************************

								Input Ports

***************************************************************************/

#define DRIVING_WHEEL \
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_CENTER, 30, 30, 0, 0xff)

/* Fake input port to read the status of five buttons: used to
   implement the shift using 2 buttons, and the accelerator in
   f1gpstar */

#define BUTTONS_STATUS \
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) \
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 ) \
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 ) \
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON5 )

/**************************************************************************
								[ Cisco Heat ]
**************************************************************************/


//	Input Ports:	[0] Fake: Buttons Status
//					[1] Coins		[2] Controls	[3] Unknown
//					[4]	DSW 1 & 2	[5] DSW 3		[6] Driving Wheel

INPUT_PORTS_START( cischeat )

	PORT_START	// IN0 - Fake input port - Buttons status
	BUTTONS_STATUS


	PORT_START	// IN1 - Coins - $80000.w
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_COIN1   )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_COIN2   )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_COIN3   )	// operator's facility
	PORT_BITX( 0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) 	// called "Test"
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )


	PORT_START	// IN2 - Controls - $80002.w
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )	// Brake
//	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_BUTTON4 )	// Shift - We handle it using buttons 3&4
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )	// Accel
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_BUTTON5 )	// Horn


	PORT_START	// IN3 - Motor Control? - $80004.w
	PORT_DIPNAME( 0x01, 0x01, "Up Limit SW"  	)	// Limit the Cockpit movements?
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On )  )
	PORT_DIPNAME( 0x02, 0x02, "Down Limit SW"	)
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On )  )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x10, 0x10, "Right Limit SW"	)
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On )  )
	PORT_DIPNAME( 0x20, 0x20, "Left Limit SW"	)
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On )  )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )


	PORT_START	// IN4 - DSW 1 & 2 - $80006.w -> !f000a.w(hi byte) !f0008.w(low byte)
	COINAGE_6BITS_2
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )	// unused?
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )	// unused?
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	// DSW 2
	PORT_DIPNAME( 0x0300, 0x0300, "Unit ID"			)		// -> !f0020 (ID of this unit, when linked)
	PORT_DIPSETTING(      0x0300, "0 (Red Car)"    )
	PORT_DIPSETTING(      0x0200, "1 (Blue Car)"   )
	PORT_DIPSETTING(      0x0100, "2 (Yellow Car)" )
	PORT_DIPSETTING(      0x0000, "3 (Green Car)"  )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Difficulty ) )	// -> !f0026
	PORT_DIPSETTING(      0x0000, "Easy"    )
	PORT_DIPSETTING(      0x0c00, "Normal"  )
	PORT_DIPSETTING(      0x0800, "Hard"    )
	PORT_DIPSETTING(      0x0400, "Hardest" )
	PORT_BITX(    0x1000, 0x1000, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Time", IP_KEY_NONE, IP_JOY_NONE ) // -> !f0028
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( On )  )
	PORT_DIPNAME( 0x4000, 0x4000, "Country" )
	PORT_DIPSETTING(      0x4000, "Japan" )
	PORT_DIPSETTING(      0x0000, "USA"   )
	PORT_DIPNAME( 0x8000, 0x8000, "Allow Continue" )		// -> !f00c0
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )


	PORT_START	// IN5 - DSW 3 (4 bits, Cabinet Linking) - $82200.w
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x06, 0x06, "Unit ID (2)" )	// -> f0020 (like DSW2 !!)
	PORT_DIPSETTING(    0x06, "Use other"      )
	PORT_DIPSETTING(    0x00, "0 (Red Car)"    )
	PORT_DIPSETTING(    0x02, "1 (Blue Car)"   )
	PORT_DIPSETTING(    0x04, "2 (Yellow Car)" )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN6 - Driving Wheel - $80010.w(0)
	DRIVING_WHEEL

INPUT_PORTS_END







/**************************************************************************
							[ F1 GrandPrix Star ]
**************************************************************************/

//	Input Ports:	[0] Fake: Buttons Status
//					[1] DSW 1 & 2		[2] Controls		[3] Unknown
//					[4]	DSW 3			[5] Driving Wheel
//					[6]	Coinage JP&USA	[7] Coinage UK&FR

INPUT_PORTS_START( f1gpstar )


	PORT_START	// IN0 - Fake input port - Buttons status
	BUTTONS_STATUS

/*	[Country]
	Japan		"race together" in Test Mode, Always Choose Race
				Japanese, Km/h, "handle shock"  , "(c)1991",
	USA			English,  Mph , "steering shock", "(c)1992 North America Only"
	England		English,  Mph , "steering shock", "(c)1992"
	France		French,   Km/h, "steering shock", "(c)1992"	*/

	PORT_START	// IN1 - DSW 1 & 2 - $80000.w	-> !f9012
	// DSW 1 ( Coinage - it changes with Country: we use IN6 & IN7 )
	PORT_DIPNAME( 0x0040, 0x0040, "Free Play (UK FR)" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )	// unused?
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	// DSW 2
	PORT_DIPNAME( 0x0300, 0x0300, "Country"  )	// -> !f901e
	PORT_DIPSETTING(      0x0300, "Japan"   )
	PORT_DIPSETTING(      0x0200, "USA"     )
	PORT_DIPSETTING(      0x0100, "UK"      )
	PORT_DIPSETTING(      0x0000, "France"  )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Difficulty ) )	// -> !f9026
	PORT_DIPSETTING(      0x0000, "Easy"      )	// 58 <- Initial Time (seconds, Germany)
	PORT_DIPSETTING(      0x0c00, "Normal"    )	// 51
	PORT_DIPSETTING(      0x0800, "Hard"      )	// 48
	PORT_DIPSETTING(      0x0400, "Very Hard" )	// 46
	PORT_BITX(    0x1000, 0x1000, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Time", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )	// ?
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( On )  )
	PORT_DIPNAME( 0x4000, 0x4000, "Choose Race (US UK FR)"  )	// -> f0020
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On )  )
	PORT_DIPNAME( 0x8000, 0x8000, "Vibrations" )
	PORT_DIPSETTING(      0x8000, "High?" )
	PORT_DIPSETTING(      0x0000, "Low?"  )


	PORT_START	// IN2 - Controls - $80004.w -> !f9016
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_COIN1   )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_COIN2   )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_COIN3   )	// operator's facility
	PORT_BITX( 0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) 	// -> f0100 (called "Test")
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_START1  )
//	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON4 )	// Shift -> !f900e - We handle it with 2 buttons
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON2 )	// Brake -> !f9010
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START2  )	// "Race Together"


	PORT_START	// IN3 - ? Read at boot only - $80006.w
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

/*	DSW3-2&1 (Country: JP)	Effect
	OFF-OFF					Red-White Car
	OFF- ON					Red Car
	ON-OFF					Blue-White Car
	ON- ON					Blue Car, "equipped with communication link"	*/

	PORT_START	// IN4 - DSW 3 (4 bits, Cabinet Linking) - $8000c.w -> !f9014
	PORT_DIPNAME( 0x01, 0x01, "This Unit Is" )
	PORT_DIPSETTING(    0x01, "Slave" )
	PORT_DIPSETTING(    0x00, "Master" )
	PORT_DIPNAME( 0x06, 0x06, "Unit ID" )			// -> !f901c
	PORT_DIPSETTING(    0x06, "0 (Red-White Car)" )
	PORT_DIPSETTING(    0x04, "1 (Red Car)" )
	PORT_DIPSETTING(    0x02, "2 (Blue-White Car)" )
	PORT_DIPSETTING(    0x00, "3 (Blue Car)" )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	// Redundant: Invert Unit ID
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )


				// 		 Accelerator   - $80010.b ->  !f9004.w
	PORT_START	// IN5 - Driving Wheel - $80011.b ->  !f9008.w
	DRIVING_WHEEL

	PORT_START	// IN6 - Coinage Japan & USA (it changes with Country)
	PORT_DIPNAME( 0x0007, 0x0007, "Coin A (JP US)" )
	PORT_DIPSETTING(      0x0001, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0038, 0x0038, "Coin B (JP US)" )
	PORT_DIPSETTING(      0x0008, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )

	PORT_START	// IN7 - Coinage UK & France (it changes with Country)
	PORT_DIPNAME( 0x0007, 0x0007, "Coin A (UK FR)" )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x0038, 0x0038, "Coin B (UK FR)" )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )

INPUT_PORTS_END








/*
**
** 			Gfx data
**
*/

/* Tiles are 8x8 */
MEGASYS1_LAYOUT_8x8(tiles_8x8_01,  0x010000)
MEGASYS1_LAYOUT_8x8(tiles_8x8_02,  0x020000)
MEGASYS1_LAYOUT_8x8(tiles_8x8_04,  0x040000)
MEGASYS1_LAYOUT_8x8(tiles_8x8_08,  0x080000)

/* Sprites are 16x16 */
MEGASYS1_LAYOUT_16x16(tiles_16x16_4M,  0x400000)
MEGASYS1_LAYOUT_16x16(tiles_16x16_5M,  0x500000)


/* Road: 64 x 1 x 4 */
#define ROAD_LAYOUT( _name_, _romsize_ ) \
static struct GfxLayout _name_ = \
{ \
	64,1, \
	8*(_romsize_)/(64*1*4), \
	4, \
	{0,1,2,3}, \
	{0*4,1*4,2*4,3*4,4*4,5*4,6*4,7*4, \
	 8*4,9*4,10*4,11*4,12*4,13*4,14*4,15*4, \
	 16*4,17*4,18*4,19*4,20*4,21*4,22*4,23*4, \
	 24*4,25*4,26*4,27*4,28*4,29*4,30*4,31*4, \
\
	 32*4,33*4,34*4,35*4,36*4,37*4,38*4,39*4, \
	 40*4,41*4,42*4,43*4,44*4,45*4,46*4,47*4, \
	 48*4,49*4,50*4,51*4,52*4,53*4,54*4,55*4, \
	 56*4,57*4,58*4,59*4,60*4,61*4,62*4,63*4}, \
	{0}, \
	64*1*4 \
};

ROAD_LAYOUT( road_layout_1M, 0x100000 )
ROAD_LAYOUT( road_layout_2M, 0x200000 )


/**************************************************************************
								[ Cisco Heat ]
**************************************************************************/

static struct GfxDecodeInfo cischeat_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tiles_8x8_04,	32*16*0, 32  }, // [0] Scroll 0
	{ REGION_GFX2, 0, &tiles_8x8_04,	32*16*1, 32  }, // [1] Scroll 1
	{ REGION_GFX3, 0, &tiles_8x8_01,	32*16*2, 32  }, // [2] Scroll 2
	{ REGION_GFX4, 0, &road_layout_1M,	32*16*3, 64  }, // [3] Road 0
	{ REGION_GFX4, 0, &road_layout_1M,	32*16*5, 64  }, // [4] Road 1
	{ REGION_GFX5, 0, &tiles_16x16_4M,	32*16*7, 128 }, // [5] Sprites
	{ -1 }
};


/**************************************************************************
							[ F1 GrandPrix Star ]
**************************************************************************/

static struct GfxDecodeInfo f1gpstar_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tiles_8x8_08,	0x0000, 16  }, // [0] Scroll 0
	{ REGION_GFX2, 0, &tiles_8x8_08,	0x0100, 16  }, // [1] Scroll 1
	{ REGION_GFX3, 0, &tiles_8x8_02,	0x0200, 16  }, // [2] Scroll 2
	{ REGION_GFX4, 0, &road_layout_2M,	0x0300, 64  }, // [3] Road 0
	{ REGION_GFX5, 0, &road_layout_1M,	0x0700, 64  }, // [4] Road 1
	{ REGION_GFX6, 0, &tiles_16x16_5M,	0x0b00, 128 }, // [5] Sprites
	{ -1 }
};




/* CPU # 1 */
#define CISCHEAT_INTERRUPT_NUM	3

int cischeat_interrupt(void)
{
	if (cpu_getiloops()==0)	return 4; /* Once */
	else
	{
		if (cpu_getiloops()%2)	return 2;
		else 					return 1;
	}
}




/* CPU # 2 & 3 */
#define CISCHEAT_SUB_INTERRUPT_NUM	1
int cischeat_sub_interrupt(void)
{
	return 4;
}



/* CPU # 4 */
#define CISCHEAT_SOUND_INTERRUPT_NUM	16
int cischeat_sound_interrupt(void)
{
	return 4;
}

#define STD_FM_CLOCK	3000000
#define STD_OKI_CLOCK	  12000


#define GAME_DRIVER(_shortname_, \
					_cpu_1_clock_,_cpu_2_clock_,_cpu_3_clock_,_cpu_4_clock_, \
					_fm_clock_,_oki1_clock_,_oki2_clock_, \
					_visible_area_, _colors_num_ ) \
\
static struct YM2151interface _shortname_##_ym2151_interface = \
{ \
	1, \
	_fm_clock_, \
	{ YM3012_VOL(33,MIXER_PAN_LEFT,33,MIXER_PAN_RIGHT) }, \
	{ 0 } \
}; \
\
static struct OKIM6295interface _shortname_##_okim6295_interface = \
{ \
	2, \
	{_oki1_clock_, _oki2_clock_},\
	{REGION_SOUND1,REGION_SOUND2}, \
	{ 33, 33 } \
}; \
\
static struct MachineDriver machine_driver_##_shortname_ = \
{ \
	{ \
		{ \
			CPU_M68000, \
			_cpu_1_clock_, \
			_shortname_##_readmem,_shortname_##_writemem,0,0, \
			cischeat_interrupt, CISCHEAT_INTERRUPT_NUM \
		}, \
		{ \
			CPU_M68000, \
			_cpu_2_clock_, \
			_shortname_##_readmem2,_shortname_##_writemem2,0,0, \
			cischeat_sub_interrupt, CISCHEAT_SUB_INTERRUPT_NUM \
		}, \
		{ \
			CPU_M68000, \
			_cpu_3_clock_, \
			_shortname_##_readmem3,_shortname_##_writemem3,0,0, \
			cischeat_sub_interrupt, CISCHEAT_SUB_INTERRUPT_NUM \
		}, \
		{ \
			CPU_M68000 | CPU_AUDIO_CPU, \
			_cpu_4_clock_, \
			_shortname_##_sound_readmem,_shortname_##_sound_writemem,0,0, \
			cischeat_sound_interrupt, CISCHEAT_SOUND_INTERRUPT_NUM \
		}, \
	}, \
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION, \
	100, \
	0, /* Init Machine */ \
\
	/* video hardware */ \
	256, 256,_visible_area_, \
\
	_shortname_##_gfxdecodeinfo, \
	_colors_num_,_colors_num_, \
	0, \
\
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK, \
	0, \
	_shortname_##_vh_start, \
	0, \
	_shortname_##_vh_screenrefresh, \
\
	/* sound hardware */ \
	0,0,0,0, \
	{ \
		{ \
			SOUND_YM2151, \
			&_shortname_##_ym2151_interface \
		},\
		{\
			SOUND_OKIM6295, \
			&_shortname_##_okim6295_interface \
		} \
	} \
};


/***************************************************************************

  Game driver(s)

**************************************************************************/

/*
	Sprite data is stored like this:

	Sprite 0
		Line 0-15 (left half)
		Line 0-15 (right half)
	Sprite 1
	..

	We need to untangle it
*/
void cischeat_untangle_sprites(int region)
{
	unsigned char		*src = memory_region(region);
	const unsigned char	*end = memory_region(region) + memory_region_length(region);

	while (src < end)
	{
		unsigned char sprite[16*8];
		int i;

		for (i = 0; i < 16 ; i++)
		{
			memcpy(&sprite[i*8+0], &src[i*4+0],    4);
			memcpy(&sprite[i*8+4], &src[i*4+16*4], 4);
		}
		memcpy(src, sprite, 16*8);
		src += 16*8;
	}
}



/***************************************************************************

								[ Cisco Heat ]

From "ARCADE ROMS FROM JAPAN (ARFJ)"'s readme:

 -BOARD #1 CH-9072 EB90001-20024-
|                [9]r15 [10]r16  |	EP:	[1]ch9072.01	[2]ch9072.02
|               [11]r17 [12]r18  |		[3]ch9072.03
|               [13]r25 [14]r26  |MASK:	[9]ch9072.r15	[10]ch9072.r16
|               [15]r19 [16]r20  |		[11]ch9072.r17	[12]ch9072.r18
|     [1]01                      |		[13]ch9072.r25	[14]ch9072.r26
|[2]02[3]03                      |		[15]ch9072.r19	[16]ch9072.r20
|[4] [5] [6]                     |
|[7]                             |	([4][5][6][8]:27cx322  [7]:82S135)
|[8]                             |
 --------------------------------

Video                        Sound
 -BOARD #2 CH-9071 EB90001-20023- 	X1:12Mhz  X2:4Mhz  X3:20Mhz  X4:7Mhz
|68000             [9]      68000|	YM2151x1 OKI M6295 x2 ([8]82S147 [9]:82S185)
|[1]01 X3               X4 [11]11|	EP:	[1]ch9071v2.01 "CH-9071 Ver.2  1"
|[2]02                     [10]10|		[2]ch9071.02
|[3]03                           |		[3]ch9071v2.03 "CH-9071 Ver.2  3"
|[4]04            X1 X2   [12]r23|		[4]ch9071.04
|           [8]     YM2151[13]r24|		[7]ch9071.07
|[7]07[5]a14[6]t74               |		[10]ch9071.10 	[11]ch9071.11
|                                |MASK:	[5]ch9071.a14	[6]ch9071.t74
 --------------------------------		[12]ch9071.r23	[13]ch9071.r24

 -BOARD #3 CH-9073 EB90001-20025-
|           [5]r21 [6]r22   68000|
|    [9]    [1]01  [2]02         | EP:	[1]ch9073.01	[2]ch9073.02
|                                |		[3]ch9073v1.03 "CH-9073 Ver.1  3"
|           [7]r21 [8]r22   68000|		[4]ch9073v1.04 "CH-9073 Ver.1  4"
|           [3]03  [4]04         |MASK:	[5][7]ch9073.r21
|            [10]    [11]        |		[6][8]ch9073.r22
|                                |		([9][10][11]:82S129)
 --------------------------------

DIP SW:8BITx2 , 4BITx1

According to KLOV:

Controls:	Steering: Wheel - A 'judder' motor is attached to the wheel.
			Pedals: 2 - Both foot controls are simple switches.
Sound:		Amplified Stereo (two channel)


***************************************************************************/

ROM_START( cischeat )
	ROM_REGION( 0x080000, REGION_CPU1 )
	ROM_LOAD_EVEN( "ch9071v2.03", 0x000000, 0x040000, 0xdd1bb26f )
	ROM_LOAD_ODD(  "ch9071v2.01", 0x000000, 0x040000, 0x7b65276a )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD_EVEN( "ch9073.01",  0x000000, 0x040000, 0xba331526 )
	ROM_LOAD_ODD(  "ch9073.02",  0x000000, 0x040000, 0xb45ff10f )

	ROM_REGION( 0x80000, REGION_CPU3 )
	ROM_LOAD_EVEN( "ch9073v1.03", 0x000000, 0x040000, 0xbf1d1cbf )
	ROM_LOAD_ODD(  "ch9073v1.04", 0x000000, 0x040000, 0x1ec8a597 )

	ROM_REGION( 0x40000, REGION_CPU4 )
	ROM_LOAD_EVEN( "ch9071.11", 0x000000, 0x020000, 0xbc137bea )
	ROM_LOAD_ODD(  "ch9071.10", 0x000000, 0x020000, 0xbf7b634d )

	ROM_REGION( 0x100000, REGION_USER1 )	/* second halves of program ROMs */
	ROM_LOAD_EVEN( "ch9071.04",   0x000000, 0x040000, 0x7fb48cbc )	// cpu #1
	ROM_LOAD_ODD(  "ch9071.02",   0x000000, 0x040000, 0xa5d0f4dc )
	// cpu #2 (0x40000 bytes will be copied here)
	// cpu #3 (0x40000 bytes will be copied here)

	ROM_REGION( 0x040000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ch9071.a14",  0x000000, 0x040000, 0x7a6d147f ) // scroll 0

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ch9071.t74",  0x000000, 0x040000, 0x735a2e25 ) // scroll 1

	ROM_REGION( 0x010000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ch9071.07",   0x000000, 0x010000, 0x3724ccc3 ) // scroll 2

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ch9073.r21",  0x000000, 0x080000, 0x2943d2f6 ) // Road
	ROM_LOAD( "ch9073.r22",  0x080000, 0x080000, 0x2dd44f85 )

	ROM_REGION( 0x400000, REGION_GFX5 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD_GFX_EVEN( "ch9072.r15",  0x000000, 0x080000, 0x38af4aea )
	ROM_LOAD_GFX_ODD(  "ch9072.r16",  0x000000, 0x080000, 0x71388dad )
	ROM_LOAD_GFX_EVEN( "ch9072.r17",  0x100000, 0x080000, 0x9d052cf3 )
	ROM_LOAD_GFX_ODD(  "ch9072.r18",  0x100000, 0x080000, 0xfe402a56 )
	ROM_LOAD_GFX_EVEN( "ch9072.r25",  0x200000, 0x080000, 0xbe8cca47 )
	ROM_LOAD_GFX_ODD(  "ch9072.r26",  0x200000, 0x080000, 0x2f96f47b )
	ROM_LOAD_GFX_EVEN( "ch9072.r19",  0x300000, 0x080000, 0x4e996fa8 )
	ROM_LOAD_GFX_ODD(  "ch9072.r20",  0x300000, 0x080000, 0xfa70b92d )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "ch9071.r23", 0x000000, 0x080000, 0xc7dbb992 ) // 2 x 0x40000

	ROM_REGION( 0x80000, REGION_SOUND2 )	/* samples */
	ROM_LOAD( "ch9071.r24", 0x000000, 0x080000, BADCRC(0xe87ca4d7) ) // 2 x 0x40000 (FIRST AND SECOND HALF IDENTICAL)

	/* Unused ROMs */
//	ROM_LOAD( "ch9072.01",  0x000000, 0x080000, 0xb2efed33 ) // FIXED BITS (xxxxxxxx0xxxxxxx)
//	ROM_LOAD( "ch9072.02",  0x000000, 0x080000, 0x536edde4 )
//	ROM_LOAD( "ch9072.03",  0x000000, 0x080000, 0x7e79151a )
ROM_END



void init_cischeat(void)
{
/* Split ROMs */
	rom_1 = memory_region(REGION_USER1) + 0x00000;

	rom_2 = memory_region(REGION_CPU2) + 0x40000;
	memcpy(memory_region(REGION_USER1) + 0x80000, rom_2, 0x40000);
	memset(rom_2, 0, 0x40000);
	rom_2 = memory_region(REGION_USER1) + 0x80000;

	rom_3 = memory_region(REGION_CPU3) + 0x40000;
	memcpy(memory_region(REGION_USER1) + 0xc0000, rom_3, 0x40000);
	memset(rom_3, 0, 0x40000);
	rom_3 = memory_region(REGION_USER1) + 0xc0000;

	cischeat_untangle_sprites(REGION_GFX5);	// Untangle sprites
	astyanax_rom_decode(3);					// Decrypt sound cpu code
}


#define CISCHEAT_VISIBLE_AREA	{0,255,16,231}

GAME_DRIVER(	cischeat,
				10000000,10000000,10000000,7000000,
				STD_FM_CLOCK,STD_OKI_CLOCK,STD_OKI_CLOCK,
				CISCHEAT_VISIBLE_AREA,
				32*16 * 3 + 64*16 * 2 + 128*16)	/* scroll 0,1,2; road 0,1; sprites */



/***************************************************************************

							[ F1 GrandPrix Star ]

From malcor's readme:

Location     Device      File ID     Checksum      PROM Label
-----------------------------------------------------------------
LB IC2       27C010    9188A-1.V10     46C6     GP-9188A 1  Ver1.0
LB IC27      27C010    9188A-6.V10     DB84     GP-9188A 6  Ver1.0
LB IC46      27C010    9188A-11.V10    DFEA     GP-9188A 11 Ver1.0
LB IC70      27C010    9188A-16.V10    1034     GP-9188A 16 Ver1.0
LB IC91      27C020    9188A-21.V10    B510     GP-9188A 21 Ver1.0
LB IC92      27C020    9188A-22.V20    A9BA     GP-9188A 22 Ver2.0
LB IC124     27C020    9188A-26.V10    AA81     GP-9188A 26 Ver1.0
LB IC125     27C020    9188A-27.V20    F34D     GP-9188A 27 Ver2.0
LB IC174    23C1000    9188A-30.V10    0AA5     GP-9188A 30 Ver1.0
TB IC2       27C010    9190A-1.V11     B05A     GP-9190A 1  Ver1.1
TB IC4       27C010    9190A-2.V11     ED7A     GP-9190A 2  Ver1.1
LB IC37     23C4001    90015-01.W06    F10F     MR90015-01-W06      *
LB IC79     23C4001    90015-01.W06    F10F     MR90015-01-W06      *
LB IC38     23C4001    90015-02.W07    F901     MR90015-02-W07      *
LB IC80     23C4001    90015-02.W07    F901     MR90015-02-W07      *
LB IC39     23C4001    90015-03.W08    F100     MR90015-03-W08      *
LB IC81     23C4001    90015-03.W08    F100     MR90015-03-W08      *
LB IC40     23C4001    90015-04.W09    FA00     MR90015-04-W09      *
LB IC82     23C4001    90015-04.W09    FA00     MR90015-04-W09      *
LB IC64     23C4001    90015-05.W10    5FF9     MR90015-05-W10
LB IC63     23C4001    90015-06.W11    6EDA     MR90015-06-W11
LB IC62     23C4001    90015-07.W12    E9B4     MR90015-07-W12
LB IC61     23C4001    90015-08.W14    5107     MR90015-08-W14
LB IC17     23C4001    90015-09.W13    71EE     MR90015-09-W13
LB IC16     23C4001    90015-10.W15    EFEF     MR90015-10-W15
MB IC54     23C4001    90015-20.R45    7890     MR90015-20-R45      *
MB IC67     23C4001    90015-20.R45    7890     MR90015-20-R45      *
MB IC1      23C4001    90015-21.R46    C73C     MR90015-21-R46
MB IC2      23C4001    90015-22.R47    5D58     MR90015-22-R47
MB IC5      23C4001    90015-23.R48    4E7B     MR90015-23-R48
MB IC6      23C4001    90015-24.R49    F6A0     MR90015-24-R49
MB IC11     23C4001    90015-25.R50    9FC0     MR90015-25-R50
MB IC12     23C4001    90015-26.R51    13E4     MR90015-26-R51
MB IC15     23C4001    90015-27.R52    8D5D     MR90015-27-R52
MB IC16     23C4001    90015-28.R53    E0B8     MR90015-28-R53
MB IC21     23C4001    90015-29.R54    DF33     MR90015-29-R54
MB IC22     23C4001    90015-30.R55    DA2D     MR90015-30-R55
LB IC123    23C4001    90015-31.R56    BE57     MR90015-31-R56
LB IC152    23C4001    90015-32.R57    8B57     MR90015-32-R57
TB IC12     23C4001    90015-33.W31    7C0E     MR90015-33-W31
TB IC11     23C4001    90015-34.W32    B203     MR90015-34-W32
MB IC39     27CX322    CH9072-4        085F     CH9072 4
MB IC33     27CX322    CH9072-5        641D     CH9072 5
MB IC35     27CX322    CH9072-6        EAE1     CH9072 6
MB IC59     27CX322    CH9072-8        AB60     CH9072 8
LB IC105    N82S147    PR88004Q        FCFC     PR88004Q
LB IC66     N82S135    PR88004W        20C8     PR88004W
LB IC117    N82S185    PR90015A        3326     PR90015A
LB IC153    N82S135    PR90015B        1E52     PR90015B

Notes:  TB - Top board     (audio & I/O)       GP-9190A EB90015-20039-1
        MB - Middle board  (GFX)               GP-9189  EB90015-20038
        LB - Lower board   (CPU/GFX)           GP-9188A EB90015-20037-1

         * - These ROMs are found twice on the PCB
           - There are two linked boards per cabinet (two player cabinet)
             (attract mode displays across both monitors)

Brief hardware overview:
------------------------

Main processor   - 68000
                 - program ROMs: 9188A-22.V20 (odd), 9188A-27.V20 (even) bank 1
                                 9188A-21.V10 (odd), 9188A-26.V10 (even) bank 2
                 - Processor RAM 2x LH52250AD-90L (32kx8 SRAM)

Slave processor1 - 68000
                 - program ROMs: 9188A-11.V10 (odd), 9188A-16.V10 (even)
                 - Processor RAM 2x LH5168D-10L (8kx8 SRAM)
                 - CS90015-04 (64 pin PQFP)  + 2x MCM2018AN45 (2kx8 SRAM)
                 - GS90015-05 (100 pin PQFP) + 2x MCM2018AN45 (2kx8 SRAM)
                 - uses ROMs: 90015-08.W14, 90015-07.W12, 90015-06.W11
                              90015-05.W10, 90015-01.W06, 90015-02.W07
                              90015-03.W08, 90015-04.W09

Slave processor2 - 68000
                 - Program ROMs: 9188A-1.V10 (odd), 9188A-6.V10 (even)
                 - Processor RAM 2x LH5168D-10L (8kx8 SRAM)
                 - CS90015-04 (64 pin PQFP)  + 2x MCM2018AN45 (2kx8 SRAM)
                 - GS90015-05 (100 pin PQFP) + 2x MCM2018AN45 (2kx8 SRAM)
                 - uses ROMs: 90015-01.W06, 90015-02.W07, 90015-03.W08
                              90015-10.W15, 90015-09.W13, 90015-04.W09

Sound processor  - 68000
                 - Program ROMs: 9190A-1.V11 (odd), 9190A-2.V11 (even)
                 - Processor RAM 2x LH52250AD-90L (32kx8 SRAM)
                 - M6295,  uses ROM  90015-34.W32
                 - M6295,  uses ROM  90015-33.W31
                 - YM2151

GFX & Misc       - GS90015-02 (100 pin PQFP),  uses ROM 90015-31-R56
                   GS-9000406 (80 pin PQFP)  + 2x LH5168D-10L (8kx8 SRAM)

                 - GS90015-02 (100 pin PQFP),  uses ROM 90015-32-R57
                   GS-9000406 (80 pin PQFP)  + 2x LH5168D-10L (8kx8 SRAM)

                 - GS90015-02 (100 pin PQFP),  uses ROM 9188A-30-V10
                   GS-9000406 (80 pin PQFP)  + 2x LH5168D-10L (8kx8 SRAM)

                 - GS900151   (44 pin PQFP) (too small for the full part No.)
             3x  - GS90015-03 (80 pin PQFP)  + 2x LH52258D-45 (32kx8 SRAM)
             2x  - GS90015-06 (100 pin PQFP) + 2x LH52250AD-90L (32kx8 SRAM)
                 - GS90015-07 (64 pin PQFP)
                 - GS90015-08 (64 pin PQFP)
                 - GS90015-09 (64 pin PQFP)  + 2x MCM2018AN45 (2kx8 SRAM)
                 - GS90015-10 (64 pin PQFP)
                 - GS90015-12 (80 pin PQFP)  + 2x MCM2018AN45 (2kx8 SRAM)
                 - GS90015-11 (100 pin PQFP)
                   uses ROMs 90015-30-R55, 90015-25-R50, 90015-24-R49
                             90015-29-R54, 90015-23-R48, 90015-22-R47
                             90015-28-R53, 90015-21-R46, 90015-27-R52
                             90015-26-R51


***************************************************************************/

ROM_START( f1gpstar )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_EVEN( "9188a-27.v20", 0x000000, 0x040000, 0x0a9d3896 )
	ROM_LOAD_ODD(  "9188a-22.v20", 0x000000, 0x040000, 0xde15c9ca )

	ROM_REGION( 0x80000, REGION_CPU2 )
	/* Should Use ROMs:	90015-01.W06, 90015-02.W07, 90015-03.W08, 90015-04.W09 */
	ROM_LOAD_EVEN( "9188a-16.v10",  0x000000, 0x020000, 0xef0f7ca9 )
	ROM_LOAD_ODD(  "9188a-11.v10",  0x000000, 0x020000, 0xde292ea3 )

	ROM_REGION( 0x80000, REGION_CPU3 )
	/* Should Use ROMs:	90015-01.W06, 90015-02.W07, 90015-03.W08, 90015-04.W09 */
	ROM_LOAD_EVEN( "9188a-6.v10",  0x000000, 0x020000, 0x18ba0340 )
	ROM_LOAD_ODD(  "9188a-1.v10",  0x000000, 0x020000, 0x109d2913 )

	ROM_REGION( 0x40000, REGION_CPU4 )
	ROM_LOAD_EVEN( "9190a-2.v11", 0x000000, 0x020000, 0xacb2fd80 )
	ROM_LOAD_ODD(  "9190a-1.v11", 0x000000, 0x020000, 0x7cccadaf )

	ROM_REGION( 0x80000, REGION_USER1 )	/* second halves of program ROMs */
	ROM_LOAD_EVEN( "9188a-26.v10", 0x000000, 0x040000, 0x0b76673f )	// cpu #1
	ROM_LOAD_ODD(  "9188a-21.v10", 0x000000, 0x040000, 0x3e098d77 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "90015-31.r56",  0x000000, 0x080000, 0x0c8f0e2b ) // scroll 0

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "90015-32.r57",  0x000000, 0x080000, 0x9c921cfb ) // scroll 1

	ROM_REGION( 0x020000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9188a-30.v10",  0x000000, 0x020000, 0x0ef1fbf1 ) // scroll 2

	ROM_REGION( 0x200000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "90015-05.w10",  0x000000, 0x080000, 0x8eb48a23 ) // Road 0
	ROM_LOAD( "90015-06.w11",  0x080000, 0x080000, 0x32063a68 )
	ROM_LOAD( "90015-07.w12",  0x100000, 0x080000, 0x0d0d54f3 )
	ROM_LOAD( "90015-08.w14",  0x180000, 0x080000, 0xf48a42c5 )

	ROM_REGION( 0x100000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "90015-09.w13",  0x000000, 0x080000, 0x55f49315 ) // Road 1
	ROM_LOAD( "90015-10.w15",  0x080000, 0x080000, 0x678be0cb )

	ROM_REGION( 0x500000, REGION_GFX6 | REGIONFLAG_DISPOSE )	/* sprites */
	ROM_LOAD_GFX_EVEN( "90015-21.r46",  0x000000, 0x080000, 0x6f30211f )
	ROM_LOAD_GFX_ODD(  "90015-22.r47",  0x000000, 0x080000, 0x05a9a5da )
	ROM_LOAD_GFX_EVEN( "90015-23.r48",  0x100000, 0x080000, 0x58e9c6d2 )
	ROM_LOAD_GFX_ODD(  "90015-24.r49",  0x100000, 0x080000, 0xabd6c91d )
	ROM_LOAD_GFX_EVEN( "90015-25.r50",  0x200000, 0x080000, 0x7ded911f )
	ROM_LOAD_GFX_ODD(  "90015-26.r51",  0x200000, 0x080000, 0x18a6c663 )
	ROM_LOAD_GFX_EVEN( "90015-27.r52",  0x300000, 0x080000, 0x7378c82f )
	ROM_LOAD_GFX_ODD(  "90015-28.r53",  0x300000, 0x080000, 0x9944dacd )
	ROM_LOAD_GFX_EVEN( "90015-29.r54",  0x400000, 0x080000, 0x2cdec370 )
	ROM_LOAD_GFX_ODD(  "90015-30.r55",  0x400000, 0x080000, 0x47e37604 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "90015-34.w32", 0x000000, 0x080000, 0x2ca9b062 ) // 2 x 0x40000

	ROM_REGION( 0x80000, REGION_SOUND2 )	/* samples */
	ROM_LOAD( "90015-33.w31", 0x000000, 0x080000, 0x6121d247 ) // 2 x 0x40000

	/* Unused ROMs */

// "I know that one of the ROM images in the archive looks bad (90015-04.W09)
//  however, it is good as far as I can tell. There were two of those ROMs
// (soldered) onto the board and I checked them both against each other. "

//	ROM_LOAD( "90015-04.w09",  0x000000, 0x080000, 0x5b324c81 )	// x 2 xxxxxxxxx0xxxxxxxxx = 0x00
//	ROM_LOAD( "90015-03.w08",  0x000000, 0x080000, 0xccf5b158 )	// x 2 FIXED BITS (000x000x)
//	ROM_LOAD( "90015-02.w07",  0x000000, 0x080000, 0xfcbecc9b )	// x 2
//	ROM_LOAD( "90015-01.w06",  0x000000, 0x080000, 0xce4bfe6e )	// x 2 FIXED BITS (000x000x)

//	ROM_LOAD( "90015-20.r44",  0x000000, 0x080000, 0x9d428fb7 ) // x 2

//	ROM_LOAD( "ch9072-4",  0x000000, 0x001000, 0x5bc23535 )	// FIXED BITS (0000000x)
//	ROM_LOAD( "ch9072-5",  0x000000, 0x001000, 0x0efac5b4 )	// FIXED BITS (xxxx0xxx)
//	ROM_LOAD( "ch9072-6",  0x000000, 0x001000, 0x76ff63c5 )
//	ROM_LOAD( "ch9072-8",  0x000000, 0x001000, 0xca04bace )	// FIXED BITS (0xxx0xxx)

//	ROM_LOAD( "pr88004q",  0x000000, 0x000200, 0x9327dc37 )	// FIXED BITS (1xxxxxxx1111x1xx)
//	ROM_LOAD( "pr88004w",  0x000000, 0x000100, 0x3d648467 )	// FIXED BITS (00xxxxxx)

//	ROM_LOAD( "pr90015a",  0x000000, 0x000800, 0x777583db )	// FIXED BITS (00000xxx0000xxxx)
//	ROM_LOAD( "pr90015b",  0x000000, 0x000100, 0xbe240dac )	// FIXED BITS (000xxxxx000xxxx1)
ROM_END



void init_f1gpstar(void)
{
/* Split ROMs */
	rom_1 = memory_region(REGION_USER1) + 0x00000;

	cischeat_untangle_sprites(REGION_GFX6);
}



#define F1GPSTAR_VISIBLE_AREA	{0,255,16,239}

// The date is 1992 whenever the country (DSW) isn't set to Japan
GAME_DRIVER(	f1gpstar,
				12000000,12000000,12000000,7000000,
				STD_FM_CLOCK,STD_OKI_CLOCK,STD_OKI_CLOCK,
				F1GPSTAR_VISIBLE_AREA,
				16*16 * 3 + 64*16 * 2 + 128*16)	/* scroll 0,1,2; road 0,1; sprites */



GAME( 1990, cischeat, 0, cischeat, cischeat, cischeat, ROT0_16BIT, "Jaleco", "Cisco Heat" )
GAME( 1991, f1gpstar, 0, f1gpstar, f1gpstar, f1gpstar, ROT0_16BIT, "Jaleco", "F1 Grand Prix Star" )
