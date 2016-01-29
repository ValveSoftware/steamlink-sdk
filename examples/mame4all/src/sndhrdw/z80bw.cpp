/* z80bw.c *********************************
 updated: 1997-04-09 08:46 TT
 updated  20-3-1998 LT Added colour changes on base explosion
 updated  02-6-1998 HJB copied from 8080bw and removed unneeded code
 *
 * Author      : Tormod Tjaberg
 * Created     : 1997-04-09
 * Description : Sound routines for the 'astinvad' games
 *
 * Note:
 * The samples were taken from Michael Strutt's (mstrutt@pixie.co.za)
 * excellent space invader emulator and converted to signed samples so
 * they would work under SEAL. The port info was also gleaned from
 * his emulator. These sounds should also work on all the invader games.
 *
 * The sounds are generated using output port 3 and 5
 *
 * Port 4:
 * bit 0=UFO  (repeats)       0.raw
 * bit 1=Shot                 1.raw
 * bit 2=Base hit             2.raw
 * bit 3=Invader hit          3.raw
 * bit 5=global enable?????
 *
 * Port 5:
 * bit 0=Fleet movement 1     4.raw
 * bit 1=Fleet movement 2     5.raw
 * bit 2=Fleet movement 3     6.raw
 * bit 3=Fleet movement 4     7.raw
 * bit 4=UFO 2                8.raw
 * but 5=screen flip		   n/a
 */

#include "driver.h"


void invaders_flip_screen_w(int data);
void invaders_screen_red_w(int data);


/* output port 0x04 definitions - sound effect drive outputs */
#define OUT_PORT_4_UFO			0x01
#define OUT_PORT_4_SHOT			0x02
#define OUT_PORT_4_BASEHIT		0x04
#define OUT_PORT_4_INVADERHIT	0x08
#define OUT_PORT_4_UNUSED		0xf0

/* output port 0x05 definitions - sound effect drive outputs */
#define OUT_PORT_5_FLEET1		0x01
#define OUT_PORT_5_FLEET2		0x02
#define OUT_PORT_5_FLEET3		0x04
#define OUT_PORT_5_FLEET4		0x08
#define OUT_PORT_5_UFO2			0x10
#define OUT_PORT_5_FLIP			0x20
#define OUT_PORT_5_UNUSED		0xc0


#define PLAY(id,loop)           sample_start( id, id, loop )
#define STOP(id)                sample_stop( id )


static const char *astinvad_sample_names[] =
{
	"*invaders",
	"0.wav",
	"1.wav",
	"2.wav",
	"3.wav",
	"4.wav",
	"5.wav",
	"6.wav",
	"7.wav",
	"8.wav",
	0       /* end of array */
};

struct Samplesinterface astinvad_samples_interface =
{
	9,	/* 9 channels */
	25,	/* volume */
	astinvad_sample_names
};


/* sample sound IDs - must match sample file name table above */
enum
{
	SND_UFO = 0,
	SND_SHOT,
	SND_BASEHIT,
	SND_INVADERHIT,
	SND_FLEET1,
	SND_FLEET2,
	SND_FLEET3,
	SND_FLEET4,
	SND_UFO2
};


/* LT 20-3-1998 */
WRITE_HANDLER( astinvad_sh_port_4_w )
{
	static int port4State;

	int bitsChanged;
	int bitsGoneHigh;
	int bitsGoneLow;


	bitsChanged  = port4State ^ data;
	bitsGoneHigh = bitsChanged & data;
	bitsGoneLow  = bitsChanged & ~data;

	port4State = data;


	if ( bitsGoneHigh & OUT_PORT_4_UFO )  PLAY( SND_UFO, 1 );
	if ( bitsGoneLow  & OUT_PORT_4_UFO )  STOP( SND_UFO );

	if ( bitsGoneHigh & OUT_PORT_4_SHOT )  PLAY( SND_SHOT, 0 );
	if ( bitsGoneLow  & OUT_PORT_4_SHOT )  STOP( SND_SHOT );

	if ( bitsGoneHigh & OUT_PORT_4_BASEHIT )
	{
		PLAY( SND_BASEHIT, 0 );
    	/* turn all colours red here */
    	invaders_screen_red_w(1);
    }
	if ( bitsGoneLow & OUT_PORT_4_BASEHIT )
	{
		STOP( SND_BASEHIT );
    	/* restore colours here */
    	invaders_screen_red_w(0);
    }

	if ( bitsGoneHigh & OUT_PORT_4_INVADERHIT )  PLAY( SND_INVADERHIT, 0 );
	if ( bitsGoneLow  & OUT_PORT_4_INVADERHIT )  STOP( SND_INVADERHIT );

	//if ( bitsChanged & OUT_PORT_4_UNUSED ) logerror("Snd Port 4 = %02X\n", data & OUT_PORT_4_UNUSED);
}



WRITE_HANDLER( astinvad_sh_port_5_w )
{
	static int port5State;

	int bitsChanged;
	int bitsGoneHigh;
	int bitsGoneLow;


	bitsChanged  = port5State ^ data;
	bitsGoneHigh = bitsChanged & data;
	bitsGoneLow  = bitsChanged & ~data;

	port5State = data;


	if ( bitsGoneHigh & OUT_PORT_5_FLEET1 )  PLAY( SND_FLEET1, 0 );

	if ( bitsGoneHigh & OUT_PORT_5_FLEET2 )  PLAY( SND_FLEET2, 0 );

	if ( bitsGoneHigh & OUT_PORT_5_FLEET3 )  PLAY( SND_FLEET3, 0 );

	if ( bitsGoneHigh & OUT_PORT_5_FLEET4 )  PLAY( SND_FLEET4, 0 );

	if ( bitsGoneHigh & OUT_PORT_5_UFO2 )  PLAY( SND_UFO2, 0 );
	if ( bitsGoneLow  & OUT_PORT_5_UFO2 )  STOP( SND_UFO2 );

	if ( bitsChanged  & OUT_PORT_5_FLIP )  invaders_flip_screen_w(data & 0x20);

	//if ( bitsChanged  & OUT_PORT_5_UNUSED ) logerror("Snd Port 5 = %02X\n", data & OUT_PORT_5_UNUSED);
}

