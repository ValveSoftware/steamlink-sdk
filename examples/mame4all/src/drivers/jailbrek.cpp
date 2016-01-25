#include "../vidhrdw/jailbrek.cpp"

/***************************************************************************

Jailbreak - (c) 1986 Konami

Ernesto Corvi
ernesto@imagina.com

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"


void konami1_decode(void);

/* from vidhrdw */
extern unsigned char *jailbrek_scroll_x;
extern int jailbrek_vh_start( void );
extern void jailbrek_vh_stop( void );
extern void jailbrek_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
extern void jailbrek_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

static unsigned char *interrupt_control;

static void jailbrek_machine_init( void )
{
	interrupt_control[0] = 0;
}

static READ_HANDLER( jailbrek_speech_r ) {
	return ( VLM5030_BSY() ? 1 : 0 );
}

static WRITE_HANDLER( jailbrek_speech_w ) {
	VLM5030_VCU( data & 1 );
	VLM5030_ST( ( data >> 1 ) & 1 );
	VLM5030_RST( ( data >> 2 ) & 1 );
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, colorram_r },
	{ 0x0800, 0x0fff, videoram_r },
	{ 0x1000, 0x10bf, MRA_RAM }, /* sprites */
	{ 0x10c0, 0x14ff, MRA_RAM }, /* ??? */
	{ 0x1500, 0x1fff, MRA_RAM }, /* work ram */
	{ 0x2000, 0x203f, MRA_RAM }, /* scroll registers */
	{ 0x3000, 0x307f, MRA_NOP }, /* related to sprites? */
	{ 0x3100, 0x3100, input_port_1_r }, /* DSW1 */
	{ 0x3200, 0x3200, MRA_NOP }, /* ??? */
	{ 0x3300, 0x3300, input_port_2_r }, /* coins, start */
	{ 0x3301, 0x3301, input_port_3_r }, /* joy1 */
	{ 0x3302, 0x3302, input_port_4_r }, /* joy2 */
	{ 0x3303, 0x3303, input_port_0_r }, /* DSW0 */
	{ 0x6000, 0x6000, jailbrek_speech_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x07ff, colorram_w, &colorram },
    { 0x0800, 0x0fff, videoram_w, &videoram, &videoram_size },
    { 0x1000, 0x10bf, MWA_RAM, &spriteram, &spriteram_size }, /* sprites */
    { 0x10c0, 0x14ff, MWA_RAM }, /* ??? */
	{ 0x1500, 0x1fff, MWA_RAM }, /* work ram */
    { 0x2000, 0x203f, MWA_RAM, &jailbrek_scroll_x }, /* scroll registers */
    { 0x2043, 0x2043, MWA_NOP }, /* ??? */
    { 0x2044, 0x2044, MWA_RAM, &interrupt_control }, /* irq, nmi enable, bit3 = cocktail mode? */
    { 0x3000, 0x307f, MWA_RAM }, /* ??? */
	{ 0x3100, 0x3100, SN76496_0_w }, /* SN76496 data write */
	{ 0x3200, 0x3200, MWA_NOP },	/* mirror of the previous? */
    { 0x3300, 0x3300, watchdog_reset_w }, /* watchdog */
	{ 0x4000, 0x4000, jailbrek_speech_w }, /* speech pins */
	{ 0x5000, 0x5000, VLM5030_data_w }, /* speech data */
    { 0x8000, 0xffff, MWA_ROM },
    { -1 }  /* end of table */
};

INPUT_PORTS_START( jailbrek )
	PORT_START	/* DSW0  - $3303 */
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
	PORT_DIPSETTING(    0x00, "Invalid?" )

	PORT_START	/* DSW1  - $3100 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "30000 70000" )
	PORT_DIPSETTING(    0x00, "40000 80000" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN0 - $3300 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 - $3301 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 - $3302 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },	/* the four bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	512,	/* 512 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },	/* the bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
			32*8+0*4, 32*8+1*4, 32*8+2*4, 32*8+3*4, 32*8+4*4, 32*8+5*4, 32*8+6*4, 32*8+7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 16 }, /* characters */
	{ REGION_GFX2, 0, &spritelayout, 16*16, 16 }, /* sprites */
	{ -1 } /* end of array */
};

static int jb_interrupt( void ) {
	if ( interrupt_control[0] & 2 )
		return interrupt();

	return ignore_interrupt();
}

static int jb_interrupt_nmi( void ) {
	if ( interrupt_control[0] & 1 )
		return nmi_interrupt();

	return ignore_interrupt();
}

static struct SN76496interface sn76496_interface =
{
	1,	/* 1 chip */
	{ 1500000 },	/*  1.5 MHz ? (hand tuned) */
	{ 100 }
};

static struct VLM5030interface vlm5030_interface =
{
	3580000,    /* master clock */
	100,        /* volume       */
	REGION_SOUND1,	/* memory region of speech rom */
	0,          /* memory size of speech rom */
	0           /* VCU pin level (default)     */
};

static struct MachineDriver machine_driver_jailbrek =
{
	/* basic machine hardware */
	{
		{
		    CPU_M6809,
		    3000000,        /* 3 Mhz ??? */
		    readmem,writemem,0,0,
		    jb_interrupt,1,
		    jb_interrupt_nmi, 500 /* ? */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1, /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	jailbrek_machine_init,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32,512,
	jailbrek_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	jailbrek_vh_start,
	generic_vh_stop,
	jailbrek_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		},
		{
			SOUND_VLM5030,
			&vlm5030_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( jailbrek )
    ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "jailb11d.bin", 0x8000, 0x4000, 0xa0b88dfd )
	ROM_LOAD( "jailb9d.bin",  0xc000, 0x4000, 0x444b7d8e )

    ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "jailb4f.bin",  0x00000, 0x4000, 0xe3b7a226 )	/* characters */
    ROM_LOAD( "jailb5f.bin",  0x04000, 0x4000, 0x504f0912 )

    ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "jailb3e.bin",  0x00000, 0x4000, 0x0d269524 )	/* sprites */
    ROM_LOAD( "jailb4e.bin",  0x04000, 0x4000, 0x27d4f6f4 )
    ROM_LOAD( "jailb5e.bin",  0x08000, 0x4000, 0x717485cb )
    ROM_LOAD( "jailb3f.bin",  0x0c000, 0x4000, 0xe933086f )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "jailbbl.cl2",  0x0000, 0x0020, 0xf1909605 ) /* red & green */
	ROM_LOAD( "jailbbl.cl1",  0x0020, 0x0020, 0xf70bb122 ) /* blue */
	ROM_LOAD( "jailbbl.bp2",  0x0040, 0x0100, 0xd4fe5c97 ) /* char lookup */
	ROM_LOAD( "jailbbl.bp1",  0x0140, 0x0100, 0x0266c7db ) /* sprites lookup */

	ROM_REGION( 0x4000, REGION_SOUND1 ) /* speech rom */
	ROM_LOAD( "jailb8c.bin",  0x0000, 0x2000, 0xd91d15e3 )
ROM_END


static void init_jailbrek(void)
{
	konami1_decode();
}


GAMEX( 1986, jailbrek, 0, jailbrek, jailbrek, jailbrek, ROT0, "Konami", "Jail Break", GAME_NO_COCKTAIL )
