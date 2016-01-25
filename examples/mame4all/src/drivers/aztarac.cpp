#include "../vidhrdw/aztarac.cpp"
#include "../sndhrdw/aztarac.cpp"

/*
 * Aztarac driver
 *
 * Jul 25 1999 by Mathis Rosenhauer
 *
 * Thanks to David Fish for additional hardware information.
 *
 */

#include "driver.h"
#include "vidhrdw/vector.h"

/* from machine/foodf.c */
READ_HANDLER( foodf_nvram_r );
WRITE_HANDLER( foodf_nvram_w );
void foodf_nvram_handler(void *file,int read_or_write);

/* from vidhrdw/aztarac.c */
void aztarac_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int aztarac_vh_start (void);
WRITE_HANDLER( aztarac_ubr_w );
int aztarac_vg_interrupt(void);

extern UINT8 *aztarac_vectorram;

/* from sndhrdw/aztarac.c */
READ_HANDLER( aztarac_sound_r );
WRITE_HANDLER( aztarac_sound_w );
READ_HANDLER( aztarac_snd_command_r );
READ_HANDLER( aztarac_snd_status_r );
WRITE_HANDLER( aztarac_snd_status_w );
int aztarac_snd_timed_irq (void);

static READ_HANDLER( aztarac_joystick_r )
{
    return (((input_port_0_r (offset) - 0xf) << 8) |
            ((input_port_1_r (offset) - 0xf) & 0xff));
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x00bfff, MRA_ROM },
	{ 0x022000, 0x022fff, foodf_nvram_r },
	{ 0x027000, 0x027001, aztarac_joystick_r },
	{ 0x027004, 0x027005, input_port_3_r },
	{ 0x027008, 0x027009, aztarac_sound_r },
	{ 0x02700c, 0x02700d, input_port_2_r },
	{ 0x02700e, 0x02700f, watchdog_reset_r },
	{ 0xff8000, 0xffafff, MRA_BANK1 },
	{ 0xffe000, 0xffffff, MRA_BANK2 },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x00bfff, MWA_ROM },
	{ 0x022000, 0x022fff, foodf_nvram_w },
	{ 0x027008, 0x027009, aztarac_sound_w },
	{ 0xff8000, 0xffafff, MWA_BANK1, &aztarac_vectorram },
	{ 0xffb000, 0xffb001, aztarac_ubr_w },
	{ 0xffe000, 0xffffff, MWA_BANK2 },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8800, 0x8800, aztarac_snd_command_r },
	{ 0x8c00, 0x8c01, AY8910_read_port_0_r },
	{ 0x8c02, 0x8c03, AY8910_read_port_1_r },
	{ 0x8c04, 0x8c05, AY8910_read_port_2_r },
	{ 0x8c06, 0x8c07, AY8910_read_port_3_r },
	{ 0x9000, 0x9000, aztarac_snd_status_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8c00, 0x8c00, AY8910_write_port_0_w },
	{ 0x8c01, 0x8c01, AY8910_control_port_0_w },
	{ 0x8c02, 0x8c02, AY8910_write_port_1_w },
	{ 0x8c03, 0x8c03, AY8910_control_port_1_w },
	{ 0x8c04, 0x8c04, AY8910_write_port_2_w },
	{ 0x8c05, 0x8c05, AY8910_control_port_2_w },
	{ 0x8c06, 0x8c06, AY8910_write_port_3_w },
	{ 0x8c07, 0x8c07, AY8910_control_port_3_w },
	{ 0x9000, 0x9000, aztarac_snd_status_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( aztarac )
	PORT_START /* IN0 */
	PORT_ANALOG( 0x1f, 0xf, IPT_AD_STICK_X | IPF_CENTER, 100, 1, 0, 0x1e )

	PORT_START /* IN1 */
	PORT_ANALOG( 0x1f, 0xf, IPT_AD_STICK_Y | IPF_CENTER | IPF_REVERSE, 100, 1, 0, 0x1e )

	PORT_START /* IN2 */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 10, 0, 0, JOYCODE_1_BUTTON5, JOYCODE_1_BUTTON6, 0, 0 )

	PORT_START /* IN3 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
INPUT_PORTS_END



static struct AY8910interface ay8910_interface =
{
	4,	/* 4 chips */
	2000000,	/* 2 MHz */
	{ 15, 15, 15, 15 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 }
};

static struct MachineDriver machine_driver_aztarac =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000, /* 8 MHz */
			readmem, writemem,0,0,
            aztarac_vg_interrupt, 1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2000000,	/* 2 MHz */
			sound_readmem,sound_writemem, 0, 0,
			0,0,
            aztarac_snd_timed_irq, 100
		}
	},
	40, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 1024-1, 0, 768-1 },
	0,
	256, 256,
	aztarac_init_colors,

	VIDEO_TYPE_VECTOR,
	0,
	aztarac_vh_start,
	vector_vh_stop,
	vector_vh_update,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
    },

	foodf_nvram_handler
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( aztarac )
	ROM_REGION( 0xc000, REGION_CPU1 )
	ROM_LOAD_EVEN( "l8_6.bin", 0x000000, 0x001000, 0x25f8da18 )
	ROM_LOAD_ODD ( "n8_0.bin", 0x000000, 0x001000, 0x04e20626 )
	ROM_LOAD_EVEN( "l7_7.bin", 0x002000, 0x001000, 0x230e244c )
	ROM_LOAD_ODD ( "n7_1.bin", 0x002000, 0x001000, 0x37b12697 )
	ROM_LOAD_EVEN( "l6_8.bin", 0x004000, 0x001000, 0x1293fb9d )
	ROM_LOAD_ODD ( "n6_2.bin", 0x004000, 0x001000, 0x712c206a )
	ROM_LOAD_EVEN( "l5_9.bin", 0x006000, 0x001000, 0x743a6501 )
	ROM_LOAD_ODD ( "n5_3.bin", 0x006000, 0x001000, 0xa65cbf99 )
	ROM_LOAD_EVEN( "l4_a.bin", 0x008000, 0x001000, 0x9cf1b0a1 )
	ROM_LOAD_ODD ( "n4_4.bin", 0x008000, 0x001000, 0x5f0080d5 )
	ROM_LOAD_EVEN( "l3_b.bin", 0x00a000, 0x001000, 0x8cc7f7fa )
	ROM_LOAD_ODD ( "n3_5.bin", 0x00a000, 0x001000, 0x40452376 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "j4_c.bin", 0x0000, 0x1000, 0xe897dfcd )
	ROM_LOAD( "j3_d.bin", 0x1000, 0x1000, 0x4016de77 )
ROM_END



static void init_aztarac(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);

	/* patch IRQ vector 4 to autovector location */
	WRITE_WORD(&rom[0x70], 0);
	WRITE_WORD(&rom[0x72], 0x0c02);
}



GAME( 1983, aztarac, 0, aztarac, aztarac, aztarac, ROT0, "Centuri", "Aztarac" )
