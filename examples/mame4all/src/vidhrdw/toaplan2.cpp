/***************************************************************************

  Functions to emulate the video hardware of some Toaplan games


 To Do / Unknowns
	-  Hack is needed to reset sound CPU and sound chip when machine
		is 'tilted'in Pipi & Bibis. Otherwise sound CPU interferes
		with the main CPU test of shared RAM. You get a 'Sub CPU RAM Error'
	-  What do Scroll registers 0Eh and 0Fh really do ????
	-  Snow Bros 2 sets bit 6 of the sprite X info word during weather
		world map, and bits 4, 5 and 6 of the sprite X info word during
		the Rabbit boss screen - reasons are unknown.
	-  Fourth set of scroll registers have been used for Sprite scroll
		though it may not be correct. For most parts this looks right
		except for Snow Bros 2 when in the rabbit boss screen (all sprites
		jump when big green nasty (which is the foreground layer) comes
		in from the left)
	-  Teki Paki tests video RAM from address 0 past SpriteRAM to $37ff.
		It doesnt seem to use that memory above SpriteRAM for anything though.
	-  Teki Paki sprites seem to be three frames ahead of the tile layers.
		Since this driver double buffers the sprites, Teki Paki is still
		suffering sprite lag (Does it need an extra buffer - quite strange)
	-  Batsugun, relationship between the two video controllers (priority
		wise) is wrong and unknown.

 Video RAM address layout:

	Bank		  data size of video layer
	-----------------------------------------
	$0000-07FF	  800h words for background layer
	$0800-0FFF	  800h words for foreground layer
	$1000-17FF	  800h words for top (text) layer
	$1800-1BFF	  400h words for sprites (100 possible sprites)



 Tile RAM format (each tile takes up 32 bits)

  0         1         2         3
  ---- ---- ---- ---- xxxx xxxx xxxx xxxx = Tile number (0 - FFFFh)
  ---- ---- -xxx xxxx ---- ---- ---- ---- = Color (0 - 7Fh)
  ---- ---- ?--- ---- ---- ---- ---- ---- = unknown / unused
  ---- xxxx ---- ---- ---- ---- ---- ---- = Priority (0 - Fh)
  ???? ---- ---- ---- ---- ---- ---- ---- = unknown / unused / possible flips

Sprites are of varying sizes between 8x8 and 128x128 and any size
inbetween, in multiples of 8 either way.

Here we draw the first 8x8 part of the sprite, then by using the sprite
dimensions, we draw the rest of the 8x8 parts to produce the complete
sprite.

There seems to be sprite buffering - double buffering actually.

 Sprite RAM format (data for each sprite takes up 4 words)

  0
  ---- ----  ---- --xx = top 2 bits of Sprite number
  ---- ----  xxxx xx-- = Color (0 - 3Fh)
  ---- xxxx  ---- ---- = Priority (0 - Fh)
  ---x ----  ---- ---- = Flip X
  --x- ----  ---- ---- = Flip Y
  -?-- ----  ---- ---- = unknown / unused
  x--- ----  ---- ---- = Show sprite ?

  1
  xxxx xxxx  xxxx xxxx = Sprite number (top two bits in word 0)

  2
  ---- ----  ---- xxxx = Sprite X size (add 1, then multiply by 8)
  ---- ----  -??? ---- = unknown - used in Snow Bros. 2
  xxxx xxxx  x--- ---- = X position

  3
  ---- ----  ---- xxxx = Sprite Y size (add 1, then multiply by 8)
  ---- ----  -??? ---- = unknown / unused
  xxxx xxxx  x--- ---- = Y position


 Extra-text RAM format (data for each text takes up 2 words)

Raizing and Tatsujin2 and Fixeight have extra-text layer.

  0
  ---- ---- ---- --xx = top 2 bits of Tile number
  ---- ---- xxxx xx-- = Color (0 - 3Fh) + 40h

  1
  xxxx xxxx xxxx xxxx = Tile number (top two bits in word 0)


Scroll Registers (hex) :

	00		Background scroll X (X flip off)
	01		Background scroll Y (Y flip off)
	02		Foreground scroll X (X flip off)
	03		Foreground scroll Y (Y flip off)
	04		Top (text) scroll X (X flip off)
	05		Top (text) scroll Y (Y flip off)
	06		Sprites    scroll X (X flip off) ???
	07		Sprites    scroll Y (Y flip off) ???
	0E		???? Initialise Video controller at startup ????
	0F		Scroll update complete ??? (Not used in Ghox and V-Five)

	80		Background scroll X (X flip on)
	81		Background scroll Y (Y flip on)
	82		Foreground scroll X (X flip on)
	83		Foreground scroll Y (Y flip on)
	84		Top (text) scroll X (X flip on)
	85		Top (text) scroll Y (Y flip on)
	86		Sprites    scroll X (X flip on) ???
	87		Sprites    scroll Y (Y flip on) ???
	8F		Same as 0Fh except flip bit is active


Scroll Register 0E writes (Video controller inits ?) from different games:

Teki-Paki		 | Ghox				| Knuckle Bash	   | Tatsujin 2		  |
0003, 0002, 4000 | ????, ????, ???? | 0202, 0203, 4200 | 0003, 0002, 4000 |

Dogyuun			 | Batsugun			|
0202, 0203, 4200 | 0202, 0203, 4200 |
1202, 1203, 5200 | 1202, 1203, 5200 | <--- Second video controller ???

Pipi & Bibis	 | Fix Eight		| V-Five		   | Snow Bros. 2	  |
0003, 0002, 4000 | 0202, 0203, 4200 | 0202, 0203, 4200 | 0202, 0203, 4200 |

***************************************************************************/

#include "driver.h"
#include "tilemap.h"



#define TOAPLAN2_BG_VRAM_SIZE	0x1000	/* Background RAM size (in bytes) */
#define TOAPLAN2_FG_VRAM_SIZE	0x1000	/* Foreground RAM size (in bytes) */
#define TOAPLAN2_TOP_VRAM_SIZE	0x1000	/* Top Layer  RAM size (in bytes) */
#define TOAPLAN2_SPRITERAM_SIZE	0x0800	/* Sprite	  RAM size (in bytes) */

#define TOAPLAN2_TEXT_VRAM_SIZE	0x4000	/* Text Layer RAM size (in bytes) */

#define TOAPLAN2_SPRITE_FLIPX 0x1000	/* Sprite flip flags (for screen flip) */
#define TOAPLAN2_SPRITE_FLIPY 0x2000

#define CPU_2_NONE		0x00
#define CPU_2_Z80		0x5a
#define CPU_2_HD647180	0xa5
#define CPU_2_Zx80		0xff


static unsigned char *bgvideoram[2];
static unsigned char *fgvideoram[2];
static unsigned char *topvideoram[2];
static unsigned char *spriteram_now[2];	 /* Sprites to draw this frame */
static unsigned char *spriteram_next[2]; /* Sprites to draw next frame */
static unsigned char *spriteram_new[2];	 /* Sprites to add to next frame */
static int toaplan2_unk_vram;			 /* Video RAM tested but not used (for Teki Paki)*/

static int toaplan2_scroll_reg[2];
static int toaplan2_voffs[2];
static int bg_offs[2];
static int fg_offs[2];
static int top_offs[2];
static int sprite_offs[2];
static int bg_scrollx[2];
static int bg_scrolly[2];
static int fg_scrollx[2];
static int fg_scrolly[2];
static int top_scrollx[2];
static int top_scrolly[2];
static int sprite_scrollx[2];
static int sprite_scrolly[2];

static int display_sp[2] = { 1, 1 };

static int sprite_priority[2][16];
static int bg_flip[2] = { 0, 0 };
static int fg_flip[2] = { 0, 0 };
static int top_flip[2] = { 0, 0 };
static int sprite_flip[2] = { 0, 0 };

extern int toaplan2_sub_cpu;

static struct tilemap *top_tilemap[2], *fg_tilemap[2], *bg_tilemap[2];

/* Added by Yochizo 2000/08/19 */
unsigned char *textvideoram;			 /* Video ram for extra-text-layer */
static struct tilemap *text_tilemap;	 /* Tilemap for extra-text-layer */


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_top0_tile_info(int tile_index)
{
	int color, tile_number, attrib;
	UINT16 *source = (UINT16 *)(topvideoram[0]);

	attrib = source[2*tile_index];
	tile_number = source[2*tile_index+1];
	color = attrib & 0x7f;
	SET_TILE_INFO(0,tile_number,color)
	tile_info.priority = (attrib & 0x0f00) >> 8;
}

static void get_fg0_tile_info(int tile_index)
{
	int color, tile_number, attrib;
	UINT16 *source = (UINT16 *)(fgvideoram[0]);

	attrib = source[2*tile_index];
	tile_number = source[2*tile_index+1];
	color = attrib & 0x7f;
	SET_TILE_INFO(0,tile_number,color)
	tile_info.priority = (attrib & 0x0f00) >> 8;
}

static void get_bg0_tile_info(int tile_index)
{
	int color, tile_number, attrib;
	UINT16 *source = (UINT16 *)(bgvideoram[0]);

	attrib = source[2*tile_index];
	tile_number = source[2*tile_index+1];
	color = attrib & 0x7f;
	SET_TILE_INFO(0,tile_number,color)
	tile_info.priority = (attrib & 0x0f00) >> 8;
}

static void get_top1_tile_info(int tile_index)
{
	int color, tile_number, attrib;
	UINT16 *source = (UINT16 *)(topvideoram[1]);

	attrib = source[2*tile_index];
	tile_number = source[2*tile_index+1];
	color = attrib & 0x7f;
	SET_TILE_INFO(2,tile_number,color)
	tile_info.priority = (attrib & 0x0f00) >> 8;
}

static void get_fg1_tile_info(int tile_index)
{
	int color, tile_number, attrib;
	UINT16 *source = (UINT16 *)(fgvideoram[1]);

	attrib = source[2*tile_index];
	tile_number = source[2*tile_index+1];
	color = attrib & 0x7f;
	SET_TILE_INFO(2,tile_number,color)
	tile_info.priority = (attrib & 0x0f00) >> 8;
}

static void get_bg1_tile_info(int tile_index)
{
	int color, tile_number, attrib;
	UINT16 *source = (UINT16 *)(bgvideoram[1]);

	attrib = source[2*tile_index];
	tile_number = source[2*tile_index+1];
	color = attrib & 0x7f;
	SET_TILE_INFO(2,tile_number,color)
	tile_info.priority = (attrib & 0x0f00) >> 8;
}

/* Added by Yochizo 2000/08/19 */
static void get_text_tile_info(int tile_index)
{
	int color, tile_number, attrib;
	UINT16 *source = (UINT16 *)textvideoram;

	attrib = source[tile_index];
	tile_number = attrib & 0x3ff;
	color = ((attrib >> 10) | 0x40) & 0x7f;
	SET_TILE_INFO(2,tile_number,color)
	tile_info.priority = 0;
}

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
static void toaplan2_vh_stop(int controller)
{
	free(     bgvideoram[controller] );
	free(     fgvideoram[controller] );
	free(    topvideoram[controller] );
	free(  spriteram_now[controller] );
	free( spriteram_next[controller] );
	free(  spriteram_new[controller] );
}
void toaplan2_0_vh_stop(void)
{
	toaplan2_vh_stop(0);
}
void toaplan2_1_vh_stop(void)
{
	toaplan2_vh_stop(1);
	toaplan2_vh_stop(0);
}


static int create_tilemaps_0(void)
{
	top_tilemap[0] = tilemap_create(get_top0_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	fg_tilemap[0] = tilemap_create(get_fg0_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	bg_tilemap[0] = tilemap_create(get_bg0_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);

	if (!top_tilemap[0] || !fg_tilemap[0] || !bg_tilemap[0])
		return 1;

	top_tilemap[0]->transparent_pen = 0;
	fg_tilemap[0]->transparent_pen = 0;
	bg_tilemap[0]->transparent_pen = 0;

	return 0;
}
static int create_tilemaps_1(void)
{
	top_tilemap[1] = tilemap_create(get_top1_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	fg_tilemap[1] = tilemap_create(get_fg1_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	bg_tilemap[1] = tilemap_create(get_bg1_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);

	if (!top_tilemap[1] || !fg_tilemap[1] || !bg_tilemap[1])
		return 1;

	top_tilemap[1]->transparent_pen = 0;
	fg_tilemap[1]->transparent_pen = 0;
	bg_tilemap[1]->transparent_pen = 0;

	return 0;
}

static int toaplan2_vh_start(int controller)
{
	static int error_level = 0;
	if ((spriteram_new[controller] = (unsigned char*)malloc(TOAPLAN2_SPRITERAM_SIZE)) == 0)
	{
		return 1;
	}
	memset(spriteram_new[controller],0,TOAPLAN2_SPRITERAM_SIZE);

	if ((spriteram_next[controller] = (unsigned char*)malloc(TOAPLAN2_SPRITERAM_SIZE)) == 0)
	{
		free( spriteram_new[controller] );
		return 1;
	}
	memset(spriteram_next[controller],0,TOAPLAN2_SPRITERAM_SIZE);

	if ((spriteram_now[controller] = (unsigned char*)malloc(TOAPLAN2_SPRITERAM_SIZE)) == 0)
	{
		free( spriteram_next[controller] );
		free(  spriteram_new[controller] );
		return 1;
	}
	memset(spriteram_now[controller],0,TOAPLAN2_SPRITERAM_SIZE);

	if ((topvideoram[controller] = (unsigned char*)malloc(TOAPLAN2_TOP_VRAM_SIZE)) == 0)
	{
		free(  spriteram_now[controller] );
		free( spriteram_next[controller] );
		free(  spriteram_new[controller] );
		return 1;
	}
	memset(topvideoram[controller],0,TOAPLAN2_TOP_VRAM_SIZE);

	if ((fgvideoram[controller] = (unsigned char*)malloc(TOAPLAN2_FG_VRAM_SIZE)) == 0)
	{
		free(    topvideoram[controller] );
		free(  spriteram_now[controller] );
		free( spriteram_next[controller] );
		free(  spriteram_new[controller] );
		return 1;
	}
	memset(fgvideoram[controller],0,TOAPLAN2_FG_VRAM_SIZE);

	if ((bgvideoram[controller] = (unsigned char*)malloc(TOAPLAN2_BG_VRAM_SIZE)) == 0)
	{
		free(     fgvideoram[controller] );
		free(    topvideoram[controller] );
		free(  spriteram_now[controller] );
		free( spriteram_next[controller] );
		free(  spriteram_new[controller] );
		return 1;
	}
	memset(bgvideoram[controller],0,TOAPLAN2_BG_VRAM_SIZE);

	if (controller == 0)
	{
		error_level |= create_tilemaps_0();
	}
	if (controller == 1)
	{
		error_level |= create_tilemaps_1();
	}
	return error_level;
}
int toaplan2_0_vh_start(void)
{
	return toaplan2_vh_start(0);
}
int toaplan2_1_vh_start(void)
{
	int error_level = 0;
	error_level |= toaplan2_vh_start(0);
	error_level |= toaplan2_vh_start(1);
	return error_level;
}
/* Added by Yochizo 2000/08/19 */
int raizing_0_vh_start(void)
{
	if (toaplan2_vh_start(0))
		return 1;
	memset(textvideoram,0,TOAPLAN2_TEXT_VRAM_SIZE);
	text_tilemap = tilemap_create(get_text_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
	if (!text_tilemap)
		return 1;
	text_tilemap->transparent_pen = 0;
	return 0;
}
int raizing_1_vh_start(void)
{
	int error_level = 0;
	error_level |= toaplan2_vh_start(0);
	error_level |= toaplan2_vh_start(1);
	if (error_level)
		return 1;
	memset(textvideoram,0,TOAPLAN2_TEXT_VRAM_SIZE);
	text_tilemap = tilemap_create(get_text_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
	if (!text_tilemap)
		return 1;
	text_tilemap->transparent_pen = 0;
	return 0;
}



/***************************************************************************

  Video I/O port hardware.

***************************************************************************/

void toaplan2_voffs_w(int offset, int data, int controller)
{
	toaplan2_voffs[controller] = data;

	/* Layers are seperated by ranges in the offset */
	switch (data & 0xfc00)
	{
		case 0x0400:
		case 0x0000:	bg_offs[controller] = (data & 0x7ff) * 2; break;
		case 0x0c00:
		case 0x0800:	fg_offs[controller] = (data & 0x7ff) * 2; break;
		case 0x1400:
		case 0x1000:	top_offs[controller] = (data & 0x7ff) * 2; break;
		case 0x1800:	sprite_offs[controller] = (data & 0x3ff) * 2; break;
		default:		//logerror("Hmmm, unknown video controller %01x layer being selected (%08x)\n",controller,data);
						data &= 0x1800;
						if ((data & 0x1800) == 0x0000)
							bg_offs[controller] = (data & 0x7ff) * 2;
						if ((data & 0x1800) == 0x0800)
							fg_offs[controller] = (data & 0x7ff) * 2;
						if ((data & 0x1800) == 0x1000)
							top_offs[controller] = (data & 0x7ff) * 2;
						if ((data & 0x1800) == 0x1800)
							sprite_offs[controller] = (data & 0x3ff) * 2;
						break;
	}
}
WRITE_HANDLER( toaplan2_0_voffs_w )
{
	toaplan2_voffs_w(offset, data, 0);
}
WRITE_HANDLER( toaplan2_1_voffs_w )
{
	toaplan2_voffs_w(offset, data, 1);
}
/* Added by Yochizo 2000/08/19 */
READ_HANDLER( raizing_textram_r )
{
	return READ_WORD(&textvideoram[offset]);
}
WRITE_HANDLER( raizing_textram_w )
{
	int oldword;
	offset &= TOAPLAN2_TEXT_VRAM_SIZE - 1;
	oldword = READ_WORD(&textvideoram[offset]);
	if (oldword != data){
		WRITE_WORD(&textvideoram[offset], data);
		tilemap_mark_tile_dirty(text_tilemap,offset / 2);
//		tilemap_mark_all_tiles_dirty(text_tilemap);
	}
}

int toaplan2_videoram_r(int offset, int controller)
{
	static int video_data = 0;
	int videoram_offset;

	switch (toaplan2_voffs[controller] & 0xfc00)
	{
		case 0x0400:
		case 0x0000:
				videoram_offset = bg_offs[controller] & (TOAPLAN2_BG_VRAM_SIZE-1);
				video_data = READ_WORD (&bgvideoram[controller][videoram_offset]);
				bg_offs[controller] += 2;
				/*if (bg_offs[controller] > TOAPLAN2_BG_VRAM_SIZE)
				{
					logerror("Reading %04x from out of range BG Layer address (%08x)  Video controller %01x  !!!\n",video_data,bg_offs[controller],controller);
				}*/
				break;
		case 0x0c00:
		case 0x0800:
				videoram_offset = fg_offs[controller] & (TOAPLAN2_FG_VRAM_SIZE-1);
				video_data = READ_WORD (&fgvideoram[controller][videoram_offset]);
				fg_offs[controller] += 2;
				/*if (fg_offs[controller] > TOAPLAN2_FG_VRAM_SIZE)
				{
					logerror("Reading %04x from out of range FG Layer address (%08x)  Video controller %01x  !!!\n",video_data,fg_offs[controller],controller);
				}*/
				break;
		case 0x1400:
		case 0x1000:
				videoram_offset = top_offs[controller] & (TOAPLAN2_TOP_VRAM_SIZE-1);
				video_data = READ_WORD (&topvideoram[controller][videoram_offset]);
				top_offs[controller] += 2;
				/*if (top_offs[controller] > TOAPLAN2_TOP_VRAM_SIZE)
				{
					logerror("Reading %04x from out of range TOP Layer address (%08x)  Video controller %01x  !!!\n",video_data,top_offs[controller],controller);
				}*/
				break;
		case 0x1800:
				videoram_offset = sprite_offs[controller] & (TOAPLAN2_SPRITERAM_SIZE-1);
				video_data = READ_WORD (&spriteram_new[controller][videoram_offset]);
				sprite_offs[controller] += 2;
				/*if (sprite_offs[controller] > TOAPLAN2_SPRITERAM_SIZE)
				{
					logerror("Reading %04x from out of range Sprite address (%08x)  Video controller %01x  !!!\n",video_data,sprite_offs[controller],controller);
				}*/
				break;
		default:
				video_data = toaplan2_unk_vram;
				//logerror("Hmmm, reading %04x from unknown video layer (%08x)  Video controller %01x  !!!\n",video_data,toaplan2_voffs[controller],controller);
				break;
	}
	return video_data;
}
READ_HANDLER( toaplan2_0_videoram_r )
{
	return toaplan2_videoram_r(offset, 0);
}
READ_HANDLER( toaplan2_1_videoram_r )
{
	return toaplan2_videoram_r(offset, 1);
}

void toaplan2_videoram_w(int offset, int data, int controller)
{
	int oldword = 0;
	int videoram_offset;
	int dirty_cell;

	switch (toaplan2_voffs[controller] & 0xfc00)
	{
		case 0x0400:
		case 0x0000:
				videoram_offset = bg_offs[controller] & (TOAPLAN2_BG_VRAM_SIZE-1);
				oldword = READ_WORD (&bgvideoram[controller][videoram_offset]);
				if (data != oldword)
				{
					WRITE_WORD (&bgvideoram[controller][videoram_offset],data);
					dirty_cell = (bg_offs[controller] & (TOAPLAN2_BG_VRAM_SIZE-3))/2;
					tilemap_mark_tile_dirty(bg_tilemap[controller],dirty_cell/2);
				}
				bg_offs[controller] += 2;
				/*if (bg_offs[controller] > TOAPLAN2_BG_VRAM_SIZE)
				{
					logerror("Writing %04x to out of range BG Layer address (%08x)  Video controller %01x  !!!\n",data,bg_offs[controller],controller);
				}*/
				break;
		case 0x0c00:
		case 0x0800:
				videoram_offset = fg_offs[controller] & (TOAPLAN2_FG_VRAM_SIZE-1);
				oldword = READ_WORD (&fgvideoram[controller][videoram_offset]);
				if (data != oldword)
				{
					WRITE_WORD (&fgvideoram[controller][videoram_offset],data);
					dirty_cell = (fg_offs[controller] & (TOAPLAN2_FG_VRAM_SIZE-3))/2;
					tilemap_mark_tile_dirty(fg_tilemap[controller],dirty_cell/2);
				}
				fg_offs[controller] += 2;
				/*if (fg_offs[controller] > TOAPLAN2_FG_VRAM_SIZE)
				{
					logerror("Writing %04x to out of range FG Layer address (%08x)  Video controller %01x  !!!\n",data,fg_offs[controller],controller);
				}*/
				break;
		case 0x1400:
		case 0x1000:
				videoram_offset = top_offs[controller] & (TOAPLAN2_TOP_VRAM_SIZE-1);
				oldword = READ_WORD (&topvideoram[controller][videoram_offset]);
				if (data != oldword)
				{
					WRITE_WORD (&topvideoram[controller][videoram_offset],data);
					dirty_cell = (top_offs[controller] & (TOAPLAN2_TOP_VRAM_SIZE-3))/2;
					tilemap_mark_tile_dirty(top_tilemap[controller],dirty_cell/2);
				}
				top_offs[controller] += 2;
				/*if (top_offs[controller] > TOAPLAN2_TOP_VRAM_SIZE)
				{
					logerror("Writing %04x to out of range TOP Layer address (%08x)  Video controller %01x  !!!\n",data,top_offs[controller],controller);
				}*/
				break;
		case 0x1800:
				videoram_offset = sprite_offs[controller] & (TOAPLAN2_SPRITERAM_SIZE-1);
				WRITE_WORD (&spriteram_new[controller][videoram_offset],data);
				sprite_offs[controller] += 2;
				/*if (sprite_offs[controller] > TOAPLAN2_SPRITERAM_SIZE)
				{
					logerror("Writing %04x to out of range Sprite address (%08x)  Video controller %01x  !!!\n",data,sprite_offs[controller],controller);
				}*/
				break;
		default:
				toaplan2_unk_vram = data;
				//logerror("Hmmm, writing %04x to unknown video layer (%08x)  Video controller %01x  \n",toaplan2_unk_vram,toaplan2_voffs[controller],controller);
				break;
	}
}
WRITE_HANDLER( toaplan2_0_videoram_w )
{
	toaplan2_videoram_w(offset, data, 0);
}
WRITE_HANDLER( toaplan2_1_videoram_w )
{
	toaplan2_videoram_w(offset, data, 1);
}


void toaplan2_scroll_reg_select_w(int offset, int data, int controller)
{
	toaplan2_scroll_reg[controller] = data;
	/*if (toaplan2_scroll_reg[controller] & 0xffffff70)
	{
		logerror("Hmmm, unknown video control register selected (%08x)  Video controller %01x  \n",toaplan2_scroll_reg[controller],controller);
	}*/
}
WRITE_HANDLER( toaplan2_0_scroll_reg_select_w )
{
	toaplan2_scroll_reg_select_w(offset, data, 0);
}
WRITE_HANDLER( toaplan2_1_scroll_reg_select_w )
{
	toaplan2_scroll_reg_select_w(offset, data, 1);
}


void toaplan2_scroll_reg_data_w(int offset, int data, int controller)
{
	/************************************************************************/
	/***** X and Y layer flips can be set independantly, so emulate it ******/
	/************************************************************************/

	switch(toaplan2_scroll_reg[controller])
	{
		case 0x00:	bg_scrollx[controller] = data - 0x1d6;			/* 1D6h */
					bg_flip[controller] &= (~TILEMAP_FLIPX);
					tilemap_set_flip(bg_tilemap[controller],bg_flip[controller]);
					tilemap_set_scrollx(bg_tilemap[controller],0,bg_scrollx[controller]);
					break;
		case 0x01:	bg_scrolly[controller] = data - 0x1ef;			/* 1EFh */
					bg_flip[controller] &= (~TILEMAP_FLIPY);
					tilemap_set_flip(bg_tilemap[controller],bg_flip[controller]);
					tilemap_set_scrolly(bg_tilemap[controller],0,bg_scrolly[controller]);
					break;
		case 0x02:	fg_scrollx[controller] = data - 0x1d8;			/* 1D0h */
					fg_flip[controller] &= (~TILEMAP_FLIPX);
					tilemap_set_flip(fg_tilemap[controller],fg_flip[controller]);
					tilemap_set_scrollx(fg_tilemap[controller],0,fg_scrollx[controller]);
					break;
		case 0x03:  fg_scrolly[controller] = data - 0x1ef;			/* 1EFh */
					fg_flip[controller] &= (~TILEMAP_FLIPY);
					tilemap_set_flip(fg_tilemap[controller],fg_flip[controller]);
					tilemap_set_scrolly(fg_tilemap[controller],0,fg_scrolly[controller]);
					break;
		case 0x04:	top_scrollx[controller] = data - 0x1da;			/* 1DAh */
					top_flip[controller] &= (~TILEMAP_FLIPX);
					tilemap_set_flip(top_tilemap[controller],top_flip[controller]);
					tilemap_set_scrollx(top_tilemap[controller],0,top_scrollx[controller]);
					break;
		case 0x05:	top_scrolly[controller] = data - 0x1ef;			/* 1EFh */
					top_flip[controller] &= (~TILEMAP_FLIPY);
					tilemap_set_flip(top_tilemap[controller],top_flip[controller]);
					tilemap_set_scrolly(top_tilemap[controller],0,top_scrolly[controller]);
					break;
		case 0x06:	sprite_scrollx[controller] = data - 0x1cc;		/* 1D4h */
					if (sprite_scrollx[controller] & 0x80000000) sprite_scrollx[controller] |= 0xfffffe00;
					else sprite_scrollx[controller] &= 0x1ff;
					sprite_flip[controller] &= (~TOAPLAN2_SPRITE_FLIPX);
					break;
		case 0x07:	sprite_scrolly[controller] = data - 0x1ef;		/* 1F7h */
					if (sprite_scrolly[controller] & 0x80000000) sprite_scrolly[controller] |= 0xfffffe00;
					else sprite_scrolly[controller] &= 0x1ff;
					sprite_flip[controller] &= (~TOAPLAN2_SPRITE_FLIPY);
					break;
		case 0x0f:	break;
		case 0x80:  bg_scrollx[controller] = data - 0x229;			/* 169h */
					bg_flip[controller] |= TILEMAP_FLIPX;
					tilemap_set_flip(bg_tilemap[controller],bg_flip[controller]);
					tilemap_set_scrollx(bg_tilemap[controller],0,bg_scrollx[controller]);
					break;
		case 0x81:	bg_scrolly[controller] = data - 0x210;			/* 100h */
					bg_flip[controller] |= TILEMAP_FLIPY;
					tilemap_set_flip(bg_tilemap[controller],bg_flip[controller]);
					tilemap_set_scrolly(bg_tilemap[controller],0,bg_scrolly[controller]);
					break;
		case 0x82:	fg_scrollx[controller] = data - 0x227;			/* 15Fh */
					fg_flip[controller] |= TILEMAP_FLIPX;
					tilemap_set_flip(fg_tilemap[controller],fg_flip[controller]);
					tilemap_set_scrollx(fg_tilemap[controller],0,fg_scrollx[controller]);
					break;
		case 0x83:	fg_scrolly[controller] = data - 0x210;			/* 100h */
					fg_flip[controller] |= TILEMAP_FLIPY;
					tilemap_set_flip(fg_tilemap[controller],fg_flip[controller]);
					tilemap_set_scrolly(fg_tilemap[controller],0,fg_scrolly[controller]);
					break;
		case 0x84:	top_scrollx[controller] = data - 0x225;			/* 165h */
					top_flip[controller] |= TILEMAP_FLIPX;
					tilemap_set_flip(top_tilemap[controller],top_flip[controller]);
					tilemap_set_scrollx(top_tilemap[controller],0,top_scrollx[controller]);
					break;
		case 0x85:	top_scrolly[controller] = data - 0x210;			/* 100h */
					top_flip[controller] |= TILEMAP_FLIPY;
					tilemap_set_flip(top_tilemap[controller],top_flip[controller]);
					tilemap_set_scrolly(top_tilemap[controller],0,top_scrolly[controller]);
					break;
		case 0x86:	sprite_scrollx[controller] = data - 0x17b;		/* 17Bh */
					if (sprite_scrollx[controller] & 0x80000000) sprite_scrollx[controller] |= 0xfffffe00;
					else sprite_scrollx[controller] &= 0x1ff;
					sprite_flip[controller] |= TOAPLAN2_SPRITE_FLIPX;
					break;
		case 0x87:	sprite_scrolly[controller] = data - 0x108;		/* 108h */
					if (sprite_scrolly[controller] & 0x80000000) sprite_scrolly[controller] |= 0xfffffe00;
					else sprite_scrolly[controller] &= 0x1ff;
					sprite_flip[controller] |= TOAPLAN2_SPRITE_FLIPY;
					break;
		case 0x8f:	break;

		case 0x0e:	/******* Initialise video controller register ? *******/
					if ((toaplan2_sub_cpu == CPU_2_Z80) && (data == 3))
					{
						/* HACK! When tilted, sound CPU needs to be reset. */
						cpu_set_reset_line(1,PULSE_LINE);
						YM3812_sh_reset();
					}
		default:	//logerror("Hmmm, writing %08x to unknown video control register (%08x)  Video controller %01x  !!!\n",data ,toaplan2_scroll_reg[controller],controller);
					break;
	}

}
WRITE_HANDLER( toaplan2_0_scroll_reg_data_w )
{
	toaplan2_scroll_reg_data_w(offset, data, 0);
}
WRITE_HANDLER( toaplan2_1_scroll_reg_data_w )
{
	toaplan2_scroll_reg_data_w(offset, data, 1);
}

/*************** PIPIBIBI interface into this video driver *****************/

WRITE_HANDLER( pipibibi_scroll_w )
{
	int controller = 0;		/* only 1 video controller */

	offset /= 2;

	switch(offset)
	{
		case 0x00:	data -= 0x01f; break;
		case 0x01:	data += 0x1ef; break;
		case 0x02:	data -= 0x01d; break;
		case 0x03:	data += 0x1ef; break;
		case 0x04:	data -= 0x01b; break;
		case 0x05:	data += 0x1ef; break;
		case 0x06:	data += 0x1d4; break;
		case 0x07:	data += 0x1f7; break;
		default:	//logerror("PIPIBIBI writing %04x to unknown scroll register %04x",data, offset);
		    break;
	}

	toaplan2_scroll_reg[controller] = offset;
	toaplan2_scroll_reg_data_w(0, data, controller);
}

READ_HANDLER( pipibibi_videoram_r )
{
	int controller = 0;		/* only 1 video controller */

	offset /= 2;
	toaplan2_voffs_w(0, offset, controller);
	return toaplan2_videoram_r(0, controller);
}

WRITE_HANDLER( pipibibi_videoram_w)
{
	int controller = 0;		/* only 1 video controller */

	offset /= 2;
	toaplan2_voffs_w(0, offset, controller);
	toaplan2_videoram_w(0, data, controller);
}

READ_HANDLER( pipibibi_spriteram_r )
{
	int controller = 0;		/* only 1 video controller */

	offset /= 2;
    toaplan2_voffs_w(0, (0x1800 + offset), controller);
	return toaplan2_videoram_r(0, controller);
}

WRITE_HANDLER( pipibibi_spriteram_w )
{
	int controller = 0;		/* only 1 video controller */

	offset /= 2;
	toaplan2_voffs_w(0, (0x1800 + offset), controller);
	toaplan2_videoram_w(0, data, controller);
}



/***************************************************************************
	Sprite Handlers
***************************************************************************/

static void mark_sprite_colors(int controller)
{
	int offs, attrib, sprite, color, i, pal_base;
	int sprite_sizex, sprite_sizey, temp_x, temp_y;
	int colmask[64];

	UINT16 *source = (UINT16 *)(spriteram_now[controller]);

	pal_base = Machine->drv->gfxdecodeinfo[ ((controller*2)+1) ].color_codes_start;

	for(i=0; i < 64; i++) colmask[i] = 0;

	for (offs = 0; offs < (TOAPLAN2_SPRITERAM_SIZE/2); offs += 4)
	{
		attrib = source[offs];
		sprite = source[offs + 1] | ((attrib & 3) << 16);
		sprite %= Machine->gfx[ ((controller*2)+1) ]->total_elements;
		if (attrib & 0x8000)
		{
			/* While we're here, mark all priorities used */
			sprite_priority[controller][((attrib & 0x0f00) >> 8)] = display_sp[controller];

			color = (attrib >> 2) & 0x3f;
			sprite_sizex = (source[offs + 2] & 0x0f) + 1;
			sprite_sizey = (source[offs + 3] & 0x0f) + 1;

			for (temp_y = 0; temp_y < sprite_sizey; temp_y++)
			{
				for (temp_x = 0; temp_x < sprite_sizex; temp_x++)
				{
					colmask[color] |= Machine->gfx[ ((controller*2)+1) ]->pen_usage[sprite];
					sprite++ ;
				}
			}
		}
	}

	for (color = 0;color < 64;color++)
	{
		if ((color == 0) && (colmask[0] & 1))
			palette_used_colors[pal_base + 16 * color] = PALETTE_COLOR_TRANSPARENT;
		for (i = 1; i < 16; i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}
}



static void draw_sprites( struct osd_bitmap *bitmap, int controller, int priority_to_display )
{
	const struct GfxElement *gfx = Machine->gfx[ ((controller*2)+1) ];
	const struct rectangle *clip = &Machine->visible_area;

	UINT16 *source = (UINT16 *)(spriteram_now[controller]);

	int offs;
	for (offs = 0; offs < (TOAPLAN2_SPRITERAM_SIZE/2); offs += 4)
	{
		int attrib, sprite, color, priority, flipx, flipy, sx, sy;
		int sprite_sizex, sprite_sizey, temp_x, temp_y, sx_base, sy_base;

		attrib = source[offs];
		priority = (attrib & 0x0f00) >> 8;

		if ((priority == priority_to_display) && (attrib & 0x8000))
		{
			sprite = ((attrib & 3) << 16) | source[offs + 1] ;	/* 18 bit */
			color = (attrib >> 2) & 0x3f;

			/****** find out sprite size ******/
			sprite_sizex = ((source[offs + 2] & 0x0f) + 1) * 8;
			sprite_sizey = ((source[offs + 3] & 0x0f) + 1) * 8;

			/****** find position to display sprite ******/
			sx_base = (source[offs + 2] >> 7) - sprite_scrollx[controller];
			sy_base = (source[offs + 3] >> 7) - sprite_scrolly[controller];

			flipx = attrib & TOAPLAN2_SPRITE_FLIPX;
			flipy = attrib & TOAPLAN2_SPRITE_FLIPY;

			if (flipx)
			{
				sx_base -= 7;
				/****** wrap around sprite position ******/
				if (sx_base >= 0x1c0) sx_base -= 0x200;
			}
			else
			{
				if (sx_base >= 0x180) sx_base -= 0x200;
			}

			if (flipy)
			{
				sy_base -= 7;
				if (sy_base >= 0x1c0) sy_base -= 0x200;
			}
			else
			{
				if (sy_base >= 0x180) sy_base -= 0x200;
			}

			/****** flip the sprite layer in any active X or Y flip ******/
			if (sprite_flip[controller])
			{
				if (sprite_flip[controller] & TOAPLAN2_SPRITE_FLIPX)
					sx_base = 320 - sx_base;
				if (sprite_flip[controller] & TOAPLAN2_SPRITE_FLIPY)
					sy_base = 240 - sy_base;
			}

			/****** cancel flip, if it and sprite layer flip are active ******/
			flipx = (flipx ^ (sprite_flip[controller] & TOAPLAN2_SPRITE_FLIPX));
			flipy = (flipy ^ (sprite_flip[controller] & TOAPLAN2_SPRITE_FLIPY));

			for (temp_y = 0; temp_y < sprite_sizey; temp_y += 8)
			{
				if (flipy) sy = sy_base - temp_y;
				else       sy = sy_base + temp_y;
				for (temp_x = 0; temp_x < sprite_sizex; temp_x += 8)
				{
					if (flipx) sx = sx_base - temp_x;
					else       sx = sx_base + temp_x;

					drawgfx(bitmap,gfx,sprite,
						color,
						flipx,flipy,
						sx,sy,
						clip,TRANSPARENCY_PEN,0);

					sprite++ ;
				}
			}
		}
	}
}

/***************************************************************************
	Draw the game screen in the given osd_bitmap.
***************************************************************************/

void toaplan2_0_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int priority;

	for (priority = 0; priority < 16; priority++)
		sprite_priority[0][priority] = 0;		/* Clear priorities used list */

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();
	mark_sprite_colors(0);	/* Also mark priorities used */

	if (palette_recalc()) tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);

	for (priority = 0; priority < 16; priority++)
	{
		tilemap_draw(bitmap,bg_tilemap[0],priority);
		tilemap_draw(bitmap,fg_tilemap[0],priority);
		tilemap_draw(bitmap,top_tilemap[0],priority);
		if (sprite_priority[0][priority])
			draw_sprites(bitmap,0,priority);
	}
}
void toaplan2_1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int priority;

	for (priority = 0; priority < 16; priority++)
	{
		sprite_priority[0][priority] = 0;		/* Clear priorities used list */
		sprite_priority[1][priority] = 0;		/* Clear priorities used list */
	}

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();
	mark_sprite_colors(0);	/* Also mark priorities used */
	mark_sprite_colors(1);	/* Also mark priorities used */

	if (palette_recalc()) tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);

	for (priority = 0; priority < 16; priority++)
	{
		tilemap_draw(bitmap,bg_tilemap[1],priority);
		tilemap_draw(bitmap,fg_tilemap[1],priority);
		tilemap_draw(bitmap,top_tilemap[1],priority);
		if (sprite_priority[1][priority])
			draw_sprites(bitmap,1,priority);
	}
	for (priority = 0; priority < 16; priority++)
	{
		tilemap_draw(bitmap,bg_tilemap[0],priority);
		tilemap_draw(bitmap,fg_tilemap[0],priority);
		tilemap_draw(bitmap,top_tilemap[0],priority);
		if (sprite_priority[0][priority])
			draw_sprites(bitmap,0,priority);
	}
}
void batsugun_1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int priority;

	for (priority = 0; priority < 16; priority++)
	{
		sprite_priority[0][priority] = 0;		/* Clear priorities used list */
		sprite_priority[1][priority] = 0;		/* Clear priorities used list */
	}

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();
	mark_sprite_colors(0);	/* Also mark priorities used */
	mark_sprite_colors(1);	/* Also mark priorities used */

	if (palette_recalc()) tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);

	for (priority = 0; priority < 16; priority++)
	{
		tilemap_draw(bitmap,bg_tilemap[0],priority); /* 2 */
		tilemap_draw(bitmap,bg_tilemap[1],priority);
		tilemap_draw(bitmap,fg_tilemap[1],priority);
		tilemap_draw(bitmap,top_tilemap[1],priority);
		if (sprite_priority[1][priority])
			draw_sprites(bitmap,1,priority);
	}
	for (priority = 0; priority < 16; priority++)
	{
		tilemap_draw(bitmap,fg_tilemap[0],priority);
		tilemap_draw(bitmap,top_tilemap[0],priority);
		if (sprite_priority[0][priority])
			draw_sprites(bitmap,0,priority);
	}
}
/* Added by Yochizo 2000/08/19 */
void raizing_0_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	toaplan2_0_vh_screenrefresh(bitmap, full_refresh);
	tilemap_draw(bitmap,text_tilemap,0);
}
void raizing_1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	toaplan2_1_vh_screenrefresh(bitmap, full_refresh);
	tilemap_draw(bitmap,text_tilemap,0);
}


void toaplan2_0_eof_callback(void)
{
	/** Shift sprite RAM buffers  ***  Used to fix sprite lag **/
	memcpy(spriteram_now[0],spriteram_next[0],TOAPLAN2_SPRITERAM_SIZE);
	memcpy(spriteram_next[0],spriteram_new[0],TOAPLAN2_SPRITERAM_SIZE);
}
void toaplan2_1_eof_callback(void)
{
	/** Shift sprite RAM buffers  ***  Used to fix sprite lag **/
	memcpy(spriteram_now[0],spriteram_next[0],TOAPLAN2_SPRITERAM_SIZE);
	memcpy(spriteram_next[0],spriteram_new[0],TOAPLAN2_SPRITERAM_SIZE);
	memcpy(spriteram_now[1],spriteram_next[1],TOAPLAN2_SPRITERAM_SIZE);
	memcpy(spriteram_next[1],spriteram_new[1],TOAPLAN2_SPRITERAM_SIZE);
}
