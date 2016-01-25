#include "../vidhrdw/gsword.cpp"
#include "../machine/tait8741.cpp"

/* Great Swordsman (Taito) 1984

Credits:
- Steve Ellenoff: Original emulation and Mame driver
- Jarek Parchanski: Dip Switch Fixes, Color improvements, ADPCM Interface code
- Tatsuyuki Satoh: sound improvements, NEC 8741 emulation,adpcm improvements
- Charlie Miltenberger: sprite colors improvements & precious hardware
			information and screenshots

Trick:
If you want fight with ODILION swordsman patch program for 1st CPU
at these addresses, otherwise you won't never fight with him.

		ROM[0x2256] = 0
		ROM[0x2257] = 0
		ROM[0x2258] = 0
		ROM[0x2259] = 0
		ROM[0x225A] = 0


There are 3 Z80s and two AY-3-8910s..

Prelim memory map (last updated 6/15/98)
*****************************************
GS1		z80 Main Code	(8K)	0000-1FFF
Gs2     z80 Game Data   (8K)    2000-3FFF
Gs3     z80 Game Data   (8K)    4000-5FFF
Gs4     z80 Game Data   (8K)    6000-7FFF
Gs5     z80 Game Data   (4K)    8000-8FFF
Gs6     Sprites         (8K)
Gs7     Sprites         (8K)
Gs8		Sprites			(8K)
Gs10	Tiles			(8K)
Gs11	Tiles			(8K)
Gs12    3rd z80 CPU &   (8K)
        ADPCM Samples?
Gs13    ADPCM Samples?  (8K)
Gs14    ADPCM Samples?  (8K)
Gs15    2nd z80 CPU     (8K)    0000-1FFF
Gs16    2nd z80 Data    (8K)    2000-3FFF
*****************************************

**********
*Main Z80*
**********

	9000 - 9fff	Work Ram
        982e - 982e Free play
        98e0 - 98e0 Coin Input
        98e1 - 98e1 Player 1 Controls
        98e2 - 98e2 Player 2 Controls
        9c00 - 9c30 (Hi score - Scores)
        9c78 - 9cd8 (Hi score - Names)
        9e00 - 9e7f Sprites in working ram!
        9e80 - 9eff Sprite X & Y in working ram!

	a000 - afff	Sprite RAM & Video Attributes
        a000 - a37F	???
        a380 - a77F	Sprite Tile #s
        a780 - a7FF	Sprite Y & X positions
        a980 - a980	Background Tile Bank Select
        ab00 - ab00	Background Tile Y-Scroll register
        ab80 - abff	Sprite Attributes(X & Y Flip)

	b000 - b7ff	Screen RAM
	b800 - ffff	not used?!

PORTS:
7e 8741-#0 data port
7f 8741-#1 command / status port

*************
*2nd Z80 CPU*
*************
0000 - 3FFF ROM CODE
4000 - 43FF WORK RAM

write
6000 adpcm sound command for 3rd CPU

PORTS:
00 8741-#2 data port
01 8741-#2 command / status port
20 8741-#3 data port
21 8741-#3 command / status port
40 8741-#1 data port
41 8741-#1 command / status port

read:
60 fake port #0 ?
61 ay8910-#0 read port
data / ay8910-#0 read
80 fake port #1 ?
81 ay8910-#1 read port

write:
60 ay8910-#0 controll port
61 ay8910-#0 data port
80 ay8910-#1 controll port
81 ay8910-#1 data port
   ay8910-A  : NMI controll ?
a0 unknown
e0 unknown (watch dog?)

*************
*3rd Z80 CPU*
*************
0000-5fff ROM

read:
a000 adpcm sound command

write:
6000 MSM5205 reset and data

*************
I8741 communication data

reg: 0->1 (main->2nd) /     : (1->0) 2nd->main :
 0 : DSW.2 (port)           : DSW.1(port)
 1 : DSW.1                  : DSW.2
 2 : IN0 / sound error code :
 3 : IN1 / ?                :
 4 : IN2                    :
 4 : IN3                    :
 5 :                        :
 6 :                        : DSW0?
 7 :                        : ?

******************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "machine/tait8741.h"

void gsword_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  gsword_vh_start(void);
void gsword_vh_stop(void);
void gsword_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( gs_charbank_w );
WRITE_HANDLER( gs_videoctrl_w );
WRITE_HANDLER( gs_videoram_w );


extern size_t gs_videoram_size;
extern size_t gs_spritexy_size;

extern unsigned char *gs_videoram;
extern unsigned char *gs_scrolly_ram;
extern unsigned char *gs_spritexy_ram;
extern unsigned char *gs_spritetile_ram;
extern unsigned char *gs_spriteattrib_ram;

static int coins;
static int fake8910_0,fake8910_1;
static int gsword_nmi_step,gsword_nmi_count;

static READ_HANDLER( gsword_8741_2_r )
{
	switch (offset)
	{
	case 0x01: /* start button , coins */
		return readinputport(0);
	case 0x02: /* Player 1 Controller */
		return readinputport(1);
	case 0x04: /* Player 2 Controller */
		return readinputport(3);
	default:
		//logerror("8741-2 unknown read %d PC=%04x\n",offset,cpu_get_pc());
		break;
	}
	/* unknown */
	return 0;
}

static READ_HANDLER( gsword_8741_3_r )
{
	switch (offset)
	{
	case 0x01: /* start button  */
		return readinputport(2);
	case 0x02: /* Player 1 Controller? */
		return readinputport(1);
	case 0x04: /* Player 2 Controller? */
		return readinputport(3);
	}
	/* unknown */
	//logerror("8741-3 unknown read %d PC=%04x\n",offset,cpu_get_pc());
	return 0;
}

static struct TAITO8741interface gsword_8741interface=
{
	4,         /* 4 chips */
	{ TAITO8741_MASTER,TAITO8741_SLAVE,TAITO8741_PORT,TAITO8741_PORT },  /* program mode */
	{ 1,0,0,0 },							     /* serial port connection */
	{ input_port_7_r,input_port_6_r,gsword_8741_2_r,gsword_8741_3_r }    /* port handler */
};

void machine_init(void)
{
	unsigned char *ROM2 = memory_region(REGION_CPU2);

	ROM2[0x1da] = 0xc3; /* patch for rom self check */
	ROM2[0x726] = 0;    /* patch for sound protection or time out function */
	ROM2[0x727] = 0;

	TAITO8741_start(&gsword_8741interface);
}

void init_gsword(void)
{
	int i;

	for(i=0;i<4;i++) TAITO8741_reset(i);
	coins = 0;
	gsword_nmi_count = 0;
	gsword_nmi_step  = 0;
}

static int gsword_snd_interrupt(void)
{
	if( (gsword_nmi_count+=gsword_nmi_step) >= 4)
	{
		gsword_nmi_count = 0;
		return Z80_NMI_INT;
	}
	return Z80_IGNORE_INT;
}

static WRITE_HANDLER( gsword_nmi_set_w )
{
	switch(data)
	{
	case 0x02:
		/* needed to disable NMI for memory check */
		gsword_nmi_step  = 0;
		gsword_nmi_count = 0;
		break;
	case 0x0d:
	case 0x0f:
		gsword_nmi_step  = 4;
		break;
	case 0xfe:
	case 0xff:
		gsword_nmi_step  = 4;
		break;
	}
	/* bit1= nmi disable , for ram check */
	//logerror("NMI controll %02x\n",data);
}

static WRITE_HANDLER( gsword_AY8910_control_port_0_w )
{
	AY8910_control_port_0_w(offset,data);
	fake8910_0 = data;
}
static WRITE_HANDLER( gsword_AY8910_control_port_1_w )
{
	AY8910_control_port_1_w(offset,data);
	fake8910_1 = data;
}

static READ_HANDLER( gsword_fake_0_r )
{
	return fake8910_0+1;
}
static READ_HANDLER( gsword_fake_1_r )
{
	return fake8910_1+1;
}

WRITE_HANDLER( gsword_adpcm_data_w )
{
	MSM5205_data_w (0,data & 0x0f); /* bit 0..3 */
	MSM5205_reset_w(0,(data>>5)&1); /* bit 5    */
	MSM5205_vclk_w(0,(data>>4)&1);  /* bit 4    */
}

WRITE_HANDLER( adpcm_soundcommand_w )
{
	soundlatch_w(0,data);
	cpu_set_nmi_line(2, PULSE_LINE);
}

static struct MemoryReadAddress gsword_readmem[] =
{
	{ 0x0000, 0x8fff, MRA_ROM },
	{ 0x9000, 0x9fff, MRA_RAM },
	{ 0xb000, 0xb7ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress gsword_writemem[] =
{
	{ 0x0000, 0x8fff, MWA_ROM },
	{ 0x9000, 0x9fff, MWA_RAM },
	{ 0xa380, 0xa3ff, MWA_RAM, &gs_spritetile_ram },
	{ 0xa780, 0xa7ff, MWA_RAM, &gs_spritexy_ram, &gs_spritexy_size },
	{ 0xa980, 0xa980, gs_charbank_w },
	{ 0xaa80, 0xaa80, gs_videoctrl_w },	/* flip screen, char palette bank */
	{ 0xab00, 0xab00, MWA_RAM, &gs_scrolly_ram },
	{ 0xab80, 0xabff, MWA_RAM, &gs_spriteattrib_ram },
	{ 0xb000, 0xb7ff, gs_videoram_w, &gs_videoram, &gs_videoram_size },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem_cpu2[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_cpu2[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ 0x6000, 0x6000, adpcm_soundcommand_w },
	{ -1 }
};

static struct MemoryReadAddress readmem_cpu3[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0xa000, 0xa000, soundlatch_r },
	{ -1 }
};

static struct MemoryWriteAddress writemem_cpu3[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x8000, 0x8000, gsword_adpcm_data_w },
	{ -1 }
};

static struct IOReadPort readport[] =
{
	{ 0x7e, 0x7f, TAITO8741_0_r },
	{ -1 }
};

static struct IOWritePort writeport[] =
{
	{ 0x7e, 0x7f, TAITO8741_0_w },
	{ -1 }
};

static struct IOReadPort readport_cpu2[] =
{
	{ 0x00, 0x01, TAITO8741_2_r },
	{ 0x20, 0x21, TAITO8741_3_r },
	{ 0x40, 0x41, TAITO8741_1_r },
	{ 0x60, 0x60, gsword_fake_0_r },
	{ 0x61, 0x61, AY8910_read_port_0_r },
	{ 0x80, 0x80, gsword_fake_1_r },
	{ 0x81, 0x81, AY8910_read_port_1_r },
	{ 0xe0, 0xe0, IORP_NOP }, /* ?? */
	{ -1 }
};

static struct IOWritePort writeport_cpu2[] =
{
	{ 0x00, 0x01, TAITO8741_2_w },
	{ 0x20, 0x21, TAITO8741_3_w },
	{ 0x40, 0x41, TAITO8741_1_w },
	{ 0x60, 0x60, gsword_AY8910_control_port_0_w },
	{ 0x61, 0x61, AY8910_write_port_0_w },
	{ 0x80, 0x80, gsword_AY8910_control_port_1_w },
	{ 0x81, 0x81, AY8910_write_port_1_w },
	{ 0xa0, 0xa0, IOWP_NOP }, /* ?? */
	{ 0xe0, 0xe0, IOWP_NOP }, /* watch dog ?*/
	{ -1 }
};

INPUT_PORTS_START( gsword )
	PORT_START	/* IN0 (8741-2 port1?) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_START	/* IN1 (8741-2 port2?) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_START	/* IN2 (8741-3 port1?) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_START	/* IN3  (8741-3 port2?) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_START	/* IN4 (coins) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN1, 1 )

	PORT_START	/* DSW0 */
	/* NOTE: Switches 0 & 1, 6,7,8 not used 	 */
	/*	 Coins configurations were handled 	 */
	/*	 via external hardware & not via program */
	PORT_DIPNAME( 0x1c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_5C ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x04, "Stage 1 Difficulty" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x0c, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Stage 2 Difficulty" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPNAME( 0x20, 0x20, "Stage 3 Difficulty" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Free Game Round" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x30, 0x00, "Stage Begins" )
	PORT_DIPSETTING(    0x00, "Fencing" )
	PORT_DIPSETTING(    0x10, "Kendo" )
	PORT_DIPSETTING(    0x20, "Rome" )
	PORT_DIPSETTING(    0x30, "Kendo" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

static struct GfxLayout gsword_text =
{
	8,8,    /* 8x8 characters */
	1024,	/* 1024 characters */
	2,      /* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 bytes */
};

static struct GfxLayout gsword_sprites1 =
{
	16,16,   /* 16x16 sprites */
	64*2,    /* 128 sprites */
	2,       /* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8     /* every sprite takes 64 bytes */
};

static struct GfxLayout gsword_sprites2 =
{
	32,32,    /* 32x32 sprites */
	64,       /* 64 sprites */
	2,       /* 2 bits per pixel */
	{ 0, 4 }, /* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3,
			64*8+0, 64*8+1, 64*8+2, 64*8+3, 72*8+0, 72*8+1, 72*8+2, 72*8+3,
			80*8+0, 80*8+1, 80*8+2, 80*8+3, 88*8+0, 88*8+1, 88*8+2, 88*8+3},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8,
			128*8, 129*8, 130*8, 131*8, 132*8, 133*8, 134*8, 135*8,
			160*8, 161*8, 162*8, 163*8, 164*8, 165*8, 166*8, 167*8 },
	64*8*4    /* every sprite takes (64*8=16x6)*4) bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &gsword_text,         0, 64 },
	{ REGION_GFX2, 0, &gsword_sprites1,  64*4, 64 },
	{ REGION_GFX3, 0, &gsword_sprites2,  64*4, 64 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,		/* 2 chips */
	1500000,	/* 1.5 MHZ */
	{ 30, 30 },
	{ 0,0 },
	{ 0,0 },
	{ 0,gsword_nmi_set_w }, /* portA write */
	{ 0,0 }
};

static struct MSM5205interface msm5205_interface =
{
	1,				/* 1 chip             */
	384000,				/* 384KHz verified!   */
	{ 0 },				/* interrupt function */
	{ MSM5205_SEX_4B },		/* vclk input mode    */
	{ 60 }
};



static struct MachineDriver machine_driver_gsword =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3000000,
			gsword_readmem,gsword_writemem,
			readport,writeport,
			interrupt,1
		},
		{
			CPU_Z80,
			3000000,
			readmem_cpu2,writemem_cpu2,
			readport_cpu2,writeport_cpu2,
			gsword_snd_interrupt,4
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,
			readmem_cpu3,writemem_cpu3,
			0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	200,                                 /* Allow time for 2nd cpu to interleave*/
	machine_init,

	/* video hardware */
	32*8, 32*8,{ 0*8, 32*8-1, 2*8, 30*8-1 },

	gfxdecodeinfo,
	256, 64*4+64*4,
	gsword_vh_convert_color_prom,
	VIDEO_TYPE_RASTER,
	0,
	gsword_vh_start,
	gsword_vh_stop,
	gsword_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}

};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( gsword )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64K for main CPU */
	ROM_LOAD( "gs1",          0x0000, 0x2000, 0x565c4d9e )
	ROM_LOAD( "gs2",          0x2000, 0x2000, 0xd772accf )
	ROM_LOAD( "gs3",          0x4000, 0x2000, 0x2cee1871 )
	ROM_LOAD( "gs4",          0x6000, 0x2000, 0xca9d206d )
	ROM_LOAD( "gs5",          0x8000, 0x1000, 0x2a892326 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for 2nd CPU */
	ROM_LOAD( "gs15",         0x0000, 0x2000, 0x1aa4690e )
	ROM_LOAD( "gs16",         0x2000, 0x2000, 0x10accc10 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64K for 3nd z80 */
	ROM_LOAD( "gs12",         0x0000, 0x2000, 0xa6589068 )
	ROM_LOAD( "gs13",         0x2000, 0x2000, 0x4ee79796 )
	ROM_LOAD( "gs14",         0x4000, 0x2000, 0x455364b6 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gs10",         0x0000, 0x2000, 0x517c571b )	/* tiles */
	ROM_LOAD( "gs11",         0x2000, 0x2000, 0x7a1d8a3a )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gs6",          0x0000, 0x2000, 0x1b0a3cb7 )	/* sprites */

	ROM_REGION( 0x4000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gs7",          0x0000, 0x2000, 0xef5f28c6 )
	ROM_LOAD( "gs8",          0x2000, 0x2000, 0x46824b30 )

	ROM_REGION( 0x0360, REGION_PROMS )
	ROM_LOAD( "ac0-1.bpr",    0x0000, 0x0100, 0x5c4b2adc )	/* palette low bits */
	ROM_LOAD( "ac0-2.bpr",    0x0100, 0x0100, 0x966bda66 )	/* palette high bits */
	ROM_LOAD( "ac0-3.bpr",    0x0200, 0x0100, 0xdae13f77 )	/* sprite lookup table */
	ROM_LOAD( "003",          0x0300, 0x0020, 0x43a548b8 )	/* address decoder? not used */
	ROM_LOAD( "004",          0x0320, 0x0020, 0x43a548b8 )	/* address decoder? not used */
	ROM_LOAD( "005",          0x0340, 0x0020, 0xe8d6dec0 )	/* address decoder? not used */
ROM_END



GAMEX( 1984, gsword, 0, gsword, gsword, gsword, ROT0, "Taito Corporation", "Great Swordsman", GAME_IMPERFECT_COLORS )
