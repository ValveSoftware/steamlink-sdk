#ifndef OSDEPEND_H
#define OSDEPEND_H

#include "osd_cpu.h"
#include "inptport.h"


/* The Win32 port requires this constant for variable arg routines. */
#ifndef CLIB_DECL
#define CLIB_DECL
#endif

#ifdef __LP64__
#define FPTR long   /* 64bit: sizeof(void *) is sizeof(long)  */
#else
#define FPTR int
#endif


int osd_init(void);
void osd_exit(void);


/******************************************************************************

  Display

******************************************************************************/

struct osd_bitmap
{
	int width,height;       /* width and height of the bitmap */
	int depth;		/* bits per pixel */
	void *_private; /* don't touch! - reserved for osdepend use */
	unsigned char **line; /* pointers to the start of each line */
};

/* VERY IMPORTANT: the function must allocate also a "safety area" 16 pixels wide all */
/* around the bitmap. This is required because, for performance reasons, some graphic */
/* routines don't clip at boundaries of the bitmap. */
struct osd_bitmap *osd_alloc_bitmap(int width,int height,int depth);
void osd_clearbitmap(struct osd_bitmap *bitmap);
void osd_free_bitmap(struct osd_bitmap *bitmap);

/*
  Create a display screen, or window, of the given dimensions (or larger). It is
  acceptable to create a smaller display if necessary, in that case the user must
  have a way to move the visibility window around.
  Attributes are the ones defined in driver.h, they can be used to perform
  optimizations, e.g. dirty rectangle handling if the game supports it, or faster
  blitting routines with fixed palette if the game doesn't change the palette at
  run time. The VIDEO_PIXEL_ASPECT_RATIO flags should be honored to produce a
  display of correct proportions.
  Orientation is the screen orientation (as defined in driver.h) which will be done
  by the core. This can be used to select thinner screen modes for vertical games
  (ORIENTATION_SWAP_XY set), or even to ask the user to rotate the monitor if it's
  a pivot model. Note that the OS dependant code must NOT perform any rotation,
  this is done entirely in the core.
  Returns 0 on success.
*/
int osd_create_display(int width,int height,int depth,int fps,int attributes,int orientation);
int osd_set_display(int width,int height,int depth,int attributes,int orientation);
void osd_close_display(void);

void osd_set_visible_area(int min_x,int max_x,int min_y,int max_y);

/*
  osd_allocate_colors() is called after osd_create_display(), to create and initialize
  the palette.
  palette is an array of 'totalcolors' R,G,B triplets. The function returns
  in *pens the pen values corresponding to the requested colors.
  When modifiable is not 0, the palette will be modified later via calls to
  osd_modify_pen(). Otherwise, the code can assume that the palette will not change,
  and activate special optimizations (e.g. direct copy for a 16-bit display).
  The function must also initialize Machine->uifont->colortable[] to get proper
  white-on-black and black-on-white text.
  Return 0 for success.
*/
int osd_allocate_colors(unsigned int totalcolors,const unsigned char *palette,unsigned short *pens,int modifiable);
void osd_modify_pen(int pen,unsigned char red, unsigned char green, unsigned char blue);
void osd_get_pen(int pen,unsigned char *red, unsigned char *green, unsigned char *blue);
void osd_mark_dirty(int xmin, int ymin, int xmax, int ymax, int ui);    /* ASG 971011 */

/*
  osd_skip_this_frame() must return 0 if the current frame will be displayed. This
  can be used by drivers to skip cpu intensive processing for skipped frames, so the
  function must return a consistent result throughout the current frame. The function
  MUST NOT check timers and dynamically determine whether to display the frame: such
  calculations must be done in osd_update_video_and_audio(), and they must affect the
  FOLLOWING frames, not the current one. At the end of osd_update_video_and_audio(),
  the code must already know exactly whether the next frame will be skipped or not.
*/
int osd_skip_this_frame(void);
void osd_update_video_and_audio(struct osd_bitmap *bitmap);
void osd_set_gamma(float _gamma);
float osd_get_gamma(void);
void osd_set_brightness(int brightness);
int osd_get_brightness(void);
void osd_save_snapshot(struct osd_bitmap *bitmap);


/******************************************************************************

  Sound

******************************************************************************/

/*
  osd_start_audio_stream() is called at the start of the emulation to initialize
  the output stream, the osd_update_audio_stream() is called every frame to
  feed new data. osd_stop_audio_stream() is called when the emulation is stopped.

  The sample rate is fixed at Machine->sample_rate. Samples are 16-bit, signed.

  When the stream is stereo, left and right samples are alternated in the
  stream.

  osd_start_audio_stream() and osd_update_audio_stream() must return the number
  of samples (or couples of samples, when using stereo) required for next frame.
  This will be around Machine->sample_rate / Machine->drv->frames_per_second,
  the code may adjust it by SMALL AMOUNTS to keep timing accurate and to maintain
  audio and video in sync when using vsync. Note that sound generation,
  especially when DACs are involved, greatly depends on the samples per frame to
  be roughly constant, so the returned value must always stay close to the
  reference value of Machine->sample_rate / Machine->drv->frames_per_second.
  Of course that value is not necessarily an integer so at least a +/- 1
  adjustment is necessary to avoid drifting over time.
 */
int osd_start_audio_stream(int stereo);
int osd_update_audio_stream(INT16 *buffer);
void osd_stop_audio_stream(void);

/*
  control master volume, attenuation is the attenuation in dB (a negative
  number).
 */
void osd_set_mastervolume(int attenuation);
int osd_get_mastervolume(void);
void osd_sound_enable(int enable);

/* direct access to the Sound Blaster OPL chip */
void osd_opl_control(int chip,int reg);
void osd_opl_write(int chip,int data);


/******************************************************************************

  Keyboard

******************************************************************************/

/*
  return a list of all available keys (see input.h)
*/
const struct KeyboardInfo *osd_get_key_list(void);

/*
  tell whether the specified key is pressed or not. keycode is the OS dependant
  code specified in the list returned by osd_get_key_list().
*/
int osd_is_key_pressed(int keycode);

/*
  wait for the user to press a key and return its code. This function is not
  required to do anything, it is here so we can avoid bogging down multitasking
  systems while using the debugger. If you don't want to or can't support this
  function you can just return OSD_KEY_NONE.
*/
int osd_wait_keypress(void);

/*
  Return the Unicode value of the most recently pressed key. This
  function is used only by text-entry routines in the user interface and should
  not be used by drivers. The value returned is in the range of the first 256
  bytes of Unicode, e.g. ISO-8859-1. A return value of 0 indicates no key down.

  Set flush to 1 to clear the buffer before entering text. This will avoid
  having prior UI and game keys leak into the text entry.
*/
int osd_readkey_unicode(int flush);

/* Code returned by the function osd_wait_keypress() if no key available */
#define OSD_KEY_NONE 0xffffffff


/******************************************************************************

  Joystick & Mouse/Trackball

******************************************************************************/

/*
  return a list of all available joystick inputs (see input.h)
*/
const struct JoystickInfo *osd_get_joy_list(void);

/*
  tell whether the specified joystick direction/button is pressed or not.
  joycode is the OS dependant code specified in the list returned by
  osd_get_joy_list().
*/
int osd_is_joy_pressed(int joycode);


/* We support 4 players for each analog control */
#define OSD_MAX_JOY_ANALOG	4
#define X_AXIS          1
#define Y_AXIS          2

void osd_poll_joysticks(void);

/* Joystick calibration routines BW 19981216 */
/* Do we need to calibrate the joystick at all? */
int osd_joystick_needs_calibration (void);
/* Preprocessing for joystick calibration. Returns 0 on success */
void osd_joystick_start_calibration (void);
/* Prepare the next calibration step. Return a description of this step. */
/* (e.g. "move to upper left") */
char *osd_joystick_calibrate_next (void);
/* Get the actual joystick calibration data for the current position */
void osd_joystick_calibrate (void);
/* Postprocessing (e.g. saving joystick data to config) */
void osd_joystick_end_calibration (void);

void osd_trak_read(int player,int *deltax,int *deltay);

/* return values in the range -128 .. 128 (yes, 128, not 127) */
void osd_analogjoy_read(int player,int *analog_x, int *analog_y);


/*
  inptport.c defines some general purpose defaults for key and joystick bindings.
  They may be further adjusted by the OS dependant code to better match the
  available keyboard, e.g. one could map pause to the Pause key instead of P, or
  snapshot to PrtScr instead of F12. Of course the user can further change the
  settings to anything he/she likes.
  This function is called on startup, before reading the configuration from disk.
  Scan the list, and change the keys/joysticks you want.
*/
void osd_customize_inputport_defaults(struct ipd *defaults);


/******************************************************************************

  File I/O

******************************************************************************/

/* inp header */
typedef struct
{
	char name[9];      /* 8 bytes for game->name + NULL */
	char version[3];   /* byte[0] = 0, byte[1] = version byte[2] = beta_version */
	char reserved[20]; /* for future use, possible store game options? */
} INP_HEADER;


/* file handling routines */
enum
{
	OSD_FILETYPE_ROM = 1,
	OSD_FILETYPE_SAMPLE,
	OSD_FILETYPE_NVRAM,
	OSD_FILETYPE_HIGHSCORE,
	OSD_FILETYPE_HIGHSCORE_DB, /* LBO 040400 */
	OSD_FILETYPE_CONFIG,
	OSD_FILETYPE_INPUTLOG,
	OSD_FILETYPE_STATE,
	OSD_FILETYPE_ARTWORK,
	OSD_FILETYPE_MEMCARD,
	OSD_FILETYPE_SCREENSHOT,
	OSD_FILETYPE_HISTORY,  /* LBO 040400 */
	OSD_FILETYPE_CHEAT,  /* LBO 040400 */
	OSD_FILETYPE_LANGUAGE, /* LBO 042400 */
#ifdef MESS
	OSD_FILETYPE_IMAGE_R,
	OSD_FILETYPE_IMAGE_RW,
#endif
	OSD_FILETYPE_end /* dummy last entry */
};

/* gamename holds the driver name, filename is only used for ROMs and    */
/* samples. If 'write' is not 0, the file is opened for write. Otherwise */
/* it is opened for read. */

int osd_faccess(const char *filename, int filetype);
void *osd_fopen(const char *gamename,const char *filename,int filetype,int read_or_write);
int osd_fread(void *file,void *buffer,int length);
int osd_fwrite(void *file,const void *buffer,int length);
int osd_fread_swap(void *file,void *buffer,int length);
int osd_fwrite_swap(void *file,const void *buffer,int length);
#if LSB_FIRST
#define osd_fread_msbfirst osd_fread_swap
#define osd_fwrite_msbfirst osd_fwrite_swap
#define osd_fread_lsbfirst osd_fread
#define osd_fwrite_lsbfirst osd_fwrite
#else
#define osd_fread_msbfirst osd_fread
#define osd_fwrite_msbfirst osd_fwrite
#define osd_fread_lsbfirst osd_fread_swap
#define osd_fwrite_lsbfirst osd_fwrite_swap
#endif
int osd_fread_scatter(void *file,void *buffer,int length,int increment);
int osd_fseek(void *file,int offset,int whence);
void osd_fclose(void *file);
void osd_fsync(void);
int osd_fchecksum(const char *gamename, const char *filename, unsigned int *length, unsigned int *sum);
int osd_fsize(void *file);
unsigned int osd_fcrc(void *file);
/* LBO 040400 - start */
int osd_fgetc(void *file);
int osd_ungetc(int c, void *file);
char *osd_fgets(char *s, int n, void *file);
int osd_feof(void *file);
int osd_ftell(void *file);
/* LBO 040400 - end */

/******************************************************************************

  Miscellaneous

******************************************************************************/

/* called while loading ROMs. It is called a last time with name == 0 to signal */
/* that the ROM loading process is finished. */
/* return non-zero to abort loading */
int osd_display_loading_rom_message(const char *name,int current,int total);

/* called when the game is paused/unpaused, so the OS dependant code can do special */
/* things like changing the title bar or darkening the display. */
/* Note that the OS dependant code must NOT stop processing input, since the user */
/* interface is still active while the game is paused. */
void osd_pause(int paused);

/* control keyboard leds or other indicators */
void osd_led_w(int led,int on);



#ifdef MAME_NET
/* network */
int osd_net_init(void);
int osd_net_send(int player, unsigned char buf[], int *size);
int osd_net_recv(int player, unsigned char buf[], int *size);
int osd_net_sync(void);
int osd_net_input_sync(void);
int osd_net_exit(void);
int osd_net_add_player(void);
int osd_net_remove_player(int player);
int osd_net_game_init(void);
int osd_net_game_exit(void);
#endif /* MAME_NET */

#ifdef MESS
int osd_num_devices(void);
const char *osd_get_device_name(int i);
void osd_change_device(const char *vol);
void *osd_dir_open(const char *mdirname, const char *filemask);
int osd_dir_get_entry(void *dir, char *name, int namelength, int *is_dir);
void osd_dir_close(void *dir);
#endif


void CLIB_DECL logerror(const char *text,...);

#endif
