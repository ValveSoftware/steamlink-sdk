/***************************************************************************

  vidhrdw/toaplan1.c

  Functions to emulate the video hardware of the machine.

  There are 4 scrolling layers of graphics, stored in planes of 64x64 tiles.
  Each tile in each plane is assigned a priority between 1 and 15, higher
  numbers have greater priority.

  Each tile takes up 32 bits, the format is:

  0         1         2         3
  ---- ---- ---- ---- -xxx xxxx xxxx xxxx = tile number (0 - $7fff)
  ---- ---- ---- ---- ?--- ---- ---- ---- = unknown / unused
  ---- ---- --xx xxxx ---- ---- ---- ---- = color (0 - $3f)
  ---- ???? ??-- ---- ---- ---- ---- ---- = unknown / unused
  xxxx ---- ---- ---- ---- ---- ---- ---- = priority (0-$f)

  BG Scroll Reg

  0         1         2         3
  xxxx xxxx x--- ---- ---- ---- ---- ---- = x
  ---- ---- ---- ---- yyyy yyyy ---- ---- = y


 Sprite RAM format  (except Rally Bike)

  0         1         2         3
  ---- ---- ---- ---- -xxx xxxx xxxx xxxx = tile number (0 - $7fff)
  ---- ---- ---- ---- x--- ---- ---- ---- = hidden
  ---- ---- --xx xxxx ---- ---- ---- ---- = color (0 - $3f)
  ---- xxxx xx-- ---- ---- ---- ---- ---- = sprite number
  xxxx ---- ---- ---- ---- ---- ---- ---- = priority (0-$f)

  4         5         6         7
  ---- ---- ---- ---- xxxx xxxx x--- ---- = x
  yyyy yyyy y--- ---- ---- ---- ---- ---- = y

  The tiles use a palette of 1024 colors, the sprites use a different palette
  of 1024 colors.

***************************************************************************/

#include "driver.h"
#include "palette.h"
#include "vidhrdw/generic.h"
#include "cpu/m68000/m68000.h"

#define VIDEORAM1_SIZE	0x1000		/* size in bytes - sprite ram */
#define VIDEORAM2_SIZE	0x100		/* size in bytes - sprite size ram */
#define VIDEORAM3_SIZE	0x4000		/* size in bytes - tile ram */

unsigned char *toaplan1_videoram1;
unsigned char *toaplan1_videoram2;
unsigned char *toaplan1_videoram3;
unsigned char *toaplan1_buffered_videoram1;
unsigned char *toaplan1_buffered_videoram2;

unsigned char *toaplan1_colorram1;
unsigned char *toaplan1_colorram2;

size_t colorram1_size;
size_t colorram2_size;

unsigned int scrollregs[8];
unsigned int vblank;
unsigned int num_tiles;

unsigned int video_ofs;
unsigned int video_ofs3;

int toaplan1_flipscreen;
int tiles_offsetx;
int tiles_offsety;
int layers_offset[4];


typedef struct
	{
	UINT16 tile_num;
	UINT16 color;
	char priority;
	int xpos;
	int ypos;
	} tile_struct;

tile_struct *bg_list[4];

tile_struct *tile_list[32];
tile_struct *temp_list;
static int max_list_size[32];
static int tile_count[32];

struct osd_bitmap *tmpbitmap1;
static struct osd_bitmap *tmpbitmap2;
static struct osd_bitmap *tmpbitmap3;


int rallybik_vh_start(void)
{
	int i;

	if ((toaplan1_videoram3 = (unsigned char*)calloc(VIDEORAM3_SIZE * 4, 1)) == 0) /* 4 layers */
	{
		return 1;
	}

	//logerror("colorram_size: %08x\n", colorram1_size + colorram2_size);
	if ((paletteram = (unsigned char*)calloc(colorram1_size + colorram2_size, 1)) == 0)
	{
		free(toaplan1_videoram3);
		return 1;
	}

	for (i=0; i<4; i++)
	{
		if ((bg_list[i]=(tile_struct *)malloc( 33 * 44 * sizeof(tile_struct))) == 0)
		{
			free(paletteram);
			free(toaplan1_videoram3);
			return 1;
		}
		memset(bg_list[i], 0, 33 * 44 * sizeof(tile_struct));
	}

	for (i=0; i<16; i++)
	{
		max_list_size[i] = 8192;
		if ((tile_list[i]=(tile_struct *)malloc(max_list_size[i]*sizeof(tile_struct))) == 0)
		{
			for (i=3; i>=0; i--)
				free(bg_list[i]);
			free(paletteram);
			free(toaplan1_videoram3);
			return 1;
		}
		memset(tile_list[i],0,max_list_size[i]*sizeof(tile_struct));
	}

	max_list_size[16] = 65536;
	if ((tile_list[16]=(tile_struct *)malloc(max_list_size[16]*sizeof(tile_struct))) == 0)
	{
		for (i=15; i>=0; i--)
			free(tile_list[i]);
		for (i=3; i>=0; i--)
			free(bg_list[i]);
		free(paletteram);
		free(toaplan1_videoram3);
		return 1;
	}
	memset(tile_list[16],0,max_list_size[16]*sizeof(tile_struct));

	num_tiles = (Machine->drv->screen_width/8+1)*(Machine->drv->screen_height/8) ;

	video_ofs = video_ofs3 = 0;

	return 0;
}

void rallybik_vh_stop(void)
{
	int i ;

	for (i=16; i>=0; i--)
	{
		free(tile_list[i]);
		//logerror("max_list_size[%d]=%08x\n",i,max_list_size[i]);
	}

	for (i=3; i>=0; i--)
		free(bg_list[i]);

	free(paletteram);
	free(toaplan1_videoram3);
}

int toaplan1_vh_start(void)
{
	tmpbitmap1 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height);
	tmpbitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height);
	tmpbitmap3 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height);


	if ((toaplan1_videoram1 = (unsigned char*)calloc(VIDEORAM1_SIZE, 1)) == 0)
	{
		return 1;
	}
	if ((toaplan1_buffered_videoram1 = (unsigned char*)calloc(VIDEORAM1_SIZE, 1)) == 0)
	{
		free(toaplan1_videoram1);
		return 1;
	}
	if ((toaplan1_videoram2 = (unsigned char*)calloc(VIDEORAM2_SIZE, 1)) == 0)
	{
		free(toaplan1_buffered_videoram1);
		free(toaplan1_videoram1);
		return 1;
	}
	if ((toaplan1_buffered_videoram2 = (unsigned char*)calloc(VIDEORAM2_SIZE, 1)) == 0)
	{
		free(toaplan1_videoram2);
		free(toaplan1_buffered_videoram1);
		free(toaplan1_videoram1);
		return 1;
	}

	/* Also include all allocated stuff in Rally Bike startup */
	return rallybik_vh_start();
}

void toaplan1_vh_stop(void)
{
	rallybik_vh_stop();

	free(toaplan1_buffered_videoram2);
	free(toaplan1_videoram2);
	free(toaplan1_buffered_videoram1);
	free(toaplan1_videoram1);
	bitmap_free(tmpbitmap3);
	bitmap_free(tmpbitmap2);
	bitmap_free(tmpbitmap1);
}




READ_HANDLER( toaplan1_vblank_r )
{
	return vblank ^= 1;
}

WRITE_HANDLER( toaplan1_flipscreen_w )
{
	toaplan1_flipscreen = data; /* 8000 flip, 0000 dont */
}

READ_HANDLER( video_ofs_r )
{
	return video_ofs ;
}

WRITE_HANDLER( video_ofs_w )
{
	video_ofs = data ;
}

/* tile palette */
READ_HANDLER( toaplan1_colorram1_r )
{
	return READ_WORD (&toaplan1_colorram1[offset]);
}

WRITE_HANDLER( toaplan1_colorram1_w )
{
	WRITE_WORD (&toaplan1_colorram1[offset], data);
	paletteram_xBBBBBGGGGGRRRRR_word_w(offset,data);
}

/* sprite palette */
READ_HANDLER( toaplan1_colorram2_r )
{
	return READ_WORD (&toaplan1_colorram2[offset]);
}

WRITE_HANDLER( toaplan1_colorram2_w )
{
	WRITE_WORD (&toaplan1_colorram2[offset], data);
	paletteram_xBBBBBGGGGGRRRRR_word_w(offset+colorram1_size,data);
}

READ_HANDLER( toaplan1_videoram1_r )
{
	return READ_WORD (&toaplan1_videoram1[2*(video_ofs & (VIDEORAM1_SIZE-1))]);
}

WRITE_HANDLER( toaplan1_videoram1_w )
{
	int oldword = READ_WORD (&toaplan1_videoram1[2*video_ofs & (VIDEORAM1_SIZE-1)]);
	int newword = COMBINE_WORD (oldword, data);

#ifdef DEBUG
	if (2*video_ofs >= VIDEORAM1_SIZE)
	{
		logerror("videoram1_w, %08x\n", 2*video_ofs);
		return;
	}
#endif

	WRITE_WORD (&toaplan1_videoram1[2*video_ofs & (VIDEORAM1_SIZE-1)],newword);
	video_ofs++;
}

READ_HANDLER( toaplan1_videoram2_r )
{
	return READ_WORD (&toaplan1_videoram2[2*video_ofs & (VIDEORAM2_SIZE-1)]);
}

WRITE_HANDLER( toaplan1_videoram2_w )
{
	int oldword = READ_WORD (&toaplan1_videoram2[2*video_ofs & (VIDEORAM2_SIZE-1)]);
	int newword = COMBINE_WORD (oldword, data);

#ifdef DEBUG
	if (2*video_ofs >= VIDEORAM2_SIZE)
	{
		logerror("videoram2_w, %08x\n", 2*video_ofs);
		return;
	}
#endif

	WRITE_WORD (&toaplan1_videoram2[2*video_ofs & (VIDEORAM2_SIZE-1)],newword);
	video_ofs++;
}

READ_HANDLER( video_ofs3_r )
{
	return video_ofs3;
}

WRITE_HANDLER( video_ofs3_w )
{
	video_ofs3 = data ;
}

READ_HANDLER( rallybik_videoram3_r )
{
	int rb_tmp_vid;

	rb_tmp_vid = READ_WORD (&toaplan1_videoram3[(video_ofs3 & (VIDEORAM3_SIZE-1))*4 + offset]);

	if (offset == 0)
	{
		rb_tmp_vid |= ((rb_tmp_vid & 0xf000) >> 4);
		rb_tmp_vid |= ((rb_tmp_vid & 0x0030) << 2);
	}
	return rb_tmp_vid;
}

READ_HANDLER( toaplan1_videoram3_r )
{
	return READ_WORD (&toaplan1_videoram3[(video_ofs3 & (VIDEORAM3_SIZE-1))*4 + offset]);
}

WRITE_HANDLER( toaplan1_videoram3_w )
{
	int oldword = READ_WORD (&toaplan1_videoram3[(video_ofs3 & (VIDEORAM3_SIZE-1))*4 + offset]);
	int newword = COMBINE_WORD (oldword, data);

#ifdef DEBUG
	if ((video_ofs3 & (VIDEORAM3_SIZE-1))*4 + offset >= VIDEORAM3_SIZE*4)
	{
		logerror("videoram3_w, %08x\n", 2*video_ofs3);
		return;
	}
#endif

	WRITE_WORD (&toaplan1_videoram3[(video_ofs3 & (VIDEORAM3_SIZE-1))*4 + offset],newword);
	if ( offset == 2 ) video_ofs3++;
}

READ_HANDLER( scrollregs_r )
{
	return scrollregs[offset>>1];
}

WRITE_HANDLER( scrollregs_w )
{
	scrollregs[offset>>1] = data ;
}

WRITE_HANDLER( offsetregs_w )
{
	if ( offset == 0 )
		tiles_offsetx = data ;
	else
		tiles_offsety = data ;
}

WRITE_HANDLER( layers_offset_w )
{
	switch (offset)
	{
/*		case 0:
			layers_offset[0] = (data&0xff) - 0xdb ;
			break ;
		case 2:
			layers_offset[1] = (data&0xff) - 0x14 ;
			break ;
		case 4:
			layers_offset[2] = (data&0xff) - 0x85 ;
			break ;
		case 6:
			layers_offset[3] = (data&0xff) - 0x07 ;
			break ;
*/
		case 0:
			layers_offset[0] = data;
			break ;
		case 2:
			layers_offset[1] = data;
			break ;
		case 4:
			layers_offset[2] = data;
			break ;
		case 6:
			layers_offset[3] = data;
			break ;
	}

	/*
	logerror("layers_offset[0]:%08x\n",layers_offset[0]);
	logerror("layers_offset[1]:%08x\n",layers_offset[1]);
	logerror("layers_offset[2]:%08x\n",layers_offset[2]);
	logerror("layers_offset[3]:%08x\n",layers_offset[3]);
	*/

}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.

***************************************************************************/


static void toaplan1_update_palette (void)
{
	int i;
	int priority;
	int color;
	unsigned short palette_map[64*2];

	memset (palette_map, 0, sizeof (palette_map));

	/* extract color info from priority layers in order */
	for (priority = 0; priority < 17; priority++ )
	{
		tile_struct *tinfo;

		tinfo = (tile_struct *)&(tile_list[priority][0]);
		/* draw only tiles in list */
		for ( i = 0; i < tile_count[priority]; i++ )
		{
			int bank;

			bank  = (tinfo->color >> 7) & 1;
			color = (tinfo->color & 0x3f);
			palette_map[color + bank*64] |= Machine->gfx[bank]->pen_usage[tinfo->tile_num & (Machine->gfx[bank]->total_elements-1)];

			tinfo++;
		}
	}

	/* Tell MAME about the color usage */
	for (i = 0; i < 64*2; i++)
	{
		int usage = palette_map[i];
		int j;

		if (usage)
		{
			palette_used_colors[i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (j = 0; j < 16; j++)
			{
				if (usage & (1 << j))
					palette_used_colors[i * 16 + j] = PALETTE_COLOR_USED;
				else
					palette_used_colors[i * 16 + j] = PALETTE_COLOR_UNUSED;
			}
		}
		else
			memset(&palette_used_colors[i * 16],PALETTE_COLOR_UNUSED,16);
	}

	palette_recalc ();
}



static int 	layer_scrollx[4];
static int 	layer_scrolly[4];

static void toaplan1_find_tiles(int xoffs,int yoffs)
{
	int priority;
	int layer;
	tile_struct *tinfo;
	unsigned char *t_info;

	if(toaplan1_flipscreen){
		layer_scrollx[0] = ((scrollregs[0]) >> 7) + (523 - xoffs);
		layer_scrollx[1] = ((scrollregs[2]) >> 7) + (525 - xoffs);
		layer_scrollx[2] = ((scrollregs[4]) >> 7) + (527 - xoffs);
		layer_scrollx[3] = ((scrollregs[6]) >> 7) + (529 - xoffs);

		layer_scrolly[0] = ((scrollregs[1]) >> 7) +	(256 - yoffs);
		layer_scrolly[1] = ((scrollregs[3]) >> 7) + (256 - yoffs);
		layer_scrolly[2] = ((scrollregs[5]) >> 7) + (256 - yoffs);
		layer_scrolly[3] = ((scrollregs[7]) >> 7) + (256 - yoffs);
	}else{
		layer_scrollx[0] = ((scrollregs[0]) >> 7) +(495 - xoffs + 6);
		layer_scrollx[1] = ((scrollregs[2]) >> 7) +(495 - xoffs + 4);
		layer_scrollx[2] = ((scrollregs[4]) >> 7) +(495 - xoffs + 2);
		layer_scrollx[3] = ((scrollregs[6]) >> 7) +(495 - xoffs);

		layer_scrolly[0] = ((scrollregs[1]) >> 7) +	(0x101 - yoffs);
		layer_scrolly[1] = ((scrollregs[3]) >> 7) + (0x101 - yoffs);
		layer_scrolly[2] = ((scrollregs[5]) >> 7) + (0x101 - yoffs);
		layer_scrolly[3] = ((scrollregs[7]) >> 7) + (0x101 - yoffs);
	}

	for ( layer = 3 ; layer >= 0 ; layer-- )
	{
		int scrolly,scrollx,offsetx,offsety;
		int sx,sy,tattr;
		int i;

		t_info = (toaplan1_videoram3+layer * VIDEORAM3_SIZE);
		scrollx = layer_scrollx[layer];
		offsetx = scrollx / 8 ;
		scrolly = layer_scrolly[layer];
		offsety = scrolly / 8 ;

		for ( sy = 0 ; sy < 32 ; sy++ )
		{
			for ( sx = 0 ; sx <= 40 ; sx++ )
			{
				i = ((sy+offsety)&0x3f)*256 + ((sx+offsetx)&0x3f)*4 ;
				tattr = READ_WORD(&t_info[i]);
				priority = (tattr >> 12);

				tinfo = (tile_struct *)&(bg_list[layer][sy*41+sx]) ;
				tinfo->tile_num = READ_WORD(&t_info[i+2]) ;
				tinfo->priority = priority ;
				tinfo->color = tattr & 0x3f ;
				tinfo->xpos = (sx*8)-(scrollx&0x7) ;
				tinfo->ypos = (sy*8)-(scrolly&0x7) ;

				if ( (priority) || (layer == 0) )	/* if priority 0 draw layer 0 only */
				{

					tinfo = (tile_struct *)&(tile_list[priority][tile_count[priority]]) ;
					tinfo->tile_num = READ_WORD(&t_info[i+2]) ;
					if ( (tinfo->tile_num & 0x8000) == 0 )
					{
						tinfo->priority = priority ;
						tinfo->color = tattr & 0x3f ;
						tinfo->color |= layer<<8;
						tinfo->xpos = (sx*8)-(scrollx&0x7) ;
						tinfo->ypos = (sy*8)-(scrolly&0x7) ;
						tile_count[priority]++ ;
						if(tile_count[priority]==max_list_size[priority])
						{
							/*reallocate tile_list[priority] to larger size */
							temp_list=(tile_struct *)malloc(sizeof(tile_struct)*(max_list_size[priority]+512)) ;
							memcpy(temp_list,tile_list[priority],sizeof(tile_struct)*max_list_size[priority]);
							max_list_size[priority]+=512;
							free(tile_list[priority]);
							tile_list[priority] = temp_list ;
						}
					}
				}
			}
		}
	}
	for ( layer = 3 ; layer >= 0 ; layer-- )
	{
		layer_scrollx[layer] &= 0x7;
		layer_scrolly[layer] &= 0x7;
	}
}

static void rallybik_find_tiles(void)
{
	int priority;
	int layer;
	tile_struct *tinfo;
	unsigned char *t_info;

	for ( priority = 0 ; priority < 16 ; priority++ )
	{
		tile_count[priority]=0;
	}

	for ( layer = 3 ; layer >= 0 ; layer-- )
	{
		int scrolly,scrollx,offsetx,offsety;
		int sx,sy,tattr;
		int i;

		t_info = (toaplan1_videoram3+layer * VIDEORAM3_SIZE);
		scrollx = scrollregs[layer*2];
		scrolly = scrollregs[(layer*2)+1];

		scrollx >>= 7 ;
		scrollx += 43 ;
		if ( layer == 0 ) scrollx += 2 ;
		if ( layer == 2 ) scrollx -= 2 ;
		if ( layer == 3 ) scrollx -= 4 ;
		offsetx = scrollx / 8 ;

		scrolly >>= 7 ;
		scrolly += 21 ;
		offsety = scrolly / 8 ;

		for ( sy = 0 ; sy < 32 ; sy++ )
		{
			for ( sx = 0 ; sx <= 40 ; sx++ )
			{
				i = ((sy+offsety)&0x3f)*256 + ((sx+offsetx)&0x3f)*4 ;
				tattr = READ_WORD(&t_info[i]);
				priority = tattr >> 12 ;
				if ( (priority) || (layer == 0) )	/* if priority 0 draw layer 0 only */
				{
					tinfo = (tile_struct *)&(tile_list[priority][tile_count[priority]]) ;
					tinfo->tile_num = READ_WORD(&t_info[i+2]) ;

					if ( !((priority) && (tinfo->tile_num & 0x8000)) )
					{
						tinfo->tile_num &= 0x3fff ;
						tinfo->color = tattr & 0x3f ;
						tinfo->xpos = (sx*8)-(scrollx&0x7) ;
						tinfo->ypos = (sy*8)-(scrolly&0x7) ;
						tile_count[priority]++ ;
						if(tile_count[priority]==max_list_size[priority])
						{
							/*reallocate tile_list[priority] to larger size */
							temp_list=(tile_struct *)malloc(sizeof(tile_struct)*(max_list_size[priority]+512)) ;
							memcpy(temp_list,tile_list[priority],sizeof(tile_struct)*max_list_size[priority]);
							max_list_size[priority]+=512;
							free(tile_list[priority]);
							tile_list[priority] = temp_list ;
						}
					}
				}
			}
		}
	}
}

unsigned long toaplan_sp_ram_dump = 0;

static void toaplan1_find_sprites (void)
{
	int priority;
	int sprite;
	unsigned char *s_info,*s_size;


	for ( priority = 0 ; priority < 17 ; priority++ )
	{
		tile_count[priority]=0;
	}


#if 0		// sp ram dump start
	s_size = (toaplan1_buffered_videoram2);		/* sprite block size */
	s_info = (toaplan1_buffered_videoram1);		/* start of sprite ram */
	if( (toaplan_sp_ram_dump == 0)
	 && (keyboard_pressed(KEYCODE_N)) )
	{
		toaplan_sp_ram_dump = 1;
		for ( sprite = 0 ; sprite < 256 ; sprite++ )
		{
			int tattr,tchar;
			tchar = READ_WORD (&s_info[0]) & 0xffff;
			tattr = READ_WORD (&s_info[2]);
			logerror("%08x: %04x %04x\n", sprite, tattr, tchar);
			s_info += 8 ;
		}
	}
#endif		// end

	s_size = (toaplan1_buffered_videoram2);		/* sprite block size */
	s_info = (toaplan1_buffered_videoram1);		/* start of sprite ram */

	for ( sprite = 0 ; sprite < 256 ; sprite++ )
	{
		int tattr,tchar;

		tchar = READ_WORD (&s_info[0]) & 0xffff;
		tattr = READ_WORD (&s_info[2]);

		if ( (tattr & 0xf000) && ((tchar & 0x8000) == 0) )
		{
			int sx,sy,dx,dy,s_sizex,s_sizey;
			int sprite_size_ptr;

			sx=READ_WORD(&s_info[4]);
			sx >>= 7 ;
			if ( sx > 416 ) sx -= 512 ;

			sy=READ_WORD(&s_info[6]);
			sy >>= 7 ;
			if ( sy > 416 ) sy -= 512 ;

			priority = (tattr >> 12);

			sprite_size_ptr = (tattr>>6)&0x3f ;
			s_sizey = (READ_WORD(&s_size[2*sprite_size_ptr])>>4)&0xf ;
			s_sizex = (READ_WORD(&s_size[2*sprite_size_ptr]))&0xf ;

			for ( dy = s_sizey ; dy > 0 ; dy-- )
			for ( dx = s_sizex; dx > 0 ; dx-- )
			{
				tile_struct *tinfo;

				tinfo = (tile_struct *)&(tile_list[16][tile_count[16]]) ;
				tinfo->priority = priority;
				tinfo->tile_num = tchar;
				tinfo->color = 0x80 | (tattr & 0x3f) ;
				tinfo->xpos = sx-dx*8+s_sizex*8 ;
				tinfo->ypos = sy-dy*8+s_sizey*8 ;
				tile_count[16]++ ;
				if(tile_count[16]==max_list_size[16])
				{
					/*reallocate tile_list[priority] to larger size */
					temp_list=(tile_struct *)malloc(sizeof(tile_struct)*(max_list_size[16]+512)) ;
					memcpy(temp_list,tile_list[16],sizeof(tile_struct)*max_list_size[16]);
					max_list_size[16]+=512;
					free(tile_list[16]);
					tile_list[16] = temp_list ;
				}
				tchar++;
			}
		}
		s_info += 8 ;
	}
}

static void rallybik_find_sprites (void)
{
	int offs;
	int tattr;
	int sx,sy,tchar;
	int priority;
	tile_struct *tinfo;

	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		tattr = READ_WORD(&buffered_spriteram[offs+2]);
		if ( tattr )	/* no need to render hidden sprites */
		{
			sx=READ_WORD(&buffered_spriteram[offs+4]);
			sx >>= 7 ;
			sx &= 0x1ff ;
			if ( sx > 416 ) sx -= 512 ;

			sy=READ_WORD(&buffered_spriteram[offs+6]);
			sy >>= 7 ;
			sy &= 0x1ff ;
			if ( sy > 416 ) sy -= 512 ;

			priority = (tattr>>8) & 0xc ;
			tchar = READ_WORD(&buffered_spriteram[offs+0]);
			tinfo = (tile_struct *)&(tile_list[priority][tile_count[priority]]) ;
			tinfo->tile_num = tchar & 0x7ff ;
			tinfo->color = 0x80 | (tattr&0x3f) ;
			tinfo->color |= (tattr & 0x0100) ;
			tinfo->color |= (tattr & 0x0200) ;
			if (tinfo->color & 0x0100) sx -= 15;

			tinfo->xpos = sx-31 ;
			tinfo->ypos = sy-16 ;
			tile_count[priority]++ ;
			if(tile_count[priority]==max_list_size[priority])
			{
				/*reallocate tile_list[priority] to larger size */
				temp_list=(tile_struct *)malloc(sizeof(tile_struct)*(max_list_size[priority]+512)) ;
				memcpy(temp_list,tile_list[priority],sizeof(tile_struct)*max_list_size[priority]);
				max_list_size[priority]+=512;
				free(tile_list[priority]);
				tile_list[priority] = temp_list ;
			}
		}  // if tattr
	} // for sprite
}


void toaplan1_fillbgmask(struct osd_bitmap *dest_bmp, struct osd_bitmap *source_bmp,
 const struct rectangle *clip,int transparent_color)
{
	struct rectangle myclip;
	int sx=0;
	int sy=0;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		/* clip and myclip might be the same, so we need a temporary storage */
		temp = clip->min_x;
		myclip.min_x = clip->min_y;
		myclip.min_y = temp;
		temp = clip->max_x;
		myclip.max_x = clip->max_y;
		myclip.max_y = temp;
		clip = &myclip;
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		int temp;

		sx = -sx;

		/* clip and myclip might be the same, so we need a temporary storage */
		temp = clip->min_x;
		myclip.min_x = dest_bmp->width-1 - clip->max_x;
		myclip.max_x = dest_bmp->width-1 - temp;
		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;
		clip = &myclip;
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		int temp;

		sy = -sy;

		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;
		/* clip and myclip might be the same, so we need a temporary storage */
		temp = clip->min_y;
		myclip.min_y = dest_bmp->height-1 - clip->max_y;
		myclip.max_y = dest_bmp->height-1 - temp;
		clip = &myclip;
	}

	if (dest_bmp->depth != 16)
	{
		int ex = sx+source_bmp->width;
		int ey = sy+source_bmp->height;

		if( sx < clip->min_x)
		{ /* clip left */
			sx = clip->min_x;
		}
		if( sy < clip->min_y )
		{ /* clip top */
			sy = clip->min_y;
		}
		if( ex > clip->max_x+1 )
		{ /* clip right */
			ex = clip->max_x + 1;
		}
		if( ey > clip->max_y+1 )
		{ /* clip bottom */
			ey = clip->max_y + 1;
		}

		if( ex>sx )
		{ /* skip if inner loop doesn't draw anything */
			int y;
			for( y=sy; y<ey; y++ )
			{
				unsigned char *dest = dest_bmp->line[y];
				unsigned char *source = source_bmp->line[y];
				int x;

				for( x=sx; x<ex; x++ )
				{
					int c = source[x];
					if( c != transparent_color )
						dest[x] = transparent_color;
				}
			}
		}
	}

	else
	{
		int ex = sx+source_bmp->width;
		int ey = sy+source_bmp->height;

		if( sx < clip->min_x)
		{ /* clip left */
			sx = clip->min_x;
		}
		if( sy < clip->min_y )
		{ /* clip top */
			sy = clip->min_y;
		}
		if( ex > clip->max_x+1 )
		{ /* clip right */
			ex = clip->max_x + 1;
		}
		if( ey > clip->max_y+1 )
		{ /* clip bottom */
			ey = clip->max_y + 1;
		}

		if( ex>sx )
		{ /* skip if inner loop doesn't draw anything */
			int y;

			for( y=sy; y<ey; y++ )
			{
				unsigned short *dest = (unsigned short *)dest_bmp->line[y];
				unsigned char *source = source_bmp->line[y];
				int x;

				for( x=sx; x<ex; x++ )
				{
					int c = source[x];
					if( c != transparent_color )
						dest[x] = transparent_color;
				}
			}
		}
	}
}


//#define BGDBG
#ifdef BGDBG
int	toaplan_dbg_priority = 0;
#endif


static void toaplan1_render (struct osd_bitmap *bitmap)
{
	int i,j;
	int priority,pen;
	int	flip;
	tile_struct *tinfo;
	tile_struct *tinfo2;
	struct rectangle sp_rect;

	fillbitmap (bitmap, palette_transparent_pen, &Machine->visible_area);

#ifdef BGDBG

if (keyboard_pressed(KEYCODE_Q)) { toaplan_dbg_priority = 0; }
if (keyboard_pressed(KEYCODE_W)) { toaplan_dbg_priority = 1; }
if (keyboard_pressed(KEYCODE_E)) { toaplan_dbg_priority = 2; }
if (keyboard_pressed(KEYCODE_R)) { toaplan_dbg_priority = 3; }
if (keyboard_pressed(KEYCODE_T)) { toaplan_dbg_priority = 4; }
if (keyboard_pressed(KEYCODE_Y)) { toaplan_dbg_priority = 5; }
if (keyboard_pressed(KEYCODE_U)) { toaplan_dbg_priority = 6; }
if (keyboard_pressed(KEYCODE_I)) { toaplan_dbg_priority = 7; }
if (keyboard_pressed(KEYCODE_A)) { toaplan_dbg_priority = 8; }
if (keyboard_pressed(KEYCODE_S)) { toaplan_dbg_priority = 9; }
if (keyboard_pressed(KEYCODE_D)) { toaplan_dbg_priority = 10; }
if (keyboard_pressed(KEYCODE_F)) { toaplan_dbg_priority = 11; }
if (keyboard_pressed(KEYCODE_G)) { toaplan_dbg_priority = 12; }
if (keyboard_pressed(KEYCODE_H)) { toaplan_dbg_priority = 13; }
if (keyboard_pressed(KEYCODE_J)) { toaplan_dbg_priority = 14; }
if (keyboard_pressed(KEYCODE_K)) { toaplan_dbg_priority = 15; }

if( toaplan_dbg_priority != 0 ){

	priority = toaplan_dbg_priority;
	{
		tinfo = (tile_struct *)&(tile_list[priority][0]) ;
		/* hack to fix black blobs in Demon's World sky */
		pen = TRANSPARENCY_NONE ;
		for ( i = 0 ; i < tile_count[priority] ; i++ ) /* draw only tiles in list */
		{
			/* hack to fix blue blobs in Zero Wing attract mode */
//			if ((pen == TRANSPARENCY_NONE) && ((tinfo->color&0x3f)==0))
//				pen = TRANSPARENCY_PEN ;
			drawgfx(bitmap,Machine->gfx[0],
				tinfo->tile_num,
				(tinfo->color&0x3f),
				0,0,						/* flipx,flipy */
				tinfo->xpos,tinfo->ypos,
				&Machine->visible_area,pen,0);
			tinfo++ ;
		}
	}

}else{

#endif

//	if(toaplan1_flipscreen)
//		flip = 1;
//	else
		flip = 0;
//
	priority = 0;
	while ( priority < 16 )			/* draw priority layers in order */
	{
		int	layer;

		tinfo = (tile_struct *)&(tile_list[priority][0]) ;
		layer = (tinfo->color >> 8);
		if ( (layer < 3) && (priority < 2))
			pen = TRANSPARENCY_NONE ;
		else
			pen = TRANSPARENCY_PEN ;

		for ( i = 0 ; i < tile_count[priority] ; i++ ) /* draw only tiles in list */
		{
			int	xpos,ypos;

			/* hack to fix blue blobs in Zero Wing attract mode */
//			if ((pen == TRANSPARENCY_NONE) && ((tinfo->color&0x3f)==0))
//				pen = TRANSPARENCY_PEN ;

//			if ( flip ){
//				xpos = tinfo->xpos;
//				ypos = tinfo->ypos;
//			}
//			else{
				xpos = tinfo->xpos;
				ypos = tinfo->ypos;
//			}

			drawgfx(bitmap,Machine->gfx[0],
				tinfo->tile_num,
				(tinfo->color&0x3f),
				flip,flip,							/* flipx,flipy */
				tinfo->xpos,tinfo->ypos,
				&Machine->visible_area,pen,0);
			tinfo++ ;
		}
		priority++;
	}

	tinfo2 = (tile_struct *)&(tile_list[16][0]) ;
	for ( i = 0; i < tile_count[16]; i++ )	/* draw sprite No. in order */
	{
		int	flipx,flipy;

		sp_rect.min_x = tinfo2->xpos;
		sp_rect.min_y = tinfo2->ypos;
		sp_rect.max_x = tinfo2->xpos + 7;
		sp_rect.max_y = tinfo2->ypos + 7;

		fillbitmap (tmpbitmap2, palette_transparent_pen, &sp_rect);

		flipx = (tinfo2->color & 0x0100);
		flipy = (tinfo2->color & 0x0200);
//		if(toaplan1_flipscreen){
//			flipx = !flipx;
//			flipy = !flipy;
//		}
		drawgfx(tmpbitmap2,Machine->gfx[1],
			tinfo2->tile_num,
			(tinfo2->color&0x3f), 			/* bit 7 not for colour */
			flipx,flipy,					/* flipx,flipy */
			tinfo2->xpos,tinfo2->ypos,
			&Machine->visible_area,TRANSPARENCY_PEN,0);

		priority = tinfo2->priority;
		{
		int ix0,ix1,ix2,ix3;
		int dirty;

		dirty = 0;
		fillbitmap (tmpbitmap3, palette_transparent_pen, &sp_rect);
		for ( j = 0 ; j < 4 ; j++ )
		{
			int x,y;

			y = tinfo2->ypos+layer_scrolly[j];
			x = tinfo2->xpos+layer_scrollx[j];
			ix0 = ( y   /8) * 41 +  x   /8;
			ix1 = ( y   /8) * 41 + (x+7)/8;
			ix2 = ((y+7)/8) * 41 +  x   /8;
			ix3 = ((y+7)/8) * 41 + (x+7)/8;

			if(	(ix0 >= 0) && (ix0 < 32*41) ){
				tinfo = (tile_struct *)&(bg_list[j][ix0]) ;
				if( (tinfo->priority >= tinfo2->priority) ){
					drawgfx(tmpbitmap3,Machine->gfx[0],
//					drawgfx(tmpbitmap2,Machine->gfx[0],
						tinfo->tile_num,
						(tinfo->color&0x3f),
						flip,flip,
						tinfo->xpos,tinfo->ypos,
						&sp_rect,TRANSPARENCY_PEN,0);
					dirty=1;
				}
			}
			if(	(ix1 >= 0) && (ix1 < 32*41) ){
				tinfo = (tile_struct *)&(bg_list[j][ix1]) ;
//				tinfo++;
				if( (ix0 != ix1)
				 && (tinfo->priority >= tinfo2->priority) ){
					drawgfx(tmpbitmap3,Machine->gfx[0],
//					drawgfx(tmpbitmap2,Machine->gfx[0],
						tinfo->tile_num,
						(tinfo->color&0x3f),
						flip,flip,
						tinfo->xpos,tinfo->ypos,
						&sp_rect,TRANSPARENCY_PEN,0);
					dirty=1;
				}
			}
			if(	(ix2 >= 0) && (ix2 < 32*41) ){
				tinfo = (tile_struct *)&(bg_list[j][ix2]) ;
//				tinfo += 40;
				if( (ix0 != ix2)
				 && (tinfo->priority >= tinfo2->priority) ){
					drawgfx(tmpbitmap3,Machine->gfx[0],
//					drawgfx(tmpbitmap2,Machine->gfx[0],
						tinfo->tile_num,
						(tinfo->color&0x3f),
						flip,flip,
						tinfo->xpos,tinfo->ypos,
						&sp_rect,TRANSPARENCY_PEN,0);
					dirty=1;
				}
			}
			if(	(ix3 >= 0) && (ix3 < 32*41) ){
				tinfo = (tile_struct *)&(bg_list[j][ix3]) ;
//				tinfo++;
				if( (ix0 != ix3) && (ix1 != ix3) && (ix2 != ix3)
				 && (tinfo->priority >= tinfo2->priority) ){
					drawgfx(tmpbitmap3,Machine->gfx[0],
//					drawgfx(tmpbitmap2,Machine->gfx[0],
						tinfo->tile_num,
						(tinfo->color&0x3f),
						flip,flip,
						tinfo->xpos,tinfo->ypos,
						&sp_rect,TRANSPARENCY_PEN,0);
					dirty=1;
				}
			}
		}
		if(	dirty != 0 )
		{
			toaplan1_fillbgmask(
				tmpbitmap2,				// dist
				tmpbitmap3,				// mask
				&sp_rect,
				palette_transparent_pen
			);
		}
		copybitmap(bitmap, tmpbitmap2, 0, 0, 0, 0, &sp_rect, TRANSPARENCY_PEN, palette_transparent_pen);
		tinfo2++;
		}
	}

#ifdef BGDBG
}
#endif

}


static void rallybik_render (struct osd_bitmap *bitmap)
{
	int i;
	int priority,pen;
	tile_struct *tinfo;

	fillbitmap (bitmap, palette_transparent_pen, &Machine->visible_area);

	for ( priority = 0 ; priority < 16 ; priority++ )	/* draw priority layers in order */
	{
		tinfo = (tile_struct *)&(tile_list[priority][0]) ;
		/* hack to fix black blobs in Demon's World sky */
		if ( priority == 1 )
			pen = TRANSPARENCY_NONE ;
		else
			pen = TRANSPARENCY_PEN ;
		for ( i = 0 ; i < tile_count[priority] ; i++ ) /* draw only tiles in list */
		{
			/* hack to fix blue blobs in Zero Wing attract mode */
			if ((pen == TRANSPARENCY_NONE) && ((tinfo->color&0x3f)==0))
				pen = TRANSPARENCY_PEN ;

			drawgfx(bitmap,Machine->gfx[(tinfo->color>>7)&1],	/* bit 7 set for sprites */
				tinfo->tile_num,
				(tinfo->color&0x3f), 			/* bit 7 not for colour */
				(tinfo->color & 0x0100),(tinfo->color & 0x0200),	/* flipx,flipy */
				tinfo->xpos,tinfo->ypos,
				&Machine->visible_area,pen,0);
			tinfo++ ;
		}
	}
}


void toaplan1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* discover what data will be drawn */
	toaplan1_find_sprites();
	toaplan1_find_tiles(tiles_offsetx,tiles_offsety);

	toaplan1_update_palette();
	toaplan1_render(bitmap);
}

void rallybik_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* discover what data will be drawn */
	rallybik_find_tiles();
	rallybik_find_sprites();

	toaplan1_update_palette();
	rallybik_render(bitmap);
}



/****************************************************************************
	Spriteram is always 1 frame ahead, suggesting spriteram buffering.
	There are no CPU output registers that control this so we
	assume it happens automatically every frame, at the end of vblank
****************************************************************************/

void toaplan1_eof_callback(void)
{
	memcpy(toaplan1_buffered_videoram1,toaplan1_videoram1,VIDEORAM1_SIZE);
	memcpy(toaplan1_buffered_videoram2,toaplan1_videoram2,VIDEORAM2_SIZE);
}

void rallybik_eof_callback(void)
{
	buffer_spriteram_w(0,0);
}

void samesame_eof_callback(void)
{
	memcpy(toaplan1_buffered_videoram1,toaplan1_videoram1,VIDEORAM1_SIZE);
	memcpy(toaplan1_buffered_videoram2,toaplan1_videoram2,VIDEORAM2_SIZE);
	cpu_set_irq_line(0, MC68000_IRQ_2, HOLD_LINE);  /* Frame done */
}

