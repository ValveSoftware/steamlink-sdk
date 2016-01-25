#include "../vidhrdw/seta.cpp"
#include "../sndhrdw/seta.cpp"

/***************************************************************************

								-= Seta Games =-

					driver by	Luca Elia (eliavit@unina.it)


CPU:	68000 + 65C02 [Optional]
Sound:	X1-010  [Custom 16 Bit PCM]
Other:  NEC D4701 [?]

---------------------------------------------------------------------------
Game							Year	Licensed To 		Issues / Notes
---------------------------------------------------------------------------
Thundercade / Twin Formation	1987	Taito
Twin Eagle (Japan)				1988	Taito				Some Wrong Tiles
Caliber 50						1988	Taito / RomStar
DownTown						1989	Taito / RomStar
Dragon Unit (Japan) 			1989	Athena / Taito / RomStar
U.S. Classic					1989	Taito / RomStar		Wrong Colors
Arbalester						1989	Taito / RomStar
Meta Fox						1989	Taito / RomStar
Blandia             (1)			1992	Allumer
Zing Zing Zip					1992	Allumer + Tecmo
Mobile Suit Gundam				1993	Banpresto			Not Working
War Of Aero						1993	Yang Cheng
---------------------------------------------------------------------------
(1) Prototype?


To do: better sound, nvram.


***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"

/* Variables and functions only used here */

static unsigned char *sharedram;


/* Variables that vidhrdw has access to */

int blandia_samples_bank;


/* Variables and functions defined in vidhrdw */

extern unsigned char *seta_vram_0, *seta_vram_1, *seta_vctrl_0;
extern unsigned char *seta_vram_2, *seta_vram_3, *seta_vctrl_2;
extern unsigned char *seta_vregs;

extern int seta_tiles_offset;

WRITE_HANDLER( seta_vram_0_w );
WRITE_HANDLER( seta_vram_1_w );
WRITE_HANDLER( seta_vram_2_w );
WRITE_HANDLER( seta_vram_3_w );
WRITE_HANDLER( seta_vregs_w );

void blandia_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void zingzip_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void usclssic_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

int  seta_vh_start_1_layer(void);
int  seta_vh_start_1_layer_offset_0x02(void);
int  seta_vh_start_2_layers(void);

void seta_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void seta_vh_screenrefresh_no_layers(struct osd_bitmap *bitmap,int full_refresh);



/* Variables and functions defined in sndhrdw */

extern unsigned char *seta_sound_ram;

READ_HANDLER ( seta_sound_r );
READ_HANDLER ( seta_sound_word_r );
WRITE_HANDLER( seta_sound_w );
WRITE_HANDLER( seta_sound_word_w );
int  seta_sh_start_4KHz(const struct MachineSound *msound);
int  seta_sh_start_6KHz(const struct MachineSound *msound);
int  seta_sh_start_8KHz(const struct MachineSound *msound);


static struct CustomSound_interface seta_4KHz_interface =
{
	seta_sh_start_4KHz,
	0,
	0,
};
static struct CustomSound_interface seta_6KHz_interface =
{
	seta_sh_start_6KHz,
	0,
	0,
};
static struct CustomSound_interface seta_8KHz_interface =
{
	seta_sh_start_8KHz,
	0,
	0,
};



/***************************************************************************


								Main CPU

(for debugging it is useful to be able to peek at some memory regions that
 the game writes to but never reads from. I marked this regions with an empty
 comment to distinguish them, since there's always the possibility that some
 games actually read from this kind of regions, expecting some hardware
 register's value there, instead of the data they wrote)

***************************************************************************/


/*

 Shared RAM:

 The 65c02 sees a linear array of bytes that is mapped, for the 68000,
 to a linear array of words whose low order bytes hold the data

*/

static READ_HANDLER( sharedram_68000_r )
{
	return sharedram[offset/2];
}

static WRITE_HANDLER( sharedram_68000_w )
{
	sharedram[offset/2] = data & 0xff;
}


static WRITE_HANDLER( sub_ctrl_w )
{
	static int old_data = 0;

	switch(offset)
	{
		case 0:	// bit 0: reset sub cpu?
			if ( !(old_data&1) && (data&1) ) cpu_set_reset_line(1,PULSE_LINE);
			old_data = data;
			break;

		case 2:	// ?
			break;

		case 4:	// not sure
			soundlatch_w(0,data&0xff);
			break;

		case 6:	// not sure
			soundlatch2_w(0,data&0xff);
			break;
	}

}

static READ_HANDLER( seta_dsw_r )
{
	int dsw = readinputport(3);
	if (offset == 0)	return (dsw >> 8) & 0xff;
	else				return (dsw >> 0) & 0xff;
}


/***************************************************************************
								Caliber 50
***************************************************************************/

READ_HANDLER ( calibr50_ip_r )
{
	int dir1 = readinputport(4) & 0xfff;	// analog port
	int dir2 = readinputport(5) & 0xfff;	// analog port

	switch (offset)
	{
		case 0x00:	return readinputport(0);	// p1
		case 0x02:	return readinputport(1);	// p2
		case 0x08:	return readinputport(2);	// Coins
		case 0x10:	return (dir1&0xff);			// lower 8 bits of p1 rotation
		case 0x12:	return (dir1>>8);			// upper 4 bits of p1 rotation
		case 0x14:	return (dir2&0xff);			// lower 8 bits of p2 rotation
		case 0x16:	return (dir2>>8);			// upper 4 bits of p2 rotation
		case 0x18:	return 0xffff;				// ? (value's read but not used)
		default:
			//logerror("PC %06X - Read input %02X !\n", cpu_get_pc(), offset);
			return 0;
	}
}

WRITE_HANDLER( calibr50_soundlatch_w )
{
	soundlatch_w(0,data);
	cpu_set_nmi_line(1,PULSE_LINE);
	cpu_spinuntil_time(TIME_IN_USEC(50));	// Allow the sound cpu to acknowledge
}

static struct MemoryReadAddress calibr50_readmem[] =
{
	{ 0x000000, 0x09ffff, MRA_ROM					},	// ROM
	{ 0xff0000, 0xffffff, MRA_BANK1					},	// RAM
	{ 0x100000, 0x100007, MRA_NOP					},	// ? (same as a00010-a00017?)
	{ 0x200000, 0x200fff, MRA_BANK2					},	// NVRAM
	{ 0x300000, 0x300001, MRA_NOP					},	// ? (value's read but not used)
	{ 0x400000, 0x400001, watchdog_reset_r			},	// Watchdog
	{ 0x600000, 0x600003, seta_dsw_r				},	// DSW
	{ 0x700000, 0x7003ff, MRA_BANK3					},	// Palette
/**/{ 0x800000, 0x800005, MRA_BANK4					},	// VRAM Ctrl
	{ 0x900000, 0x901fff, MRA_BANK5					},	// VRAM
	{ 0x902000, 0x903fff, MRA_BANK6					},	// VRAM
	{ 0x904000, 0x904fff, MRA_BANK7					},	//
	{ 0xa00000, 0xa00019, calibr50_ip_r				},	// Input Ports
/**/{ 0xd00000, 0xd00607, MRA_BANK8					},	// Sprites Y
	{ 0xe00000, 0xe03fff, MRA_BANK9					},	// Sprites Code + X + Attr
	{ 0xb00000, 0xb00001, soundlatch2_r				},	// From Sub CPU
/**/{ 0xc00000, 0xc00001, MRA_BANK10				},	// ? $4000
	{ -1 }
};

static struct MemoryWriteAddress calibr50_writemem[] =
{
	{ 0x000000, 0x09ffff, MWA_ROM								},	// ROM
	{ 0xff0000, 0xffffff, MWA_BANK1								},	// RAM
	{ 0x200000, 0x200fff, MWA_BANK2								},	// NVRAM
	{ 0x300000, 0x300001, MWA_NOP								},	// ? (random value)
	{ 0x500000, 0x500001, MWA_NOP								},	// ?
	{ 0x700000, 0x7003ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram	},	// Palette
	{ 0x800000, 0x800005, MWA_BANK4, &seta_vctrl_0				},	// VRAM Ctrl
	{ 0x900000, 0x901fff, seta_vram_0_w, &seta_vram_0			},	// VRAM
	{ 0x902000, 0x903fff, seta_vram_1_w, &seta_vram_1			},	// VRAM
	{ 0x904000, 0x904fff, MWA_BANK7								},	//
	{ 0xd00000, 0xd00607, MWA_BANK8, &spriteram					},	// Sprites Y
	{ 0xe00000, 0xe03fff, MWA_BANK9, &spriteram_2				},	// Sprites Code + X + Attr
	{ 0xb00000, 0xb00001, calibr50_soundlatch_w					},	// To Sub CPU
	{ 0xc00000, 0xc00001, MWA_BANK10							},	// ? $4000
	{ -1 }
};


/***************************************************************************
				DownTown, Meta Fox, Twin Eagle, Arbalester
			(with slight variations, and protections hooked in)
***************************************************************************/

static struct MemoryReadAddress downtown_readmem[] =
{
	{ 0x000000, 0x09ffff, MRA_ROM					},	// ROM
	{ 0xf00000, 0xffffff, MRA_BANK1					},	// RAM
	{ 0x100000, 0x103fff, seta_sound_word_r			},	// Sound
	{ 0x600000, 0x600003, seta_dsw_r				},	// DSW
	{ 0x700000, 0x7003ff, MRA_BANK3					},	// Palette
/**/{ 0x800000, 0x800005, MRA_BANK4					},	// VRAM Ctrl
	{ 0x900000, 0x901fff, MRA_BANK5					},	// VRAM
	{ 0x902000, 0x903fff, MRA_BANK6					},	// VRAM
	{ 0xb00000, 0xb00fff, sharedram_68000_r			},	// Shared RAM
/**/{ 0xc00000, 0xc00001, MRA_BANK7					},	// ? $4000
/**/{ 0xd00000, 0xd00607, MRA_BANK8					},	// Sprites Y
	{ 0xe00000, 0xe03fff, MRA_BANK9					},	// Sprites Code + X + Attr
	{ -1 }
};

static struct MemoryWriteAddress downtown_writemem[] =
{
	{ 0x000000, 0x09ffff, MWA_ROM								},	// ROM
	{ 0xf00000, 0xffffff, MWA_BANK1								},	// RAM
	{ 0x100000, 0x103fff, seta_sound_word_w, &seta_sound_ram	},	// Sound
	{ 0x500000, 0x500001, MWA_NOP								},	// ?
	{ 0x700000, 0x7003ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram	},	// Palette
	{ 0x800000, 0x800005, MWA_BANK4, &seta_vctrl_0				},	// VRAM Ctrl
	{ 0x900000, 0x901fff, seta_vram_0_w, &seta_vram_0			},	// VRAM
	{ 0x902000, 0x903fff, seta_vram_1_w, &seta_vram_1			},	// VRAM
	{ 0xa00000, 0xa00007, sub_ctrl_w							},	// Sub CPU Control?
	{ 0xb00000, 0xb00fff, sharedram_68000_w						},	// Shared RAM
	{ 0xc00000, 0xc00001, MWA_BANK7								},	// ? $4000
	{ 0xd00000, 0xd00607, MWA_BANK8, &spriteram					},	// Sprites Y
	{ 0xe00000, 0xe03fff, MWA_BANK9, &spriteram_2				},	// Sprites Code + X + Attr
	{ -1 }
};




/***************************************************************************
							Mobile Suit Gundam
***************************************************************************/


static struct MemoryReadAddress msgundam_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM				},	// ROM
	{ 0x100000, 0x1fffff, MRA_ROM				},	// ROM
	{ 0x200000, 0x24ffff, MRA_BANK1				},	// RAM
	{ 0x400000, 0x400001, input_port_0_r		},	// P1
	{ 0x400002, 0x400003, input_port_1_r		},	// P2
	{ 0x400004, 0x400005, input_port_2_r		},	// Coins
	{ 0x500000, 0x500005, MRA_BANK4				},	// Coin Lockout + Video Registers
	{ 0x600000, 0x600003, seta_dsw_r			},	// DSW
	{ 0x700400, 0x700fff, MRA_BANK5				},	// Palette
	{ 0x800000, 0x800607, MRA_BANK6				},	// Sprites Y
/**/{ 0x880000, 0x880001, MRA_BANK7				},	// ? 0x4000
	{ 0x900000, 0x903fff, MRA_BANK8				},	// Sprites Code + X + Attr
	{ 0xa00000, 0xa03fff, MRA_BANK9				},	// VRAM 0&1
	{ 0xa80000, 0xa83fff, MRA_BANK10			},	// VRAM 2&3
	{ 0xb00000, 0xb00005, MRA_BANK11			},	// VRAM 0&1 Ctrl
	{ 0xb80000, 0xb80005, MRA_BANK12			},	// VRAM 1&2 Ctrl
	{ 0xc00000, 0xc03fff, seta_sound_word_r		},	// Sound
	{ -1 }
};
static struct MemoryWriteAddress msgundam_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM								},	// ROM
	{ 0x100000, 0x1fffff, MWA_ROM								},	// ROM
	{ 0x200000, 0x24ffff, MWA_BANK1								},	// RAM
	{ 0x400000, 0x400001, MWA_NOP								},	// ? 0
	{ 0x400004, 0x400005, MWA_NOP								},	// ? 0
	{ 0x500000, 0x500005, seta_vregs_w, &seta_vregs				},	// Coin Lockout + Video Registers
	{ 0x700400, 0x700fff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram	},	// Palette
	{ 0x800000, 0x800607, MWA_BANK6 , &spriteram				},	// Sprites Y
	{ 0x880000, 0x880001, MWA_BANK7								},	// ? 0x4000
	{ 0x900000, 0x903fff, MWA_BANK8 , &spriteram_2				},	// Sprites Code + X + Attr
	{ 0xa00000, 0xa01fff, seta_vram_0_w, &seta_vram_0			},	// VRAM 0
	{ 0xa02000, 0xa03fff, seta_vram_1_w, &seta_vram_1			},	// VRAM 1
	{ 0xa80000, 0xa81fff, seta_vram_2_w, &seta_vram_2			},	// VRAM 2
	{ 0xa82000, 0xa83fff, seta_vram_3_w, &seta_vram_3			},	// VRAM 3
	{ 0xb00000, 0xb00005, MWA_BANK11, &seta_vctrl_0				},	// VRAM 0&1 Ctrl
	{ 0xb80000, 0xb80005, MWA_BANK12, &seta_vctrl_2				},	// VRAM 2&3 Ctrl
	{ 0xc00000, 0xc03fff, seta_sound_word_w, &seta_sound_ram	},	// Sound
//	{ 0xd00000, 0xd00007, MWA_NOP								},	// ?
	{ -1 }
};





/***************************************************************************
								Thundercade
***************************************************************************/

/* Mirror ram seems necessary since the e00000-e03fff area is not cleared
   on startup. Level 2 int uses $e0000a as a counter that controls when
   to write a value to the sub cpu, and when to read the result back.
   If the check fails "error x0-006" is displayed. Hence if the counter
   is not cleared at startup the game could check for the result before
   writing to sharedram! */


static unsigned char *mirror_ram;

READ_HANDLER( mirror_ram_r )
{
	return READ_WORD(&mirror_ram[offset]);
}

WRITE_HANDLER( mirror_ram_w )
{
	COMBINE_WORD_MEM(&mirror_ram[offset], data);
}


static struct MemoryReadAddress tndrcade_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM					},	// ROM
	{ 0x380000, 0x3803ff, MRA_BANK1					},	// Palette
/**/{ 0x400000, 0x400001, MRA_BANK2					},	// ? $4000
/**/{ 0x600000, 0x600607, MRA_BANK3					},	// Sprites Y
	{ 0xa00000, 0xa00fff, sharedram_68000_r			},	// Shared RAM
	{ 0xc00000, 0xc03fff, MRA_BANK4					},	// Sprites Code + X + Attr
	{ 0xe00000, 0xe03fff, MRA_BANK5					},	// RAM (Mirrored?)
	{ 0xffc000, 0xffffff, mirror_ram_r				},	// RAM (Mirrored?)
	{ -1 }
};

static struct MemoryWriteAddress tndrcade_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM							},	// ROM
	{ 0x200000, 0x200001, MWA_NOP							},	// ? 0
	{ 0x280000, 0x280001, MWA_NOP							},	// ? 0 / 1 (sub cpu related?)
	{ 0x300000, 0x300001, MWA_NOP							},	// ? 0 / 1
	{ 0x380000, 0x3803ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram	},	// Palette
	{ 0x400000, 0x400001, MWA_BANK2							},	// ? $4000
	{ 0x600000, 0x600607, MWA_BANK3, &spriteram				},	// Sprites Y
	{ 0x800000, 0x800007, sub_ctrl_w						},	// Sub CPU Control?
	{ 0xa00000, 0xa00fff, sharedram_68000_w					},	// Shared RAM
	{ 0xc00000, 0xc03fff, MWA_BANK4, &spriteram_2			},	// Sprites Code + X + Attr
	{ 0xe00000, 0xe03fff, MWA_BANK5, &mirror_ram			},	// RAM (Mirrored?)
	{ 0xffc000, 0xffffff, mirror_ram_w						},	// RAM (Mirrored?)
	{ -1 }
};




/***************************************************************************
								U.S. Classic
***************************************************************************/

READ_HANDLER( usclssic_dsw_r )
{
	switch (offset)
	{
		case 0:	return (readinputport(3) >>  8) & 0xf;
		case 2:	return (readinputport(3) >> 12) & 0xf;
		case 4:	return (readinputport(3) >>  0) & 0xf;
		case 6:	return (readinputport(3) >>  4) & 0xf;
	}
	return 0;
}

READ_HANDLER( usclssic_trackball_x_r )
{
	switch (offset)
	{
		case 0:	return (readinputport(0) >> 0) & 0xff;
		case 2:	return (readinputport(0) >> 8) & 0xff;
	}
	return 0;
}

READ_HANDLER( usclssic_trackball_y_r )
{
	switch (offset)
	{
		case 0:	return (readinputport(1) >> 0) & 0xff;
		case 2:	return (readinputport(1) >> 8) & 0xff;
	}
	return 0;
}


WRITE_HANDLER( usclssic_lockout_w )
{
	static int old_tiles_offset = 0;

	seta_tiles_offset = (data & 0x10) ? 0x4000: 0;
	if (old_tiles_offset != seta_tiles_offset)	tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
	old_tiles_offset = seta_tiles_offset;

	coin_lockout_w(0, ((~data) >> 2) & 1 );
	coin_lockout_w(1, ((~data) >> 3) & 1 );
}


static struct MemoryReadAddress usclssic_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM					},	// ROM
	{ 0xff0000, 0xffffff, MRA_BANK1					},	// RAM
	{ 0x800000, 0x800607, MRA_BANK2					},	// Sprites Y
/**/{ 0x900000, 0x900001, MRA_BANK3					},	// ?
	{ 0xa00000, 0xa00005, MRA_BANK4					},	// VRAM Ctrl
/**/{ 0xb00000, 0xb003ff, MRA_BANK5					},	// Palette
	{ 0xb40000, 0xb40003, usclssic_trackball_x_r	},	// TrackBall X
	{ 0xb40004, 0xb40007, usclssic_trackball_y_r	},	// TrackBall Y + Buttons
	{ 0xb40010, 0xb40011, input_port_2_r			},	// Coins
	{ 0xb40018, 0xb4001f, usclssic_dsw_r			},	// 2 DSWs
	{ 0xb80000, 0xb80001, MRA_NOP					},	// watchdog (value is discarded)?
	{ 0xc00000, 0xc03fff, MRA_BANK6					},	// Sprites Code + X + Attr
	{ 0xd00000, 0xd01fff, MRA_BANK7					},	// VRAM
	{ 0xd02000, 0xd03fff, MRA_BANK8					},	// VRAM
	{ 0xd04000, 0xd04fff, MRA_BANK9					},	//
	{ 0xe00000, 0xe00fff, MRA_BANK10				},	// NVRAM? (odd bytes)
	{ -1 }
};

static struct MemoryWriteAddress usclssic_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM							},	// ROM
	{ 0xff0000, 0xffffff, MWA_BANK1							},	// RAM
	{ 0x800000, 0x800607, MWA_BANK2 , &spriteram			},	// Sprites Y
	{ 0x900000, 0x900001, MWA_BANK3							},	// ? $4000
	{ 0xa00000, 0xa00005, MWA_BANK4, &seta_vctrl_0			},	// VRAM Ctrl
	{ 0xb00000, 0xb003ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram	},	// Palette
	{ 0xb40000, 0xb40001, usclssic_lockout_w				},	// Coin Lockout + Tiles Banking
	{ 0xb40010, 0xb40011, calibr50_soundlatch_w				},	// To Sub CPU
	{ 0xb40018, 0xb40019, watchdog_reset_w					},	// Watchdog
	{ 0xb4000a, 0xb4000b, MWA_NOP							},	// ? (value's not important. In lev2&6)
	{ 0xc00000, 0xc03fff, MWA_BANK6 , &spriteram_2			},	// Sprites Code + X + Attr
	{ 0xd00000, 0xd01fff, seta_vram_0_w, &seta_vram_0		},	// VRAM
	{ 0xd02000, 0xd03fff, seta_vram_1_w, &seta_vram_1		},	// VRAM
	{ 0xd04000, 0xd04fff, MWA_BANK9							},	//
	{ 0xe00000, 0xe00fff, MWA_BANK10						},	// NVRAM? (odd bytes)
	{ -1 }
};



/***************************************************************************
					Blandia / War of Aero / Zing Zing Zip
***************************************************************************/

static struct MemoryReadAddress wrofaero_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM				},	// ROM
	{ 0x100000, 0x1fffff, MRA_ROM				},	// ROM (for blandia)
	{ 0x200000, 0x20ffff, MRA_BANK1				},	// RAM (main ram for zingzip, wrofaero writes to 20f000-20ffff)
	{ 0x300000, 0x30ffff, MRA_BANK2				},	// RAM (wrofaero only?)
	{ 0x400000, 0x400001, input_port_0_r		},	// P1
	{ 0x400002, 0x400003, input_port_1_r		},	// P2
	{ 0x400004, 0x400005, input_port_2_r		},	// Coins
	{ 0x600000, 0x600003, seta_dsw_r			},	// DSW
	{ 0x700400, 0x700fff, MRA_BANK3				},	// Palette
	{ 0x701000, 0x7013ff, MRA_BANK13			},	// ? Palette ?
	{ 0x800000, 0x803fff, MRA_BANK4				},	// VRAM 0&1
	{ 0x880000, 0x883fff, MRA_BANK5				},	// VRAM 2&3
/**/{ 0x900000, 0x900005, MRA_BANK6				},	// VRAM 0&1 Ctrl
/**/{ 0x980000, 0x980005, MRA_BANK7				},	// VRAM 2&3 Ctrl
/**/{ 0xa00000, 0xa00607, MRA_BANK8				},	// Sprites Y
/**/{ 0xa80000, 0xa80001, MRA_BANK9				},	// ? 0x4000
	{ 0xb00000, 0xb03fff, MRA_BANK10			},	// Sprites Code + X + Attr
/**/{ 0xc00000, 0xc03fff, seta_sound_word_r		},	// Sound
	{ -1 }
};
static struct MemoryWriteAddress wrofaero_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM								},	// ROM
	{ 0x100000, 0x1fffff, MWA_ROM								},	// ROM (for blandia)
	{ 0x200000, 0x20ffff, MWA_BANK1								},	// RAM
	{ 0x300000, 0x30ffff, MWA_BANK2								},	// RAM (wrofaero only?)
	{ 0x500000, 0x500005, seta_vregs_w, &seta_vregs				},	// Coin Lockout + Video Registers
	{ 0x700400, 0x700fff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram	},	// Palette
	{ 0x701000, 0x7013ff, MWA_BANK13							},	// ? Palette ?
	{ 0x800000, 0x801fff, seta_vram_0_w, &seta_vram_0			},	// VRAM 0
	{ 0x802000, 0x803fff, seta_vram_1_w, &seta_vram_1			},	// VRAM 1
	{ 0x880000, 0x881fff, seta_vram_2_w, &seta_vram_2			},	// VRAM 2
	{ 0x882000, 0x883fff, seta_vram_3_w, &seta_vram_3			},	// VRAM 3
	{ 0x900000, 0x900005, MWA_BANK6, &seta_vctrl_0				},	// VRAM 0&1 Ctrl
	{ 0x980000, 0x980005, MWA_BANK7, &seta_vctrl_2				},	// VRAM 2&3 Ctrl
	{ 0xa00000, 0xa00607, MWA_BANK8, &spriteram					},	// Sprites Y
	{ 0xa80000, 0xa80001, MWA_BANK9								},	// ? 0x4000
	{ 0xb00000, 0xb03fff, MWA_BANK10, &spriteram_2				},	// Sprites Code + X + Attr
	{ 0xc00000, 0xc03fff, seta_sound_word_w, &seta_sound_ram	},	// Sound
//	{ 0xd00000, 0xd00007, MWA_NOP								},	// ?
	{ 0xe00000, 0xe00001, MWA_NOP								},	// ?
	{ 0xf00000, 0xf00001, MWA_NOP								},	// ?
	{ -1 }
};







/***************************************************************************


									Sub CPU


***************************************************************************/

static WRITE_HANDLER( sub_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);
	int bank = data >> 4;

	coin_lockout_w(0, ((~data) >> 2) & 1 );
	coin_lockout_w(1, ((~data) >> 3) & 1 );

	cpu_setbank(15, &RAM[bank*0x4000 + 0xc000]);

#if 0
{
	char buf[80];
	sprintf(buf,"%02X",	data&0xff );
	usrintf_showmessage(buf);
}
#endif
}


/***************************************************************************
								Caliber 50
***************************************************************************/

static struct MemoryReadAddress calibr50_sub_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM			},	// RAM
	{ 0x1000, 0x107f, seta_sound_r		},	// Sound
	{ 0x1080, 0x1fff, MRA_RAM			},	// RAM
	{ 0x4000, 0x4000, soundlatch_r		},	// From Main CPU
	{ 0x8000, 0xbfff, MRA_BANK15		},	// Banked ROM
	{ 0xc000, 0xffff, MRA_ROM			},	// ROM
	{ -1 }
};

static struct MemoryWriteAddress calibr50_sub_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM							},	// RAM
	{ 0x1000, 0x107f, seta_sound_w, &seta_sound_ram		},	// Sound
	{ 0x1080, 0x1fff, MWA_RAM							},	// RAM
	{ 0x4000, 0x4000, sub_bankswitch_w					},	// Bankswitching
	{ 0x8000, 0xbfff, MWA_ROM							},	// Banked ROM
	{ 0xc000, 0xc000, soundlatch2_w						},	// To Main CPU
	{ 0xc001, 0xffff, MWA_ROM							},	// ROM
	{ -1 }
};



/***************************************************************************
								DownTown
***************************************************************************/

READ_HANDLER( downtown_ip_r )
{
	int dir1 = readinputport(4);	// analog port
	int dir2 = readinputport(5);	// analog port

	dir1 = (~ (0x800 >> ((dir1 * 12)/0x100)) ) & 0xfff;
	dir2 = (~ (0x800 >> ((dir2 * 12)/0x100)) ) & 0xfff;

	switch (offset)
	{
		case 0:	return (readinputport(2) & 0xf0) + (dir1>>8);	// upper 4 bits of p1 rotation + coins
		case 1:	return (dir1&0xff);			// lower 8 bits of p1 rotation
		case 2:	return readinputport(0);	// p1
		case 3:	return 0xff;				// ?
		case 4:	return (dir2>>8);			// upper 4 bits of p2 rotation + ?
		case 5:	return (dir2&0xff);			// lower 8 bits of p2 rotation
		case 6:	return readinputport(1);	// p2
		case 7:	return 0xff;				// ?
	}

	return 0;
}

static struct MemoryReadAddress downtown_sub_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM			},	// RAM
	{ 0x0800, 0x0800, soundlatch_r		},	//
	{ 0x0801, 0x0801, soundlatch2_r		},	//
	{ 0x1000, 0x1007, downtown_ip_r		},	// Input Ports
	{ 0x5000, 0x57ff, MRA_RAM			},	// Shared RAM
	{ 0x7000, 0x7fff, MRA_ROM			},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK15		},	// Banked ROM
	{ 0xc000, 0xffff, MRA_ROM			},	// ROM
	{ -1 }
};

static struct MemoryWriteAddress downtown_sub_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM				},	// RAM
	{ 0x1000, 0x1000, sub_bankswitch_w		},	// ROM Bank + Coin Lockout
	{ 0x5000, 0x57ff, MWA_RAM, &sharedram	},	// Shared RAM
	{ 0x7000, 0xffff, MWA_ROM				},	// ROM
	{ -1 }
};




/***************************************************************************
								Meta Fox
***************************************************************************/

static struct MemoryReadAddress metafox_sub_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM			},	// RAM
	{ 0x0800, 0x0800, soundlatch_r		},	//
	{ 0x0801, 0x0801, soundlatch2_r		},	//
	{ 0x1000, 0x1000, input_port_2_r	},	// Coins
	{ 0x1002, 0x1002, input_port_0_r	},	// P1
//	{ 0x1004, 0x1004, MRA_NOP			},	// ?
	{ 0x1006, 0x1006, input_port_1_r	},	// P2
	{ 0x5000, 0x57ff, MRA_RAM			},	// Shared RAM
	{ 0x7000, 0x7fff, MRA_ROM			},	// ROM
	{ 0xc000, 0xffff, MRA_ROM			},	// ROM
	{ -1 }
};

static struct MemoryWriteAddress metafox_sub_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM				},	// RAM
	{ 0x1000, 0x1000, sub_bankswitch_w		},	// ROM Bank + Coin Lockout
	{ 0x5000, 0x57ff, MWA_RAM, &sharedram	},	// Shared RAM
	{ 0x7000, 0x7fff, MWA_ROM				},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM				},	// ROM
	{ 0xc000, 0xffff, MWA_ROM				},	// ROM
	{ -1 }
};


/***************************************************************************
								Twin Eagle
***************************************************************************/

static struct MemoryReadAddress twineagl_sub_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM			},	// RAM
	{ 0x0800, 0x0800, soundlatch_r		},	//
	{ 0x0801, 0x0801, soundlatch2_r		},	//
	{ 0x1000, 0x1000, input_port_0_r	},	// P1
	{ 0x1001, 0x1001, input_port_1_r	},	// P2
	{ 0x1002, 0x1002, input_port_2_r	},	// Coins
	{ 0x5000, 0x57ff, MRA_RAM			},	// Shared RAM
	{ 0x7000, 0x7fff, MRA_ROM			},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK15		},	// ROM
	{ 0xc000, 0xffff, MRA_ROM			},	// ROM
	{ -1 }
};

static struct MemoryWriteAddress twineagl_sub_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM				},	// RAM
	{ 0x1000, 0x1000, sub_bankswitch_w		},	// ROM Bank + Coin Lockout
	{ 0x5000, 0x57ff, MWA_RAM, &sharedram	},	// Shared RAM
	{ 0x7000, 0x7fff, MWA_ROM				},	// ROM
	{ 0xc000, 0xffff, MWA_ROM				},	// ROM
	{ -1 }
};



/***************************************************************************
								Thundercade
***************************************************************************/

static READ_HANDLER( ff_r )	{return 0xff;}

static struct MemoryReadAddress tndrcade_sub_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM					},	// RAM
	{ 0x0800, 0x0800, ff_r						},	// ? (bits 0/1/2/3: 1 -> do test 0-ff/100-1e0/5001-57ff/banked rom)
//	{ 0x0800, 0x0800, soundlatch_r				},	//
//	{ 0x0801, 0x0801, soundlatch2_r				},	//
	{ 0x1000, 0x1000, input_port_0_r			},	// P1
	{ 0x1001, 0x1001, input_port_1_r			},	// P2
	{ 0x1002, 0x1002, input_port_2_r			},	// Coins
	{ 0x2001, 0x2001, YM2203_read_port_0_r		},
	{ 0x5000, 0x57ff, MRA_RAM					},	// Shared RAM
	{ 0x6000, 0x7fff, MRA_ROM					},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK15				},	// Banked ROM
	{ 0xc000, 0xffff, MRA_ROM					},	// ROM
	{ -1 }
};

static struct MemoryWriteAddress tndrcade_sub_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM					},	// RAM
	{ 0x1000, 0x1000, sub_bankswitch_w			},	// ROM Bank + Coin Lockout
	{ 0x2000, 0x2000, YM2203_control_port_0_w	},
	{ 0x2001, 0x2001, YM2203_write_port_0_w		},
	{ 0x3000, 0x3000, YM3812_control_port_0_w	},
	{ 0x3001, 0x3001, YM3812_write_port_0_w		},
	{ 0x5000, 0x57ff, MWA_RAM, &sharedram		},	// Shared RAM
	{ 0x6000, 0xffff, MWA_ROM					},	// ROM
	{ -1 }
};







/***************************************************************************


								Input Ports


***************************************************************************/

#define	JOY_TYPE1_2BUTTONS(_n_) \
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN								) \
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START##_n_							)

#define	JOY_TYPE1_3BUTTONS(_n_) \
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START##_n_							)



#define	JOY_TYPE2_2BUTTONS(_n_) \
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN								) \
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START##_n_							)

#define	JOY_TYPE2_3BUTTONS(_n_) \
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT		|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3			|	IPF_PLAYER##_n_	) \
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START##_n_							)


#define JOY_ROTATION(_n_, _left_, _right_ ) \
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_PLAYER##_n_, 15, 15, 0, 0, KEYCODE_##_left_, KEYCODE_##_right_, 0, 0 )



/***************************************************************************
								Arbalester
***************************************************************************/

INPUT_PORTS_START( arbalest )

	PORT_START	// IN0 - Player 1
	JOY_TYPE2_2BUTTONS(1)

	PORT_START	// IN1 - Player 2
	JOY_TYPE2_2BUTTONS(2)

	PORT_START	// IN2 - Coins
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x0040, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT_IMPULSE( 0x0080, IP_ACTIVE_LOW, IPT_COIN1, 5 )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x4001, 0x4001, "Licensed To" )
	PORT_DIPSETTING(      0x0001, "Jordan" )
	PORT_DIPSETTING(      0x4000, "Romstar" )
	PORT_DIPSETTING(      0x4001, "Romstar" )
	PORT_DIPSETTING(      0x0000, "Taito" )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0004, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown 2-4" )	// not used, according to manual
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )

	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0300, "Easy" )
	PORT_DIPSETTING(      0x0200, "Hard" )
	PORT_DIPSETTING(      0x0100, "Harder" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0c00, "Never" )
	PORT_DIPSETTING(      0x0800, "300k Only" )
	PORT_DIPSETTING(      0x0400, "600k Only" )
	PORT_DIPSETTING(      0x0000, "300k & 600k" )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x1000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x3000, "3" )
	PORT_DIPSETTING(      0x2000, "5" )
//                        0x4000  License (see first dsw)
	PORT_DIPNAME( 0x8000, 0x8000, "Coinage Type" )	// not supported
	PORT_DIPSETTING(      0x8000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )

INPUT_PORTS_END



/***************************************************************************
								Blandia
***************************************************************************/

INPUT_PORTS_START( blandia )

	PORT_START	// IN0 - Player 1 - $400000.w
	JOY_TYPE1_3BUTTONS(1)

	PORT_START	// IN1 - Player 2 - $400002.w
	JOY_TYPE1_3BUTTONS(2)

	PORT_START	// IN2 - Coins - $400004.w
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0002, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "Coinage Type" )	// not supported
	PORT_DIPSETTING(      0x0002, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPNAME( 0x001c, 0x001c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x001c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x0014, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x00e0, 0x00e0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x00e0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0080, "3 Coins/7 Credit" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )

	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0200, "1" )
	PORT_DIPSETTING(      0x0300, "2" )
	PORT_DIPSETTING(      0x0100, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0800, "Easy"    )
	PORT_DIPSETTING(      0x0c00, "Normal"  )
	PORT_DIPSETTING(      0x0400, "Hard"    )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x1000, 0x1000, "2 Player Game" )
	PORT_DIPSETTING(      0x1000, "2 Credits" )
	PORT_DIPSETTING(      0x0000, "1 Credit"  )
	PORT_DIPNAME( 0x2000, 0x2000, "Continue" )
	PORT_DIPSETTING(      0x2000, "1 Credit" )
	PORT_DIPSETTING(      0x0000, "1 Coin"   )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

INPUT_PORTS_END



/***************************************************************************
								Caliber 50
***************************************************************************/

INPUT_PORTS_START( calibr50 )

	PORT_START	// IN0 - Player 1
	JOY_TYPE2_2BUTTONS(1)

	PORT_START	// IN1 - Player 2
	JOY_TYPE2_2BUTTONS(2)

	PORT_START	// IN2 - Coins
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x0040, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT_IMPULSE( 0x0080, IP_ACTIVE_LOW, IPT_COIN1, 5 )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x4001, 0x4000, "Licensed To" )
	PORT_DIPSETTING(      0x0001, "Romstar"       )
	PORT_DIPSETTING(      0x4001, "Taito America" )
	PORT_DIPSETTING(      0x4000, "Taito"         )
	PORT_DIPSETTING(      0x0000, "None (Japan)"  )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0004, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( On ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )

	PORT_DIPNAME( 0x0300, 0x0100, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0300, "Easiest" )
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0100, "Normal" )
	PORT_DIPSETTING(      0x0000, "Hard" )
	PORT_DIPNAME( 0x0400, 0x0400, "Score Digits" )
	PORT_DIPSETTING(      0x0400, "7" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0800, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x1000, 0x1000, "Display Score" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "Erase Backup Ram" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( On ) )
	//                    0x4000  Country / License (see first dsw)
	PORT_DIPNAME( 0x8000, 0x8000, "Unknown 2-7" )	/* manual: "Don't Touch" */
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START	// IN4 - Rotation Player 1
	JOY_ROTATION(1, Z, X)

	PORT_START	// IN5 - Rotation Player 2
	JOY_ROTATION(2, N, M)

INPUT_PORTS_END



/***************************************************************************
								Dragon Unit
***************************************************************************/

INPUT_PORTS_START( drgnunit )

	PORT_START	// IN0 - Player 1
	JOY_TYPE1_3BUTTONS(1)

	PORT_START	// IN1 - Player 2
	JOY_TYPE1_3BUTTONS(2)

	PORT_START	// IN2 - Coins
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0002, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_DIPNAME( 0x0010, 0x0010, "Coinage Type" ) // not supported
	PORT_DIPSETTING(      0x0010, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPNAME( 0x0020, 0x0020, "Title" )
	PORT_DIPSETTING(      0x0020, "Dragon Unit" )
	PORT_DIPSETTING(      0x0000, "Castle of Dragon" )
	PORT_DIPNAME( 0x00c0, 0x00c0, "(C) / License" )
	PORT_DIPSETTING(      0x00c0, "Athena (Japan)" )
	PORT_DIPSETTING(      0x0080, "Athena / Taito (Japan)" )
	PORT_DIPSETTING(      0x0040, "Seta USA / Taito America" )
	PORT_DIPSETTING(      0x0000, "Seta USA / Romstar" )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0003, 0x0003, "Unknown 1-0&1" )
	PORT_DIPSETTING(      0x0002, "00" )
	PORT_DIPSETTING(      0x0003, "08" )
	PORT_DIPSETTING(      0x0001, "10" )
	PORT_DIPSETTING(      0x0000, "18" )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0008, "150K, Every 300K" )
	PORT_DIPSETTING(      0x000c, "200K, Every 400K" )
	PORT_DIPSETTING(      0x0004, "300K, Every 500K" )
	PORT_DIPSETTING(      0x0000, "400K Only" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0020, "5" )
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown 1-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Unknown 1-7*" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_DIPNAME( 0x0100, 0x0100, "Unknown 2-0" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown 2-2*" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0800, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc000, 0xc000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_2C ) )

INPUT_PORTS_END



/***************************************************************************
								DownTown
***************************************************************************/

INPUT_PORTS_START( downtown )

	PORT_START	// IN0 - Player 1
	JOY_TYPE2_2BUTTONS(1)

	PORT_START	// IN1 - Player 2
	JOY_TYPE2_2BUTTONS(2)

	PORT_START	// IN2 - Coins
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x0040, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT_IMPULSE( 0x0080, IP_ACTIVE_LOW, IPT_COIN1, 5 )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0001, 0x0000, "Sales" )
	PORT_DIPSETTING(      0x0001, "Japan Only" )
	PORT_DIPSETTING(      0x0000, "World" )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0004, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0008, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )
// other coinage
#if 0
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
#endif

	PORT_DIPNAME( 0x0300, 0x0300, "Unknown 2-0&1" )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0100, "4" )
	PORT_DIPSETTING(      0x0000, "5" )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0c00, "Never" )
	PORT_DIPSETTING(      0x0800, "50K Only" )
	PORT_DIPSETTING(      0x0400, "100K Only" )
	PORT_DIPSETTING(      0x0000, "50K, Every 150K" )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x1000, "2" )
	PORT_DIPSETTING(      0x3000, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPSETTING(      0x2000, "5" )
	PORT_DIPNAME( 0x4000, 0x4000, "World License" )
	PORT_DIPSETTING(      0x4000, "Romstar" )
	PORT_DIPSETTING(      0x0000, "Taito" )
	PORT_DIPNAME( 0x8000, 0x8000, "Coinage Type" )	// not supported
	PORT_DIPSETTING(      0x8000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )

	PORT_START	// IN4 - Rotation Player 1
	JOY_ROTATION(1, Z, X)

	PORT_START	// IN5 - Rotation Player 2
	JOY_ROTATION(1, N, M)


INPUT_PORTS_END



/***************************************************************************
								Meta Fox
***************************************************************************/

INPUT_PORTS_START( metafox )

	PORT_START	// IN0
	JOY_TYPE2_2BUTTONS(1)

	PORT_START	// IN1
	JOY_TYPE2_2BUTTONS(2)

	PORT_START	// IN2
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT_IMPULSE( 0x0040, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT_IMPULSE( 0x0080, IP_ACTIVE_LOW, IPT_COIN1, 5 )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x4001, 0x4001, "Licensed To"    )
	PORT_DIPSETTING(      0x0001, "Jordan"        )
	PORT_DIPSETTING(      0x4001, "Romstar"       )
	PORT_DIPSETTING(      0x4000, "Taito"         )
	PORT_DIPSETTING(      0x0000, "Taito America" )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0004, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( On ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )

	PORT_DIPNAME( 0x0300, 0x0100, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0300, "Normal"  )
	PORT_DIPSETTING(      0x0200, "Easy"    )
	PORT_DIPSETTING(      0x0100, "Hard"    )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown 2-2" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "Unknown 2-3" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x1000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x3000, "3" )
	PORT_DIPSETTING(      0x2000, "5" )
//	PORT_DIPNAME( 0x4000, 0x4000, "License" )
	PORT_DIPNAME( 0x8000, 0x8000, "Coinage Type" )	// not supported
	PORT_DIPSETTING(      0x8000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )


INPUT_PORTS_END




/***************************************************************************
							Mobile Suit Gundam
***************************************************************************/


INPUT_PORTS_START( msgundam )

	PORT_START	// IN0 - Player 1 - $400000.w
	JOY_TYPE2_2BUTTONS(1)

	PORT_START	// IN1 - Player 2 - $400002.w
	JOY_TYPE2_2BUTTONS(2)

	PORT_START	// IN2 - Coins - $400004.w
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0002, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown 2-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_DIPNAME( 0x0100, 0x0100, "Unknown 1-0" )	// demo sound??
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "Unknown 1-1" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown 1-2" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "Unknown 1-3" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "Memory Check?" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "Unknown 1-6" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

INPUT_PORTS_END





/***************************************************************************
								Thundercade (US)
***************************************************************************/

INPUT_PORTS_START( tndrcade )

	PORT_START	// IN0 - Player 1
	JOY_TYPE1_2BUTTONS(1)

	PORT_START	// IN1 - Player 2
	JOY_TYPE1_2BUTTONS(2)

	PORT_START	// IN2 - Coins
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0002, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0003, 0x0003, "Difficulty?" )
	PORT_DIPSETTING(      0x0002, "0" )
	PORT_DIPSETTING(      0x0003, "1" )
	PORT_DIPSETTING(      0x0001, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x000c, "50K  Only" )
	PORT_DIPSETTING(      0x0004, "50K, Every 150K" )
	PORT_DIPSETTING(      0x0004, "70K, Every 200K" )
	PORT_DIPSETTING(      0x0008, "100K Only" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0010, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0020, "5" )
	PORT_DIPNAME( 0x0040, 0x0040, "Allow Continue" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0000, "Licensed To" )	// + coin mode (not supported)
	PORT_DIPSETTING(      0x0080, "Taito America Corp." )
	PORT_DIPSETTING(      0x0000, "Taito Corp. Japan" )

	PORT_DIPNAME( 0x0100, 0x0100, "Title" )
	PORT_DIPSETTING(      0x0100, "Thundercade" )
	PORT_DIPSETTING(      0x0000, "Twin Formation" )
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0400, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0800, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc000, 0xc000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_2C ) )

INPUT_PORTS_END


/***************************************************************************
								Thundercade (Japan)
***************************************************************************/

INPUT_PORTS_START( tndrcadj )

	PORT_START	// IN0 - Player 1
	JOY_TYPE1_2BUTTONS(1)

	PORT_START	// IN1 - Player 2
	JOY_TYPE1_2BUTTONS(2)

	PORT_START	// IN2 - Coins
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0002, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0003, 0x0003, "Difficulty?" )
	PORT_DIPSETTING(      0x0002, "0" )
	PORT_DIPSETTING(      0x0003, "1" )
	PORT_DIPSETTING(      0x0001, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x000c, "50K  Only" )
	PORT_DIPSETTING(      0x0004, "50K, Every 150K" )
	PORT_DIPSETTING(      0x0004, "70K, Every 200K" )
	PORT_DIPSETTING(      0x0008, "100K Only" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0010, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0020, "5" )
	PORT_DIPNAME( 0x0040, 0x0040, "Allow Continue" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_BITX(    0x0100, 0x0100, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0400, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0800, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc000, 0xc000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_2C ) )

INPUT_PORTS_END



/***************************************************************************
								Twin Eagle
***************************************************************************/

INPUT_PORTS_START( twineagl )

	PORT_START	// IN0 - Player 1
	JOY_TYPE1_2BUTTONS(1)

	PORT_START	// IN1 - Player 2
	JOY_TYPE1_2BUTTONS(2)

	PORT_START	// IN2 - Coins
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0002, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown 1-0*" )	// this is merged with 2-6!
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0004, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )

	PORT_DIPNAME( 0x0300, 0x0100, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0300, "Normal"  )
	PORT_DIPSETTING(      0x0200, "Easy"    )
	PORT_DIPSETTING(      0x0100, "Hard"    )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0c00, "Never" )
	PORT_DIPSETTING(      0x0800, "500K Only" )
	PORT_DIPSETTING(      0x0400, "1000K Only" )
	PORT_DIPSETTING(      0x0000, "500K, Every 1500K" )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x1000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x3000, "3" )
	PORT_DIPSETTING(      0x2000, "5" )
	PORT_DIPNAME( 0x4000, 0x4000, "?Continue Mode?" )
	PORT_DIPSETTING(      0x4000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPNAME( 0x8000, 0x8000, "Coinage Type" )	// not supported
	PORT_DIPSETTING(      0x8000, "1" )
	PORT_DIPSETTING(      0x0000, "2" )

INPUT_PORTS_END




/***************************************************************************
								U.S. Classic
***************************************************************************/

INPUT_PORTS_START( usclssic )

#define TRACKBALL(_dir_) \
	PORT_ANALOG( 0x0fff, 0x0000, IPT_TRACKBALL_##_dir_ | IPF_CENTER, 70, 30, 0, 0 )

	PORT_START	// IN0
	TRACKBALL(X)
	PORT_BIT   ( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT   ( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT   ( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT   ( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1
	TRACKBALL(Y)
	PORT_BIT   ( 0x1000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT   ( 0x2000, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT   ( 0x4000, IP_ACTIVE_HIGH, IPT_START1  )
	PORT_BIT   ( 0x8000, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START	// IN2
	PORT_BIT(  0x0001, IP_ACTIVE_LOW,  IPT_UNKNOWN  )	// tested (sound related?)
	PORT_BIT(  0x0002, IP_ACTIVE_LOW,  IPT_UNKNOWN  )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW,  IPT_UNKNOWN  )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW,  IPT_UNKNOWN  )
	PORT_BIT_IMPULSE( 0x0010, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0020, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0040, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT(  0x0080, IP_ACTIVE_HIGH, IPT_TILT     )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0001, 0x0001, "Credits For 9-Hole" )
	PORT_DIPSETTING(      0x0001, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0002, 0x0002, "Game Type" )
	PORT_DIPSETTING(      0x0002, "Domestic" )
	PORT_DIPSETTING(      0x0000, "Foreign" )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0004, "1" )
	PORT_DIPSETTING(      0x0008, "2" )
	PORT_DIPSETTING(      0x000c, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )

	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0400, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x3800, 0x3800, "Flight Distance" )
	PORT_DIPSETTING(      0x3800, "Normal" )
	PORT_DIPSETTING(      0x3000, "-30 Yards" )
	PORT_DIPSETTING(      0x2800, "+10 Yards" )
	PORT_DIPSETTING(      0x2000, "+20 Yards" )
	PORT_DIPSETTING(      0x1800, "+30 Yards" )
	PORT_DIPSETTING(      0x1000, "+40 Yards" )
	PORT_DIPSETTING(      0x0800, "+50 Yards" )
	PORT_DIPSETTING(      0x0000, "+60 Yards" )
	PORT_DIPNAME( 0xc000, 0xc000, "Licensed To"     )
	PORT_DIPSETTING(      0xc000, "Romstar"       )
	PORT_DIPSETTING(      0x8000, "None (Japan)"  )
	PORT_DIPSETTING(      0x4000, "Taito"         )
	PORT_DIPSETTING(      0x0000, "Taito America" )

INPUT_PORTS_END




/***************************************************************************
								Zing Zing Zip
***************************************************************************/

INPUT_PORTS_START( zingzip )

	PORT_START	// IN0 - Player 1 - $400000.w
	JOY_TYPE1_2BUTTONS(1)

	PORT_START	// IN1 - Player 2 - $400002.w
	JOY_TYPE1_2BUTTONS(2)

	PORT_START	// IN2 - Coins - $400004.w
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN  ) // no coin 2
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown 1-2" )	// remaining switches seem unused
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown 1-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown 1-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown 1-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown 1-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )

	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0100, "4" )
	PORT_DIPSETTING(      0x0000, "5" )
	PORT_DIPNAME( 0x0c00, 0x0c00, "Unknown 2-2&3" )
	PORT_DIPSETTING(      0x0800, "01" )
	PORT_DIPSETTING(      0x0c00, "08" )
	PORT_DIPSETTING(      0x0400, "10" )
	PORT_DIPSETTING(      0x0000, "18" )
	PORT_DIPNAME( 0xf000, 0xf000, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(      0xb000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0xd000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 8C_3C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0xf000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x9000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )

INPUT_PORTS_END



/***************************************************************************
								War of Aero
***************************************************************************/

INPUT_PORTS_START( wrofaero )

	PORT_START	// IN0 - Player 1 - $400000.w
	JOY_TYPE1_3BUTTONS(1)	// 3rd button selects the weapon
							// when the dsw for cheating is on

	PORT_START	// IN1 - Player 2 - $400002.w
	JOY_TYPE1_3BUTTONS(2)

	PORT_START	// IN2 - Coins - $400004.w
	PORT_BIT_IMPULSE( 0x0001, IP_ACTIVE_LOW, IPT_COIN1, 5 )
	PORT_BIT_IMPULSE( 0x0002, IP_ACTIVE_LOW, IPT_COIN2, 5 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	// IN3 - 2 DSWs - $600001 & 3.b
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "Unknown 1-1*" )	// tested
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown 1-2*" )	// tested
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BITX(    0x0008, 0x0008, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stage & Weapon Select", IP_KEY_NONE, IP_JOY_NONE )	// P2 Start Is Freeze Screen...
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown 1-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown 1-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown 1-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )

	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0100, "4" )
	PORT_DIPSETTING(      0x0000, "5" )
	PORT_DIPNAME( 0x0c00, 0x0c00, "Unknown 2-2&3" )
	PORT_DIPSETTING(      0x0800, "0" )
	PORT_DIPSETTING(      0x0c00, "1" )
	PORT_DIPSETTING(      0x0400, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0xf000, 0xf000, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(      0xb000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0xd000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 8C_3C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0xf000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x9000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )


INPUT_PORTS_END








/***************************************************************************


								Graphics Layouts

Sprites and layers use 16x16 tile, made of four 8x8 tiles. They can be 4
or 6 planes deep and are stored in a wealth of formats.

***************************************************************************/

						/* First the 4 bit tiles */


/* The bitplanes are packed togheter */
static struct GfxLayout layout_packed =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{2*4,3*4,0*4,1*4},
	{256+128,256+129,256+130,256+131, 256+0,256+1,256+2,256+3,
	 128,129,130,131, 0,1,2,3},
	{0*16,1*16,2*16,3*16,4*16,5*16,6*16,7*16,
	 32*16,33*16,34*16,35*16,36*16,37*16,38*16,39*16},
	16*16*4
};


/* The bitplanes are separated */
static struct GfxLayout layout_planes =
{
	16,16,
	RGN_FRAC(1,4),
	4,
	{ RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) },
	{0,1,2,3,4,5,6,7, 64,65,66,67,68,69,70,71},
	{0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
	 16*8,17*8,18*8,19*8,20*8,21*8,22*8,23*8 },
	16*16*1
};


/* The bitplanes are separated (but there are 2 per rom) */
static struct GfxLayout layout_planes_2roms =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{RGN_FRAC(1,2)+8, RGN_FRAC(1,2)+0, 8, 0},
	{0,1,2,3,4,5,6,7, 128,129,130,131,132,133,134,135},
	{0*16,1*16,2*16,3*16,4*16,5*16,6*16,7*16,
	 16*16,17*16,18*16,19*16,20*16,21*16,22*16,23*16 },
	16*16*2
};


/* The bitplanes are separated (but there are 2 per rom).
   Each 8x8 tile is additionally split in 2 vertical halves four bits wide,
   stored one after the other */
static struct GfxLayout layout_planes_2roms_split =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{0,4, RGN_FRAC(1,2)+0,RGN_FRAC(1,2)+4},
	{128+64,128+65,128+66,128+67, 128+0,128+1,128+2,128+3,
	 8*8+0,8*8+1,8*8+2,8*8+3, 0,1,2,3},
	{0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
	 32*8,33*8,34*8,35*8,36*8,37*8,38*8,39*8},
	16*16*2
};




						/* Then the 6 bit tiles */


/* The bitplanes are packed together: 3 roms with 2 bits in each */
static struct GfxLayout layout_packed_6bits_3roms =
{
	16,16,
	RGN_FRAC(1,3),
	6,
	{RGN_FRAC(0,3)+0,RGN_FRAC(0,3)+4,  RGN_FRAC(1,3)+0,RGN_FRAC(1,3)+4,  RGN_FRAC(2,3)+0,RGN_FRAC(2,3)+4},
	{128+64,128+65,128+66,128+67, 128+0,128+1,128+2,128+3,
	 64,65,66,67, 0,1,2,3},
	{0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
	 32*8,33*8,34*8,35*8,36*8,37*8,38*8,39*8},
	16*16*2
};


/* The bitplanes are packed togheter: 4 bits in one rom, 2 bits in another.
   Since there isn't simmetry between the two roms, we load the latter with
   ROM_LOAD_GFX_EVEN. This way we can think of it as a 4 planes rom, with the
   upper 2 planes unused.	 */

static struct GfxLayout layout_packed_6bits_2roms =
{
	16,16,
	RGN_FRAC(6,8),	// use 6 of the "8 planes"
	6,
	{RGN_FRAC(4,8), RGN_FRAC(4,8)+1*4, 2*4,3*4,0*4,1*4},
	{256+128,256+129,256+130,256+131, 256+0,256+1,256+2,256+3,
	 128,129,130,131, 0,1,2,3},
	{0*16,1*16,2*16,3*16,4*16,5*16,6*16,7*16,
	 32*16,33*16,34*16,35*16,36*16,37*16,38*16,39*16},
	16*16*4
};



/***************************************************************************
								Blandia
***************************************************************************/

static struct GfxDecodeInfo blandia_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_planes,             0,           32 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_packed_6bits_3roms, 16*32+64*32, 32 }, // [1] Layer 1
	{ REGION_GFX3, 0, &layout_packed_6bits_3roms, 16*32,       32 }, // [2] Layer 2
	{ -1 }
};

/***************************************************************************
								DownTown
***************************************************************************/

static struct GfxDecodeInfo downtown_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_planes,             512*0, 32 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_planes_2roms_split, 512*0, 32 }, // [1] Layer 1
	{ -1 }
};

/***************************************************************************
							Mobile Suit Gundam
***************************************************************************/

static struct GfxDecodeInfo msgundam_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_planes_2roms, 512*0, 32 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_packed,       512*2, 32 }, // [1] Layer 1
	{ REGION_GFX3, 0, &layout_packed,       512*1, 32 }, // [2] Layer 2
	{ -1 }
};

/***************************************************************************
								Thundercade
***************************************************************************/

static struct GfxDecodeInfo tndrcade_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_planes_2roms, 512*0, 32 }, // [0] Sprites
	{ -1 }
};

/***************************************************************************
								U.S. Classic
***************************************************************************/

/* 6 bit layer. The colors are still WRONG.
   Remeber there's a vh_init_palette */

static struct GfxDecodeInfo usclssic_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_planes,             512*0+256, 32/2 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_packed_6bits_3roms, 512*1, 32 }, // [1] Layer 1
	{ -1 }
};

/***************************************************************************
								Zing Zing Zip
***************************************************************************/

static struct GfxDecodeInfo zingzip_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_planes_2roms,       512*0, 32 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_packed_6bits_2roms, 512*2, 32 }, // [1] Layer 1
	{ REGION_GFX3, 0, &layout_packed,             512*1, 32 }, // [2] Layer 2
	{ -1 }
};










/***************************************************************************

								Machine drivers

***************************************************************************/

#define SETA_INTERRUPTS_NUM 2

static int seta_interrupt_1_and_2(void)
{
	switch (cpu_getiloops())
	{
		case 0:		return 1;
		case 1:		return 2;
		default:	return ignore_interrupt();
	}
}

static int seta_interrupt_2_and_4(void)
{
	switch (cpu_getiloops())
	{
		case 0:		return 2;
		case 1:		return 4;
		default:	return ignore_interrupt();
	}
}


#define SETA_SUB_INTERRUPTS_NUM 2

static int seta_sub_interrupt(void)
{
	switch (cpu_getiloops())
	{
		case 0:		return nmi_interrupt();
		case 1:		return interrupt();
		default:	return ignore_interrupt();
	}
}


/***************************************************************************
								Blandia
***************************************************************************/

/*
	Similar to wrofaero, but the layers are 6 planes deep (and
	the pens are strangely mapped to palette entries) + the
	samples are bankswitched
*/

void blandia_init_machine(void)
{
	blandia_samples_bank = -1;	// set the samples bank to an out of range value at start-up
}

static struct MachineDriver machine_driver_blandia =
{
	{
		{
			CPU_M68000,
			16000000,
			wrofaero_readmem, wrofaero_writemem,0,0,
			seta_interrupt_2_and_4, SETA_INTERRUPTS_NUM
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	blandia_init_machine,	/* This game bankswitches the samples */

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	blandia_gfxdecodeinfo,
	16*32+16*32+16*32, 16*32+64*32+64*32,	/* sprites, layer1, layer2 */
	blandia_vh_init_palette,				/* layers 1&2 are 6 planes deep */
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_2_layers,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_6KHz_interface
		}
	}
};


/***************************************************************************
								Caliber 50
***************************************************************************/

/*	calibr50 lev 6 = lev 2 + lev 4 !
	Test mode shows a 16ms and 4ms counters. I wonder if every game has
	5 ints per frame */

#define calibr50_INTERRUPTS_NUM (4+1)
int calibr50_interrupt(void)
{
	switch (cpu_getiloops())
	{
		case 0:
		case 1:
		case 2:
		case 3:		return 4;

		case 4:		return 2;

		default:	return ignore_interrupt();
	}
}


static struct MachineDriver machine_driver_calibr50 =
{
	{
		{
			CPU_M68000,
			8000000,
			calibr50_readmem, calibr50_writemem,0,0,
			calibr50_interrupt, calibr50_INTERRUPTS_NUM
		},
		{
			CPU_M65C02,
			1000000,	/* ?? */
			calibr50_sub_readmem, calibr50_sub_writemem,0,0,
			interrupt, 1	/* NMI caused by main cpu when writing to the sound latch */
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	downtown_gfxdecodeinfo,
	512, 512,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
//	seta_vh_start_1_layer,
	seta_vh_start_1_layer_offset_0x02,	// a little offset
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_4KHz_interface
		}
	}
};


/***************************************************************************
								DownTown
***************************************************************************/

/* downtown lev 3 = lev 2 + lev 1 ! */

static struct MachineDriver machine_driver_downtown =
{
	{
		{
			CPU_M68000,
			8000000,
			downtown_readmem, downtown_writemem,0,0,
			m68_level3_irq, 1
		},
		{
			CPU_M65C02,
			1000000,	/* ?? */
			downtown_sub_readmem, downtown_sub_writemem,0,0,
			seta_sub_interrupt, SETA_SUB_INTERRUPTS_NUM
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	downtown_gfxdecodeinfo,
	512, 512,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_1_layer,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_4KHz_interface
		}
	}
};



/***************************************************************************
								Dragon Unit
***************************************************************************/

/* Like downtown but without the sub cpu?? */

/* drgnunit lev 1 == lev 3 (writes to $500000)
   lev 2 drives the game */

static struct MachineDriver machine_driver_drgnunit =
{
	{
		{
			CPU_M68000,
			8000000,
			downtown_readmem, downtown_writemem,0,0,
			seta_interrupt_1_and_2, SETA_INTERRUPTS_NUM
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	downtown_gfxdecodeinfo,
	512, 512,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
//	seta_vh_start_1_layer,
	seta_vh_start_1_layer_offset_0x02,	// a little offset
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_4KHz_interface
		}
	}
};


/***************************************************************************
								Meta Fox
***************************************************************************/

/* metafox lev 3 = lev 2 + lev 1 ! */

static struct MachineDriver machine_driver_metafox =
{
	{
		{
			CPU_M68000,
			8000000,
			downtown_readmem, downtown_writemem,0,0,
			m68_level3_irq, 1
		},
		{
			CPU_M65C02,
			1000000,	/* ?? */
			metafox_sub_readmem, metafox_sub_writemem,0,0,
			seta_sub_interrupt, SETA_SUB_INTERRUPTS_NUM
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-8-16-1 },
	downtown_gfxdecodeinfo,
	512, 512,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_1_layer,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_4KHz_interface
		}
	}
};



/***************************************************************************
							Mobile Suit Gundam
***************************************************************************/

/* msgundam lev 2 == lev 6 ! */

static struct MachineDriver machine_driver_msgundam =
{
	{
		{
			CPU_M68000,
			16000000,
			msgundam_readmem, msgundam_writemem,0,0,
			seta_interrupt_2_and_4, SETA_INTERRUPTS_NUM
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	msgundam_gfxdecodeinfo,
	512 * 3, 512 * 3,	/* sprites, layer2, layer1 */
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_2_layers,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_6KHz_interface
		}
	}
};



/***************************************************************************
								Thundercade
***************************************************************************/

static READ_HANDLER( dsw1_r )
{
	return (readinputport(3) >> 8) & 0xff;
}

static READ_HANDLER( dsw2_r )
{
	return (readinputport(3) >> 0) & 0xff;
}

/*static void irq_handler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}*/

static struct YM2203interface tndrcade_ym2203_interface =
{
	1,
	2000000,		/* ? */
	{ YM2203_VOL(50,50) },
	{ dsw1_r },		/* input A: DSW 1 */
	{ dsw2_r },		/* input B: DSW 2 */
	{ 0 },
	{ 0 },
//	{ irq_handler }
};

static struct YM3812interface ym3812_interface =
{
	1,
	4000000,	/* ? */
	{ 80,80 },	/* mixing level */
//	{ irq_handler },
};


#define TNDRCADE_SUB_INTERRUPTS_NUM 			1+16
static int tndrcade_sub_interrupt(void)
{
	switch (cpu_getiloops())
	{
		case 0:		return nmi_interrupt();
		default:	return interrupt();
	}
}

static struct MachineDriver machine_driver_tndrcade =
{
	{
		{
			CPU_M68000,
			8000000,
			tndrcade_readmem, tndrcade_writemem,0,0,
			m68_level2_irq, 1
		},
		{
			CPU_M65C02,
			2000000,	/* ?? */
			tndrcade_sub_readmem, tndrcade_sub_writemem,0,0,
			tndrcade_sub_interrupt, TNDRCADE_SUB_INTERRUPTS_NUM
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
//	400, 256, { 16, 400-1, 0, 256-16-1 },
	400, 256, { 16, 400-1, 0+8, 256-16-8-1 },
	tndrcade_gfxdecodeinfo,
	512, 512,	/* sprites only */
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	0,	/* no need for a vh_start: no tilemaps */
	0,
	seta_vh_screenrefresh_no_layers, /* just draw the sprites */

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&tndrcade_ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		}
	}
};



/***************************************************************************
								Twin Eagle
***************************************************************************/

/* Just like metafox, but:
   the sub cpu reads the ip at different locations,
   the samples need a different playback rate,
   the visible area seems different. */

static struct MachineDriver machine_driver_twineagl =
{
	{
		{
			CPU_M68000,
			8000000,
			downtown_readmem, downtown_writemem,0,0,
			m68_level3_irq, 1
		},
		{
			CPU_M65C02,
			1000000,	/* ?? */
			twineagl_sub_readmem, twineagl_sub_writemem,0,0,
			seta_sub_interrupt, SETA_SUB_INTERRUPTS_NUM
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
//	400, 256, { 16, 400-1, 0, 256-16-1 },
	400, 256, { 16, 400-1, 0+8, 256-16-1-8 },
	downtown_gfxdecodeinfo,
	512, 512,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_1_layer,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_8KHz_interface
		}
	}
};


/***************************************************************************
								U.S. Classic
***************************************************************************/


/*	usclssic lev 6 = lev 2+4 !
	Test mode shows a 16ms and 4ms counters. I wonder if every game has
	5 ints per frame
*/

static struct MachineDriver machine_driver_usclssic =
{
	{
		{
			CPU_M68000,
			8000000,
			usclssic_readmem, usclssic_writemem,0,0,
			calibr50_interrupt, calibr50_INTERRUPTS_NUM
		},
		{
			CPU_M65C02,
			1000000,	/* ?? */
			calibr50_sub_readmem, calibr50_sub_writemem,0,0,
			interrupt, 1	/* NMI caused by main cpu when writing to the sound latch */
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	usclssic_gfxdecodeinfo,
	16*32, 16*32 + 64*32,		/* sprites, layer */
	usclssic_vh_init_palette,	/* layer is 6 planes deep */
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_1_layer,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_4KHz_interface
		}
	}
};


/***************************************************************************
								War of Aero
***************************************************************************/

/* wrofaero lev 2 almost identical to lev 6, but there's an additional
   call in the latter (sound related?) */

static struct MachineDriver machine_driver_wrofaero =
{
	{
		{
			CPU_M68000,
			16000000,
			wrofaero_readmem, wrofaero_writemem,0,0,
			m68_level6_irq, 1
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	msgundam_gfxdecodeinfo,
	512 * 3, 512 * 3,	/* sprites, layer1, layer2 */
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_2_layers,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_6KHz_interface
		}
	}
};




/***************************************************************************
								Zing Zing Zip
***************************************************************************/

/* zingzip lev 3 = lev 2 + lev 1 !
   SR = 2100 -> lev1 is ignored so we must supply int 3, since the routine
   at int 1 is necessary: it plays the background music.
 */

static struct MachineDriver machine_driver_zingzip =
{
	{
		{
			CPU_M68000,
			16000000,
			wrofaero_readmem, wrofaero_writemem,0,0,
			m68_level3_irq, 1
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	400, 256, { 16, 400-1, 0, 256-16-1 },
	zingzip_gfxdecodeinfo,
	16*32+16*32+16*32, 16*32+16*32+64*32,	/* sprites, layer2, layer1 */
	zingzip_vh_init_palette,				/* layer 1 gfx is 6 planes deep */
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	seta_vh_start_2_layers,
	0,
	seta_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&seta_6KHz_interface
		}
	}
};





/***************************************************************************


								ROMs Loading


***************************************************************************/




/***************************************************************************

								Arbalester

***************************************************************************/

ROM_START( arbalest )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "uk001.03",  0x000000, 0x040000, 0xee878a2c )
	ROM_LOAD_ODD(  "uk001.04",  0x000000, 0x040000, 0x902bb4e3 )

	ROM_REGION( 0x010000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "uk001.05", 0x006000, 0x002000, 0x0339fc53 )
	ROM_RELOAD(           0x008000, 0x002000  )
	ROM_RELOAD(           0x00a000, 0x002000  )
	ROM_RELOAD(           0x00c000, 0x002000  )
	ROM_RELOAD(           0x00e000, 0x002000  )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "uk001.06", 0x000000, 0x040000, 0x11c75746 )
	ROM_LOAD( "uk001.07", 0x040000, 0x040000, 0x01b166c7 )
	ROM_LOAD( "uk001.08", 0x080000, 0x040000, 0x78d60ba3 )
	ROM_LOAD( "uk001.09", 0x0c0000, 0x040000, 0xb4748ae0 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "uk001.10", 0x000000, 0x080000, 0xc1e2f823 )
	ROM_LOAD( "uk001.11", 0x080000, 0x080000, 0x09dfe56a )
	ROM_LOAD( "uk001.12", 0x100000, 0x080000, 0x818a4085 )
	ROM_LOAD( "uk001.13", 0x180000, 0x080000, 0x771fa164 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "uk001.15", 0x000000, 0x080000, 0xce9df5dd )
	ROM_LOAD( "uk001.14", 0x080000, 0x080000, 0x016b844a )

ROM_END

READ_HANDLER( arbalest_protection_r )
{
	return 0;
}

void init_arbalest(void)
{
	install_mem_read_handler(0, 0x80000, 0x8000f, arbalest_protection_r);
}




/***************************************************************************

								Blandia [Prototype]

PCB:	P0-072-2
CPU:	68000-16
Sound:	X1-010
OSC:	16.0000MHz

Chips:	X1-001A		X1-004					X1-011 x2
		X1-002A		X1-007		X1-010		X1-012 x2

***************************************************************************/

ROM_START( blandia )

	ROM_REGION( 0x200000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "prg-even.bin", 0x000000, 0x040000, 0x7ecd30e8 )
	ROM_LOAD_ODD(  "prg-odd.bin",  0x000000, 0x040000, 0x42b86c15 )
	ROM_LOAD_EVEN( "tbl0.bin",     0x100000, 0x080000, 0x69b79eb8 )
	ROM_LOAD_ODD(  "tbl1.bin",     0x100000, 0x080000, 0xcf2fd350 )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "o-1.bin",  0x000000, 0x080000, 0x4c67b7f0 )
	ROM_LOAD( "o-5.bin",  0x080000, 0x080000, 0x40bee78b )
	ROM_LOAD( "o-0.bin",  0x100000, 0x080000, 0x5e7b8555 )
	ROM_LOAD( "o-4.bin",  0x180000, 0x080000, 0x7c634784 )
	ROM_LOAD( "o-3.bin",  0x200000, 0x080000, 0x387fc7c4 )
	ROM_LOAD( "o-7.bin",  0x280000, 0x080000, 0xfc77b04a )
	ROM_LOAD( "o-2.bin",  0x300000, 0x080000, 0xc669bb49 )
	ROM_LOAD( "o-6.bin",  0x380000, 0x080000, 0x92882943 )

	ROM_REGION( 0x0c0000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "v1-2.bin",  0x000000, 0x020000, 0xd524735e )
	ROM_LOAD( "v1-5.bin",  0x020000, 0x020000, 0xeb440cdb )
	ROM_LOAD( "v1-1.bin",  0x040000, 0x020000, 0x09bdf75f )
	ROM_LOAD( "v1-4.bin",  0x060000, 0x020000, 0x803911e5 )
	ROM_LOAD( "v1-0.bin",  0x080000, 0x020000, 0x73617548 )
	ROM_LOAD( "v1-3.bin",  0x0a0000, 0x020000, 0x7f18e4fb )

	ROM_REGION( 0x0c0000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "v2-2.bin",  0x000000, 0x020000, 0xc4f15638 )	// identical to v2-1
	ROM_LOAD( "v2-5.bin",  0x020000, 0x020000, 0xc2e57622 )
	ROM_LOAD( "v2-1.bin",  0x040000, 0x020000, 0xc4f15638 )
	ROM_LOAD( "v2-4.bin",  0x060000, 0x020000, 0x16ec2130 )
	ROM_LOAD( "v2-0.bin",  0x080000, 0x020000, 0x5b05eba9 )
	ROM_LOAD( "v2-3.bin",  0x0a0000, 0x020000, 0x80ad0c3b )

	/* 6KHz? The c0000-fffff region is bankswitched */
	ROM_REGION( 0x240000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "s-0.bin",  0x000000, 0x020000, 0xa5fde408 )
	ROM_CONTINUE(         0x140000, 0x020000             )
	ROM_LOAD( "s-1.bin",  0x020000, 0x020000, 0x3083f9c4 )
	ROM_CONTINUE(         0x160000, 0x020000             )
	ROM_LOAD( "s-2.bin",  0x040000, 0x020000, 0xa591c9ef )
	ROM_CONTINUE(         0x180000, 0x020000             )
	ROM_LOAD( "s-3.bin",  0x060000, 0x020000, 0x68826c9d )
	ROM_CONTINUE(         0x1a0000, 0x020000             )
	ROM_LOAD( "s-4.bin",  0x080000, 0x020000, 0x1c7dc8c2 )
	ROM_CONTINUE(         0x1c0000, 0x020000             )
	ROM_LOAD( "s-5.bin",  0x0a0000, 0x020000, 0x4bb0146a )
	ROM_CONTINUE(         0x1e0000, 0x020000             )
	ROM_LOAD( "s-6.bin",  0x100000, 0x020000, 0x9f8f34ee )	// skip c0000-fffff (banked region)
	ROM_CONTINUE(         0x200000, 0x020000             )	// this half is 0
	ROM_LOAD( "s-7.bin",  0x120000, 0x020000, 0xe077dd39 )
	ROM_CONTINUE(         0x220000, 0x020000             )	// this half is 0

ROM_END



/***************************************************************************

								Caliber 50

CPU:   TMP 68000N-8, 65C02
Other: NEC D4701

UH-001-006        SW2  SW1
UH-001-007
UH-001-008                    8464         68000-8
UH-001-009  X1-002A X1-001A   8464         Uh-002-001=T01
UH-001-010                    8464            51832
UH-001-011                    8464            51832
                                           UH-001-002
UH-001-012            X1-012               UH-001-003
UH-001-013                               UH-002-004-T02
                      X1-011               5116-10
                                           BAT
                         16MHz
             X1-010   65C02      X1-006
                      UH-001-005 X1-007
                      4701       X1-004


***************************************************************************/

ROM_START( calibr50 )

	ROM_REGION( 0x0a0000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "uh002001.u45", 0x000000, 0x040000, 0xeb92e7ed )
	ROM_LOAD_ODD(  "uh002004.u41", 0x000000, 0x040000, 0x5a0ed31e )
	ROM_LOAD_EVEN( "uh001003.9a",  0x080000, 0x010000, 0x0d30d09f )
	ROM_LOAD_ODD(  "uh001002.7a",  0x080000, 0x010000, 0x7aecc3f9 )

	ROM_REGION( 0x04c000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "uh001005.u61", 0x004000, 0x040000, 0x4a54c085 )
	ROM_RELOAD(               0x00c000, 0x040000             )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "uh001006.ux2", 0x000000, 0x080000, 0xfff52f91 )
	ROM_LOAD( "uh001007.ux1", 0x080000, 0x080000, 0xb6c19f71 )
	ROM_LOAD( "uh001008.ux6", 0x100000, 0x080000, 0x7aae07ef )
	ROM_LOAD( "uh001009.ux0", 0x180000, 0x080000, 0xf85da2c5 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "uh001010.u3x", 0x000000, 0x080000, 0xf986577a )
	ROM_LOAD( "uh001011.u50", 0x080000, 0x080000, 0x08620052 )

	/* 4KHz? */
	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "uh001013.u60", 0x000000, 0x080000, 0x09ec0df6 )
	ROM_LOAD( "uh001012.u46", 0x080000, 0x080000, 0xbb996547 )

ROM_END


/***************************************************************************

								DownTown

***************************************************************************/

ROM_START( downtown )

	ROM_REGION( 0x0a0000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "ud2001.000", 0x000000, 0x040000, 0xf1965260 )
	ROM_LOAD_ODD(  "ud2001.003", 0x000000, 0x040000, 0xe7d5fa5f )
	ROM_LOAD_EVEN( "ud2000.002", 0x080000, 0x010000, 0xca976b24 )
	ROM_LOAD_ODD(  "ud2000.001", 0x080000, 0x010000, 0x1708aebd )

	ROM_REGION( 0x04c000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "ud2002.004", 0x004000, 0x040000, 0xbbd538b1 )
	ROM_RELOAD(             0x00c000, 0x040000             )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "ud2005.t01", 0x000000, 0x080000, 0x77e6d249 )
	ROM_LOAD( "ud2006.t02", 0x080000, 0x080000, 0x6e381bf2 )
	ROM_LOAD( "ud2007.t03", 0x100000, 0x080000, 0x737b4971 )
	ROM_LOAD( "ud2008.t04", 0x180000, 0x080000, 0x99b9d757 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "ud2009.t05", 0x000000, 0x080000, 0xaee6c581 )
	ROM_LOAD( "ud2010.t06", 0x080000, 0x080000, 0x3d399d54 )

	/* 4KHz? */
	ROM_REGION( 0x080000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "ud2011.t07", 0x000000, 0x080000, 0x9c9ff69f )

ROM_END


/* Protection. NVRAM is handled writing commands here */
unsigned char downtown_protection[0x200];
static READ_HANDLER( downtown_protection_r )
{
	int job = READ_WORD(&downtown_protection[0xf8]) & 0xff;

	switch (job)
	{
		case 0xa3:
		{
			char word[] = "WALTZ0";
			if (offset >= 0x100 && offset <= 0x10a)	return word[(offset-0x100)/2];
			else									return 0;
		}
		default:
			return READ_WORD(&downtown_protection[offset]) & 0xff;
	}
}
static WRITE_HANDLER( downtown_protection_w )
{
	COMBINE_WORD_MEM(&downtown_protection[offset], data);
}

void init_downtown(void)
{
	install_mem_read_handler (0, 0x200000, 0x2001ff, downtown_protection_r);
	install_mem_write_handler(0, 0x200000, 0x2001ff, downtown_protection_w);
}




/***************************************************************************

								Dragon Unit
					 [Prototype of "Castle Of Dragon"]

PCB:	P0-053-1
CPU:	68000-8
Sound:	X1-010
OSC:	16.0000MHz

Chips:	X1-001A, X1-002A, X1-004, X1-006, X1-007, X1-010, X1-011, X1-012

***************************************************************************/

ROM_START( drgnunit )

	ROM_REGION( 0x040000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "prg-e.bin", 0x000000, 0x020000, 0x728447df )
	ROM_LOAD_ODD(  "prg-o.bin", 0x000000, 0x020000, 0xb2f58ecf )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "obj-2.bin", 0x000000, 0x020000, 0xd7f6ab5a )
	ROM_LOAD( "obj-6.bin", 0x020000, 0x020000, 0x80b801f7 )
	ROM_LOAD( "obj-1.bin", 0x040000, 0x020000, 0x53a95b13 )
	ROM_LOAD( "obj-5.bin", 0x060000, 0x020000, 0x6b87bc20 )
	ROM_LOAD( "obj-4.bin", 0x080000, 0x020000, 0x60d17771 )
	ROM_LOAD( "obj-8.bin", 0x0a0000, 0x020000, 0x826c1543 )
	ROM_LOAD( "obj-3.bin", 0x0c0000, 0x020000, 0x0bccd4d5 )
	ROM_LOAD( "obj-7.bin", 0x0e0000, 0x020000, 0xcbaa7f6a )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "scr-1o.bin",  0x000000, 0x020000, 0x671525db )
	ROM_LOAD( "scr-2o.bin",  0x020000, 0x020000, 0x2a3f2ed8 )
	ROM_LOAD( "scr-3o.bin",  0x040000, 0x020000, 0x4d33a92d )
	ROM_LOAD( "scr-4o.bin",  0x060000, 0x020000, 0x79a0aa61 )
	ROM_LOAD( "scr-1e.bin",  0x080000, 0x020000, 0xdc9cd8c9 )
	ROM_LOAD( "scr-2e.bin",  0x0a0000, 0x020000, 0xb6126b41 )
	ROM_LOAD( "scr-3e.bin",  0x0c0000, 0x020000, 0x1592b8c2 )
	ROM_LOAD( "scr-4e.bin",  0x0e0000, 0x020000, 0x8201681c )

	/* 4 KHz? */
	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "snd-1.bin", 0x000000, 0x020000, 0x8f47bd0d )
	ROM_LOAD( "snd-2.bin", 0x020000, 0x020000, 0x65c40ef5 )
	ROM_LOAD( "snd-3.bin", 0x040000, 0x020000, 0x71fbd54e )
	ROM_LOAD( "snd-4.bin", 0x060000, 0x020000, 0xac50133f )
	ROM_LOAD( "snd-5.bin", 0x080000, 0x020000, 0x70652f2c )
	ROM_LOAD( "snd-6.bin", 0x0a0000, 0x020000, 0x10a1039d )
	ROM_LOAD( "snd-7.bin", 0x0c0000, 0x020000, 0xdecbc8b0 )
	ROM_LOAD( "snd-8.bin", 0x0e0000, 0x020000, 0x3ac51bee )

ROM_END

void init_drgnunit(void)
{
	install_mem_read_handler (0, 0xb00000, 0xb00001, input_port_0_r);
	install_mem_read_handler (0, 0xb00002, 0xb00003, input_port_1_r);
	install_mem_read_handler (0, 0xb00004, 0xb00005, input_port_2_r);
}



/***************************************************************************

									Meta Fox

(Seta 1990)

P0-045A

P1-006-163                    8464   68000-8
P1-007-164    X1-002A X1-001A 8464
P1-008-165                    8464
P1-009-166                    8464     256K-12
                                       256K-12

                 X1-012
                 X1-011


   2063    X1-010     X1-006     X0-006
                      X1-007
                      X1-004     X1-004

----------------------
P1-036-A

UP-001-010
UP-001-011
UP-001-012
UP-001-013


UP-001-014
UP-001-015

-----------------------
P1-049-A

              UP-001-001
              UP-001-002
              P1-003-161
              P1-004-162


              UP-001-005
              x

***************************************************************************/

ROM_START( metafox )

	ROM_REGION( 0x0a0000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "p1003161", 0x000000, 0x040000, 0x4fd6e6a1 )
	ROM_LOAD_ODD(  "p1004162", 0x000000, 0x040000, 0xb6356c9a )
	ROM_LOAD_EVEN( "up001002", 0x080000, 0x010000, 0xce91c987 )
	ROM_LOAD_ODD(  "up001001", 0x080000, 0x010000, 0x0db7a505 )

	ROM_REGION( 0x010000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "up001005", 0x006000, 0x002000, 0x2ac5e3e3 )
	ROM_RELOAD(           0x008000, 0x002000  )
	ROM_RELOAD(           0x00a000, 0x002000  )
	ROM_RELOAD(           0x00c000, 0x002000  )
	ROM_RELOAD(           0x00e000, 0x002000  )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "p1006163", 0x000000, 0x040000, 0x80f69c7c )
	ROM_LOAD( "p1007164", 0x040000, 0x040000, 0xd137e1a3 )
	ROM_LOAD( "p1008165", 0x080000, 0x040000, 0x57494f2b )
	ROM_LOAD( "p1009166", 0x0c0000, 0x040000, 0x8344afd2 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "up001010", 0x000000, 0x080000, 0xbfbab472 )
	ROM_LOAD( "up001011", 0x080000, 0x080000, 0x26cea381 )
	ROM_LOAD( "up001012", 0x100000, 0x080000, 0xfed2c5f9 )
	ROM_LOAD( "up001013", 0x180000, 0x080000, 0xadabf9ea )

	/* 4KHz? */
	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "up001015", 0x000000, 0x080000, 0x2e20e39f )
	ROM_LOAD( "up001014", 0x080000, 0x080000, 0xfca6315e )

ROM_END


void init_metafox(void)
{
	unsigned char *RAM;

	/* This game uses the 21c000-21cfff area for protecton? */
	install_mem_read_handler (0, 0x200000, 0x2001ff, MRA_NOP);
	install_mem_write_handler(0, 0x200000, 0x2001ff, MWA_NOP);

	RAM	= memory_region(REGION_CPU1);
	WRITE_WORD(&RAM[0x8ab1c], 0x0000);	// patch protection test: "cp error"
	WRITE_WORD(&RAM[0x8ab1e], 0x0000);
	WRITE_WORD(&RAM[0x8ab20], 0x0000);
}



/***************************************************************************

							Mobile Suit Gundam

Banpresto 1993
P0-081A
                               SW2  SW1

FA-001-008                          FA-001-001
FA-001-007    X1-002A X1-001A       FA-002-002
                              5160
                              5160
                                        71054
FA-001-006                    5160     62256
FA-001-005    X1-011  X1-012  5160     62256

FA-001-004    X1-011  X1-012  5160
5160                          5160

                                68000-16

                                         16MHz
  X1-010
                    X1-007   X1-004     X1-005

***************************************************************************/

ROM_START( msgundam )

	ROM_REGION( 0x200000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_WIDE_SWAP( "fa002",  0x000000, 0x080000, 0xdee3b083 )
	ROM_LOAD_WIDE_SWAP( "fa001",  0x100000, 0x100000, 0xfca139d0 )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "fa008",  0x000000, 0x200000, 0xe7accf48 )
	ROM_LOAD( "fa007",  0x200000, 0x200000, 0x793198a6 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "fa006",  0x000000, 0x100000, 0x3b60365c )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "fa005",  0x000000, 0x080000, 0x8cd7ff86 )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "fa004",  0x000000, 0x100000, 0xb965f07c )

ROM_END






/***************************************************************************

						Thundercade / Twin Formation

CPU: HD68000PS8
SND: YM3812, YM2203C
OSC: 16Mhz

This PCB is loaded with custom SETA chips as follows
X1-001 (also has written YM3906)
X1-002 (also has written YM3909)
X1-003
X1-004
X1-006

Rom code is UAO, M/B code is M6100287A (the TAITO logo is written also)

P0-029-A

  UA0-4 UA0-3 4364 UA0-2 UA0-1 4364  X1-001  16MHz  X1-002
  68000-8
                         4364 4364   UA0-9  UA0-8  UA0-7  UA0-6
                                     UA0-13 UA0-12 UA0-11 UA0-10
     X0-006
  UA10-5 2016 YM3812 YM2203  SW1
                             SW2                   X1-006
                                     X1-004
                                                 X1-003

***************************************************************************/

ROM_START( tndrcade )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "ua0-4.1l", 0x000000, 0x020000, 0x73bd63eb )
	ROM_LOAD_ODD(  "ua0-2.1h", 0x000000, 0x020000, 0xe96194b1 )
	ROM_LOAD_EVEN( "ua0-3.1k", 0x040000, 0x020000, 0x0a7b1c41 )
	ROM_LOAD_ODD(  "ua0-1.1g", 0x040000, 0x020000, 0xfa906626 )

	ROM_REGION( 0x02c000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "ua10-5.8m", 0x004000, 0x020000, 0x8eff6122 )	// $1fffd=2 (country code)
	ROM_RELOAD(            0x00c000, 0x020000             )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "ua0-10", 0x000000, 0x040000, 0xaa7b6757 )
	ROM_LOAD( "ua0-11", 0x040000, 0x040000, 0x11eaf931 )
	ROM_LOAD( "ua0-12", 0x080000, 0x040000, 0x00b5381c )
	ROM_LOAD( "ua0-13", 0x0c0000, 0x040000, 0x8f9a0ed3 )
	ROM_LOAD( "ua0-6",  0x100000, 0x040000, 0x14ecc7bb )
	ROM_LOAD( "ua0-7",  0x140000, 0x040000, 0xff1a4e68 )
	ROM_LOAD( "ua0-8",  0x180000, 0x040000, 0x936e1884 )
	ROM_LOAD( "ua0-9",  0x1c0000, 0x040000, 0xe812371c )

ROM_END


ROM_START( tndrcadj )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "ua0-4.1l", 0x000000, 0x020000, 0x73bd63eb )
	ROM_LOAD_ODD(  "ua0-2.1h", 0x000000, 0x020000, 0xe96194b1 )
	ROM_LOAD_EVEN( "ua0-3.1k", 0x040000, 0x020000, 0x0a7b1c41 )
	ROM_LOAD_ODD(  "ua0-1.1g", 0x040000, 0x020000, 0xfa906626 )

	ROM_REGION( 0x02c000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "thcade5.bin", 0x004000, 0x020000, 0x8cb9df7b )	// $1fffd=1 (country code jp)
	ROM_RELOAD(              0x00c000, 0x020000             )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "ua0-10", 0x000000, 0x040000, 0xaa7b6757 )
	ROM_LOAD( "ua0-11", 0x040000, 0x040000, 0x11eaf931 )
	ROM_LOAD( "ua0-12", 0x080000, 0x040000, 0x00b5381c )
	ROM_LOAD( "ua0-13", 0x0c0000, 0x040000, 0x8f9a0ed3 )
	ROM_LOAD( "ua0-6",  0x100000, 0x040000, 0x14ecc7bb )
	ROM_LOAD( "ua0-7",  0x140000, 0x040000, 0xff1a4e68 )
	ROM_LOAD( "ua0-8",  0x180000, 0x040000, 0x936e1884 )
	ROM_LOAD( "ua0-9",  0x1c0000, 0x040000, 0xe812371c )

ROM_END


/***************************************************************************

								Twin Eagle

M6100326A	Taito (Seta)

ua2-4              68000
ua2-3
ua2-6
ua2-5
ua2-8
ua2-10
ua2-7               ua2-1
ua2-9
ua2-12
ua2-11              ua2-2

***************************************************************************/

ROM_START( twineagl )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_WIDE( "ua2-1", 0x000000, 0x080000, 0x5c3fe531 )

	ROM_REGION( 0x010000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "ua2-2", 0x006000, 0x002000, 0x783ca84e )
	ROM_RELOAD(        0x008000, 0x002000  )
	ROM_RELOAD(        0x00a000, 0x002000  )
	ROM_RELOAD(        0x00c000, 0x002000  )
	ROM_RELOAD(        0x00e000, 0x002000  )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "ua2-4",  0x000000, 0x040000, 0x8b7532d6 )
	ROM_LOAD( "ua2-3",  0x040000, 0x040000, 0x1124417a )
	ROM_LOAD( "ua2-6",  0x080000, 0x040000, 0x99d8dbba )
	ROM_LOAD( "ua2-5",  0x0c0000, 0x040000, 0x6e450d28 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "ua2-8",  0x000000, 0x080000, 0x7d3a8d73 )
	ROM_LOAD( "ua2-10", 0x080000, 0x080000, 0x5bbe1f56 )
	ROM_LOAD( "ua2-7",  0x100000, 0x080000, 0xfce56907 )
	ROM_LOAD( "ua2-9",  0x180000, 0x080000, 0xa451eae9 )

	/* 8KHz? */
	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "ua2-11", 0x000000, 0x080000, 0x624e6057 )
	ROM_LOAD( "ua2-12", 0x080000, 0x080000, 0x3068ff64 )

ROM_END


READ_HANDLER( twineagl_protection_r )
{
	return 0;
}

void init_twineagl(void)
{
	int i;
	unsigned char *RAM;

	/* Protection? */
	install_mem_read_handler(0, 0x800000, 0x8000ff, twineagl_protection_r);

	RAM	= memory_region(REGION_GFX2);

#if 1
	// waterfalls: tiles 3e00-3fff must be a copy of 2e00-2fff ??
	for (i = 0x3e00 * (16*16*2/8); i < 0x3f00 * (16*16*2/8); i++)
	{
		RAM[i+0x000000] = RAM[i+0x000000 - 0x700*(16*16*2/8)];
		RAM[i+0x100000] = RAM[i+0x100000 - 0x700*(16*16*2/8)];
	}


	// Sea level:  tiles 3e00-3fff must be a copy of 3700-38ff ??
	for (i = 0x3f00 * (16*16*2/8); i < 0x4000 * (16*16*2/8); i++)
	{
		RAM[i+0x000000] = RAM[i+0x000000 - 0x1000*(16*16*2/8)];
		RAM[i+0x100000] = RAM[i+0x100000 - 0x1000*(16*16*2/8)];
	}

#endif
}




/***************************************************************************

								U.S. Classic

M6100430A (Taito 1989)

       u7 119  u6 118   u5 117   u4 116
                                         68000-8
u13  120                                 000
u19  121                                 001
u21  122                                 002
u29  123                                 003
u33  124
u40  125
u44  126
u51  127
u58  128
u60  129                                 65c02
u68  130
u75  131                                 u61 004

                                         u83 132

***************************************************************************/

ROM_START( usclssic )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "ue2001.u20", 0x000000, 0x020000, 0x18b41421 )
	ROM_LOAD_ODD(  "ue2000.u14", 0x000000, 0x020000, 0x69454bc2 )
	ROM_LOAD_EVEN( "ue2002.u22", 0x040000, 0x020000, 0xa7bbe248 )
	ROM_LOAD_ODD(  "ue2003.u30", 0x040000, 0x020000, 0x29601906 )

	ROM_REGION( 0x04c000, REGION_CPU2 )		/* 65c02 Code */
	ROM_LOAD( "ue002u61.004", 0x004000, 0x040000, 0x476e9f60 )
	ROM_RELOAD(               0x00c000, 0x040000             )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "ue001009.119", 0x000000, 0x080000, 0xdc065204 )
	ROM_LOAD( "ue001008.118", 0x080000, 0x080000, 0x5947d9b5 )
	ROM_LOAD( "ue001007.117", 0x100000, 0x080000, 0xb48a885c )
	ROM_LOAD( "ue001006.116", 0x180000, 0x080000, 0xa6ab6ef4 )

	ROM_REGION( 0x600000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "ue001010.120", 0x000000, 0x080000, 0xdd683031 )	// planes 01
	ROM_LOAD( "ue001011.121", 0x080000, 0x080000, 0x0e27bc49 )
	ROM_LOAD( "ue001012.122", 0x100000, 0x080000, 0x961dfcdc )
	ROM_LOAD( "ue001013.123", 0x180000, 0x080000, 0x03e9eb79 )

	ROM_LOAD( "ue001014.124", 0x200000, 0x080000, 0x9576ace7 )	// planes 23
	ROM_LOAD( "ue001015.125", 0x280000, 0x080000, 0x631d6eb1 )
	ROM_LOAD( "ue001016.126", 0x300000, 0x080000, 0xf44a8686 )
	ROM_LOAD( "ue001017.127", 0x380000, 0x080000, 0x7f568258 )

	ROM_LOAD( "ue001018.128", 0x400000, 0x080000, 0x4bd98f23 )	// planes 45
	ROM_LOAD( "ue001019.129", 0x480000, 0x080000, 0x6d9f5a33 )
	ROM_LOAD( "ue001020.130", 0x500000, 0x080000, 0xbc07403f )
	ROM_LOAD( "ue001021.131", 0x580000, 0x080000, 0x98c03efd )

	/* 4KHz? */
	ROM_REGION( 0x080000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "ue001005.132", 0x000000, 0x080000, 0xc5fea37c )

ROM_END




/***************************************************************************

								Zing Zing Zip

P0-079A

UY-001-005   X1-002A   X1-001A   5168-10      256k-12
UY-001-006                       5168-10      UY-001-001
UY-001-007                                    UY-001-002
UY-001-008   X1-011 X1-012                    58257-12
                                 5168-10
UY-001-010   X1-011 X1-012       5168-10
UY-001-017
UY-001-018
                                 5168-10
X1-010                           5168-10       68000-16


                           8464-80
                           8464-80       16MHz


                             X1-007    X1-004

***************************************************************************/

ROM_START( zingzip )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "uy001001",  0x000000, 0x040000, 0x1a1687ec )
	ROM_LOAD_ODD(  "uy001002",  0x000000, 0x040000, 0x62e3b0c4 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "uy001016",  0x000000, 0x080000, 0x46e4a7d8 )
	ROM_LOAD( "uy001015",  0x080000, 0x080000, 0x4aac128e )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "uy001008",  0x000000, 0x200000, 0x0d07d34b ) // FIRST AND SECOND HALF IDENTICAL
	ROM_LOAD_GFX_EVEN( "uy001007",  0x100000, 0x080000, 0xec5b3ab9 )

	ROM_REGION( 0x200000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "uy001010",  0x000000, 0x200000, 0x0129408a ) // FIRST AND SECOND HALF IDENTICAL

	/* 6KHz? */
	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "uy001017",  0x000000, 0x080000, 0xd2cda2eb )
	ROM_LOAD( "uy001018",  0x080000, 0x080000, 0x3d30229a )

ROM_END



/***************************************************************************

							   War of Aero
							Project M E I O U

93111A	YANG CHENG

CPU   : TOSHIBA TMP68HC000N-16
Sound : ?
OSC   : 16.000000MHz

***************************************************************************/

ROM_START( wrofaero )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* 68000 Code */
	ROM_LOAD_EVEN( "u3.bin",  0x000000, 0x040000, 0x9b896a97 )
	ROM_LOAD_ODD(  "u4.bin",  0x000000, 0x040000, 0xdda84846 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "u64.bin",  0x000000, 0x080000, 0xf06ccd78 )
	ROM_LOAD( "u63.bin",  0x080000, 0x080000, 0x2a602a1b )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u66.bin",  0x000000, 0x080000, 0xc9fc6a0c )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 2 */
	ROM_LOAD( "u68.bin",  0x000000, 0x080000, BADCRC(0x3b9cc2b9) )

	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "u69.bin",  0x000000, 0x080000, 0x957ecd41 )
	ROM_LOAD( "u70.bin",  0x080000, 0x080000, 0x8d756fdf )

ROM_END


void init_wrofaero(void)
{
	unsigned char *RAM;

	RAM	= memory_region(REGION_GFX3);	// layer 2's tile 0 has some
	RAM[0] = 0;							// opaque pixels (bad dump)
}




/***************************************************************************

								Game Drivers

***************************************************************************/

GAMEX( 1987, tndrcade, 0,        tndrcade, tndrcade, 0,        ROT270, "[Seta] (Taito license)", "Thundercade / Twin Formation", GAME_IMPERFECT_SOUND ) // Title/License: DSW
GAMEX( 1987, tndrcadj, tndrcade, tndrcade, tndrcadj, 0,        ROT270, "[Seta] (Taito license)", "Tokusyu Butai UAG (Japan)", GAME_IMPERFECT_SOUND ) // License: DSW
GAMEX( 1988, twineagl, 0,        twineagl, twineagl, twineagl, ROT270, "Seta (Taito license)", "Twin Eagle (Japan)",  GAME_IMPERFECT_SOUND )
GAMEX( 1989, calibr50, 0,        calibr50, calibr50, 0,        ROT270, "Athena / Seta",        "Caliber 50",          GAME_IMPERFECT_SOUND ) // Country/License: DSW
GAMEX( 1989, drgnunit, 0,        drgnunit, drgnunit, drgnunit, ROT0,   "Seta",                 "Dragon Unit / Castle of Dragon", GAME_IMPERFECT_SOUND )
GAMEX( 1989, downtown, 0,        downtown, downtown, downtown, ROT270, "Seta",                 "DownTown",            GAME_IMPERFECT_SOUND ) // Country/License: DSW
GAMEX( 1989, usclssic, 0,        usclssic, usclssic, 0,        ROT270, "Seta",                 "U.S. Classic",        GAME_IMPERFECT_SOUND | GAME_WRONG_COLORS ) // Country/License: DSW
GAMEX( 1989, arbalest, 0,        metafox,  arbalest, arbalest, ROT270, "Seta",                 "Arbalester",          GAME_IMPERFECT_SOUND ) // Country/License: DSW
GAMEX( 1989, metafox,  0,        metafox,  metafox,  metafox,  ROT270, "Seta",                 "Meta Fox",            GAME_IMPERFECT_SOUND ) // Country/License: DSW
GAMEX( 1992, blandia,  0,        blandia,  blandia,  0,        ROT0,   "Allumer",              "Blandia [Prototype]", GAME_IMPERFECT_SOUND )
GAMEX( 1992, zingzip,  0,        zingzip,  zingzip,  0,        ROT270, "Allumer + Tecmo",      "Zing Zing Zip",       GAME_IMPERFECT_SOUND )
GAMEX( 1993, msgundam, 0,        msgundam, msgundam, 0,        ROT0  , "Banpresto",            "Mobile Suit Gundam",  GAME_IMPERFECT_SOUND|GAME_NOT_WORKING )
GAMEX( 1993, wrofaero, 0,        wrofaero, wrofaero, wrofaero, ROT270, "Yang Cheng",           "War of Aero - Project MEIOU", GAME_IMPERFECT_SOUND )
