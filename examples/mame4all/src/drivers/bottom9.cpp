#include "../vidhrdw/bottom9.cpp"

/***************************************************************************

Bottom of the Ninth (c) 1989 Konami

Similar to S.P.Y.

driver by Nicola Salmoria

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"
#include "vidhrdw/konamiic.h"


extern int bottom9_video_enable;

int bottom9_vh_start(void);
void bottom9_vh_stop(void);
void bottom9_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static int bottom9_interrupt(void)
{
	if (K052109_is_IRQ_enabled()) return interrupt();
	else return ignore_interrupt();
}


static int zoomreadroms,K052109_selected;

static READ_HANDLER( bottom9_bankedram1_r )
{
	if (K052109_selected)
		return K052109_051960_r(offset);
	else
	{
		if (zoomreadroms)
			return K051316_rom_0_r(offset);
		else
			return K051316_0_r(offset);
	}
}

static WRITE_HANDLER( bottom9_bankedram1_w )
{
	if (K052109_selected) K052109_051960_w(offset,data);
	else K051316_0_w(offset,data);
}

static READ_HANDLER( bottom9_bankedram2_r )
{
	if (K052109_selected) return K052109_051960_r(offset + 0x2000);
	else return paletteram_r(offset);
}

static WRITE_HANDLER( bottom9_bankedram2_w )
{
	if (K052109_selected) K052109_051960_w(offset + 0x2000,data);
	else paletteram_xBBBBBGGGGGRRRRR_swap_w(offset,data);
}

static WRITE_HANDLER( bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int offs;

	/* bit 0 = RAM bank */
if ((data & 1) == 0) usrintf_showmessage("bankswitch RAM bank 0");

	/* bit 1-4 = ROM bank */
	if (data & 0x10) offs = 0x20000 + (data & 0x06) * 0x1000;
	else offs = 0x10000 + (data & 0x0e) * 0x1000;
	cpu_setbank(1,&RAM[offs]);
}

static WRITE_HANDLER( bottom9_1f90_w )
{
	/* bits 0/1 = coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* bit 2 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line((data & 0x04) ? ASSERT_LINE : CLEAR_LINE);

	/* bit 3 = disable video */
	bottom9_video_enable = ~data & 0x08;

	/* bit 4 = enable 051316 ROM reading */
	zoomreadroms = data & 0x10;

	/* bit 5 = RAM bank */
	K052109_selected = data & 0x20;
}

static WRITE_HANDLER( bottom9_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,0xff);
}

static int nmienable;

static int bottom9_sound_interrupt(void)
{
	if (nmienable) return nmi_interrupt();
	else return ignore_interrupt();
}

static WRITE_HANDLER( nmi_enable_w )
{
	nmienable = data;
}

static WRITE_HANDLER( sound_bank_w )
{
	unsigned char *RAM;
	int bank_A,bank_B;

	RAM = memory_region(REGION_SOUND1);
	bank_A = 0x20000 * ((data >> 0) & 0x03);
	bank_B = 0x20000 * ((data >> 2) & 0x03);
	K007232_bankswitch(0,RAM + bank_A,RAM + bank_B);
	RAM = memory_region(REGION_SOUND2);
	bank_A = 0x20000 * ((data >> 4) & 0x03);
	bank_B = 0x20000 * ((data >> 6) & 0x03);
	K007232_bankswitch(1,RAM + bank_A,RAM + bank_B);
}



static struct MemoryReadAddress bottom9_readmem[] =
{
	{ 0x0000, 0x07ff, bottom9_bankedram1_r },
	{ 0x1fd0, 0x1fd0, input_port_4_r },
	{ 0x1fd1, 0x1fd1, input_port_0_r },
	{ 0x1fd2, 0x1fd2, input_port_1_r },
	{ 0x1fd3, 0x1fd3, input_port_2_r },
	{ 0x1fe0, 0x1fe0, input_port_3_r },
	{ 0x2000, 0x27ff, bottom9_bankedram2_r },
	{ 0x0000, 0x3fff, K052109_051960_r },
	{ 0x4000, 0x5fff, MRA_RAM },
	{ 0x6000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bottom9_writemem[] =
{
	{ 0x0000, 0x07ff, bottom9_bankedram1_w },
	{ 0x1f80, 0x1f80, bankswitch_w },
	{ 0x1f90, 0x1f90, bottom9_1f90_w },
	{ 0x1fa0, 0x1fa0, watchdog_reset_w },
	{ 0x1fb0, 0x1fb0, soundlatch_w },
	{ 0x1fc0, 0x1fc0, bottom9_sh_irqtrigger_w },
	{ 0x1ff0, 0x1fff, K051316_ctrl_0_w },
	{ 0x2000, 0x27ff, bottom9_bankedram2_w, &paletteram },
	{ 0x0000, 0x3fff, K052109_051960_w },
	{ 0x4000, 0x5fff, MWA_RAM },
	{ 0x6000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress bottom9_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa00d, K007232_read_port_0_r },
	{ 0xb000, 0xb00d, K007232_read_port_1_r },
	{ 0xd000, 0xd000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bottom9_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x9000, 0x9000, sound_bank_w },
	{ 0xa000, 0xa00d, K007232_write_port_0_w },
	{ 0xb000, 0xb00d, K007232_write_port_1_w },
	{ 0xf000, 0xf000, nmi_enable_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( bottom9 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )

	PORT_START
	PORT_DIPNAME( 0x07, 0x04, "Play Time" )
	PORT_DIPSETTING(    0x07, "1'00" )
	PORT_DIPSETTING(    0x06, "1'10" )
	PORT_DIPSETTING(    0x05, "1'20" )
	PORT_DIPSETTING(    0x04, "1'30" )
	PORT_DIPSETTING(    0x03, "1'40" )
	PORT_DIPSETTING(    0x02, "1'50" )
	PORT_DIPSETTING(    0x01, "2'00" )
	PORT_DIPSETTING(    0x00, "2'10" )
	PORT_DIPNAME( 0x18, 0x08, "Bonus Time" )
	PORT_DIPSETTING(    0x18, "00" )
	PORT_DIPSETTING(    0x10, "20" )
	PORT_DIPSETTING(    0x08, "30" )
	PORT_DIPSETTING(    0x00, "40" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x60, "Easy" )
	PORT_DIPSETTING(	0x40, "Normal" )
	PORT_DIPSETTING(	0x20, "Difficult" )
	PORT_DIPSETTING(	0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, "Fielder Control" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x00, "Auto" )
INPUT_PORTS_END



static void volume_callback0(int v)
{
	K007232_set_volume(0,0,(v >> 4) * 0x11,0);
	K007232_set_volume(0,1,0,(v & 0x0f) * 0x11);
}

static void volume_callback1(int v)
{
	K007232_set_volume(1,0,(v >> 4) * 0x11,0);
	K007232_set_volume(1,1,0,(v & 0x0f) * 0x11);
}

static struct K007232_interface k007232_interface =
{
	2,			/* number of chips */
	{ REGION_SOUND1, REGION_SOUND2 },	/* memory regions */
	{ K007232_VOL(40,MIXER_PAN_CENTER,40,MIXER_PAN_CENTER),
			K007232_VOL(40,MIXER_PAN_CENTER,40,MIXER_PAN_CENTER) },	/* volume */
	{ volume_callback0, volume_callback1 }	/* external port callback */
};



static struct MachineDriver machine_driver_bottom9 =
{
	{
		{
			CPU_M6809,
			2000000, /* ? */
			bottom9_readmem,bottom9_writemem,0,0,
			bottom9_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,
			bottom9_sound_readmem, bottom9_sound_writemem,0,0,
			bottom9_sound_interrupt,8	/* irq is triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	bottom9_vh_start,
	bottom9_vh_stop,
	bottom9_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_K007232,
			&k007232_interface
		}
	}
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( bottom9 )
	ROM_REGION( 0x28000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "891n03.k17",   0x10000, 0x10000, 0x8b083ff3 )
    ROM_LOAD( "891-t02.k15",  0x20000, 0x08000, 0x2c10ced2 )
    ROM_CONTINUE(             0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Z80 code */
	ROM_LOAD( "891j01.g8",    0x0000, 0x8000, 0x31b0a0a8 )

	ROM_REGION( 0x080000, REGION_GFX1 ) /* graphics ( dont dispose as the program can read them ) */
	ROM_LOAD_GFX_EVEN( "891e10c", 0x00000, 0x10000, 0x209b0431 )	/* characters */
	ROM_LOAD_GFX_ODD ( "891e10a", 0x00000, 0x10000, 0x8020a9e8 )
	ROM_LOAD_GFX_EVEN( "891e10d", 0x20000, 0x10000, 0x16d5fd7a )
	ROM_LOAD_GFX_ODD ( "891e10b", 0x20000, 0x10000, 0x30121cc0 )
	ROM_LOAD_GFX_EVEN( "891e09c", 0x40000, 0x10000, 0x9dcaefbf )
	ROM_LOAD_GFX_ODD ( "891e09a", 0x40000, 0x10000, 0x56b0ead9 )
	ROM_LOAD_GFX_EVEN( "891e09d", 0x60000, 0x10000, 0x4e1335e6 )
	ROM_LOAD_GFX_ODD ( "891e09b", 0x60000, 0x10000, 0xb6f914fb )

	ROM_REGION( 0x100000, REGION_GFX2 ) /* graphics ( dont dispose as the program can read them ) */
	ROM_LOAD_GFX_EVEN( "891e06e", 0x00000, 0x10000, 0x0b04db1c )	/* sprites */
	ROM_LOAD_GFX_ODD ( "891e06a", 0x00000, 0x10000, 0x5ee37327 )
	ROM_LOAD_GFX_EVEN( "891e06f", 0x20000, 0x10000, 0xf9ada524 )
	ROM_LOAD_GFX_ODD ( "891e06b", 0x20000, 0x10000, 0x2295dfaa )
	ROM_LOAD_GFX_EVEN( "891e06g", 0x40000, 0x10000, 0x04abf78f )
	ROM_LOAD_GFX_ODD ( "891e06c", 0x40000, 0x10000, 0xdbdb0d55 )
	ROM_LOAD_GFX_EVEN( "891e06h", 0x60000, 0x10000, 0x5d5ded8c )
	ROM_LOAD_GFX_ODD ( "891e06d", 0x60000, 0x10000, 0xf9ecbd71 )
	ROM_LOAD_GFX_EVEN( "891e05e", 0x80000, 0x10000, 0xb356e729 )
	ROM_LOAD_GFX_ODD ( "891e05a", 0x80000, 0x10000, 0xbfd5487e )
	ROM_LOAD_GFX_EVEN( "891e05f", 0xa0000, 0x10000, 0xecdd11c5 )
	ROM_LOAD_GFX_ODD ( "891e05b", 0xa0000, 0x10000, 0xaba18d24 )
	ROM_LOAD_GFX_EVEN( "891e05g", 0xc0000, 0x10000, 0xc315f9ae )
	ROM_LOAD_GFX_ODD ( "891e05c", 0xc0000, 0x10000, 0x21fcbc6f )
	ROM_LOAD_GFX_EVEN( "891e05h", 0xe0000, 0x10000, 0xb0aba53b )
	ROM_LOAD_GFX_ODD ( "891e05d", 0xe0000, 0x10000, 0xf6d3f886 )

	ROM_REGION( 0x020000, REGION_GFX3 ) /* graphics ( dont dispose as the program can read them ) */
	ROM_LOAD( "891e07a",      0x00000, 0x10000, 0xb8d8b939 )	/* zoom/rotate */
	ROM_LOAD( "891e07b",      0x10000, 0x10000, 0x83b2f92d )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "891b11.f23",   0x0000, 0x0100, 0xecb854aa )	/* priority encoder (not used) */

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* samples for 007232 #0 */
	ROM_LOAD( "891e08a",      0x00000, 0x10000, 0xcef667bf )
	ROM_LOAD( "891e08b",      0x10000, 0x10000, 0xf7c14a7a )
	ROM_LOAD( "891e08c",      0x20000, 0x10000, 0x756b7f3c )
	ROM_LOAD( "891e08d",      0x30000, 0x10000, 0xcd0d7305 )

	ROM_REGION( 0x40000, REGION_SOUND2 ) /* samples for 007232 #1 */
	ROM_LOAD( "891e04a",      0x00000, 0x10000, 0xdaebbc74 )
	ROM_LOAD( "891e04b",      0x10000, 0x10000, 0x5ffb9ad1 )
	ROM_LOAD( "891e04c",      0x20000, 0x10000, 0x2dbbf16b )
	ROM_LOAD( "891e04d",      0x30000, 0x10000, 0x8b0cd2cc )
ROM_END

ROM_START( bottom9n )
	ROM_REGION( 0x28000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "891n03.k17",   0x10000, 0x10000, 0x8b083ff3 )
    ROM_LOAD( "891n02.k15",   0x20000, 0x08000, 0xd44d9ed4 )
    ROM_CONTINUE(             0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Z80 code */
	ROM_LOAD( "891j01.g8",    0x0000, 0x8000, 0x31b0a0a8 )

	ROM_REGION( 0x080000, REGION_GFX1 ) /* graphics ( dont dispose as the program can read them ) */
	ROM_LOAD_GFX_EVEN( "891e10c", 0x00000, 0x10000, 0x209b0431 )	/* characters */
	ROM_LOAD_GFX_ODD ( "891e10a", 0x00000, 0x10000, 0x8020a9e8 )
	ROM_LOAD_GFX_EVEN( "891e10d", 0x20000, 0x10000, 0x16d5fd7a )
	ROM_LOAD_GFX_ODD ( "891e10b", 0x20000, 0x10000, 0x30121cc0 )
	ROM_LOAD_GFX_EVEN( "891e09c", 0x40000, 0x10000, 0x9dcaefbf )
	ROM_LOAD_GFX_ODD ( "891e09a", 0x40000, 0x10000, 0x56b0ead9 )
	ROM_LOAD_GFX_EVEN( "891e09d", 0x60000, 0x10000, 0x4e1335e6 )
	ROM_LOAD_GFX_ODD ( "891e09b", 0x60000, 0x10000, 0xb6f914fb )

	ROM_REGION( 0x100000, REGION_GFX2 ) /* graphics ( dont dispose as the program can read them ) */
	ROM_LOAD_GFX_EVEN( "891e06e", 0x00000, 0x10000, 0x0b04db1c )	/* sprites */
	ROM_LOAD_GFX_ODD ( "891e06a", 0x00000, 0x10000, 0x5ee37327 )
	ROM_LOAD_GFX_EVEN( "891e06f", 0x20000, 0x10000, 0xf9ada524 )
	ROM_LOAD_GFX_ODD ( "891e06b", 0x20000, 0x10000, 0x2295dfaa )
	ROM_LOAD_GFX_EVEN( "891e06g", 0x40000, 0x10000, 0x04abf78f )
	ROM_LOAD_GFX_ODD ( "891e06c", 0x40000, 0x10000, 0xdbdb0d55 )
	ROM_LOAD_GFX_EVEN( "891e06h", 0x60000, 0x10000, 0x5d5ded8c )
	ROM_LOAD_GFX_ODD ( "891e06d", 0x60000, 0x10000, 0xf9ecbd71 )
	ROM_LOAD_GFX_EVEN( "891e05e", 0x80000, 0x10000, 0xb356e729 )
	ROM_LOAD_GFX_ODD ( "891e05a", 0x80000, 0x10000, 0xbfd5487e )
	ROM_LOAD_GFX_EVEN( "891e05f", 0xa0000, 0x10000, 0xecdd11c5 )
	ROM_LOAD_GFX_ODD ( "891e05b", 0xa0000, 0x10000, 0xaba18d24 )
	ROM_LOAD_GFX_EVEN( "891e05g", 0xc0000, 0x10000, 0xc315f9ae )
	ROM_LOAD_GFX_ODD ( "891e05c", 0xc0000, 0x10000, 0x21fcbc6f )
	ROM_LOAD_GFX_EVEN( "891e05h", 0xe0000, 0x10000, 0xb0aba53b )
	ROM_LOAD_GFX_ODD ( "891e05d", 0xe0000, 0x10000, 0xf6d3f886 )

	ROM_REGION( 0x020000, REGION_GFX3 ) /* graphics ( dont dispose as the program can read them ) */
	ROM_LOAD( "891e07a",      0x00000, 0x10000, 0xb8d8b939 )	/* zoom/rotate */
	ROM_LOAD( "891e07b",      0x10000, 0x10000, 0x83b2f92d )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "891b11.f23",   0x0000, 0x0100, 0xecb854aa )	/* priority encoder (not used) */

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* samples for 007232 #0 */
	ROM_LOAD( "891e08a",      0x00000, 0x10000, 0xcef667bf )
	ROM_LOAD( "891e08b",      0x10000, 0x10000, 0xf7c14a7a )
	ROM_LOAD( "891e08c",      0x20000, 0x10000, 0x756b7f3c )
	ROM_LOAD( "891e08d",      0x30000, 0x10000, 0xcd0d7305 )

	ROM_REGION( 0x40000, REGION_SOUND2 ) /* samples for 007232 #1 */
	ROM_LOAD( "891e04a",      0x00000, 0x10000, 0xdaebbc74 )
	ROM_LOAD( "891e04b",      0x10000, 0x10000, 0x5ffb9ad1 )
	ROM_LOAD( "891e04c",      0x20000, 0x10000, 0x2dbbf16b )
	ROM_LOAD( "891e04d",      0x30000, 0x10000, 0x8b0cd2cc )
ROM_END



static void init_bottom9(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_2(REGION_GFX2);
}



GAME( 1989, bottom9,  0,       bottom9, bottom9, bottom9, ROT0, "Konami", "Bottom of the Ninth (version T)" )
GAME( 1989, bottom9n, bottom9, bottom9, bottom9, bottom9, ROT0, "Konami", "Bottom of the Ninth (version N)" )
