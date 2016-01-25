#include "../vidhrdw/aerofgt.cpp"

/***************************************************************************

Various Video System Co. games using the C7-01 VS8803 VS8904 VS8905 video
chips.
I'm not sure, but I think 8904/8905 handle sprites and c7-01/8803 tilemaps.
tail2nos doesn't have the 8904/8905, and indeed it has a different sprite
system.

Driver by Nicola Salmoria


Notes:
- Sound doesn't work in Spinal Breakers
- Sprite zoom is probably not 100% accurate (check the table in vidhrdw).
  In pspikes, the zooming text during attract mode is horrible.

pspikes/turbofrc/aerofgtb write to two addresses which look like control
registers for a video generator. Maybe they control the display size/position.
aerofgt is different, it writes to consecutive memory addresses and the values
it writes don't seem to be related to these ones.

          00 01 02 03 04 05  08 09 0a 0b 0c 0d
          ------------------------------------
pspikes   57 63 69 71 1f 00  77 79 7b 7f 1f 00
karatblz  57 63 69 71 1f 00  77 79 7b 7f 1f 00
turbofrc  57 63 69 71 1f 00  77 79 7b 7f 1f 00
spinlbrk  57 68 6f 75 ff 01  77 78 7b 7f ff 00
aerofgtb  4f 5d 63 71 1f 00  6f 70 72 7c 1f 02
tail2nos  4f 5e 64 71 1f 09  7a 7c 7e 7f 1f 02

I think that bit 1 of register 0d is flip screen.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


extern unsigned char *aerofgt_rasterram;
extern unsigned char *aerofgt_bg1videoram,*aerofgt_bg2videoram;
extern unsigned char *aerofgt_spriteram1,*aerofgt_spriteram2;
extern size_t aerofgt_spriteram1_size,aerofgt_spriteram2_size;

READ_HANDLER( aerofgt_rasterram_r );
WRITE_HANDLER( aerofgt_rasterram_w );
READ_HANDLER( aerofgt_spriteram_2_r );
WRITE_HANDLER( aerofgt_spriteram_2_w );
READ_HANDLER( aerofgt_bg1videoram_r );
READ_HANDLER( aerofgt_bg2videoram_r );
WRITE_HANDLER( aerofgt_bg1videoram_w );
WRITE_HANDLER( aerofgt_bg2videoram_w );
WRITE_HANDLER( pspikes_gfxbank_w );
WRITE_HANDLER( karatblz_gfxbank_w );
WRITE_HANDLER( spinlbrk_gfxbank_w );
WRITE_HANDLER( turbofrc_gfxbank_w );
WRITE_HANDLER( aerofgt_gfxbank_w );
WRITE_HANDLER( aerofgt_bg1scrollx_w );
WRITE_HANDLER( aerofgt_bg1scrolly_w );
WRITE_HANDLER( aerofgt_bg2scrollx_w );
WRITE_HANDLER( aerofgt_bg2scrolly_w );
WRITE_HANDLER( pspikes_palette_bank_w );
int pspikes_vh_start(void);
int karatblz_vh_start(void);
int spinlbrk_vh_start(void);
int turbofrc_vh_start(void);
void pspikes_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void karatblz_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void spinlbrk_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void turbofrc_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void aerofgt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);




static unsigned char *aerofgt_workram;

static READ_HANDLER( aerofgt_workram_r )
{
	return READ_WORD(&aerofgt_workram[offset]);
}

static WRITE_HANDLER( aerofgt_workram_w )
{
	COMBINE_WORD_MEM(&aerofgt_workram[offset],data);
}



static int pending_command;

static WRITE_HANDLER( sound_command_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		pending_command = 1;
		soundlatch_w(offset,data & 0xff);
		cpu_cause_interrupt(1,Z80_NMI_INT);
	}
}

static WRITE_HANDLER( turbofrc_sound_command_w )
{
	if ((data & 0xff000000) == 0)
	{
		pending_command = 1;
		soundlatch_w(offset,(data >> 8) & 0xff);
		cpu_cause_interrupt(1,Z80_NMI_INT);
	}
}

static READ_HANDLER( pending_command_r )
{
	return pending_command;
}

static WRITE_HANDLER( pending_command_clear_w )
{
	pending_command = 0;
}

static WRITE_HANDLER( aerofgt_sh_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);
	int bankaddress;


	bankaddress = 0x10000 + (data & 0x03) * 0x8000;
	cpu_setbank(1,&RAM[bankaddress]);
}



static struct MemoryReadAddress pspikes_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x100000, 0x10ffff, MRA_BANK6 },
	{ 0x200000, 0x203fff, MRA_BANK4 },
	{ 0xff8000, 0xff8fff, aerofgt_bg1videoram_r },
	{ 0xffd000, 0xffdfff, aerofgt_rasterram_r },
	{ 0xffe000, 0xffefff, paletteram_word_r },
	{ 0xfff000, 0xfff001, input_port_0_r },
	{ 0xfff002, 0xfff003, input_port_1_r },
	{ 0xfff004, 0xfff005, input_port_2_r },
	{ 0xfff006, 0xfff007, pending_command_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress pspikes_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x100000, 0x10ffff, MWA_BANK6 },	/* work RAM */
	{ 0x200000, 0x203fff, MWA_BANK4, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0xff8000, 0xff8fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0xffc000, 0xffc3ff, aerofgt_spriteram_2_w, &spriteram_2, &spriteram_2_size },
	{ 0xffd000, 0xffdfff, aerofgt_rasterram_w, &aerofgt_rasterram },	/* bg1 scroll registers */
	{ 0xffe000, 0xffefff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0xfff000, 0xfff001, pspikes_palette_bank_w },
	{ 0xfff002, 0xfff003, pspikes_gfxbank_w },
	{ 0xfff004, 0xfff005, aerofgt_bg1scrolly_w },
	{ 0xfff006, 0xfff007, sound_command_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress karatblz_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x080000, 0x081fff, aerofgt_bg1videoram_r },
	{ 0x082000, 0x083fff, aerofgt_bg2videoram_r },
	{ 0x0a0000, 0x0affff, MRA_BANK4 },
	{ 0x0b0000, 0x0bffff, MRA_BANK5 },
	{ 0x0c0000, 0x0cffff, MRA_BANK6 },	/* work RAM */
	{ 0x0f8000, 0x0fbfff, aerofgt_workram_r },	/* work RAM */
	{ 0xff8000, 0xffbfff, aerofgt_workram_r },	/* mirror */
	{ 0x0fc000, 0x0fc7ff, aerofgt_spriteram_2_r },
	{ 0x0fe000, 0x0fe7ff, paletteram_word_r },
	{ 0x0ff000, 0x0ff001, input_port_0_r },
	{ 0x0ff002, 0x0ff003, input_port_1_r },
	{ 0x0ff004, 0x0ff005, input_port_2_r },
	{ 0x0ff006, 0x0ff007, input_port_3_r },
	{ 0x0ff008, 0x0ff009, input_port_4_r },
	{ 0x0ff00a, 0x0ff00b, pending_command_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress karatblz_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x080000, 0x081fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x082000, 0x083fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x0a0000, 0x0affff, MWA_BANK4, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x0b0000, 0x0bffff, MWA_BANK5, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x0c0000, 0x0cffff, MWA_BANK6 },	/* work RAM */
	{ 0x0f8000, 0x0fbfff, aerofgt_workram_w, &aerofgt_workram },	/* work RAM */
	{ 0xff8000, 0xffbfff, aerofgt_workram_w },	/* mirror */
	{ 0x0fc000, 0x0fc7ff, aerofgt_spriteram_2_w, &spriteram_2, &spriteram_2_size },
	{ 0x0fe000, 0x0fe7ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0x0ff002, 0x0ff003, karatblz_gfxbank_w },
	{ 0x0ff006, 0x0ff007, sound_command_w },
	{ 0x0ff008, 0x0ff009, aerofgt_bg1scrollx_w },
	{ 0x0ff00a, 0x0ff00b, aerofgt_bg1scrolly_w },
	{ 0x0ff00c, 0x0ff00d, aerofgt_bg2scrollx_w },
	{ 0x0ff00e, 0x0ff00f, aerofgt_bg2scrolly_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress spinlbrk_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x080000, 0x080fff, aerofgt_bg1videoram_r },
	{ 0x082000, 0x082fff, aerofgt_bg2videoram_r },
	{ 0xff8000, 0xffbfff, MRA_BANK6 },
	{ 0xffc000, 0xffc7ff, aerofgt_spriteram_2_r },
	{ 0xffd000, 0xffd1ff, aerofgt_rasterram_r },
	{ 0xffe000, 0xffe7ff, paletteram_word_r },
	{ 0xfff000, 0xfff001, input_port_0_r },
	{ 0xfff002, 0xfff003, input_port_1_r },
	{ 0xfff004, 0xfff005, input_port_2_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress spinlbrk_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x080000, 0x080fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x082000, 0x082fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0xff8000, 0xffbfff, MWA_BANK6 },	/* work RAM */
	{ 0xffc000, 0xffc7ff, aerofgt_spriteram_2_w, &spriteram_2, &spriteram_2_size },
	{ 0xffd000, 0xffd1ff, aerofgt_rasterram_w, &aerofgt_rasterram },	/* bg1 scroll registers */
	{ 0xffe000, 0xffe7ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0xfff000, 0xfff001, spinlbrk_gfxbank_w },
	{ 0xfff002, 0xfff003, aerofgt_bg2scrollx_w },
	{ 0xfff006, 0xfff007, sound_command_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress turbofrc_readmem[] =
{
	{ 0x000000, 0x0bffff, MRA_ROM },
	{ 0x0c0000, 0x0cffff, MRA_BANK6 },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, aerofgt_bg1videoram_r },
	{ 0x0d2000, 0x0d3fff, aerofgt_bg2videoram_r },
	{ 0x0e0000, 0x0e3fff, MRA_BANK4 },
	{ 0x0e4000, 0x0e7fff, MRA_BANK5 },
	{ 0x0f8000, 0x0fbfff, aerofgt_workram_r },	/* work RAM */
	{ 0xff8000, 0xffbfff, aerofgt_workram_r },	/* mirror */
	{ 0x0fc000, 0x0fc7ff, aerofgt_spriteram_2_r },
	{ 0xffc000, 0xffc7ff, aerofgt_spriteram_2_r },	/* mirror */
	{ 0x0fd000, 0x0fdfff, aerofgt_rasterram_r },
	{ 0xffd000, 0xffdfff, aerofgt_rasterram_r },	/* mirror */
	{ 0x0fe000, 0x0fe7ff, paletteram_word_r },
	{ 0xfff000, 0xfff001, input_port_0_r },
	{ 0xfff002, 0xfff003, input_port_1_r },
	{ 0xfff004, 0xfff005, input_port_2_r },
	{ 0xfff006, 0xfff007, pending_command_r },
	{ 0xfff008, 0xfff009, input_port_3_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress turbofrc_writemem[] =
{
	{ 0x000000, 0x0bffff, MWA_ROM },
	{ 0x0c0000, 0x0cffff, MWA_BANK6 },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x0d2000, 0x0d3fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x0e0000, 0x0e3fff, MWA_BANK4, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x0e4000, 0x0e7fff, MWA_BANK5, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x0f8000, 0x0fbfff, aerofgt_workram_w, &aerofgt_workram },	/* work RAM */
	{ 0xff8000, 0xffbfff, aerofgt_workram_w },	/* mirror */
	{ 0x0fc000, 0x0fc7ff, aerofgt_spriteram_2_w, &spriteram_2, &spriteram_2_size },
	{ 0xffc000, 0xffc7ff, aerofgt_spriteram_2_w },	/* mirror */
	{ 0x0fd000, 0x0fdfff, aerofgt_rasterram_w, &aerofgt_rasterram },	/* bg1 scroll registers */
	{ 0xffd000, 0xffdfff, aerofgt_rasterram_w },	/* mirror */
	{ 0x0fe000, 0x0fe7ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0xfff002, 0xfff003, aerofgt_bg1scrolly_w },
	{ 0xfff004, 0xfff005, aerofgt_bg2scrollx_w },
	{ 0xfff006, 0xfff007, aerofgt_bg2scrolly_w },
	{ 0xfff008, 0xfff00b, turbofrc_gfxbank_w },
	{ 0xfff00c, 0xfff00d, MWA_NOP },	/* related to bg2 (written together with the scroll registers) */
	{ 0xfff00e, 0xfff00f, turbofrc_sound_command_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress aerofgtb_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x0c0000, 0x0cffff, MRA_BANK6 },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, aerofgt_bg1videoram_r },
	{ 0x0d2000, 0x0d3fff, aerofgt_bg2videoram_r },
	{ 0x0e0000, 0x0e3fff, MRA_BANK4 },
	{ 0x0e4000, 0x0e7fff, MRA_BANK5 },
	{ 0x0f8000, 0x0fbfff, aerofgt_workram_r },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, aerofgt_spriteram_2_r },
	{ 0x0fd000, 0x0fd7ff, paletteram_word_r },
	{ 0x0fe000, 0x0fe001, input_port_0_r },
	{ 0x0fe002, 0x0fe003, input_port_1_r },
	{ 0x0fe004, 0x0fe005, input_port_2_r },
	{ 0x0fe006, 0x0fe007, pending_command_r },
	{ 0x0fe008, 0x0fe009, input_port_3_r },
	{ 0x0ff000, 0x0fffff, aerofgt_rasterram_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress aerofgtb_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x0c0000, 0x0cffff, MWA_BANK6 },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x0d2000, 0x0d3fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x0e0000, 0x0e3fff, MWA_BANK4, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x0e4000, 0x0e7fff, MWA_BANK5, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x0f8000, 0x0fbfff, aerofgt_workram_w, &aerofgt_workram },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, aerofgt_spriteram_2_w, &spriteram_2, &spriteram_2_size },
	{ 0x0fd000, 0x0fd7ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0x0fe002, 0x0fe003, aerofgt_bg1scrolly_w },
	{ 0x0fe004, 0x0fe005, aerofgt_bg2scrollx_w },
	{ 0x0fe006, 0x0fe007, aerofgt_bg2scrolly_w },
	{ 0x0fe008, 0x0fe00b, turbofrc_gfxbank_w },
	{ 0x0fe00e, 0x0fe00f, turbofrc_sound_command_w },
	{ 0x0ff000, 0x0fffff, aerofgt_rasterram_w, &aerofgt_rasterram },	/* used only for the scroll registers */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress aerofgt_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x1a0000, 0x1a07ff, paletteram_word_r },
	{ 0x1b0000, 0x1b07ff, aerofgt_rasterram_r },
	{ 0x1b0800, 0x1b0801, MRA_NOP },	/* ??? */
	{ 0x1b0ff0, 0x1b0fff, MRA_BANK7 },	/* stack area during boot */
	{ 0x1b2000, 0x1b3fff, aerofgt_bg1videoram_r },
	{ 0x1b4000, 0x1b5fff, aerofgt_bg2videoram_r },
	{ 0x1c0000, 0x1c3fff, MRA_BANK4 },
	{ 0x1c4000, 0x1c7fff, MRA_BANK5 },
	{ 0x1d0000, 0x1d1fff, aerofgt_spriteram_2_r },
	{ 0xfef000, 0xffefff, aerofgt_workram_r },	/* work RAM */
	{ 0xffffa0, 0xffffa1, input_port_0_r },
	{ 0xffffa2, 0xffffa3, input_port_1_r },
	{ 0xffffa4, 0xffffa5, input_port_2_r },
	{ 0xffffa6, 0xffffa7, input_port_3_r },
	{ 0xffffa8, 0xffffa9, input_port_4_r },
	{ 0xffffac, 0xffffad, pending_command_r },
	{ 0xffffae, 0xffffaf, input_port_5_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress aerofgt_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x1a0000, 0x1a07ff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0x1b0000, 0x1b07ff, aerofgt_rasterram_w, &aerofgt_rasterram },	/* used only for the scroll registers */
	{ 0x1b0800, 0x1b0801, MWA_NOP },	/* ??? */
	{ 0x1b0ff0, 0x1b0fff, MWA_BANK7 },	/* stack area during boot */
	{ 0x1b2000, 0x1b3fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x1b4000, 0x1b5fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x1c0000, 0x1c3fff, MWA_BANK4, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x1c4000, 0x1c7fff, MWA_BANK5, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x1d0000, 0x1d1fff, aerofgt_spriteram_2_w, &spriteram_2, &spriteram_2_size },
	{ 0xfef000, 0xffefff, aerofgt_workram_w, &aerofgt_workram },	/* work RAM */
	{ 0xffff80, 0xffff87, aerofgt_gfxbank_w },
	{ 0xffff88, 0xffff89, aerofgt_bg1scrolly_w },	/* + something else in the top byte */
	{ 0xffff90, 0xffff91, aerofgt_bg2scrolly_w },	/* + something else in the top byte */
	{ 0xffffac, 0xffffad, MWA_NOP },	/* ??? */
	{ 0xffffc0, 0xffffc1, sound_command_w },
	{ -1 }  /* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x77ff, MRA_ROM },
	{ 0x7800, 0x7fff, MRA_RAM },
	{ 0x8000, 0xffff, MRA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x77ff, MWA_ROM },
	{ 0x7800, 0x7fff, MWA_RAM },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct IOReadPort turbofrc_sound_readport[] =
{
	{ 0x14, 0x14, soundlatch_r },
	{ 0x18, 0x18, YM2610_status_port_0_A_r },
	{ 0x1a, 0x1a, YM2610_status_port_0_B_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort turbofrc_sound_writeport[] =
{
	{ 0x00, 0x00, aerofgt_sh_bankswitch_w },
	{ 0x14, 0x14, pending_command_clear_w },
	{ 0x18, 0x18, YM2610_control_port_0_A_w },
	{ 0x19, 0x19, YM2610_data_port_0_A_w },
	{ 0x1a, 0x1a, YM2610_control_port_0_B_w },
	{ 0x1b, 0x1b, YM2610_data_port_0_B_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort aerofgt_sound_readport[] =
{
	{ 0x00, 0x00, YM2610_status_port_0_A_r },
	{ 0x02, 0x02, YM2610_status_port_0_B_r },
	{ 0x0c, 0x0c, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort aerofgt_sound_writeport[] =
{
	{ 0x00, 0x00, YM2610_control_port_0_A_w },
	{ 0x01, 0x01, YM2610_data_port_0_A_w },
	{ 0x02, 0x02, YM2610_control_port_0_B_w },
	{ 0x03, 0x03, YM2610_data_port_0_B_w },
	{ 0x04, 0x04, aerofgt_sh_bankswitch_w },
	{ 0x08, 0x08, pending_command_clear_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( pspikes )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0003, 0x0003, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_2C ) )
	/* the following two select country in the Chinese version (ROMs not available) */
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0100, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0600, 0x0600, "1 Player Starting Score" )
	PORT_DIPSETTING(      0x0600, "12-12" )
	PORT_DIPSETTING(      0x0400, "11-11" )
	PORT_DIPSETTING(      0x0200, "11-12" )
	PORT_DIPSETTING(      0x0000, "10-12" )
	PORT_DIPNAME( 0x1800, 0x1800, "2 Players Starting Score" )
	PORT_DIPSETTING(      0x1800, "9-9" )
	PORT_DIPSETTING(      0x1000, "7-7" )
	PORT_DIPSETTING(      0x0800, "5-5" )
	PORT_DIPSETTING(      0x0000, "0-0" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x2000, "Normal" )
	PORT_DIPSETTING(      0x0000, "Hard" )
	PORT_DIPNAME( 0x4000, 0x4000, "2 Players Time per Credit" )
	PORT_DIPSETTING(      0x4000, "3 min" )
	PORT_DIPSETTING(      0x0000, "2 min" )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( karatblz )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER4 )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0008, 0x0008, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPNAME( 0x0060, 0x0060, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0060, "2 Players" )
	PORT_DIPSETTING(      0x0040, "3 Players" )
	PORT_DIPSETTING(      0x0020, "4 Players" )
	PORT_DIPSETTING(      0x0000, "4 Players (Team)" )
	/*  With 4 player (Team) selected and Same Coin Slot:
		Coin A & B credit together for use by _only_ player 1 or player 2
		Coin C & D credit together for use by _only_ player 3 or player 4
		Otherwise with Individual selected, everyone is seperate  */
	PORT_DIPNAME( 0x0080, 0x0080, "Coin Slot" )
	PORT_DIPSETTING(      0x0080, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_SERVICE( 0x0100, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0600, 0x0600, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0400, "Easy" )
	PORT_DIPSETTING(      0x0600, "Normal" )
	PORT_DIPSETTING(      0x0200, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "Freeze" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( spinlbrk )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x000f, 0x000f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x000f, "1 Credit 1 Health Pack" )	/* I chose "Health Packs" as the actual value can change */
	PORT_DIPSETTING(      0x000e, "1 Credit 2 Health Packs" )	/*  via dipswitch 2-7 (0x4000) see below */
	PORT_DIPSETTING(      0x000d, "1 Credit 3 Health Packs" )
	PORT_DIPSETTING(      0x000c, "1 Credit 4 Health Packs" )
	PORT_DIPSETTING(      0x000b, "1 Credit 5 Health Packs" )
	PORT_DIPSETTING(      0x000a, "1 Credit 6 Health Packs" )
	PORT_DIPSETTING(      0x0009, "2 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0008, "3 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0007, "4 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0006, "5 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0005, "2 Credits 2 Health Packs" )
	PORT_DIPSETTING(      0x0004, "2-1-1C  1-1-1 HPs" )
	PORT_DIPSETTING(      0x0003, "2-2C 1-2 HPs" )
	PORT_DIPSETTING(      0x0002, "1-1-1-1-1C 1-1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0001, "1-1-1-1C 1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0000, "1-1C 1-2 HPs" )
/* The last 5 Coin/Credit selections are cycles:
	Example: 0x0004 = 2-1-1C 1-1-1 HPs:
	2 Credits for the 1st Health Pack, 1 Credit for the 2nd Health Pack, 1 Credit
	for the 3rd Health Pack... Then back to 2 Credits agian for 1 HP, then 1 credit
	and 1 credit.... on and on.  With all Coin/Credit dips set to on, it's 1 Health
	Pack for odd credits, 2 Health Packs for even credits :p
	*/
	PORT_DIPNAME( 0x00f0, 0x00f0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x00f0, "1 Credit 1 Health Pack" )
	PORT_DIPSETTING(      0x00e0, "1 Credit 2 Health Packs" )
	PORT_DIPSETTING(      0x00d0, "1 Credit 3 Health Packs" )
	PORT_DIPSETTING(      0x00c0, "1 Credit 4 Health Packs" )
	PORT_DIPSETTING(      0x00b0, "1 Credit 5 Health Packs" )
	PORT_DIPSETTING(      0x00a0, "1 Credit 6 Health Packs" )
	PORT_DIPSETTING(      0x0090, "2 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0080, "3 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0070, "4 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0060, "5 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0050, "2 Credits 2 Health Packs" )
	PORT_DIPSETTING(      0x0040, "2-1-1C  1-1-1 HPs" )
	PORT_DIPSETTING(      0x0030, "2-2C 1-2 HPs" )
	PORT_DIPSETTING(      0x0020, "1-1-1-1-1C 1-1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0010, "1-1-1-1C 1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0000, "1-1C 1-2 HPs" )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0300, "Normal" )
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0400, "Coin Slot" )		/* Japan ver acts like "Same" is always selected */
	PORT_DIPSETTING(      0x0000, "Same" )
	PORT_DIPSETTING(      0x0400, "Individual" )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "Player Lever Check?" ) /* Possibly Demo Sound??? but causes lever error??? */
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x2000, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x4000, 0x4000, "Health Pack" )
	PORT_DIPSETTING(      0x4000, "20 Hitpoints" )			/* World and Japan ver this value is 32 */
	PORT_DIPSETTING(      0x0000, "32 Hitpoints" )			/* World and Japan ver this value is 40 */
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )			/* This DIP has no effect that I can see */
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )			/*  most likely "Bonus Life" */
INPUT_PORTS_END

INPUT_PORTS_START( turbofrc )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE )	/* "TEST" */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )//START1 )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0008, 0x0008, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Coin Slot" )
	PORT_DIPSETTING(      0x0010, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_DIPNAME( 0x0020, 0x0000, "Max Players" )
	PORT_DIPSETTING(      0x0020, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x1000, "3" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

INPUT_PORTS_START( aerofgtb )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Coin Slot" )
	PORT_DIPSETTING(      0x0001, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_DIPNAME( 0x000e, 0x000e, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0070, 0x0070, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0050, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0070, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0080, 0x0080, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0800, "Easy" )
	PORT_DIPSETTING(      0x0c00, "Normal" )
	PORT_DIPSETTING(      0x0400, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x2000, "1" )
	PORT_DIPSETTING(      0x1000, "2" )
	PORT_DIPSETTING(      0x3000, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x4000, "200000" )
	PORT_DIPSETTING(      0x0000, "300000" )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0000, "Country" )
	PORT_DIPSETTING(      0x0000, "Japan" )
	PORT_DIPSETTING(      0x0001, "Taiwan" )
	/* TODO: there are others in the table at 11910 */
	/* this port is checked at 1b080 */
INPUT_PORTS_END

INPUT_PORTS_START( aerofgt )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Coin Slot" )
	PORT_DIPSETTING(      0x0001, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_DIPNAME( 0x000e, 0x000e, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0070, 0x0070, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0050, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0070, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0080, 0x0080, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0008, "Easy" )
	PORT_DIPSETTING(      0x000c, "Normal" )
	PORT_DIPSETTING(      0x0004, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0020, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0040, "200000" )
	PORT_DIPSETTING(      0x0000, "300000" )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x000f, 0x0000, "Country" )
	PORT_DIPSETTING(      0x0000, "Any" )
	PORT_DIPSETTING(      0x000f, "USA" )
	PORT_DIPSETTING(      0x000e, "Korea" )
	PORT_DIPSETTING(      0x000d, "Hong Kong" )
	PORT_DIPSETTING(      0x000b, "Taiwan" )
INPUT_PORTS_END



static struct GfxLayout pspikes_charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout aerofgt_charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout pspikes_spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ 0, 1, 2, 3 },
	{ 1*4, 0*4, 3*4, 2*4, RGN_FRAC(1,2)+1*4, RGN_FRAC(1,2)+0*4, RGN_FRAC(1,2)+3*4, RGN_FRAC(1,2)+2*4,
			5*4, 4*4, 7*4, 6*4, RGN_FRAC(1,2)+5*4, RGN_FRAC(1,2)+4*4, RGN_FRAC(1,2)+7*4, RGN_FRAC(1,2)+6*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8
};

static struct GfxLayout aerofgtb_spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ 0, 1, 2, 3 },
	{ 3*4, 2*4, 1*4, 0*4, RGN_FRAC(1,2)+3*4, RGN_FRAC(1,2)+2*4, RGN_FRAC(1,2)+1*4, RGN_FRAC(1,2)+0*4,
			7*4, 6*4, 5*4, 4*4, RGN_FRAC(1,2)+7*4, RGN_FRAC(1,2)+6*4, RGN_FRAC(1,2)+5*4, RGN_FRAC(1,2)+4*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8
};

static struct GfxLayout aerofgt_spritelayout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4,
			10*4, 11*4, 8*4, 9*4, 14*4, 15*4, 12*4, 13*4 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8
};

static struct GfxDecodeInfo pspikes_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pspikes_charlayout,      0, 64 },	/* colors    0-1023 in 8 banks */
	{ REGION_GFX2, 0, &pspikes_spritelayout, 1024, 64 },	/* colors 1024-2047 in 4 banks */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo turbofrc_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pspikes_charlayout,     0, 16 },
	{ REGION_GFX2, 0, &pspikes_charlayout,   256, 16 },
	{ REGION_GFX3, 0, &pspikes_spritelayout, 512, 16 },
	{ REGION_GFX4, 0, &pspikes_spritelayout, 768, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo aerofgtb_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pspikes_charlayout,      0, 16 },
	{ REGION_GFX2, 0, &pspikes_charlayout,    256, 16 },
	{ REGION_GFX3, 0, &aerofgtb_spritelayout, 512, 16 },
	{ REGION_GFX4, 0, &aerofgtb_spritelayout, 768, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo aerofgt_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &aerofgt_charlayout,     0, 16 },
	{ REGION_GFX1, 0, &aerofgt_charlayout,   256, 16 },
	{ REGION_GFX2, 0, &aerofgt_spritelayout, 512, 16 },
	{ REGION_GFX3, 0, &aerofgt_spritelayout, 768, 16 },
	{ -1 } /* end of array */
};



static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2610interface ym2610_interface =
{
	1,
	8000000,	/* 8 MHz??? */
	{ 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler },
	{ REGION_SOUND1 },
	{ REGION_SOUND2 },
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) }
};



static struct MachineDriver machine_driver_pspikes =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			20000000/2,	/* 10 MHz (?) */
			pspikes_readmem,pspikes_writemem,0,0,
			m68_level1_irq,1	/* all irq vectors are the same */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,	/* 4 Mhz ??? */
			sound_readmem,sound_writemem,turbofrc_sound_readport,turbofrc_sound_writeport,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 2*8-4, 44*8-4-1, 1*8, 29*8-1 },
	pspikes_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	pspikes_vh_start,
	0,
	pspikes_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface,
		}
	}
};

static struct MachineDriver machine_driver_karatblz =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			20000000/2,	/* 10 MHz (?) */
			karatblz_readmem,karatblz_writemem,0,0,
			m68_level1_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,	/* 4 Mhz ??? */
			sound_readmem,sound_writemem,turbofrc_sound_readport,turbofrc_sound_writeport,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 1*8, 45*8-1, 0*8, 30*8-1 },
	turbofrc_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	karatblz_vh_start,
	0,
	karatblz_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface,
		}
	}
};

static struct MachineDriver machine_driver_spinlbrk =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			20000000/2,	/* 10 MHz (?) */
			spinlbrk_readmem,spinlbrk_writemem,0,0,
			m68_level1_irq,1	/* there are vectors for 3 and 4 too */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,	/* 4 Mhz ??? */
			sound_readmem,sound_writemem,turbofrc_sound_readport,turbofrc_sound_writeport,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 1*8, 45*8-1, 0*8, 30*8-1 },
	turbofrc_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	spinlbrk_vh_start,
	0,
	spinlbrk_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface,
		}
	}
};

static struct MachineDriver machine_driver_turbofrc =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			20000000/2,	/* 10 MHz (?) */
			turbofrc_readmem,turbofrc_writemem,0,0,
			m68_level1_irq,1	/* all irq vectors are the same */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,	/* 4 Mhz ??? */
			sound_readmem,sound_writemem,turbofrc_sound_readport,turbofrc_sound_writeport,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 1*8, 44*8-1, 0*8, 30*8-1 },
	turbofrc_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	turbofrc_vh_start,
	0,
	turbofrc_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface,
		}
	}
};

static struct MachineDriver machine_driver_aerofgtb =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			20000000/2,	/* 10 MHz (?) */
			aerofgtb_readmem,aerofgtb_writemem,0,0,
			m68_level1_irq,1	/* all irq vectors are the same */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,	/* 4 Mhz ??? */
			sound_readmem,sound_writemem,aerofgt_sound_readport,aerofgt_sound_writeport,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2610 */
		}
	},
	60, 500,	/* frames per second, vblank duration */
				/* wrong but improves sprite-background synchronization */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 0*8+12, 40*8-1+12, 0*8, 28*8-1 },
	aerofgtb_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	turbofrc_vh_start,
	0,
	turbofrc_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface,
		}
	}
};

static struct MachineDriver machine_driver_aerofgt =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			20000000/2,	/* 10 MHz (?) */
			aerofgt_readmem,aerofgt_writemem,0,0,
			m68_level1_irq,1	/* all irq vectors are the same */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,	/* 4 Mhz ??? */
			sound_readmem,sound_writemem,aerofgt_sound_readport,aerofgt_sound_writeport,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2610 */
		}
	},
	60, 400,	/* frames per second, vblank duration */
				/* wrong but improves sprite-background synchronization */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	aerofgt_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	turbofrc_vh_start,
	0,
	aerofgt_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface,
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( pspikes )
	ROM_REGION( 0xc0000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_WIDE_SWAP( "20",           0x00000, 0x40000, 0x75cdcee2 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "19",           0x00000, 0x20000, 0x7e8ed6e5 )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g7h",          0x000000, 0x80000, 0x74c23c3d )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g7j",          0x000000, 0x80000, 0x0b9e4739 )
	ROM_LOAD( "g7l",          0x080000, 0x80000, 0x943139ff )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "a47",          0x00000, 0x40000, 0xc6779dfa )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "o5b",          0x000000, 0x100000, 0x07d6cbac )
ROM_END

ROM_START( svolly91 )
	ROM_REGION( 0xc0000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_WIDE_SWAP( "u11.jpn",      0x00000, 0x40000, 0xea2e4c82 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "19",           0x00000, 0x20000, 0x7e8ed6e5 )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g7h",          0x000000, 0x80000, 0x74c23c3d )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g7j",          0x000000, 0x80000, 0x0b9e4739 )
	ROM_LOAD( "g7l",          0x080000, 0x80000, 0x943139ff )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "a47",          0x00000, 0x40000, 0xc6779dfa )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "o5b",          0x000000, 0x100000, 0x07d6cbac )
ROM_END

ROM_START( spinlbrk )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "ic98",    0x00000, 0x10000, 0x36c2bf70 )
	ROM_LOAD_ODD ( "ic104",   0x00000, 0x10000, 0x34a7e158 )
	ROM_LOAD_EVEN( "ic93",    0x20000, 0x10000, 0x726f4683 )
	ROM_LOAD_ODD ( "ic94",    0x20000, 0x10000, 0xc4385e03 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "ic117",        0x00000, 0x08000, 0x625ada41 )
	ROM_LOAD( "ic118",        0x10000, 0x10000, 0x1025f024 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic15",         0x000000, 0x80000, 0xe318cf3a )
	ROM_LOAD( "ic9",          0x080000, 0x80000, 0xe071f674 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic17",         0x000000, 0x80000, 0xa63d5a55 )
	ROM_LOAD( "ic11",         0x080000, 0x80000, 0x7dcc913d )
	ROM_LOAD( "ic16",         0x100000, 0x80000, 0x0d84af7f )	//FIRST AND SECOND HALF IDENTICAL

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic12",         0x000000, 0x80000, 0xd63fac4e )
	ROM_LOAD( "ic18",         0x080000, 0x80000, 0x5a60444b )

	ROM_REGION( 0x200000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic14",         0x000000, 0x80000, 0x1befd0f3 )
	ROM_LOAD( "ic20",         0x080000, 0x80000, 0xc2f84a61 )
	ROM_LOAD( "ic35",         0x100000, 0x80000, 0xeba8e1a3 )
	ROM_LOAD( "ic40",         0x180000, 0x80000, 0x5ef5aa7e )

	ROM_REGION( 0x24000, REGION_GFX5 )	/* hardcoded sprite maps */
	ROM_LOAD_EVEN( "ic19",    0x00000, 0x10000, 0xdb24eeaa )
	ROM_LOAD_ODD ( "ic13",    0x00000, 0x10000, 0x97025bf4 )
	/* 20000-23fff empty space, filled in vh_startup */

	ROM_REGION( 0x80000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "ic166",        0x000000, 0x80000, 0x6e0d063a )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "ic163",        0x000000, 0x80000, 0xe6621dfb )	//FIRST AND SECOND HALF IDENTICAL
ROM_END

ROM_START( spinlbru )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "ic98.u5", 0x00000, 0x10000, 0x3a0f7667 )
	ROM_LOAD_ODD ( "ic104.u6",0x00000, 0x10000, 0xa0e0af31 )
	ROM_LOAD_EVEN( "ic93.u4", 0x20000, 0x10000, 0x0cf73029 )
	ROM_LOAD_ODD ( "ic94.u3", 0x20000, 0x10000, 0x5cf7c426 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "ic117",        0x00000, 0x08000, 0x625ada41 )
	ROM_LOAD( "ic118",        0x10000, 0x10000, 0x1025f024 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic15",         0x000000, 0x80000, 0xe318cf3a )
	ROM_LOAD( "ic9",          0x080000, 0x80000, 0xe071f674 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic17",         0x000000, 0x80000, 0xa63d5a55 )
	ROM_LOAD( "ic11",         0x080000, 0x80000, 0x7dcc913d )
	ROM_LOAD( "ic16",         0x100000, 0x80000, 0x0d84af7f )	//FIRST AND SECOND HALF IDENTICAL

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic12",         0x000000, 0x80000, 0xd63fac4e )
	ROM_LOAD( "ic18",         0x080000, 0x80000, 0x5a60444b )

	ROM_REGION( 0x200000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic14",         0x000000, 0x80000, 0x1befd0f3 )
	ROM_LOAD( "ic20",         0x080000, 0x80000, 0xc2f84a61 )
	ROM_LOAD( "ic35",         0x100000, 0x80000, 0xeba8e1a3 )
	ROM_LOAD( "ic40",         0x180000, 0x80000, 0x5ef5aa7e )

	ROM_REGION( 0x24000, REGION_GFX5 )	/* hardcoded sprite maps */
	ROM_LOAD_EVEN( "ic19",    0x00000, 0x10000, 0xdb24eeaa )
	ROM_LOAD_ODD ( "ic13",    0x00000, 0x10000, 0x97025bf4 )
	/* 20000-23fff empty space, filled in vh_startup */

	ROM_REGION( 0x80000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "ic166",        0x000000, 0x80000, 0x6e0d063a )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "ic163",        0x000000, 0x80000, 0xe6621dfb )	//FIRST AND SECOND HALF IDENTICAL
ROM_END

ROM_START( spinlbrj )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "j5",      0x00000, 0x10000, 0x6a3d690e )
	ROM_LOAD_ODD ( "j6",      0x00000, 0x10000, 0x869593fa )
	ROM_LOAD_EVEN( "j4",      0x20000, 0x10000, 0x33e33912 )
	ROM_LOAD_ODD ( "j3",      0x20000, 0x10000, 0x16ca61d0 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "ic117",        0x00000, 0x08000, 0x625ada41 )
	ROM_LOAD( "ic118",        0x10000, 0x10000, 0x1025f024 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic15",         0x000000, 0x80000, 0xe318cf3a )
	ROM_LOAD( "ic9",          0x080000, 0x80000, 0xe071f674 )

	ROM_REGION( 0x200000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic17",         0x000000, 0x80000, 0xa63d5a55 )
	ROM_LOAD( "ic11",         0x080000, 0x80000, 0x7dcc913d )
	ROM_LOAD( "ic16",         0x100000, 0x80000, 0x0d84af7f )	//FIRST AND SECOND HALF IDENTICAL

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic12",         0x000000, 0x80000, 0xd63fac4e )
	ROM_LOAD( "ic18",         0x080000, 0x80000, 0x5a60444b )

	ROM_REGION( 0x200000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic14",         0x000000, 0x80000, 0x1befd0f3 )
	ROM_LOAD( "ic20",         0x080000, 0x80000, 0xc2f84a61 )
	ROM_LOAD( "ic35",         0x100000, 0x80000, 0xeba8e1a3 )
	ROM_LOAD( "ic40",         0x180000, 0x80000, 0x5ef5aa7e )

	ROM_REGION( 0x24000, REGION_GFX5 )	/* hardcoded sprite maps */
	ROM_LOAD_EVEN( "ic19",    0x00000, 0x10000, 0xdb24eeaa )
	ROM_LOAD_ODD ( "ic13",    0x00000, 0x10000, 0x97025bf4 )
	/* 20000-23fff empty space, filled in vh_startup */

	ROM_REGION( 0x80000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "ic166",        0x000000, 0x80000, 0x6e0d063a )

	ROM_REGION( 0x80000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "ic163",        0x000000, 0x80000, 0xe6621dfb )	//FIRST AND SECOND HALF IDENTICAL
ROM_END

ROM_START( karatblz )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_WIDE_SWAP( "rom2v3",  0x00000, 0x40000, 0x01f772e1 )
	ROM_LOAD_WIDE_SWAP( "1.u15",   0x40000, 0x40000, 0xd16ee21b )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "5.u92",        0x00000, 0x20000, 0x97d67510 )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gha.u55",      0x00000, 0x80000, 0x3e0cea91 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gh9.u61",      0x00000, 0x80000, 0x5d1676bd )

	ROM_REGION( 0x400000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "u42",          0x000000, 0x100000, 0x65f0da84 )
	ROM_LOAD( "3.u44",        0x100000, 0x020000, 0x34bdead2 )
	ROM_LOAD( "u43",          0x200000, 0x100000, 0x7b349e5d )
	ROM_LOAD( "4.u45",        0x300000, 0x020000, 0xbe4d487d )

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "u59.ghb",      0x000000, 0x80000, 0x158c9cde )
	ROM_LOAD( "ghd.u60",      0x080000, 0x80000, 0x73180ae3 )

	ROM_REGION( 0x080000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "u105.gh8",     0x000000, 0x080000, 0x7a68cb1b )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "u104",         0x000000, 0x100000, 0x5795e884 )
ROM_END

ROM_START( karatblu )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_WIDE_SWAP( "2.u14",   0x00000, 0x40000, 0x202e6220 )
	ROM_LOAD_WIDE_SWAP( "1.u15",   0x40000, 0x40000, 0xd16ee21b )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "5.u92",        0x00000, 0x20000, 0x97d67510 )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gha.u55",      0x00000, 0x80000, 0x3e0cea91 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gh9.u61",      0x00000, 0x80000, 0x5d1676bd )

	ROM_REGION( 0x400000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "u42",          0x000000, 0x100000, 0x65f0da84 )
	ROM_LOAD( "3.u44",        0x100000, 0x020000, 0x34bdead2 )
	ROM_LOAD( "u43",          0x200000, 0x100000, 0x7b349e5d )
	ROM_LOAD( "4.u45",        0x300000, 0x020000, 0xbe4d487d )

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "u59.ghb",      0x000000, 0x80000, 0x158c9cde )
	ROM_LOAD( "ghd.u60",      0x080000, 0x80000, 0x73180ae3 )

	ROM_REGION( 0x080000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "u105.gh8",     0x000000, 0x080000, 0x7a68cb1b )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "u104",         0x000000, 0x100000, 0x5795e884 )
ROM_END

ROM_START( turbofrc )
	ROM_REGION( 0xc0000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_WIDE_SWAP( "tfrc2.bin",    0x00000, 0x40000, 0x721300ee )
	ROM_LOAD_WIDE_SWAP( "tfrc1.bin",    0x40000, 0x40000, 0x6cd5312b )
	ROM_LOAD_WIDE_SWAP( "tfrc3.bin",    0x80000, 0x40000, 0x63f50557 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "tfrcu166.bin", 0x00000, 0x20000, 0x2ca14a65 )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x0a0000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tfrcu94.bin",  0x000000, 0x80000, 0xbaa53978 )
	ROM_LOAD( "tfrcu95.bin",  0x080000, 0x20000, 0x71a6c573 )

	ROM_REGION( 0x0a0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tfrcu105.bin", 0x000000, 0x80000, 0x4de4e59e )
	ROM_LOAD( "tfrcu106.bin", 0x080000, 0x20000, 0xc6479eb5 )

	ROM_REGION( 0x200000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tfrcu116.bin", 0x000000, 0x80000, 0xdf210f3b )
	ROM_LOAD( "tfrcu118.bin", 0x080000, 0x40000, 0xf61d1d79 )
	ROM_LOAD( "tfrcu117.bin", 0x100000, 0x80000, 0xf70812fd )
	ROM_LOAD( "tfrcu119.bin", 0x180000, 0x40000, 0x474ea716 )

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tfrcu134.bin", 0x000000, 0x80000, 0x487330a2 )
	ROM_LOAD( "tfrcu135.bin", 0x080000, 0x80000, 0x3a7e5b6d )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "tfrcu180.bin",   0x00000, 0x20000, 0x39c7c7d5 )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "tfrcu179.bin", 0x000000, 0x100000, 0x60ca0333 )
ROM_END

ROM_START( aerofgt )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_WIDE_SWAP( "1.u4",         0x00000, 0x80000, 0x6fdff0a2 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "2.153",        0x00000, 0x20000, 0xa1ef64ec )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "538a54.124",   0x000000, 0x80000, 0x4d2c4df2 )
	ROM_LOAD( "1538a54.124",  0x080000, 0x80000, 0x286d109e )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "538a53.u9",    0x000000, 0x100000, 0x630d8e0b )

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "534g8f.u18",   0x000000, 0x80000, 0x76ce0926 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, 0x6d42723d )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, 0xcdbbdb1d )
ROM_END

ROM_START( aerofgtb )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "v2",                0x00000, 0x40000, 0x5c9de9f0 )
	ROM_LOAD_ODD ( "v1",                0x00000, 0x40000, 0x89c1dcf4 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "v3",           0x00000, 0x20000, 0xcbb18cf4 )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-03",     0x000000, 0x80000, 0x85eba1a4 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-02",     0x000000, 0x80000, 0x4f57f8ba )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-04",     0x000000, 0x80000, 0x3b329c1f )
	ROM_LOAD( "it-19-05",     0x080000, 0x80000, 0x02b525af )

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g27",          0x000000, 0x40000, 0x4d89cbc8 )
	ROM_LOAD( "g26",          0x040000, 0x40000, 0x8072c1d2 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, 0x6d42723d )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, 0xcdbbdb1d )
ROM_END

ROM_START( aerofgtc )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "v2.149",            0x00000, 0x40000, 0xf187aec6 )
	ROM_LOAD_ODD ( "v1.111",            0x00000, 0x40000, 0x9e684b19 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "2.153",        0x00000, 0x20000, 0xa1ef64ec )
	ROM_RELOAD(               0x10000, 0x20000 )

	/* gfx ROMs were missing in this set, I'm using the aerofgtb ones */
	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-03",     0x000000, 0x80000, 0x85eba1a4 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-02",     0x000000, 0x80000, 0x4f57f8ba )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-04",     0x000000, 0x80000, 0x3b329c1f )
	ROM_LOAD( "it-19-05",     0x080000, 0x80000, 0x02b525af )

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g27",          0x000000, 0x40000, 0x4d89cbc8 )
	ROM_LOAD( "g26",          0x040000, 0x40000, 0x8072c1d2 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, 0x6d42723d )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, 0xcdbbdb1d )
ROM_END

ROM_START( sonicwi )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN( "2.149",             0x00000, 0x40000, 0x3d1b96ba )
	ROM_LOAD_ODD ( "1.111",             0x00000, 0x40000, 0xa3d09f94 )

	ROM_REGION( 0x30000, REGION_CPU2 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "2.153",        0x00000, 0x20000, 0xa1ef64ec )	// 3.156
	ROM_RELOAD(               0x10000, 0x20000 )

	/* gfx ROMs were missing in this set, I'm using the aerofgtb ones */
	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-03",     0x000000, 0x80000, 0x85eba1a4 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-02",     0x000000, 0x80000, 0x4f57f8ba )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "it-19-04",     0x000000, 0x80000, 0x3b329c1f )
	ROM_LOAD( "it-19-05",     0x080000, 0x80000, 0x02b525af )

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "g27",          0x000000, 0x40000, 0x4d89cbc8 )
	ROM_LOAD( "g26",          0x040000, 0x40000, 0x8072c1d2 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, 0x6d42723d )

	ROM_REGION( 0x100000, REGION_SOUND2 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, 0xcdbbdb1d )
ROM_END



GAMEX( 1991, pspikes,  0,        pspikes,  pspikes,  0, ROT0,   "Video System Co.", "Power Spikes (Korea)", GAME_NO_COCKTAIL )
GAMEX( 1991, svolly91, pspikes,  pspikes,  pspikes,  0, ROT0,   "Video System Co.", "Super Volley '91 (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1991, karatblz, 0,        karatblz, karatblz, 0, ROT0,   "Video System Co.", "Karate Blazers (World?)", GAME_NO_COCKTAIL )
GAMEX( 1991, karatblu, karatblz, karatblz, karatblz, 0, ROT0,   "Video System Co.", "Karate Blazers (US)", GAME_NO_COCKTAIL )
GAMEX( 1990, spinlbrk, 0,        spinlbrk, spinlbrk, 0, ROT0,   "V-System Co.", "Spinal Breakers (World)", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1990, spinlbru, spinlbrk, spinlbrk, spinlbrk, 0, ROT0,   "V-System Co.", "Spinal Breakers (US)", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1990, spinlbrj, spinlbrk, spinlbrk, spinlbrk, 0, ROT0,   "V-System Co.", "Spinal Breakers (Japan)", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1991, turbofrc, 0,        turbofrc, turbofrc, 0, ROT270, "Video System Co.", "Turbo Force", GAME_NO_COCKTAIL )
GAMEX( 1992, aerofgt,  0,        aerofgt,  aerofgt,  0, ROT270, "Video System Co.", "Aero Fighters", GAME_NO_COCKTAIL )
GAMEX( 1992, aerofgtb, aerofgt,  aerofgtb, aerofgtb, 0, ROT270, "Video System Co.", "Aero Fighters (Turbo Force hardware set 1)", GAME_NO_COCKTAIL )
GAMEX( 1992, aerofgtc, aerofgt,  aerofgtb, aerofgtb, 0, ROT270, "Video System Co.", "Aero Fighters (Turbo Force hardware set 2)", GAME_NO_COCKTAIL )
GAMEX( 1992, sonicwi,  aerofgt,  aerofgtb, aerofgtb, 0, ROT270, "Video System Co.", "Sonic Wings (Japan)", GAME_NO_COCKTAIL )
