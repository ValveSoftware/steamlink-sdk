#ifndef GFX_OBJECT_MANAGER
#define GFX_OBJECT_MANAGER
/*
	gfx object manager
*/
/* dirty flag */
#define GFXOBJ_DIRTY_ALL      0xff
#define GFXOBJ_DIRTY_SX_SY    0xff
#define GFXOBJ_DIRTY_SIZE     0xff
#define GFXOBJ_DIRTY_PRIORITY 0xff
#define GFXOBJ_DIRTY_CODE     0xff
#define GFXOBJ_DIRTY_COLOR    0xff
#define GFXOBJ_DIRTY_FLIP     0xff

/* sort(priority) flag */
#define GFXOBJ_DONT_SORT          0x00
#define GFXOBJ_DO_SORT            0x01
#define GFXOBJ_SORT_OBJECT_BACK   0x02
#define GFXOBJ_SORT_PRIORITY_BACK 0x04

#define GFXOBJ_SORT_DEFAULT GFXOBJ_DO_SORT

/* one of object */
struct gfx_object {
	int		transparency;		/* transparency of gfx */
	int		transparet_color;	/* transparet color of gfx */
	struct	GfxElement *gfx;	/* source gfx , if gfx==0 then not calcrate sx,sy,visible,clip */
	int		code;				/* code of gfx */
	int		color;				/* color of gfx */
	int		priority;			/* priority 0=lower */
	int		sx;					/* x position */
	int		sy;					/* y position */
	int		flipx;				/* x flip */
	int		flipy;				/* y flip */
	/* source window in gfx tile : only non zooming gfx */
	/* if use zooming gfx , top,left should be set 0, */
	/* and width,height should be set same as gfx element */
	int		top;					/* x offset of source data */
	int		left;					/* y offset of source data */
	int		width;				/* x size */
	int		height;				/* y size */
	int		palette_flag;		/* !! not supported !! , palette usage flag tracking */
	/* zooming */
	int scalex;					/* zommscale , if 0 then non zooming gfx */
	int scaley;					/* */
	/* link */
	struct gfx_object *next;	/* next object point */
	/* exrernal draw handler , (for tilemap,special sprite,etc) */
	void	(*special_handler)(struct osd_bitmap *bitmap,struct gfx_object *object);
								/* !!! not suppored yet !!! */
	int		dirty_flag;			/* dirty flag */
	/* !! only drawing routine !! */
	int		visible;		/* visible flag        */
	int		draw_x;			/* x adjusted position */
	int		draw_y;			/* y adjusted position */
	struct rectangle clip; /* clipping object size with visible area */
} __attribute__ ((__aligned__ (32)));

/* object list */
struct gfx_object_list {
	int nums;						/* read only */
	int max_priority;				/* read only */
	struct gfx_object *objects;
									/* priority : objects[0]=lower       */
									/*          : objects[nums-1]=higher */
	struct gfx_object *first_object; /* pointer of first(lower) link object */
	/* !! private area !! */
	int sort_type;					/* priority order type */
	struct gfx_object_list *next;	/* resource tracking */
};

void gfxobj_init(void);		/* called by core - don't call this in drivers */
void gfxobj_close(void);	/* called by core - don't call this in drivers */

void gfxobj_mark_all_pixels_dirty(struct gfx_object_list *object_list);
struct gfx_object_list *gfxobj_create(int nums,int max_priority,const struct gfx_object *def_object);
void gfxobj_update(void);
void gfxobj_draw(struct gfx_object_list *object_list);

#endif
