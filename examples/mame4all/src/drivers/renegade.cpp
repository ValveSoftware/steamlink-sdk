#include "../vidhrdw/renegade.cpp"

/***************************************************************************

Renegade
(c)1986 Taito

Nekketsu Kouha Kunio Kun
(c)1986 Technos Japan

Nekketsu Kouha Kunio Kun (bootleg)

driver by Phil Stroffolino, Carlos A. Lozano, Rob Rosenbrock

to enter test mode, hold down P1+P2 and press reset

NMI is used to refresh the sprites
IRQ is used to handle coin inputs

Known issues:
- flipscreen isn't implemented for sprites
- coin counter isn't working properly (interrupt related?)

Memory Map (Preliminary):

Working RAM
  $24			used to mirror bankswitch state
  $25			coin trigger state
  $26			#credits (decimal)
  $27 -  $28	partial credits
  $2C -  $2D	sprite refresh trigger (used by NMI)
  $31			live/demo (if live, player controls are read from input ports)
  $32			indicates 2 player (alternating) game, or 1 player game
  $33			active player
  $37			stage number
  $38			stage state (for stages with more than one part)
  $40			game status flags; 0x80 indicates time over, 0x40 indicates player dead
 $220			player health
 $222 -  $223	stage timer
 $48a -  $48b	horizontal scroll buffer
 $511 -  $690	sprite RAM buffer
 $693			num pending sound commands
 $694 -  $698	sound command queue

$1002			#lives
$1014 - $1015	stage timer - separated digits
$1017 - $1019	stage timer: (ticks,seconds,minutes)
$101a			timer for palette animation
$1020 - $1048	high score table
$10e5 - $10ff	68705 data buffer

Video RAM
$1800 - $1bff	text layer, characters
$1c00 - $1fff	text layer, character attributes
$2000 - $217f	MIX RAM (96 sprites)
$2800 - $2bff	BACK LOW MAP RAM (background tiles)
$2C00 - $2fff	BACK HIGH MAP RAM (background attributes)
$3000 - $30ff	COLOR RG RAM
$3100 - $31ff	COLOR B RAM

Registers
$3800w	scroll(0ff)
$3801w	scroll(300)
$3802w	sound command
$3803w	screen flip (0=flip; 1=noflip)

$3804w	send data to 68705
$3804r	receive data from 68705

$3805w	bankswitch
$3806w	watchdog?
$3807w	coin counter

$3800r	'player1'
		xx		start buttons
		  xx		fire buttons
		    xxxx	joystick state

$3801r	'player2'
		xx		coin inputs
		  xx		fire buttons
		    xxxx	joystick state

$3802r	'DIP2'
		x		unused?
		 x		vblank
		  x		0: 68705 is ready to send information
		   x		1: 68705 is ready to receive information
		    xx		3rd fire buttons for player 2,1
		      xx	difficulty

$3803r 'DIP1'
		x		screen flip
		 x		cabinet type
		  x		bonus (extra life for high score)
		   x		starting lives: 1 or 2
		    xxxx	coins per play

ROM
$4000 - $7fff	bankswitched ROM
$8000 - $ffff	ROM

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"
#include "cpu/m6809/m6809.h"

extern void renegade_vh_screenrefresh(struct osd_bitmap *bitmap, int fullrefresh);
extern int renegade_vh_start( void );
WRITE_HANDLER( renegade_scroll0_w );
WRITE_HANDLER( renegade_scroll1_w );
WRITE_HANDLER( renegade_videoram_w );
WRITE_HANDLER( renegade_textram_w );
WRITE_HANDLER( renegade_flipscreen_w );

extern unsigned char *renegade_textram;

/********************************************************************************************/

static WRITE_HANDLER( adpcm_play_w ){
	data -= 0x2c;
	if( data >= 0 ) ADPCM_play( 0, 0x2000*data, 0x2000*2 );
}

static WRITE_HANDLER( sound_w ){
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,M6809_INT_IRQ);
}

/********************************************************************************************/
/*	MCU Simulation
**
**	Renegade and Nekketsu Kouha Kunio Kun MCU behaviors are identical,
**	except for the initial MCU status byte, and command encryption table.
*/

static int mcu_type;
static const unsigned char *mcu_encrypt_table;
static int mcu_encrypt_table_len;

static const UINT8 renegade_xor_table[0x37] = {
	0x8A,0x48,0x98,0x48,0xA9,0x00,0x85,0x14,0x85,0x15,0xA5,0x11,0x05,0x10,0xF0,0x21,
	0x46,0x11,0x66,0x10,0x90,0x0F,0x18,0xA5,0x14,0x65,0x12,0x85,0x14,0xA5,0x15,0x65,
	0x13,0x85,0x15,0xB0,0x06,0x06,0x12,0x26,0x13,0x90,0xDF,0x68,0xA8,0x68,0xAA,0x38,
	0x60,0x68,0xA8,0x68,0xAA,0x18,0x60
};

static const UINT8 kuniokun_xor_table[0x2a] = {
	0x48,0x8a,0x48,0xa5,0x01,0x48,0xa9,0x00,0x85,0x01,0xa2,0x10,0x26,0x10,0x26,0x11,
	0x26,0x01,0xa5,0x01,0xc5,0x00,0x90,0x04,0xe5,0x00,0x85,0x01,0x26,0x10,0x26,0x11,
	0xca,0xd0,0xed,0x68,0x85,0x01,0x68,0xaa,0x68,0x60
};

void init_kuniokun( void )
{
	mcu_type = 0x85;
	mcu_encrypt_table = kuniokun_xor_table;
	mcu_encrypt_table_len = 0x2a;
}
void init_renegade( void )
{
	mcu_type = 0xda;
	mcu_encrypt_table = renegade_xor_table;
	mcu_encrypt_table_len = 0x37;
}

#define MCU_BUFFER_MAX 6
static unsigned char mcu_buffer[MCU_BUFFER_MAX];
static size_t mcu_input_size;
static int mcu_output_byte;
static int mcu_key;

static READ_HANDLER( mcu_reset_r ){
	mcu_key = -1;
	mcu_input_size = 0;
	mcu_output_byte = 0;
	return 0;
}

static WRITE_HANDLER( mcu_w ){
	mcu_output_byte = 0;

	if( mcu_key<0 ){
		mcu_key = 0;
		mcu_input_size = 1;
		mcu_buffer[0] = data;
	}
	else {
		data ^= mcu_encrypt_table[mcu_key];
		if( ++mcu_key==mcu_encrypt_table_len) mcu_key = 0;
		if( mcu_input_size<MCU_BUFFER_MAX ) mcu_buffer[mcu_input_size++] = data;
	}
}

static void mcu_process_command( void ){
	mcu_input_size = 0;
	mcu_output_byte = 0;

	switch( mcu_buffer[0] ){
		case 0x10:
		mcu_buffer[0] = mcu_type;
		break;

		case 0x26: /* sound code -> sound command */
		{
			int sound_code = mcu_buffer[1];
			static const unsigned char sound_command_table[256] = {
				0xa0,0xa1,0xa2,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,
				0x8d,0x8e,0x8f,0x97,0x96,0x9b,0x9a,0x95,0x9e,0x98,0x90,0x93,0x9d,0x9c,0xa3,0x91,
				0x9f,0x99,0xa6,0xae,0x94,0xa5,0xa4,0xa7,0x92,0xab,0xac,0xb0,0xb1,0xb2,0xb3,0xb4,
				0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x20,
				0x50,0x50,0x90,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x80,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x40,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x20,0x00,0x00,0x10,0x10,0x00,0x00,0x90,0x30,0x30,0x30,0xb0,0xb0,0xb0,0xb0,0xf0,
				0xf0,0xf0,0xf0,0xd0,0xf0,0x00,0x00,0x00,0x00,0x10,0x10,0x50,0x30,0xb0,0xb0,0xf0,
				0xf0,0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
				0x10,0x10,0x30,0x30,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
				0x0f,0x0f,0x0f,0x0f,0x0f,0x8f,0x8f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
				0x0f,0x0f,0x0f,0x0f,0x0f,0xff,0xff,0xff,0xef,0xef,0xcf,0x8f,0x8f,0x0f,0x0f,0x0f
			};
			mcu_buffer[0] = 1;
			mcu_buffer[1] = sound_command_table[sound_code];
		}
		break;

		case 0x33: /* joy bits -> joy dir */
		{
			int joy_bits = mcu_buffer[2];
			static const unsigned char joy_table[0x10] = {
				0,3,7,0,1,2,8,0,5,4,6,0,0,0,0,0
			};
			mcu_buffer[0] = 1;
			mcu_buffer[1] = joy_table[joy_bits&0xf];
		}
		break;

		case 0x44: /* 0x44,0xff,DSW2,stage# -> difficulty */
		{
			int difficulty = mcu_buffer[2]&0x3;
			int stage = mcu_buffer[3];
			const unsigned char difficulty_table[4] = { 5,3,1,2 };
			int result = difficulty_table[difficulty];
			if( stage==0 ) result--;
			result += stage/4;
			mcu_buffer[0] = 1;
			mcu_buffer[1] = result;
		}

		case 0x55: /* 0x55,0x00,0x00,0x00,DSW2 -> timer */
		{
			int difficulty = mcu_buffer[4]&0x3;
			const UINT16 table[4] = {
				0x4001,0x5001,0x1502,0x0002
			};

			mcu_buffer[0] = 3;
			mcu_buffer[2] = table[difficulty]>>8;
			mcu_buffer[3] = table[difficulty]&0xff;
		}
		break;

		case 0x41: { /* 0x41,0x00,0x00,stage# -> ? */
//			int stage = mcu_buffer[3];
			mcu_buffer[0] = 2;
			mcu_buffer[1] = 0x20;
			mcu_buffer[2] = 0x78;
		}
		break;

		case 0x40: /* 0x40,0x00,difficulty,enemy_type -> enemy health */
		{
			int difficulty = mcu_buffer[2];
			int enemy_type = mcu_buffer[3];
			int health;

			if( enemy_type<=4 || (enemy_type&1)==0 ) health = 0x18 + difficulty*8;
			else health = 0x06 + difficulty*2;
			//logerror("e_type:0x%02x diff:0x%02x -> 0x%02x\n", enemy_type, difficulty, health );
			mcu_buffer[0] = 1;
			mcu_buffer[1] = health;
		}
		break;

		case 0x42: /* 0x42,0x00,stage#,character# -> enemy_type */
		{
			int stage = mcu_buffer[2]&0x3;
			int indx = mcu_buffer[3];
			int enemy_type=0;

			if( indx==0 ){
				enemy_type = 1+stage; /* boss */
			}
			else {
				switch( stage ){
					case 0x00:
					if( indx<=2 ) enemy_type = 0x06; else enemy_type = 0x05;
					break;

					case 0x01:
					if( indx<=2 ) enemy_type = 0x0a; else enemy_type = 0x09;
					break;

					case 0x02:
					if( indx<=3 ) enemy_type = 0x0e; else enemy_type = 0xd;
					break;

					case 0x03:
					enemy_type = 0x12;
					break;
				}
			}
			mcu_buffer[0] = 1;
			mcu_buffer[1] = enemy_type;
		}
		break;

		default:
		//logerror("unknown MCU command: %02x\n", mcu_buffer[0] );
		break;
	}
}

static READ_HANDLER( mcu_r ){
	int result = 1;

	if( mcu_input_size ) mcu_process_command();

	if( mcu_output_byte<MCU_BUFFER_MAX )
		result = mcu_buffer[mcu_output_byte++];

	return result;
}

/********************************************************************************************/

static int bank;

static WRITE_HANDLER( bankswitch_w ){
	if( (data&1)!=bank ){
		unsigned char *RAM = memory_region(REGION_CPU1);
		bank = data&1;
		cpu_setbank(1,&RAM[ bank?0x10000:0x4000 ]);
	}
}

static int renegade_interrupt(void){
/*
	static int coin;
	int port = readinputport(1) & 0xc0;
	if (port != 0xc0 ){
		if (coin == 0){
			coin = 1;
			return interrupt();
		}
	}
	else coin = 0;
*/

	static int count;
	count = !count;
	if( count )
	return nmi_interrupt();
	return interrupt();
}

/********************************************************************************************/

static struct MemoryReadAddress main_readmem[] = {
	{ 0x0000, 0x37ff, MRA_RAM },
	{ 0x3800, 0x3800, input_port_0_r },	/* Player#1 controls; P1,P2 start */
	{ 0x3801, 0x3801, input_port_1_r },	/* Player#2 controls; coin triggers */
	{ 0x3802, 0x3802, input_port_2_r },	/* DIP2 ; various IO ports */
	{ 0x3803, 0x3803, input_port_3_r },	/* DIP1 */
	{ 0x3804, 0x3804, mcu_r },
	{ 0x3805, 0x3805, mcu_reset_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress main_writemem[] = {
	{ 0x0000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, renegade_textram_w, &renegade_textram },
	{ 0x2000, 0x27ff, MWA_RAM, &spriteram },
	{ 0x2800, 0x2fff, renegade_videoram_w, &videoram },
	{ 0x3000, 0x30ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x3100, 0x31ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x3800, 0x3800, renegade_scroll0_w },
	{ 0x3801, 0x3801, renegade_scroll1_w },
	{ 0x3802, 0x3802, sound_w },
	{ 0x3803, 0x3803, renegade_flipscreen_w },
	{ 0x3804, 0x3804, mcu_w },
	{ 0x3805, 0x3805, bankswitch_w },
	{ 0x3806, 0x3806, MWA_NOP }, /* watchdog? */
	{ 0x3807, 0x3807, coin_counter_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryReadAddress sound_readmem[] = {
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x1000, 0x1000, soundlatch_r },
	{ 0x2801, 0x2801, YM3526_status_port_0_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress sound_writemem[] = {
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1800, 0x1800, MWA_NOP }, // this gets written the same values as 0x2000
	{ 0x2000, 0x2000, adpcm_play_w },
	{ 0x2800, 0x2800, YM3526_control_port_0_w },
	{ 0x2801, 0x2801, YM3526_write_port_0_w },
	{ 0x3000, 0x3000, MWA_NOP }, /* adpcm related? stereo pan? */
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};



INPUT_PORTS_START( renegade )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )	/* attack left */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )	/* jump */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START /* DIP2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )

	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )	/* attack right */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )		/* 68705 status */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )	/* 68705 status */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* DIP1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING (   0x10, "1" )
	PORT_DIPSETTING (   0x00, "2" )
	PORT_DIPNAME( 0x20, 0x20, "Bonus" )
	PORT_DIPSETTING (   0x20, "30k" )
	PORT_DIPSETTING (   0x00, "None" )
	PORT_DIPNAME(0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING (  0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING (  0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME(0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING (  0x80, DEF_STR( Off ) )
	PORT_DIPSETTING (  0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8, /* 8x8 characters */
	1024, /* 1024 characters */
	3, /* bits per pixel */
	{ 2, 4, 6 },	/* plane offsets; bit 0 is always clear */
	{ 1, 0, 65, 64, 129, 128, 193, 192 }, /* x offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 }, /* y offsets */
	32*8 /* offset to next character */
};

static struct GfxLayout tileslayout1 =
{
	16,16, /* tile size */
	256, /* number of tiles */
	3, /* bits per pixel */

	/* plane offsets */
	{ 4, 0x8000*8+0, 0x8000*8+4 },

	/* x offsets */
	{ 3, 2, 1, 0, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
		32*8+3,32*8+2 ,32*8+1 ,32*8+0 ,48*8+3 ,48*8+2 ,48*8+1 ,48*8+0 },

	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },

	64*8 /* offset to next tile */
};

static struct GfxLayout tileslayout2 =
{
	16,16, /* tile size */
	256, /* number of tiles */
	3, /* bits per pixel */

	/* plane offsets */
	{ 0, 0xC000*8+0, 0xC000*8+4 },

	/* x offsets */
	{ 3, 2, 1, 0, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
		32*8+3,32*8+2 ,32*8+1 ,32*8+0 ,48*8+3 ,48*8+2 ,48*8+1 ,48*8+0 },

	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },

	64*8 /* offset to next tile */
};

static struct GfxLayout tileslayout3 =
{
	16,16, /* tile size */
	256, /* number of tiles */
	3, /* bits per pixel */

	/* plane offsets */
	{ 0x4000*8+4, 0x10000*8+0, 0x10000*8+4 },

	/* x offsets */
	{ 3, 2, 1, 0, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
		32*8+3,32*8+2 ,32*8+1 ,32*8+0 ,48*8+3 ,48*8+2 ,48*8+1 ,48*8+0 },

	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },

	64*8 /* offset to next tile */
};

static struct GfxLayout tileslayout4 =
{
	16,16, /* tile size */
	256, /* number of tiles */
	3, /* bits per pixel */

	/* plane offsets */
	{ 0x4000*8+0, 0x14000*8+0, 0x14000*8+4 },

	/* x offsets */
	{ 3, 2, 1, 0, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
		32*8+3,32*8+2 ,32*8+1 ,32*8+0 ,48*8+3 ,48*8+2 ,48*8+1 ,48*8+0 },

	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },

	64*8 /* offset to next tile */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	/* 8x8 text, 8 colors */
	{ REGION_GFX1, 0x00000, &charlayout,     0, 4 },	/* colors   0- 32 */

	/* 16x16 background tiles, 8 colors */
	{ REGION_GFX2, 0x00000, &tileslayout1, 192, 8 },	/* colors 192-255 */
	{ REGION_GFX2, 0x00000, &tileslayout2, 192, 8 },
	{ REGION_GFX2, 0x00000, &tileslayout3, 192, 8 },
	{ REGION_GFX2, 0x00000, &tileslayout4, 192, 8 },

	{ REGION_GFX2, 0x18000, &tileslayout1, 192, 8 },
	{ REGION_GFX2, 0x18000, &tileslayout2, 192, 8 },
	{ REGION_GFX2, 0x18000, &tileslayout3, 192, 8 },
	{ REGION_GFX2, 0x18000, &tileslayout4, 192, 8 },

	/* 16x16 sprites, 8 colors */
	{ REGION_GFX3, 0x00000, &tileslayout1, 128, 4 },	/* colors 128-159 */
	{ REGION_GFX3, 0x00000, &tileslayout2, 128, 4 },
	{ REGION_GFX3, 0x00000, &tileslayout3, 128, 4 },
	{ REGION_GFX3, 0x00000, &tileslayout4, 128, 4 },

	{ REGION_GFX3, 0x18000, &tileslayout1, 128, 4 },
	{ REGION_GFX3, 0x18000, &tileslayout2, 128, 4 },
	{ REGION_GFX3, 0x18000, &tileslayout3, 128, 4 },
	{ REGION_GFX3, 0x18000, &tileslayout4, 128, 4 },

	{ REGION_GFX3, 0x30000, &tileslayout1, 128, 4 },
	{ REGION_GFX3, 0x30000, &tileslayout2, 128, 4 },
	{ REGION_GFX3, 0x30000, &tileslayout3, 128, 4 },
	{ REGION_GFX3, 0x30000, &tileslayout4, 128, 4 },

	{ REGION_GFX3, 0x48000, &tileslayout1, 128, 4 },
	{ REGION_GFX3, 0x48000, &tileslayout2, 128, 4 },
	{ REGION_GFX3, 0x48000, &tileslayout3, 128, 4 },
	{ REGION_GFX3, 0x48000, &tileslayout4, 128, 4 },
	{ -1 }
};



/* handler called by the 3526 emulator when the internal timers cause an IRQ */
static void irqhandler(int linestate)
{
	cpu_set_irq_line(1,M6809_FIRQ_LINE,linestate);
}

static struct YM3526interface ym3526_interface = {
	1,			/* 1 chip (no more supported) */
	3000000,	/* 3 MHz ? (hand tuned) */
	{ 50 },		/* volume */
	{ irqhandler },
};

static struct ADPCMinterface adpcm_interface =
{
	1,			/* 1 channel */
	8000,		/* 8000Hz playback */
	REGION_SOUND1,	/* memory region */
	0,			/* init function */
	{ 100 }		/* volume */
};



static struct MachineDriver machine_driver_renegade =
{
	{
		{
 			CPU_M6502,
			1500000,	/* 1.5 MHz? */
			main_readmem,main_writemem,0,0,
			renegade_interrupt,2
		},
		{
 			CPU_M6809 | CPU_AUDIO_CPU,
			1500000,	/* ? */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0,	/* FIRQs are caused by the YM3526 */
								/* IRQs are caused by the main CPU */
		}
	},
	60,

	DEFAULT_REAL_60HZ_VBLANK_DURATION*2,

	1, /* cpu slices */
	0, /* init machine */

	32*8, 32*8, { 1*8, 31*8-1, 0, 30*8-1 },
	gfxdecodeinfo,
	256,256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	renegade_vh_start,0,
	renegade_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3526,
			&ym3526_interface
		},
		{
			SOUND_ADPCM,
			&adpcm_interface
		}
	}
};



ROM_START( renegade )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* 64k for code + bank switched ROM */
	ROM_LOAD( "nb-5.bin",     0x08000, 0x8000, 0xba683ddf )
	ROM_LOAD( "na-5.bin",     0x04000, 0x4000, 0xde7e7df4 )
	ROM_CONTINUE(             0x10000, 0x4000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* audio CPU (M6809) */
	ROM_LOAD( "n0-5.bin",     0x8000, 0x8000, 0x3587de3b )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* mcu (missing) */
	ROM_LOAD( "mcu",          0x8000, 0x8000, 0x00000000 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nc-5.bin",     0x0000, 0x8000, 0x9adfaa5d )	/* characters */

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "n1-5.bin",     0x00000, 0x8000, 0x4a9f47f3 )	/* tiles */
	ROM_LOAD( "n6-5.bin",     0x08000, 0x8000, 0xd62a0aa8 )
	ROM_LOAD( "n7-5.bin",     0x10000, 0x8000, 0x7ca5a532 )
	ROM_LOAD( "n2-5.bin",     0x18000, 0x8000, 0x8d2e7982 )
	ROM_LOAD( "n8-5.bin",     0x20000, 0x8000, 0x0dba31d3 )
	ROM_LOAD( "n9-5.bin",     0x28000, 0x8000, 0x5b621b6a )

	ROM_REGION( 0x60000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nh-5.bin",     0x00000, 0x8000, 0xdcd7857c )	/* sprites */
	ROM_LOAD( "nd-5.bin",     0x08000, 0x8000, 0x2de1717c )
	ROM_LOAD( "nj-5.bin",     0x10000, 0x8000, 0x0f96a18e )
	ROM_LOAD( "nn-5.bin",     0x18000, 0x8000, 0x1bf15787 )
	ROM_LOAD( "ne-5.bin",     0x20000, 0x8000, 0x924c7388 )
	ROM_LOAD( "nk-5.bin",     0x28000, 0x8000, 0x69499a94 )
	ROM_LOAD( "ni-5.bin",     0x30000, 0x8000, 0x6f597ed2 )
	ROM_LOAD( "nf-5.bin",     0x38000, 0x8000, 0x0efc8d45 )
	ROM_LOAD( "nl-5.bin",     0x40000, 0x8000, 0x14778336 )
	ROM_LOAD( "no-5.bin",     0x48000, 0x8000, 0x147dd23b )
	ROM_LOAD( "ng-5.bin",     0x50000, 0x8000, 0xa8ee3720 )
	ROM_LOAD( "nm-5.bin",     0x58000, 0x8000, 0xc100258e )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* adpcm */
	ROM_LOAD( "n5-5.bin",     0x00000, 0x8000, 0x7ee43a3c )
	ROM_LOAD( "n4-5.bin",     0x10000, 0x8000, 0x6557564c )
	ROM_LOAD( "n3-5.bin",     0x18000, 0x8000, 0x78fd6190 )
ROM_END

ROM_START( kuniokun )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* 64k for code + bank switched ROM */
	ROM_LOAD( "nb-01.bin",	  0x08000, 0x8000, 0x93fcfdf5 ) // original
	ROM_LOAD( "ta18-11.bin",  0x04000, 0x4000, 0xf240f5cd )
	ROM_CONTINUE(             0x10000, 0x4000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* audio CPU (M6809) */
	ROM_LOAD( "n0-5.bin",     0x8000, 0x8000, 0x3587de3b )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* mcu (missing) */
	ROM_LOAD( "mcu",          0x8000, 0x8000, 0x00000000 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta18-25.bin",  0x0000, 0x8000, 0x9bd2bea3 )	/* characters */

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta18-01.bin",  0x00000, 0x8000, 0xdaf15024 )	/* tiles */
	ROM_LOAD( "ta18-06.bin",  0x08000, 0x8000, 0x1f59a248 )
	ROM_LOAD( "n7-5.bin",     0x10000, 0x8000, 0x7ca5a532 )
	ROM_LOAD( "ta18-02.bin",  0x18000, 0x8000, 0x994c0021 )
	ROM_LOAD( "ta18-04.bin",  0x20000, 0x8000, 0x55b9e8aa )
	ROM_LOAD( "ta18-03.bin",  0x28000, 0x8000, 0x0475c99a )

	ROM_REGION( 0x60000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta18-20.bin",  0x00000, 0x8000, 0xc7d54139 )	/* sprites */
	ROM_LOAD( "ta18-24.bin",  0x08000, 0x8000, 0x84677d45 )
	ROM_LOAD( "ta18-18.bin",  0x10000, 0x8000, 0x1c770853 )
	ROM_LOAD( "ta18-14.bin",  0x18000, 0x8000, 0xaf656017 )
	ROM_LOAD( "ta18-23.bin",  0x20000, 0x8000, 0x3fd19cf7 )
	ROM_LOAD( "ta18-17.bin",  0x28000, 0x8000, 0x74c64c6e )
	ROM_LOAD( "ta18-19.bin",  0x30000, 0x8000, 0xc8795fd7 )
	ROM_LOAD( "ta18-22.bin",  0x38000, 0x8000, 0xdf3a2ff5 )
	ROM_LOAD( "ta18-16.bin",  0x40000, 0x8000, 0x7244bad0 )
	ROM_LOAD( "ta18-13.bin",  0x48000, 0x8000, 0xb6b14d46 )
	ROM_LOAD( "ta18-21.bin",  0x50000, 0x8000, 0xc95e009b )
	ROM_LOAD( "ta18-15.bin",  0x58000, 0x8000, 0xa5d61d01 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* adpcm */
	ROM_LOAD( "ta18-07.bin",  0x00000, 0x8000, 0x02e3f3ed )
	ROM_LOAD( "ta18-08.bin",  0x10000, 0x8000, 0xc9312613 )
	ROM_LOAD( "ta18-09.bin",  0x18000, 0x8000, 0x07ed4705 )
ROM_END

ROM_START( kuniokub )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* 64k for code + bank switched ROM */
	ROM_LOAD( "ta18-10.bin",  0x08000, 0x8000, 0xa90cf44a ) // bootleg
	ROM_LOAD( "ta18-11.bin",  0x04000, 0x4000, 0xf240f5cd )
	ROM_CONTINUE(             0x10000, 0x4000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* audio CPU (M6809) */
	ROM_LOAD( "n0-5.bin",     0x8000, 0x8000, 0x3587de3b )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta18-25.bin",  0x0000, 0x8000, 0x9bd2bea3 )	/* characters */

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta18-01.bin",  0x00000, 0x8000, 0xdaf15024 )	/* tiles */
	ROM_LOAD( "ta18-06.bin",  0x08000, 0x8000, 0x1f59a248 )
	ROM_LOAD( "n7-5.bin",     0x10000, 0x8000, 0x7ca5a532 )
	ROM_LOAD( "ta18-02.bin",  0x18000, 0x8000, 0x994c0021 )
	ROM_LOAD( "ta18-04.bin",  0x20000, 0x8000, 0x55b9e8aa )
	ROM_LOAD( "ta18-03.bin",  0x28000, 0x8000, 0x0475c99a )

	ROM_REGION( 0x60000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta18-20.bin",  0x00000, 0x8000, 0xc7d54139 )	/* sprites */
	ROM_LOAD( "ta18-24.bin",  0x08000, 0x8000, 0x84677d45 )
	ROM_LOAD( "ta18-18.bin",  0x10000, 0x8000, 0x1c770853 )
	ROM_LOAD( "ta18-14.bin",  0x18000, 0x8000, 0xaf656017 )
	ROM_LOAD( "ta18-23.bin",  0x20000, 0x8000, 0x3fd19cf7 )
	ROM_LOAD( "ta18-17.bin",  0x28000, 0x8000, 0x74c64c6e )
	ROM_LOAD( "ta18-19.bin",  0x30000, 0x8000, 0xc8795fd7 )
	ROM_LOAD( "ta18-22.bin",  0x38000, 0x8000, 0xdf3a2ff5 )
	ROM_LOAD( "ta18-16.bin",  0x40000, 0x8000, 0x7244bad0 )
	ROM_LOAD( "ta18-13.bin",  0x48000, 0x8000, 0xb6b14d46 )
	ROM_LOAD( "ta18-21.bin",  0x50000, 0x8000, 0xc95e009b )
	ROM_LOAD( "ta18-15.bin",  0x58000, 0x8000, 0xa5d61d01 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* adpcm */
	ROM_LOAD( "ta18-07.bin",  0x00000, 0x8000, 0x02e3f3ed )
	ROM_LOAD( "ta18-08.bin",  0x10000, 0x8000, 0xc9312613 )
	ROM_LOAD( "ta18-09.bin",  0x18000, 0x8000, 0x07ed4705 )
ROM_END



GAMEX( 1986, renegade, 0,        renegade, renegade, renegade, ROT0, "Technos (Taito America license)", "Renegade (US)", GAME_NO_COCKTAIL )
GAMEX( 1986, kuniokun, renegade, renegade, renegade, kuniokun, ROT0, "Technos", "Nekketsu Kouha Kunio-kun (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1986, kuniokub, renegade, renegade, renegade, 0,        ROT0, "bootleg", "Nekketsu Kouha Kunio-kun (Japan bootleg)", GAME_NO_COCKTAIL )
