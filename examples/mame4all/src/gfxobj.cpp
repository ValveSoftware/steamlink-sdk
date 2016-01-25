/*
	gfx object manager

	Games currently using the Sprite Manager:
		Namco System1

	Graphic Object Manager controls all Graphic object and the priority.

	The thing to draw in tile of gfx element by an arbitrary size can be done.
	That is,  it is useful for the sprite system which shares a different size.

	The drawing routine can be replaced with callbacking user routine.
	The control of priority becomes easy if tilemap draws by using this function.
	If gfx like special drawing routine is made,complexity overlapping sprite can be include.

	Currently supported features include:
	- The position and the size can be arbitrarily specified from gfx tile.
	  ( It is achieved by using clipping.)
	-  priority ordering.
	- resource tracking (gfxobj_init and gfxobj_close must be called by mame.c)
	- offscreen object skipping

	Schedule for the future
	- zoomming gfx with selectable tile position and size
	- palette usage tracking
	- uniting with Sprite Manager
	- priority link system in object list.

	Hint:

	-The position and the size select :

	This is similar to SPRITE_TYPE_UNPACK of Sprite Manager.
	(see also comments in sprite.c.)

	objects->top
	objects->left    -> The display beginning position in gfx tile is specified.
	objects->width
	objects->height  -> The display size is specified.

	Gfx object manager adjust the draw position from the value of top and left.
	And,the clipping position is adjusted according to the size.

	-The palette resource tracking :

	sprite manager it not supported yet.
	User should control palette resource for yourself.
	If you use sort mode,the object not seen has exclusion from the object link.
	Therefore,  examining objectlink will decrease the overhead.

	example:

	gfxobj_update();
	if(object=objectlist->first_object ; object != 0 ; object=object->next)
	{
		color = object->color;
		mark_palette_used_flag(color);
	}

	-Priority order

	Gfx object manager can sort the object by priority.
	The condition of sorting is decided by the flag.

	gfxobject_list->sort_type;

	GFXOBJ_DONT_SORT:

	Sorting is not done.
	The user should be build object link list.
	excluded objects from link are not updated.

	gfxobject_list->first_object; -> first object point (lower object)
	gfxobject->next               -> next object point ,0 = end of link(higher)

	GFXOBJ_DOT_SORT:

	Sorting is done.

	The condition by object->priority is given to priority most.
		object->priority = p    -> p=0 : lower , p=object_list->max_priority-1 : higher
	If 'GFXOBJ_SORT_PRIORITY_BACK' is specified,  order is reversed.

	Next,  the array of object is given to priority.
		object_list->objects[x] -> x=0 : lower , x=object_list->nums-1 = higher
	If 'GFXOBJ_SORT_OBJECT_BACK' is specified,  order is reversed.

	-The handling of tilemap :

	Tilemap can be treated by the callback function as one object.

	If the user function is registered in object->special_handler,
	the user function is called instead of drawgfx.

	Call tilemap_draw() in your callback handler.
	If object->code is numbered with two or more tilemap,it might be good.

	If you adjust objects->gfx to 0,Useless processing to tilemap is omitted.
	clipping,drawposition,visible,dirty_flag are not updated.

	If one tilemap is displayed with background and foreground
	Use two objects behind in front of sprite.

	Example:

	object resource:

	objects[0]   ==  tilemap1 , higher background
	objects[1]   ==  tilemap2 , lower  background
	objects[3]   ==  lower sprite
	objects[4]   ==  middle sprite
	objects[5]   ==  higher sprite
	objects[2]   ==  tilemap2 , foreground

	objects[0].code  = 1;  number of tilemap_2
	objects[0].color = 0;  background mark
	objects[1].code  = 0;  number of tilemap_1
	objects[1].color = 0;  background mark
	objects[5].code  = 0;  number of tilemap_1
	objects[5].color = 1;  foreground mark

	objects[0,1,5].special_handler = object_draw_tile;
	objects[0,1,5].gfx = 0;

	callback handler:

	void object_draw_tile(struct osd_bitmap *bitmap,struct gfx_object *object)
	{
		tilemap_draw( bitmap , tilemap[object->code] , object->color );
	}

*/

#include "driver.h"
/* #include "gfxobj.h" */

static struct gfx_object_list *first_object_list;

void gfxobj_init(void)
{
	first_object_list = 0;
}

void gfxobj_close(void)
{
	struct gfx_object_list *object_list,*object_next;
	for(object_list = first_object_list ; object_list != 0 ; object_list=object_next)
	{
		free(object_list->objects);
		object_next = object_list->next;
		free(object_list);
	}
}

#define MAX_PRIORITY 16
struct gfx_object_list *gfxobj_create(int nums,int max_priority,const struct gfx_object *def_object)
{
	struct gfx_object_list *object_list;
	int i;

	/* priority limit check */
	if(max_priority >= MAX_PRIORITY)
		return 0;

	/* allocate object liset */
	if( (object_list = (struct gfx_object_list*)malloc(sizeof(struct gfx_object_list))) == 0 )
		return 0;
	memset(object_list,0,sizeof(struct gfx_object_list));

	/* allocate objects */
	if( (object_list->objects = (struct gfx_object*)malloc(nums*sizeof(struct gfx_object))) == 0)
	{
		free(object_list);
		return 0;
	}
	if(def_object == 0)
	{	/* clear all objects */
		memset(object_list->objects,0,nums*sizeof(struct gfx_object));
	}
	else
	{	/* preset with default objects */
		for(i=0;i<nums;i++)
			memcpy(&object_list->objects[i],def_object,sizeof(struct gfx_object));
	}
	/* setup objects */
	for(i=0;i<nums;i++)
	{
		/*	dirty flag */
		object_list->objects[i].dirty_flag = GFXOBJ_DIRTY_ALL;
		/* link map */
		object_list->objects[i].next = &object_list->objects[i+1];
	}
	/* setup object_list */
	object_list->max_priority = max_priority;
	object_list->nums = nums;
	object_list->first_object = object_list->objects; /* top of link */
	object_list->objects[nums-1].next = 0; /* bottom of link */
	object_list->sort_type = GFXOBJ_SORT_DEFAULT;
	/* resource tracking */
	object_list->next = first_object_list;
	first_object_list = object_list;

	return object_list;
}

/* set pixel dirty flag */
void gfxobj_mark_all_pixels_dirty(struct gfx_object_list *object_list)
{
	/* don't care , gfx object manager don't keep color remapped bitmap */
}

/* update object */
static void object_update(struct gfx_object *object)
{
	int min_x,min_y,max_x,max_y;

	/* clear dirty flag */
	object->dirty_flag = 0;

	/* if gfx == 0 ,then bypass (for special_handler ) */
	if(object->gfx == 0)
		return;

	/* check visible area */
	min_x = Machine->visible_area.min_x;
	max_x = Machine->visible_area.max_x;
	min_y = Machine->visible_area.min_y;
	max_y = Machine->visible_area.max_y;
	if(
		(object->width==0)  ||
		(object->height==0) ||
		(object->sx > max_x) ||
		(object->sy > max_y) ||
		(object->sx+object->width  <= min_x) ||
		(object->sy+object->height <= min_y) )
	{	/* outside of visible area */
		object->visible = 0;
		return;
	}
	object->visible = 1;
	/* set draw position with adjust source offset */
	object->draw_x = object->sx -
		(	object->flipx ?
			(object->gfx->width - (object->left + object->width)) : /* flip */
			(object->left) /* non flip */
		);

	object->draw_y = object->sy -
		(	object->flipy ?
			(object->gfx->height - (object->top + object->height)) : /* flip */
			(object->top) /* non flip */
		);
	/* set clipping point to object draw area */
	object->clip.min_x = object->sx;
	object->clip.max_x = object->sx + object->width -1;
	object->clip.min_y = object->sy;
	object->clip.max_y = object->sy + object->height -1;
	/* adjust clipping point with visible area */
	if (object->clip.min_x < min_x) object->clip.min_x = min_x;
	if (object->clip.max_x > max_x) object->clip.max_x = max_x;
	if (object->clip.min_y < min_y) object->clip.min_y = min_y;
	if (object->clip.max_y > max_y) object->clip.max_y = max_y;
}

/* update one of object list */
static void gfxobj_update_one(struct gfx_object_list *object_list)
{
	struct gfx_object *object;
	struct gfx_object *start_object,*last_object;
	int dx,start_priority,end_priority;
	int priorities = object_list->max_priority;
	int priority;

	if(object_list->sort_type&GFXOBJ_DO_SORT)
	{
		struct gfx_object *top_object[MAX_PRIORITY],*end_object[MAX_PRIORITY];
		/* object sort direction */
		if(object_list->sort_type&GFXOBJ_SORT_OBJECT_BACK)
		{
			start_object   = object_list->objects + object_list->nums-1;
			last_object    = object_list->objects-1;
			dx = -1;
		}
		else
		{
			start_object = object_list->objects;
			last_object  = object_list->objects + object_list->nums;
			dx = 1;
		}
		/* reset each priority point */
		for( priority = 0; priority < priorities; priority++ )
			end_object[priority] = 0;
		/* update and sort */
		for(object=start_object ; object != last_object ; object+=dx)
		{
			/* update all objects */
			if(object->dirty_flag)
				object_update(object);
			/* store link */
			if(object->visible)
			{
				priority = object->priority;
				if(end_object[priority])
					end_object[priority]->next = object;
				else
					top_object[priority] = object;
				end_object[priority] = object;
			}
		}

		/* priority sort direction */
		if(object_list->sort_type&GFXOBJ_SORT_PRIORITY_BACK)
		{
			start_priority = priorities-1;
			end_priority   = -1;
			dx = -1;
		}
		else
		{
			start_priority = 0;
			end_priority   = priorities;
			dx = 1;
		}
		/* link between priority */
		last_object = 0;
		for( priority = start_priority; priority != end_priority; priority+=dx )
		{
			if(end_object[priority])
			{
				if(last_object)
					last_object->next = top_object[priority];
				else
					object_list->first_object = top_object[priority];
				last_object = end_object[priority];
			}
		}
		if(last_object == 0 )
			object_list->first_object = 0;
		else
			last_object->next = 0;
	}
	else
	{	/* non sort , update only linked object */
		for(object=object_list->first_object ; object !=0 ; object=object->next)
		{
			/* update all objects */
			if(object->dirty_flag)
				object_update(object);
		}
	}
	/* palette resource */
	if(object->palette_flag)
	{
		/* !!!!! do not supported yet !!!!! */
	}
}

void gfxobj_update(void)
{
	struct gfx_object_list *object_list;

	for(object_list=first_object_list ; object_list != 0 ; object_list=object_list->next)
		gfxobj_update_one(object_list);
}

static void draw_object_one(struct osd_bitmap *bitmap,struct gfx_object *object)
{
	if(object->special_handler)
	{	/* special object , callback user draw handler */
		object->special_handler(bitmap,object);
	}
	else
	{	/* normaly gfx object */
		drawgfx(bitmap,object->gfx,
				object->code,
				object->color,
				object->flipx,
				object->flipy,
				object->draw_x,
				object->draw_y,
				&object->clip,
				object->transparency,
				object->transparet_color);
	}
}

void gfxobj_draw(struct gfx_object_list *object_list)
{
	struct osd_bitmap *bitmap = Machine->scrbitmap;
	struct gfx_object *object;

	for(object=object_list->first_object ; object ; object=object->next)
	{
		if(object->visible )
			draw_object_one(bitmap,object);
	}
}
