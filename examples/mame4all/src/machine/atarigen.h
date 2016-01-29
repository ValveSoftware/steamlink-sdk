/***************************************************************************

  atarigen.h

  General functions for mid-to-late 80's Atari raster games.

***************************************************************************/

#include "driver.h"

#ifndef __MACHINE_ATARIGEN__
#define __MACHINE_ATARIGEN__


#define ATARI_CLOCK_14MHz	14318180
#define ATARI_CLOCK_20MHz	20000000
#define ATARI_CLOCK_32MHz	32000000
#define ATARI_CLOCK_50MHz	50000000



/*--------------------------------------------------------------------------

	Atari generic interrupt model (required)

		atarigen_scanline_int_state - state of the scanline interrupt line
		atarigen_sound_int_state - state of the sound interrupt line
		atarigen_video_int_state - state of the video interrupt line

		atarigen_int_callback - called when the interrupt state changes

		atarigen_interrupt_reset - resets & initializes the interrupt state
		atarigen_update_interrupts - forces the interrupts to be reevaluted

		atarigen_scanline_int_set - scanline interrupt initialization
		atarigen_sound_int_gen - scanline interrupt generator
		atarigen_scanline_int_ack_w - scanline interrupt acknowledgement

		atarigen_sound_int_gen - sound interrupt generator
		atarigen_sound_int_ack_w - sound interrupt acknowledgement

		atarigen_video_int_gen - video interrupt generator
		atarigen_video_int_ack_w - video interrupt acknowledgement

--------------------------------------------------------------------------*/
extern int atarigen_scanline_int_state;
extern int atarigen_sound_int_state;
extern int atarigen_video_int_state;

typedef void (*atarigen_int_callback)(void);

void atarigen_interrupt_reset(atarigen_int_callback update_int);
void atarigen_update_interrupts(void);

void atarigen_scanline_int_set(int scanline);
int atarigen_scanline_int_gen(void);
WRITE_HANDLER( atarigen_scanline_int_ack_w );

int atarigen_sound_int_gen(void);
WRITE_HANDLER( atarigen_sound_int_ack_w );

int atarigen_video_int_gen(void);
WRITE_HANDLER( atarigen_video_int_ack_w );


/*--------------------------------------------------------------------------

	EEPROM I/O (optional)

		atarigen_eeprom_default - pointer to compressed default data
		atarigen_eeprom - pointer to base of EEPROM memory
		atarigen_eeprom_size - size of EEPROM memory

		atarigen_eeprom_reset - resets the EEPROM system

		atarigen_eeprom_enable_w - write handler to enable EEPROM access
		atarigen_eeprom_w - write handler for EEPROM data
		atarigen_eeprom_r - read handler for EEPROM data (low byte)
		atarigen_eeprom_upper_r - read handler for EEPROM data (high byte)

		atarigen_nvram_handler - load/save EEPROM data

--------------------------------------------------------------------------*/
extern const UINT16 *atarigen_eeprom_default;
extern UINT8 *atarigen_eeprom;
extern size_t atarigen_eeprom_size;

void atarigen_eeprom_reset(void);

WRITE_HANDLER( atarigen_eeprom_enable_w );
WRITE_HANDLER( atarigen_eeprom_w );
READ_HANDLER( atarigen_eeprom_r );
READ_HANDLER( atarigen_eeprom_upper_r );

void atarigen_nvram_handler(void *file,int read_or_write);
void atarigen_hisave(void);


/*--------------------------------------------------------------------------

	Slapstic I/O (optional)

		atarigen_slapstic_init - select and initialize the slapstic handlers
		atarigen_slapstic_reset - resets the slapstic state

		atarigen_slapstic_w - write handler for slapstic data
		atarigen_slapstic_r - read handler for slapstic data

		slapstic_init - low-level init routine
		slapstic_reset - low-level reset routine
		slapstic_bank - low-level routine to return the current bank
		slapstic_tweak - low-level tweak routine

--------------------------------------------------------------------------*/
void atarigen_slapstic_init(int cpunum, int base, int chipnum);
void atarigen_slapstic_reset(void);

WRITE_HANDLER( atarigen_slapstic_w );
READ_HANDLER( atarigen_slapstic_r );

void slapstic_init(int chip);
void slapstic_reset(void);
int slapstic_bank(void);
int slapstic_tweak(offs_t offset);



/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/



/*--------------------------------------------------------------------------

	Sound I/O

		atarigen_sound_io_reset - reset the sound I/O system

		atarigen_6502_irq_gen - standard 6502 IRQ interrupt generator
		atarigen_6502_irq_ack_r - standard 6502 IRQ interrupt acknowledgement
		atarigen_6502_irq_ack_w - standard 6502 IRQ interrupt acknowledgement

		atarigen_ym2151_irq_gen - YM2151 sound IRQ generator

		atarigen_sound_w - Main CPU -> sound CPU data write (low byte)
		atarigen_sound_r - Sound CPU -> main CPU data read (low byte)
		atarigen_sound_upper_w - Main CPU -> sound CPU data write (high byte)
		atarigen_sound_upper_r - Sound CPU -> main CPU data read (high byte)

		atarigen_sound_reset_w - 6502 CPU reset
		atarigen_6502_sound_w - Sound CPU -> main CPU data write
		atarigen_6502_sound_r - Main CPU -> sound CPU data read

--------------------------------------------------------------------------*/
extern int atarigen_cpu_to_sound_ready;
extern int atarigen_sound_to_cpu_ready;

void atarigen_sound_io_reset(int cpu_num);

int atarigen_6502_irq_gen(void);
READ_HANDLER( atarigen_6502_irq_ack_r );
WRITE_HANDLER( atarigen_6502_irq_ack_w );

void atarigen_ym2151_irq_gen(int irq);

WRITE_HANDLER( atarigen_sound_w );
READ_HANDLER( atarigen_sound_r );
WRITE_HANDLER( atarigen_sound_upper_w );
READ_HANDLER( atarigen_sound_upper_r );

void atarigen_sound_reset(void);
WRITE_HANDLER( atarigen_sound_reset_w );
WRITE_HANDLER( atarigen_6502_sound_w );
READ_HANDLER( atarigen_6502_sound_r );



/*--------------------------------------------------------------------------

	Misc sound helpers

		atarigen_init_6502_speedup - installs 6502 speedup cheat handler
		atarigen_set_ym2151_vol - set the volume of the 2151 chip
		atarigen_set_ym2413_vol - set the volume of the 2413 chip
		atarigen_set_pokey_vol - set the volume of the POKEY chip(s)
		atarigen_set_tms5220_vol - set the volume of the 5220 chip
		atarigen_set_oki6295_vol - set the volume of the OKI6295

--------------------------------------------------------------------------*/
void atarigen_init_6502_speedup(int cpunum, int compare_pc1, int compare_pc2);
void atarigen_set_ym2151_vol(int volume);
void atarigen_set_ym2413_vol(int volume);
void atarigen_set_pokey_vol(int volume);
void atarigen_set_tms5220_vol(int volume);
void atarigen_set_oki6295_vol(int volume);



/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/


/* general video globals */
extern UINT8 *atarigen_playfieldram;
extern UINT8 *atarigen_playfield2ram;
extern UINT8 *atarigen_playfieldram_color;
extern UINT8 *atarigen_playfield2ram_color;
extern UINT8 *atarigen_spriteram;
extern UINT8 *atarigen_alpharam;
extern UINT8 *atarigen_vscroll;
extern UINT8 *atarigen_hscroll;

extern size_t atarigen_playfieldram_size;
extern size_t atarigen_playfield2ram_size;
extern size_t atarigen_spriteram_size;
extern size_t atarigen_alpharam_size;


/*--------------------------------------------------------------------------

	Video scanline timing

		atarigen_scanline_callback - called every n scanlines

		atarigen_scanline_timer_reset - call to reset the system

--------------------------------------------------------------------------*/
typedef void (*atarigen_scanline_callback)(int scanline);

void atarigen_scanline_timer_reset(atarigen_scanline_callback update_graphics, int frequency);



/*--------------------------------------------------------------------------

	Video Controller I/O: used in Shuuz, Thunderjaws, Relief Pitcher, Off the Wall

		atarigen_video_control_data - pointer to base of control memory
		atarigen_video_control_state - current state of the video controller

		atarigen_video_control_reset - initializes the video controller

		atarigen_video_control_w - write handler for the video controller
		atarigen_video_control_r - read handler for the video controller

--------------------------------------------------------------------------*/
struct atarigen_video_control_state_desc
{
	int latch1;								/* latch #1 value (-1 means disabled) */
	int latch2;								/* latch #2 value (-1 means disabled) */
	int rowscroll_enable;					/* true if row-scrolling is enabled */
	int palette_bank;						/* which palette bank is enabled */
	int pf1_xscroll;						/* playfield 1 xscroll */
	int pf1_yscroll;						/* playfield 1 yscroll */
	int pf2_xscroll;						/* playfield 2 xscroll */
	int pf2_yscroll;						/* playfield 2 yscroll */
	int sprite_xscroll;						/* sprite xscroll */
	int sprite_yscroll;						/* sprite xscroll */
};

extern UINT8 *atarigen_video_control_data;
extern struct atarigen_video_control_state_desc atarigen_video_control_state;

void atarigen_video_control_reset(void);
void atarigen_video_control_update(const UINT8 *data);

WRITE_HANDLER( atarigen_video_control_w );
READ_HANDLER( atarigen_video_control_r );



/*--------------------------------------------------------------------------

	Motion object rendering

		atarigen_mo_desc - description of the M.O. layout

		atarigen_mo_callback - called back for each M.O. during processing

		atarigen_mo_init - initializes and configures the M.O. list walker
		atarigen_mo_free - frees all memory allocated by atarigen_mo_init
		atarigen_mo_reset - reset for a new frame (use only if not using interrupt system)
		atarigen_mo_update - updates the M.O. list for the given scanline
		atarigen_mo_process - processes the current list

--------------------------------------------------------------------------*/
#define ATARIGEN_MAX_MAXCOUNT				1024	/* no more than 1024 MO's ever */

struct atarigen_mo_desc
{
	int maxcount;                           /* maximum number of MO's */
	int entryskip;                          /* number of bytes per MO entry */
	int wordskip;                           /* number of bytes between MO words */
	int ignoreword;                         /* ignore an entry if this word == 0xffff */
	int linkword, linkshift, linkmask;		/* link = (data[linkword >> linkshift) & linkmask */
	int reverse;                            /* render in reverse link order */
	int entrywords;							/* number of words/entry (0 defaults to 4) */
};

typedef void (*atarigen_mo_callback)(const UINT16 *data, const struct rectangle *clip, void *param);

int atarigen_mo_init(const struct atarigen_mo_desc *source_desc);
void atarigen_mo_free(void);
void atarigen_mo_reset(void);
void atarigen_mo_update(const UINT8 *base, int start, int scanline);
void atarigen_mo_update_slip_512(const UINT8 *base, int scroll, int scanline, const UINT8 *slips);
void atarigen_mo_process(atarigen_mo_callback callback, void *param);



/*--------------------------------------------------------------------------

	RLE Motion object rendering/decoding

		atarigen_rle_descriptor - describes a single object

		atarigen_rle_count - total number of objects found
		atarigen_rle_info - array of descriptors for objects we found

		atarigen_rle_init - prescans the RLE objects
		atarigen_rle_free - frees all memory allocated by atarigen_rle_init
		atarigen_rle_render - render an RLE-compressed motion object

--------------------------------------------------------------------------*/
struct atarigen_rle_descriptor
{
	int width;
	int height;
	INT16 xoffs;
	INT16 yoffs;
	int bpp;
	UINT32 pen_usage;
	UINT32 pen_usage_hi;
	const UINT16 *table;
	const UINT16 *data;
};

extern int atarigen_rle_count;
extern struct atarigen_rle_descriptor *atarigen_rle_info;

int atarigen_rle_init(int region, int colorbase);
void atarigen_rle_free(void);
void atarigen_rle_render(struct osd_bitmap *bitmap, struct atarigen_rle_descriptor *info, int color, int hflip, int vflip,
	int x, int y, int xscale, int yscale, const struct rectangle *clip);



/*--------------------------------------------------------------------------

	Playfield rendering

		atarigen_pf_state - data block describing the playfield

		atarigen_pf_callback - called back for each chunk during processing

		atarigen_pf_init - initializes and configures the playfield params
		atarigen_pf_free - frees all memory allocated by atarigen_pf_init
		atarigen_pf_reset - reset for a new frame (use only if not using interrupt system)
		atarigen_pf_update - updates the playfield params for the given scanline
		atarigen_pf_process - processes the current list of parameters

		atarigen_pf2_init - same as above but for a second playfield
		atarigen_pf2_free - same as above but for a second playfield
		atarigen_pf2_reset - same as above but for a second playfield
		atarigen_pf2_update - same as above but for a second playfield
		atarigen_pf2_process - same as above but for a second playfield

--------------------------------------------------------------------------*/
extern struct osd_bitmap *atarigen_pf_bitmap;
extern UINT8 *atarigen_pf_dirty;
extern UINT8 *atarigen_pf_visit;

extern struct osd_bitmap *atarigen_pf2_bitmap;
extern UINT8 *atarigen_pf2_dirty;
extern UINT8 *atarigen_pf2_visit;

extern struct osd_bitmap *atarigen_pf_overrender_bitmap;
extern UINT16 atarigen_overrender_colortable[32];

struct atarigen_pf_desc
{
	int tilewidth, tileheight;              /* width/height of each tile */
	int xtiles, ytiles;						/* number of tiles in each direction */
	int noscroll;							/* non-scrolling? */
};

struct atarigen_pf_state
{
	int hscroll;							/* current horizontal starting offset */
	int vscroll;							/* current vertical starting offset */
	int param[2];							/* up to 2 other parameters that will cause a boundary break */
};

typedef void (*atarigen_pf_callback)(const struct rectangle *tiles, const struct rectangle *clip, const struct atarigen_pf_state *state, void *param);

int atarigen_pf_init(const struct atarigen_pf_desc *source_desc);
void atarigen_pf_free(void);
void atarigen_pf_reset(void);
void atarigen_pf_update(const struct atarigen_pf_state *state, int scanline);
void atarigen_pf_process(atarigen_pf_callback callback, void *param, const struct rectangle *clip);

int atarigen_pf2_init(const struct atarigen_pf_desc *source_desc);
void atarigen_pf2_free(void);
void atarigen_pf2_reset(void);
void atarigen_pf2_update(const struct atarigen_pf_state *state, int scanline);
void atarigen_pf2_process(atarigen_pf_callback callback, void *param, const struct rectangle *clip);



/*--------------------------------------------------------------------------

	Misc Video stuff

		atarigen_get_hblank - returns the current HBLANK state
		atarigen_halt_until_hblank_0_w - write handler for a HBLANK halt
		atarigen_666_paletteram_w - 6-6-6 special RGB paletteram handler
		atarigen_expanded_666_paletteram_w - byte version of above

--------------------------------------------------------------------------*/
int atarigen_get_hblank(void);
WRITE_HANDLER( atarigen_halt_until_hblank_0_w );
WRITE_HANDLER( atarigen_666_paletteram_w );
WRITE_HANDLER( atarigen_expanded_666_paletteram_w );



/*--------------------------------------------------------------------------

	General stuff

		atarigen_show_slapstic_message - display warning about slapstic
		atarigen_show_sound_message - display warning about coins
		atarigen_update_messages - update messages

--------------------------------------------------------------------------*/
void atarigen_show_slapstic_message(void);
void atarigen_show_sound_message(void);
void atarigen_update_messages(void);



/*--------------------------------------------------------------------------

	Motion object drawing macros

		atarigen_mo_compute_clip - computes the M.O. clip rect
		atarigen_mo_compute_clip_8x8 - computes the M.O. clip rect
		atarigen_mo_compute_clip_16x16 - computes the M.O. clip rect

		atarigen_mo_draw - draws a generically-sized M.O.
		atarigen_mo_draw_strip - draws a generically-sized M.O. strip
		atarigen_mo_draw_8x8 - draws an 8x8 M.O.
		atarigen_mo_draw_8x8_strip - draws an 8x8 M.O. strip (hsize == 1)
		atarigen_mo_draw_16x16 - draws a 16x16 M.O.
		atarigen_mo_draw_16x16_strip - draws a 16x16 M.O. strip (hsize == 1)

--------------------------------------------------------------------------*/
#define atarigen_mo_compute_clip(dest, xpos, ypos, hsize, vsize, clip, tile_width, tile_height) \
{																								\
	/* determine the bounding box */															\
	dest.min_x = xpos;																			\
	dest.min_y = ypos;																			\
	dest.max_x = xpos + hsize * tile_width - 1;													\
	dest.max_y = ypos + vsize * tile_height - 1;												\
																								\
	/* clip to the display */																	\
	if (dest.min_x < clip->min_x)																\
		dest.min_x = clip->min_x;																\
	else if (dest.min_x > clip->max_x)															\
		dest.min_x = clip->max_x;																\
	if (dest.max_x < clip->min_x)																\
		dest.max_x = clip->min_x;																\
	else if (dest.max_x > clip->max_x)															\
		dest.max_x = clip->max_x;																\
	if (dest.min_y < clip->min_y)																\
		dest.min_y = clip->min_y;																\
	else if (dest.min_y > clip->max_y)															\
		dest.min_y = clip->max_y;																\
	if (dest.max_y < clip->min_y)																\
		dest.max_y = clip->min_y;																\
	else if (dest.max_y > clip->max_y)															\
		dest.max_y = clip->max_y;																\
}

#define atarigen_mo_compute_clip_8x8(dest, xpos, ypos, hsize, vsize, clip) \
	atarigen_mo_compute_clip(dest, xpos, ypos, hsize, vsize, clip, 8, 8)

#define atarigen_mo_compute_clip_16x8(dest, xpos, ypos, hsize, vsize, clip) \
	atarigen_mo_compute_clip(dest, xpos, ypos, hsize, vsize, clip, 16, 8)

#define atarigen_mo_compute_clip_16x16(dest, xpos, ypos, hsize, vsize, clip) \
	atarigen_mo_compute_clip(dest, xpos, ypos, hsize, vsize, clip, 16, 16)


#define atarigen_mo_draw(bitmap, gfx, code, color, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, tile_width, tile_height) \
{																										\
	int tilex, tiley, screenx, screendx, screendy;														\
	int startx = x;																						\
	int screeny = y;																					\
	int tile = code;																					\
																										\
	/* adjust for h flip */																				\
	if (hflip)																							\
		startx += (hsize - 1) * tile_width, screendx = -tile_width;										\
	else																								\
		screendx = tile_width;																			\
																										\
	/* adjust for v flip */																				\
	if (vflip)																							\
		screeny += (vsize - 1) * tile_height, screendy = -tile_height;									\
	else																								\
		screendy = tile_height;																			\
																										\
	/* loop over the height */																			\
	for (tiley = 0; tiley < vsize; tiley++, screeny += screendy)										\
	{																									\
		/* clip the Y coordinate */																		\
		if (screeny <= clip->min_y - tile_height)														\
		{																								\
			tile += hsize;																				\
			continue;																					\
		}																								\
		else if (screeny > clip->max_y)																	\
			break;																						\
																										\
		/* loop over the width */																		\
		for (tilex = 0, screenx = startx; tilex < hsize; tilex++, screenx += screendx, tile++)			\
		{																								\
			/* clip the X coordinate */																	\
			if (screenx <= clip->min_x - tile_width || screenx > clip->max_x)							\
				continue;																				\
																										\
			/* draw the sprite */																		\
			drawgfx(bitmap, gfx, tile, color, hflip, vflip, screenx, screeny, clip, trans, trans_pen);	\
		}																								\
	}																									\
}

#define atarigen_mo_draw_transparent(bitmap, gfx, code, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, tile_width, tile_height) \
{																										\
	UINT16 *temp = gfx->colortable;																\
	gfx->colortable = atarigen_overrender_colortable;													\
	atarigen_mo_draw(bitmap, gfx, code, 0, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, tile_width, tile_height);\
	gfx->colortable = temp;																				\
}

#define atarigen_mo_draw_strip(bitmap, gfx, code, color, hflip, vflip, x, y, vsize, clip, trans, trans_pen, tile_width, tile_height) \
{																										\
	int tiley, screendy;																				\
	int screenx = x;																					\
	int screeny = y;																					\
	int tile = code;																					\
																										\
	/* clip the X coordinate */																			\
	if (screenx > clip->min_x - tile_width && screenx <= clip->max_x)									\
	{																									\
		/* adjust for v flip */																			\
		if (vflip)																						\
			screeny += (vsize - 1) * tile_height, screendy = -tile_height;								\
		else																							\
			screendy = tile_height;																		\
																										\
		/* loop over the height */																		\
		for (tiley = 0; tiley < vsize; tiley++, screeny += screendy, tile++)							\
		{																								\
			/* clip the Y coordinate */																	\
			if (screeny <= clip->min_y - tile_height)													\
				continue;																				\
			else if (screeny > clip->max_y)																\
				break;																					\
																										\
			/* draw the sprite */																		\
			drawgfx(bitmap, gfx, tile, color, hflip, vflip, screenx, screeny, clip, trans, trans_pen);	\
		}																								\
	}																									\
}

#define atarigen_mo_draw_transparent_strip(bitmap, gfx, code, hflip, vflip, x, y, vsize, clip, trans, trans_pen, tile_width, tile_height) \
{																										\
	UINT16 *temp = gfx->colortable;																\
	gfx->colortable = atarigen_overrender_colortable;													\
	atarigen_mo_draw_strip(bitmap, gfx, code, 0, hflip, vflip, x, y, vsize, clip, trans, trans_pen, tile_width, tile_height);\
	gfx->colortable = temp;																				\
}


#define atarigen_mo_draw_8x8(bitmap, gfx, code, color, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw(bitmap, gfx, code, color, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, 8, 8)

#define atarigen_mo_draw_16x8(bitmap, gfx, code, color, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw(bitmap, gfx, code, color, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, 16, 8)

#define atarigen_mo_draw_16x16(bitmap, gfx, code, color, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw(bitmap, gfx, code, color, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, 16, 16)


#define atarigen_mo_draw_transparent_8x8(bitmap, gfx, code, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_transparent(bitmap, gfx, code, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, 8, 8)

#define atarigen_mo_draw_transparent_16x8(bitmap, gfx, code, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_transparent(bitmap, gfx, code, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, 16, 8)

#define atarigen_mo_draw_transparent_16x16(bitmap, gfx, code, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_transparent(bitmap, gfx, code, hflip, vflip, x, y, hsize, vsize, clip, trans, trans_pen, 16, 16)


#define atarigen_mo_draw_8x8_strip(bitmap, gfx, code, color, hflip, vflip, x, y, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_strip(bitmap, gfx, code, color, hflip, vflip, x, y, vsize, clip, trans, trans_pen, 8, 8)

#define atarigen_mo_draw_16x8_strip(bitmap, gfx, code, color, hflip, vflip, x, y, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_strip(bitmap, gfx, code, color, hflip, vflip, x, y, vsize, clip, trans, trans_pen, 16, 8)

#define atarigen_mo_draw_16x16_strip(bitmap, gfx, code, color, hflip, vflip, x, y, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_strip(bitmap, gfx, code, color, hflip, vflip, x, y, vsize, clip, trans, trans_pen, 16, 16)


#define atarigen_mo_draw_transparent_8x8_strip(bitmap, gfx, code, hflip, vflip, x, y, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_transparent_strip(bitmap, gfx, code, hflip, vflip, x, y, vsize, clip, trans, trans_pen, 8, 8)

#define atarigen_mo_draw_transparent_16x8_strip(bitmap, gfx, code, hflip, vflip, x, y, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_transparent_strip(bitmap, gfx, code, hflip, vflip, x, y, vsize, clip, trans, trans_pen, 16, 8)

#define atarigen_mo_draw_transparent_16x16_strip(bitmap, gfx, code, hflip, vflip, x, y, vsize, clip, trans, trans_pen) \
	atarigen_mo_draw_transparent_strip(bitmap, gfx, code, hflip, vflip, x, y, vsize, clip, trans, trans_pen, 16, 16)

#endif
