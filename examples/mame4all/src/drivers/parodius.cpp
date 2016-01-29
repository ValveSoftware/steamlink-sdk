#include "../vidhrdw/parodius.cpp"

/***************************************************************************

Parodius (Konami GX955) (c) 1990 Konami

driver by Nicola Salmoria

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/konami/konami.h" /* for the callback and the firq irq definition */
#include "vidhrdw/konamiic.h"

/* prototypes */
static void parodius_init_machine( void );
static void parodius_banking( int lines );
int parodius_vh_start( void );
void parodius_vh_stop( void );
void parodius_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static int videobank;
static unsigned char *ram;

static int parodius_interrupt(void)
{
	if (K052109_is_IRQ_enabled()) return interrupt();
	else return ignore_interrupt();
}

static READ_HANDLER( bankedram_r )
{
	if (videobank & 0x01)
	{
		if (videobank & 0x04)
			return paletteram_r(offset + 0x0800);
		else
			return paletteram_r(offset);
	}
	else
		return ram[offset];
}

static WRITE_HANDLER( bankedram_w )
{
	if (videobank & 0x01)
	{
		if (videobank & 0x04)
			paletteram_xBBBBBGGGGGRRRRR_swap_w(offset + 0x0800,data);
		else
			paletteram_xBBBBBGGGGGRRRRR_swap_w(offset,data);
	}
	else
		ram[offset] = data;
}

static READ_HANDLER( parodius_052109_053245_r )
{
	if (videobank & 0x02)
		return K053245_r(offset);
	else
		return K052109_r(offset);
}

static WRITE_HANDLER( parodius_052109_053245_w )
{
	if (videobank & 0x02)
		K053245_w(offset,data);
	else
		K052109_w(offset,data);
}

static WRITE_HANDLER( parodius_videobank_w )
{
	//if (videobank & 0xf8) logerror("%04x: videobank = %02x\n",cpu_get_pc(),data);

	/* bit 0 = select palette or work RAM at 0000-07ff */
	/* bit 1 = select 052109 or 053245 at 2000-27ff */
	/* bit 2 = select palette bank 0 or 1 */
	videobank = data;
}

static WRITE_HANDLER( parodius_3fc0_w )
{
	//if ((data & 0xf4) != 0x10) logerror("%04x: 3fc0 = %02x\n",cpu_get_pc(),data);

	/* bit 0/1 = coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* bit 3 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line( ( data & 0x08 ) ? ASSERT_LINE : CLEAR_LINE );

	/* other bits unknown */
}

static READ_HANDLER( parodius_sound_r )
{
	/* If the sound CPU is running, read the status, otherwise
	   just make it pass the test */
	if (Machine->sample_rate != 0) 	return K053260_r(2 + offset);
	else return offset ? 0x00 : 0x80;
}

static WRITE_HANDLER( parodius_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,0xff);
}

static void nmi_callback(int param)
{
	cpu_set_nmi_line(1,ASSERT_LINE);
}

static WRITE_HANDLER( sound_arm_nmi_w )
{
//	sound_nmi_enabled = 1;
	cpu_set_nmi_line(1,CLEAR_LINE);
	timer_set(TIME_IN_USEC(50),0,nmi_callback);	/* kludge until the K053260 is emulated correctly */
}

static READ_HANDLER( speedup_r )
{
	int data = memory_region(REGION_CPU1)[0x1837];

	if ( cpu_get_pc() == 0xa400 && data == 0 )
		cpu_spinuntil_int();

	return data;
}

/********************************************/

static struct MemoryReadAddress parodius_readmem[] =
{
	{ 0x0000, 0x07ff, bankedram_r },
	{ 0x1837, 0x1837, speedup_r },
	{ 0x0800, 0x1fff, MRA_RAM },
	{ 0x3f8c, 0x3f8c, input_port_0_r },
	{ 0x3f8d, 0x3f8d, input_port_1_r },
	{ 0x3f8e, 0x3f8e, input_port_4_r },
	{ 0x3f8f, 0x3f8f, input_port_2_r },
	{ 0x3f90, 0x3f90, input_port_3_r },
	{ 0x3fa0, 0x3faf, K053244_r },
	{ 0x3fc0, 0x3fc0, watchdog_reset_r },
	{ 0x3fcc, 0x3fcd, parodius_sound_r },	/* K053260 */
	{ 0x2000, 0x27ff, parodius_052109_053245_r },
	{ 0x2000, 0x5fff, K052109_r },
	{ 0x6000, 0x9fff, MRA_BANK1 },			/* banked ROM */
	{ 0xa000, 0xffff, MRA_ROM },			/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress parodius_writemem[] =
{
	{ 0x0000, 0x07ff, bankedram_w, &ram },
	{ 0x0800, 0x1fff, MWA_RAM },
	{ 0x3fa0, 0x3faf, K053244_w },
	{ 0x3fb0, 0x3fbf, K053251_w },
	{ 0x3fc0, 0x3fc0, parodius_3fc0_w },
	{ 0x3fc4, 0x3fc4, parodius_videobank_w },
	{ 0x3fc8, 0x3fc8, parodius_sh_irqtrigger_w },
	{ 0x3fcc, 0x3fcd, K053260_w },
	{ 0x2000, 0x27ff, parodius_052109_053245_w },
	{ 0x2000, 0x5fff, K052109_w },
	{ 0x6000, 0x9fff, MWA_ROM },					/* banked ROM */
	{ 0xa000, 0xffff, MWA_ROM },					/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress parodius_readmem_sound[] =
{
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf801, 0xf801, YM2151_status_port_0_r },
	{ 0xfc00, 0xfc2f, K053260_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress parodius_writemem_sound[] =
{
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf800, YM2151_register_port_0_w },
	{ 0xf801, 0xf801, YM2151_data_port_0_w },
	{ 0xfa00, 0xfa00, sound_arm_nmi_w },
	{ 0xfc00, 0xfc2f, K053260_w },
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( parodius )
	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )

	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
//	PORT_DIPSETTING(    0x00, "No Use" )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "20000 80000" )
	PORT_DIPSETTING(    0x10, "30000 100000" )
	PORT_DIPSETTING(    0x08, "20000" )
	PORT_DIPSETTING(    0x00, "70000" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW #3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Upright Controls" )
	PORT_DIPSETTING(    0x20, "Single" )
	PORT_DIPSETTING(    0x00, "Dual" )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ 0 },
};

static struct K053260_interface k053260_interface =
{
	3579545,
	REGION_SOUND1, /* memory region */
	{ MIXER(70,MIXER_PAN_LEFT), MIXER(70,MIXER_PAN_RIGHT) },
//	sound_nmi_callback
};



static struct MachineDriver machine_driver_parodius =
{
	/* basic machine hardware */
	{
		{
			CPU_KONAMI,		/* 053248 */
			3000000,		/* ? */
			parodius_readmem,parodius_writemem,0,0,
            parodius_interrupt,1
        },
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,
			parodius_readmem_sound, parodius_writemem_sound,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
								/* NMIs are triggered by the 053260 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	parodius_init_machine,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	parodius_vh_start,
	parodius_vh_stop,
	parodius_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K053260,
			&k053260_interface
		}
	}
};

/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( parodius )
	ROM_REGION( 0x51000, REGION_CPU1 ) /* code + banked roms + palette RAM */
	ROM_LOAD( "955e01.bin", 0x10000, 0x20000, 0x49baa334 )
	ROM_LOAD( "955e02.bin", 0x30000, 0x18000, 0x14010d6f )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "955e03.bin", 0x0000, 0x10000, 0x940aa356 )

	ROM_REGION( 0x100000, REGION_GFX1 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "955d07.bin", 0x000000, 0x080000, 0x89473fec ) /* characters */
	ROM_LOAD( "955d08.bin", 0x080000, 0x080000, 0x43d5cda1 ) /* characters */

	ROM_REGION( 0x100000, REGION_GFX2 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "955d05.bin", 0x000000, 0x080000, 0x7a1e55e0 )	/* sprites */
	ROM_LOAD( "955d06.bin", 0x080000, 0x080000, 0xf4252875 )	/* sprites */

	ROM_REGION( 0x80000, REGION_SOUND1 ) /* 053260 samples */
	ROM_LOAD( "955d04.bin", 0x00000, 0x80000, 0xe671491a )
ROM_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

static void parodius_banking(int lines)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int offs = 0;

	//if (lines & 0xf0) logerror("%04x: setlines %02x\n",cpu_get_pc(),lines);

	offs = 0x10000 + (((lines & 0x0f)^0x0f) * 0x4000);
	if (offs >= 0x48000) offs -= 0x40000;
	cpu_setbank( 1, &RAM[offs] );
}

static void parodius_init_machine( void )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	konami_cpu_setlines_callback = parodius_banking;

	paletteram = &memory_region(REGION_CPU1)[0x48000];

	videobank = 0;

	/* init the default bank */
	cpu_setbank(1,&RAM[0x10000]);
}


static void init_parodius(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_2(REGION_GFX2);
}



GAME( 1990, parodius, 0, parodius, parodius, parodius, ROT0, "Konami", "Parodius DA! (Japan)" )
