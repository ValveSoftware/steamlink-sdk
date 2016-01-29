#include <ctype.h>
#include <stdarg.h>
#include "driver.h"
#include "ui_text.h" /* LBO 042400 */
#include "artwork.h"

static struct RunningMachine machine;
struct RunningMachine *Machine = &machine;
static const struct GameDriver *gamedrv;
static const struct MachineDriver *drv;
static struct osd_bitmap *real_scrbitmap;

/* Variables to hold the status of various game options */
struct GameOptions	options;

void *record;   /* for -record */
void *playback; /* for -playback */
int mame_debug; /* !0 when -debug option is specified */

int bailing;	/* set to 1 if the startup is aborted to prevent multiple error messages */

static int settingsloaded;

int bitmap_dirty;	/* set by osd_clearbitmap() */


/* Used in vh_open */
extern unsigned char *spriteram,*spriteram_2;
extern unsigned char *buffered_spriteram,*buffered_spriteram_2;
extern int spriteram_size,spriteram_2_size;

int init_machine(void);
void shutdown_machine(void);
int run_machine(void);

void overlay_free(void);
void backdrop_free(void);
void overlay_remap(void);
void overlay_draw(struct osd_bitmap *dest,struct osd_bitmap *source);

int run_game(int game)
{
	int err;

	/* copy some settings into easier-to-handle variables */
	record     = options.record;
	playback   = options.playback;
	mame_debug = options.mame_debug;

	Machine->gamedrv = gamedrv = drivers[game];
	Machine->drv = drv = gamedrv->drv;

	/* copy configuration */
	if (options.color_depth == 16 ||
			(options.color_depth != 8 && (Machine->gamedrv->flags & GAME_REQUIRES_16BIT)))
		Machine->color_depth = 16;
	else
		Machine->color_depth = 8;

	//if (options.vector_width == 0) options.vector_width = 640;
	//if (options.vector_height == 0) options.vector_height = 480;
	if (options.vector_width == 0) options.vector_width = 320;
	if (options.vector_height == 0) options.vector_height = 240;

	Machine->sample_rate = options.samplerate;

	/* get orientation right */
	Machine->orientation = gamedrv->flags & ORIENTATION_MASK;
	Machine->ui_orientation = ROT0;
	if (options.norotate)
		Machine->orientation = ROT0;
	if (options.ror)
	{
		/* if only one of the components is inverted, switch them */
		if ((Machine->orientation & ROT180) == ORIENTATION_FLIP_X ||
				(Machine->orientation & ROT180) == ORIENTATION_FLIP_Y)
			Machine->orientation ^= ROT180;

		Machine->orientation ^= ROT90;

		/* if only one of the components is inverted, switch them */
		if ((Machine->ui_orientation & ROT180) == ORIENTATION_FLIP_X ||
				(Machine->ui_orientation & ROT180) == ORIENTATION_FLIP_Y)
			Machine->ui_orientation ^= ROT180;

		Machine->ui_orientation ^= ROT90;
	}
	if (options.rol)
	{
		/* if only one of the components is inverted, switch them */
		if ((Machine->orientation & ROT180) == ORIENTATION_FLIP_X ||
				(Machine->orientation & ROT180) == ORIENTATION_FLIP_Y)
			Machine->orientation ^= ROT180;

		Machine->orientation ^= ROT270;

		/* if only one of the components is inverted, switch them */
		if ((Machine->ui_orientation & ROT180) == ORIENTATION_FLIP_X ||
				(Machine->ui_orientation & ROT180) == ORIENTATION_FLIP_Y)
			Machine->ui_orientation ^= ROT180;

		Machine->ui_orientation ^= ROT270;
	}
	if (options.flipx)
	{
		Machine->orientation ^= ORIENTATION_FLIP_X;
		Machine->ui_orientation ^= ORIENTATION_FLIP_X;
	}
	if (options.flipy)
	{
		Machine->orientation ^= ORIENTATION_FLIP_Y;
		Machine->ui_orientation ^= ORIENTATION_FLIP_Y;
	}

	set_pixel_functions();

	/* Do the work*/
	err = 1;
	bailing = 0;

	#ifdef MESS
	if (get_filenames())
		return err;
	#endif

	if (osd_init() == 0)
	{
		if (init_machine() == 0)
		{
			if (run_machine() == 0)
				err = 0;
			else if (!bailing)
			{
				bailing = 1;
				printf("Unable to start machine emulation\n");
			}

			shutdown_machine();
		}
		else if (!bailing)
		{
			bailing = 1;
			printf("Unable to initialize machine emulation\n");
		}

		osd_exit();
	}
	else if (!bailing)
	{
		bailing = 1;
		printf ("Unable to initialize system\n");
	}

	return err;
}



/***************************************************************************

  Initialize the emulated machine (load the roms, initialize the various
  subsystems...). Returns 0 if successful.

***************************************************************************/
int init_machine(void)
{
	int i;

	/* LBO 042400 start */
	if (uistring_init (options.language_file) != 0)
		goto out;
	/* LBO 042400 end */

	if (code_init() != 0)
		goto out;

	for (i = 0;i < MAX_MEMORY_REGIONS;i++)
	{
		Machine->memory_region[i] = 0;
		Machine->memory_region_length[i] = 0;
		Machine->memory_region_type[i] = 0;
	}

	if (gamedrv->input_ports)
	{
		Machine->input_ports = input_port_allocate(gamedrv->input_ports);
		if (!Machine->input_ports)
			goto out_code;
		Machine->input_ports_default = input_port_allocate(gamedrv->input_ports);
		if (!Machine->input_ports_default)
		{
			input_port_free(Machine->input_ports);
			Machine->input_ports = 0;
			goto out_code;
		}
	}

    #ifdef MESS
	if (!gamedrv->rom)
	{
		logerror("Going to load_next tag\n");
		goto load_next;
	}
    #endif

	if (readroms() != 0)
		goto out_free;

	#ifdef MESS
	load_next:
		if (init_devices(gamedrv))
			goto out_free;
	#endif

	/* Mish:  Multi-session safety - set spriteram size to zero before memory map is set up */
	spriteram_size=spriteram_2_size=0;

	/* first of all initialize the memory handlers, which could be used by the */
	/* other initialization routines */
	cpu_init();

	/* load input ports settings (keys, dip switches, and so on) */
	settingsloaded = load_input_port_settings();

	if( !memory_init() )
		goto out_free;

	if (gamedrv->driver_init) (*gamedrv->driver_init)();

	return 0;

out_free:
	input_port_free(Machine->input_ports);
	Machine->input_ports = 0;
	input_port_free(Machine->input_ports_default);
	Machine->input_ports_default = 0;
out_code:
	code_close();
out:
	return 1;
}



void shutdown_machine(void)
{
	int i;


	#ifdef MESS
	exit_devices();
	#endif

    /* ASG 971007 free memory element map */
	memory_shutdown();

	/* free the memory allocated for ROM and RAM */
	for (i = 0;i < MAX_MEMORY_REGIONS;i++)
	{
		if (Machine->memory_region[i])
			free(Machine->memory_region[i]);
		Machine->memory_region[i] = 0;
		Machine->memory_region_length[i] = 0;
		Machine->memory_region_type[i] = 0;
	}

	/* free the memory allocated for input ports definition */
	input_port_free(Machine->input_ports);
	Machine->input_ports = 0;
	input_port_free(Machine->input_ports_default);
	Machine->input_ports_default = 0;

	code_close();

	uistring_shutdown (); /* LBO 042400 */
}



static void vh_close(void)
{
	int i;


	for (i = 0;i < MAX_GFX_ELEMENTS;i++)
	{
		freegfx(Machine->gfx[i]);
		Machine->gfx[i] = 0;
	}
	freegfx(Machine->uifont);
	Machine->uifont = 0;
	osd_close_display();
	if (Machine->scrbitmap)
	{
		bitmap_free(Machine->scrbitmap);
		Machine->scrbitmap = NULL;
	}

	palette_stop();

	if (drv->video_attributes & VIDEO_BUFFERS_SPRITERAM) {
		if (buffered_spriteram) free(buffered_spriteram);
		if (buffered_spriteram_2) free(buffered_spriteram_2);
		buffered_spriteram=NULL;
		buffered_spriteram_2=NULL;
	}
}



/* Scale the vector games to a given resolution */
static void scale_vectorgames(int gfx_width,int gfx_height,int *width,int *height)
{
	double x_scale, y_scale, scale;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		x_scale=(double)gfx_width/(double)(*height);
		y_scale=(double)gfx_height/(double)(*width);
	}
	else
	{
		x_scale=(double)gfx_width/(double)(*width);
		y_scale=(double)gfx_height/(double)(*height);
	}
	if (x_scale<y_scale)
		scale=x_scale;
	else
		scale=y_scale;
	*width=(int)((double)*width*scale);
	*height=(int)((double)*height*scale);

	/* Padding to an dword value */
	*width-=*width % 4;
	*height-=*height % 4;
}


static int vh_open(void)
{
	int i;
	int width,height;


	for (i = 0;i < MAX_GFX_ELEMENTS;i++) Machine->gfx[i] = 0;
	Machine->uifont = 0;

	if (palette_start() != 0)
	{
		vh_close();
		return 1;
	}


	/* convert the gfx ROMs into character sets. This is done BEFORE calling the driver's */
	/* convert_color_prom() routine (in palette_init()) because it might need to check the */
	/* Machine->gfx[] data */
	if (drv->gfxdecodeinfo)
	{
		for (i = 0;i < MAX_GFX_ELEMENTS && drv->gfxdecodeinfo[i].memory_region != -1;i++)
		{
			int reglen = 8*memory_region_length(drv->gfxdecodeinfo[i].memory_region);
			struct GfxLayout glcopy;
			int j;


			memcpy(&glcopy,drv->gfxdecodeinfo[i].gfxlayout,sizeof(glcopy));

			if (IS_FRAC(glcopy.total))
				glcopy.total = reglen / glcopy.charincrement * FRAC_NUM(glcopy.total) / FRAC_DEN(glcopy.total);
			for (j = 0;j < MAX_GFX_PLANES;j++)
			{
				if (IS_FRAC(glcopy.planeoffset[j]))
				{
					glcopy.planeoffset[j] = FRAC_OFFSET(glcopy.planeoffset[j]) +
							reglen * FRAC_NUM(glcopy.planeoffset[j]) / FRAC_DEN(glcopy.planeoffset[j]);
				}
			}
			for (j = 0;j < MAX_GFX_SIZE;j++)
			{
				if (IS_FRAC(glcopy.xoffset[j]))
				{
					glcopy.xoffset[j] = FRAC_OFFSET(glcopy.xoffset[j]) +
							reglen * FRAC_NUM(glcopy.xoffset[j]) / FRAC_DEN(glcopy.xoffset[j]);
				}
				if (IS_FRAC(glcopy.yoffset[j]))
				{
					glcopy.yoffset[j] = FRAC_OFFSET(glcopy.yoffset[j]) +
							reglen * FRAC_NUM(glcopy.yoffset[j]) / FRAC_DEN(glcopy.yoffset[j]);
				}
			}

			if ((Machine->gfx[i] = decodegfx(memory_region(drv->gfxdecodeinfo[i].memory_region)
					+ drv->gfxdecodeinfo[i].start,
					&glcopy)) == 0)
			{
				vh_close();

				bailing = 1;
				printf("Out of memory decoding gfx\n");

				return 1;
			}
			if (Machine->remapped_colortable)
				Machine->gfx[i]->colortable = &Machine->remapped_colortable[drv->gfxdecodeinfo[i].color_codes_start];
			Machine->gfx[i]->total_colors = drv->gfxdecodeinfo[i].total_color_codes;
		}
	}


	width = drv->screen_width;
	height = drv->screen_height;

	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
		scale_vectorgames(options.vector_width,options.vector_height,&width,&height);

	Machine->scrbitmap = bitmap_alloc_depth(width,height,Machine->color_depth);
	if (!Machine->scrbitmap)
	{
		vh_close();
		return 1;
	}

	if (!(Machine->drv->video_attributes & VIDEO_TYPE_VECTOR))
	{
		width = drv->default_visible_area.max_x - drv->default_visible_area.min_x + 1;
		height = drv->default_visible_area.max_y - drv->default_visible_area.min_y + 1;
	}

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;
		temp = width; width = height; height = temp;
	}

	/* create the display bitmap, and allocate the palette */
	if (osd_create_display(width,height,Machine->color_depth,
			drv->frames_per_second,drv->video_attributes,Machine->orientation))
	{
		vh_close();
		return 1;
	}

	set_visible_area(
			drv->default_visible_area.min_x,
			drv->default_visible_area.max_x,
			drv->default_visible_area.min_y,
			drv->default_visible_area.max_y);

	/* create spriteram buffers if necessary */
	if (drv->video_attributes & VIDEO_BUFFERS_SPRITERAM) {
		if (spriteram_size!=0) {
			buffered_spriteram= (unsigned char *) malloc(spriteram_size);
			if (!buffered_spriteram) { vh_close(); return 1; }
			if (spriteram_2_size!=0) buffered_spriteram_2 = (unsigned char *) malloc(spriteram_2_size);
			if (spriteram_2_size && !buffered_spriteram_2) { vh_close(); return 1; }
		} else {
			logerror("vh_open():  Video buffers spriteram but spriteram_size is 0\n");
			buffered_spriteram=NULL;
			buffered_spriteram_2=NULL;
		}
	}

	/* build our private user interface font */
	/* This must be done AFTER osd_create_display() so the function knows the */
	/* resolution we are running at and can pick a different font depending on it. */
	/* It must be done BEFORE palette_init() because that will also initialize */
	/* (through osd_allocate_colors()) the uifont colortable. */
	if ((Machine->uifont = builduifont()) == 0)
	{
		vh_close();
		return 1;
	}

	/* initialize the palette - must be done after osd_create_display() */
	if (palette_init())
	{
		vh_close();
		return 1;
	}

	return 0;
}



/***************************************************************************

  This function takes care of refreshing the screen, processing user input,
  and throttling the emulation speed to obtain the required frames per second.

***************************************************************************/

int need_to_clear_bitmap;	/* set by the user interface */

int updatescreen(void)
{
	/* update sound */
	sound_update();

	if (osd_skip_this_frame() == 0)
	{
		profiler_mark(PROFILER_VIDEO);
		if (need_to_clear_bitmap)
		{
			osd_clearbitmap(real_scrbitmap);
			need_to_clear_bitmap = 0;
		}
		draw_screen(bitmap_dirty);	/* update screen */
		bitmap_dirty = 0;
		profiler_mark(PROFILER_END);
	}

	/* the user interface must be called between vh_update() and osd_update_video_and_audio(), */
	/* to allow it to overlay things on the game display. We must call it even */
	/* if the frame is skipped, to keep a consistent timing. */
	if (handle_user_interface(real_scrbitmap))
		/* quit if the user asked to */
		return 1;

	update_video_and_audio();

	if (drv->vh_eof_callback) (*drv->vh_eof_callback)();

	return 0;
}


/***************************************************************************

  Draw screen with overlays and backdrops (not yet)

***************************************************************************/

void draw_screen(int _bitmap_dirty)
{
	if (_bitmap_dirty)  overlay_remap();

	(*Machine->drv->vh_update)(Machine->scrbitmap,_bitmap_dirty);  /* update screen */

	if (artwork_overlay)
	{
		overlay_draw(overlay_real_scrbitmap, Machine->scrbitmap);
	}
}


/***************************************************************************

  Calls OSD layer handling overlays and backdrops (not yet)

***************************************************************************/
void update_video_and_audio(void)
{
	osd_update_video_and_audio(real_scrbitmap);
}


/***************************************************************************

  Run the emulation. Start the various subsystems and the CPU emulation.
  Returns non zero in case of error.

***************************************************************************/
int run_machine(void)
{
	int res = 1;


	if (vh_open() == 0)
	{
		tilemap_init();
		sprite_init();
		gfxobj_init();
		if (drv->vh_start == 0 || (*drv->vh_start)() == 0)      /* start the video hardware */
		{
			if (sound_start() == 0) /* start the audio hardware */
			{
				int	region;

				real_scrbitmap = artwork_overlay ? overlay_real_scrbitmap : Machine->scrbitmap;

				/* free memory regions allocated with REGIONFLAG_DISPOSE (typically gfx roms) */
				for (region = 0; region < MAX_MEMORY_REGIONS; region++)
				{
					if (Machine->memory_region_type[region] & REGIONFLAG_DISPOSE)
					{
						int i;

						/* invalidate contents to avoid subtle bugs */
						for (i = 0;i < memory_region_length(region);i++)
							memory_region(region)[i] = rand();
						free(Machine->memory_region[region]);
						Machine->memory_region[region] = 0;
					}
				}

				if (settingsloaded == 0)
				{
					/* if there is no saved config, it must be first time we run this game, */
					/* so show the disclaimer. */
//sq					if (showcopyright(real_scrbitmap)) goto userquit;
				}

//sq				if (showgamewarnings(real_scrbitmap) == 0)  /* show info about incorrect behaviour (wrong colors etc.) */
				{
					/* shut down the leds (work around Allegro hanging bug in the DOS port) */
					osd_led_w(0,1);
					osd_led_w(1,1);
					osd_led_w(2,1);
					osd_led_w(3,1);
					osd_led_w(0,0);
					osd_led_w(1,0);
					osd_led_w(2,0);
					osd_led_w(3,0);

					init_user_interface();

					/* disable cheat if no roms */
					if (!gamedrv->rom) options.cheat = 0;

					if (options.cheat) InitCheat();

					if (drv->nvram_handler)
					{
						void *f;

						f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_NVRAM,0);
						(*drv->nvram_handler)(f,0);
						if (f) osd_fclose(f);
					}

					cpu_run();      /* run the emulation! */

					if (drv->nvram_handler)
					{
						void *f;

						if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_NVRAM,1)) != 0)
						{
							(*drv->nvram_handler)(f,1);
							osd_fclose(f);
						}
					}

					if (options.cheat) StopCheat();

					/* save input ports settings */
					save_input_port_settings();
				}

userquit:
				/* the following MUST be done after hiscore_save() otherwise */
				/* some 68000 games will not work */
				sound_stop();
				if (drv->vh_stop) (*drv->vh_stop)();
				overlay_free();
				backdrop_free();

				res = 0;
			}
			else if (!bailing)
			{
				bailing = 1;
				printf("Unable to start audio emulation\n");
			}
		}
		else if (!bailing)
		{
			bailing = 1;
			printf("Unable to start video emulation\n");
		}

		gfxobj_close();
		sprite_close();
		tilemap_close();
		vh_close();
	}
	else if (!bailing)
	{
		bailing = 1;
		printf("Unable to start video emulation\n");
	}

	return res;
}



int mame_highscore_enabled(void)
{
	/* disable high score when record/playback is on */
	if (record != 0 || playback != 0) return 0;

	/* disable high score when cheats are used */
	if (he_did_cheat != 0) return 0;

#ifdef MAME_NET
    /* disable high score when playing network game */
    /* (this forces all networked machines to start from the same state!) */
    if (net_active()) return 0;
#endif /* MAME_NET */

	return 1;
}
