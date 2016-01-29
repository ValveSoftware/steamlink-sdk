#include "../vidhrdw/m72.cpp"
#include "../sndhrdw/m72.cpp"

/***************************************************************************

IREM M72 board

driver by Nicola Salmoria
protection information by Nao

                                   Board    Working? Protected?
R-Type                              M72        Y         N
Battle Chopper / Mr. Heli           M72        Y         Y
Ninja Spirit                        M72        Y         Y
Image Fight                         M72        Y         Y
Legend of Hero Tonma                M72        Y         Y
X Multiply                          M72(1)     Y         Y
Dragon Breed                        M81        Y         Y
R-Type II                           M82/M84(2) Y         N
Major Title                         M84        Y         N
Hammerin' Harry	/ Daiku no Gensan   M82(3)     Y         N
Ken-Go                              ?          N      Encrypted
Pound for Pound                     M85        Y         N
Air Duel                            M72?       Y         Y
Gallop - Armed Police Unit          M73?(4)    Y         N

(1) different addressing PALs, so different memory map
(2) rtype2j has M84 written on the board, but it's the same hardware as rtype2
(3) multiple versions supported, running on different hardware
(4) there is also a M84 version of Gallop


Notes:

Major Title is supposed to disable rowscroll after a shot, but I haven't found how

Sprite/tile priorities are not completely understood.

Sound doesn't work in Gallop

Samples are missing in Gallop. The NMI handler for the sound CPU is just RETN, so
the hardware has to be different. I also can't make a good sample start offset
table. Same thing with Air Duel.

Maybe there is a layer enable register, e.g. nspirit shows (for an instant)
incomplete screens with bad colors when you start a game.

No samples in Pound for Pound, I haven't checked why; there are a lot of unknown
I/O writes.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "sndhrdw/m72.h"


/* in vidhrdw/m72.c */
extern unsigned char *m72_videoram1,*m72_videoram2,*majtitle_rowscrollram;
void m72_init_machine(void);
void xmultipl_init_machine(void);
void poundfor_init_machine(void);
int m72_interrupt(void);
int m72_vh_start(void);
int dbreed_vh_start(void);
int rtype2_vh_start(void);
int majtitle_vh_start(void);
int hharry_vh_start(void);
void m72_vh_stop(void);
READ_HANDLER( m72_palette1_r );
READ_HANDLER( m72_palette2_r );
WRITE_HANDLER( m72_palette1_w );
WRITE_HANDLER( m72_palette2_w );
READ_HANDLER( m72_videoram1_r );
READ_HANDLER( m72_videoram2_r );
WRITE_HANDLER( m72_videoram1_w );
WRITE_HANDLER( m72_videoram2_w );
WRITE_HANDLER( majtitle_videoram2_w );
WRITE_HANDLER( m72_irq_line_w );
WRITE_HANDLER( m72_scrollx1_w );
WRITE_HANDLER( m72_scrollx2_w );
WRITE_HANDLER( m72_scrolly1_w );
WRITE_HANDLER( m72_scrolly2_w );
WRITE_HANDLER( m72_spritectrl_w );
WRITE_HANDLER( hharry_spritectrl_w );
WRITE_HANDLER( hharryu_spritectrl_w );
void m72_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void dbreed_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void majtitle_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void m72_eof_callback(void);


static unsigned char *protection_ram;



/***************************************************************************

Sample playback

In the later games, the sound CPU can program the start offset of the PCM
samples, but it seems the earlier games have them hardcoded somewhere (maybe
a PROM?). So, here I provided some tables with the start offset precomputed.
They could be built automatically for the most part (00 marks the end of a
sample), but some games have holes in the numbering so we would have to
do some alterations anyway.

Note that Gallop is wrong, and it doesn't play anything anyway, because the
NMI handler of the sound CPU consists of just RETN - so it must be using
different hardware.

***************************************************************************/

static WRITE_HANDLER( bchopper_sample_trigger_w )
{
	int a[6] = { 0x0000, 0x0010, 0x2510, 0x6510, 0x8510, 0x9310 };
	if (data < 6) m72_set_sample_start(a[data]);
}

static WRITE_HANDLER( nspirit_sample_trigger_w )
{
	int a[9] = { 0x0000, 0x0020, 0x2020, 0, 0x5720, 0, 0x7b60, 0x9b60, 0xc360 };
	if (data < 9) m72_set_sample_start(a[data]);
}

static WRITE_HANDLER( imgfight_sample_trigger_w )
{
	int a[7] = { 0x0000, 0x0020, 0x44e0, 0x98a0, 0xc820, 0xf7a0, 0x108c0 };
	if (data < 7) m72_set_sample_start(a[data]);
}

static WRITE_HANDLER( loht_sample_trigger_w )
{
	int a[7] = { 0x0000, 0x0020, 0, 0x2c40, 0x4320, 0x7120, 0xb200 };
	if (data < 7) m72_set_sample_start(a[data]);
}

static WRITE_HANDLER( xmultipl_sample_trigger_w )
{
	int a[3] = { 0x0000, 0x0020, 0x1a40 };
	if (data < 3) m72_set_sample_start(a[data]);
}

static WRITE_HANDLER( dbreed_sample_trigger_w )
{
	int a[9] = { 0x00000, 0x00020, 0x02c40, 0x08160, 0x0c8c0, 0x0ffe0, 0x13000, 0x15820, 0x15f40 };
	if (data < 9) m72_set_sample_start(a[data]);
}

static WRITE_HANDLER( airduel_sample_trigger_w )
{
	int a[16] = { 0x00000, 0x00020, 0x03ec0, 0x05640, 0x06dc0, 0x083a0, 0x0c000, 0x0eb60,
				  0x112e0, 0x13dc0, 0x16520, 0x16d60, 0x18ae0, 0x1a5a0, 0x1bf00, 0x1c340 };
	if (data < 16) m72_set_sample_start(a[data]);
}

static WRITE_HANDLER( gallop_sample_trigger_w )
{
	/* this is most likely wrong */
	int a[31] = { 0x00000, 0x00040, 0x01360, 0x02580, 0x04f20, 0x06240, 0x076e0, 0x08660,
				  0x092a0, 0x09ba0, 0x0a560, 0x0cee0, 0x0de20, 0x0e620, 0x0f1c0, 0x10240,
				  0x11380, 0x127a0, 0x13c40, 0x140a0, 0x16760, 0x17e40, 0x18ee0, 0x19f60,
				  0x1bbc0, 0x1cee0, 0x1e320,       0,       0,       0,       0 };
	if (data < 31) m72_set_sample_start(a[data]);
}



/***************************************************************************

Protection simulation

Most of the games running on this board have an 8751 protection mcu.
It is not known how it works in detail, however it's pretty clear that it
shares RAM at b0000-b0fff.
On startup, the game writes a pattern to the whole RAM, then reads it back
expecting it to be INVERTED. If it isn't, it reports a RAM error.
If the RAM passes the test, the program increments every byte up to b0ffb,
then calls a subroutine at b0000, which has to be provided by the mcu.
It seems that this routine is not supposed to RET, but instead it should
jump directly to the game entry point. The routine should also write some
bytes here and there in RAM (different in every game); those bytes are
checked at various points during the game, causing a crash if they aren't
right.
Note that the program keeps incrementing b0ffe while the game is running,
maybe this is done to keep the 8751 alive. We don't bother with that.

Finally, to do the ROM test the program asks the mcu to provide the correct
values. This is done only in service, so doesn't seem to be much of a
protection. Here we have provided the correct crcs for the available dumps,
of course there is no guarantee that they are actually good.

All the protection routines below are entirely made up. They get the games
running, but they have not been derived from the real 8751 code.

***************************************************************************/

#define CODE_LEN 96
#define CRC_LEN 18

/* Battle Chopper / Mr. Heli */
static unsigned char bchopper_code[CODE_LEN] =
{
	0x68,0x00,0xa0,				// push 0a000h
	0x1f,						// pop ds
	0xc6,0x06,0x38,0x38,0x53,	// mov [3838h], byte 053h
	0xc6,0x06,0x3a,0x38,0x41,	// mov [383ah], byte 041h
	0xc6,0x06,0x3c,0x38,0x4d,	// mov [383ch], byte 04dh
	0xc6,0x06,0x3e,0x38,0x4f,	// mov [383eh], byte 04fh
	0xc6,0x06,0x40,0x38,0x54,	// mov [3840h], byte 054h
	0xc6,0x06,0x42,0x38,0x4f,	// mov [3842h], byte 04fh
	0x68,0x00,0xb0,				// push 0b000h
	0x1f,						// pop ds
	0xc6,0x06,0x00,0x09,0x49^0xff,	// mov [0900h], byte 049h
	0xc6,0x06,0x00,0x0a,0x49^0xff,	// mov [0a00h], byte 049h
	0xc6,0x06,0x00,0x0b,0x49^0xff,	// mov [0b00h], byte 049h
	0xc6,0x06,0x00,0x00,0xcb^0xff,	// mov [0000h], byte 0cbh ; retf : bypass protection check during the game
	0x68,0x00,0xd0,				// push 0d000h
	0x1f,						// pop ds
	// the following is for mrheli only, the game checks for
	// "This game can only be played in Japan..." message in the video text buffer
	// the message is nowhere to be found in the ROMs, so has to be displayed by the mcu
	0xc6,0x06,0x70,0x16,0x77,	// mov [1670h], byte 077h
	0xea,0x68,0x01,0x40,0x00	// jmp  0040:$0168
};
static unsigned char bchopper_crc[CRC_LEN] =  {	0x1a,0x12,0x5c,0x08, 0x84,0xb6,0x73,0xd1,
												0x54,0x91,0x94,0xeb, 0x00,0x00 };
static unsigned char mrheli_crc[CRC_LEN] =	  {	0x24,0x21,0x1f,0x14, 0xf9,0x28,0xfb,0x47,
												0x4c,0x77,0x9e,0xc2, 0x00,0x00 };

/* Ninja Spirit */
static unsigned char nspirit_code[CODE_LEN] =
{
	0x68,0x00,0xa0,				// push 0a000h
	0x1f,						// pop ds
	0xc6,0x06,0x38,0x38,0x4e,	// mov [3838h], byte 04eh
	0xc6,0x06,0x3a,0x38,0x49,	// mov [383ah], byte 049h
	0xc6,0x06,0x3c,0x38,0x4e,	// mov [383ch], byte 04eh
	0xc6,0x06,0x3e,0x38,0x44,	// mov [383eh], byte 044h
	0xc6,0x06,0x40,0x38,0x4f,	// mov [3840h], byte 04fh
	0xc6,0x06,0x42,0x38,0x55,	// mov [3842h], byte 055h
	0x68,0x00,0xb0,				// push 0b000h
	0x1f,						// pop ds
	0xc6,0x06,0x00,0x09,0x49^0xff,	// mov [0900h], byte 049h
	0xc6,0x06,0x00,0x0a,0x49^0xff,	// mov [0a00h], byte 049h
	0xc6,0x06,0x00,0x0b,0x49^0xff,	// mov [0b00h], byte 049h
	0x68,0x00,0xd0,				// push 0d000h
	0x1f,						// pop ds
	// the following is for nspiritj only, the game checks for
	// "This game can only be played in Japan..." message in the video text buffer
	// the message is nowhere to be found in the ROMs, so has to be displayed by the mcu
	0xc6,0x06,0x70,0x16,0x57,	// mov [1670h], byte 057h
	0xc6,0x06,0x71,0x16,0x00,	// mov [1671h], byte 000h
	0xea,0x00,0x00,0x40,0x00	// jmp  0040:$0000
};
static unsigned char nspirit_crc[CRC_LEN] =   {	0xfe,0x94,0x6e,0x4e, 0xc8,0x33,0xa7,0x2d,
												0xf2,0xa3,0xf9,0xe1, 0xa9,0x6c,0x02,0x95, 0x00,0x00 };
static unsigned char nspiritj_crc[CRC_LEN] =  {	0x26,0xa3,0xa5,0xe9, 0xc8,0x33,0xa7,0x2d,
												0xf2,0xa3,0xf9,0xe1, 0xbc,0x6c,0x01,0x95, 0x00,0x00 };

/* Image Fight */
static unsigned char imgfight_code[CODE_LEN] =
{
	0x68,0x00,0xa0,				// push 0a000h
	0x1f,						// pop ds
	0xc6,0x06,0x38,0x38,0x50,	// mov [3838h], byte 050h
	0xc6,0x06,0x3a,0x38,0x49,	// mov [383ah], byte 049h
	0xc6,0x06,0x3c,0x38,0x43,	// mov [383ch], byte 043h
	0xc6,0x06,0x3e,0x38,0x4b,	// mov [383eh], byte 04bh
	0xc6,0x06,0x40,0x38,0x45,	// mov [3840h], byte 045h
	0xc6,0x06,0x42,0x38,0x54,	// mov [3842h], byte 054h
	0x68,0x00,0xb0,				// push 0b000h
	0x1f,						// pop ds
	0xc6,0x06,0x00,0x09,0x49^0xff,	// mov [0900h], byte 049h
	0xc6,0x06,0x00,0x0a,0x49^0xff,	// mov [0a00h], byte 049h
	0xc6,0x06,0x00,0x0b,0x49^0xff,	// mov [0b00h], byte 049h
	0xc6,0x06,0x20,0x09,0x49^0xff,	// mov [0920h], byte 049h
	0xc6,0x06,0x21,0x09,0x4d^0xff,	// mov [0921h], byte 04dh
	0xc6,0x06,0x22,0x09,0x41^0xff,	// mov [0922h], byte 041h
	0xc6,0x06,0x23,0x09,0x47^0xff,	// mov [0923h], byte 047h
	0x68,0x00,0xd0,				// push 0d000h
	0x1f,						// pop ds
	// the game checks for
	// "This game can only be played in Japan..." message in the video text buffer
	// the message is nowhere to be found in the ROMs, so has to be displayed by the mcu
	0xc6,0x06,0xb0,0x1c,0x57,	// mov [1cb0h], byte 057h
	0xea,0x00,0x00,0x40,0x00	// jmp  0040:$0000
};
static unsigned char imgfight_crc[CRC_LEN] =  {	0x7e,0xcc,0xec,0x03, 0x04,0x33,0xb6,0xc5,
												0xbf,0x37,0x92,0x94, 0x00,0x00 };

/* Legend of Hero Tonma */
static unsigned char loht_code[CODE_LEN] =
{
	0x68,0x00,0xa0,				// push 0a000h
	0x1f,						// pop ds
	0xc6,0x06,0x3c,0x38,0x47,	// mov [383ch], byte 047h
	0xc6,0x06,0x3d,0x38,0x47,	// mov [383dh], byte 047h
	0xc6,0x06,0x42,0x38,0x44,	// mov [3842h], byte 044h
	0xc6,0x06,0x43,0x38,0x44,	// mov [3843h], byte 044h
	0x68,0x00,0xb0,				// push 0b000h
	0x1f,						// pop ds
	0xc6,0x06,0x00,0x09,0x49^0xff,	// mov [0900h], byte 049h
	0xc6,0x06,0x00,0x0a,0x49^0xff,	// mov [0a00h], byte 049h
	0xc6,0x06,0x00,0x0b,0x49^0xff,	// mov [0b00h], byte 049h
	0xea,0x5d,0x01,0x40,0x00	// jmp  0040:$015d
};
static unsigned char loht_crc[CRC_LEN] =	  {	0x39,0x00,0x82,0xae, 0x2c,0x9d,0x4b,0x73,
												0xfb,0xac,0xd4,0x6d, 0x6d,0x5b,0x77,0xc0, 0x00,0x00 };

/* X Multiply */
static unsigned char xmultipl_code[CODE_LEN] =
{
	0xea,0x30,0x02,0x00,0x0e	// jmp  0e00:$0230
};
static unsigned char xmultipl_crc[CRC_LEN] =  {	0x73,0x82,0x4e,0x3f, 0xfc,0x56,0x59,0x06,
												0x05,0x48,0xa8,0xf4, 0x00,0x00 };

/* Dragon Breed */
static unsigned char dbreed_code[CODE_LEN] =
{
	0xea,0x6c,0x00,0x00,0x00	// jmp  0000:$006c
};
static unsigned char dbreed_crc[CRC_LEN] =	  {	0xa4,0x96,0x5f,0xc0, 0xab,0x49,0x9f,0x19,
												0x84,0xe6,0xd6,0xca, 0x00,0x00 };

/* Air Duel */
static unsigned char airduel_code[CODE_LEN] =
{
	0x68,0x00,0xd0,				// push 0d000h
	0x1f,						// pop ds
	// the game checks for
	// "This game can only be played in Japan..." message in the video text buffer
	// the message is nowhere to be found in the ROMs, so has to be displayed by the mcu
	0xc6,0x06,0xc0,0x1c,0x57,	// mov [1cc0h], byte 057h
	0xea,0x69,0x0b,0x00,0x00	// jmp  0000:$0b69
};
static unsigned char airduel_crc[CRC_LEN] =	  {	0x72,0x9c,0xca,0x85, 0xc9,0x12,0xcc,0xea,
												0x00,0x00 };


unsigned char *protection_code,*protection_crc;

static READ_HANDLER( protection_r )
{
	if (offset == 0xffb)
		memcpy(protection_ram,protection_code,CODE_LEN);

	return protection_ram[offset];
}

static WRITE_HANDLER( protection_w )
{
	protection_ram[offset] = data ^ 0xff;;

	if (offset == 0x0fff && data == 0)
		memcpy(&protection_ram[0x0fe0],protection_crc,CRC_LEN);
}

static void install_protection_handler(unsigned char *code,unsigned char *crc)
{
	protection_code = code;
	protection_crc =  crc;
	install_mem_read_handler (0,0xb0000,0xb0fff,protection_r);
	install_mem_write_handler(0,0xb0000,0xb0fff,protection_w);
	protection_ram = &memory_region(REGION_CPU1)[0xb0000];
}

static void init_bchopper(void)
{
	install_protection_handler(bchopper_code,bchopper_crc);

	install_port_write_handler(0,0xc0,0xc0,bchopper_sample_trigger_w);
}

static void init_mrheli(void)
{
	install_protection_handler(bchopper_code,mrheli_crc);

	install_port_write_handler(0,0xc0,0xc0,bchopper_sample_trigger_w);
}

static void init_nspirit(void)
{
	install_protection_handler(nspirit_code,nspirit_crc);

	install_port_write_handler(0,0xc0,0xc0,nspirit_sample_trigger_w);
}

static void init_nspiritj(void)
{
	install_protection_handler(nspirit_code,nspiritj_crc);

	install_port_write_handler(0,0xc0,0xc0,nspirit_sample_trigger_w);
}

static void init_imgfight(void)
{
	install_protection_handler(imgfight_code,imgfight_crc);

	install_port_write_handler(0,0xc0,0xc0,imgfight_sample_trigger_w);
}

static void init_loht(void)
{
	install_protection_handler(loht_code,loht_crc);

	install_port_write_handler(0,0xc0,0xc0,loht_sample_trigger_w);
}

static void init_xmultipl(void)
{
	install_protection_handler(xmultipl_code,xmultipl_crc);

	install_port_write_handler(0,0xc0,0xc0,xmultipl_sample_trigger_w);
}

static void init_dbreed(void)
{
	install_protection_handler(dbreed_code,dbreed_crc);

	install_port_write_handler(0,0xc0,0xc0,dbreed_sample_trigger_w);
}

static void init_airduel(void)
{
	install_protection_handler(airduel_code,airduel_crc);

	install_port_write_handler(0,0xc0,0xc0,airduel_sample_trigger_w);
}

static void init_gallop(void)
{
	install_port_write_handler(0,0xc0,0xc0,gallop_sample_trigger_w);
}




static unsigned char *soundram;


static READ_HANDLER( soundram_r )
{
	return soundram[offset];
}

static WRITE_HANDLER( soundram_w )
{
	soundram[offset] = data;
}

static WRITE_HANDLER( m72_port02_w )
{
	if (offset != 0)
	{
		//if (data) logerror("write %02x to port 03\n",data);
		return;
	}
	//if (data & 0xec) logerror("write %02x to port 02\n",data);

	/* bits 0/1 are coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* bit 3 is used but unknown */

	/* bit 4 resets sound CPU (active low) */
	if (data & 0x10)
		cpu_set_reset_line(1,CLEAR_LINE);
	else
		cpu_set_reset_line(1,ASSERT_LINE);

	/* other bits unknown */
}

static WRITE_HANDLER( rtype2_port02_w )
{
	if (offset != 0)
	{
		//if (data) logerror("write %02x to port 03\n",data);
		return;
	}
	//if (data & 0xfc) logerror("write %02x to port 02\n",data);

	/* bits 0/1 are coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* other bits unknown */
}


static READ_HANDLER( poundfor_trackball_r )
{
	static int prev[4],diff[4];

	if (offset == 0)
	{
		int i,curr;

		for (i = 0;i < 4;i++)
		{
			curr = readinputport(6+i);
			diff[i] = (curr - prev[i]);
			prev[i] = curr;
		}
	}

	switch (offset)
	{
		default:
		case 0:
			return diff[0] & 0xff;
		case 1:
			return 0;
		case 2:
			return ((diff[0] >> 8) & 0x1f) | (readinputport(4) & 0xe0);
		case 3:
			return ((diff[2] >> 8) & 0x1f) | (readinputport(5) & 0xe0);
		case 4:
			return diff[1] & 0xff;
		case 5:
			return diff[3] & 0xff;
		case 6:
			return ((diff[1] >> 8) & 0x1f);
		case 7:
			return ((diff[3] >> 8) & 0x1f);
	}
}



#define CPU1_MEMORY(NAME,ROMSIZE,WORKRAM) 						\
static struct MemoryReadAddress NAME##_readmem[] =				\
{																\
	{ 0x00000, ROMSIZE-1, MRA_ROM },							\
	{ WORKRAM, WORKRAM+0x3fff, MRA_RAM },						\
	{ 0xc0000, 0xc03ff, MRA_RAM },								\
	{ 0xc8000, 0xc8bff, m72_palette1_r },						\
	{ 0xcc000, 0xccbff, m72_palette2_r },						\
	{ 0xd0000, 0xd3fff, m72_videoram1_r },						\
	{ 0xd8000, 0xdbfff, m72_videoram2_r },						\
	{ 0xe0000, 0xeffff, soundram_r },							\
	{ -1 }	/* end of table */									\
};																\
static struct MemoryWriteAddress NAME##_writemem[] =			\
{																\
	{ 0x00000, ROMSIZE-1, MWA_ROM },							\
	{ WORKRAM, WORKRAM+0x3fff, MWA_RAM },	/* work RAM */		\
	{ 0xc0000, 0xc03ff, MWA_RAM, &spriteram, &spriteram_size },	\
	{ 0xc8000, 0xc8bff, m72_palette1_w, &paletteram },			\
	{ 0xcc000, 0xccbff, m72_palette2_w, &paletteram_2 },		\
	{ 0xd0000, 0xd3fff, m72_videoram1_w, &m72_videoram1 },		\
	{ 0xd8000, 0xdbfff, m72_videoram2_w, &m72_videoram2 },		\
	{ 0xe0000, 0xeffff, soundram_w },							\
	{ -1 }	/* end of table */									\
};

/*                     ROMSIZE  WORKRAM */
CPU1_MEMORY( rtype,    0x40000, 0x40000 )
CPU1_MEMORY( m72,      0x80000, 0xa0000 )
CPU1_MEMORY( xmultipl, 0x80000, 0x80000 )
CPU1_MEMORY( dbreed,   0x80000, 0x90000 )


static struct MemoryReadAddress rtype2_readmem[] =
{
	{ 0x00000, 0x7ffff, MRA_ROM },
	{ 0xc0000, 0xc03ff, MRA_RAM },
	{ 0xc8000, 0xc8bff, m72_palette1_r },
	{ 0xd0000, 0xd3fff, m72_videoram1_r },
	{ 0xd4000, 0xd7fff, m72_videoram2_r },
	{ 0xd8000, 0xd8bff, m72_palette2_r },
	{ 0xe0000, 0xe3fff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress rtype2_writemem[] =
{
	{ 0x00000, 0x7ffff, MWA_ROM },
	{ 0xb0000, 0xb0001, m72_irq_line_w },
	{ 0xbc000, 0xbc001, m72_spritectrl_w },
	{ 0xc0000, 0xc03ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc8000, 0xc8bff, m72_palette1_w, &paletteram },
	{ 0xd0000, 0xd3fff, m72_videoram1_w, &m72_videoram1 },
	{ 0xd4000, 0xd7fff, m72_videoram2_w, &m72_videoram2 },
	{ 0xd8000, 0xd8bff, m72_palette2_w, &paletteram_2 },
	{ 0xe0000, 0xe3fff, MWA_RAM },	/* work RAM */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress majtitle_readmem[] =
{
	{ 0x00000, 0x7ffff, MRA_ROM },
	{ 0xa0000, 0xa03ff, MRA_RAM },
	{ 0xa4000, 0xa4bff, m72_palette2_r },
	{ 0xac000, 0xaffff, m72_videoram1_r },
	{ 0xb0000, 0xbffff, m72_videoram2_r },
	{ 0xc0000, 0xc03ff, MRA_RAM },
	{ 0xc8000, 0xc83ff, MRA_RAM },
	{ 0xcc000, 0xccbff, m72_palette1_r },
	{ 0xd0000, 0xd3fff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress majtitle_writemem[] =
{
	{ 0x00000, 0x7ffff, MWA_ROM },
	{ 0xa0000, 0xa03ff, MWA_RAM, &majtitle_rowscrollram },
	{ 0xa4000, 0xa4bff, m72_palette2_w, &paletteram_2 },
	{ 0xac000, 0xaffff, m72_videoram1_w, &m72_videoram1 },
	{ 0xb0000, 0xbffff, majtitle_videoram2_w, &m72_videoram2 },
	{ 0xc0000, 0xc03ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc8000, 0xc83ff, MWA_RAM, &spriteram_2 },
	{ 0xcc000, 0xccbff, m72_palette1_w, &paletteram },
	{ 0xd0000, 0xd3fff, MWA_RAM },	/* work RAM */
	{ 0xe0000, 0xe0001, m72_irq_line_w },
//	{ 0xe4000, 0xe4001, MWA_RAM },	/* playfield enable? 1 during screen transitions, 0 otherwise */
	{ 0xec000, 0xec001, hharryu_spritectrl_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress hharry_readmem[] =
{
	{ 0x00000, 0x7ffff, MRA_ROM },
	{ 0xa0000, 0xa3fff, MRA_RAM },
	{ 0xc0000, 0xc03ff, MRA_RAM },
	{ 0xc8000, 0xc8bff, m72_palette1_r },
	{ 0xcc000, 0xccbff, m72_palette2_r },
	{ 0xd0000, 0xd3fff, m72_videoram1_r },
	{ 0xd8000, 0xdbfff, m72_videoram2_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress hharry_writemem[] =
{
	{ 0x00000, 0x7ffff, MWA_ROM },
	{ 0xa0000, 0xa3fff, MWA_RAM },	/* work RAM */
	{ 0xb0ffe, 0xb0fff, MWA_RAM },	/* leftover from protection?? */
	{ 0xc0000, 0xc03ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc8000, 0xc8bff, m72_palette1_w, &paletteram },
	{ 0xcc000, 0xccbff, m72_palette2_w, &paletteram_2 },
	{ 0xd0000, 0xd3fff, m72_videoram1_w, &m72_videoram1 },
	{ 0xd8000, 0xdbfff, m72_videoram2_w, &m72_videoram2 },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress hharryu_readmem[] =
{
	{ 0x00000, 0x7ffff, MRA_ROM },
	{ 0xa0000, 0xa0bff, m72_palette1_r },
	{ 0xa8000, 0xa8bff, m72_palette2_r },
	{ 0xc0000, 0xc03ff, MRA_RAM },
	{ 0xd0000, 0xd3fff, m72_videoram1_r },
	{ 0xd4000, 0xd7fff, m72_videoram2_r },
	{ 0xe0000, 0xe3fff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress hharryu_writemem[] =
{
	{ 0x00000, 0x7ffff, MWA_ROM },
	{ 0xa0000, 0xa0bff, m72_palette1_w, &paletteram },
	{ 0xa8000, 0xa8bff, m72_palette2_w, &paletteram_2 },
	{ 0xb0000, 0xb0001, m72_irq_line_w },
	{ 0xbc000, 0xbc001, hharryu_spritectrl_w },
	{ 0xb0ffe, 0xb0fff, MWA_RAM },	/* leftover from protection?? */
	{ 0xc0000, 0xc03ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd0000, 0xd3fff, m72_videoram1_w, &m72_videoram1 },
	{ 0xd4000, 0xd7fff, m72_videoram2_w, &m72_videoram2 },
	{ 0xe0000, 0xe3fff, MWA_RAM },	/* work RAM */
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ 0x02, 0x02, input_port_2_r },
	{ 0x03, 0x03, input_port_3_r },
	{ 0x04, 0x04, input_port_4_r },
	{ 0x05, 0x05, input_port_5_r },
	{ -1 }  /* end of table */
};

static struct IOReadPort poundfor_readport[] =
{
	{ 0x02, 0x02, input_port_0_r },
	{ 0x03, 0x03, input_port_1_r },
	{ 0x04, 0x04, input_port_2_r },
	{ 0x05, 0x05, input_port_3_r },
	{ 0x08, 0x0f, poundfor_trackball_r },
	{ -1 }  /* end of table */
};


static struct IOWritePort writeport[] =
{
	{ 0x00, 0x01, m72_sound_command_w },
	{ 0x02, 0x03, m72_port02_w },	/* coin counters, reset sound cpu, other stuff? */
	{ 0x04, 0x05, m72_spritectrl_w },
	{ 0x06, 0x07, m72_irq_line_w },
	{ 0x80, 0x81, m72_scrolly1_w },
	{ 0x82, 0x83, m72_scrollx1_w },
	{ 0x84, 0x85, m72_scrolly2_w },
	{ 0x86, 0x87, m72_scrollx2_w },
/*	{ 0xc0, 0xc0      trigger sample, filled by init_ function */
	{ -1 }  /* end of table */
};

static struct IOWritePort xmultipl_writeport[] =
{
	{ 0x00, 0x01, m72_sound_command_w },
	{ 0x02, 0x03, m72_port02_w },	/* coin counters, reset sound cpu, other stuff? */
	{ 0x04, 0x04, hharry_spritectrl_w },
	{ 0x06, 0x07, m72_irq_line_w },
	{ 0x80, 0x81, m72_scrolly1_w },
	{ 0x82, 0x83, m72_scrollx1_w },
	{ 0x84, 0x85, m72_scrolly2_w },
	{ 0x86, 0x87, m72_scrollx2_w },
/*	{ 0xc0, 0xc0      trigger sample, filled by init_ function */
	{ -1 }  /* end of table */
};

static struct IOWritePort rtype2_writeport[] =
{
	{ 0x00, 0x01, m72_sound_command_w },
	{ 0x02, 0x03, rtype2_port02_w },
	{ 0x80, 0x81, m72_scrolly1_w },
	{ 0x82, 0x83, m72_scrollx1_w },
	{ 0x84, 0x85, m72_scrolly2_w },
	{ 0x86, 0x87, m72_scrollx2_w },
	{ -1 }  /* end of table */
};

static struct IOWritePort hharry_writeport[] =
{
	{ 0x00, 0x01, m72_sound_command_w },
	{ 0x02, 0x03, rtype2_port02_w },	/* coin counters, reset sound cpu, other stuff? */
	{ 0x04, 0x04, hharry_spritectrl_w },
	{ 0x06, 0x07, m72_irq_line_w },
	{ 0x80, 0x81, m72_scrolly1_w },
	{ 0x82, 0x83, m72_scrollx1_w },
	{ 0x84, 0x85, m72_scrolly2_w },
	{ 0x86, 0x87, m72_scrollx2_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xffff, MWA_RAM, &soundram },
	{ -1 }	/* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x01, 0x01, YM2151_status_port_0_r },
	{ 0x02, 0x02, soundlatch_r },
	{ 0x84, 0x84, m72_sample_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, YM2151_register_port_0_w },
	{ 0x01, 0x01, YM2151_data_port_0_w },
	{ 0x06, 0x06, m72_sound_irq_ack_w },
	{ 0x82, 0x82, m72_sample_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort rtype2_sound_readport[] =
{
	{ 0x01, 0x01, YM2151_status_port_0_r },
	{ 0x80, 0x80, soundlatch_r },
	{ 0x84, 0x84, m72_sample_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort rtype2_sound_writeport[] =
{
	{ 0x00, 0x00, YM2151_register_port_0_w },
	{ 0x01, 0x01, YM2151_data_port_0_w },
	{ 0x80, 0x81, rtype2_sample_addr_w },
	{ 0x82, 0x82, m72_sample_w },
	{ 0x83, 0x83, m72_sound_irq_ack_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort poundfor_sound_readport[] =
{
	{ 0x41, 0x41, YM2151_status_port_0_r },
	{ 0x42, 0x42, soundlatch_r },
//	{ 0x84, 0x84, m72_sample_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort poundfor_sound_writeport[] =
{
	{ 0x40, 0x40, YM2151_register_port_0_w },
	{ 0x41, 0x41, YM2151_data_port_0_w },
	{ 0x42, 0x42, m72_sound_irq_ack_w },
//	{ 0x80, 0x81, _sample_addr_w },
//	{ 0x82, 0x82, m72_sample_w },
	{ -1 }  /* end of table */
};



#define JOYSTICK_1 \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )

#define JOYSTICK_2 \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )

#define COIN_MODE_1 \
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) ) \
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) ) \
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) ) \
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 8C_3C ) ) \
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 5C_3C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) ) \
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )

#define COIN_MODE_2 \
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) ) \
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )


INPUT_PORTS_START( rtype )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50k 150k 250k 400k 600k" )
	PORT_DIPSETTING(    0x08, "100k 200k 350k 500k 700k"  )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/* identical but Demo Sounds is inverted */
INPUT_PORTS_START( rtypep )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50k 150k 250k 400k 600k" )
	PORT_DIPSETTING(    0x08, "100k 200k 350k 500k 700k"  )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( bchopper )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, "Weapons Cost" )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x00, "Double" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( nspirit )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) ) /* Probably Bonus Life */
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( imgfight )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( loht )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x08, "Very Hard" )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( xmultipl )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "4" )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( dbreed )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Flip Screen?" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( rtype2 )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
    /* Coin Mode 1 */
    COIN_MODE_1
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x10, "Upright 1 Player" )
	PORT_DIPSETTING(    0x00, "Upright 2 Players" )
	PORT_DIPSETTING(    0x18, DEF_STR( Cocktail ) )
//	PORT_DIPSETTING(    0x08, "Upright 2 Players" )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( hharry )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x10, 0x10, "Limit N. of Continue?" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x04, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, "Upright, separate controls" )
	// PORT_DIPSETTING(    0x02, "Upright, separate controls" )
	PORT_DIPSETTING(    0x06, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x08, "Coin Mode" )
	PORT_DIPSETTING(    0x08, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	/* Coin Mode 1 */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, "2 to start, 1 to continue" )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* Coin mode 2, not supported yet */
    // COIN_MODE_2
INPUT_PORTS_END

INPUT_PORTS_START( poundfor )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Coin Mode" )
	PORT_DIPSETTING(    0x08, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )

	/* Coin Mode 1 */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, "1 Coin/1 Credit, 1 to continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2

	PORT_START
	PORT_BIT( 0x1f, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* high bits of trackball X */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x1f, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* high bits of trackball X */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_ANALOG( 0xffff, 0x0000, IPT_TRACKBALL_X | IPF_PLAYER1, 50, 30, 0, 0 )

	PORT_START
	PORT_ANALOG( 0xffff, 0x0000, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER1, 50, 30, 0, 0 )

	PORT_START
	PORT_ANALOG( 0xffff, 0x0000, IPT_TRACKBALL_X | IPF_PLAYER2, 50, 30, 0, 0 )

	PORT_START
	PORT_ANALOG( 0xffff, 0x0000, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER2, 50, 30, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( airduel )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Coin Mode" )
	PORT_DIPSETTING(    0x08, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )

	/* Coin Mode 1 */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
//	PORT_DIPSETTING(    0x10, DEF_STR( Free-Play ) )  /* another free play */
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
    /* Coin Mode 2, not supported yet */
    // COIN_MODE_2
INPUT_PORTS_END

INPUT_PORTS_START( gallop )
	PORT_START
	JOYSTICK_1
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	JOYSTICK_2
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE ) /* 0x20 is another test mode */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Upright 1 Player" )
	PORT_DIPSETTING(    0x02, "Upright 2 Players" )
	PORT_DIPSETTING(    0x06, DEF_STR( Cocktail ) )
//	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPNAME( 0x08, 0x08, "Coin Mode" )
	PORT_DIPSETTING(    0x08, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	/* Coin Mode 1 */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, "2 to start, 1 to continue" )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* Coin mode 2, not supported yet */
    // COIN_MODE_2
INPUT_PORTS_END



static struct GfxLayout tilelayout =
{
	8,8,	/* 8*8 characters */
	RGN_FRAC(1,4),	/* NUM characters */
	4,	/* 4 bits per pixel */
	{ RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	RGN_FRAC(1,4),	/* NUM characters */
	4,	/* 4 bits per pixel */
	{ RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo m72_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,    0, 16 },
	{ REGION_GFX2, 0, &tilelayout,    512, 16 },
	{ REGION_GFX3, 0, &tilelayout,    512, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo rtype2_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,     0, 16 },
	{ REGION_GFX2, 0, &tilelayout,     512, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo majtitle_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,     0, 16 },
	{ REGION_GFX2, 0, &tilelayout,     512, 16 },
	{ REGION_GFX3, 0, &spritelayout,     0, 16 },
	{ -1 } /* end of array */
};



static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ m72_ym2151_irq_handler },
	{ 0 }
};

static struct DACinterface dac_interface =
{
	1,	/* 1 channel */
	{ 40 }
};



static struct MachineDriver machine_driver_rtype =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			rtype_readmem,rtype_writemem,readport,writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,0	/* no NMIs unlike the other games */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	m72_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	m72_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	m72_vh_start,
	m72_vh_stop,
	m72_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
};

static struct MachineDriver machine_driver_m72 =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			m72_readmem,m72_writemem,readport,writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	m72_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	m72_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	m72_vh_start,
	m72_vh_stop,
	m72_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_xmultipl =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			xmultipl_readmem,xmultipl_writemem,readport,xmultipl_writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	xmultipl_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	m72_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	m72_vh_start,
	m72_vh_stop,
	m72_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_dbreed =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			dbreed_readmem,dbreed_writemem,readport,xmultipl_writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	xmultipl_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	m72_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	dbreed_vh_start,
	m72_vh_stop,
	dbreed_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_rtype2 =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			rtype2_readmem,rtype2_writemem,readport,rtype2_writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,rtype2_sound_readport,rtype2_sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	m72_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	rtype2_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	rtype2_vh_start,
	m72_vh_stop,
	m72_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_majtitle =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			majtitle_readmem,majtitle_writemem,readport,rtype2_writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,rtype2_sound_readport,rtype2_sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	m72_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	majtitle_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	majtitle_vh_start,
	m72_vh_stop,
	majtitle_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_hharry =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			hharry_readmem,hharry_writemem,readport,hharry_writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,rtype2_sound_readport,rtype2_sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	xmultipl_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	rtype2_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	hharry_vh_start,
	m72_vh_stop,
	m72_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_hharryu =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			hharryu_readmem,hharryu_writemem,readport,rtype2_writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,rtype2_sound_readport,rtype2_sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	xmultipl_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	rtype2_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	rtype2_vh_start,
	m72_vh_stop,
	m72_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_poundfor =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			8000000,	/* ?? */
			rtype2_readmem,rtype2_writemem,poundfor_readport,rtype2_writeport,
			m72_interrupt,16
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 MHz */
			sound_readmem,sound_writemem,poundfor_sound_readport,poundfor_sound_writeport,
			nmi_interrupt,128	/* clocked by V1? (Vigilante) */
								/* IRQs are generated by main Z80 and YM2151 */
		}
	},
	55, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	poundfor_init_machine,

	/* video hardware */
	512, 512, { 8*8, (64-8)*8-1, 16*8, (64-16)*8-1 },
	rtype2_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	m72_eof_callback,
	rtype2_vh_start,
	m72_vh_stop,
	m72_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( rtype )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "rt_r-h0-.bin", 0x00000, 0x10000, 0xc2940df2 )
	ROM_LOAD_V20_ODD ( "rt_r-l0-.bin", 0x00000, 0x10000, 0x858cc0f6 )
	ROM_LOAD_V20_EVEN( "rt_r-h1-.bin", 0x20000, 0x10000, 0x5bcededa )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "rt_r-l1-.bin", 0x20000, 0x10000, 0x4821141c )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-00.bin",   0x00000, 0x10000, 0xdad53bc0 )	/* sprites */
	ROM_LOAD( "cpu-01.bin",   0x10000, 0x10000, 0xb28d1a60 )
	ROM_LOAD( "cpu-10.bin",   0x20000, 0x10000, 0xd6a66298 )
	ROM_LOAD( "cpu-11.bin",   0x30000, 0x10000, 0xbb182f1a )
	ROM_LOAD( "cpu-20.bin",   0x40000, 0x10000, 0xfc247c8a )
	ROM_LOAD( "cpu-21.bin",   0x50000, 0x10000, 0x5b41f5f3 )
	ROM_LOAD( "cpu-30.bin",   0x60000, 0x10000, 0xeb02a1cb )
	ROM_LOAD( "cpu-31.bin",   0x70000, 0x10000, 0x2bec510a )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-a0.bin",   0x00000, 0x08000, 0x4e212fb0 )	/* tiles #1 */
	ROM_LOAD( "cpu-a1.bin",   0x08000, 0x08000, 0x8a65bdff )
	ROM_LOAD( "cpu-a2.bin",   0x10000, 0x08000, 0x5a4ae5b9 )
	ROM_LOAD( "cpu-a3.bin",   0x18000, 0x08000, 0x73327606 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-b0.bin",   0x00000, 0x08000, 0xa7b17491 )	/* tiles #2 */
	ROM_LOAD( "cpu-b1.bin",   0x08000, 0x08000, 0xb9709686 )
	ROM_LOAD( "cpu-b2.bin",   0x10000, 0x08000, 0x433b229a )
	ROM_LOAD( "cpu-b3.bin",   0x18000, 0x08000, 0xad89b072 )
ROM_END

ROM_START( rtypepj )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "db_b1.bin",   0x00000, 0x10000, 0xc1865141 )
	ROM_LOAD_V20_ODD ( "db_a1.bin",   0x00000, 0x10000, 0x5ad2bd90 )
	ROM_LOAD_V20_EVEN( "db_b2.bin",   0x20000, 0x10000, 0xb4f6407e )
	ROM_RELOAD_V20_EVEN(              0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "db_a2.bin",   0x20000, 0x10000, 0x6098d86f )
	ROM_RELOAD_V20_ODD (              0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-00.bin",   0x00000, 0x10000, 0xdad53bc0 )	/* sprites */
	ROM_LOAD( "cpu-01.bin",   0x10000, 0x10000, 0xb28d1a60 )
	ROM_LOAD( "cpu-10.bin",   0x20000, 0x10000, 0xd6a66298 )
	ROM_LOAD( "cpu-11.bin",   0x30000, 0x10000, 0xbb182f1a )
	ROM_LOAD( "cpu-20.bin",   0x40000, 0x10000, 0xfc247c8a )
	ROM_LOAD( "cpu-21.bin",   0x50000, 0x10000, 0x5b41f5f3 )
	ROM_LOAD( "cpu-30.bin",   0x60000, 0x10000, 0xeb02a1cb )
	ROM_LOAD( "cpu-31.bin",   0x70000, 0x10000, 0x2bec510a )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-a0.bin",   0x00000, 0x08000, 0x4e212fb0 )	/* tiles #1 */
	ROM_LOAD( "cpu-a1.bin",   0x08000, 0x08000, 0x8a65bdff )
	ROM_LOAD( "cpu-a2.bin",   0x10000, 0x08000, 0x5a4ae5b9 )
	ROM_LOAD( "cpu-a3.bin",   0x18000, 0x08000, 0x73327606 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-b0.bin",   0x00000, 0x08000, 0xa7b17491 )	/* tiles #2 */
	ROM_LOAD( "cpu-b1.bin",   0x08000, 0x08000, 0xb9709686 )
	ROM_LOAD( "cpu-b2.bin",   0x10000, 0x08000, 0x433b229a )
	ROM_LOAD( "cpu-b3.bin",   0x18000, 0x08000, 0xad89b072 )
ROM_END

ROM_START( rtypeu )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "aud-h0.bin",   0x00000, 0x10000, 0x36008a4e )
	ROM_LOAD_V20_ODD ( "aud-l0.bin",   0x00000, 0x10000, 0x4aaa668e )
	ROM_LOAD_V20_EVEN( "aud-h1.bin",   0x20000, 0x10000, 0x7ebb2a53 )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "aud-l1.bin",   0x20000, 0x10000, 0xc28b103b )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-00.bin",   0x00000, 0x10000, 0xdad53bc0 )	/* sprites */
	ROM_LOAD( "cpu-01.bin",   0x10000, 0x10000, 0xb28d1a60 )
	ROM_LOAD( "cpu-10.bin",   0x20000, 0x10000, 0xd6a66298 )
	ROM_LOAD( "cpu-11.bin",   0x30000, 0x10000, 0xbb182f1a )
	ROM_LOAD( "cpu-20.bin",   0x40000, 0x10000, 0xfc247c8a )
	ROM_LOAD( "cpu-21.bin",   0x50000, 0x10000, 0x5b41f5f3 )
	ROM_LOAD( "cpu-30.bin",   0x60000, 0x10000, 0xeb02a1cb )
	ROM_LOAD( "cpu-31.bin",   0x70000, 0x10000, 0x2bec510a )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-a0.bin",   0x00000, 0x08000, 0x4e212fb0 )	/* tiles #1 */
	ROM_LOAD( "cpu-a1.bin",   0x08000, 0x08000, 0x8a65bdff )
	ROM_LOAD( "cpu-a2.bin",   0x10000, 0x08000, 0x5a4ae5b9 )
	ROM_LOAD( "cpu-a3.bin",   0x18000, 0x08000, 0x73327606 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu-b0.bin",   0x00000, 0x08000, 0xa7b17491 )	/* tiles #2 */
	ROM_LOAD( "cpu-b1.bin",   0x08000, 0x08000, 0xb9709686 )
	ROM_LOAD( "cpu-b2.bin",   0x10000, 0x08000, 0x433b229a )
	ROM_LOAD( "cpu-b3.bin",   0x18000, 0x08000, 0xad89b072 )
ROM_END

ROM_START( bchopper )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "c-h0-b.rom",   0x00000, 0x10000, 0xf2feab16 )
	ROM_LOAD_V20_ODD ( "c-l0-b.rom",   0x00000, 0x10000, 0x9f887096 )
	ROM_LOAD_V20_EVEN( "c-h1-b.rom",   0x20000, 0x10000, 0xa995d64f )
	ROM_LOAD_V20_ODD ( "c-l1-b.rom",   0x20000, 0x10000, 0x41dda999 )
	ROM_LOAD_V20_EVEN( "c-h3-b.rom",   0x60000, 0x10000, 0xab9451ca )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "c-l3-b.rom",   0x60000, 0x10000, 0x11562221 )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c-00-a.rom",   0x00000, 0x10000, 0xf6e6e660 )	/* sprites */
	ROM_LOAD( "c-01-b.rom",   0x10000, 0x10000, 0x708cdd37 )
	ROM_LOAD( "c-10-a.rom",   0x20000, 0x10000, 0x292c8520 )
	ROM_LOAD( "c-11-b.rom",   0x30000, 0x10000, 0x20904cf3 )
	ROM_LOAD( "c-20-a.rom",   0x40000, 0x10000, 0x1ab50c23 )
	ROM_LOAD( "c-21-b.rom",   0x50000, 0x10000, 0xc823d34c )
	ROM_LOAD( "c-30-a.rom",   0x60000, 0x10000, 0x11f6c56b )
	ROM_LOAD( "c-31-b.rom",   0x70000, 0x10000, 0x23134ec5 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-a0-b.rom",   0x00000, 0x10000, 0xe46ed7bf )	/* tiles #1 */
	ROM_LOAD( "b-a1-b.rom",   0x10000, 0x10000, 0x590605ff )
	ROM_LOAD( "b-a2-b.rom",   0x20000, 0x10000, 0xf8158226 )
	ROM_LOAD( "b-a3-b.rom",   0x30000, 0x10000, 0x0f07b9b7 )

	ROM_REGION( 0x040000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-b0-.rom",    0x00000, 0x10000, 0xb5b95776 )	/* tiles #2 */
	ROM_LOAD( "b-b1-.rom",    0x10000, 0x10000, 0x74ca16ee )
	ROM_LOAD( "b-b2-.rom",    0x20000, 0x10000, 0xb82cca04 )
	ROM_LOAD( "b-b3-.rom",    0x30000, 0x10000, 0xa7afc920 )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "c-v0-b.rom",   0x00000, 0x10000, 0xd0c27e58 )
ROM_END

ROM_START( mrheli )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "mh-c-h0.bin",  0x00000, 0x10000, 0xe2ca5646 )
	ROM_LOAD_V20_ODD ( "mh-c-l0.bin",  0x00000, 0x10000, 0x643e23cd )
	ROM_LOAD_V20_EVEN( "mh-c-h1.bin",  0x20000, 0x10000, 0x8974e84d )
	ROM_LOAD_V20_ODD ( "mh-c-l1.bin",  0x20000, 0x10000, 0x5f8bda69 )
	ROM_LOAD_V20_EVEN( "mh-c-h3.bin",  0x60000, 0x10000, 0x143f596e )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "mh-c-l3.bin",  0x60000, 0x10000, 0xc0982536 )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mh-c-00.bin",  0x00000, 0x20000, 0xdec4e121 )	/* sprites */
	ROM_LOAD( "mh-c-10.bin",  0x20000, 0x20000, 0x7aaa151e )
	ROM_LOAD( "mh-c-20.bin",  0x40000, 0x20000, 0xeae0de74 )
	ROM_LOAD( "mh-c-30.bin",  0x60000, 0x20000, 0x01d5052f )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mh-b-a0.bin",  0x00000, 0x10000, 0x6a0db256 )	/* tiles #1 */
	ROM_LOAD( "mh-b-a1.bin",  0x10000, 0x10000, 0x14ec9795 )
	ROM_LOAD( "mh-b-a2.bin",  0x20000, 0x10000, 0xdfcb510e )
	ROM_LOAD( "mh-b-a3.bin",  0x30000, 0x10000, 0x957e329b )

	ROM_REGION( 0x040000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b-b0-.rom",    0x00000, 0x10000, 0xb5b95776 )	/* tiles #2 */
	ROM_LOAD( "b-b1-.rom",    0x10000, 0x10000, 0x74ca16ee )
	ROM_LOAD( "b-b2-.rom",    0x20000, 0x10000, 0xb82cca04 )
	ROM_LOAD( "b-b3-.rom",    0x30000, 0x10000, 0xa7afc920 )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "c-v0-b.rom",   0x00000, 0x10000, 0xd0c27e58 )
ROM_END

ROM_START( nspirit )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "nin-c-h0.rom", 0x00000, 0x10000, 0x035692fa )
	ROM_LOAD_V20_ODD ( "nin-c-l0.rom", 0x00000, 0x10000, 0x9a405898 )
	ROM_LOAD_V20_EVEN( "nin-c-h1.rom", 0x20000, 0x10000, 0xcbc10586 )
	ROM_LOAD_V20_ODD ( "nin-c-l1.rom", 0x20000, 0x10000, 0xb75c9a4d )
	ROM_LOAD_V20_EVEN( "nin-c-h2.rom", 0x40000, 0x10000, 0x8ad818fa )
	ROM_LOAD_V20_ODD ( "nin-c-l2.rom", 0x40000, 0x10000, 0xc52ca78c )
	ROM_LOAD_V20_EVEN( "nin-c-h3.rom", 0x60000, 0x10000, 0x501104ef )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "nin-c-l3.rom", 0x60000, 0x10000, 0xfd7408b8 )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nin-r00.rom",  0x00000, 0x20000, 0x5f61d30b )	/* sprites */
	ROM_LOAD( "nin-r10.rom",  0x20000, 0x20000, 0x0caad107 )
	ROM_LOAD( "nin-r20.rom",  0x40000, 0x20000, 0xef3617d3 )
	ROM_LOAD( "nin-r30.rom",  0x60000, 0x20000, 0x175d2a24 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nin-b-a0.rom", 0x00000, 0x10000, 0x63f8f658 )	/* tiles #1 */
	ROM_LOAD( "nin-b-a1.rom", 0x10000, 0x10000, 0x75eb8306 )
	ROM_LOAD( "nin-b-a2.rom", 0x20000, 0x10000, 0xdf532172 )
	ROM_LOAD( "nin-b-a3.rom", 0x30000, 0x10000, 0x4dedd64c )

	ROM_REGION( 0x040000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nin-b0.rom",   0x00000, 0x10000, 0x1b0e08a6 )	/* tiles #2 */
	ROM_LOAD( "nin-b1.rom",   0x10000, 0x10000, 0x728727f0 )
	ROM_LOAD( "nin-b2.rom",   0x20000, 0x10000, 0xf87efd75 )
	ROM_LOAD( "nin-b3.rom",   0x30000, 0x10000, 0x98856cb4 )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "nin-v0.rom",   0x00000, 0x10000, 0xa32e8caf )
ROM_END

ROM_START( nspiritj )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "c-h0",         0x00000, 0x10000, 0x8603fab2 )
	ROM_LOAD_V20_ODD ( "c-l0",         0x00000, 0x10000, 0xe520fa35 )
	ROM_LOAD_V20_EVEN( "nin-c-h1.rom", 0x20000, 0x10000, 0xcbc10586 )
	ROM_LOAD_V20_ODD ( "nin-c-l1.rom", 0x20000, 0x10000, 0xb75c9a4d )
	ROM_LOAD_V20_EVEN( "nin-c-h2.rom", 0x40000, 0x10000, 0x8ad818fa )
	ROM_LOAD_V20_ODD ( "nin-c-l2.rom", 0x40000, 0x10000, 0xc52ca78c )
	ROM_LOAD_V20_EVEN( "c-h3",         0x60000, 0x10000, 0x95b63a61 )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "c-l3",         0x60000, 0x10000, 0xe754a87a )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nin-r00.rom",  0x00000, 0x20000, 0x5f61d30b )	/* sprites */
	ROM_LOAD( "nin-r10.rom",  0x20000, 0x20000, 0x0caad107 )
	ROM_LOAD( "nin-r20.rom",  0x40000, 0x20000, 0xef3617d3 )
	ROM_LOAD( "nin-r30.rom",  0x60000, 0x20000, 0x175d2a24 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nin-b-a0.rom", 0x00000, 0x10000, 0x63f8f658 )	/* tiles #1 */
	ROM_LOAD( "nin-b-a1.rom", 0x10000, 0x10000, 0x75eb8306 )
	ROM_LOAD( "nin-b-a2.rom", 0x20000, 0x10000, 0xdf532172 )
	ROM_LOAD( "nin-b-a3.rom", 0x30000, 0x10000, 0x4dedd64c )

	ROM_REGION( 0x040000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nin-b0.rom",   0x00000, 0x10000, 0x1b0e08a6 )	/* tiles #2 */
	ROM_LOAD( "nin-b1.rom",   0x10000, 0x10000, 0x728727f0 )
	ROM_LOAD( "nin-b2.rom",   0x20000, 0x10000, 0xf87efd75 )
	ROM_LOAD( "nin-b3.rom",   0x30000, 0x10000, 0x98856cb4 )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "nin-v0.rom",   0x00000, 0x10000, 0xa32e8caf )
ROM_END

ROM_START( imgfight )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "if-c-h0.bin",  0x00000, 0x10000, 0x592d2d80 )
	ROM_LOAD_V20_ODD ( "if-c-l0.bin",  0x00000, 0x10000, 0x61f89056 )
	ROM_LOAD_V20_EVEN( "if-c-h3.bin",  0x40000, 0x20000, 0xea030541 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "if-c-l3.bin",  0x40000, 0x20000, 0xc66ae348 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "if-c-00.bin",  0x00000, 0x20000, 0x745e6638 )	/* sprites */
	ROM_LOAD( "if-c-10.bin",  0x20000, 0x20000, 0xb7108449 )
	ROM_LOAD( "if-c-20.bin",  0x40000, 0x20000, 0xaef33cba )
	ROM_LOAD( "if-c-30.bin",  0x60000, 0x20000, 0x1f98e695 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "if-a-a0.bin",  0x00000, 0x10000, 0x34ee2d77 )	/* tiles #1 */
	ROM_LOAD( "if-a-a1.bin",  0x10000, 0x10000, 0x6bd2845b )
	ROM_LOAD( "if-a-a2.bin",  0x20000, 0x10000, 0x090d50e5 )
	ROM_LOAD( "if-a-a3.bin",  0x30000, 0x10000, 0x3a8e3083 )

	ROM_REGION( 0x040000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "if-a-b0.bin",  0x00000, 0x10000, 0xb425c829 )	/* tiles #2 */
	ROM_LOAD( "if-a-b1.bin",  0x10000, 0x10000, 0xe9bfe23e )
	ROM_LOAD( "if-a-b2.bin",  0x20000, 0x10000, 0x256e50f2 )
	ROM_LOAD( "if-a-b3.bin",  0x30000, 0x10000, 0x4c682785 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "if-c-v0.bin",  0x00000, 0x10000, 0xcb64a194 )
	ROM_LOAD( "if-c-v1.bin",  0x10000, 0x10000, 0x45b68bf5 )
ROM_END

ROM_START( loht )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "tom_c-h0.rom", 0x00000, 0x20000, 0xa63204b6 )
	ROM_LOAD_V20_ODD ( "tom_c-l0.rom", 0x00000, 0x20000, 0xe788002f )
	ROM_LOAD_V20_EVEN( "tom_c-h3.rom", 0x40000, 0x20000, 0x714778b5 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "tom_c-l3.rom", 0x40000, 0x20000, 0x2f049b03 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tom_m53.rom",  0x00000, 0x20000, 0x0b83265f )	/* sprites */
	ROM_LOAD( "tom_m51.rom",  0x20000, 0x20000, 0x8ec5f6f3 )
	ROM_LOAD( "tom_m49.rom",  0x40000, 0x20000, 0xa41d3bfd )
	ROM_LOAD( "tom_m47.rom",  0x60000, 0x20000, 0x9d81a25b )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tom_m21.rom",  0x00000, 0x10000, 0x3ca3e771 )	/* tiles #1 */
	ROM_LOAD( "tom_m22.rom",  0x10000, 0x10000, 0x7a05ee2f )
	ROM_LOAD( "tom_m20.rom",  0x20000, 0x10000, 0x79aa2335 )
	ROM_LOAD( "tom_m23.rom",  0x30000, 0x10000, 0x789e8b24 )

	ROM_REGION( 0x040000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tom_m26.rom",  0x00000, 0x10000, 0x44626bf6 )	/* tiles #2 */
	ROM_LOAD( "tom_m27.rom",  0x10000, 0x10000, 0x464952cf )
	ROM_LOAD( "tom_m25.rom",  0x20000, 0x10000, 0x3db9b2c7 )
	ROM_LOAD( "tom_m24.rom",  0x30000, 0x10000, 0xf01fe899 )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "tom_m44.rom",  0x00000, 0x10000, 0x3ed51d1f )
ROM_END

ROM_START( xmultipl )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ch3.h3",       0x00000, 0x20000, 0x20685021 )
	ROM_LOAD_V20_ODD ( "cl3.l3",       0x00000, 0x20000, 0x93fdd200 )
	ROM_LOAD_V20_EVEN( "ch0.h0",       0x40000, 0x10000, 0x9438dd8a )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "cl0.l0",       0x40000, 0x10000, 0x06a9e213 )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "t44.00",       0x00000, 0x20000, 0xdb45186e )	/* sprites */
	ROM_LOAD( "t45.01",       0x20000, 0x20000, 0x4d0764d4 )
	ROM_LOAD( "t46.10",       0x40000, 0x20000, 0xf0c465a4 )
	ROM_LOAD( "t47.11",       0x60000, 0x20000, 0x1263b24b )
	ROM_LOAD( "t48.20",       0x80000, 0x20000, 0x4129944f )
	ROM_LOAD( "t49.21",       0xa0000, 0x20000, 0x2346e6f9 )
	ROM_LOAD( "t50.30",       0xc0000, 0x20000, 0xe322543e )
	ROM_LOAD( "t51.31",       0xe0000, 0x20000, 0x229bf7b1 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "t53.a0",       0x00000, 0x20000, 0x1a082494 )	/* tiles #1 */
	ROM_LOAD( "t54.a1",       0x20000, 0x20000, 0x076c16c5 )
	ROM_LOAD( "t55.a2",       0x40000, 0x20000, 0x25d877a5 )
	ROM_LOAD( "t56.a3",       0x60000, 0x20000, 0x5b1213f5 )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "t57.b0",       0x00000, 0x20000, 0x0a84e0c7 )	/* tiles #2 */
	ROM_LOAD( "t58.b1",       0x20000, 0x20000, 0xa874121d )
	ROM_LOAD( "t59.b2",       0x40000, 0x20000, 0x69deb990 )
	ROM_LOAD( "t60.b3",       0x60000, 0x20000, 0x14c69f99 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "t52.v0",       0x00000, 0x20000, 0x2db1bd80 )
ROM_END

ROM_START( dbreed )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "db_c-h3.rom",  0x00000, 0x20000, 0x4bf3063c )
	ROM_LOAD_V20_ODD ( "db_c-l3.rom",  0x00000, 0x20000, 0xe4b89b79 )
	ROM_LOAD_V20_EVEN( "db_c-h0.rom",  0x60000, 0x10000, 0x5aa79fb2 )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "db_c-l0.rom",  0x60000, 0x10000, 0xed0f5e06 )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "db_k800m.rom", 0x00000, 0x20000, 0xc027a8cf )	/* sprites */
	ROM_LOAD( "db_k801m.rom", 0x20000, 0x20000, 0x093faf33 )
	ROM_LOAD( "db_k802m.rom", 0x40000, 0x20000, 0x055b4c59 )
	ROM_LOAD( "db_k803m.rom", 0x60000, 0x20000, 0x8ed63922 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "db_a0m.rom",   0x00000, 0x20000, 0x4c83e92e )	/* tiles #1 */
	ROM_LOAD( "db_a1m.rom",   0x20000, 0x20000, 0x835ef268 )
	ROM_LOAD( "db_a2m.rom",   0x40000, 0x20000, 0x5117f114 )
	ROM_LOAD( "db_a3m.rom",   0x60000, 0x20000, 0x8eb0c978 )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "db_b0m.rom",   0x00000, 0x20000, 0x4c83e92e )	/* tiles #2 */
	ROM_LOAD( "db_b1m.rom",   0x20000, 0x20000, 0x835ef268 )
	ROM_LOAD( "db_b2m.rom",   0x40000, 0x20000, 0x5117f114 )
	ROM_LOAD( "db_b3m.rom",   0x60000, 0x20000, 0x8eb0c978 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "db_c-v0.rom",  0x00000, 0x20000, 0x312f7282 )
ROM_END

ROM_START( rtype2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ic54.8d",      0x00000, 0x20000, 0xd8ece6f4 )
	ROM_LOAD_V20_ODD ( "ic60.9d",      0x00000, 0x20000, 0x32cfb2e4 )
	ROM_LOAD_V20_EVEN( "ic53.8b",      0x40000, 0x20000, 0x4f6e9b15 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "ic59.9b",      0x40000, 0x20000, 0x0fd123bf )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ic17.4f",      0x0000, 0x10000, 0x73ffecb4 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic31.6l",      0x00000, 0x20000, 0x2cd8f913 )	/* sprites */
	ROM_LOAD( "ic21.4l",      0x20000, 0x20000, 0x5033066d )
	ROM_LOAD( "ic32.6m",      0x40000, 0x20000, 0xec3a0450 )
	ROM_LOAD( "ic22.4m",      0x60000, 0x20000, 0xdb6176fc )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic50.7s",      0x00000, 0x20000, 0xf3f8736e )	/* tiles */
	ROM_LOAD( "ic51.7u",      0x20000, 0x20000, 0xb4c543af )
	ROM_LOAD( "ic56.8s",      0x40000, 0x20000, 0x4cb80d66 )
	ROM_LOAD( "ic57.8u",      0x60000, 0x20000, 0xbee128e0 )
	ROM_LOAD( "ic65.9r",      0x80000, 0x20000, 0x2dc9c71a )
	ROM_LOAD( "ic66.9u",      0xa0000, 0x20000, 0x7533c428 )
	ROM_LOAD( "ic63.9m",      0xc0000, 0x20000, 0xa6ad67f2 )
	ROM_LOAD( "ic64.9p",      0xe0000, 0x20000, 0x3686d555 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "ic14.4c",      0x00000, 0x20000, 0x637172d5 )
ROM_END

ROM_START( rtype2j )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "rt2-a-h0.54",  0x00000, 0x20000, 0x7857ccf6 )
	ROM_LOAD_V20_ODD ( "rt2-a-l0.60",  0x00000, 0x20000, 0xcb22cd6e )
	ROM_LOAD_V20_EVEN( "rt2-a-h1.53",  0x40000, 0x20000, 0x49e75d28 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "rt2-a-l1.59",  0x40000, 0x20000, 0x12ec1676 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ic17.4f",      0x0000, 0x10000, 0x73ffecb4 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic31.6l",      0x00000, 0x20000, 0x2cd8f913 )	/* sprites */
	ROM_LOAD( "ic21.4l",      0x20000, 0x20000, 0x5033066d )
	ROM_LOAD( "ic32.6m",      0x40000, 0x20000, 0xec3a0450 )
	ROM_LOAD( "ic22.4m",      0x60000, 0x20000, 0xdb6176fc )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic50.7s",      0x00000, 0x20000, 0xf3f8736e )	/* tiles */
	ROM_LOAD( "ic51.7u",      0x20000, 0x20000, 0xb4c543af )
	ROM_LOAD( "ic56.8s",      0x40000, 0x20000, 0x4cb80d66 )
	ROM_LOAD( "ic57.8u",      0x60000, 0x20000, 0xbee128e0 )
	ROM_LOAD( "ic65.9r",      0x80000, 0x20000, 0x2dc9c71a )
	ROM_LOAD( "ic66.9u",      0xa0000, 0x20000, 0x7533c428 )
	ROM_LOAD( "ic63.9m",      0xc0000, 0x20000, 0xa6ad67f2 )
	ROM_LOAD( "ic64.9p",      0xe0000, 0x20000, 0x3686d555 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "ic14.4c",      0x00000, 0x20000, 0x637172d5 )
ROM_END

ROM_START( majtitle )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "mt_m0.bin",    0x00000, 0x20000, 0xb9682c70 )
	ROM_LOAD_V20_ODD ( "mt_l0.bin",    0x00000, 0x20000, 0x702c9fd6 )
	ROM_LOAD_V20_EVEN( "mt_m1.bin",    0x40000, 0x20000, 0xd9e97c30 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "mt_l1.bin",    0x40000, 0x20000, 0x8dbd91b5 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "mt_sp.bin",    0x0000, 0x10000, 0xe44260a9 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mt_n0.bin",    0x00000, 0x40000, 0x5618cddc )	/* sprites #1 */
	ROM_LOAD( "mt_n1.bin",    0x40000, 0x40000, 0x483b873b )
	ROM_LOAD( "mt_n2.bin",    0x80000, 0x40000, 0x4f5d665b )
	ROM_LOAD( "mt_n3.bin",    0xc0000, 0x40000, 0x83571549 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mt_c0.bin",    0x00000, 0x20000, 0x780e7a02 )	/* tiles */
	ROM_LOAD( "mt_c1.bin",    0x20000, 0x20000, 0x45ad1381 )
	ROM_LOAD( "mt_c2.bin",    0x40000, 0x20000, 0x5df5856d )
	ROM_LOAD( "mt_c3.bin",    0x60000, 0x20000, 0xf5316cc8 )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mt_f0.bin",    0x00000, 0x20000, 0x2d5e05d5 )	/* sprites #2 */
	ROM_LOAD( "mt_f1.bin",    0x20000, 0x20000, 0xc68cd65f )
	ROM_LOAD( "mt_f2.bin",    0x40000, 0x20000, 0xa71feb2d )
	ROM_LOAD( "mt_f3.bin",    0x60000, 0x20000, 0x179f7562 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "mt_vo.bin",    0x00000, 0x20000, 0xeb24bb2c )
ROM_END

ROM_START( hharry )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "a-h0-v.rom",   0x00000, 0x20000, 0xc52802a5 )
	ROM_LOAD_V20_ODD ( "a-l0-v.rom",   0x00000, 0x20000, 0xf463074c )
	ROM_LOAD_V20_EVEN( "a-h1-0.rom",   0x60000, 0x10000, 0x3ae21335 )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "a-l1-0.rom",   0x60000, 0x10000, 0xbc6ac5f9 )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "a-sp-0.rom",   0x0000, 0x10000, 0x80e210e7 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hh_00.rom",    0x00000, 0x20000, 0xec5127ef )	/* sprites */
	ROM_LOAD( "hh_10.rom",    0x20000, 0x20000, 0xdef65294 )
	ROM_LOAD( "hh_20.rom",    0x40000, 0x20000, 0xbb0d6ad4 )
	ROM_LOAD( "hh_30.rom",    0x60000, 0x20000, 0x4351044e )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hh_a0.rom",    0x00000, 0x20000, 0xc577ba5f )	/* tiles */
	ROM_LOAD( "hh_a1.rom",    0x20000, 0x20000, 0x429d12ab )
	ROM_LOAD( "hh_a2.rom",    0x40000, 0x20000, 0xb5b163b0 )
	ROM_LOAD( "hh_a3.rom",    0x60000, 0x20000, 0x8ef566a1 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "a-v0-0.rom",   0x00000, 0x20000, 0xfaaacaff )
ROM_END

ROM_START( hharryu )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "a-ho-u.8d",    0x00000, 0x20000, 0xede7f755 )
	ROM_LOAD_V20_ODD ( "a-lo-u.9d",    0x00000, 0x20000, 0xdf0726ae )
	ROM_LOAD_V20_EVEN( "a-h1-f.8b",    0x60000, 0x10000, 0x31b741c5 )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "a-l1-f.9b",    0x60000, 0x10000, 0xb23e966c )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "a-sp-0.rom",   0x0000, 0x10000, 0x80e210e7 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hh_00.rom",    0x00000, 0x20000, 0xec5127ef )	/* sprites */
	ROM_LOAD( "hh_10.rom",    0x20000, 0x20000, 0xdef65294 )
	ROM_LOAD( "hh_20.rom",    0x40000, 0x20000, 0xbb0d6ad4 )
	ROM_LOAD( "hh_30.rom",    0x60000, 0x20000, 0x4351044e )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hh_a0.rom",    0x00000, 0x20000, 0xc577ba5f )	/* tiles */
	ROM_LOAD( "hh_a1.rom",    0x20000, 0x20000, 0x429d12ab )
	ROM_LOAD( "hh_a2.rom",    0x40000, 0x20000, 0xb5b163b0 )
	ROM_LOAD( "hh_a3.rom",    0x60000, 0x20000, 0x8ef566a1 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "a-v0-0.rom",   0x00000, 0x20000, 0xfaaacaff )
ROM_END

ROM_START( dkgensan )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "gen-a-h0.bin", 0x00000, 0x20000, 0x07a45f6d )
	ROM_LOAD_V20_ODD ( "gen-a-l0.bin", 0x00000, 0x20000, 0x46478fea )
	ROM_LOAD_V20_EVEN( "gen-a-h1.bin", 0x60000, 0x10000, 0x54e5b73c )
	ROM_RELOAD_V20_EVEN(               0xe0000, 0x10000 )
	ROM_LOAD_V20_ODD ( "gen-a-l1.bin", 0x60000, 0x10000, 0x894f8a9f )
	ROM_RELOAD_V20_ODD (               0xe0000, 0x10000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "gen-a-sp.bin", 0x0000, 0x10000, 0xe83cfc2c )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hh_00.rom",    0x00000, 0x20000, 0xec5127ef )	/* sprites */
	ROM_LOAD( "hh_10.rom",    0x20000, 0x20000, 0xdef65294 )
	ROM_LOAD( "hh_20.rom",    0x40000, 0x20000, 0xbb0d6ad4 )
	ROM_LOAD( "hh_30.rom",    0x60000, 0x20000, 0x4351044e )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hh_a0.rom",    0x00000, 0x20000, 0xc577ba5f )	/* tiles */
	ROM_LOAD( "hh_a1.rom",    0x20000, 0x20000, 0x429d12ab )
	ROM_LOAD( "hh_a2.rom",    0x40000, 0x20000, 0xb5b163b0 )
	ROM_LOAD( "hh_a3.rom",    0x60000, 0x20000, 0x8ef566a1 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "gen-vo.bin",   0x00000, 0x20000, 0xd8595c66 )
ROM_END

ROM_START( kengo )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ken_d-h0.rom", 0x00000, 0x20000, 0xf4ddeea5 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "ken_d-l0.rom", 0x00000, 0x20000, 0x04dc0f81 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ken_d-sp.rom", 0x0000, 0x10000, 0x233ca1cf )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ken_m21.rom",  0x00000, 0x20000, 0xd7722f87 )	/* sprites */
	ROM_LOAD( "ken_m22.rom",  0x20000, 0x20000, 0xa00dac85 )
	ROM_LOAD( "ken_m31.rom",  0x40000, 0x20000, 0xe00b95a6 )
	ROM_LOAD( "ken_m32.rom",  0x60000, 0x20000, 0x30a844c4 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ken_m51.rom",  0x00000, 0x20000, 0x1646cf4f )	/* tiles */
	ROM_LOAD( "ken_m57.rom",  0x20000, 0x20000, 0xa9f88d90 )
	ROM_LOAD( "ken_m66.rom",  0x40000, 0x20000, 0xe9d17645 )
	ROM_LOAD( "ken_m64.rom",  0x60000, 0x20000, 0xdf46709b )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "ken_m14.rom",  0x00000, 0x20000, 0x6651e9b7 )
ROM_END

ROM_START( poundfor )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ppa-h0-b.bin", 0x00000, 0x20000, 0x50d4a2d8 )
	ROM_LOAD_V20_ODD ( "ppa-l0-b.bin", 0x00000, 0x20000, 0xbd997942 )
	ROM_LOAD_V20_EVEN( "ppa-h1.9f",    0x40000, 0x20000, 0xf6c82f48 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "ppa-l1.9c",    0x40000, 0x20000, 0x5b07b087 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ppa-sp.4j",    0x0000, 0x10000, 0x3f458a5b )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ppb-n0.bin",   0x00000, 0x40000, 0x951a41f8 )	/* sprites */
	ROM_LOAD( "ppb-n1.bin",   0x40000, 0x40000, 0xc609b7f2 )
	ROM_LOAD( "ppb-n2.bin",   0x80000, 0x40000, 0x318c0b5f )
	ROM_LOAD( "ppb-n3.bin",   0xc0000, 0x40000, 0x93dc9490 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ppa-g00.bin",  0x00000, 0x20000, 0x8a88a174 )	/* tiles */
	ROM_LOAD( "ppa-g10.bin",  0x20000, 0x20000, 0xe48a66ac )
	ROM_LOAD( "ppa-g20.bin",  0x40000, 0x20000, 0x12b93e79 )
	ROM_LOAD( "ppa-g30.bin",  0x60000, 0x20000, 0xfaa39aee )

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "ppa-v0.bin",   0x00000, 0x40000, 0x03321664 )
ROM_END

ROM_START( poundfou )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ppa-ho-a.9e",  0x00000, 0x20000, 0xff4c83a4 )
	ROM_LOAD_V20_ODD ( "ppa-lo-a.9d",  0x00000, 0x20000, 0x3374ce8f )
	ROM_LOAD_V20_EVEN( "ppa-h1.9f",    0x40000, 0x20000, 0xf6c82f48 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "ppa-l1.9c",    0x40000, 0x20000, 0x5b07b087 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ppa-sp.4j",    0x0000, 0x10000, 0x3f458a5b )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ppb-n0.bin",   0x00000, 0x40000, 0x951a41f8 )	/* sprites */
	ROM_LOAD( "ppb-n1.bin",   0x40000, 0x40000, 0xc609b7f2 )
	ROM_LOAD( "ppb-n2.bin",   0x80000, 0x40000, 0x318c0b5f )
	ROM_LOAD( "ppb-n3.bin",   0xc0000, 0x40000, 0x93dc9490 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ppa-g00.bin",  0x00000, 0x20000, 0x8a88a174 )	/* tiles */
	ROM_LOAD( "ppa-g10.bin",  0x20000, 0x20000, 0xe48a66ac )
	ROM_LOAD( "ppa-g20.bin",  0x40000, 0x20000, 0x12b93e79 )
	ROM_LOAD( "ppa-g30.bin",  0x60000, 0x20000, 0xfaa39aee )

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "ppa-v0.bin",   0x00000, 0x40000, 0x03321664 )
ROM_END

ROM_START( airduel )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "ad-c-h0.bin",  0x00000, 0x20000, 0x12140276 )
	ROM_LOAD_V20_ODD ( "ad-c-l0.bin",  0x00000, 0x20000, 0x4ac0b91d )
	ROM_LOAD_V20_EVEN( "ad-c-h3.bin",  0x40000, 0x20000, 0x9f7cfca3 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "ad-c-l3.bin",  0x40000, 0x20000, 0x9dd343f7 )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ad-00.bin",    0x00000, 0x20000, 0x2f0d599b )	/* sprites */
	ROM_LOAD( "ad-10.bin",    0x20000, 0x20000, 0x9865856b )
	ROM_LOAD( "ad-20.bin",    0x40000, 0x20000, 0xd392aef2 )
	ROM_LOAD( "ad-30.bin",    0x60000, 0x20000, 0x923240c3 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ad-a0.bin",    0x00000, 0x20000, 0xce134b47 )	/* tiles #1 */
	ROM_LOAD( "ad-a1.bin",    0x20000, 0x20000, 0x097fd853 )
	ROM_LOAD( "ad-a2.bin",    0x40000, 0x20000, 0x6a94c1b9 )
	ROM_LOAD( "ad-a3.bin",    0x60000, 0x20000, 0x6637c349 )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ad-b0.bin",    0x00000, 0x20000, 0xce134b47 )	/* tiles #2 */
	ROM_LOAD( "ad-b1.bin",    0x20000, 0x20000, 0x097fd853 )
	ROM_LOAD( "ad-b2.bin",    0x40000, 0x20000, 0x6a94c1b9 )
	ROM_LOAD( "ad-b3.bin",    0x60000, 0x20000, 0x6637c349 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "ad-v0.bin",    0x00000, 0x20000, 0x339f474d )
ROM_END

ROM_START( gallop )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "cc-c-h0.bin",  0x00000, 0x20000, 0x2217dcd0 )
	ROM_LOAD_V20_ODD ( "cc-c-l0.bin",  0x00000, 0x20000, 0xff39d7fb )
	ROM_LOAD_V20_EVEN( "cc-c-h3.bin",  0x40000, 0x20000, 0x9b2bbab9 )
	ROM_RELOAD_V20_EVEN(               0xc0000, 0x20000 )
	ROM_LOAD_V20_ODD ( "cc-c-l3.bin",  0x40000, 0x20000, 0xacd3278e )
	ROM_RELOAD_V20_ODD (               0xc0000, 0x20000 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	/* no ROM, program will be copied by the main CPU */

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc-c-00.bin",  0x00000, 0x20000, 0x9d99deaa )	/* sprites */
	ROM_LOAD( "cc-c-10.bin",  0x20000, 0x20000, 0x7eb083ed )
	ROM_LOAD( "cc-c-20.bin",  0x40000, 0x20000, 0x9421489e )
	ROM_LOAD( "cc-c-30.bin",  0x60000, 0x20000, 0x920ec735 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc-b-a0.bin",  0x00000, 0x10000, 0xa33472bd )	/* tiles #1 */
	ROM_LOAD( "cc-b-a1.bin",  0x10000, 0x10000, 0x118b1f2d )
	ROM_LOAD( "cc-b-a2.bin",  0x20000, 0x10000, 0x83cebf48 )
	ROM_LOAD( "cc-b-a3.bin",  0x30000, 0x10000, 0x572903fc )

	ROM_REGION( 0x040000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cc-b-b0.bin",  0x00000, 0x10000, 0x0df5b439 )	/* tiles #2 */
	ROM_LOAD( "cc-b-b1.bin",  0x10000, 0x10000, 0x010b778f )
	ROM_LOAD( "cc-b-b2.bin",  0x20000, 0x10000, 0xbda9f6fb )
	ROM_LOAD( "cc-b-b3.bin",  0x30000, 0x10000, 0xd361ba3f )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "cc-c-v0.bin",  0x00000, 0x20000, 0x6247bade )
ROM_END



GAMEX( 1987, rtype,    0,        rtype,    rtype,    0,        ROT0,   "Irem", "R-Type (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1987, rtypepj,  rtype,    rtype,    rtypep,   0,        ROT0,   "Irem", "R-Type (Japan prototype)", GAME_NO_COCKTAIL )
GAMEX( 1987, rtypeu,   rtype,    rtype,    rtype,    0,        ROT0,   "Irem (Nintendo of America license)", "R-Type (US)", GAME_NO_COCKTAIL )
GAMEX( 1987, bchopper, 0,        m72,      bchopper, bchopper, ROT0,   "Irem", "Battle Chopper", GAME_NO_COCKTAIL )
GAMEX( 1987, mrheli,   bchopper, m72,      bchopper, mrheli,   ROT0,   "Irem", "Mr. HELI no Dai-Bouken", GAME_NO_COCKTAIL )
GAMEX( 1988, nspirit,  0,        m72,      nspirit,  nspirit,  ROT0,   "Irem", "Ninja Spirit", GAME_NO_COCKTAIL )
GAMEX( 1988, nspiritj, nspirit,  m72,      nspirit,  nspiritj, ROT0,   "Irem", "Saigo no Nindou (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1988, imgfight, 0,        m72,      imgfight, imgfight, ROT270, "Irem", "Image Fight (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1989, loht,     0,        m72,      loht,     loht,     ROT0,   "Irem", "Legend of Hero Tonma", GAME_NO_COCKTAIL )
GAMEX( 1989, xmultipl, 0,        xmultipl, xmultipl, xmultipl, ROT0,   "Irem", "X Multiply (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1989, dbreed,   0,        dbreed,   dbreed,   dbreed,   ROT0,   "Irem", "Dragon Breed", GAME_NO_COCKTAIL )
GAMEX( 1989, rtype2,   0,        rtype2,   rtype2,   0,        ROT0,   "Irem", "R-Type II", GAME_NO_COCKTAIL )
GAMEX( 1989, rtype2j,  rtype2,   rtype2,   rtype2,   0,        ROT0,   "Irem", "R-Type II (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1990, majtitle, 0,        majtitle, rtype2,   0,        ROT0,   "Irem", "Major Title (Japan)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1990, hharry,   0,        hharry,   hharry,   0,        ROT0,   "Irem", "Hammerin' Harry (World)", GAME_NO_COCKTAIL )
GAMEX( 1990, hharryu,  hharry,   hharryu,  hharry,   0,        ROT0,   "Irem America", "Hammerin' Harry (US)", GAME_NO_COCKTAIL )
GAMEX( 1990, dkgensan, hharry,   hharryu,  hharry,   0,        ROT0,   "Irem", "Daiku no Gensan (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1991, kengo,    0,        hharry,   hharry,   0,        ROT0,   "Irem", "Ken-Go", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1990, poundfor, 0,        poundfor, poundfor, 0,        ROT270, "Irem", "Pound for Pound (World)", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1990, poundfou, poundfor, poundfor, poundfor, 0,        ROT270, "Irem America", "Pound for Pound (US)", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1990, airduel,  0,        m72,      airduel,  airduel,  ROT270, "Irem", "Air Duel (Japan)", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1991, gallop,   0,        m72,      gallop,   gallop,   ROT0,   "Irem", "Gallop - Armed police Unit (Japan)", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
