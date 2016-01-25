/*
 * Configuration routines.
 *
 * 19971219 support for mame.cfg by Valerio Verrando
 * 19980402 moved out of msdos.c (N.S.), generalized routines (BW)
 * 19980917 added a "-cheatfile" option (misc) in MAME.CFG      JCK
 */

#include "driver.h"
#include <ctype.h>
#ifdef HAVE_GLIB
#include <glib.h>
#else
#include "glib_ini_file.h"
#endif

/* from video.c */
extern int frameskip,autoframeskip;
extern int video_sync, wait_vsync;
extern int use_dirty;
extern int skiplines, skipcolumns;
extern float osd_gamma_correction;
extern int gfx_width, gfx_height;

/* from sound.c */
extern int soundcard, usestereo, attenuation;

/* from input.c */
extern int use_mouse, joystick;

/* from cheat.c */
extern char *cheatfile;

/* from datafile.c */
extern char *history_filename,*mameinfo_filename;

/* from fileio.c */
void decompose_rom_sample_path (char *rompath, char *samplepath);
extern char *nvdir, *hidir, *cfgdir, *inpdir, *stadir, *memcarddir;
extern char *artworkdir, *screenshotdir, *alternate_name;

extern char *cheatdir;

/*from video.c flag for 15.75KHz modes (req. for 15.75KHz Arcade Monitor Modes)*/
extern int arcade_mode;

static int mame_argc;
static char **mame_argv;
static int game;
char *rompath, *samplepath;

int underclock_sound=0;
int underclock_cpu=0;
int fast_sound=0;

/* from minimal.c */
extern int rotate_controls;

static GKeyFile *gkeyfile=0;

void open_config_file(void)
{
	GError *error = NULL;

	if (!gkeyfile)
	{
		gkeyfile = g_key_file_new();
		if (!(int)g_key_file_load_from_file (gkeyfile, "mame.cfg", G_KEY_FILE_NONE, &error))
		{
		    gkeyfile=0;
		}
	}
}

void close_config_file(void)
{
	// Leave it open for other config queries
	//g_key_file_free(gkeyfile);
}

/*
 * gets some boolean config value.
 * 0 = false, >0 = true, <0 = auto
 * the shortcut can only be used on the commandline
 */
static int get_bool (char *section, char *option, char *shortcut, int def)
{
	GError *error=NULL;
	char *yesnoauto;
	int res, i;

	res = def;

	/* look into mame.cfg, [section] */
	if(gkeyfile) 
	{
		yesnoauto = g_key_file_get_string(gkeyfile, section, option, &error);

		/* also take numerical values instead of "yes", "no" and "auto" */
		if (!error) //return value if not found is null
		{
		    if      (strcasecmp(yesnoauto, "no"  ) == 0) res = 0;
		    else if (strcasecmp(yesnoauto, "yes" ) == 0) res = 1;
		    else if (strcasecmp(yesnoauto, "auto") == 0) res = -1;
		    else    res = atoi (yesnoauto);
		}
	}

	/* check the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-') continue;
		/* look for "-option" */
		if (strcasecmp(&mame_argv[i][1], option) == 0)
			res = 1;
		/* look for "-shortcut" */
		if (shortcut && (strcasecmp(&mame_argv[i][1], shortcut) == 0))
			res = 1;
		/* look for "-nooption" */
		if (strncasecmp(&mame_argv[i][1], "no", 2) == 0)
		{
			if (strcasecmp(&mame_argv[i][3], option) == 0)
				res = 0;
			if (shortcut && (strcasecmp(&mame_argv[i][3], shortcut) == 0))
				res = 0;
		}
		/* look for "-autooption" */
		if (strncasecmp(&mame_argv[i][1], "auto", 4) == 0)
		{
			if (strcasecmp(&mame_argv[i][5], option) == 0)
				res = -1;
			if (shortcut && (strcasecmp(&mame_argv[i][5], shortcut) == 0))
				res = -1;
		}
	}
	return res;
}

int get_int (char *section, char *option, char *shortcut, int def)
{
	int res,i;
	GError *error=NULL;

	int tempint;

	res = def;

	/* look into mame.cfg, [section] */
	if(gkeyfile) 
	{
		tempint = g_key_file_get_integer(gkeyfile, section, option, &error);
		if (!error)
		{
			res = tempint;
		}
	}

	/* get it from the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-')
			continue;
		if ((strcasecmp(&mame_argv[i][1], option) == 0) ||
			(shortcut && (strcasecmp(&mame_argv[i][1], shortcut ) == 0)))
		{
			i++;
			if (i < mame_argc) res = atoi (mame_argv[i]);
		}
	}
	return res;
}

static float get_float (char *section, char *option, char *shortcut, float def)
{
	GError *error=NULL;
	int i;
	float res;
	double tempdouble=0;

	res = def;

	/* look into mame.cfg, [section] */
	if(gkeyfile) 
	{
		tempdouble = g_key_file_get_double(gkeyfile, section, option, &error);
		if (!error)
		{
		    res = (float)tempdouble;
		}
	}

	/* get it from the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-')
			continue;
		if ((strcasecmp(&mame_argv[i][1], option) == 0) ||
			(shortcut && (strcasecmp(&mame_argv[i][1], shortcut ) == 0)))
		{
			i++;
			if (i < mame_argc) res = atof (mame_argv[i]);
		}
	}
	return res;
}

static char *get_string (char *section, char *option, char *shortcut, char *def)
{
	GError *error=NULL;
	char *res;
	char *tempstr;
	int i;

	res = def;

	if(gkeyfile) 
	{
		tempstr = g_key_file_get_string(gkeyfile, section, option, &error);
		if (!error)
		{
		    res = tempstr;
		}
	}

	/* get it from the commandline */
	for (i = 1; i < mame_argc; i++)
	{
		if (mame_argv[i][0] != '-')
			continue;

		if ((strcasecmp(&mame_argv[i][1], option) == 0) ||
			(shortcut && (strcasecmp(&mame_argv[i][1], shortcut)  == 0)))
		{
			i++;
			if (i < mame_argc) res = mame_argv[i];
		}
	}
	return res;
}

void get_rom_sample_path (int argc, char **argv, int game_index)
{
	int i;

	alternate_name = 0;
	mame_argc = argc;
	mame_argv = argv;
	game = game_index;

	rompath    = get_string ("directory", "rompath",    NULL, ".;roms");
	samplepath = get_string ("directory", "samplepath", NULL, ".;samples");

	/* handle '-romdir' hack. We should get rid of this BW */
	alternate_name = 0;
	for (i = 1; i < argc; i++)
	{
		if (strcasecmp (argv[i], "-romdir") == 0)
		{
			i++;
			if (i < argc) alternate_name = argv[i];
		}
	}

	/* decompose paths into components (handled by fileio.c) */
	decompose_rom_sample_path (rompath, samplepath);
}

/* for playback of .inp files */
void init_inpdir(void)
{
	inpdir = get_string ("directory", "inp",     NULL, "inp");
}

void parse_cmdline (int argc, char **argv, int game_index)
{
	static float f_beam, f_flicker;
	char *resolution;
	char *joyname;
	char tmpres[10];
	int i;
	char *tmpstr;

	extern int kiosk_mode;

	mame_argc = argc;
	mame_argv = argv;
	game = game_index;

	//Open mame.cfg file for scanning options
	open_config_file();

	/* read graphic configuration */
	options.use_artwork = get_bool   ("config", "artwork",	NULL,  1);
	options.use_samples = get_bool   ("config", "samples",	NULL,  1);
	video_sync  = get_bool   ("config", "vsync",        NULL,  0);
	wait_vsync  = get_bool   ("config", "waitvsync",    NULL,  0);
	use_dirty	= get_bool	 ("config", "dirty",	NULL,	0);
	options.antialias   = get_bool   ("config", "antialias",    NULL,  1);
	options.translucency = get_bool    ("config", "translucency", NULL, 1);

	//sq tmpstr             = get_string ("config", "depth", NULL, "auto");
	//sq options.color_depth = atoi(tmpstr);
	//sq if (options.color_depth != 8 && options.color_depth != 16) options.color_depth = 0;	/* auto */

	//SQ Force 8 bit only
	//sq options.color_depth=8;	
	options.color_depth=0;	

	skiplines   = get_int    ("config", "skiplines",    NULL, 0);
	skipcolumns = get_int    ("config", "skipcolumns",  NULL, 0);
	f_beam      = get_float  ("config", "beam",         NULL, 1.0);
	if (f_beam < 1.0) f_beam = 1.0;
	if (f_beam > 16.0) f_beam = 16.0;
	f_flicker   = get_float  ("config", "flicker",      NULL, 0.0);
	if (f_flicker < 0.0) f_flicker = 0.0;
	if (f_flicker > 100.0) f_flicker = 100.0;
	osd_gamma_correction = get_float ("config", "gamma",   NULL, 1.0);
	if (osd_gamma_correction < 0.5) osd_gamma_correction = 0.5;
	if (osd_gamma_correction > 2.0) osd_gamma_correction = 2.0;

	tmpstr = get_string ("config", "frameskip", "fs", "auto");
	if (!strcasecmp(tmpstr,"auto"))
	{
		frameskip = 0;
		autoframeskip = 1;
	}
	else
	{
		frameskip = atoi(tmpstr);
		autoframeskip = 0;
	}
	options.norotate  = get_bool ("config", "norotate",  NULL, 0);
	options.ror       = get_bool ("config", "ror",       NULL, 0);
	options.rol       = get_bool ("config", "rol",       NULL, 0);
	options.flipx     = get_bool ("config", "flipx",     NULL, 0);
	options.flipy     = get_bool ("config", "flipy",     NULL, 0);

	/* read sound configuration */
//sq	soundcard           = get_int  ("config", "soundcard",  NULL, -1);
	options.use_emulated_ym3812 = !get_bool ("config", "ym3812opl",  NULL,  0);
	options.samplerate = get_int  ("config", "samplerate", "sr", 44100);
	if (options.samplerate < 5000) options.samplerate = 5000;
	if (options.samplerate > 44100) options.samplerate = 44100;
	usestereo           = get_bool ("config", "stereo",  NULL,  1);
	attenuation         = get_int  ("config", "volume",  NULL,  0);
	if (attenuation < -32) attenuation = -32;
	if (attenuation > 0) attenuation = 0;

	options.force_stereo = get_bool ("config", "force_stereo",  NULL,  0);

	/* read input configuration */
	use_mouse = get_bool   ("config", "mouse",   NULL,  1);
	joyname   = get_string ("config", "joystick", "joy", "standard");
	joystick = (strcmp(joyname, "none") != 0);

	/* misc configuration */
	options.cheat      = get_bool ("config", "cheat", NULL, 0);
	options.mame_debug = get_bool ("config", "debug", NULL, 0);

	cheatfile  = get_string ("config", "cheatfile", "cf", "cheat.dat");

 	history_filename  = get_string ("config", "historyfile", NULL, "history.dat");    /* JCK 980917 */

	mameinfo_filename  = get_string ("config", "mameinfofile", NULL, "mameinfo.dat");    /* JCK 980917 */

	/* get resolution */
	resolution  = get_string ("config", "resolution", NULL, "auto");

	//vector games
	options.vector_width = get_int ("config", "vector_width", NULL, 640);
	options.vector_height = get_int ("config", "vector_height", NULL, 480);

	/* set default subdirectories */
	nvdir      = get_string ("directory", "nvram",   NULL, "nvram");
	hidir      = get_string ("directory", "hi",      NULL, "hi");
	cfgdir     = get_string ("directory", "cfg",     NULL, "cfg");
	screenshotdir = get_string ("directory", "snap",     NULL, "snap");
	memcarddir = get_string ("directory", "memcard", NULL, "memcard");
	stadir     = get_string ("directory", "sta",     NULL, "sta");
	artworkdir = get_string ("directory", "artwork", NULL, "artwork");

	cheatdir = get_string ("directory", "cheat", NULL, ".");

	logerror("cheatfile = %s - cheatdir = %s\n",cheatfile,cheatdir);

	tmpstr = get_string ("config", "language", NULL, "english");
	options.language_file = osd_fopen(0,tmpstr,OSD_FILETYPE_LANGUAGE,0);

	/* this is handled externally cause the audit stuff needs it, too */
	get_rom_sample_path (argc, argv, game_index);

	/* process some parameters */
	options.beam = (int)(f_beam * 0x00010000);
	if (options.beam < 0x00010000)
		options.beam = 0x00010000;
	if (options.beam > 0x00100000)
		options.beam = 0x00100000;

	options.flicker = (int)(f_flicker * 2.55);
	if (options.flicker < 0)
		options.flicker = 0;
	if (options.flicker > 255)
		options.flicker = 255;

	/* any option that starts with a digit is taken as a resolution option */
	/* this is to handle the old "-wxh" commandline option. */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-' && isdigit(argv[i][1]) &&
				(strstr(argv[i],"x") || strstr(argv[i],"X")))
			resolution = &argv[i][1];
	}

	/* break up resolution into gfx_width and gfx_height */
	gfx_height = gfx_width = 0;
	if (strcasecmp (resolution, "auto") != 0)
	{
		char *tmp;
		strncpy (tmpres, resolution, 10);
		tmp = strtok (tmpres, "xX");
		gfx_width = atoi (tmp);
		tmp = strtok (0, "xX");
		if (tmp)
			gfx_height = atoi (tmp);

		options.vector_width = gfx_width;
		options.vector_height = gfx_height;
	}

	/* Underclock settings */
	underclock_sound = get_int ("config", "uclocks",   NULL, 0);
	underclock_cpu   = get_int ("config", "uclock",    NULL, 10);

	/* Fast sound setting */
	fast_sound       = get_bool("config", "fastsound", NULL, 0);

	/* Rotate controls */
	rotate_controls  = get_bool("config", "rotatecontrols", NULL, 0);

	/* Display configuration */
	options.display_border = get_int ("config", "display_border", NULL, 24);
	options.display_smooth_stretch = get_bool ("config", "display_smooth_stretch", NULL, 1);
	options.display_effect = get_int ("config", "display_effect", NULL, 0);

	kiosk_mode = get_bool("config", "kioskmode", NULL, 0);

	close_config_file();
}
