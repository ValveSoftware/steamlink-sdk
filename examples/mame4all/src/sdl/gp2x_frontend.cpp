#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "minimal.h"
#include "gp2x_frontend_list.h"
#include <SDL.h>

int game_num_avail=0;
static int last_game_selected=0;
char playemu[16] = "mame\0";
char playgame[16] = "builtinn\0";

int gp2x_freq=200;
int gp2x_video_depth=8;
int gp2x_video_aspect=0;
int gp2x_video_sync=0;
int gp2x_frameskip=-1;
int gp2x_sound = 14;
int gp2x_volume = 3;
int gp2x_clock_cpu=80;
int gp2x_clock_sound=80;
int gp2x_cpu_cores=1;
int gp2x_ramtweaks=1;
int gp2x_cheat=0;

static unsigned short *gp2xmenu_bmp;
static unsigned short *gp2xsplash_bmp;

#define MAXFAVS 1000
static char favarray[MAXFAVS][9];

enum
{
	A_1,
	START_1,
	SELECT_1,
	UP_1,
	DOWN_1,
	LEFT_1,
	RIGHT_1,
	QUIT,
	NUMKEYS
};

void open_config_file(void);
void close_config_file(void);
int get_int (char *section, char *option, char *shortcut, int def);
int is_joy_button_pressed (int button, int ExKey);

static void load_bmp_16bpp(uint32_t *out, unsigned short *in)
{
 	int x,y;

	//Load bitmap, file will be flipped y so invert
	in+=(640*480)-1;
 	for (y=479;y!=-1;y--) {
		for (x=0;x<640;++x) {
			out[x] = gp2x_col16to32(in[x]);
		}
		out+=640;
		in-=640;
	}
}

#pragma pack(2) // Align
typedef struct                       /**** BMP file header structure ****/
{
    unsigned short bfType;           /* Magic number for file */
    unsigned int   bfSize;           /* Size of file */
    unsigned short bfReserved1;      /* Reserved */
    unsigned short bfReserved2;      /* ... */
    unsigned int   bfOffBits;        /* Offset to bitmap data */
} BITMAPFILEHEADER;
#pragma pack()

static void gp2x_intro_screen(int first_run) {
	char name[256];
	FILE *f;
	BITMAPFILEHEADER h;

	FE_DisplayScreen();

	sprintf(name,"skins/rpisplash16.bmp");

	f=fopen(name,"rb");
	if (f) {
		//Read header to find where to skip to for bitmap
        fread(&h, sizeof(BITMAPFILEHEADER), 1, f); //reading the FILEHEADER
		fseek(f, h.bfOffBits, SEEK_SET);

		fread(gp2xsplash_bmp,1,1000000,f);
		fclose(f);
	} 	
	else {
		printf("\nERROR: Splash screen missing from skins directory\n");
		gp2x_exit();
	}

	if(first_run) {
		load_bmp_16bpp(gp2x_screen32,gp2xsplash_bmp);
		FE_DisplayScreen();
		sleep(1);
	}
	
	sprintf(name,"skins/rpimenu16.bmp");
	f=fopen(name,"rb");
	if (f) {
		//Read header to find where to skip to for bitmap
        fread(&h, sizeof(BITMAPFILEHEADER), 1, f); //reading the FILEHEADER
		fseek(f, h.bfOffBits+sizeof(h), SEEK_SET);

		fread(gp2xmenu_bmp,1,1000000,f);
		fclose(f);
	} 
	else {
		printf("\nERROR: Menu screen missing from skins directory\n");
		gp2x_exit();
	}
}

static void favorites_read(void)
{
    //SQ: Read the favorites list from the favorites.ini file
    FILE *file;
    int counter=0;

    favarray[0][0] = '\0';

    file = fopen ("folders/Favorites.ini", "r");
    if ( file != NULL )
    {
        char line[256]; /* or other suitable maximum line size */

        while ( fgets(line, sizeof line, file) != NULL ) /* read a line */
        {
            if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
            if (line[strlen(line) - 1] == '\r') line[strlen(line) - 1] = '\0';

            if (line[0] == '[' || line[0] == '\0' || strlen(line) > 8) continue;

            //Everything checks out so stick the line in the array
            strcpy(favarray[counter++], line);
            if(counter == MAXFAVS-2) break;
        }
        fclose ( file );
        favarray[counter][0] = '\0';
    }
}

static void favorites_remove(char *game)
{
    //SQ: Scan through the favorites file and remove
    //the requested line, creating a new file.
    FILE *file, *file2;

    file = fopen ("folders/Favorites.ini", "r");
    file2 = fopen ("folders/Favorites.ini.new", "w");
    if ( file != NULL && file2 != NULL) {
        char line[256];
        char line2[256];

        while ( fgets(line, sizeof line, file) != NULL ) { /* read a line */
            strcpy(line2, line);

            if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
            if (line[strlen(line) - 1] == '\r') line[strlen(line) - 1] = '\0';

            if (line[0] != '[' && line[0] != '\0' && strlen(line) <= 8) {
                if (strcmp(line, game) == 0) {
                    continue;
                }
            }
            fputs(line2, file2);

        }
        fclose (file);
        fclose (file2);

        //Move the new file over the old one.
        rename("folders/Favorites.ini.new", "folders/Favorites.ini");
    }
   
    //SQ:All done so re-read the favorites array
    favorites_read();
}

static void favorites_add(char *game)
{
    //SQ:Add the game to the favorites file
    FILE *file;

    //SQ:Make sure the directory exists before creating a new file
    mkdir("folders", 0777);

    file = fopen("folders/Favorites.ini", "a");
    if (file != NULL) {
        fputs(game, file);
        fputc('\n', file);
        fclose(file);
    }

    //SQ:All done so re-read the favorites array
    favorites_read();
}

static void game_list_init_nocache(void)
{
	int i, indx;
	FILE *f;

	extern char **rompathv;
	extern int rompathc;

	//SQ: read the favorites
	favorites_read();

    for( indx = 0; indx < rompathc ; ++indx )
    {
        const char *dir_name = rompathv[indx];

		//sq DIR *d=opendir("roms");
		DIR *d=opendir(dir_name);
		char game[32];
		if (d)
		{
			struct dirent *actual=readdir(d);
			while(actual)
			{
				for (i=0;i<NUMGAMES;i++)
				{
					if (fe_drivers[i].available==0)
					{
						sprintf(game,"%s.zip",fe_drivers[i].name);
						if (strcmp(actual->d_name,game)==0)
						{
							fe_drivers[i].available=1;
							game_num_avail++;
							break;
						}
					}
				}
				actual=readdir(d);
			}
			closedir(d);
		}
	}
	
	if (game_num_avail)
	{
		remove("frontend/mame.lst");
		sync();
		f=fopen("frontend/mame.lst","w");
		if (f)
		{
			for (i=0;i<NUMGAMES;i++)
			{
				fputc(fe_drivers[i].available,f);
			}
			fclose(f);
			sync();
		}
	}
}

static void game_list_init_cache(void)
{
	FILE *f;
	int i;

	//SQ: read the favorites
	favorites_read();

	f=fopen("frontend/mame.lst","r");
	if (f)
	{
		for (i=0;i<NUMGAMES;i++)
		{
			fe_drivers[i].available=fgetc(f);
			if (fe_drivers[i].available)
				game_num_avail++;
		}
		fclose(f);
	}
	else
		game_list_init_nocache();
}

static void game_list_init(int argc)
{
	if (argc==1)
		game_list_init_nocache();
	else
		game_list_init_cache();
}

static void game_list_view(int *pos) {

	int i;
	int view_pos;
	int aux_pos=0;
	int screen_y = 45;
	int screen_x = 38;

	/* Draw background image */
	load_bmp_16bpp(gp2x_screen32,gp2xmenu_bmp);

	/* Check Limits */
	if (*pos<0)
		*pos=game_num_avail-1;
	if (*pos>(game_num_avail-1))
		*pos=0;
					   
	/* Set View Pos */
	if (*pos<10) {
		view_pos=0;
	} else {
		if (*pos>game_num_avail-11) {
			view_pos=game_num_avail-21;
			view_pos=(view_pos<0?0:view_pos);
		} else {
			view_pos=*pos-10;
		}
	}

	/* Show List */
	for (i=0;i<NUMGAMES;i++) {
		if (fe_drivers[i].available==1) {
			if (aux_pos>=view_pos && aux_pos<=view_pos+20) {
                //Check if the game is a favorite
                int foundfav=0;
                int counter=0;
                while(true) {
                    if (favarray[counter][0] == '\0') break;    //Null is the array terminator
                    if (strcasecmp(favarray[counter], fe_drivers[i].name) == 0) {
                        foundfav=1;
                        break;
                    }
                    counter++;
                }

				if (aux_pos==*pos) {
					if(foundfav) 
						gp2x_gamelist_text_out( screen_x, screen_y, fe_drivers[i].description, gp2x_color32(50,255,50));
					else
						gp2x_gamelist_text_out( screen_x, screen_y, fe_drivers[i].description, gp2x_color32(0,150,255));
					gp2x_gamelist_text_out( screen_x-10, screen_y,">",gp2x_color32(255,255,255) );
					gp2x_gamelist_text_out( screen_x-13, screen_y-1,"-",gp2x_color32(255,255,255) );
				}
				else {
					if(foundfav) 
						gp2x_gamelist_text_out( screen_x, screen_y, fe_drivers[i].description, gp2x_color32(50,255,50));
					else
						gp2x_gamelist_text_out( screen_x, screen_y, fe_drivers[i].description, gp2x_color32(255,255,255));
				}
				
				screen_y+=8;
			}
			aux_pos++;
		}
	}
}

static void game_list_select (int index, char *game, char *emu) {
	int i;
	int aux_pos=0;
	for (i=0;i<NUMGAMES;i++)
	{
		if (fe_drivers[i].available==1)
		{
			if(aux_pos==index)
			{
				strcpy(game,fe_drivers[i].name);
				strcpy(emu,fe_drivers[i].exe);
				gp2x_cpu_cores=fe_drivers[i].cores;
				break;
			}
			aux_pos++;
		}
	}
}

static char *game_list_description (int index)
{
	int i;
	int aux_pos=0;
	for (i=0;i<NUMGAMES;i++) {
		if (fe_drivers[i].available==1) {
			if(aux_pos==index) {
				return(fe_drivers[i].description);
			}
			aux_pos++;
		   }
	}
	return ((char *)0);
}


extern unsigned long ExKey1;

extern int osd_is_sdlkey_pressed(int inkey);
extern int is_joy_axis_pressed (int axis, int dir, int joynum);

static int pi_key[NUMKEYS];
static int pi_joy[NUMKEYS];
int joyaxis_LR, joyaxis_UD;

static void select_game(char *emu, char *game)
{
	extern int kiosk_mode;

	unsigned long keytimer=0;
	int keydirection=0, last_keydirection=0;

	/* No Selected game */
	strcpy(game,"builtinn");

	/* Clean screen */
	FE_DisplayScreen();

	gp2x_joystick_clear();	

	/* Wait until user selects a game */
	while(1)
	{
		game_list_view(&last_game_selected);
		FE_DisplayScreen();
       	gp2x_timer_delay(70000);

		while(1)
		{
            usleep(10000);
			gp2x_joystick_read();	

			last_keydirection=keydirection;
			keydirection=0;

			//Any keyboard key pressed?
			if(osd_is_sdlkey_pressed(pi_key[LEFT_1]) || osd_is_sdlkey_pressed(pi_key[RIGHT_1]) ||
			   osd_is_sdlkey_pressed(pi_key[UP_1]) || osd_is_sdlkey_pressed(pi_key[DOWN_1]) )
			{
				keydirection=1;
				break;
			}

			if(osd_is_sdlkey_pressed(pi_key[START_1]) || osd_is_sdlkey_pressed(pi_key[A_1]) ||
			   osd_is_sdlkey_pressed(pi_key[QUIT]) || osd_is_sdlkey_pressed(pi_key[SELECT_1]) )
			{
				break;
			}

			//Any stick direction?
			if(is_joy_axis_pressed (joyaxis_LR, 1, 0) || is_joy_axis_pressed (joyaxis_LR, 2, 0) ||
			   is_joy_axis_pressed (joyaxis_UD, 1, 0) || is_joy_axis_pressed (joyaxis_UD, 2, 0) )
			{
				keydirection=1;
				break;
			}

			//Any joy buttons pressed?
			if (ExKey1)
			{
				break;
			}

			//Used to delay the initial key press, but 
			//once pressed and held the delay will clear
			keytimer = gp2x_timer_read() + (TICKS_PER_SEC/2);

		}

		//Key delay
		if(keydirection && last_keydirection && gp2x_timer_read() < keytimer) {
			continue;
		}

		int updown=0;
		if(is_joy_axis_pressed (joyaxis_UD, 1, 0)) {last_game_selected--; updown=1;};
		if(is_joy_axis_pressed (joyaxis_UD, 2, 0)) {last_game_selected++; updown=1;};

		// Stop diagonals on game selection
		if(!updown) {
			if(is_joy_axis_pressed (joyaxis_LR, 1, 0)) last_game_selected-=21;
			if(is_joy_axis_pressed (joyaxis_LR, 2, 0)) last_game_selected+=21;
		}

		if (osd_is_sdlkey_pressed(pi_key[UP_1])) last_game_selected--;
		if (osd_is_sdlkey_pressed(pi_key[DOWN_1])) last_game_selected++;
		if (osd_is_sdlkey_pressed(pi_key[LEFT_1])) last_game_selected-=21;
		if (osd_is_sdlkey_pressed(pi_key[RIGHT_1])) last_game_selected+=21;

		if (!kiosk_mode)
		{
			if( osd_is_sdlkey_pressed(pi_key[QUIT]) || 
			    	(is_joy_button_pressed(pi_joy[START_1], ExKey1) && is_joy_button_pressed(pi_joy[SELECT_1], ExKey1)) ) {
				gp2x_exit();
			}
		}

		if (is_joy_button_pressed(pi_joy[A_1], ExKey1) || 
			osd_is_sdlkey_pressed(pi_key[A_1]) || 
			osd_is_sdlkey_pressed(pi_key[START_1]) )
		{
			/* Select the game */
			game_list_select(last_game_selected, game, emu);

			break;
		}

		if (is_joy_button_pressed(pi_joy[SELECT_1], ExKey1) || osd_is_sdlkey_pressed(pi_key[SELECT_1]) )
		{
           //Check if the game is already a favorite
            game_list_select(last_game_selected, game, emu);

            int foundfav=0;
            int counter=0;
            while(true) {
                if (favarray[counter][0] == '\0') break;    //Null is the array terminator
                if (strcasecmp(favarray[counter], game) == 0) {
                    foundfav=1;
                    break;
                }
                counter++;
            }

            if(foundfav) {
                favorites_remove(game);
            } else {
                favorites_add(game);
            }

			//Redraw and pause slightly
	        game_list_view(&last_game_selected);
			FE_DisplayScreen();
			usleep(300000);
			gp2x_joystick_clear();

		}
	}
}

void frontend_gui (char *gamename, int first_run)
{
	FILE *f;

	/* GP2X Initialization */
	gp2x_frontend_init();

	gp2xmenu_bmp = (unsigned short*)calloc(1, 1000000);
	gp2xsplash_bmp = (unsigned short*)calloc(1, 1000000);

	/* Show load bitmaps and show intro screen */
    gp2x_intro_screen(first_run);

	/* Initialize list of available games */
	game_list_init(1);
	if (game_num_avail==0)
	{
		/* Draw background image */
    	load_bmp_16bpp(gp2x_screen32,gp2xmenu_bmp);
		gp2x_gamelist_text_out(35, 110, "ERROR: NO AVAILABLE GAMES FOUND",gp2x_color32(255,255,255));
		FE_DisplayScreen();
		sleep(5);
		gp2x_exit();
	}

	/* Read default configuration */
	f=fopen("frontend/mame.cfg","r");
	if (f) {
		fscanf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",&gp2x_freq,&gp2x_video_depth,&gp2x_video_aspect,&gp2x_video_sync,
		&gp2x_frameskip,&gp2x_sound,&gp2x_clock_cpu,&gp2x_clock_sound,&gp2x_cpu_cores,&gp2x_ramtweaks,&last_game_selected,&gp2x_cheat,&gp2x_volume);
		fclose(f);
	}

	//Read joystick configuration
	memset(pi_key, 0, sizeof(pi_key));
	memset(pi_joy, 0, sizeof(pi_key));
	open_config_file();	

	pi_key[START_1] = get_int("frontend", "K_START",    NULL, SDL_SCANCODE_RETURN);
	pi_key[SELECT_1] = get_int("frontend", "K_SELECT",    NULL, SDL_SCANCODE_5);
	pi_key[LEFT_1] = get_int("frontend", "K_LEFT",    NULL, SDL_SCANCODE_LEFT);
	pi_key[RIGHT_1] = get_int("frontend", "K_RIGHT",    NULL, SDL_SCANCODE_RIGHT);
	pi_key[UP_1] = get_int("frontend", "K_UP",    NULL, SDL_SCANCODE_UP);
	pi_key[DOWN_1] = get_int("frontend", "K_DOWN",    NULL, SDL_SCANCODE_DOWN);
	pi_key[A_1] = get_int("frontend", "K_A",    NULL, SDL_SCANCODE_LCTRL);
	pi_key[QUIT] = get_int("frontend", "K_QUIT",    NULL, SDL_SCANCODE_ESCAPE);

	pi_joy[START_1] = get_int("frontend", "J_START",    NULL, 9);
	pi_joy[SELECT_1] = get_int("frontend", "J_SELECT",    NULL, 8);
	pi_joy[A_1] = get_int("frontend", "J_A",    NULL, 3);

    //Read joystick axis to use, default to 0 & 1
    joyaxis_LR = get_int("frontend", "AXIS_LR", NULL, 0);
    joyaxis_UD = get_int("frontend", "AXIS_UD", NULL, 1);

	close_config_file();

	
	/* Select Game */
	select_game(playemu,playgame); 

	/* Write default configuration */
	f=fopen("frontend/mame.cfg","w");
	if (f) {
		fprintf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",gp2x_freq,gp2x_video_depth,gp2x_video_aspect,gp2x_video_sync,
		gp2x_frameskip,gp2x_sound,gp2x_clock_cpu,gp2x_clock_sound,gp2x_cpu_cores,gp2x_ramtweaks,last_game_selected,gp2x_cheat,gp2x_volume);
		fclose(f);
		sync();
	}
	
    strcpy(gamename, playgame);
    
    gp2x_frontend_deinit();

	free(gp2xmenu_bmp);
	free(gp2xsplash_bmp);
	
}
