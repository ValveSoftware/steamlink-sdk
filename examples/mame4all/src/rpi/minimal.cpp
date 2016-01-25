/*
*/

#include "minimal.h"
#include "driver.h"

#include <bcm_host.h>
#include <SDL.h>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

static SDL_Surface* sdlscreen = NULL;

unsigned long 			gp2x_dev[3];
unsigned char 			*gp2x_screen8;
unsigned short 			*gp2x_screen15;
volatile unsigned short 	gp2x_palette[512];
int 				rotate_controls=0;

static int surface_width;
static int surface_height;

#define MAX_SAMPLE_RATE (44100*2)

void gp2x_video_flip(struct osd_bitmap *bitmap)
{
    DisplayScreen(bitmap);
}

void gp2x_video_flip_single(struct osd_bitmap *bitmap)
{
    DisplayScreen(bitmap);
}

extern void gles2_palette_changed();

void gles2_palette_changed();

void gp2x_video_setpalette(void)
{
	gles2_palette_changed();
}

extern void keyprocess(SDLKey inkey, SDL_bool pressed);
extern void joyprocess(Uint8 button, SDL_bool pressed, Uint8 njoy);
extern void mouse_motion_process(int x, int y);
extern void mouse_button_process(Uint8 button, SDL_bool pressed);

unsigned long gp2x_joystick_read()
{
    SDL_Event event;

	//Reset mouse incase there is no motion
	mouse_motion_process(0,0);

	while(SDL_PollEvent(&event)) {
		switch(event.type)
		{
			case SDL_KEYDOWN:
				keyprocess(event.key.keysym.sym, SDL_TRUE);
				break;
			case SDL_KEYUP:
				keyprocess(event.key.keysym.sym, SDL_FALSE);
				break;
			case SDL_JOYBUTTONDOWN:
				joyprocess(event.jbutton.button, SDL_TRUE, event.jbutton.which);
				break;
			case SDL_JOYBUTTONUP:
				joyprocess(event.jbutton.button, SDL_FALSE, event.jbutton.which);
				break;
			case SDL_MOUSEMOTION:
				mouse_motion_process(event.motion.xrel, event.motion.yrel);
				break;
			case SDL_MOUSEBUTTONDOWN:
				mouse_button_process(event.button.button, SDL_TRUE);
				break;
			case SDL_MOUSEBUTTONUP:
				mouse_button_process(event.button.button, SDL_FALSE);
				break;

			default:
				break;
		}
	}
}

void gp2x_sound_volume(int l, int r)
{
}

void gp2x_timer_delay(unsigned long ticks)
{
	unsigned long ini=gp2x_timer_read();
	while (gp2x_timer_read()-ini<ticks);
}


unsigned long gp2x_timer_read(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	return ((unsigned long long)now.tv_sec * 1000000LL + (now.tv_nsec / 1000LL));

}

// create two resources for 'page flipping'
static DISPMANX_RESOURCE_HANDLE_T   resource0;
static DISPMANX_RESOURCE_HANDLE_T   resource1;
static DISPMANX_RESOURCE_HANDLE_T   resource_bg;

// these are used for switching between the buffers
static DISPMANX_RESOURCE_HANDLE_T cur_res;
static DISPMANX_RESOURCE_HANDLE_T prev_res;
static DISPMANX_RESOURCE_HANDLE_T tmp_res;

DISPMANX_ELEMENT_HANDLE_T dispman_element;
DISPMANX_ELEMENT_HANDLE_T dispman_element_bg;
DISPMANX_DISPLAY_HANDLE_T dispman_display;
DISPMANX_UPDATE_HANDLE_T dispman_update;

void gles2_create(int display_width, int display_height, int bitmap_width, int bitmap_height, int depth);
void gles2_destroy();
void gles2_palette_changed();

EGLDisplay display = NULL;
EGLSurface surface = NULL;
static EGLContext context = NULL;
static EGL_DISPMANX_WINDOW_T nativewindow;


void exitfunc()
{
	SDL_Quit();
	bcm_host_deinit();
}

SDL_Joystick* myjoy[4];

int init_SDL(void)
{
	myjoy[0]=0;
	myjoy[1]=0;
	myjoy[2]=0;
	myjoy[3]=0;

    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        return(0);
    }
    sdlscreen = SDL_SetVideoMode(0,0, 16, SDL_SWSURFACE);

	//We handle up to four joysticks
	if(SDL_NumJoysticks()) 
	{
		int i;
    	SDL_JoystickEventState(SDL_ENABLE);
		
		for(i=0;i<SDL_NumJoysticks();i++) {	
			myjoy[i]=SDL_JoystickOpen(i);
		}
		if(myjoy[0]) 
			logerror("Found %d joysticks\n",SDL_NumJoysticks());
	}
	SDL_EventState(SDL_ACTIVEEVENT,SDL_IGNORE);
	SDL_EventState(SDL_SYSWMEVENT,SDL_IGNORE);
	SDL_EventState(SDL_VIDEORESIZE,SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT,SDL_IGNORE);
	SDL_ShowCursor(SDL_DISABLE);

    //Initialise dispmanx
    bcm_host_init();
    
	//Clean exits, hopefully!
	atexit(exitfunc);
    
    return(1);
}

void deinit_SDL(void)
{
    if(sdlscreen)
    {
        SDL_FreeSurface(sdlscreen);
        sdlscreen = NULL;
    }
    SDL_Quit();
    
    bcm_host_deinit();
}


void gp2x_deinit(void)
{
	int ret;

    gles2_destroy();
    // Release OpenGL resources
    eglMakeCurrent( display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroySurface( display, surface );
    eglDestroyContext( display, context );
    eglTerminate( display );

	dispman_update = vc_dispmanx_update_start( 0 );
	ret = vc_dispmanx_element_remove( dispman_update, dispman_element );
	ret = vc_dispmanx_element_remove( dispman_update, dispman_element_bg );
	ret = vc_dispmanx_update_submit_sync( dispman_update );
	ret = vc_dispmanx_resource_delete( resource0 );
	ret = vc_dispmanx_resource_delete( resource1 );
	ret = vc_dispmanx_resource_delete( resource_bg );
	ret = vc_dispmanx_display_close( dispman_display );

	if(gp2x_screen8) free(gp2x_screen8);
	if(gp2x_screen15) free(gp2x_screen15);
	gp2x_screen8=0;
	gp2x_screen15=0;
}

static uint32_t display_adj_width, display_adj_height;		//display size minus border

void gp2x_set_video_mode(struct osd_bitmap *bitmap, int bpp,int width,int height)
{

	int ret;
	uint32_t display_width, display_height;
	uint32_t display_x=0, display_y=0;
	float display_ratio,game_ratio;

	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	surface_width = width;
	surface_height = height;

	gp2x_screen8=(unsigned char *) calloc(1, width*height);
	gp2x_screen15=(unsigned short *) calloc(1, width*height*2);

	// get an EGL display connection
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(display != EGL_NO_DISPLAY);

	// initialize the EGL display connection
	EGLBoolean result = eglInitialize(display, NULL, NULL);
	assert(EGL_FALSE != result);

	// get an appropriate EGL frame buffer configuration
	EGLint num_config;
	EGLConfig config;
	static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8, 
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	    EGL_NONE
	};
	result = eglChooseConfig(display, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);

	result = eglBindAPI(EGL_OPENGL_ES_API);
	assert(EGL_FALSE != result);

	// create an EGL rendering context
	static const EGLint context_attributes[] =
	{
	    EGL_CONTEXT_CLIENT_VERSION, 2,
	    EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
	assert(context != EGL_NO_CONTEXT);

	// create an EGL window surface
	int32_t success = graphics_get_display_size(0, &display_width, &display_height);
	assert(success >= 0);

	display_adj_width = display_width - (options.display_border * 2);
	display_adj_height = display_height - (options.display_border * 2);

	if (options.display_smooth_stretch) 
	{
		//We use the dispmanx scaler to smooth stretch the display
		//so GLES2 doesn't have to handle the performance intensive postprocessing

	    uint32_t sx, sy;

	 	// Work out the position and size on the display
	 	display_ratio = (float)display_width/(float)display_height;
	 	game_ratio = (float)width/(float)height;
	 
		display_x = sx = display_adj_width;
		display_y = sy = display_adj_height;

	 	if (game_ratio>display_ratio) 
			sy = (float)display_adj_width/(float)game_ratio;
	 	else 
			sx = (float)display_adj_height*(float)game_ratio;
	 
		// Centre bitmap on screen
	 	display_x = (display_x - sx) / 2;
	 	display_y = (display_y - sy) / 2;
	
		vc_dispmanx_rect_set( &dst_rect, 
								display_x + options.display_border, 
								display_y + options.display_border, 
								sx, sy);
	}
	else
		vc_dispmanx_rect_set( &dst_rect, options.display_border, options.display_border, 
								display_adj_width, display_adj_height);

	if (options.display_smooth_stretch) 
		vc_dispmanx_rect_set( &src_rect, 0, 0, width << 16, height << 16);
	else
		vc_dispmanx_rect_set( &src_rect, 0, 0, display_adj_width << 16, display_adj_height << 16);

	dispman_display = vc_dispmanx_display_open(0);
	dispman_update = vc_dispmanx_update_start(0);
	dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
	  				10, &dst_rect, 0, &src_rect, 
					DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_NO_ROTATE);

	//Black background surface dimensions
	vc_dispmanx_rect_set( &dst_rect, 0, 0, display_width, display_height );
	vc_dispmanx_rect_set( &src_rect, 0, 0, 128 << 16, 128 << 16);
 
	//Create a blank background for the whole screen, make sure width is divisible by 32!
	uint32_t crap;
	resource_bg = vc_dispmanx_resource_create(VC_IMAGE_RGB565, 128, 128, &crap);
	dispman_element_bg = vc_dispmanx_element_add(  dispman_update, dispman_display,
	                                      9, &dst_rect, resource_bg, &src_rect,
	                                      DISPMANX_PROTECTION_NONE, 0, 0,
	                                      (DISPMANX_TRANSFORM_T) 0 );

	nativewindow.element = dispman_element;
	if (options.display_smooth_stretch) {
		nativewindow.width = width;
		nativewindow.height = height;
	}
	else {
		nativewindow.width = display_adj_width;
		nativewindow.height = display_adj_height;
	}

	vc_dispmanx_update_submit_sync(dispman_update);

	surface = eglCreateWindowSurface(display, config, &nativewindow, NULL);
	assert(surface != EGL_NO_SURFACE);

	// connect the context to the surface
	result = eglMakeCurrent(display, surface, surface, context);
	assert(EGL_FALSE != result);

	//Smooth stretch the display size for GLES2 is the size of the bitmap
	//otherwise let GLES2 upscale (NEAR) to the size of the display
	if (options.display_smooth_stretch) 
		gles2_create(width, height, width, height, bitmap->depth);
	else
		gles2_create(display_adj_width, display_adj_height, width, height, bitmap->depth);
}

void gles2_draw(void * screen, int width, int height, int depth);
extern EGLDisplay display;
extern EGLSurface surface;

void DisplayScreen(struct osd_bitmap *bitmap)
{
	extern int throttle;
	static int save_throttle=0;

	if (throttle != save_throttle)
	{
		if(throttle)
			eglSwapInterval(display, 1);
		else
			eglSwapInterval(display, 0);

		save_throttle=throttle;
	}

    //Draw to the screen
	if (bitmap->depth == 8)
    	gles2_draw(gp2x_screen8, surface_width, surface_height, bitmap->depth);
	else
    	gles2_draw(gp2x_screen15, surface_width, surface_height, bitmap->depth);
    eglSwapBuffers(display, surface);
}


void gp2x_frontend_init(void)
{
	int ret;
        
	uint32_t display_width, display_height;
    
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
    
	surface_width = 640;
	surface_height = 480;
    
	gp2x_screen8=0;
	gp2x_screen15=(unsigned short *) calloc(1, 640*480*2);
	
	graphics_get_display_size(0 /* LCD */, &display_width, &display_height);
    
	dispman_display = vc_dispmanx_display_open( 0 );
	assert( dispman_display != 0 );
        
	// Add border around bitmap for TV
	display_width -= options.display_border * 2;
	display_height -= options.display_border * 2;
    
	//Create two surfaces for flipping between
	//Make sure bitmap type matches the source for better performance
	uint32_t crap;
	resource0 = vc_dispmanx_resource_create(VC_IMAGE_RGB565, 640, 480, &crap);
	resource1 = vc_dispmanx_resource_create(VC_IMAGE_RGB565, 640, 480, &crap);
    
	vc_dispmanx_rect_set( &dst_rect, options.display_border, options.display_border,
                         display_width, display_height);
	vc_dispmanx_rect_set( &src_rect, 0, 0, 640 << 16, 480 << 16);
    
	//Make sure mame and background overlay the menu program
	dispman_update = vc_dispmanx_update_start( 0 );
    
	// create the 'window' element - based on the first buffer resource (resource0)
	dispman_element = vc_dispmanx_element_add(  dispman_update,
                                              dispman_display,
                                              1,
                                              &dst_rect,
                                              resource0,
                                              &src_rect,
                                              DISPMANX_PROTECTION_NONE,
                                              0,
                                              0,
                                              (DISPMANX_TRANSFORM_T) 0 );
    
	ret = vc_dispmanx_update_submit_sync( dispman_update );
    
	// setup swapping of double buffers
	cur_res = resource1;
	prev_res = resource0;
    
}

void gp2x_frontend_deinit(void)
{
	int ret;
    
	dispman_update = vc_dispmanx_update_start( 0 );
	ret = vc_dispmanx_element_remove( dispman_update, dispman_element );
	ret = vc_dispmanx_update_submit_sync( dispman_update );
	ret = vc_dispmanx_resource_delete( resource0 );
	ret = vc_dispmanx_resource_delete( resource1 );
	ret = vc_dispmanx_display_close( dispman_display );
    
	if(gp2x_screen8) free(gp2x_screen8);
	if(gp2x_screen15) free(gp2x_screen15);
	gp2x_screen8=0;
	gp2x_screen15=0;
    
}

void FE_DisplayScreen(void)
{
	VC_RECT_T dst_rect;

	vc_dispmanx_rect_set( &dst_rect, 0, 0, surface_width, surface_height );

	vc_dispmanx_resource_write_data( cur_res, VC_IMAGE_RGB565, surface_width*2, gp2x_screen15, &dst_rect );
	dispman_update = vc_dispmanx_update_start( 0 );
	vc_dispmanx_element_change_source( dispman_update, dispman_element, cur_res );
	vc_dispmanx_update_submit( dispman_update, 0, 0 );

	// swap current resource
	tmp_res = cur_res;
	cur_res = prev_res;
	prev_res = tmp_res;
}


static unsigned char fontdata8x8[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x3C,0x42,0x99,0xBD,0xBD,0x99,0x42,0x3C,0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C,
	0xFE,0x82,0x8A,0xD2,0xA2,0x82,0xFE,0x00,0xFE,0x82,0x82,0x82,0x82,0x82,0xFE,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x80,0xC0,0xF0,0xFC,0xF0,0xC0,0x80,0x00,0x01,0x03,0x0F,0x3F,0x0F,0x03,0x01,0x00,
	0x18,0x3C,0x7E,0x18,0x7E,0x3C,0x18,0x00,0xEE,0xEE,0xEE,0xCC,0x00,0xCC,0xCC,0x00,
	0x00,0x00,0x30,0x68,0x78,0x30,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x3C,0x66,0x7A,0x7A,0x7E,0x7E,0x3C,0x00,0x0E,0x3E,0x3A,0x22,0x26,0x6E,0xE4,0x40,
	0x18,0x3C,0x7E,0x3C,0x3C,0x3C,0x3C,0x00,0x3C,0x3C,0x3C,0x3C,0x7E,0x3C,0x18,0x00,
	0x08,0x7C,0x7E,0x7E,0x7C,0x08,0x00,0x00,0x10,0x3E,0x7E,0x7E,0x3E,0x10,0x00,0x00,
	0x58,0x2A,0xDC,0xC8,0xDC,0x2A,0x58,0x00,0x24,0x66,0xFF,0xFF,0x66,0x24,0x00,0x00,
	0x00,0x10,0x10,0x38,0x38,0x7C,0xFE,0x00,0xFE,0x7C,0x38,0x38,0x10,0x10,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x1C,0x18,0x00,0x18,0x18,0x00,
	0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x7C,0x28,0x7C,0x28,0x00,0x00,
	0x10,0x38,0x60,0x38,0x0C,0x78,0x10,0x00,0x40,0xA4,0x48,0x10,0x24,0x4A,0x04,0x00,
	0x18,0x34,0x18,0x3A,0x6C,0x66,0x3A,0x00,0x18,0x18,0x20,0x00,0x00,0x00,0x00,0x00,
	0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x00,0x0C,0x06,0x06,0x06,0x06,0x06,0x0C,0x00,
	0x10,0x54,0x38,0x7C,0x38,0x54,0x10,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x00,0x00,
	0x38,0x4C,0xC6,0xC6,0xC6,0x64,0x38,0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x7C,0xC6,0x0E,0x3C,0x78,0xE0,0xFE,0x00,0x7E,0x0C,0x18,0x3C,0x06,0xC6,0x7C,0x00,
	0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x00,0xFC,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00,
	0x3C,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00,0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00,
	0x78,0xC4,0xE4,0x78,0x86,0x86,0x7C,0x00,0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00,
	0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x18,0x18,0x30,
	0x1C,0x38,0x70,0xE0,0x70,0x38,0x1C,0x00,0x00,0x7C,0x00,0x00,0x7C,0x00,0x00,0x00,
	0x70,0x38,0x1C,0x0E,0x1C,0x38,0x70,0x00,0x7C,0xC6,0xC6,0x1C,0x18,0x00,0x18,0x00,
	0x3C,0x42,0x99,0xA1,0xA5,0x99,0x42,0x3C,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00,
	0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xFC,0x00,0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00,
	0xF8,0xCC,0xC6,0xC6,0xC6,0xCC,0xF8,0x00,0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xFE,0x00,
	0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0x00,0x3E,0x60,0xC0,0xCE,0xC6,0x66,0x3E,0x00,
	0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00,0xC6,0xCC,0xD8,0xF0,0xF8,0xDC,0xCE,0x00,
	0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00,
	0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xFC,0xC6,0xC6,0xC6,0xFC,0xC0,0xC0,0x00,0x7C,0xC6,0xC6,0xC6,0xDE,0xCC,0x7A,0x00,
	0xFC,0xC6,0xC6,0xCE,0xF8,0xDC,0xCE,0x00,0x78,0xCC,0xC0,0x7C,0x06,0xC6,0x7C,0x00,
	0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xC6,0xC6,0xC6,0xEE,0x7C,0x38,0x10,0x00,0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00,
	0xC6,0xEE,0x3C,0x38,0x7C,0xEE,0xC6,0x00,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00,
	0xFE,0x0E,0x1C,0x38,0x70,0xE0,0xFE,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,
	0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,
	0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,
	0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x06,0x3E,0x66,0x66,0x3C,0x00,
	0x60,0x7C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x3C,0x66,0x60,0x60,0x66,0x3C,0x00,
	0x06,0x3E,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x3C,0x66,0x66,0x7E,0x60,0x3C,0x00,
	0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x60,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x00,
	0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0x38,0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00,
	0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0xEC,0xFE,0xFE,0xFE,0xD6,0xC6,0x00,
	0x00,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00,
	0x00,0x7C,0x66,0x66,0x66,0x7C,0x60,0x60,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x06,
	0x00,0x7E,0x70,0x60,0x60,0x60,0x60,0x00,0x00,0x3C,0x60,0x3C,0x06,0x66,0x3C,0x00,
	0x30,0x78,0x30,0x30,0x30,0x30,0x1C,0x00,0x00,0x66,0x66,0x66,0x66,0x6E,0x3E,0x00,
	0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x7C,0x6C,0x00,
	0x00,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00,0x0E,0x18,0x0C,0x38,0x0C,0x18,0x0E,0x00,
	0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,0x70,0x18,0x30,0x1C,0x30,0x18,0x70,0x00,
	0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x10,0x28,0x10,0x54,0xAA,0x44,0x00,0x00,
};

static void gp2x_text(unsigned short *screen, int x, int y, char *text, int color)
{
	unsigned int i,l;
	screen=screen+(x*2)+(y*2)*640;

	for (i=0;i<strlen(text);i++) {
		
		for (l=0;l<16;l=l+2) {
			screen[l*640+0]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+0];
			screen[l*640+1]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+1];

			screen[l*640+2]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+2];
			screen[l*640+3]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+3];

			screen[l*640+4]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+4];
			screen[l*640+5]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+5];

			screen[l*640+6]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+6];
			screen[l*640+7]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+7];

			screen[l*640+8]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+8];
			screen[l*640+9]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+9];

			screen[l*640+10]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+10];
			screen[l*640+11]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+11];

			screen[l*640+12]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+12];
			screen[l*640+13]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+13];

			screen[l*640+14]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+14];
			screen[l*640+15]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+15];
		}
		for (l=1;l<16;l=l+2) {
			screen[l*640+0]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+0];
			screen[l*640+1]=(fontdata8x8[((text[i])*8)+l/2]&0x80)?color:screen[l*640+1];

			screen[l*640+2]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+2];
			screen[l*640+3]=(fontdata8x8[((text[i])*8)+l/2]&0x40)?color:screen[l*640+3];

			screen[l*640+4]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+4];
			screen[l*640+5]=(fontdata8x8[((text[i])*8)+l/2]&0x20)?color:screen[l*640+5];

			screen[l*640+6]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+6];
			screen[l*640+7]=(fontdata8x8[((text[i])*8)+l/2]&0x10)?color:screen[l*640+7];

			screen[l*640+8]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+8];
			screen[l*640+9]=(fontdata8x8[((text[i])*8)+l/2]&0x08)?color:screen[l*640+9];

			screen[l*640+10]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+10];
			screen[l*640+11]=(fontdata8x8[((text[i])*8)+l/2]&0x04)?color:screen[l*640+11];

			screen[l*640+12]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+12];
			screen[l*640+13]=(fontdata8x8[((text[i])*8)+l/2]&0x02)?color:screen[l*640+13];

			screen[l*640+14]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+14];
			screen[l*640+15]=(fontdata8x8[((text[i])*8)+l/2]&0x01)?color:screen[l*640+15];
		}
		screen+=16;
	} 
}

void gp2x_gamelist_text_out(int x, int y, char *eltexto, int color)
{
	char texto[33];
	strncpy(texto,eltexto,32);
	texto[32]=0;
	gp2x_text(gp2x_screen15,x,y,texto,color);
}

void gp2x_gamelist_text_out_fmt(int x, int y, char* fmt, ...)
{
	char strOut[128];
	va_list marker;
	
	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);	

	gp2x_gamelist_text_out(x, y, strOut, 255);
}

static int pflog=0;

void gp2x_printf_init(void)
{
	pflog=0;
}

#define gp2x_color15(R,G,B)  ((R >> 3) << 11) | (( G >> 2) << 5 ) | (( B >> 3 ) << 0 )

void gp2x_text_log(char *texto)
{
    if (!pflog)
    {
        memset(gp2x_screen15,0,320*240*2);
    }
    gp2x_text(gp2x_screen15,0,pflog,texto,gp2x_color15(255,255,255));
    pflog+=8;
    if(pflog>239) pflog=0;
}

/* Variadic functions guide found at http://www.unixpapa.com/incnote/variadic.html */
void gp2x_printf(char* fmt, ...)
{
	int i,c;
	char strOut[4096];
	char str[41];
	va_list marker;

	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);

	gp2x_frontend_init();

	c=0;
	for (i=0;i<strlen(strOut);i++)
	{
	    str[c]=strOut[i];
	    if (str[c]=='\n')
	    {
	        str[c]=0;
	        gp2x_text_log(str);
	        c=0;
	    }
	    else if (c==39)
	    {
	        str[40]=0;
	        gp2x_text_log(str);
	        c=0;
	    }
	    else
	    {
	        c++;
	    }
	}

	FE_DisplayScreen();
	sleep(6);	
	gp2x_frontend_deinit();

	pflog=0;

}
