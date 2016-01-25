#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/taitoic.h"

#define TC0100SCN_GFX_NUM 1
#define TC0480SCP_GFX_NUM 1
#define TC0280GRD_GFX_NUM 2
#define TC0430GRW_GFX_NUM 2


/*
TC0360PRI
---------
Priority manager.
A higher priority value means higher priority. 0 could mean disable but
I'm not sure. If two inputs have the same priority value, I think the first
one has priority, but I'm not sure of that either.
It seems the chip accepts three inputs from three different sources, and
each one of them can declare to have four different priority levels.

000 unknown. Could it be related to highlight/shadow effects in qcrayon2
    and gunfront?
001 in games with a roz layer, this is the roz palette bank (bottom 6 bits
    affect roz color, top 2 bits affect priority)
002 unknown
003 unknown

004 ----xxxx \       priority level 0 (unused? usually 0, pulirula sets it to F in some places)
    xxxx---- | Input priority level 1 (usually FG0)
005 ----xxxx |   #1  priority level 2 (usually BG0)
    xxxx---- /       priority level 3 (usually BG1)

006 ----xxxx \       priority level 0 (usually sprites with top color bits 00)
    xxxx---- | Input priority level 1 (usually sprites with top color bits 01)
007 ----xxxx |   #2  priority level 2 (usually sprites with top color bits 10)
    xxxx---- /       priority level 3 (usually sprites with top color bits 11)

008 ----xxxx \       priority level 0 (e.g. roz layer if top bits of register 001 are 00)
    xxxx---- | Input priority level 1 (e.g. roz layer if top bits of register 001 are 01)
009 ----xxxx |   #3  priority level 2 (e.g. roz layer if top bits of register 001 are 10)
    xxxx---- /       priority level 3 (e.g. roz layer if top bits of register 001 are 11)

00a unused
00b unused
00c unused
00d unused
00e unused
00f unused
*/

static UINT8 TC0360PRI_regs[16];

WRITE_HANDLER( TC0360PRI_w )
{
	TC0360PRI_regs[offset] = data;

if (offset >= 0x0a)
	usrintf_showmessage("write %02x to unused TC0360PRI reg %x",data,offset);
#if 0
#define regs TC0360PRI_regs
	usrintf_showmessage("%02x %02x  %02x %02x  %02x %02x %02x %02x %02x %02x",
		regs[0x00],regs[0x01],regs[0x02],regs[0x03],
		regs[0x04],regs[0x05],regs[0x06],regs[0x07],
		regs[0x08],regs[0x09]);
#endif
}

extern int TC0480SCP_pri_reg;

unsigned char *f2_sprite_extension;
size_t f2_spriteext_size;
int sprites_disabled,sprites_active_area,sprites_master_scrollx,sprites_master_scrolly;
static UINT16 *spriteram_buffered,*spriteram_delayed;


struct tempsprite
{
	int code,color;
	int flipx,flipy;
	int x,y;
	int zoomx,zoomy;
	int primask;
};
struct tempsprite *spritelist;

/****************************************************
Needed by Metal Black
****************************************************/
int f2_tilemap_col_base = 0;

/****************************************************

!!!! NO LONGER USED IN THE F2 DRIVER !!!!

(I think f2_video_version can be removed altogether.)

Determines what capabilities handled in video driver:
 0 = sprites,
 1 = sprites + layers,
 2 = sprites + layers + rotation
 3 = double lot of standard layers (thundfox)
 4 = 4 bg layers (e.g. deadconx)
****************************************************/
//int f2_video_version = 1;

/****************************************************
Determines sprite banking method:
 0 = standard
 1 = use sprite extension area lo bytes for hi 6 bits
 2 = use sprite extension area hi bytes
 3 = use sprite extension area lo bytes as hi bytes
*****************************************************/
int f2_spriteext = 0;

static int f2_pivot_xdisp = 0;
static int f2_pivot_ydisp = 0;
static int spritebank[8];
static int koshien_spritebank;
/******************************************************************************
f2_hide_pixels
==============
Allows us to iron out pixels of rubbish on edge of scr layers.
Value has to be set per game, 0 to +3. Cannot be calculated.
******************************************************************************/
static int f2_hide_pixels;
static int f2_xkludge = 0;   /* Needed by Deadconx, Metalb, Footchmp */
static int f2_ykludge = 0;



static int has_two_TC0100SCN(void)
{
	const struct MemoryWriteAddress *mwa;

	/* scan the memory handlers and see if the second TC0100SCN is used */

	mwa = Machine->drv->cpu[0].memory_write;
	if (mwa)
	{
		while (mwa->start != -1)
		{
			if (mwa->handler == TC0100SCN_word_1_w)
				return 1;

			mwa++;
		}
	}

	return 0;
}

static int has_TC0480SCP(void)
{
	const struct MemoryWriteAddress *mwa;

	/* scan the memory handlers and see if the TC0480SCP is used */

	mwa = Machine->drv->cpu[0].memory_write;
	if (mwa)
	{
		while (mwa->start != -1)
		{
			if (mwa->handler == TC0480SCP_word_w)
				return 1;

			mwa++;
		}
	}

	return 0;
}

static int has_TC0110PCR(void)
{
	const struct MemoryWriteAddress *mwa;

	/* scan the memory handlers and see if the TC0110PCR is used */

	mwa = Machine->drv->cpu[0].memory_write;
	if (mwa)
	{
		while (mwa->start != -1)
		{
			if (mwa->handler == TC0110PCR_word_w)
				return 1;

			mwa++;
		}
	}

	return 0;
}

static int has_TC0280GRD(void)
{
	const struct MemoryWriteAddress *mwa;

	/* scan the memory handlers and see if the TC0280GRD is used */

	mwa = Machine->drv->cpu[0].memory_write;
	if (mwa)
	{
		while (mwa->start != -1)
		{
			if (mwa->handler == TC0280GRD_word_w)
				return 1;

			mwa++;
		}
	}

	return 0;
}

static int has_TC0430GRW(void)
{
	const struct MemoryWriteAddress *mwa;

	/* scan the memory handlers and see if the TC0430GRW is used */

	mwa = Machine->drv->cpu[0].memory_write;
	if (mwa)
	{
		while (mwa->start != -1)
		{
			if (mwa->handler == TC0430GRW_word_w)
				return 1;

			mwa++;
		}
	}

	return 0;
}



int taitof2_core_vh_start (void)
{
	spriteram_delayed = (UINT16*)malloc(spriteram_size);
	spriteram_buffered = (UINT16*)malloc(spriteram_size);
	spritelist = (struct tempsprite *)malloc(0x400 * sizeof(*spritelist));
	if (!spriteram_delayed || !spriteram_buffered || !spritelist)
		return 1;

	/* we only call tc0100scn_vh_start if NOT Deadconx, Footchmp, MetalB */

	if (has_TC0480SCP())	/* it's a tc0480scp game */
	{
		if (TC0480SCP_vh_start(TC0480SCP_GFX_NUM,f2_hide_pixels,f2_xkludge,f2_ykludge,f2_tilemap_col_base))
			return 1;
	}
	else	/* it's a tc0100scn game */
	{
		if (TC0100SCN_vh_start(has_two_TC0100SCN() ? 2 : 1,TC0100SCN_GFX_NUM,f2_hide_pixels))
			return 1;
	}

	if (has_TC0110PCR())
		if (TC0110PCR_vh_start())
			return 1;

	if (has_TC0280GRD())
		if (TC0280GRD_vh_start(TC0280GRD_GFX_NUM))
			return 1;

	if (has_TC0430GRW())
		if (TC0430GRW_vh_start(TC0430GRW_GFX_NUM))
			return 1;

	{
		int i;

		for (i = 0; i < 8; i ++)
			spritebank[i] = 0x400 * i;
	}

	sprites_disabled = 1;
	sprites_active_area = 0;

	return 0;
}


//DG: some of these can be merged...?

int taitof2_default_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 0;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_finalb_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 1;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_3p_vh_start (void)   /* Megab, Liquidk */
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_3p_buf_vh_start (void)   /* Solfigtr, Koshien */
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_driftout_vh_start (void)
{
//	f2_video_version = 2;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	f2_pivot_xdisp = -16;
	f2_pivot_ydisp = 16;
	return (taitof2_core_vh_start());
}

int taitof2_c_vh_start (void)   /* Quiz Crayons, Quiz Jinsei */
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 3;
	return (taitof2_core_vh_start());
}

int taitof2_ssi_vh_start (void)
{
//	f2_video_version = 0;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_growl_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_ninjak_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 0;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_gunfront_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_thundfox_vh_start (void)
{
//	f2_video_version = 3;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_mjnquest_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 0;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_footchmp_vh_start (void)
{
//	f2_video_version = 4;
	f2_hide_pixels = 3;
	f2_xkludge = 0x1d;
	f2_ykludge = 0x08;
	f2_tilemap_col_base = 0;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_hthero_vh_start (void)
{
//	f2_video_version = 4;
	f2_hide_pixels = 3;
	f2_xkludge = 0x33;   // needs different kludges from Footchmp
	f2_ykludge = - 0x04;
	f2_tilemap_col_base = 0;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_deadconx_vh_start (void)
{
//	f2_video_version = 4;
	f2_hide_pixels = 3;
	f2_xkludge = 0x1e;
	f2_ykludge = 0x08;
	f2_tilemap_col_base = 0;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_deadconj_vh_start (void)
{
//	f2_video_version = 4;
	f2_hide_pixels = 3;
	f2_xkludge = 0x34;
	f2_ykludge = - 0x05;
	f2_tilemap_col_base = 0;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_metalb_vh_start (void)
{
//	f2_video_version = 4;
	f2_hide_pixels = 3;
	f2_xkludge = 0x34;
	f2_ykludge = - 0x04;
	f2_tilemap_col_base = 256;   // uses separate palette area for tilemaps
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_yuyugogo_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 1;
	return (taitof2_core_vh_start());
}

int taitof2_yesnoj_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	return (taitof2_core_vh_start());
}

int taitof2_dinorex_vh_start (void)
{
//	f2_video_version = 1;
	f2_hide_pixels = 3;
	f2_spriteext = 3;
	return (taitof2_core_vh_start());
}

int taitof2_dondokod_vh_start (void)	/* dondokod, cameltry */
{
//	f2_video_version = 2;
	f2_hide_pixels = 3;
	f2_spriteext = 0;
	f2_pivot_xdisp = -16;
	f2_pivot_ydisp = 0;
	return (taitof2_core_vh_start());
}

int taitof2_pulirula_vh_start (void)
{
//	f2_video_version = 2;
	f2_hide_pixels = 3;
	f2_spriteext = 2;
	f2_pivot_xdisp = -10;	/* alignment seems correct (see level 2, falling */
	f2_pivot_ydisp = 16;	/* block of ice after armour man) */
	return (taitof2_core_vh_start());
}

void taitof2_vh_stop (void)
{
	free(spriteram_delayed);
	spriteram_delayed = 0;
	free(spriteram_buffered);
	spriteram_buffered = 0;
	free(spritelist);
	spritelist = 0;

	if (has_TC0480SCP())   /* Deadconx, Footchmp, Metalb */
	{
		TC0480SCP_vh_stop();
	}
	else	/* it's a tc0100scn game */
	{
		TC0100SCN_vh_stop();
	}

	if (has_TC0110PCR())
		TC0110PCR_vh_stop();

	if (has_TC0280GRD())
		TC0280GRD_vh_stop();

	if (has_TC0430GRW())
		TC0430GRW_vh_stop();

}


/********************************************************
          SPRITE READ AND WRITE HANDLERS
********************************************************/

READ_HANDLER( taitof2_spriteram_r )
{
	return READ_WORD(&spriteram[offset]);
}

WRITE_HANDLER( taitof2_spriteram_w )
{
	COMBINE_WORD_MEM(&spriteram[offset],data);
}

WRITE_HANDLER( taitof2_sprite_extension_w )
{
	if (offset < 0x1000)   /* areas above this cleared in some games, but not used */
	{
		COMBINE_WORD_MEM(&f2_sprite_extension[offset],data);
	}
}

WRITE_HANDLER( taitof2_spritebank_w )
{
	int i;
	int j;

	i=0;
	j = (offset >> 1);
	if (j < 2) return;   /* these are always irrelevant zero writes */

	if ((offset >> 1) < 4)   /* special bank pairs */
	{
		j = (offset & 2);   /* either set pair 0&1 or 2&3 */
		i = data << 11;

		spritebank[j] = i;
		spritebank[j+1] = (i + 0x400);

	}
	else   /* last 4 are individual banks */
	{
		i = data << 10;
		spritebank[j] = i;
	}

}

READ_HANDLER( koshien_spritebank_r )
{
	return koshien_spritebank;
}

WRITE_HANDLER( koshien_spritebank_w )
{
	koshien_spritebank = data;

	spritebank[0]=0x0000;   /* never changes */
	spritebank[1]=0x0400;

	spritebank[2] =  ((data & 0x00f) + 1) * 0x800;
	spritebank[4] = (((data & 0x0f0) >> 4) + 1) * 0x800;
	spritebank[6] = (((data & 0xf00) >> 8) + 1) * 0x800;
	spritebank[3] = spritebank[2] + 0x400;
	spritebank[5] = spritebank[4] + 0x400;
	spritebank[7] = spritebank[6] + 0x400;
}



/*******************************************
			PALETTE
*******************************************/

void taitof2_update_palette(void)
{
	int i, area;
	int off,extoffs,code,color;
	int spritecont,big_sprite=0,last_continuation_tile=0;
	unsigned short palette_map[256];

	memset (palette_map, 0, sizeof (palette_map));

// DG: we aren't applying sprite marker tests here, but doesn't seem
// to cause palette overflows, so I don't think we should worry.

	color = 0;
	area = sprites_active_area;

	/* Sprites */
	for (off = 0;off < 0x4000;off += 16)
	{
		/* sprites_active_area may change during processing */
		int offs = off + area;

		if (spriteram_buffered[(offs+6)/2] & 0x8000)
		{
			area = 0x8000 * (spriteram_buffered[(offs+10)/2] & 0x0001);
			continue;
		}

		spritecont = (spriteram_buffered[(offs+8)/2] & 0xff00) >> 8;

		if (spritecont & 0x8)
		{
			big_sprite = 1;
		}
		else if (big_sprite)
		{
			last_continuation_tile = 1;
		}

		code = 0;
		extoffs = offs;
		if (extoffs >= 0x8000) extoffs -= 0x4000;   /* spriteram[0x4000-7fff] has no corresponding extension area */

		if (f2_spriteext == 0)
		{
			code = spriteram_buffered[offs/2] & 0x1fff;
			{
				int bank;

				bank = (code & 0x1c00) >> 10;
				code = spritebank[bank] + (code & 0x3ff);
			}
		}

		if (f2_spriteext == 1)   /* Yuyugogo */
		{
			code = spriteram_buffered[offs/2] & 0x3ff;
			i = (READ_WORD(&f2_sprite_extension[(extoffs >> 3)]) & 0x3f ) << 10;
			code = (i | code);
		}

		if (f2_spriteext == 2)   /* Pulirula */
		{
			code = spriteram_buffered[offs/2] & 0xff;
			i = (READ_WORD(&f2_sprite_extension[(extoffs >> 3)]) & 0xff00 );
			code = (i | code);
		}

		if (f2_spriteext == 3)   /* Dinorex and a few quizzes */
		{
			code = spriteram_buffered[offs/2] & 0xff;
			i = (READ_WORD(&f2_sprite_extension[(extoffs >> 3)]) & 0xff ) << 8;
			code = (i | code);
		}

		if ((spritecont & 0x04) == 0)
			color = spriteram_buffered[(offs+8)/2] & 0x00ff;

		if (last_continuation_tile)
		{
			big_sprite=0;
			last_continuation_tile=0;
		}

		if (!code) continue;   /* tilenum is 0, so ignore it */

		if (Machine->gfx[0]->color_granularity == 64)	/* Final Blow is 6-bit deep */
		{
			color &= ~3;
			palette_map[color+0] |= 0xffff;
			palette_map[color+1] |= 0xffff;
			palette_map[color+2] |= 0xffff;
			palette_map[color+3] |= 0xffff;
		}
		else
			palette_map[color] |= Machine->gfx[0]->pen_usage[code];
	}


	/* Tell MAME about the color usage */
	for (i = 0;i < 256;i++)
	{
		int usage = palette_map[i];
		int j;

		if (usage)
		{
			for (j = 0; j < 16; j++)
				if (palette_map[i] & (1 << j))
					palette_used_colors[i * 16 + j] = PALETTE_COLOR_USED;
		}
	}
}

static void draw_sprites(struct osd_bitmap *bitmap,int *primasks)
{
	/*
		Sprite format:
		0000: ---xxxxxxxxxxxxx tile code (0x0000 - 0x1fff)
		0002: xxxxxxxx-------- sprite y-zoom level
		      --------xxxxxxxx sprite x-zoom level

			  0x00 - non scaled = 100%
			  0x80 - scaled to 50%
			  0xc0 - scaled to 25%
			  0xe0 - scaled to 12.5%
			  0xff - scaled to zero pixels size (off)

		[this zoom scale may not be 100% correct, see Gunfront flame screen]

		0004: ----xxxxxxxxxxxx x-coordinate (-0x800 to 0x07ff)
		      ---x------------ latch extra scroll
		      --x------------- latch master scroll
		      -x-------------- don't use extra scroll compensation
		      x--------------- absolute screen coordinates (ignore all sprite scrolls)
		      xxxx------------ the typical use of the above is therefore
			                   1010 = set master scroll
		                       0101 = set extra scroll
		0006: ----xxxxxxxxxxxx y-coordinate (-0x800 to 0x07ff)
		      x--------------- marks special control commands (used in conjunction with 00a)
		                       If the special command flag is set:
		      ---------------x related to sprite ram bank
		      ---x------------ unknown (deadconx, maybe others)
		      --x------------- unknown, some games (growl, gunfront) set it to 1 when
		                       screen is flipped
		0008: --------xxxxxxxx color (0x00 - 0xff)
		      -------x-------- flipx
		      ------x--------- flipy
		      -----x---------- if set, use latched color, else use & latch specified one
		      ----x----------- if set, next sprite entry is part of sequence
		      ---x------------ if clear, use latched y coordinate, else use current y
		      --x------------- if set, y += 16
		      -x-------------- if clear, use latched x coordinate, else use current x
		      x--------------- if set, x += 16
		000a: only valid when the special command bit in 006 is set
		      ---------------x related to sprite ram bank. I think this is the one causing
		                       the bank switch, implementing it this way all games seem
		                       to properly bank switch except for footchmp which uses the
		                       bit in byte 006 instead.
		      ------------x--- unknown; some games toggle it before updating sprite ram.
		      ------xx-------- unknown (finalb)
		      -----x---------- unknown (mjnquest)
		      ---x------------ disable the following sprites until another marker with
			                   this bit clear is found
		      --x------------- flip screen

		000b - 000f : unused

	DG comment: the sprite zoom code grafted on from Jarek's TaitoB
	may mean I have pointlessly duplicated x,y latches in the zoom &
	non zoom parts.

	*/
	int x,y,off,extoffs;
	int code,color,spritedata,spritecont,flipx,flipy;
	int xcurrent,ycurrent,big_sprite=0;
	int y_no=0, x_no=0, xlatch=0, ylatch=0, last_continuation_tile=0;   /* for zooms */
	unsigned int zoomword, zoomx, zoomy, zx=0, zy=0, zoomxlatch=0, zoomylatch=0;   /* for zooms */
	int scroll1x, scroll1y;
	int scrollx=0, scrolly=0;
	int curx,cury;
	int f2_x_offset;
	int sprites_flipscreen = 0;

	/* pdrawgfx() needs us to draw sprites front to back, so we have to build a list
	   while processing sprite ram and then draw them all at the end */
	struct tempsprite *sprite_ptr = spritelist;

	/* must remember enable status from last frame because driftout fails to
	   reactivate them from a certain point onwards. */
	int disabled = sprites_disabled;

	/* must remember master scroll from previous frame because driftout
	   sometimes doesn't set it. */
	int master_scrollx = sprites_master_scrollx;
	int master_scrolly = sprites_master_scrolly;

	/* must also remember the sprite bank from previous frame. */
	int area = sprites_active_area;

	scroll1x = 0;
	scroll1y = 0;
	x = y = 0;
	xcurrent = ycurrent = 0;
	color = 0;

	f2_x_offset = f2_hide_pixels;   /* Get rid of 0-3 unwanted pixels on edge of screen. */
	if (sprites_flipscreen) f2_x_offset = -f2_x_offset;

	/* safety check to avoid getting stuck in bank 2 for games using only one bank */
	if (area == 0x8000 &&
			spriteram_buffered[(0x8000+6)/2] == 0 &&
			spriteram_buffered[(0x8000+10)/2] == 0)
		area = 0;


	for (off = 0;off < 0x4000;off += 16)
	{
		/* sprites_active_area may change during processing */
		int offs = off + area;

		if (spriteram_buffered[(offs+6)/2] & 0x8000)
		{
			disabled = spriteram_buffered[(offs+10)/2] & 0x1000;
			sprites_flipscreen = spriteram_buffered[(offs+10)/2] & 0x2000;
			f2_x_offset = f2_hide_pixels;   /* Get rid of 0-3 unwanted pixels on edge of screen. */
			if (sprites_flipscreen) f2_x_offset = -f2_x_offset;
			area = 0x8000 * (spriteram_buffered[(offs+10)/2] & 0x0001);
			continue;
		}

//usrintf_showmessage("%04x",area);

		/* check for extra scroll offset */
		if ((spriteram_buffered[(offs+4)/2] & 0xf000) == 0xa000)
		{
			master_scrollx = spriteram_buffered[(offs+4)/2] & 0xfff;
			if (master_scrollx >= 0x800) master_scrollx -= 0x1000;   /* signed value */
			master_scrolly = spriteram_buffered[(offs+6)/2] & 0xfff;
			if (master_scrolly >= 0x800) master_scrolly -= 0x1000;   /* signed value */
		}

		if ((spriteram_buffered[(offs+4)/2] & 0xf000) == 0x5000)
		{
			scroll1x = spriteram_buffered[(offs+4)/2] & 0xfff;
			if (scroll1x >= 0x800) scroll1x -= 0x1000;   /* signed value */

			scroll1y = spriteram_buffered[(offs+6)/2] & 0xfff;
			if (scroll1y >= 0x800) scroll1y -= 0x1000;   /* signed value */
		}

		if (disabled)
			continue;

		spritedata = spriteram_buffered[(offs+8)/2];

		spritecont = (spritedata & 0xff00) >> 8;

		if ((spritecont & 0x08) != 0)   /* sprite continuation flag set */
		{
			if (big_sprite == 0)   /* are we starting a big sprite ? */
			{
				xlatch = spriteram_buffered[(offs+4)/2] & 0xfff;
				ylatch = spriteram_buffered[(offs+6)/2] & 0xfff;
				x_no = 0;
				y_no = 0;
				zoomword = spriteram_buffered[(offs+2)/2];
				zoomylatch = (zoomword>>8) & 0xff;
				zoomxlatch = (zoomword) & 0xff;
				big_sprite = 1;   /* we have started a new big sprite */
			}
		}
		else if (big_sprite)
		{
			last_continuation_tile = 1;   /* don't clear big_sprite until last tile done */
		}


		if ((spritecont & 0x04) == 0)
			color = spritedata & 0xff;


// DG: the bigsprite == 0 check fixes "tied-up" little sprites in Thunderfox
// which (mostly?) have spritecont = 0x20 when they are not continuations
// of anything.
		if (big_sprite == 0 || (spritecont & 0xf0) == 0)
		{
			x = spriteram_buffered[(offs+4)/2];

// DG: some absolute x values deduced here are 1 too high (scenes when you get
// home run in Koshien, and may also relate to BG layer woods and stuff as you
// journey in MjnQuest). You will see they are 1 pixel too far to the right.
// Where is this extra pixel offset coming from??

			if (x & 0x8000)   /* absolute (koshien) */
			{
				scrollx = - f2_x_offset - 0x60;
				scrolly = 0;
			}
			else if (x & 0x4000)   /* ignore extra scroll */
			{
				scrollx = master_scrollx - f2_x_offset - 0x60;
				scrolly = master_scrolly;
			}
			else   /* all scrolls applied */
			{
				scrollx = scroll1x + master_scrollx - f2_x_offset - 0x60;
				scrolly = scroll1y + master_scrolly;
			}
			x &= 0xfff;
			y = spriteram_buffered[(offs+6)/2] & 0xfff;

			xcurrent = x;
			ycurrent = y;
		}
		else
		{
			if ((spritecont & 0x10) == 0)
				y = ycurrent;
			else if ((spritecont & 0x20) != 0)
			{
				y += 16;
				y_no++;   /* keep track of y tile for zooms */
			}
			if ((spritecont & 0x40) == 0)
				x = xcurrent;
			else if ((spritecont & 0x80) != 0)
			{
				x += 16;
				y_no=0;
				x_no++;   /* keep track of x tile for zooms */
			}
		}

/* Black lines between flames in Gunfront before zoom finishes suggest
   these calculations are flawed. */

		if (big_sprite)
		{
			zoomx = zoomxlatch;
			zoomy = zoomylatch;
			zx = 0x10;	/* default, no zoom: 16 pixels across */
			zy = 0x10;	/* default, no zoom: 16 pixels vertical */

			if (zoomx || zoomy)
			{
				/* "Zoom" zx&y is pixel size horizontally and vertically
				   of our sprite chunk. So it is difference in x and y
				   coords of our chunk and diagonally adjoining one. */

				x = xlatch + x_no * (0x100 - zoomx) / 16;
				y = ylatch + y_no * (0x100 - zoomy) / 16;
				zx = xlatch + (x_no+1) * (0x100 - zoomx) / 16 - x;
				zy = ylatch + (y_no+1) * (0x100 - zoomy) / 16 - y;
			}
		}
		else
		{
			zoomword = spriteram_buffered[(offs+2)/2];
			zoomy = (zoomword>>8) & 0xff;
			zoomx = (zoomword) & 0xff;
			zx = (0x100 - zoomx) / 16;
			zy = (0x100 - zoomy) / 16;
		}

		if (last_continuation_tile)
		{
			big_sprite=0;
			last_continuation_tile=0;
		}

		code = 0;
		extoffs = offs;
		if (extoffs >= 0x8000) extoffs -= 0x4000;   /* spriteram[0x4000-7fff] has no corresponding extension area */

		if (f2_spriteext == 0)
		{
			int bank;

			code = spriteram_buffered[(offs)/2] & 0x1fff;

			bank = (code & 0x1c00) >> 10;
			code = spritebank[bank] + (code & 0x3ff);
		}

		if (f2_spriteext == 1)   /* Yuyugogo */
		{
			int i;

			code = spriteram_buffered[(offs)/2] & 0x3ff;
			i = (READ_WORD(&f2_sprite_extension[(extoffs >> 3)]) & 0x3f ) << 10;
			code = (i | code);
		}

		if (f2_spriteext == 2)   /* Pulirula */
		{
			int i;

			code = spriteram_buffered[(offs)/2] & 0xff;
			i = (READ_WORD(&f2_sprite_extension[(extoffs >> 3)]) & 0xff00 );
			code = (i | code);
		}

		if (f2_spriteext == 3)   /* Dinorex and a few quizzes */
		{
			int i;

			code = spriteram_buffered[(offs)/2] & 0xff;
			i = (READ_WORD(&f2_sprite_extension[(extoffs >> 3)]) & 0xff ) << 8;
			code = (i | code);
		}

		if (code == 0) continue;

		flipx = spritecont & 0x01;
		flipy = spritecont & 0x02;

		curx = (x + scrollx) & 0xfff;
		if (curx >= 0x800)	curx -= 0x1000;   /* treat it as signed */

		cury = (y + scrolly) & 0xfff;
		if (cury >= 0x800)	cury -= 0x1000;   /* treat it as signed */

		if (sprites_flipscreen)
		{
			/* -zx/y is there to fix zoomed sprite coords in screenflip.
			   drawgfxzoom does not know to draw from flip-side of sprites when
			   screen is flipped; so we must correct the coords ourselves. */

			curx = 320 - curx - zx;
			cury = 256 - cury - zy;
			flipx = !flipx;
			flipy = !flipy;
		}

		{
			sprite_ptr->code = code;
			sprite_ptr->color = color;
			if (Machine->gfx[0]->color_granularity == 64)	/* Final Blow is 6-bit deep */
				sprite_ptr->color /= 4;
			sprite_ptr->flipx = flipx;
			sprite_ptr->flipy = flipy;
			sprite_ptr->x = curx;
			sprite_ptr->y = cury;
			sprite_ptr->zoomx = zx << 12;
			sprite_ptr->zoomy = zy << 12;

			if (primasks)
			{
				sprite_ptr->primask = primasks[(color & 0xc0) >> 6];

				sprite_ptr++;
			}
			else
			{
				drawgfxzoom(bitmap,Machine->gfx[0],
						sprite_ptr->code,
						sprite_ptr->color,
						sprite_ptr->flipx,sprite_ptr->flipy,
						sprite_ptr->x,sprite_ptr->y,
						&Machine->visible_area,TRANSPARENCY_PEN,0,
						sprite_ptr->zoomx,sprite_ptr->zoomy);
			}
		}
	}


	/* this happens only if primsks != NULL */
	while (sprite_ptr != spritelist)
	{
		sprite_ptr--;

		pdrawgfxzoom(bitmap,Machine->gfx[0],
				sprite_ptr->code,
				sprite_ptr->color,
				sprite_ptr->flipx,sprite_ptr->flipy,
				sprite_ptr->x,sprite_ptr->y,
				&Machine->visible_area,TRANSPARENCY_PEN,0,
				sprite_ptr->zoomx,sprite_ptr->zoomy,
				sprite_ptr->primask);
	}
}





static int prepare_sprites;

static void taitof2_handle_sprite_buffering(void)
{
	if (prepare_sprites)	/* no buffering */
	{
		memcpy(spriteram_buffered,spriteram,spriteram_size);
		prepare_sprites = 0;
	}
}

void taitof2_update_sprites_active_area(void)
{
	int off;


	/* if the frame was skipped, we'll have to do the buffering now */
	taitof2_handle_sprite_buffering();

	/* safety check to avoid getting stuck in bank 2 for games using only one bank */
	if (sprites_active_area == 0x8000 &&
			spriteram_buffered[(0x8000+6)/2] == 0 &&
			spriteram_buffered[(0x8000+10)/2] == 0)
		sprites_active_area = 0;

	for (off = 0;off < 0x4000;off += 16)
	{
		/* sprites_active_area may change during processing */
		int offs = off + sprites_active_area;

		if (spriteram_buffered[(offs+6)/2] & 0x8000)
		{
			sprites_disabled = spriteram_buffered[(offs+10)/2] & 0x1000;
			sprites_active_area = 0x8000 * (spriteram_buffered[(offs+10)/2] & 0x0001);
			continue;
		}

		/* check for extra scroll offset */
		if ((spriteram_buffered[(offs+4)/2] & 0xf000) == 0xa000)
		{
			sprites_master_scrollx = spriteram_buffered[(offs+4)/2] & 0xfff;
			if (sprites_master_scrollx >= 0x800) sprites_master_scrollx -= 0x1000;   /* signed value */
			sprites_master_scrolly = spriteram_buffered[(offs+6)/2] & 0xfff;
			if (sprites_master_scrolly >= 0x800) sprites_master_scrolly -= 0x1000;   /* signed value */
		}
	}
}

void taitof2_no_buffer_eof_callback(void)
{
	taitof2_update_sprites_active_area();

	prepare_sprites = 1;
}
void taitof2_partial_buffer_delayed_eof_callback(void)
{
	int i;

	taitof2_update_sprites_active_area();

	prepare_sprites = 0;
	memcpy(spriteram_buffered,spriteram_delayed,spriteram_size);
	for (i = 0;i < spriteram_size/2;i += 4)
		spriteram_buffered[i] = READ_WORD(&spriteram[2*i]);
	memcpy(spriteram_delayed,spriteram,spriteram_size);
}
void taitof2_partial_buffer_delayed_thundfox_eof_callback(void)
{
	int i;

	taitof2_update_sprites_active_area();

	prepare_sprites = 0;
	memcpy(spriteram_buffered,spriteram_delayed,spriteram_size);
	for (i = 0;i < spriteram_size/2;i += 8)
	{
		spriteram_buffered[i]   = READ_WORD(&spriteram[2*i]);
		spriteram_buffered[i+1] = READ_WORD(&spriteram[2*(i+1)]);
		spriteram_buffered[i+4] = READ_WORD(&spriteram[2*(i+4)]);
	}
	memcpy(spriteram_delayed,spriteram,spriteram_size);
}


/* SSI */
void ssi_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	taitof2_handle_sprite_buffering();

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	palette_recalc();

	/* SSI only uses sprites, the tilemap registers are not even initialized.
	   (they are in Majestic 12, but the tilemaps are not used anyway) */
	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);
	draw_sprites(bitmap,NULL);
}


void yesnoj_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	taitof2_handle_sprite_buffering();

	TC0100SCN_tilemap_update();

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);	/* wrong color? */
	draw_sprites(bitmap,NULL);
	TC0100SCN_tilemap_draw(bitmap,0,TC0100SCN_bottomlayer(0),0);
	TC0100SCN_tilemap_draw(bitmap,0,TC0100SCN_bottomlayer(0)^1,0);
	TC0100SCN_tilemap_draw(bitmap,0,2,0);
}


void taitof2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	taitof2_handle_sprite_buffering();

	TC0100SCN_tilemap_update();

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);	/* wrong color? */
	TC0100SCN_tilemap_draw(bitmap,0,TC0100SCN_bottomlayer(0),0);
	TC0100SCN_tilemap_draw(bitmap,0,TC0100SCN_bottomlayer(0)^1,0);
	draw_sprites(bitmap,NULL);
	TC0100SCN_tilemap_draw(bitmap,0,2,0);
}


void taitof2_pri_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int tilepri[3];
	int spritepri[4];
	int layer[3];


	taitof2_handle_sprite_buffering();

	TC0100SCN_tilemap_update();

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	layer[0] = TC0100SCN_bottomlayer(0);
	layer[1] = layer[0]^1;
	layer[2] = 2;
	tilepri[layer[0]] = TC0360PRI_regs[5] & 0x0f;
	tilepri[layer[1]] = TC0360PRI_regs[5] >> 4;
	tilepri[layer[2]] = TC0360PRI_regs[4] >> 4;

	spritepri[0] = TC0360PRI_regs[6] & 0x0f;
	spritepri[1] = TC0360PRI_regs[6] >> 4;
	spritepri[2] = TC0360PRI_regs[7] & 0x0f;
	spritepri[3] = TC0360PRI_regs[7] >> 4;

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);	/* wrong color? */

	TC0100SCN_tilemap_draw(bitmap,0,layer[0],1<<16);
	TC0100SCN_tilemap_draw(bitmap,0,layer[1],2<<16);
	TC0100SCN_tilemap_draw(bitmap,0,layer[2],4<<16);

	{
		int primasks[4] = {0,0,0,0};
		int i;

		for (i = 0;i < 4;i++)
		{
			if (spritepri[i] < tilepri[0]) primasks[i] |= 0xaa;
			if (spritepri[i] < tilepri[1]) primasks[i] |= 0xcc;
			if (spritepri[i] < tilepri[2]) primasks[i] |= 0xf0;
		}

		draw_sprites(bitmap,primasks);
	}
}



static void draw_roz_layer(struct osd_bitmap *bitmap)
{
	if (has_TC0280GRD())
		TC0280GRD_zoom_draw(bitmap,f2_pivot_xdisp,f2_pivot_ydisp,8);

	if (has_TC0430GRW())
		TC0430GRW_zoom_draw(bitmap,f2_pivot_xdisp,f2_pivot_ydisp,8);
}


void taitof2_pri_roz_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int tilepri[3];
	int spritepri[4];
	int rozpri;
	int layer[3];
	int drawn;
	int lastpri;
	int roz_base_color = (TC0360PRI_regs[1] & 0x3f) << 2;


	taitof2_handle_sprite_buffering();

	if (has_TC0280GRD())
		TC0280GRD_tilemap_update(roz_base_color);

	if (has_TC0430GRW())
		TC0430GRW_tilemap_update(roz_base_color);

	TC0100SCN_tilemap_update();

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	{
		int i;

		/* fix roz transparency, but this could compromise the background color */
		for (i = 0;i < 4;i++)
			palette_used_colors[16 * (roz_base_color+i)] = PALETTE_COLOR_TRANSPARENT;
	}
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	layer[0] = TC0100SCN_bottomlayer(0);
	layer[1] = layer[0]^1;
	layer[2] = 2;
	tilepri[layer[0]] = TC0360PRI_regs[5] & 0x0f;
	tilepri[layer[1]] = TC0360PRI_regs[5] >> 4;
	tilepri[layer[2]] = TC0360PRI_regs[4] >> 4;

	spritepri[0] = TC0360PRI_regs[6] & 0x0f;
	spritepri[1] = TC0360PRI_regs[6] >> 4;
	spritepri[2] = TC0360PRI_regs[7] & 0x0f;
	spritepri[3] = TC0360PRI_regs[7] >> 4;

	rozpri = (TC0360PRI_regs[1] & 0xc0) >> 6;
	rozpri = (TC0360PRI_regs[8 + rozpri/2] >> 4*(rozpri & 1)) & 0x0f;

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);	/* wrong color? */

	drawn = 0;
	lastpri = 0;
	while (drawn < 3)
	{
		if (rozpri > lastpri && rozpri <= tilepri[drawn])
		{
			draw_roz_layer(bitmap);
			lastpri = rozpri;
		}
		TC0100SCN_tilemap_draw(bitmap,0,layer[drawn],(1<<drawn)<<16);
		lastpri = tilepri[drawn];
		drawn++;
	}
	if (rozpri > lastpri)
		draw_roz_layer(bitmap);

	{
		int primasks[4] = {0,0,0,0};
		int i;

		for (i = 0;i < 4;i++)
		{
			if (spritepri[i] < tilepri[0]) primasks[i] |= 0xaaaa;
			if (spritepri[i] < tilepri[1]) primasks[i] |= 0xcccc;
			if (spritepri[i] < tilepri[2]) primasks[i] |= 0xf0f0;
			if (spritepri[i] < rozpri)     primasks[i] |= 0xff00;
		}

		draw_sprites(bitmap,primasks);
	}
}



/* Thunderfox */
void thundfox_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int tilepri[2][3];
	int spritepri[4];
	int layer[2][3];
	int drawn[2];


	taitof2_handle_sprite_buffering();

	TC0100SCN_tilemap_update();

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc ())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);


	layer[0][0] = TC0100SCN_bottomlayer(0);
	layer[0][1] = layer[0][0]^1;
	layer[0][2] = 2;
	tilepri[0][layer[0][0]] = TC0360PRI_regs[5] & 0x0f;
	tilepri[0][layer[0][1]] = TC0360PRI_regs[5] >> 4;
	tilepri[0][layer[0][2]] = TC0360PRI_regs[4] >> 4;

	layer[1][0] = TC0100SCN_bottomlayer(1);
	layer[1][1] = layer[1][0]^1;
	layer[1][2] = 2;
	tilepri[1][layer[1][0]] = TC0360PRI_regs[9] & 0x0f;
	tilepri[1][layer[1][1]] = TC0360PRI_regs[9] >> 4;
	tilepri[1][layer[1][2]] = TC0360PRI_regs[8] >> 4;

	spritepri[0] = TC0360PRI_regs[6] & 0x0f;
	spritepri[1] = TC0360PRI_regs[6] >> 4;
	spritepri[2] = TC0360PRI_regs[7] & 0x0f;
	spritepri[3] = TC0360PRI_regs[7] >> 4;


	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);	/* wrong color? */


	/*
	TODO: This isn't the correct way to handle the priority. At the moment of
	writing, pdrawgfx() doesn't support 6 layers, so I have to cheat, assuming
	that the two FG layers are always on top of sprites.
	*/

	drawn[0] = drawn[1] = 0;
	while (drawn[0] < 2 && drawn[1] < 2)
	{
		int pick;

		if (tilepri[0][drawn[0]] < tilepri[1][drawn[1]])
			pick = 0;
		else pick = 1;

		TC0100SCN_tilemap_draw(bitmap,pick,layer[pick][drawn[pick]],(1<<(drawn[pick]+2*pick))<<16);
		drawn[pick]++;
	}
	while (drawn[0] < 2)
	{
		TC0100SCN_tilemap_draw(bitmap,0,layer[0][drawn[0]],(1<<drawn[0])<<16);
		drawn[0]++;
	}
	while (drawn[1] < 2)
	{
		TC0100SCN_tilemap_draw(bitmap,1,layer[1][drawn[1]],(1<<(drawn[1]+2))<<16);
		drawn[1]++;
	}

	{
		int primasks[4] = {0,0,0,0};
		int i;

		for (i = 0;i < 4;i++)
		{
			if (spritepri[i] < tilepri[0][0]) primasks[i] |= 0xaaaa;
			if (spritepri[i] < tilepri[0][1]) primasks[i] |= 0xcccc;
			if (spritepri[i] < tilepri[1][0]) primasks[i] |= 0xf0f0;
			if (spritepri[i] < tilepri[1][1]) primasks[i] |= 0xff00;
		}

		draw_sprites(bitmap,primasks);
	}


	/*
	TODO: This isn't the correct way to handle the priority. At the moment of
	writing, pdrawgfx() doesn't support 6 layers, so I have to cheat, assuming
	that the two FG layers are always on top of sprites.
	*/

	if (tilepri[0][2] < tilepri[1][2])
	{
		TC0100SCN_tilemap_draw(bitmap,0,layer[0][2],0);
		TC0100SCN_tilemap_draw(bitmap,1,layer[1][2],0);
	}
	else
	{
		TC0100SCN_tilemap_draw(bitmap,1,layer[1][2],0);
		TC0100SCN_tilemap_draw(bitmap,0,layer[0][2],0);
	}
}


/*
Table from Raine. Can't see how to calculate order.

The lsbit may be irrelevant to layer order.
Seems there are only 5 truly different layer orders used.
And why does it use 5 reg values that all cause bg0123?
*/

static UINT8 TC0480SCP_pri_lookup[32][4] =
{
	{ 0, 1, 2, 3, },	// 0x00  00000  yes (i.e. seen during game)
	{ 0, 1, 2, 3, },	// 0x01  00001
	{ 0, 1, 2, 3, },	// 0x02  00010
	{ 0, 1, 2, 3, },	// 0x03  00011  yes
	{ 0, 1, 2, 3, },	// 0x04  00100
	{ 0, 1, 2, 3, },	// 0x05  00101
	{ 0, 1, 2, 3, },	// 0x06  00110
	{ 0, 1, 2, 3, },	// 0x07  00111
	{ 0, 1, 2, 3, },	// 0x08  01000
	{ 0, 1, 2, 3, },	// 0x09  01001
	{ 0, 1, 2, 3, },	// 0x0a  01010
	{ 0, 1, 2, 3, },	// 0x0b  01011
	{ 0, 3, 1, 2, },	// 0x0c  01100  yes odd (i.e. not standard bg0123 order)
	{ 0, 1, 2, 3, },	// 0x0d  01101
	{ 0, 1, 2, 3, },	// 0x0e  01110
	{ 3, 0, 1, 2, },	// 0x0f  01111  yes odd
	{ 3, 0, 1, 2, },	// 0x10  10000  yes odd
	{ 0, 1, 2, 3, },	// 0x11  10001
	{ 3, 2, 1, 0, },	// 0x12  10010  yes odd
	{ 3, 2, 1, 0, },	// 0x13  10011  yes odd
	{ 0, 1, 2, 3, },	// 0x14  10100  yes
	{ 0, 1, 2, 3, },	// 0x15  10101
	{ 0, 1, 2, 3, },	// 0x16  10110
	{ 0, 1, 2, 3, },	// 0x17  10111  yes
	{ 0, 1, 2, 3, },	// 0x18  11000
	{ 0, 1, 2, 3, },	// 0x19  11001
	{ 0, 1, 2, 3, },	// 0x1a  11010
	{ 0, 1, 2, 3, },	// 0x1b  11011
	{ 0, 1, 2, 3, },	// 0x1c  11100  yes
	{ 0, 1, 2, 3, },	// 0x1d  11101
	{ 0, 3, 2, 1, },	// 0x1e  11110  yes odd
	{ 0, 3, 2, 1, },	// 0x1f  11111  yes odd
};


/*********************************************************************

Deadconx and Footchmp use in the PRI chip
-----------------------------------------

+4	xxxx0000   BG0
	0000xxxx   BG3
+6	xxxx0000   BG2
	0000xxxx   BG1

Deadconx = 0x7db9 (bg0-3) 0x8eca (sprites)

So it has bg0 [back] / s / bg1 / s / bg2 / s / bg3 / s

Footchmp = 0x8db9 (bg0-3) 0xe5ac (sprites)

So it has s / bg0 [grass] / bg1 [crowd] / s / bg2 [goal] / s / bg3 [messages] / s [player scan dots]


Metalb uses in the PRI chip
---------------------------

+4	xxxx0000   BG1
	0000xxxx   BG0
+6	xxxx0000   BG3
	0000xxxx   BG2

and it changes these (and the sprite pri settings) quite a lot...
The sprite/tile priority seems fine.
********************************************************************/

void metalb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer[5];
	int tilepri[5];
	int spritepri[4];
	int priority;

/*
Layer toggles to help get the layer offsets in Metalb correct.
Some are still suspect! [see taitoic.c for more about this]
*/

	taitof2_handle_sprite_buffering();

	TC0480SCP_tilemap_update();
	priority = TC0480SCP_pri_reg & 0x1f;

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	{
		int i;

		/* fix TC0480SCP transparency, but this could compromise the background color */
		for (i = 0;i < Machine->drv->total_colors;i += 16)
			palette_used_colors[i] = PALETTE_COLOR_TRANSPARENT;
	}
	if (palette_recalc ())
	{
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
	}

	tilemap_render(ALL_TILEMAPS);

	layer[0] = TC0480SCP_pri_lookup[priority][0];   /* tells us which bg layer is bottom */
	layer[1] = TC0480SCP_pri_lookup[priority][1];
	layer[2] = TC0480SCP_pri_lookup[priority][2];
	layer[3] = TC0480SCP_pri_lookup[priority][3];   /* tells us which is top */

	layer[4] = 4;   // Text layer

	tilepri[0] = TC0360PRI_regs[4] & 0x0f;     /* bg0 */
	tilepri[1] = TC0360PRI_regs[4] >> 4;       /* bg1 */
	tilepri[2] = TC0360PRI_regs[5] & 0x0f;     /* bg2 */
	tilepri[3] = TC0360PRI_regs[5] >> 4;       /* bg3 */

/* we actually assume text layer is on top of everything anyway, but FWIW... */
	tilepri[layer[4]] = TC0360PRI_regs[7] & 0x0f;    /* fg (text layer) */

	spritepri[0] = TC0360PRI_regs[6] & 0x0f;
	spritepri[1] = TC0360PRI_regs[6] >> 4;
	spritepri[2] = TC0360PRI_regs[7] & 0x0f;
	spritepri[3] = TC0360PRI_regs[7] >> 4;

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	TC0480SCP_tilemap_draw(bitmap,layer[0],1<<16);

	TC0480SCP_tilemap_draw(bitmap,layer[1],2<<16);

	TC0480SCP_tilemap_draw(bitmap,layer[2],4<<16);

	TC0480SCP_tilemap_draw(bitmap,layer[3],8<<16);

	{
		int primasks[4] = {0,0,0,0};
		int i;

		for (i = 0;i < 4;i++)
		{
			if (spritepri[i] < tilepri[(layer[0])]) primasks[i] |= 0xaaaa;
			if (spritepri[i] < tilepri[(layer[1])]) primasks[i] |= 0xcccc;
			if (spritepri[i] < tilepri[(layer[2])]) primasks[i] |= 0xf0f0;
			if (spritepri[i] < tilepri[(layer[3])]) primasks[i] |= 0xff00;
		}

		draw_sprites(bitmap,primasks);
	}

	/*
	TODO: This isn't the correct way to handle the priority. At the moment of
	writing, pdrawgfx() doesn't support 5 layers (??), so I have to cheat, assuming
	that the FG layer is always on top of sprites.
	*/

	TC0480SCP_tilemap_draw(bitmap,layer[4],0);
}


/* Deadconx, Footchmp */
void deadconx_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer[5];
	int tilepri[5];
	int spritepri[4];

	taitof2_handle_sprite_buffering();

	TC0480SCP_tilemap_update();

	palette_init_used_colors();
	taitof2_update_palette();
	palette_used_colors[0] |= PALETTE_COLOR_VISIBLE;
	{
		int i;

		/* fix TC0480SCP transparency, but this could compromise the background color */
		for (i = 0;i < Machine->drv->total_colors;i += 16)
			palette_used_colors[i] = PALETTE_COLOR_TRANSPARENT;
	}
	if (palette_recalc ())
	{
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
	}

	tilemap_render(ALL_TILEMAPS);

	layer[0] = 0;	/* bottom bg */
	layer[1] = 1;
	layer[2] = 2;
	layer[3] = 3;	/* top bg */
	layer[4] = 4;	/* text */

	tilepri[0] = TC0360PRI_regs[4] >> 4;      /* bg0 */
	tilepri[1] = TC0360PRI_regs[5] & 0x0f;    /* bg1 */
	tilepri[2] = TC0360PRI_regs[5] >> 4;      /* bg2 */
	tilepri[3] = TC0360PRI_regs[4] & 0x0f;    /* bg3 */

/* we actually assume text layer is on top of everything anyway, but FWIW... */
	tilepri[layer[4]] = TC0360PRI_regs[7] >> 4;    /* fg (text layer) */

	spritepri[0] = TC0360PRI_regs[6] & 0x0f;
	spritepri[1] = TC0360PRI_regs[6] >> 4;
	spritepri[2] = TC0360PRI_regs[7] & 0x0f;
	spritepri[3] = TC0360PRI_regs[7] >> 4;

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	TC0480SCP_tilemap_draw(bitmap,layer[0],1<<16);
	TC0480SCP_tilemap_draw(bitmap,layer[1],2<<16);
	TC0480SCP_tilemap_draw(bitmap,layer[2],4<<16);
	TC0480SCP_tilemap_draw(bitmap,layer[3],8<<16);

	{
		int primasks[4] = {0,0,0,0};
		int i;

		for (i = 0;i < 4;i++)
		{
			if (spritepri[i] < tilepri[(layer[0])]) primasks[i] |= 0xaaaa;
			if (spritepri[i] < tilepri[(layer[1])]) primasks[i] |= 0xcccc;
			if (spritepri[i] < tilepri[(layer[2])]) primasks[i] |= 0xf0f0;
			if (spritepri[i] < tilepri[(layer[3])]) primasks[i] |= 0xff00;
		}

		draw_sprites(bitmap,primasks);
	}

	/*
	TODO: This isn't the correct way to handle the priority. At the moment of
	writing, pdrawgfx() doesn't support 5 layers (??), so I have to cheat, assuming
	that the FG layer is always on top of sprites.
	*/

	TC0480SCP_tilemap_draw(bitmap,layer[4],0);
}
