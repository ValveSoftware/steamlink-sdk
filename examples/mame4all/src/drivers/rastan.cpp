#include "../vidhrdw/rastan.cpp"
#include "../sndhrdw/rastan.cpp"

/***************************************************************************

Rastan

driver by Jarek Burczynski

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *rastan_ram;
extern unsigned char *rastan_videoram1,*rastan_videoram3;
extern size_t rastan_videoram_size;
extern unsigned char *rastan_spriteram;
extern unsigned char *rastan_scrollx;
extern unsigned char *rastan_scrolly;

WRITE_HANDLER( rastan_spriteram_w );
READ_HANDLER( rastan_spriteram_r );
WRITE_HANDLER( rastan_videoram1_w );
READ_HANDLER( rastan_videoram1_r );
WRITE_HANDLER( rastan_videoram3_w );
READ_HANDLER( rastan_videoram3_r );

WRITE_HANDLER( rastan_scrollY_w );
WRITE_HANDLER( rastan_scrollX_w );

WRITE_HANDLER( rastan_videocontrol_w );
WRITE_HANDLER( rastan_flipscreen_w );

int rastan_interrupt(void);
int rastan_s_interrupt(void);

WRITE_HANDLER( rastan_background_w );
void rastan_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

int  rastan_vh_start(void);
void rastan_vh_stop(void);

WRITE_HANDLER( rastan_sound_w );
READ_HANDLER( rastan_sound_r );



READ_HANDLER( rastan_a001_r );
WRITE_HANDLER( rastan_a000_w );
WRITE_HANDLER( rastan_a001_w );
WRITE_HANDLER( rastan_adpcm_trigger_w );

WRITE_HANDLER( rastan_c000_w );
WRITE_HANDLER( rastan_d000_w );

void rastan_irq_handler(int irq);

int rastan_interrupt(void)
{
	return 5;  /*Interrupt vector 5*/
}

static READ_HANDLER( rastan_input_r )
{
	switch (offset)
	{
		case 0:
			return input_port_0_r (offset);
		case 2:
			return input_port_1_r (offset);
		case 6:
			return input_port_2_r (offset);
		case 8:
			return input_port_3_r (offset);
		case 10:
			return input_port_4_r (offset);
		default:
			return 0;
	}
}

static READ_HANDLER( rastan_cycle_r )
{
	if (cpu_get_pc()==0x3b088) cpu_spinuntil_int();

	return READ_WORD(&rastan_ram[0x1c10]);
}


static struct MemoryReadAddress rastan_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
//	{ 0x10dc10, 0x10dc13, rastan_speedup_r },
{ 0x10dc10, 0x10dc11, rastan_cycle_r },
	{ 0x10c000, 0x10ffff, MRA_BANK1 },	/* RAM */
	{ 0x200000, 0x20ffff, paletteram_word_r },
	{ 0x3e0000, 0x3e0003, rastan_sound_r },
	{ 0x390000, 0x39000f, rastan_input_r },
	{ 0xc00000, 0xc03fff, rastan_videoram1_r },
	{ 0xc04000, 0xc07fff, MRA_BANK2 },
	{ 0xc08000, 0xc0bfff, rastan_videoram3_r },
	{ 0xc0c000, 0xc0ffff, MRA_BANK3 },
	{ 0xd00000, 0xd0ffff, MRA_BANK4 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress rastan_writemem[] =
{
	{ 0x000000, 0x05ffff, MWA_ROM },
//	{ 0x10dc10, 0x10dc13, rastan_speedup_w },
	{ 0x10c000, 0x10ffff, MWA_BANK1, &rastan_ram },
	{ 0x200000, 0x20ffff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x350008, 0x35000b, MWA_NOP },     /* 0 only (often) ? */
	{ 0x380000, 0x380003, rastan_videocontrol_w },	/* sprite palette bank, coin counters, other unknowns */
	{ 0x3c0000, 0x3c0003, MWA_NOP },     /*0000,0020,0063,0992,1753 (very often) watchdog? */
	{ 0x3e0000, 0x3e0003, rastan_sound_w },
	{ 0xc00000, 0xc03fff, rastan_videoram1_w, &rastan_videoram1, &rastan_videoram_size },
	{ 0xc04000, 0xc07fff, MWA_BANK2 },
	{ 0xc08000, 0xc0bfff, rastan_videoram3_w, &rastan_videoram3 },
	{ 0xc0c000, 0xc0ffff, MWA_BANK3 },
	{ 0xc20000, 0xc20003, rastan_scrollY_w, &rastan_scrolly },  /* scroll Y  1st.w plane1  2nd.w plane2 */
	{ 0xc40000, 0xc40003, rastan_scrollX_w, &rastan_scrollx },  /* scroll X  1st.w plane1  2nd.w plane2 */
	{ 0xc50000, 0xc50003, rastan_flipscreen_w },     /* bit 0  flipscreen*/
	{ 0xd00000, 0xd0ffff, MWA_BANK4, &rastan_spriteram },
	{ -1 }  /* end of table */
};


static WRITE_HANDLER( rastan_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU2);

	bankaddress = 0x10000 + ((data^1) & 0x01) * 0x4000;
	cpu_setbank(5,&RAM[bankaddress]);
}

static struct MemoryReadAddress rastan_s_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x7fff, MRA_BANK5 },
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9001, 0x9001, YM2151_status_port_0_r },
	{ 0x9002, 0x9100, MRA_RAM },
	{ 0xa001, 0xa001, rastan_a001_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress rastan_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },
	{ 0x9000, 0x9000, YM2151_register_port_0_w },
	{ 0x9001, 0x9001, YM2151_data_port_0_w },
	{ 0xa000, 0xa000, rastan_a000_w },
	{ 0xa001, 0xa001, rastan_a001_w },
	{ 0xb000, 0xb000, rastan_adpcm_trigger_w },
	{ 0xc000, 0xc000, rastan_c000_w },
	{ 0xd000, 0xd000, rastan_d000_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( rastan )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "100000" )
	PORT_DIPSETTING(    0x08, "150000" )
	PORT_DIPSETTING(    0x04, "200000" )
	PORT_DIPSETTING(    0x00, "250000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* same as rastan, coinage is different */
INPUT_PORTS_START( rastsaga )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "100000" )
	PORT_DIPSETTING(    0x08, "150000" )
	PORT_DIPSETTING(    0x04, "200000" )
	PORT_DIPSETTING(    0x00, "250000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout spritelayout1 =
{
	8,8,	/* 8*8 sprites */
	0x4000,	/* 16384 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0, 4, 0x40000*8+0 ,0x40000*8+4, 8+0, 8+4, 0x40000*8+8+0, 0x40000*8+8+4 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every sprite takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout2 =
{
	16,16,	/* 16*16 sprites */
	4096,	/* 4096 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{
	0, 4, 0x40000*8+0 ,0x40000*8+4,
	8+0, 8+4, 0x40000*8+8+0, 0x40000*8+8+4,
	16+0, 16+4, 0x40000*8+16+0, 0x40000*8+16+4,
	24+0, 24+4, 0x40000*8+24+0, 0x40000*8+24+4
	},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout1,  0, 0x80 },	/* sprites 8x8*/
	{ REGION_GFX2, 0, &spritelayout2,  0, 0x80 },	/* sprites 16x16*/
	{ -1 } /* end of array */
};



static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	4000000,	/* 4 MHz ? */
	{ YM3012_VOL(50,MIXER_PAN_CENTER,50,MIXER_PAN_CENTER) },
	{ rastan_irq_handler },
	{ rastan_bankswitch_w }
};


struct ADPCMsample rastan_samples[] =
{
	{ 0x00, 0x0000, 0x0200*2 },
	{ 0x02, 0x0200, 0x0500*2 },
	{ 0x07, 0x0700, 0x2100*2 },
	{ 0x28, 0x2800, 0x3b00*2 },
	{ 0x63, 0x6300, 0x4e00*2 },
	{ 0xb1, 0xb100, 0x1600*2 }
};

static void adpcm_init(const struct ADPCMinterface *adpcm_intf, struct ADPCMsample *sample_list, int max)
{
	memcpy(sample_list,rastan_samples,sizeof(rastan_samples));
}

static struct ADPCMinterface adpcm_interface =
{
	1,			/* 1 chip */
	8000,       /* 8000Hz playback */
	REGION_SOUND1,	/* memory region */
	adpcm_init,	/* init function */
	{ 60 }
};



static struct MachineDriver machine_driver_rastan =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,	/* 8 Mhz */
			rastan_readmem,rastan_writemem,0,0,
			rastan_interrupt,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 Mhz */
			rastan_s_readmem,rastan_s_writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,	/* 10 CPU slices per frame - enough for the sound CPU to read all commands */
	0,

	/* video hardware */
	40*8, 32*8, { 0*8, 40*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	rastan_vh_start,
	rastan_vh_stop,
	rastan_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_ADPCM,
			&adpcm_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( rastan )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "ic19_38.bin", 0x00000, 0x10000, 0x1c91dbb1 )
	ROM_LOAD_ODD ( "ic07_37.bin", 0x00000, 0x10000, 0xecf20bdd )
	ROM_LOAD_EVEN( "ic20_40.bin", 0x20000, 0x10000, 0x0930d4b3 )
	ROM_LOAD_ODD ( "ic08_39.bin", 0x20000, 0x10000, 0xd95ade5e )
	ROM_LOAD_EVEN( "ic21_42.bin", 0x40000, 0x10000, 0x1857a7cb )
	ROM_LOAD_ODD ( "ic09_43.bin", 0x40000, 0x10000, 0xc34b9152 )

	ROM_REGION( 0x1c000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ic49_19.bin", 0x00000, 0x4000, 0xee81fdd8 )
	ROM_CONTINUE(            0x10000, 0xc000 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic40_01.bin",  0x00000, 0x20000, 0xcd30de19 )        /* 8x8 0 */
	ROM_LOAD( "ic39_03.bin",  0x20000, 0x20000, 0xab67e064 )        /* 8x8 0 */
	ROM_LOAD( "ic67_02.bin",  0x40000, 0x20000, 0x54040fec )        /* 8x8 1 */
	ROM_LOAD( "ic66_04.bin",  0x60000, 0x20000, 0x94737e93 )        /* 8x8 1 */

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic15_05.bin",  0x00000, 0x20000, 0xc22d94ac )        /* sprites 1a */
	ROM_LOAD( "ic14_07.bin",  0x20000, 0x20000, 0xb5632a51 )        /* sprites 3a */
	ROM_LOAD( "ic28_06.bin",  0x40000, 0x20000, 0x002ccf39 )        /* sprites 1b */
	ROM_LOAD( "ic27_08.bin",  0x60000, 0x20000, 0xfeafca05 )        /* sprites 3b */

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* 64k for the samples */
	ROM_LOAD( "ic76_20.bin", 0x0000, 0x10000, 0xfd1a34cc ) /* samples are 4bit ADPCM */
ROM_END

ROM_START( rastanu )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "ic19_38.bin", 0x00000, 0x10000, 0x1c91dbb1 )
	ROM_LOAD_ODD ( "ic07_37.bin", 0x00000, 0x10000, 0xecf20bdd )
	ROM_LOAD_EVEN( "b04-45.20",   0x20000, 0x10000, 0x362812dd )
	ROM_LOAD_ODD ( "b04-44.8",    0x20000, 0x10000, 0x51cc5508 )
	ROM_LOAD_EVEN( "ic21_42.bin", 0x40000, 0x10000, 0x1857a7cb )
	ROM_LOAD_ODD ( "b04-41-1.9",  0x40000, 0x10000, 0xbd403269 )

	ROM_REGION( 0x1c000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ic49_19.bin", 0x00000, 0x4000, 0xee81fdd8 )
	ROM_CONTINUE(            0x10000, 0xc000 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic40_01.bin",  0x00000, 0x20000, 0xcd30de19 )        /* 8x8 0 */
	ROM_LOAD( "ic39_03.bin",  0x20000, 0x20000, 0xab67e064 )        /* 8x8 0 */
	ROM_LOAD( "ic67_02.bin",  0x40000, 0x20000, 0x54040fec )        /* 8x8 1 */
	ROM_LOAD( "ic66_04.bin",  0x60000, 0x20000, 0x94737e93 )        /* 8x8 1 */

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic15_05.bin",  0x00000, 0x20000, 0xc22d94ac )        /* sprites 1a */
	ROM_LOAD( "ic14_07.bin",  0x20000, 0x20000, 0xb5632a51 )        /* sprites 3a */
	ROM_LOAD( "ic28_06.bin",  0x40000, 0x20000, 0x002ccf39 )        /* sprites 1b */
	ROM_LOAD( "ic27_08.bin",  0x60000, 0x20000, 0xfeafca05 )        /* sprites 3b */

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* 64k for the samples */
	ROM_LOAD( "ic76_20.bin", 0x0000, 0x10000, 0xfd1a34cc ) /* samples are 4bit ADPCM */
ROM_END

ROM_START( rastanu2 )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "rs19_38.bin", 0x00000, 0x10000, 0xa38ac909 )
	ROM_LOAD_ODD ( "b04-21.7",    0x00000, 0x10000, 0x7c8dde9a )
	ROM_LOAD_EVEN( "b04-23.20",   0x20000, 0x10000, 0x254b3dce )
	ROM_LOAD_ODD ( "b04-22.8",    0x20000, 0x10000, 0x98e8edcf )
	ROM_LOAD_EVEN( "b04-25.21",   0x40000, 0x10000, 0xd1e5adee )
	ROM_LOAD_ODD ( "b04-24.9",    0x40000, 0x10000, 0xa3dcc106 )

	ROM_REGION( 0x1c000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ic49_19.bin", 0x00000, 0x4000, 0xee81fdd8 )
	ROM_CONTINUE(            0x10000, 0xc000 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic40_01.bin",  0x00000, 0x20000, 0xcd30de19 )        /* 8x8 0 */
	ROM_LOAD( "ic39_03.bin",  0x20000, 0x20000, 0xab67e064 )        /* 8x8 0 */
	ROM_LOAD( "ic67_02.bin",  0x40000, 0x20000, 0x54040fec )        /* 8x8 1 */
	ROM_LOAD( "ic66_04.bin",  0x60000, 0x20000, 0x94737e93 )        /* 8x8 1 */

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic15_05.bin",  0x00000, 0x20000, 0xc22d94ac )        /* sprites 1a */
	ROM_LOAD( "ic14_07.bin",  0x20000, 0x20000, 0xb5632a51 )        /* sprites 3a */
	ROM_LOAD( "ic28_06.bin",  0x40000, 0x20000, 0x002ccf39 )        /* sprites 1b */
	ROM_LOAD( "ic27_08.bin",  0x60000, 0x20000, 0xfeafca05 )        /* sprites 3b */

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* 64k for the samples */
	ROM_LOAD( "ic76_20.bin", 0x0000, 0x10000, 0xfd1a34cc ) /* samples are 4bit ADPCM */
ROM_END

ROM_START( rastsaga )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "rs19_38.bin", 0x00000, 0x10000, 0xa38ac909 )
	ROM_LOAD_ODD ( "rs07_37.bin", 0x00000, 0x10000, 0xbad60872 )
	ROM_LOAD_EVEN( "rs20_40.bin", 0x20000, 0x10000, 0x6bcf70dc )
	ROM_LOAD_ODD ( "rs08_39.bin", 0x20000, 0x10000, 0x8838ecc5 )
	ROM_LOAD_EVEN( "rs21_42.bin", 0x40000, 0x10000, 0xb626c439 )
	ROM_LOAD_ODD ( "rs09_43.bin", 0x40000, 0x10000, 0xc928a516 )

	ROM_REGION( 0x1c000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ic49_19.bin", 0x00000, 0x4000, 0xee81fdd8 )
	ROM_CONTINUE(            0x10000, 0xc000 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic40_01.bin",  0x00000, 0x20000, 0xcd30de19 )        /* 8x8 0 */
	ROM_LOAD( "ic39_03.bin",  0x20000, 0x20000, 0xab67e064 )        /* 8x8 0 */
	ROM_LOAD( "ic67_02.bin",  0x40000, 0x20000, 0x54040fec )        /* 8x8 1 */
	ROM_LOAD( "ic66_04.bin",  0x60000, 0x20000, 0x94737e93 )        /* 8x8 1 */

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic15_05.bin",  0x00000, 0x20000, 0xc22d94ac )        /* sprites 1a */
	ROM_LOAD( "ic14_07.bin",  0x20000, 0x20000, 0xb5632a51 )        /* sprites 3a */
	ROM_LOAD( "ic28_06.bin",  0x40000, 0x20000, 0x002ccf39 )        /* sprites 1b */
	ROM_LOAD( "ic27_08.bin",  0x60000, 0x20000, 0xfeafca05 )        /* sprites 3b */

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* 64k for the samples */
	ROM_LOAD( "ic76_20.bin", 0x0000, 0x10000, 0xfd1a34cc ) /* samples are 4bit ADPCM */
ROM_END



GAME( 1987, rastan,   0,      rastan, rastan,   0, ROT0, "Taito Corporation Japan", "Rastan (World)")
/* IDENTICAL to rastan, only difference is copyright notice and Coin B coinage */
GAME( 1987, rastanu,  rastan, rastan, rastsaga, 0, ROT0, "Taito America Corporation", "Rastan (US set 1)")
GAME( 1987, rastanu2, rastan, rastan, rastsaga, 0, ROT0, "Taito America Corporation", "Rastan (US set 2)")
GAME( 1987, rastsaga, rastan, rastan, rastsaga, 0, ROT0, "Taito Corporation", "Rastan Saga (Japan)")
