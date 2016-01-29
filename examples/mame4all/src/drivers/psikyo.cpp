#include "../vidhrdw/psikyo.cpp"

/***************************************************************************

							-= Psikyo Games =-

				driver by	Luca Elia (eliavit@unina.it)


CPU:	68EC020

Sound:	Z80A				+	YM2610
   Or:	LZ8420M (Z80 core)	+	YMF286-K

Chips:	PS2001B PS3103 PS3204 PS3305

---------------------------------------------------------------------------
Name					Year	Board	Notes
---------------------------------------------------------------------------
Sengoku Ace 		(J)	1993	SH201B
Gun Bird    		(J)	1994	KA302C
Strikers 1945		(J)	1995	SH404	Not Working: Protected (PIC16C57)
Sengoku Blade		(J)	1996	""		""
---------------------------------------------------------------------------

To Do:

- YMF286 emulation
- Protection in s1945, sngkblade
- Flip Screen support

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


/* Variables defined in vidhrdw */

extern unsigned char *psikyo_vram_0, *psikyo_vram_1, *psikyo_vregs;

/* Functions defined in vidhrdw */

WRITE_HANDLER( psikyo_vram_0_w );
WRITE_HANDLER( psikyo_vram_1_w );

int  psikyo_vh_start(void);
void psikyo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* Variables only used here */

static int ack_latch;


/***************************************************************************


								Main CPU


***************************************************************************/

WRITE_HANDLER( psikyo_soundlatch_w )
{
	if (Machine->sample_rate == 0)		return;

	ack_latch = 1;
	soundlatch_w(0,data);
	cpu_set_nmi_line(1,PULSE_LINE);
}


READ_HANDLER( gunbird_input_r )
{
	switch(offset)
	{
		case 0x0:	return readinputport(0);
		case 0x2:
		{
					const int bit = 0x80;
					int ret = ack_latch ? bit : 0;
					if (Machine->sample_rate == 0)	ret = 0;
					return (readinputport(1) & ~bit) | ret;
		}
		case 0x4:	return readinputport(2);
		case 0x6:	return readinputport(3);

//		case 0x8:
//		case 0xa:
		default:	//logerror("PC %06X - Read input %02X !\n", cpu_get_pc(), offset);
					return 0;
	}
}





READ_HANDLER( s1945_input_r )
{
	switch(offset)
	{
		case 0x0:	return readinputport(0);
		case 0x2:
		{
					const int bit = 0x04;
					int ret = ack_latch ? bit : 0;
					if (Machine->sample_rate == 0)	ret = 0;
					return (readinputport(1) & ~bit) | ret;
		}
		case 0x4:	return readinputport(2);
		case 0x6:	return readinputport(3);

		case 0x8:	return (rand() & 0xffff);	// protection??

//		case 0xa:
		default:	//logerror("PC %06X - Read input %02X !\n", cpu_get_pc(), offset);
					return 0;
	}
}




READ_HANDLER( sngkace_input_r )
{
	switch(offset)
	{
		case 0x0:	return readinputport(0);
		case 0x2:	return 0xffff;	// NC
		case 0x4:	return readinputport(1);
		case 0x6:	return 0xffff;	// NC
		case 0x8:
		{
					const int bit = 0x80;
					int ret = ack_latch ? bit : 0;
					if (Machine->sample_rate == 0)	ret = 0;
					return (readinputport(2) & ~bit) | ret;
		}
		case 0xa:	return readinputport(3);

		default:	//logerror("PC %06X - Read input %02X !\n", cpu_get_pc(), offset);
					return 0;
	}
}



/***************************************************************************
								Sengoku Ace
***************************************************************************/

static struct MemoryReadAddress sngkace_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM			},	// ROM (not all used)
	{ 0xfe0000, 0xffffff, MRA_BANK1			},	// RAM
	{ 0x400000, 0x4017ff, MRA_BANK2			},	// Sprites Data
	{ 0x401800, 0x401fff, MRA_BANK3			},	// Sprites List
	{ 0x600000, 0x601fff, MRA_BANK4			},	// Palette
	{ 0x800000, 0x801fff, MRA_BANK5			},	// Layer 0
	{ 0x802000, 0x803fff, MRA_BANK6			},	// Layer 1
	{ 0x804000, 0x807fff, MRA_BANK7			},	// RAM + Vregs
	{ 0xc00000, 0xc0000b, sngkace_input_r	},	// Input Ports
	{ -1 }
};
static struct MemoryWriteAddress sngkace_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM							},	// ROM (not all used)
	{ 0xfe0000, 0xffffff, MWA_BANK1							},	// RAM
	{ 0x400000, 0x4017ff, MWA_BANK2, &spriteram				},	// Sprites Data
	{ 0x401800, 0x401fff, MWA_BANK3, &spriteram_2			},	// Sprites List
	{ 0x600000, 0x601fff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram	},	// Palette
	{ 0x800000, 0x801fff, psikyo_vram_0_w, &psikyo_vram_0	},	// Layer 0
	{ 0x802000, 0x803fff, psikyo_vram_1_w, &psikyo_vram_1	},	// Layer 1
	{ 0x804000, 0x807fff, MWA_BANK7, &psikyo_vregs			},	// RAM + Vregs
	{ 0xc00010, 0xc00011, MWA_NOP							},	// To Sound CPU
	{ 0xc00012, 0xc00013, psikyo_soundlatch_w				},	//
	{ -1 }
};




/***************************************************************************


								Sound CPU


***************************************************************************/


WRITE_HANDLER( psikyo_ack_latch_w )
{
	ack_latch = 0;
}


/***************************************************************************
								Gun Bird
***************************************************************************/

WRITE_HANDLER( gunbird_sound_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);
	int bank = (data >> 4) & 3;

	/* I'm not sure here: since the banked rom is seen at 8200-ffff
	   are the first 0x200 or the last 0x200 bytes of the rom not
	   reachable ? */

//	cpu_setbank(15, &RAM[bank * 0x8000 + 0x10000 + 0x200]);
	cpu_setbank(15, &RAM[bank * 0x8000 + 0x10000]);
}

static struct MemoryReadAddress gunbird_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM		},	// ROM
	{ 0x8000, 0x81ff, MRA_RAM		},	// RAM
	{ 0x8200, 0xffff, MRA_BANK15	},	// Banked ROM
	{ -1 }
};
static struct MemoryWriteAddress gunbird_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM		},	// ROM
	{ 0x8000, 0x81ff, MWA_RAM		},	// RAM
	{ 0x8200, 0xffff, MWA_ROM		},	// Banked ROM
	{ -1 }
};


static struct IOReadPort gunbird_sound_readport[] =
{
	{ 0x04, 0x04, YM2610_status_port_0_A_r		},
	{ 0x06, 0x06, YM2610_status_port_0_B_r		},
	{ 0x08, 0x08, soundlatch_r					},
	{ -1 }
};
static struct IOWritePort gunbird_sound_writeport[] =
{
	{ 0x00, 0x00, gunbird_sound_bankswitch_w	},
	{ 0x04, 0x04, YM2610_control_port_0_A_w		},
	{ 0x05, 0x05, YM2610_data_port_0_A_w		},
	{ 0x06, 0x06, YM2610_control_port_0_B_w		},
	{ 0x07, 0x07, YM2610_data_port_0_B_w		},
	{ 0x0c, 0x0c, psikyo_ack_latch_w			},
	{ -1 }
};



/***************************************************************************
								Sengoku Ace
***************************************************************************/

WRITE_HANDLER( sngkace_sound_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);
	int bank = data & 3;
	cpu_setbank(15, &RAM[bank * 0x8000 + 0x10000]);
}

static struct MemoryReadAddress sngkace_sound_readmem[] =
{
	{ 0x0000, 0x77ff, MRA_ROM		},	// ROM
	{ 0x7800, 0x7fff, MRA_RAM		},	// RAM
	{ 0x8000, 0xffff, MRA_BANK15	},	// Banked ROM
	{ -1 }
};
static struct MemoryWriteAddress sngkace_sound_writemem[] =
{
	{ 0x0000, 0x77ff, MWA_ROM		},	// ROM
	{ 0x7800, 0x7fff, MWA_RAM		},	// RAM
	{ 0x8000, 0xffff, MWA_ROM		},	// Banked ROM
	{ -1 }
};


static struct IOReadPort sngkace_sound_readport[] =
{
	{ 0x00, 0x00, YM2610_status_port_0_A_r		},
	{ 0x02, 0x02, YM2610_status_port_0_B_r		},
	{ 0x08, 0x08, soundlatch_r					},
	{ -1 }
};
static struct IOWritePort sngkace_sound_writeport[] =
{
	{ 0x00, 0x00, YM2610_control_port_0_A_w		},
	{ 0x01, 0x01, YM2610_data_port_0_A_w		},
	{ 0x02, 0x02, YM2610_control_port_0_B_w		},
	{ 0x03, 0x03, YM2610_data_port_0_B_w		},
	{ 0x04, 0x04, sngkace_sound_bankswitch_w	},
	{ 0x0c, 0x0c, psikyo_ack_latch_w			},
	{ -1 }
};


/***************************************************************************
						Strikers 1945 / Sengoku Blade
***************************************************************************/


static struct IOReadPort s1945_sound_readport[] =
{
	{ 0x08, 0x08, YM2610_status_port_0_A_r		},
//	{ 0x06, 0x06, YM2610_status_port_0_B_r		},
	{ 0x10, 0x10, soundlatch_r					},
	{ -1 }
};
static struct IOWritePort s1945_sound_writeport[] =
{
	{ 0x00, 0x00, gunbird_sound_bankswitch_w	},
//	{ 0x02, 0x02, IOWP_NOP	},
//	{ 0x03, 0x03, IOWP_NOP	},
	{ 0x08, 0x08, YM2610_control_port_0_A_w		},
	{ 0x09, 0x09, YM2610_data_port_0_A_w		},
	{ 0x0a, 0x0a, YM2610_control_port_0_B_w		},
	{ 0x0b, 0x0b, YM2610_data_port_0_B_w		},
//	{ 0x0c, 0x0c, IOWP_NOP	},
//	{ 0x0d, 0x0d, IOWP_NOP	},
	{ 0x18, 0x18, psikyo_ack_latch_w			},
	{ -1 }
};


/***************************************************************************


								Input Ports


***************************************************************************/


/***************************************************************************
								Gun Bird
***************************************************************************/

INPUT_PORTS_START( gunbird )

	PORT_START	// IN0 - c00000&1
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_START2                       )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1                       )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	// IN1 - c00002&3
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1    )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2    )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0020, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_TILT     )
/*	PORT_BIT( 0x0080,  From Sound CPU here ) */

	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - c00004&5
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown 2-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0000, "Sound" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x000c, 0x000c, "?Difficulty?" )
	PORT_DIPSETTING(      0x0008, "0" )
	PORT_DIPSETTING(      0x000c, "1" )
	PORT_DIPSETTING(      0x0004, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0020, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0040, "400K" )
	PORT_DIPSETTING(      0x0000, "600K" )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )

	PORT_DIPNAME( 0x0100, 0x0100, "Credits/Coinage" )	// [Free Play] on all for free play
	PORT_DIPSETTING(      0x0100, "A+B/A&B" )
	PORT_DIPSETTING(      0x0000, "A&B/A [Free Play]" )
	PORT_DIPNAME( 0x0e00, 0x0e00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0a00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0e00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0600, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, "1C 6C [Free Play]" )
	PORT_DIPNAME( 0x7000, 0x7000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, "1C 6C [Free Play]" )
	PORT_DIPNAME( 0x8000, 0x8000, "Force 1C 1C" )
	PORT_DIPSETTING(      0x8000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x0000, "Yes   [Free Play]" )

	PORT_START	// IN3 - c00006&7
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )	// bits 3-0 -> !fffe0256, but unused?
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_VBLANK  )	// vblank

	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )	// tested!
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )	// tested!

INPUT_PORTS_END



/***************************************************************************
								Sengoku Ace
***************************************************************************/

INPUT_PORTS_START( sngkace )

	PORT_START	// IN0 - c00000&1
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_START2                       )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1                       )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	// IN1 - c00004&5
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown 2-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0000, "Sound" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x000c, 0x000c, "?Difficulty?" )
	PORT_DIPSETTING(      0x0008, "0" )
	PORT_DIPSETTING(      0x000c, "1" )
	PORT_DIPSETTING(      0x0004, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0020, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0040, "400K" )
	PORT_DIPSETTING(      0x0000, "600K" )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )

	PORT_DIPNAME( 0x0100, 0x0100, "Credits/Coinage" )	// [Free Play] on all for free play
	PORT_DIPSETTING(      0x0100, "A+B/A&B" )
	PORT_DIPSETTING(      0x0000, "A&B/A [Free Play]" )
	PORT_DIPNAME( 0x0e00, 0x0e00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0a00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0e00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0600, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, "1C 6C [Free Play]" )
	PORT_DIPNAME( 0x7000, 0x7000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, "1C 6C [Free Play]" )
	PORT_DIPNAME( 0x8000, 0x8000, "Force 1C 1C" )
	PORT_DIPSETTING(      0x8000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x0000, "Yes   [Free Play]" )

	PORT_START	// IN2 - c00008&9
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1    )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2    )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0020, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_TILT     )
/*	PORT_BIT( 0x0080,  From Sound CPU here ) */

	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )	// unused?
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN3
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_VBLANK  )	// vblank
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )	// unused?
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )	// unused?
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

INPUT_PORTS_END






/***************************************************************************


								Gfx Layouts


***************************************************************************/

static struct GfxLayout layout_16x16x4 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{0,1,2,3},
	{2*4,3*4,0*4,1*4,6*4,7*4,4*4,5*4,
	 10*4,11*4,8*4,9*4,14*4,15*4,12*4,13*4},
	{0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64,
	 8*64,9*64,10*64,11*64,12*64,13*64,14*64,15*64},
	16*16*4
};

/***************************************************************************
								Sengoku Ace
***************************************************************************/

static struct GfxDecodeInfo sngkace_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_16x16x4, 0x000, 0x20 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4, 0x800, 0x08 }, // [1] Layer 0
	{ REGION_GFX3, 0, &layout_16x16x4, 0xc00, 0x08 }, // [2] Layer 1
	{ -1 }
};



/***************************************************************************


								Machine Drivers


***************************************************************************/

static void sound_irq( int irq )
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}


/***************************************************************************
								Gun Bird
***************************************************************************/


struct YM2610interface gunbird_ym2610_interface =
{
	1,
	8000000,	/* ? */
	{ MIXERG(30,MIXER_GAIN_4x,MIXER_PAN_CENTER) },
	{ 0 },	/* A_r */
	{ 0 },	/* B_r */
	{ 0 },	/* A_w */
	{ 0 },	/* B_w */
	{ sound_irq },	/* irq */
	{ REGION_SOUND1 },	/* delta_t */
	{ REGION_SOUND2 },	/* adpcm */
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) }
};

static struct MachineDriver machine_driver_gunbird =
{
	{
		{
			CPU_M68EC020,
//			16000000,
32000000,	/* 16 MHz - bumped because the game slows down
			   (the 020 core timing has to be tuned up, I guess) */
			sngkace_readmem,sngkace_writemem,0,0,
			m68_level1_irq, 1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,	/* ! LZ8420M (Z80 core) ! */
			4000000,	/* ? */
			gunbird_sound_readmem,  gunbird_sound_writemem,
			gunbird_sound_readport, gunbird_sound_writeport,
			ignore_interrupt, 1		/* */
		}
	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,	// we're using IPT_VBLANK
	1,
	0,

	/* video hardware */
	320, 256, { 0, 320-1, 0, 256-32-1 },
	sngkace_gfxdecodeinfo,
	0x1000, 0x1000,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	psikyo_vh_start,
	0,
	psikyo_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,				/* ! YMF286-K ! */
			&gunbird_ym2610_interface,
		},
	}
};





/***************************************************************************
								Sengoku Ace
***************************************************************************/


struct YM2610interface sngkace_ym2610_interface =
{
	1,
	8000000,	/* ? */
	{ MIXERG(30,MIXER_GAIN_4x,MIXER_PAN_CENTER) },
	{ 0 },	/* A_r */
	{ 0 },	/* B_r */
	{ 0 },	/* A_w */
	{ 0 },	/* B_w */
	{ sound_irq },	/* irq */
	{ REGION_SOUND1 },	/* delta_t */
	{ REGION_SOUND1 },	/* adpcm */
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) }
};

static struct MachineDriver machine_driver_sngkace =
{
	{
		{
			CPU_M68EC020,
//			16000000,
32000000,	/* 16 MHz - bumped because the game slows down
			   (the 020 core timing has to be tuned up, I guess) */
			sngkace_readmem,sngkace_writemem,0,0,
			m68_level1_irq, 1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* ? */
			sngkace_sound_readmem,  sngkace_sound_writemem,
			sngkace_sound_readport, sngkace_sound_writeport,
			ignore_interrupt, 1		/* NMI caused by main CPU, IRQ by the YM2610 */
		}
	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,	// we're using IPT_VBLANK
	1,
	0,

	/* video hardware */
	320, 256, { 0, 320-1, 0, 256-32-1 },
	sngkace_gfxdecodeinfo,
	0x1000, 0x1000,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	psikyo_vh_start,
	0,
	psikyo_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&sngkace_ym2610_interface,
		},
	}
};


/***************************************************************************
						Strikers 1945 / Sengoku Blade
***************************************************************************/


struct YM2610interface s1945_ym2610_interface =
{
	1,
	8000000,	/* ? */
	{ MIXERG(30,MIXER_GAIN_4x,MIXER_PAN_CENTER) },
	{ 0 },	/* A_r */
	{ 0 },	/* B_r */
	{ 0 },	/* A_w */
	{ 0 },	/* B_w */
	{ sound_irq },	/* irq */
	{ REGION_SOUND1 },	/* delta_t */
	{ REGION_SOUND1 },	/* adpcm */
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) }
};

static struct MachineDriver machine_driver_s1945 =
{
	{
		{
			CPU_M68EC020,
			16000000,
			sngkace_readmem,sngkace_writemem,0,0,
			m68_level1_irq, 1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,	/* ! LZ8420M (Z80 core) ! */
			4000000,	/* ? */
			gunbird_sound_readmem, gunbird_sound_writemem,
			s1945_sound_readport,  s1945_sound_writeport,
			ignore_interrupt, 1		/* */
		}

		/* MCU should go here */

	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,	// we're using IPT_VBLANK
	1,
	0,

	/* video hardware */
	320, 256, { 0, 320-1, 0, 256-32-1 },
	sngkace_gfxdecodeinfo,
	0x1000, 0x1000,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	psikyo_vh_start,
	0,
	psikyo_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,				/* ! YMF286-K ! */
			&s1945_ym2610_interface,
		},
	}
};





/***************************************************************************


								ROMs Loading


***************************************************************************/


/* Untangle the 68020 code */
void init_psikyo(void)
{
	unsigned char *RAM	=	memory_region(REGION_CPU1);
	int len				=	memory_region_length(REGION_CPU1);
	int i;

	for (i = 0 ; i < len; i+=4)
	{
		unsigned char t[4];
		t[0]=RAM[i+0]; t[1]=RAM[i+1]; t[2]=RAM[i+2]; t[3]=RAM[i+3];
#ifdef LSB_FIRST
		RAM[i+0]=t[1]; RAM[i+1]=t[3]; RAM[i+2]=t[0]; RAM[i+3]=t[2];
#else
		RAM[i+0]=t[2]; RAM[i+1]=t[0]; RAM[i+2]=t[3]; RAM[i+3]=t[1];
#endif
	}
}




/***************************************************************************

								Gun Bird (Japan)

Board:	KA302C
CPU:	MC68EC020FG16
Sound:	LZ8420M (Z80 core) + YMF286-K
OSC:	16.000,	14.31818 MHz

Chips:	PS2001B
		PS3103
		PS3204
		PS3305

***************************************************************************/

ROM_START( gunbird )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* Main CPU Code */
	ROM_LOAD_EVEN( "1-u46.bin", 0x000000, 0x040000, 0x474abd69 ) // 1&0
	ROM_LOAD_ODD(  "2-u39.bin", 0x000000, 0x040000, 0x3e3e661f ) // 3&2

	ROM_REGION( 0x030000, REGION_CPU2 )		/* Sound CPU Code */
	ROM_LOAD( "3-u71.bin", 0x00000, 0x20000, 0x2168e4ba )
	ROM_RELOAD(            0x10000, 0x20000             )

	ROM_REGION( 0x700000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "u14.bin",  0x000000, 0x200000, 0x7d7e8a00 )
	ROM_LOAD( "u24.bin",  0x200000, 0x200000, 0x5e3ffc9d )
	ROM_LOAD( "u15.bin",  0x400000, 0x200000, 0xa827bfb5 )
	ROM_LOAD( "u25.bin",  0x600000, 0x100000, 0xef652e0c )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 0 */
	ROM_LOAD( "u33.bin",  0x000000, 0x200000, 0x54494e6b )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u33.bin",  0x000000, 0x100000, 0x54494e6b )
	ROM_CONTINUE(         0x000000, 0x100000             )

	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* DELTA-T Samples */
	ROM_LOAD( "u64.bin",  0x000000, 0x080000, 0xe187ed4f )

	ROM_REGION( 0x100000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )	/* ADPCM Samples */
	ROM_LOAD( "u56.bin",  0x000000, 0x100000, 0x9e07104d )

	ROM_REGION( 0x040000, REGION_USER1 )	/* Sprites LUT */
	ROM_LOAD( "u3.bin",  0x000000, 0x040000, 0x0905aeb2 )

ROM_END


void init_gunbird(void)
{
	init_psikyo();	// untangle code first!

	/* The input ports are different */
	install_mem_read_handler(0, 0x00c00000, 0x00c0000b, gunbird_input_r);
}





/***************************************************************************

								Sengoku Ace
						  (Samurai Ace JPN Ver.)

Board:	SH201B
CPU:	TMP68EC020F-16
Sound:	Z80A + YM2610
OSC:	32.000, 14.31818 MHz


fe0252.w:	country code (0 = Japan)

***************************************************************************/

ROM_START( sngkace )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* Main CPU Code */
	ROM_LOAD_EVEN( "1-u127.bin", 0x000000, 0x040000, 0x6c45b2f8 ) // 1&0
	ROM_LOAD_ODD(  "2-u126.bin", 0x000000, 0x040000, 0x845a6760 ) // 3&2

	ROM_REGION( 0x030000, REGION_CPU2 )		/* Sound CPU Code */
	ROM_LOAD( "3-u58.bin", 0x00000, 0x20000, 0x310f5c76 )
	ROM_RELOAD(            0x10000, 0x20000             )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "u14.bin",  0x000000, 0x200000, 0x00a546cb )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 0 */
	ROM_LOAD( "u34.bin",  0x000000, 0x100000, 0xe6a75bd8 )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u35.bin",  0x000000, 0x100000, 0xc4ca0164 )

//	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_REGION( 0x100000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "u68.bin",  0x000000, 0x100000, 0x9a7f6c34 )

	ROM_REGION( 0x040000, REGION_USER1 )	/* Sprites LUT */
	ROM_LOAD( "u11.bin",  0x000000, 0x040000, 0x11a04d91 ) // x1xxxxxxxxxxxxxxxx = 0xFF

ROM_END


void init_sngkace(void)
{
	unsigned char *RAM	=	memory_region(REGION_SOUND1);
	int len				=	memory_region_length(REGION_SOUND1);
	int i;

	/* Bit 6&7 of the samples are swapped. Naughty, naughty... */
	for (i=0;i<len;i++)
	{
		int x = RAM[i];
		RAM[i] = ((x & 0x40) << 1) | ((x & 0x80) >> 1) | (x & 0x3f);
	}

	init_psikyo();	// untangle code first!
}


/***************************************************************************

								Sengoku Blade
							  (Tengai JPN Ver.)

Board:	SH404
CPU:	MC68EC020FG16
Sound:	LZ8420M (Z80 core)
		YMF278B-F
OSC:	16.000MHz
		14.3181MHz
		33.8688MHz (YMF)
		4.000MHz (PIC)
Chips:	PS2001B
		PS3103
		PS3204
		PS3305

4-U59      security (PIC16C57; not dumped)

***************************************************************************/

ROM_START( sngkblad )

	ROM_REGION( 0x100000, REGION_CPU1 )		/* Main CPU Code */
	ROM_LOAD_EVEN( "2-u40.bin", 0x000000, 0x080000, 0xab6fe58a ) // 1&0
	ROM_LOAD_ODD(  "3-u41.bin", 0x000000, 0x080000, 0x02e42e39 ) // 3&2

	ROM_REGION( 0x030000, REGION_CPU2 )		/* Sound CPU Code */
	ROM_LOAD( "1-u63.bin", 0x00000, 0x20000, 0x2025e387 )
	ROM_RELOAD(            0x10000, 0x20000             )

	ROM_REGION( 0x000100, REGION_CPU3 )		/* MCU? */
	ROM_LOAD( "4-u59.bin", 0x00000, 0x00100, 0x00000000 )

	ROM_REGION( 0x600000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD_GFX_SWAP( "u20.bin",  0x000000, 0x200000, 0xed42ef73 )
	ROM_LOAD_GFX_SWAP( "u21.bin",  0x200000, 0x200000, 0xefe34eed )
	ROM_LOAD_GFX_SWAP( "u22.bin",  0x400000, 0x200000, 0x8d21caee )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 0 */
	ROM_LOAD_GFX_SWAP( "u34.bin",  0x000000, 0x400000, 0x2a2e2eeb )

	ROM_REGION( 0x200000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD_GFX_SWAP( "u34.bin",  0x000000, 0x200000, 0x2a2e2eeb )
	ROM_LOAD_GFX_SWAP( 0,          0x000000, 0x200000, 0          )	/* CONTINUE */

	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_LOAD( "u61.bin",  0x000000, 0x200000, 0xa63633c5 )	// 8 bit signed pcm (16KHz)
	ROM_LOAD( "u62.bin",  0x200000, 0x200000, 0x3ad0c357 )

	ROM_REGION( 0x040000, REGION_USER1 )	/* Sprites LUT */
	ROM_LOAD( "u1.bin",  0x000000, 0x040000, 0x681d7d55 )

ROM_END

void init_sngkblad(void)
{
	unsigned char *RAM	= memory_region(REGION_CPU1);

	init_psikyo();	// untangle code first!

	WRITE_WORD(&RAM[0x1a34c], 0x4e71);	// protection check -> NOP

	/* The input ports are different */
	install_mem_read_handler(0, 0x00c00000, 0x00c0000b, s1945_input_r);

	/* protection */
	install_mem_write_handler(0, 0x00c00006, 0x00c0000b, MWA_NOP);

}




/***************************************************************************

							Strikers 1945 (Japan)

Board:	SH404
CPU:	MC68EC020FG16
Sound:	LZ8420M (Z80 core)
		YMF278B-F
OSC:	16.000MHz
		14.3181MHz
		33.8688MHz (YMF)
		4.000MHz (PIC)

Chips:	PS2001B
		PS3103
		PS3204
		PS3305


1-U59      security (PIC16C57; not dumped)

***************************************************************************/

ROM_START( s1945 )

	ROM_REGION( 0x080000, REGION_CPU1 )		/* Main CPU Code */
	ROM_LOAD_EVEN( "1-u40.bin", 0x000000, 0x040000, 0xc00eb012 ) // 1&0
	ROM_LOAD_ODD(  "2-u41.bin", 0x000000, 0x040000, 0x3f5a134b ) // 3&2

	ROM_REGION( 0x030000, REGION_CPU2 )		/* Sound CPU Code */
	ROM_LOAD( "3-u63.bin", 0x00000, 0x20000, 0x42d40ae1 )
	ROM_RELOAD(            0x10000, 0x20000             )

	ROM_REGION( 0x000100, REGION_CPU3 )		/* MCU? */
	ROM_LOAD( "4-u59.bin", 0x00000, 0x00100, 0x00000000 )

	ROM_REGION( 0x800000, REGION_GFX1 | REGIONFLAG_DISPOSE )	/* Sprites */
	ROM_LOAD( "u20.bin",  0x000000, 0x200000, 0x28a27fee )
	ROM_LOAD( "u21.bin",  0x200000, 0x200000, 0xc5d60ea9 )
	ROM_LOAD( "u22.bin",  0x400000, 0x200000, 0xca152a32 )
	ROM_LOAD( "u23.bin",  0x600000, 0x200000, 0x48710332 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )	/* Layer 0 */
	ROM_LOAD( "u34.bin",  0x000000, 0x200000, 0xaaf83e23 )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )	/* Layer 1 */
	ROM_LOAD( "u34.bin",  0x000000, 0x100000, 0xaaf83e23 )
	ROM_CONTINUE(         0x000000, 0x100000             )

	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* Samples */
	ROM_LOAD( "u61.bin",  0x000000, 0x200000, 0xa839cf47 )	// 8 bit signed pcm (16KHz)

	ROM_REGION( 0x040000, REGION_USER1 )	/* */
	ROM_LOAD( "u1.bin",  0x000000, 0x040000, 0xdee22654 )

ROM_END


void init_s1945(void)
{
	unsigned char *RAM	= memory_region(REGION_CPU1);

	init_psikyo();	// untangle code first!

	WRITE_WORD(&RAM[0x19568], 0x4e71);	// protection check -> NOP
//	WRITE_WORD(&RAM[0x1994c], 0x4e75);	// JSR $400 -> RTS

	/* The input ports are different */
	install_mem_read_handler(0, 0x00c00000, 0x00c0000b, s1945_input_r);

	/* protection */
	install_mem_write_handler(0, 0x00c00006, 0x00c0000b, MWA_NOP);

}




/***************************************************************************


								Game Drivers


***************************************************************************/

GAMEX( 1993, sngkace,  0, sngkace,  sngkace,  sngkace,  ROT270, "Psikyo", "Sengoku Ace (Japan)",   GAME_NO_COCKTAIL ) // Banpresto?
GAMEX( 1994, gunbird,  0, gunbird,  gunbird,  gunbird,  ROT270, "Psikyo", "Gun Bird (Japan)",      GAME_NO_COCKTAIL )
GAMEX( 1995, s1945,    0, s1945,    gunbird,  s1945,    ROT270, "Psikyo", "Strikers 1945 (Japan)", GAME_NOT_WORKING )
GAMEX( 1996, sngkblad, 0, s1945,    gunbird,  sngkblad, ROT0,   "Psikyo", "Sengoku Blade (Japan)", GAME_NOT_WORKING )
