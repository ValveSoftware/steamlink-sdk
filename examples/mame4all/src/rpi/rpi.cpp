/***************************************************************************

  osdepend.c

  OS dependant stuff (display handling, keyboard scan...)
  This is the only file which should me modified in order to port the
  emulator to a different system.

***************************************************************************/

#include "driver.h"
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include "stdarg.h"
#include "string.h"
#include "minimal.h"

struct fe_driver {
    char description[128];
    char name[16];
    char exe[16];
    int cores; /* ASM cores: 0=None,1=Cyclone,2=DrZ80,3=Cyclone+DrZ80,4=DrZ80(snd),5=Cyclone+DrZ80(snd) */
    int available;
};

#define NUMGAMES 2270
extern fe_driver fe_drivers[NUMGAMES];

int  msdos_init_sound(void);
void msdos_init_input(void);
void msdos_shutdown_sound(void);
void msdos_shutdown_input(void);
int  frontend_help (int argc, char **argv);
void parse_cmdline (int argc, char **argv, int game);
void init_inpdir(void);

void frontend_gui (char *gamename, int first_run);

int kiosk_mode;

static FILE *errorlog;

/* put here anything you need to do when the program is started. Return 0 if */
/* initialization was successful, nonzero otherwise. */
int osd_init(void)
{
	if (msdos_init_sound())
		return 1;
	msdos_init_input();
	return 0;
}


/* put here cleanup routines to be executed when the program is terminated. */
void osd_exit(void)
{
	msdos_shutdown_sound();
	msdos_shutdown_input();
}

/* fuzzy string compare, compare short string against long string        */
/* e.g. astdel == "Asteroids Deluxe". The return code is the fuzz index, */
/* we simply count the gaps between maching chars.                       */
int fuzzycmp (const char *s, const char *l)
{
	int gaps = 0;
	int match = 0;
	int last = 1;

	for (; *s && *l; l++)
	{
		if (*s == *l)
			match = 1;
		else if (*s >= 'a' && *s <= 'z' && (*s - 'a') == (*l - 'A'))
			match = 1;
		else if (*s >= 'A' && *s <= 'Z' && (*s - 'A') == (*l - 'a'))
			match = 1;
		else
			match = 0;

		if (match)
			s++;

		if (match != last)
		{
			last = match;
			if (!match)
				gaps++;
		}
	}

	/* penalty if short string does not completely fit in */
	for (; *s; s++)
		gaps++;

	return gaps;
}


int main (int argc, char **argv)
{
	int res, i, j = 0, game_index;
    char *playbackname = NULL;
    char gamenameselection[32];
    int use_cyclone=1;
    int use_drz80_save=0;
   	int use_drz80_snd_save=1;
    int use_drz80;
   	int use_drz80_snd;
    extern int video_scale;
	extern int video_border;
	extern int video_aspect;
	extern int throttle;
	extern int soundcard;
    int use_gui=0;
    int first_run=1;

    char abspath[1000];

	kiosk_mode=0;

    realpath(argv[0], abspath);
    char *dirsep = strrchr(abspath, '/');
    if( dirsep != 0 ) *dirsep = 0;
	chdir(abspath);
    
	memset(&options,0,sizeof(options));

	/* these two are not available in mame.cfg */
	errorlog = 0;

	game_index = -1;

	soundcard=-1;

	for (i = 1;i < argc;i++) /* V.V_121997 */
	{
		if (strcasecmp(argv[i],"-log") == 0)
			errorlog = fopen("error.log","wa");
		if (strcasecmp(argv[i],"-nocyclone") == 0)
			use_cyclone=0;
		if (strcasecmp(argv[i],"-drz80") == 0)
			use_drz80_save=1;
		if (strcasecmp(argv[i],"-nodrz80_snd") == 0)
			use_drz80_snd_save=0;
		if (strcasecmp(argv[i],"-scale") == 0)
			video_scale=1;
		if (strcasecmp(argv[i],"-border") == 0)
			video_border=1;
		if (strcasecmp(argv[i],"-aspect") == 0)
			video_aspect=1;
		if (strcasecmp(argv[i],"-nothrottle") == 0)
			throttle=0;
		if (strcasecmp(argv[i],"-nosound") == 0)
			soundcard=0;
        if (strcasecmp(argv[i],"-playback") == 0)
		{
			i++;
			if (i < argc)  /* point to inp file name */
				playbackname = argv[i];
        	}
	}

	if (argc == 1)
		use_gui=1;

	/* check for frontend options */
	if(!use_gui) {
		res = frontend_help (argc, argv);
	
		/* if frontend options were used, return to DOS with the error code */
		if (res != 1234)
		{
			exit (res);
		}
	}

    for (j = 1; j < argc; j++)
    {
        if (argv[j][0] != '-') {
            strcpy(gamenameselection, argv[j]);
            break;
        }
    }

    if(init_SDL()==0) {
        exit(1);
    };
    
gui_loop:
    
    if(use_gui) {
		usleep(1000000/2);
        
        gp2x_joystick_clear();

		//Normally read in the game start but we need some vars
		//setting for the frontend.
		parse_cmdline (argc, argv, 1);
        
        frontend_gui(gamenameselection, first_run);
        
        first_run=0;
        
		usleep(1000000/2);
        
        //Clear input queue
        gp2x_joystick_clear();
    }
    
    /* handle playback which is not available in mame.cfg */
	init_inpdir(); /* Init input directory for opening .inp for playback */

    if (playbackname != NULL)
        options.playback = osd_fopen(playbackname,0,OSD_FILETYPE_INPUTLOG,0);

    /* check for game name embedded in .inp header */
    if (options.playback)
    {
        INP_HEADER inp_header;

        /* read playback header */
        osd_fread(options.playback, &inp_header, sizeof(INP_HEADER));

        if (!isalnum(inp_header.name[0])) /* If first byte is not alpha-numeric */
            osd_fseek(options.playback, 0, SEEK_SET); /* old .inp file - no header */
        else
        {
            for (i = 0; (drivers[i] != 0); i++) /* find game and play it */
			{
                if (strcmp(drivers[i]->name, inp_header.name) == 0)
                {
                    game_index = i;
                    printf("Playing back previously recorded game %s (%s) [press return]\n",
                        drivers[game_index]->name,drivers[game_index]->description);
                    getchar();
                    break;
                }
            }
        }
    }

	/* If not playing back a new .inp file */
    if (game_index == -1)
    {
		/* do we have a driver for this? */
        {
            for (i = 0; drivers[i] && (game_index == -1); i++)
            {
                    if (strcasecmp(gamenameselection,drivers[i]->name) == 0)
                {
                    game_index = i;
                    break;
                }
            }
        }

        if (game_index == -1)
        {
            printf("Game \"%s\" not supported\n", argv[j]);
            return 1;
        }
    }

	/* parse generic (os-independent) options */
	parse_cmdline (argc, argv, game_index);

{	/* Mish:  I need sample rate initialised _before_ rom loading for optional rom regions */
	extern int soundcard;

	if (soundcard == 0) {    /* silence, this would be -1 if unknown in which case all roms are loaded */
		Machine->sample_rate = 0; /* update the Machine structure to show that sound is disabled */
		options.samplerate=0;
	}
}

	/* handle record which is not available in mame.cfg */
	for (i = 1; i < argc; i++)
	{
		if (strcasecmp(argv[i],"-record") == 0)
		{
            i++;
			if (i < argc)
				options.record = osd_fopen(argv[i],0,OSD_FILETYPE_INPUTLOG,1);
		}
	}

    if (options.record)
    {
        INP_HEADER inp_header;

        memset(&inp_header, '\0', sizeof(INP_HEADER));
        strcpy(inp_header.name, drivers[game_index]->name);
        /* MAME32 stores the MAME version numbers at bytes 9 - 11
         * MAME DOS keeps this information in a string, the
         * Windows code defines them in the Makefile.
         */
        /*
        inp_header.version[0] = 0;
        inp_header.version[1] = VERSION;
        inp_header.version[2] = BETA_VERSION;
        */
        osd_fwrite(options.record, &inp_header, sizeof(INP_HEADER));
    }

	/* Replace M68000 by CYCLONE */
	if (use_cyclone)
	{
		for (i=0;i<MAX_CPU;i++)
		{
			int *type=(int*)&(drivers[game_index]->drv->cpu[i].cpu_type);
			if (((*type)&0xff)==CPU_M68000 || ((*type)&0xff)==CPU_M68010 )
			{
				*type=((*type)&(~0xff))|CPU_CYCLONE;
			}
		}
	}

	use_drz80_snd = use_drz80_snd_save;
	use_drz80 = use_drz80_save;	

	// Do not use the DrZ80 core for games that are listed as not compatible
	// in the frontend list
	for (i=0;i<NUMGAMES;i++)
 	{
		if (strcmp(drivers[game_index]->name,fe_drivers[i].name)==0)
		{
			/* ASM cores: 0=None,1=Cyclone,2=DrZ80,3=Cyclone+DrZ80,4=DrZ80(snd),5=Cyclone+DrZ80(snd) */
			if(fe_drivers[i].cores == 0 || fe_drivers[i].cores == 1) 
			{
				use_drz80_snd=0;
				use_drz80=0;
				break;
			}
		}
	}

#if (HAS_DRZ80)
	/* Replace Z80 by DRZ80 */
	if (use_drz80)
	{
		for (i=0;i<MAX_CPU;i++)
		{
			int *type=(int*)&(drivers[game_index]->drv->cpu[i].cpu_type);
			if (((*type)&0xff)==CPU_Z80)
			{
				*type=((*type)&(~0xff))|CPU_DRZ80;
			}
		}
	}

	/* Replace Z80 with DRZ80 only for sound CPUs */
	if (use_drz80_snd)
	{
		for (i=0;i<MAX_CPU;i++)
		{
			int *type=(int*)&(drivers[game_index]->drv->cpu[i].cpu_type);
			if ((((*type)&0xff)==CPU_Z80) && ((*type)&CPU_AUDIO_CPU))
			{
				*type=((*type)&(~0xff))|CPU_DRZ80;
			}
		}
	}
#endif

    /*
		for (i=0;i<MAX_CPU;i++)
		{
			int *type=(int*)&(drivers[game_index]->drv->cpu[i].cpu_type);
			if (((*type)&0xff)==CPU_V30)
			{
				*type=((*type)&(~0xff))|CPU_ARMV30;
			}
		}
		for (i=0;i<MAX_CPU;i++)
		{
			int *type=(int*)&(drivers[game_index]->drv->cpu[i].cpu_type);
			if (((*type)&0xff)==CPU_V33)
			{
				*type=((*type)&(~0xff))|CPU_ARMV33;
			}
		}
    */

    // Remove the mouse usage for certain games
    if ( (strcasecmp(drivers[game_index]->name,"hbarrel")==0) || (strcasecmp(drivers[game_index]->name,"hbarrelw")==0) ||
         (strcasecmp(drivers[game_index]->name,"midres")==0) || (strcasecmp(drivers[game_index]->name,"midresu")==0) ||
         (strcasecmp(drivers[game_index]->name,"midresj")==0) || (strcasecmp(drivers[game_index]->name,"tnk3")==0) ||
         (strcasecmp(drivers[game_index]->name,"tnk3j")==0) || (strcasecmp(drivers[game_index]->name,"ikari")==0) ||
         (strcasecmp(drivers[game_index]->name,"ikarijp")==0) || (strcasecmp(drivers[game_index]->name,"ikarijpb")==0) ||
         (strcasecmp(drivers[game_index]->name,"victroad")==0) || (strcasecmp(drivers[game_index]->name,"dogosoke")==0) ||
         (strcasecmp(drivers[game_index]->name,"gwar")==0) || (strcasecmp(drivers[game_index]->name,"gwarj")==0) ||
         (strcasecmp(drivers[game_index]->name,"gwara")==0) || (strcasecmp(drivers[game_index]->name,"gwarb")==0) ||
         (strcasecmp(drivers[game_index]->name,"bermudat")==0) || (strcasecmp(drivers[game_index]->name,"bermudaj")==0) ||
         (strcasecmp(drivers[game_index]->name,"bermudaa")==0) || (strcasecmp(drivers[game_index]->name,"mplanets")==0) ||
         (strcasecmp(drivers[game_index]->name,"forgottn")==0) || (strcasecmp(drivers[game_index]->name,"lostwrld")==0) ||
         (strcasecmp(drivers[game_index]->name,"gondo")==0) || (strcasecmp(drivers[game_index]->name,"makyosen")==0) ||
         (strcasecmp(drivers[game_index]->name,"topgunr")==0) || (strcasecmp(drivers[game_index]->name,"topgunbl")==0) ||
         (strcasecmp(drivers[game_index]->name,"tron")==0) || (strcasecmp(drivers[game_index]->name,"tron2")==0) ||
         (strcasecmp(drivers[game_index]->name,"kroozr")==0) ||(strcasecmp(drivers[game_index]->name,"crater")==0) ||
         (strcasecmp(drivers[game_index]->name,"dotron")==0) || (strcasecmp(drivers[game_index]->name,"dotrone")==0) ||
         (strcasecmp(drivers[game_index]->name,"zwackery")==0) || (strcasecmp(drivers[game_index]->name,"ikari3")==0) ||
         (strcasecmp(drivers[game_index]->name,"searchar")==0) || (strcasecmp(drivers[game_index]->name,"sercharu")==0) ||
         (strcasecmp(drivers[game_index]->name,"timesold")==0) || (strcasecmp(drivers[game_index]->name,"timesol1")==0) ||
         (strcasecmp(drivers[game_index]->name,"btlfield")==0) || (strcasecmp(drivers[game_index]->name,"aztarac")==0))
    {
        extern int use_mouse;
        use_mouse=0;
    }

    /* go for it */
    printf ("%s (%s)...\n",drivers[game_index]->description,drivers[game_index]->name);
    res = run_game (game_index);

	/* close open files */
	if (errorlog) fclose (errorlog);
	if (options.playback) osd_fclose (options.playback);
	if (options.record)   osd_fclose (options.record);
	if (options.language_file) osd_fclose (options.language_file);

	gp2x_deinit();
    
	game_index = -1;

	if(use_gui) goto gui_loop;
    
	deinit_SDL();
    
	exit (res);
}

void CLIB_DECL logerror(const char *text,...)
{
	if (errorlog)
	{
		va_list arg;
		va_start(arg,text);
		vfprintf(errorlog,text,arg);
		va_end(arg);
	}
}
