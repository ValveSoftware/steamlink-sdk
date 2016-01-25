#include "../machine/btime.cpp"
#include "../vidhrdw/btime.cpp"

/***************************************************************************

Burger Time

driver by Zsolt Vasvari

hardware description:

Actually Lock'n'Chase is (C)1981 while Burger Time is (C)1982, so it might
be more accurate to say 'Lock'n'Chase hardware'.

The bootleg called Cook Race runs on hardware similar but different. The fact
that it addresses the program ROMs in the range 0500-3fff instead of the usual
c000-ffff makes me suspect that it is a bootleg of the *tape system* version.
Little is known about that system, but it is quite likely that it would have
RAM in the range 0000-3fff and load the program there from tape.


This hardware is pretty straightforward, but has a couple of interesting
twists. There are two ports to the video and color RAMs, one normal access,
and one with X and Y coordinates swapped. The sprite RAM occupies the
first row of the swapped area, so it appears in the regular video RAM as
the first column of on the left side.

These games don't have VBLANK interrupts, but instead an IRQ or NMI
(depending on the particular board) is generated when a coin is inserted.

Some of the games also have a background playfield which, in the
case of Bump 'n' Jump and Zoar, can be scrolled vertically.

These boards use two 8910's for sound, controlled by a dedicated 6502. The
main processor triggers an IRQ request when writing a command to the sound
CPU.

Main clock: XTAL = 12 MHz
Horizontal video frequency: HSYNC = XTAL/768?? = 15.625 kHz ??
Video frequency: VSYNC = HSYNC/272 = 57.44 Hz ?
VBlank duration: 1/VSYNC * (24/272) = 1536 us ?


Note on Lock'n'Chase:

The watchdog test prints "WATCHDOG TEST ER". Just by looking at the code,
I can't see how it could print anything else, there is only one path it
can take. Should the game reset????

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"

extern unsigned char *lnc_charbank;
extern unsigned char *bnj_backgroundram;
extern size_t bnj_backgroundram_size;
extern unsigned char *zoar_scrollram;
extern unsigned char *deco_charram;

void btime_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void lnc_vh_convert_color_prom  (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

void lnc_init_machine (void);

int  btime_vh_start (void);
int  bnj_vh_start (void);

void bnj_vh_stop (void);

void btime_vh_screenrefresh   (struct osd_bitmap *bitmap,int full_refresh);
void cookrace_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void bnj_vh_screenrefresh     (struct osd_bitmap *bitmap,int full_refresh);
void lnc_vh_screenrefresh     (struct osd_bitmap *bitmap,int full_refresh);
void zoar_vh_screenrefresh    (struct osd_bitmap *bitmap,int full_refresh);
void disco_vh_screenrefresh   (struct osd_bitmap *bitmap,int full_refresh);
void eggs_vh_screenrefresh    (struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( btime_paletteram_w );
WRITE_HANDLER( bnj_background_w );
WRITE_HANDLER( bnj_scroll1_w );
WRITE_HANDLER( bnj_scroll2_w );
READ_HANDLER( btime_mirrorvideoram_r );
WRITE_HANDLER( btime_mirrorvideoram_w );
READ_HANDLER( btime_mirrorcolorram_r );
WRITE_HANDLER( btime_mirrorcolorram_w );
WRITE_HANDLER( lnc_videoram_w );
WRITE_HANDLER( lnc_mirrorvideoram_w );
WRITE_HANDLER( deco_charram_w );

WRITE_HANDLER( zoar_video_control_w );
WRITE_HANDLER( btime_video_control_w );
WRITE_HANDLER( bnj_video_control_w );
WRITE_HANDLER( lnc_video_control_w );
WRITE_HANDLER( disco_video_control_w );

int lnc_sound_interrupt(void);

static WRITE_HANDLER( sound_command_w );

READ_HANDLER( mmonkey_protection_r );
WRITE_HANDLER( mmonkey_protection_w );


INLINE int swap_bits_5_6(int data)
{
	return (data & 0x9f) | ((data & 0x20) << 1) | ((data & 0x40) >> 1);
}


static void btime_decrypt(void)
{
	int A,A1;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	/* the encryption is a simple bit rotation: 76543210 -> 65342710, but */
	/* with a catch: it is only applied if the previous instruction did a */
	/* memory write. Also, only opcodes at addresses with this bit pattern: */
	/* xxxx xxx1 xxxx x1xx are encrypted. */

	/* get the address of the next opcode */
	A = cpu_get_pc();

	/* however if the previous instruction was JSR (which caused a write to */
	/* the stack), fetch the address of the next instruction. */
	A1 = cpu_getpreviouspc();
	if (rom[A1 + diff] == 0x20)	/* JSR $xxxx */
		A = cpu_readop_arg(A1+1) + 256 * cpu_readop_arg(A1+2);

	/* If the address of the next instruction is xxxx xxx1 xxxx x1xx, decode it. */
	if ((A & 0x0104) == 0x0104)
	{
		/* 76543210 -> 65342710 bit rotation */
		rom[A + diff] = (rom[A] & 0x13) | ((rom[A] & 0x80) >> 5) | ((rom[A] & 0x64) << 1)
			   | ((rom[A] & 0x08) << 2);
	}
}

static WRITE_HANDLER( lnc_w )
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;

	if      (offset <= 0x3bff)                       ;
	else if (offset >= 0x3c00 && offset <= 0x3fff) { lnc_videoram_w(offset - 0x3c00,data); return; }
	else if (offset >= 0x7c00 && offset <= 0x7fff) { lnc_mirrorvideoram_w(offset - 0x7c00,data); return; }
	else if (offset == 0x8000)                     { return; }  /* MWA_NOP */
	else if (offset == 0x8001)                     { lnc_video_control_w(0,data); return; }
	else if (offset == 0x8003)                       ;
	else if (offset == 0x9000)                     { return; }  /* MWA_NOP */
	else if (offset == 0x9002)                     { sound_command_w(0,data); return; }
	//else if (offset >= 0xb000 && offset <= 0xb1ff)   ;
	//else logerror("CPU #%d PC %04x: warning - write %02x to unmapped memory address %04x\n",cpu_getactivecpu(),cpu_get_pc(),data,offset);

	rom[offset] = data;

	/* Swap bits 5 & 6 for opcodes */
	rom[offset+diff] = swap_bits_5_6(data);
}

static WRITE_HANDLER( mmonkey_w )
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;

	if      (offset <= 0x3bff)                       ;
	else if (offset >= 0x3c00 && offset <= 0x3fff) { lnc_videoram_w(offset - 0x3c00,data); return; }
	else if (offset >= 0x7c00 && offset <= 0x7fff) { lnc_mirrorvideoram_w(offset - 0x7c00,data); return; }
	else if (offset == 0x8001)                     { lnc_video_control_w(0,data); return; }
	else if (offset == 0x8003)                       ;
	else if (offset == 0x9000)                     { return; }  /* MWA_NOP */
	else if (offset == 0x9002)                     { sound_command_w(0,data); return; }
	else if (offset >= 0xb000 && offset <= 0xbfff) { mmonkey_protection_w(offset - 0xb000, data); return; }
	//else logerror("CPU #%d PC %04x: warning - write %02x to unmapped memory address %04x\n",cpu_getactivecpu(),cpu_get_pc(),data,offset);

	rom[offset] = data;

	/* Swap bits 5 & 6 for opcodes */
	rom[offset+diff] = swap_bits_5_6(data);
}

static WRITE_HANDLER( btime_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	if      (offset <= 0x07ff)                     RAM[offset] = data;
	else if (offset >= 0x0c00 && offset <= 0x0c0f) btime_paletteram_w(offset - 0x0c00,data);
	else if (offset >= 0x1000 && offset <= 0x13ff) videoram_w(offset - 0x1000,data);
	else if (offset >= 0x1400 && offset <= 0x17ff) colorram_w(offset - 0x1400,data);
	else if (offset >= 0x1800 && offset <= 0x1bff) btime_mirrorvideoram_w(offset - 0x1800,data);
	else if (offset >= 0x1c00 && offset <= 0x1fff) btime_mirrorcolorram_w(offset - 0x1c00,data);
	else if (offset == 0x4002)                     btime_video_control_w(0,data);
	else if (offset == 0x4003)                     sound_command_w(0,data);
	else if (offset == 0x4004)                     bnj_scroll1_w(0,data);
	//else logerror("CPU #%d PC %04x: warning - write %02x to unmapped memory address %04x\n",cpu_getactivecpu(),cpu_get_pc(),data,offset);

	btime_decrypt();
}

static WRITE_HANDLER( zoar_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	if      (offset <= 0x07ff) 					   RAM[offset] = data;
	else if (offset >= 0x8000 && offset <= 0x83ff) videoram_w(offset - 0x8000,data);
	else if (offset >= 0x8400 && offset <= 0x87ff) colorram_w(offset - 0x8400,data);
	else if (offset >= 0x8800 && offset <= 0x8bff) btime_mirrorvideoram_w(offset - 0x8800,data);
	else if (offset >= 0x8c00 && offset <= 0x8fff) btime_mirrorcolorram_w(offset - 0x8c00,data);
	else if (offset == 0x9000)					   zoar_video_control_w(0, data);
	else if (offset >= 0x9800 && offset <= 0x9803) RAM[offset] = data;
	else if (offset == 0x9804)                     bnj_scroll2_w(0,data);
	else if (offset == 0x9805)                     bnj_scroll1_w(0,data);
	else if (offset == 0x9806)                     sound_command_w(0,data);
	//else logerror("CPU #%d PC %04x: warning - write %02x to unmapped memory address %04x\n",cpu_getactivecpu(),cpu_get_pc(),data,offset);

	btime_decrypt();

}

static WRITE_HANDLER( disco_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	if      (offset <= 0x04ff)                     RAM[offset] = data;
	else if (offset >= 0x2000 && offset <= 0x7fff) deco_charram_w(offset - 0x2000,data);
	else if (offset >= 0x8000 && offset <= 0x83ff) videoram_w(offset - 0x8000,data);
	else if (offset >= 0x8400 && offset <= 0x87ff) colorram_w(offset - 0x8400,data);
	else if (offset >= 0x8800 && offset <= 0x881f) RAM[offset] = data;
	else if (offset == 0x9a00)                     sound_command_w(0,data);
	else if (offset == 0x9c00)                     disco_video_control_w(0,data);
	//else logerror("CPU #%d PC %04x: warning - write %02x to unmapped memory address %04x\n",cpu_getactivecpu(),cpu_get_pc(),data,offset);

	btime_decrypt();
}


static struct MemoryReadAddress btime_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x1000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1bff, btime_mirrorvideoram_r },
	{ 0x1c00, 0x1fff, btime_mirrorcolorram_r },
	{ 0x4000, 0x4000, input_port_0_r },     /* IN0 */
	{ 0x4001, 0x4001, input_port_1_r },     /* IN1 */
	{ 0x4002, 0x4002, input_port_2_r },     /* coin */
	{ 0x4003, 0x4003, input_port_3_r },     /* DSW1 */
	{ 0x4004, 0x4004, input_port_4_r },     /* DSW2 */
	{ 0xb000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress btime_writemem[] =
{
	{ 0x0000, 0xffff, btime_w },	    /* override the following entries to */
										/* support ROM decryption */
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0c00, 0x0c0f, btime_paletteram_w, &paletteram },
	{ 0x1000, 0x13ff, videoram_w, &videoram, &videoram_size },
	{ 0x1400, 0x17ff, colorram_w, &colorram },
	{ 0x1800, 0x1bff, btime_mirrorvideoram_w },
	{ 0x1c00, 0x1fff, btime_mirrorcolorram_w },
	{ 0x4000, 0x4000, MWA_NOP },
	{ 0x4002, 0x4002, btime_video_control_w },
	{ 0x4003, 0x4003, sound_command_w },
	{ 0x4004, 0x4004, bnj_scroll1_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress cookrace_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0500, 0x3fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xcbff, btime_mirrorvideoram_r },
	{ 0xcc00, 0xcfff, btime_mirrorcolorram_r },
	{ 0xd000, 0xd0ff, MRA_RAM },	/* background */
	{ 0xd100, 0xd3ff, MRA_RAM },	/* ? */
	{ 0xd400, 0xd7ff, MRA_RAM },	/* background? */
	{ 0xe000, 0xe000, input_port_3_r },     /* DSW1 */
	{ 0xe300, 0xe300, input_port_3_r },     /* mirror address used on high score name enter */
											/* screen */
	{ 0xe001, 0xe001, input_port_4_r },     /* DSW2 */
	{ 0xe002, 0xe002, input_port_0_r },     /* IN0 */
	{ 0xe003, 0xe003, input_port_1_r },     /* IN1 */
	{ 0xe004, 0xe004, input_port_2_r },     /* coin */
	{ 0xfff9, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cookrace_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0500, 0x3fff, MWA_ROM },
	{ 0xc000, 0xc3ff, videoram_w, &videoram, &videoram_size },
	{ 0xc400, 0xc7ff, colorram_w, &colorram },
	{ 0xc800, 0xcbff, btime_mirrorvideoram_w },
	{ 0xcc00, 0xcfff, btime_mirrorcolorram_w },
	{ 0xd000, 0xd0ff, MWA_RAM },	/* background? */
	{ 0xd100, 0xd3ff, MWA_RAM },	/* ? */
	{ 0xd400, 0xd7ff, MWA_RAM, &bnj_backgroundram, &bnj_backgroundram_size },
	{ 0xe000, 0xe000, bnj_video_control_w },
	{ 0xe001, 0xe001, sound_command_w },
#if 0
	{ 0x4004, 0x4004, bnj_scroll1_w },
#endif
	{ 0xfff9, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress zoar_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x9800, 0x9800, input_port_3_r },     /* DSW 1 */
	{ 0x9801, 0x9801, input_port_4_r },     /* DSW 2 */
	{ 0x9802, 0x9802, input_port_0_r },     /* IN 0 */
	{ 0x9803, 0x9803, input_port_1_r },     /* IN 1 */
	{ 0x9804, 0x9804, input_port_2_r },     /* coin */
	{ 0xd000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress zoar_writemem[] =
{
	{ 0x0000, 0xffff, zoar_w },	    /* override the following entries to */
									/* support ROM decryption */
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, colorram_w, &colorram },
	{ 0x8800, 0x8bff, btime_mirrorvideoram_w },
	{ 0x8c00, 0x8fff, btime_mirrorcolorram_w },
	{ 0x9000, 0x9000, zoar_video_control_w },
	{ 0x9800, 0x9803, MWA_RAM, &zoar_scrollram },
	{ 0x9805, 0x9805, bnj_scroll2_w },
	{ 0x9805, 0x9805, bnj_scroll1_w },
	{ 0x9806, 0x9806, sound_command_w },
  /*{ 0x9807, 0x9807, MWA_RAM }, */ /* Marked as ACK on schematics (Board 2 Pg 5) */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress lnc_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_RAM },
	{ 0x7c00, 0x7fff, btime_mirrorvideoram_r },
	{ 0x8000, 0x8000, input_port_3_r },     /* DSW1 */
	{ 0x8001, 0x8001, input_port_4_r },     /* DSW2 */
	{ 0x9000, 0x9000, input_port_0_r },     /* IN0 */
	{ 0x9001, 0x9001, input_port_1_r },     /* IN1 */
	{ 0x9002, 0x9002, input_port_2_r },     /* coin */
	{ 0xb000, 0xb1ff, MRA_RAM },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress lnc_writemem[] =
{
	{ 0x0000, 0xffff, lnc_w },      /* override the following entries to */
									/* support ROM decryption */
	{ 0x0000, 0x3bff, MWA_RAM },
	{ 0x3c00, 0x3fff, lnc_videoram_w, &videoram, &videoram_size },
	{ 0x7800, 0x7bff, colorram_w, &colorram },  /* this is just here to initialize the pointer */
	{ 0x7c00, 0x7fff, lnc_mirrorvideoram_w },
	{ 0x8000, 0x8000, MWA_NOP },            /* ??? */
	{ 0x8001, 0x8001, lnc_video_control_w },
	{ 0x8003, 0x8003, MWA_RAM, &lnc_charbank },
	{ 0x9000, 0x9000, MWA_NOP },            /* IRQ ACK ??? */
	{ 0x9002, 0x9002, sound_command_w },
	{ 0xb000, 0xb1ff, MWA_RAM },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress mmonkey_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_RAM },
	{ 0x7c00, 0x7fff, btime_mirrorvideoram_r },
	{ 0x8000, 0x8000, input_port_3_r },     /* DSW1 */
	{ 0x8001, 0x8001, input_port_4_r },     /* DSW2 */
	{ 0x9000, 0x9000, input_port_0_r },     /* IN0 */
	{ 0x9001, 0x9001, input_port_1_r },     /* IN1 */
	{ 0x9002, 0x9002, input_port_2_r },     /* coin */
	{ 0xb000, 0xbfff, mmonkey_protection_r },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress mmonkey_writemem[] =
{
	{ 0x0000, 0xffff, mmonkey_w },  /* override the following entries to */
									/* support ROM decryption */
	{ 0x0000, 0x3bff, MWA_RAM },
	{ 0x3c00, 0x3fff, lnc_videoram_w, &videoram, &videoram_size },
	{ 0x7800, 0x7bff, colorram_w, &colorram },  /* this is just here to initialize the pointer */
	{ 0x7c00, 0x7fff, lnc_mirrorvideoram_w },
	{ 0x8001, 0x8001, lnc_video_control_w },
	{ 0x8003, 0x8003, MWA_RAM, &lnc_charbank },
	{ 0x9000, 0x9000, MWA_NOP },            /* IRQ ACK ??? */
	{ 0x9002, 0x9002, sound_command_w },
	{ 0xb000, 0xbfff, mmonkey_protection_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress bnj_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x1000, 0x1000, input_port_3_r },     /* DSW1 */
	{ 0x1001, 0x1001, input_port_4_r },     /* DSW2 */
	{ 0x1002, 0x1002, input_port_0_r },     /* IN0 */
	{ 0x1003, 0x1003, input_port_1_r },     /* IN1 */
	{ 0x1004, 0x1004, input_port_2_r },     /* coin */
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x4800, 0x4bff, btime_mirrorvideoram_r },
	{ 0x4c00, 0x4fff, btime_mirrorcolorram_r },
	{ 0xa000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress bnj_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x1001, 0x1001, bnj_video_control_w },
	{ 0x1002, 0x1002, sound_command_w },
	{ 0x4000, 0x43ff, videoram_w, &videoram, &videoram_size },
	{ 0x4400, 0x47ff, colorram_w, &colorram },
	{ 0x4800, 0x4bff, btime_mirrorvideoram_w },
	{ 0x4c00, 0x4fff, btime_mirrorcolorram_w },
	{ 0x5000, 0x51ff, bnj_background_w, &bnj_backgroundram, &bnj_backgroundram_size },
	{ 0x5400, 0x5400, bnj_scroll1_w },
	{ 0x5800, 0x5800, bnj_scroll2_w },
	{ 0x5c00, 0x5c0f, btime_paletteram_w, &paletteram },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress disco_readmem[] =
{
	{ 0x0000, 0x04ff, MRA_RAM },
	{ 0x2000, 0x881f, MRA_RAM },
	{ 0x9000, 0x9000, input_port_2_r },     /* coin */
	{ 0x9200, 0x9200, input_port_0_r },     /* IN0 */
	{ 0x9400, 0x9400, input_port_1_r },     /* IN1 */
	{ 0x9800, 0x9800, input_port_3_r },     /* DSW1 */
	{ 0x9a00, 0x9a00, input_port_4_r },     /* DSW2 */
	{ 0x9c00, 0x9c00, input_port_5_r },     /* VBLANK */
	{ 0xa000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress disco_writemem[] =
{
	{ 0x0000, 0xffff, disco_w },    /* override the following entries to */
									/* support ROM decryption */
	{ 0x2000, 0x7fff, deco_charram_w, &deco_charram },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, colorram_w, &colorram },
	{ 0x8800, 0x881f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9a00, 0x9a00, sound_command_w },
	{ 0x9c00, 0x9c00, disco_video_control_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0200, 0x0fff, MRA_ROM },	/* Cook Race */
	{ 0xa000, 0xafff, soundlatch_r },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0200, 0x0fff, MWA_ROM },	/* Cook Race */
	{ 0x2000, 0x2fff, AY8910_write_port_0_w },
	{ 0x4000, 0x4fff, AY8910_control_port_0_w },
	{ 0x6000, 0x6fff, AY8910_write_port_1_w },
	{ 0x8000, 0x8fff, AY8910_control_port_1_w },
	{ 0xc000, 0xcfff, interrupt_enable_w },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress disco_sound_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x8000, 0x8fff, soundlatch_r },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress disco_sound_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x4000, 0x4fff, AY8910_write_port_0_w },
	{ 0x5000, 0x5fff, AY8910_control_port_0_w },
	{ 0x6000, 0x6fff, AY8910_write_port_1_w },
	{ 0x7000, 0x7fff, AY8910_control_port_1_w },
	{ 0x8000, 0x8fff, MWA_NOP },  /* ACK ? */
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


/***************************************************************************

These games don't have VBlank interrupts.
Interrupts are still used by the game, coin insertion generates an IRQ.

***************************************************************************/
static int btime_interrupt(int(*generated_interrupt)(void), int active_high)
{
	static int coin;
	int port;

	port = readinputport(2) & 0xc0;
	if (active_high) port ^= 0xc0;

	if (port != 0xc0)    /* Coin */
	{
		if (coin == 0)
		{
			coin = 1;
			return generated_interrupt();
		}
	}
	else coin = 0;

	return ignore_interrupt();
}

static int btime_irq_interrupt(void)
{
	return btime_interrupt(interrupt, 1);
}

static int zoar_irq_interrupt(void)
{
	return btime_interrupt(interrupt, 0);
}

static int btime_nmi_interrupt(void)
{
	return btime_interrupt(nmi_interrupt, 0);
}

static WRITE_HANDLER( sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,M6502_INT_IRQ);
}


INPUT_PORTS_START( btime )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH,IPT_COIN2 )

	PORT_START      /* DSW1 */
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
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x20, 0x20, "Cross Hatch Pattern" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "10000" )
	PORT_DIPSETTING(    0x04, "15000" )
	PORT_DIPSETTING(    0x02, "20000"  )
	PORT_DIPSETTING(    0x00, "30000"  )
	PORT_DIPNAME( 0x08, 0x08, "Enemies" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x10, 0x10, "End of Level Pepper" )
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( cookrace )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH,IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH,IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH,IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH,IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH,IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH,IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH,IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* DSW1 */
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
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "20000" )
	PORT_DIPSETTING(    0x04, "30000" )
	PORT_DIPSETTING(    0x02, "40000"  )
	PORT_DIPSETTING(    0x00, "50000"  )
	PORT_DIPNAME( 0x08, 0x08, "Enemies" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x10, 0x10, "End of Level Pepper" )
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( zoar )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
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
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )    /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	/* Service mode doesn't work because of missing ROMs */
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "5000" )
	PORT_DIPSETTING(    0x04, "10000" )
	PORT_DIPSETTING(    0x02, "15000"  )
	PORT_DIPSETTING(    0x00, "20000"  )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x10, 0x00, "Weapon Select" )
	PORT_DIPSETTING(    0x00, "Manual" )
	PORT_DIPSETTING(    0x10, "Auto" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )    /* These 3 switches     */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )        /* have to do with      */
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )         /* coinage.             */
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )    /* See code at $d234.   */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )        /* Feel free to figure  */
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )         /* them out.            */
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( lnc )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
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
	PORT_DIPNAME( 0x30, 0x30, "Test Mode" )
	PORT_DIPSETTING(    0x30, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, "RAM Test Only" )
	PORT_DIPSETTING(    0x20, "Watchdog Test Only" )
	PORT_DIPSETTING(    0x10, "All Tests" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "15000" )
	PORT_DIPSETTING(    0x04, "20000" )
	PORT_DIPSETTING(    0x02, "30000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x08, 0x08, "Game Speed" )
	PORT_DIPSETTING(    0x08, "Slow" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( wtennis )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "10000" )
	PORT_DIPSETTING(    0x04, "20000" )
	PORT_DIPSETTING(    0x02, "30000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )   /* definately used */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )    /* These 3 switches     */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )        /* have to do with      */
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )         /* coinage.             */
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( mmonkey )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unused ) )   /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "Every 15000" )
	PORT_DIPSETTING(    0x04, "Every 30000" )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x06, "None" )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x18, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Level Skip Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unused ) )   /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unused ) )   /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( bnj )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* DSW1 */
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
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "Every 30000" )
	PORT_DIPSETTING(    0x04, "Every 70000" )
	PORT_DIPSETTING(    0x02, "20000 Only"  )
	PORT_DIPSETTING(    0x00, "30000 Only"  )
	PORT_DIPNAME( 0x08, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( disco )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START2 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH,IPT_COIN2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x02, "20000" )
	PORT_DIPSETTING(    0x04, "30000" )
	PORT_DIPSETTING(    0x06, "None" )
	PORT_DIPNAME( 0x08, 0x00, "Music Weapons" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPSETTING(    0x08, "8" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* VBLANK */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	1024,   /* 1024 characters */
	3,      /* 3 bits per pixel */
	{ 2*1024*8*8, 1024*8*8, 0 },    /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	256,    /* 256 sprites */
	3,      /* 3 bits per pixel */
	{ 2*256*16*16, 256*16*16, 0 },  /* the bitplanes are separated */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
	  0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout zoar_spritelayout =
{
	16,16,  /* 16*16 sprites */
	128,    /* 256 sprites */
	3,      /* 3 bits per pixel */
	{ 2*128*16*16, 128*16*16, 0 },  /* the bitplanes are separated */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
	  0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout btime_tilelayout =
{
	16,16,  /* 16*16 characters */
	64,    /* 64 characters */
	3,      /* 3 bits per pixel */
	{  2*64*16*16, 64*16*16, 0 },    /* the bitplanes are separated */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
	  0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every tile takes 32 consecutive bytes */
};

static struct GfxLayout cookrace_tilelayout =
{
	8,8,  /* 8*8 characters */
	256,    /* 256 characters */
	3,      /* 3 bits per pixel */
	{  2*256*8*8, 256*8*8, 0 },    /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8    /* every tile takes 8 consecutive bytes */
};

static struct GfxLayout bnj_tilelayout =
{
	16,16,  /* 16*16 characters */
	64, /* 64 characters */
	3,  /* 3 bits per pixel */
	{ 2*64*16*16+4, 0, 4 },
	{ 3*16*8+0, 3*16*8+1, 3*16*8+2, 3*16*8+3, 2*16*8+0, 2*16*8+1, 2*16*8+2, 2*16*8+3,
	  16*8+0, 16*8+1, 16*8+2, 16*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	64*8    /* every tile takes 64 consecutive bytes */
};

static struct GfxDecodeInfo btime_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          0, 1 }, /* char set #1 */
	{ REGION_GFX1, 0, &spritelayout,        0, 1 }, /* sprites */
	{ REGION_GFX2, 0, &btime_tilelayout,    8, 1 }, /* background tiles */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo cookrace_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          0, 1 }, /* char set #1 */
	{ REGION_GFX1, 0, &spritelayout,        0, 1 }, /* sprites */
	{ REGION_GFX2, 0, &cookrace_tilelayout, 8, 1 }, /* background tiles */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo lnc_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          0, 1 },     /* char set #1 */
	{ REGION_GFX1, 0, &spritelayout,        0, 1 },     /* sprites */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo bnj_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          0, 1 }, /* char set #1 */
	{ REGION_GFX1, 0, &spritelayout,        0, 1 }, /* sprites */
	{ REGION_GFX2, 0, &bnj_tilelayout,      8, 1 }, /* background tiles */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo zoar_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          0, 8 }, /* char set #1 */
	{ REGION_GFX4, 0, &zoar_spritelayout,   0, 8 }, /* sprites */
	{ REGION_GFX2, 0, &btime_tilelayout,    0, 8 }, /* background tiles */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo disco_gfxdecodeinfo[] =
{
	{ 0, 0x2000, &charlayout,          0, 4 }, /* char set #1 */
	{ 0, 0x2000, &spritelayout,        0, 4 }, /* sprites */
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,      /* 2 chips */
	1500000,        /* 1.5 MHz ? (hand tuned) */
	{ 23, 23 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

/********************************************************************
*
*  Machine Driver macro
*  ====================
*
*  Abusing the pre-processor.
*
********************************************************************/

#define cookrace_vh_convert_color_prom   btime_vh_convert_color_prom
#define bnj_vh_convert_color_prom        0
#define wtennis_vh_convert_color_prom    lnc_vh_convert_color_prom
#define mmonkey_vh_convert_color_prom    lnc_vh_convert_color_prom
#define zoar_vh_convert_color_prom       btime_vh_convert_color_prom
#define disco_vh_convert_color_prom      btime_vh_convert_color_prom

#define wtennis_vh_screenrefresh	eggs_vh_screenrefresh
#define mmonkey_vh_screenrefresh	eggs_vh_screenrefresh

#define wtennis_readmem			lnc_readmem

#define wtennis_writemem		lnc_writemem

#define btime_sound_readmem		sound_readmem
#define cookrace_sound_readmem	sound_readmem
#define lnc_sound_readmem		sound_readmem
#define wtennis_sound_readmem	sound_readmem
#define mmonkey_sound_readmem	sound_readmem
#define bnj_sound_readmem		sound_readmem
#define zoar_sound_readmem		sound_readmem

#define btime_sound_writemem	sound_writemem
#define cookrace_sound_writemem	sound_writemem
#define lnc_sound_writemem		sound_writemem
#define wtennis_sound_writemem	sound_writemem
#define mmonkey_sound_writemem	sound_writemem
#define bnj_sound_writemem		sound_writemem
#define zoar_sound_writemem		sound_writemem

#define btime_init_machine     0
#define cookrace_init_machine  0
#define bnj_init_machine       0
#define wtennis_init_machine   lnc_init_machine
#define mmonkey_init_machine   lnc_init_machine
#define zoar_init_machine      0
#define disco_init_machine     0

#define cookrace_vh_start  btime_vh_start
#define zoar_vh_start      btime_vh_start
#define lnc_vh_start       btime_vh_start
#define wtennis_vh_start   btime_vh_start
#define mmonkey_vh_start   btime_vh_start
#define disco_vh_start     btime_vh_start

#define btime_vh_stop      generic_vh_stop
#define cookrace_vh_stop   generic_vh_stop
#define zoar_vh_stop       generic_vh_stop
#define lnc_vh_stop        generic_vh_stop
#define wtennis_vh_stop    generic_vh_stop
#define mmonkey_vh_stop    generic_vh_stop
#define disco_vh_stop      generic_vh_stop


#define MACHINE_DRIVER(GAMENAME, CLOCK, MAIN_IRQ, SOUND_IRQ, GFX, COLOR)   \
																	\
static struct MachineDriver machine_driver_##GAMENAME =             \
{                                                                   \
	/* basic machine hardware */                                	\
	{		                                                        \
		{	  	                                                    \
			CPU_M6502,                                  			\
			CLOCK,													\
			GAMENAME##_readmem,GAMENAME##_writemem,0,0, 			\
			MAIN_IRQ,1                                  			\
		},		                                                    \
		{		                                                    \
			CPU_M6502 | CPU_AUDIO_CPU,                  			\
			500000, /* 500 kHz */                       			\
			GAMENAME##_sound_readmem,GAMENAME##_sound_writemem,0,0, \
			SOUND_IRQ,16   /* IRQs are triggered by the main CPU */ \
		}                                                   		\
	},                                                          	\
	57, 3072,        /* frames per second, vblank duration */   	\
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */ \
	GAMENAME##_init_machine,		                               	\
																	\
	/* video hardware */                                        	\
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 31*8-1 },                   	\
	GFX,                                                        	\
	COLOR,COLOR,                                                	\
	GAMENAME##_vh_convert_color_prom,                           	\
																	\
	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,                   	\
	0,                                                          	\
	GAMENAME##_vh_start,                                        	\
	GAMENAME##_vh_stop,                                         	\
	GAMENAME##_vh_screenrefresh,                                   	\
																	\
	/* sound hardware */                                        	\
	0,0,0,0,                                                    	\
	{                                                           	\
		{                                                   		\
			SOUND_AY8910,                               			\
			&ay8910_interface                           			\
		}                                                   		\
	}                                                           	\
}


/*              NAME      CLOCK    MAIN_IRQ             SOUND_IRQ            GFX_DECODE              COLOR */

MACHINE_DRIVER( btime,    1500000, btime_irq_interrupt, nmi_interrupt,       btime_gfxdecodeinfo,    16);
MACHINE_DRIVER( cookrace, 1500000, btime_nmi_interrupt, nmi_interrupt,       cookrace_gfxdecodeinfo, 16);
MACHINE_DRIVER( lnc,      1500000, btime_nmi_interrupt, lnc_sound_interrupt, lnc_gfxdecodeinfo,      8);
MACHINE_DRIVER( wtennis,  1500000, btime_nmi_interrupt, nmi_interrupt,       lnc_gfxdecodeinfo,      8);
MACHINE_DRIVER( mmonkey,  1500000, btime_nmi_interrupt, nmi_interrupt,       lnc_gfxdecodeinfo,      8);
MACHINE_DRIVER( bnj,       750000, btime_nmi_interrupt, nmi_interrupt,       bnj_gfxdecodeinfo,      16);
MACHINE_DRIVER( zoar,     1500000, zoar_irq_interrupt,  nmi_interrupt,       zoar_gfxdecodeinfo,     64);
MACHINE_DRIVER( disco,     750000, btime_irq_interrupt, nmi_interrupt,       disco_gfxdecodeinfo,    32);


/***************************************************************************

	Game driver(s)

***************************************************************************/

ROM_START( btime )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "aa04.9b",      0xc000, 0x1000, 0x368a25b5 )
	ROM_LOAD( "aa06.13b",     0xd000, 0x1000, 0xb4ba400d )
	ROM_LOAD( "aa05.10b",     0xe000, 0x1000, 0x8005bffa )
	ROM_LOAD( "aa07.15b",     0xf000, 0x1000, 0x086440ad )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "ab14.12h",     0xf000, 0x1000, 0xf55e5211 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "aa12.7k",      0x0000, 0x1000, 0xc4617243 )    /* charset #1 */
	ROM_LOAD( "ab13.9k",      0x1000, 0x1000, 0xac01042f )
	ROM_LOAD( "ab10.10k",     0x2000, 0x1000, 0x854a872a )
	ROM_LOAD( "ab11.12k",     0x3000, 0x1000, 0xd4848014 )
	ROM_LOAD( "aa8.13k",      0x4000, 0x1000, 0x8650c788 )
	ROM_LOAD( "ab9.15k",      0x5000, 0x1000, 0x8dec15e6 )

	ROM_REGION( 0x1800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ab00.1b",      0x0000, 0x0800, 0xc7a14485 )    /* charset #2 */
	ROM_LOAD( "ab01.3b",      0x0800, 0x0800, 0x25b49078 )
	ROM_LOAD( "ab02.4b",      0x1000, 0x0800, 0xb8ef56c3 )

	ROM_REGION( 0x0800, REGION_GFX3 )	/* background tilemaps */
	ROM_LOAD( "ab03.6b",      0x0000, 0x0800, 0xd26bc1f3 )
ROM_END

ROM_START( btime2 )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "aa04.9b2",     0xc000, 0x1000, 0xa041e25b )
	ROM_LOAD( "aa06.13b",     0xd000, 0x1000, 0xb4ba400d )
	ROM_LOAD( "aa05.10b",     0xe000, 0x1000, 0x8005bffa )
	ROM_LOAD( "aa07.15b",     0xf000, 0x1000, 0x086440ad )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "ab14.12h",     0xf000, 0x1000, 0xf55e5211 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "aa12.7k",      0x0000, 0x1000, 0xc4617243 )    /* charset #1 */
	ROM_LOAD( "ab13.9k",      0x1000, 0x1000, 0xac01042f )
	ROM_LOAD( "ab10.10k",     0x2000, 0x1000, 0x854a872a )
	ROM_LOAD( "ab11.12k",     0x3000, 0x1000, 0xd4848014 )
	ROM_LOAD( "aa8.13k",      0x4000, 0x1000, 0x8650c788 )
	ROM_LOAD( "ab9.15k",      0x5000, 0x1000, 0x8dec15e6 )

	ROM_REGION( 0x1800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ab00.1b",      0x0000, 0x0800, 0xc7a14485 )    /* charset #2 */
	ROM_LOAD( "ab01.3b",      0x0800, 0x0800, 0x25b49078 )
	ROM_LOAD( "ab02.4b",      0x1000, 0x0800, 0xb8ef56c3 )

	ROM_REGION( 0x0800, REGION_GFX3 )	/* background tilemaps */
	ROM_LOAD( "ab03.6b",      0x0000, 0x0800, 0xd26bc1f3 )
ROM_END

ROM_START( btimem )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "ab05a1.12b",   0xb000, 0x1000, 0x0a98b230 )
	ROM_LOAD( "ab04.9b",      0xc000, 0x1000, 0x797e5f75 )
	ROM_LOAD( "ab06.13b",     0xd000, 0x1000, 0xc77f3f64 )
	ROM_LOAD( "ab05.10b",     0xe000, 0x1000, 0xb0d3640f )
	ROM_LOAD( "ab07.15b",     0xf000, 0x1000, 0xa142f862 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "ab14.12h",     0xf000, 0x1000, 0xf55e5211 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ab12.7k",      0x0000, 0x1000, 0x6c79f79f )    /* charset #1 */
	ROM_LOAD( "ab13.9k",      0x1000, 0x1000, 0xac01042f )
	ROM_LOAD( "ab10.10k",     0x2000, 0x1000, 0x854a872a )
	ROM_LOAD( "ab11.12k",     0x3000, 0x1000, 0xd4848014 )
	ROM_LOAD( "ab8.13k",      0x4000, 0x1000, 0x70b35bbe )
	ROM_LOAD( "ab9.15k",      0x5000, 0x1000, 0x8dec15e6 )

	ROM_REGION( 0x1800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ab00.1b",      0x0000, 0x0800, 0xc7a14485 )    /* charset #2 */
	ROM_LOAD( "ab01.3b",      0x0800, 0x0800, 0x25b49078 )
	ROM_LOAD( "ab02.4b",      0x1000, 0x0800, 0xb8ef56c3 )

	ROM_REGION( 0x0800, REGION_GFX3 )	/* background tilemaps */
	ROM_LOAD( "ab03.6b",      0x0000, 0x0800, 0xd26bc1f3 )
ROM_END

ROM_START( cookrace )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	/* code is in the range 0500-3fff, encrypted */
	ROM_LOAD( "1f.1",         0x0000, 0x2000, 0x68759d32 )
	ROM_LOAD( "2f.2",         0x2000, 0x2000, 0xbe7d72d1 )
	ROM_LOAD( "2k",           0xffe0, 0x0020, 0xe2553b3d )	/* reset/interrupt vectors */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "6f.6",         0x0000, 0x1000, 0x6b8e0272 ) /* starts at 0000, not f000; 0000-01ff is RAM */
	ROM_RELOAD(               0xf000, 0x1000 )     /* for the reset/interrupt vectors */

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "m8.7",         0x0000, 0x2000, 0xa1a0d5a6 )  /* charset #1 */
	ROM_LOAD( "m7.8",         0x2000, 0x2000, 0x1104f497 )
	ROM_LOAD( "m6.9",         0x4000, 0x2000, 0xd0d94477 )

	ROM_REGION( 0x1800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2f.3",         0x0000, 0x0800, 0x28609a75 )  /* garbage?? */
	ROM_CONTINUE(             0x0000, 0x0800 )              /* charset #2 */
	ROM_LOAD( "4f.4",         0x0800, 0x0800, 0x7742e771 )  /* garbage?? */
	ROM_CONTINUE(             0x0800, 0x0800 )
	ROM_LOAD( "5f.5",         0x1000, 0x0800, 0x611c686f )  /* garbage?? */
	ROM_CONTINUE(             0x1000, 0x0800 )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "f9.clr",       0x0000, 0x0020, 0xc2348c1d )	/* palette */
	ROM_LOAD( "b7",           0x0020, 0x0020, 0xe4268fa6 )	/* unknown */
ROM_END

/* There is a flyer with a screen shot for Lock'n'Chase at:
   http://www.gamearchive.com/flyers/video/taito/locknchase_f.jpg  */

ROM_START( lnc )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "s3-3d",        0xc000, 0x1000, 0x1ab4f2c2 )
	ROM_LOAD( "s2-3c",        0xd000, 0x1000, 0x5e46b789 )
	ROM_LOAD( "s1-3b",        0xe000, 0x1000, 0x1308a32e )
	ROM_LOAD( "s0-3a",        0xf000, 0x1000, 0xbeb4b1fc )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "sa-1h",        0xf000, 0x1000, 0x379387ec )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "s4-11l",       0x0000, 0x1000, 0xa2162a9e )
	ROM_LOAD( "s5-11m",       0x1000, 0x1000, 0x12f1c2db )
	ROM_LOAD( "s6-13l",       0x2000, 0x1000, 0xd21e2a57 )
	ROM_LOAD( "s7-13m",       0x3000, 0x1000, 0xc4f247cd )
	ROM_LOAD( "s8-15l",       0x4000, 0x1000, 0x672a92d0 )
	ROM_LOAD( "s9-15m",       0x5000, 0x1000, 0x87c8ee9a )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "sc-5m",        0x0000, 0x0020, 0x2a976ebe )	/* palette */
	ROM_LOAD( "sb-4c",        0x0020, 0x0020, 0xa29b4204 )	/* RAS/CAS logic - not used */
ROM_END

ROM_START( wtennis )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "tx",           0xc000, 0x0800, 0xfd343474 )
	ROM_LOAD( "t4",           0xd000, 0x1000, 0xe465d82c )
	ROM_LOAD( "t3",           0xe000, 0x1000, 0x8f090eab )
	ROM_LOAD( "t2",           0xf000, 0x1000, 0xd2f9dd30 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "t1",           0x0000, 0x1000, 0x40737ea7 ) /* starts at 0000, not f000; 0000-01ff is RAM */
	ROM_RELOAD(               0xf000, 0x1000 )     /* for the reset/interrupt vectors */

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "t7",           0x0000, 0x1000, 0xaa935169 )
	ROM_LOAD( "t10",          0x1000, 0x1000, 0x746be927 )
	ROM_LOAD( "t5",           0x2000, 0x1000, 0xea1efa5d )
	ROM_LOAD( "t8",           0x3000, 0x1000, 0x542ace7b )
	ROM_LOAD( "t6",           0x4000, 0x1000, 0x4fb8565d )
	ROM_LOAD( "t9",           0x5000, 0x1000, 0x4893286d )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "mb7051.m5",    0x0000, 0x0020, 0xf051cb28 )	/* palette */
	ROM_LOAD( "sb-4c",        0x0020, 0x0020, 0xa29b4204 )	/* RAS/CAS logic - not used */
ROM_END

ROM_START( mmonkey )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "mmonkey.e4",   0xc000, 0x1000, 0x8d31bf6a )
	ROM_LOAD( "mmonkey.d4",   0xd000, 0x1000, 0xe54f584a )
	ROM_LOAD( "mmonkey.b4",   0xe000, 0x1000, 0x399a161e )
	ROM_LOAD( "mmonkey.a4",   0xf000, 0x1000, 0xf7d3d1e3 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "mmonkey.h1",   0xf000, 0x1000, 0x5bcb2e81 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mmonkey.l11",  0x0000, 0x1000, 0xb6aa8566 )
	ROM_LOAD( "mmonkey.m11",  0x1000, 0x1000, 0x6cc4d0c4 )
	ROM_LOAD( "mmonkey.l13",  0x2000, 0x1000, 0x2a343b7e )
	ROM_LOAD( "mmonkey.m13",  0x3000, 0x1000, 0x0230b50d )
	ROM_LOAD( "mmonkey.l14",  0x4000, 0x1000, 0x922bb3e1 )
	ROM_LOAD( "mmonkey.m14",  0x5000, 0x1000, 0xf943e28c )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "mmi6331.m5",   0x0000, 0x0020, 0x55e28b32 )	/* palette */
	ROM_LOAD( "sb-4c",        0x0020, 0x0020, 0xa29b4204 )	/* RAS/CAS logic - not used */
ROM_END

ROM_START( brubber )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	/* a000-bfff space for the service ROM */
	ROM_LOAD( "brubber.12c",  0xc000, 0x2000, 0xb5279c70 )
	ROM_LOAD( "brubber.12d",  0xe000, 0x2000, 0xb2ce51f5 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "bnj6c.bin",    0xf000, 0x1000, 0x8c02f662 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bnj4e.bin",    0x0000, 0x2000, 0xb864d082 )
	ROM_LOAD( "bnj4f.bin",    0x2000, 0x2000, 0x6c31d77a )
	ROM_LOAD( "bnj4h.bin",    0x4000, 0x2000, 0x5824e6fb )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bnj10e.bin",   0x0000, 0x1000, 0xf4e9eb49 )
	ROM_LOAD( "bnj10f.bin",   0x1000, 0x1000, 0xa9ffacb4 )
ROM_END

ROM_START( bnj )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "bnj12b.bin",   0xa000, 0x2000, 0xba3e3801 )
	ROM_LOAD( "bnj12c.bin",   0xc000, 0x2000, 0xfb3a2cdd )
	ROM_LOAD( "bnj12d.bin",   0xe000, 0x2000, 0xb88bc99e )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "bnj6c.bin",    0xf000, 0x1000, 0x8c02f662 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bnj4e.bin",    0x0000, 0x2000, 0xb864d082 )
	ROM_LOAD( "bnj4f.bin",    0x2000, 0x2000, 0x6c31d77a )
	ROM_LOAD( "bnj4h.bin",    0x4000, 0x2000, 0x5824e6fb )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bnj10e.bin",   0x0000, 0x1000, 0xf4e9eb49 )
	ROM_LOAD( "bnj10f.bin",   0x1000, 0x1000, 0xa9ffacb4 )
ROM_END

ROM_START( caractn )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	/* a000-bfff space for the service ROM */
	ROM_LOAD( "brubber.12c",  0xc000, 0x2000, 0xb5279c70 )
	ROM_LOAD( "caractn.a6",   0xe000, 0x2000, 0x1d6957c4 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "bnj6c.bin",    0xf000, 0x1000, 0x8c02f662 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "caractn.a0",   0x0000, 0x2000, 0xbf3ea732 )
	ROM_LOAD( "caractn.a1",   0x2000, 0x2000, 0x9789f639 )
	ROM_LOAD( "caractn.a2",   0x4000, 0x2000, 0x51dcc111 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bnj10e.bin",   0x0000, 0x1000, 0xf4e9eb49 )
	ROM_LOAD( "bnj10f.bin",   0x1000, 0x1000, 0xa9ffacb4 )
ROM_END

ROM_START( zoar )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "zoar15",       0xd000, 0x1000, 0x1f0cfdb7 )
	ROM_LOAD( "zoar16",       0xe000, 0x1000, 0x7685999c )
	ROM_LOAD( "zoar17",       0xf000, 0x1000, 0x619ea867 )

	ROM_REGION( 0x10000, REGION_CPU2 )      /* 64k for the audio CPU */
	ROM_LOAD( "zoar09",       0xf000, 0x1000, 0x18d96ff1 )

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zoar00",       0x0000, 0x1000, 0xfd2dcb64 )
	ROM_LOAD( "zoar01",       0x1000, 0x1000, 0x74d3ca48 )
	ROM_LOAD( "zoar03",       0x2000, 0x1000, 0x77b7df14 )
	ROM_LOAD( "zoar04",       0x3000, 0x1000, 0x9be786de )
	ROM_LOAD( "zoar06",       0x4000, 0x1000, 0x07638c71 )
	ROM_LOAD( "zoar07",       0x5000, 0x1000, 0xf4710f25 )

	ROM_REGION( 0x1800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zoar10",       0x0000, 0x0800, 0xaa8bcab8 )
	ROM_LOAD( "zoar11",       0x0800, 0x0800, 0xdcdad357 )
	ROM_LOAD( "zoar12",       0x1000, 0x0800, 0xed317e40 )

	ROM_REGION( 0x1000, REGION_GFX3 )	/* background tilemaps */
	ROM_LOAD( "zoar13",       0x0000, 0x1000, 0x8fefa960 )

	ROM_REGION( 0x3000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zoar02",       0x0000, 0x1000, 0xd8c3c122 )
	ROM_LOAD( "zoar05",       0x1000, 0x1000, 0x05dc6b09 )
	ROM_LOAD( "zoar08",       0x2000, 0x1000, 0x9a148551 )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "z20-1l",       0x0000, 0x0020, 0xa63f0a07 )
	ROM_LOAD( "z21-1l",       0x0020, 0x0020, 0x5e1e5788 )
ROM_END

ROM_START( disco )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "disco.w5",     0xa000, 0x1000, 0xb2c87b78 )
	ROM_LOAD( "disco.w4",     0xb000, 0x1000, 0xad7040ee )
	ROM_LOAD( "disco.w3",     0xc000, 0x1000, 0x12fb4f08 )
	ROM_LOAD( "disco.w2",     0xd000, 0x1000, 0x73f6fb2f )
	ROM_LOAD( "disco.w1",     0xe000, 0x1000, 0xee7b536b )
	ROM_LOAD( "disco.w0",     0xf000, 0x1000, 0x7c26e76b )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "disco.w6",     0xf000, 0x1000, 0xd81e781e )

	/* no gfx1 */

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "disco.clr",    0x0000, 0x0020, 0xa393f913 )
ROM_END


static READ_HANDLER( wtennis_reset_hack_r )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	/* Otherwise the game goes into test mode and there is no way out that I
	   can see.  I'm not sure how it can work, it probably somehow has to do
	   with the tape system */

	RAM[0xfc30] = 0;

	return RAM[0xc15f];
}

static void init_btime(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;

	memory_set_opcode_base(0,rom+diff);

	/* For now, just copy the RAM array over to ROM. Decryption will happen */
	/* at run time, since the CPU applies the decryption only if the previous */
	/* instruction did a memory write. */
	memcpy(rom+diff,rom,0x10000);
}

static void init_zoar(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);


	/* At location 0xD50A is what looks like an undocumented opcode. I tried
	   implementing it given what opcode 0x23 should do, but it still didn't
	   work in demo mode. So this could be another protection or a bad ROM read.
	   I'm NOPing it out for now. */
	memset(&rom[0xd50a],0xea,8);

    init_btime();
}

static void init_lnc(void)
{
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;

	memory_set_opcode_base(0,rom+diff);

	/* Swap bits 5 & 6 for opcodes */
	for (A = 0;A < 0x10000;A++)
		rom[A+diff] = swap_bits_5_6(rom[A]);
}

static void init_wtennis(void)
{
	install_mem_read_handler(0, 0xc15f, 0xc15f, wtennis_reset_hack_r);
	init_lnc();
}



GAME( 1982, btime,    0,       btime,    btime,    btime,   ROT270, "Data East Corporation", "Burger Time (Data East set 1)" )
GAME( 1982, btime2,   btime,   btime,    btime,    btime,   ROT270, "Data East Corporation", "Burger Time (Data East set 2)" )
GAME( 1982, btimem,   btime,   btime,    btime,    btime,   ROT270, "Data East (Bally Midway license)", "Burger Time (Midway)" )
GAME( 1982, cookrace, btime,   cookrace, cookrace, lnc,     ROT270, "bootleg", "Cook Race" )
GAME( 1981, lnc,      0,       lnc,      lnc,      lnc,     ROT270, "Data East Corporation", "Lock'n'Chase" )
GAMEX(1982, wtennis,  0,       wtennis,  wtennis,  wtennis, ROT270, "bootleg", "World Tennis", GAME_IMPERFECT_COLORS )
GAME( 1982, mmonkey,  0,       mmonkey,  mmonkey,  lnc,     ROT270, "Technos + Roller Tron", "Minky Monkey" )
GAME( 1982, brubber,  0,       bnj,      bnj,      lnc,     ROT270, "Data East", "Burnin' Rubber" )
GAME( 1982, bnj,      brubber, bnj,      bnj,      lnc,     ROT270, "Data East USA (Bally Midway license)", "Bump 'n' Jump" )
GAME( 1983, caractn,  brubber, bnj,      bnj,      lnc,     ROT270, "bootleg", "Car Action" )
GAME( 1982, zoar,     0,       zoar,     zoar,     zoar,    ROT270, "Data East USA", "Zoar" )
GAME( 1982, disco,    0,       disco,    disco,    btime,   ROT270, "Data East", "Disco No.1" )





void decocass_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


READ_HANDLER( pip_r )
{
	return rand();
}

static struct MemoryReadAddress decocass_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM },
	{ 0xe300, 0xe300, input_port_3_r },     /* DSW1 */
	{ 0xe500, 0xe502, pip_r },	/* read data from tape */
#if 0
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0500, 0x3fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xcbff, btime_mirrorvideoram_r },
	{ 0xcc00, 0xcfff, btime_mirrorcolorram_r },
	{ 0xd000, 0xd0ff, MRA_RAM },	/* background */
	{ 0xd100, 0xd3ff, MRA_RAM },	/* ? */
	{ 0xd400, 0xd7ff, MRA_RAM },	/* background? */
	{ 0xe000, 0xe000, input_port_3_r },     /* DSW1 */
	{ 0xe002, 0xe002, input_port_0_r },     /* IN0 */
	{ 0xe003, 0xe003, input_port_1_r },     /* IN1 */
	{ 0xe004, 0xe004, input_port_2_r },     /* coin */
#endif
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress decocass_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x6000, 0xbfff, deco_charram_w, &deco_charram },
	{ 0xc000, 0xc3ff, videoram_w, &videoram, &videoram_size },
	{ 0xc400, 0xc7ff, colorram_w, &colorram },
	{ 0xc800, 0xcbff, btime_mirrorvideoram_w },
	{ 0xcc00, 0xcfff, btime_mirrorcolorram_w },
	{ 0xe000, 0xe01f, btime_paletteram_w, &paletteram },	/* The "bios" doesn't write to e000 */
									/* but the "loading" background should be blue, not black */
#if 0
	{ 0x0500, 0x3fff, MWA_ROM },
	{ 0xd000, 0xd0ff, MWA_RAM, &bnj_backgroundram, &bnj_backgroundram_size },
	{ 0xd000, 0xd0ff, MWA_RAM },	/* background? */
	{ 0xd100, 0xd3ff, MWA_RAM },	/* ? */
	{ 0xd400, 0xd7ff, MWA_RAM, &bnj_backgroundram, &bnj_backgroundram_size },
	{ 0xe000, 0xe000, bnj_video_control_w },
	{ 0xe001, 0xe001, sound_command_w },
	{ 0x4004, 0x4004, bnj_scroll1_w },
#endif
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress decocass_sound_readmem[] =
{
	{ 0x0000, 0x01ff, MRA_RAM },
#if 0
	{ 0xa000, 0xafff, soundlatch_r },
#endif
	{ 0xf800, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress decocass_sound_writemem[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x2000, 0x2fff, AY8910_write_port_0_w },
	{ 0x4000, 0x4fff, AY8910_control_port_0_w },
	{ 0x6000, 0x6fff, AY8910_write_port_1_w },
	{ 0x8000, 0x8fff, AY8910_control_port_1_w },
#if 0
	{ 0xc000, 0xcfff, interrupt_enable_w },
#endif
	{ 0xf800, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( decocass )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH,IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH,IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH,IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH,IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH,IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH,IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH,IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* DSW1 */
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
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )    /* used by the "bios" */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )    /* used by the "bios" */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "20000" )
	PORT_DIPSETTING(    0x04, "30000" )
	PORT_DIPSETTING(    0x02, "40000"  )
	PORT_DIPSETTING(    0x00, "50000"  )
	PORT_DIPNAME( 0x08, 0x08, "Enemies" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x10, 0x10, "End of Level Pepper" )
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

static struct GfxDecodeInfo decocass_gfxdecodeinfo[] =
{
	{ 0, 0x6000, &charlayout,          0, 4 }, /* char set #1 */
	{ 0, 0x6000, &spritelayout,        0, 4 }, /* sprites */
//	{ 0, 0x6000, &cookrace_tilelayout, 8, 1 }, /* background tiles */
	{ -1 } /* end of array */
};

static struct MachineDriver machine_driver_decocass =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* ? I guess it should be 750000 like in bnj */
			decocass_readmem,decocass_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			500000, /* 500 kHz */
			decocass_sound_readmem,decocass_sound_writemem,0,0,
ignore_interrupt,0,//			nmi_interrupt,16   /* IRQs are triggered by the main CPU */
		}
	},
	57, 3072,        /* frames per second, vblank duration */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */ \
	0,//GAMENAME##_init_machine,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 31*8-1 },
	decocass_gfxdecodeinfo,
	32,32,//COLOR,COLOR,
	0,//GAMENAME##_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,
	0,
	btime_vh_start,
	generic_vh_stop,
	decocass_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

ROM_START( decocass )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "rms8.cpu",     0xf000, 0x1000, 0x23d929b7 )
/* the following two are just about the same stuff as the one above */
//	ROM_LOAD( "dsp3.p0b",     0xf000, 0x0800, 0xb67a91d9 )
//	ROM_LOAD( "dsp3.p1b",     0xf800, 0x0800, 0x3bfff5f3 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "rms8.snd",     0xf800, 0x0800, 0xb66b2c2a )
ROM_END

GAME( ????, decocass, 0, decocass, decocass, lnc, ROT270, "?????", "decocass" )
