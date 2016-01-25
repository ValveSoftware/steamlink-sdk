#include "../machine/quantum.cpp"

/*
  quantum

  Paul Forgey, 1997

  This code is donated to the MAME team, and inherits all copyrights
  and restrictions from MAME
*/


/*
	QUANTUM MEMORY MAP (per schem):

	000000-003FFF	ROM0
	004000-004FFF	ROM1
	008000-00BFFF	ROM2
	00C000-00FFFF	ROM3
	010000-013FFF	ROM4

	018000-01BFFF	RAM0
	01C000-01CFFF	RAM1

	940000			TRACKBALL
	948000			SWITCHES
	950000			COLORRAM
	958000			CONTROL (LED and coin control)
	960000-970000	RECALL (nvram read)
	968000			VGRST (vector reset)
	970000			VGGO (vector go)
	978000			WDCLR (watchdog)
	900000			NVRAM (nvram write)
	840000			I/OS (sound and dip switches)
	800000-801FFF	VMEM (vector display list)
	940000			I/O (shematic label really - covered above)
	900000			DTACK1

*/


#include "driver.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"



int quantum_interrupt(void);
READ_HANDLER( quantum_switches_r );
WRITE_HANDLER( quantum_led_w );
WRITE_HANDLER( quantum_snd_w );
READ_HANDLER( quantum_snd_r );
READ_HANDLER( quantum_trackball_r );
READ_HANDLER( quantum_input_1_r );
READ_HANDLER( quantum_input_2_r );

READ_HANDLER( foodf_nvram_r );
WRITE_HANDLER( foodf_nvram_w );
void foodf_nvram_handler(void *file,int read_or_write);



struct MemoryReadAddress quantum_read[] =
{
	{ 0x000000, 0x013fff, MRA_ROM },
	{ 0x018000, 0x01cfff, MRA_BANK1 },
	{ 0x800000, 0x801fff, MRA_BANK2 },
	{ 0x840000, 0x84003f, quantum_snd_r },
	{ 0x900000, 0x9001ff, foodf_nvram_r },
	{ 0x940000, 0x940001, quantum_trackball_r }, /* trackball */
	{ 0x948000, 0x948001, quantum_switches_r },
	{ 0x978000, 0x978001, MRA_NOP },	/* ??? */
	{ -1 }	/* end of table */
};

struct MemoryWriteAddress quantum_write[] =
{
	{ 0x000000, 0x013fff, MWA_ROM },
	{ 0x018000, 0x01cfff, MWA_BANK1 },
	{ 0x800000, 0x801fff, MWA_BANK2, &vectorram, &vectorram_size },
	{ 0x840000, 0x84003f, quantum_snd_w },
	{ 0x900000, 0x9001ff, foodf_nvram_w },
	{ 0x950000, 0x95001f, quantum_colorram_w },
	{ 0x958000, 0x958001, quantum_led_w },
	{ 0x960000, 0x960001, MWA_NOP },	/* enable NVRAM? */
	{ 0x968000, 0x968001, avgdvg_reset_w },
//	{ 0x970000, 0x970001, avgdvg_go_w },
//	{ 0x978000, 0x978001, watchdog_reset_w },
	/* the following is wrong, but it's the only way I found to fix the service mode */
	{ 0x978000, 0x978001, avgdvg_go_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( quantum )
	PORT_START	/* IN0 */
	/* YHALT here MUST BE ALWAYS 0  */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,IPT_UNKNOWN )	/* vg YHALT */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

/* first POKEY is SW2, second is SW1 -- more confusion! */
	PORT_START /* DSW0 */
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x30, 0x00, "Right Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x20, "*4" )
	PORT_DIPSETTING(    0x10, "*5" )
	PORT_DIPSETTING(    0x30, "*6" )
	PORT_DIPNAME( 0x08, 0x00, "Left Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x08, "*2" )
	PORT_DIPNAME( 0x07, 0x00, "Bonus Coins" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPSETTING(    0x01, "1 each 5" )
	PORT_DIPSETTING(    0x02, "1 each 4" )
	PORT_DIPSETTING(    0x05, "1 each 3" )
	PORT_DIPSETTING(    0x06, "2 each 4" )

	PORT_START /* DSW1 */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_ANALOG( 0x0f, 0, IPT_TRACKBALL_Y | IPF_REVERSE, 10, 10, 0,0)

	PORT_START      /* IN3 */
	PORT_ANALOG( 0x0f, 0, IPT_TRACKBALL_X, 10, 10, 0, 0 )
INPUT_PORTS_END



static struct POKEYinterface pokey_interface =
{
	2,	/* 2 chips */
	600000,        /* .6 MHz? (hand tuned) */
	{ 50, 50 },
	/* The 8 pot handlers */
	{ quantum_input_1_r, quantum_input_2_r },
	{ quantum_input_1_r, quantum_input_2_r },
	{ quantum_input_1_r, quantum_input_2_r },
	{ quantum_input_1_r, quantum_input_2_r },
	{ quantum_input_1_r, quantum_input_2_r },
	{ quantum_input_1_r, quantum_input_2_r },
	{ quantum_input_1_r, quantum_input_2_r },
	{ quantum_input_1_r, quantum_input_2_r },
	/* The allpot handler */
	{ 0, 0 },
};



static struct MachineDriver machine_driver_quantum =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			6000000,		/* 6MHz */
			quantum_read,quantum_write,0,0,
			quantum_interrupt,3	/* IRQ rate = 750kHz/4096 */
		}
	},
	60, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	300, 400, { 0, 600, 0, 900 },
	0,
	256, 0,
	avg_init_palette_multi,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start_quantum,
	avg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	},

	foodf_nvram_handler
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( quantum )
	ROM_REGION( 0x014000, REGION_CPU1 )
    ROM_LOAD_EVEN( "136016.201",   0x000000, 0x002000, 0x7e7be63a )
    ROM_LOAD_ODD ( "136016.206",   0x000000, 0x002000, 0x2d8f5759 )
    ROM_LOAD_EVEN( "136016.102",   0x004000, 0x002000, 0x408d34f4 )
    ROM_LOAD_ODD ( "136016.107",   0x004000, 0x002000, 0x63154484 )
    ROM_LOAD_EVEN( "136016.203",   0x008000, 0x002000, 0xbdc52fad )
    ROM_LOAD_ODD ( "136016.208",   0x008000, 0x002000, 0xdab4066b )
    ROM_LOAD_EVEN( "136016.104",   0x00C000, 0x002000, 0xbf271e5c )
    ROM_LOAD_ODD ( "136016.109",   0x00C000, 0x002000, 0xd2894424 )
    ROM_LOAD_EVEN( "136016.105",   0x010000, 0x002000, 0x13ec512c )
    ROM_LOAD_ODD ( "136016.110",   0x010000, 0x002000, 0xacb50363 )
ROM_END

ROM_START( quantum1 )
	ROM_REGION( 0x014000, REGION_CPU1 )
    ROM_LOAD_EVEN( "136016.101",   0x000000, 0x002000, 0x5af0bd5b )
    ROM_LOAD_ODD ( "136016.106",   0x000000, 0x002000, 0xf9724666 )
    ROM_LOAD_EVEN( "136016.102",   0x004000, 0x002000, 0x408d34f4 )
    ROM_LOAD_ODD ( "136016.107",   0x004000, 0x002000, 0x63154484 )
    ROM_LOAD_EVEN( "136016.103",   0x008000, 0x002000, 0x948f228b )
    ROM_LOAD_ODD ( "136016.108",   0x008000, 0x002000, 0xe4c48e4e )
    ROM_LOAD_EVEN( "136016.104",   0x00C000, 0x002000, 0xbf271e5c )
    ROM_LOAD_ODD ( "136016.109",   0x00C000, 0x002000, 0xd2894424 )
    ROM_LOAD_EVEN( "136016.105",   0x010000, 0x002000, 0x13ec512c )
    ROM_LOAD_ODD ( "136016.110",   0x010000, 0x002000, 0xacb50363 )
ROM_END

ROM_START( quantump )
	ROM_REGION( 0x014000, REGION_CPU1 )
    ROM_LOAD_EVEN( "quantump.2e",  0x000000, 0x002000, 0x176d73d3 )
    ROM_LOAD_ODD ( "quantump.3e",  0x000000, 0x002000, 0x12fc631f )
    ROM_LOAD_EVEN( "quantump.2f",  0x004000, 0x002000, 0xb64fab48 )
    ROM_LOAD_ODD ( "quantump.3f",  0x004000, 0x002000, 0xa52a9433 )
    ROM_LOAD_EVEN( "quantump.2h",  0x008000, 0x002000, 0x5b29cba3 )
    ROM_LOAD_ODD ( "quantump.3h",  0x008000, 0x002000, 0xc64fc03a )
    ROM_LOAD_EVEN( "quantump.2k",  0x00C000, 0x002000, 0x854f9c09 )
    ROM_LOAD_ODD ( "quantump.3k",  0x00C000, 0x002000, 0x1aac576c )
    ROM_LOAD_EVEN( "quantump.2l",  0x010000, 0x002000, 0x1285b5e7 )
    ROM_LOAD_ODD ( "quantump.3l",  0x010000, 0x002000, 0xe19de844 )
ROM_END



GAMEX( 1982, quantum,  0,       quantum, quantum, 0, ROT0, "Atari", "Quantum (rev 2)", GAME_WRONG_COLORS )
GAMEX( 1982, quantum1, quantum, quantum, quantum, 0, ROT0, "Atari", "Quantum (rev 1)", GAME_WRONG_COLORS )
GAMEX( 1982, quantump, quantum, quantum, quantum, 0, ROT0, "Atari", "Quantum (prototype)", GAME_WRONG_COLORS )

