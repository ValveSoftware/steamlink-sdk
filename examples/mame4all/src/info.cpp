#include <ctype.h>

#include "driver.h"
#include "sound/samples.h"
#include "info.h"
#include "datafile.h"

/* Output format indentation */

/* Indentation */
#define INDENT "\t"

/* Possible output format */
#define OUTPUT_FORMAT_UNFORMATTED 0
#define OUTPUT_FORMAT_ONE_LEVEL 1
#define OUTPUT_FORMAT_TWO_LEVEL 2

/* Output format */
#define OUTPUT_FORMAT OUTPUT_FORMAT_ONE_LEVEL

/* Output format configuration
	L list
	1,2 levels
	B,S,E Begin, Separator, End
*/

#if OUTPUT_FORMAT == OUTPUT_FORMAT_UNFORMATTED
#define L1B "("
#define L1P " "
#define L1N ""
#define L1E ")"
#define L2B "("
#define L2P " "
#define L2N ""
#define L2E ")"
#elif OUTPUT_FORMAT == OUTPUT_FORMAT_ONE_LEVEL
#define L1B " (\n"
#define L1P INDENT
#define L1N "\n"
#define L1E ")\n\n"
#define L2B " ("
#define L2P " "
#define L2N ""
#define L2E " )"
#elif OUTPUT_FORMAT == OUTPUT_FORMAT_TWO_LEVEL
#define L1B " (\n"
#define L1P INDENT
#define L1N "\n"
#define L1E ")\n\n"
#define L2B " (\n"
#define L2P INDENT INDENT
#define L2N "\n"
#define L2E INDENT ")"
#else
#error Wrong OUTPUT_FORMAT
#endif

/* Print a string in C format */
static void print_c_string(FILE* out, const char* s) {
	fprintf(out, "\"");
	if (s) {
		while (*s) {
			switch (*s) {
				case '\a' : fprintf(out, "\\a"); break;
				case '\b' : fprintf(out, "\\b"); break;
				case '\f' : fprintf(out, "\\f"); break;
				case '\n' : fprintf(out, "\\n"); break;
				case '\r' : fprintf(out, "\\r"); break;
				case '\t' : fprintf(out, "\\t"); break;
				case '\v' : fprintf(out, "\\v"); break;
				case '\\' : fprintf(out, "\\\\"); break;
				case '\"' : fprintf(out, "\\\""); break;
				default:
					if (*s>=' ' && *s<='~')
						fprintf(out, "%c", *s);
					else
						fprintf(out, "\\x%02x", (unsigned)(unsigned char)*s);
			}
			++s;
		}
	}
	fprintf(out, "\"");
}

/* Print a string in statement format (remove space, parentesis, ") */
static void print_statement_string(FILE* out, const char* s) {
	if (s) {
		while (*s) {
			if (isspace(*s)) {
				fprintf(out, "_");
			} else {
				switch (*s) {
					case '(' :
					case ')' :
					case '"' :
						fprintf(out, "_");
						break;
					default:
						fprintf(out, "%c", *s);
				}
			}
			++s;
		}
	} else {
		fprintf(out, "null");
	}
}

static void print_game_switch(FILE* out, const struct GameDriver* game) {
	const struct InputPortTiny* input = game->input_ports;

	while ((input->type & ~IPF_MASK) != IPT_END) {
		if ((input->type & ~IPF_MASK)==IPT_DIPSWITCH_NAME) {
			int def = input->default_value;
			const char* def_name = 0;

			fprintf(out, L1P "dipswitch" L2B);

			fprintf(out, L2P "name " );
			print_c_string(out,input->name);
			fprintf(out, "%s", L2N);
			++input;

			while ((input->type & ~IPF_MASK)==IPT_DIPSWITCH_SETTING) {
				if (def == input->default_value)
					def_name = input->name;
				fprintf(out, L2P "entry " );
				print_c_string(out,input->name);
				fprintf(out, "%s", L2N);
				++input;
			}

			if (def_name) {
				fprintf(out, L2P "default ");
				print_c_string(out,def_name);
				fprintf(out, "%s", L2N);
			}

			fprintf(out, L2E L1N);
		}
		else
			++input;
	}
}

static void print_game_input(FILE* out, const struct GameDriver* game) {
	const struct InputPortTiny* input = game->input_ports;
	int nplayer = 0;
	const char* control = 0;
	int nbutton = 0;
	int ncoin = 0;
	const char* service = 0;
	const char* tilt = 0;

	while ((input->type & ~IPF_MASK) != IPT_END) {
		switch (input->type & IPF_PLAYERMASK) {
			case IPF_PLAYER1:
				if (nplayer<1) nplayer = 1;
				break;
			case IPF_PLAYER2:
				if (nplayer<2) nplayer = 2;
				break;
			case IPF_PLAYER3:
				if (nplayer<3) nplayer = 3;
				break;
			case IPF_PLAYER4:
				if (nplayer<4) nplayer = 4;
				break;
		}
		switch (input->type & ~IPF_MASK) {
			case IPT_JOYSTICK_UP:
			case IPT_JOYSTICK_DOWN:
			case IPT_JOYSTICK_LEFT:
			case IPT_JOYSTICK_RIGHT:
				if (input->type & IPF_2WAY)
					control = "joy2way";
				else if (input->type & IPF_4WAY)
					control = "joy4way";
				else
					control = "joy8way";
				break;
			case IPT_JOYSTICKRIGHT_UP:
			case IPT_JOYSTICKRIGHT_DOWN:
			case IPT_JOYSTICKRIGHT_LEFT:
			case IPT_JOYSTICKRIGHT_RIGHT:
			case IPT_JOYSTICKLEFT_UP:
			case IPT_JOYSTICKLEFT_DOWN:
			case IPT_JOYSTICKLEFT_LEFT:
			case IPT_JOYSTICKLEFT_RIGHT:
				if (input->type & IPF_2WAY)
					control = "doublejoy2way";
				else if (input->type & IPF_4WAY)
					control = "doublejoy4way";
				else
					control = "doublejoy8way";
				break;
			case IPT_BUTTON1:
				if (nbutton<1) nbutton = 1;
				break;
			case IPT_BUTTON2:
				if (nbutton<2) nbutton = 2;
				break;
			case IPT_BUTTON3:
				if (nbutton<3) nbutton = 3;
				break;
			case IPT_BUTTON4:
				if (nbutton<4) nbutton = 4;
				break;
			case IPT_BUTTON5:
				if (nbutton<5) nbutton = 5;
				break;
			case IPT_BUTTON6:
				if (nbutton<6) nbutton = 6;
				break;
			case IPT_BUTTON7:
				if (nbutton<7) nbutton = 7;
				break;
			case IPT_BUTTON8:
				if (nbutton<8) nbutton = 8;
				break;
			case IPT_PADDLE:
				control = "paddle";
				break;
			case IPT_DIAL:
				control = "dial";
				break;
			case IPT_TRACKBALL_X:
			case IPT_TRACKBALL_Y:
				control = "trackball";
				break;
			case IPT_AD_STICK_X:
			case IPT_AD_STICK_Y:
				control = "stick";
				break;
			case IPT_COIN1:
				if (ncoin < 1) ncoin = 1;
				break;
			case IPT_COIN2:
				if (ncoin < 2) ncoin = 2;
				break;
			case IPT_COIN3:
				if (ncoin < 3) ncoin = 3;
				break;
			case IPT_COIN4:
				if (ncoin < 4) ncoin = 4;
				break;
			case IPT_SERVICE :
				service = "yes";
				break;
			case IPT_TILT :
				tilt = "yes";
				break;
		}
		++input;
	}

	fprintf(out, L1P "input" L2B);
	fprintf(out, L2P "players %d" L2N, nplayer );
	if (control)
		fprintf(out, L2P "control %s" L2N, control );
	if (nbutton)
		fprintf(out, L2P "buttons %d" L2N, nbutton );
	if (ncoin)
		fprintf(out, L2P "coins %d" L2N, ncoin );
	if (service)
		fprintf(out, L2P "service %s" L2N, service );
	if (tilt)
		fprintf(out, L2P "tilt %s" L2N, tilt );
	fprintf(out, L2E L1N);
}

static void print_game_rom(FILE* out, const struct GameDriver* game) {
	const struct RomModule *rom = game->rom, *p_rom = NULL;
	extern struct GameDriver driver_0;

	if (!rom) return;

	if (game->clone_of && game->clone_of != &driver_0) {
		fprintf(out, L1P "romof %s" L1N, game->clone_of->name);
	}

	while (rom->name || rom->offset || rom->length) {
		int region = rom->crc;
		rom++;

		while (rom->length) {
			char name[100];
			int offset, length, crc, in_parent;

			sprintf(name,rom->name,game->name);
			offset = rom->offset;
			crc = rom->crc;

			in_parent = 0;
			length = 0;
			do {
				if (rom->name == (char *)-1)
					length = 0; /* restart */
				length += rom->length & ~ROMFLAG_MASK;
				rom++;
			} while (rom->length && (rom->name == 0 || rom->name == (char *)-1));

			if(game->clone_of && crc)
			{
				p_rom = game->clone_of->rom;
				if (p_rom)
					while( !in_parent && (p_rom->name || p_rom->offset || p_rom->length) )
					{
						p_rom++;
						while(!in_parent && p_rom->length) {
							do {
								if (p_rom->crc == crc)
									in_parent = 1;
								else
									p_rom++;
							} while (!in_parent && p_rom->length && (p_rom->name == 0 || p_rom->name == (char *)-1));
						}
					}
			}

			fprintf(out, L1P "rom" L2B);
			if (*name)
				fprintf(out, L2P "name %s" L2N, name);
			if(in_parent && p_rom && p_rom->name)
				fprintf(out, L2P "merge %s" L2N, p_rom->name);
			fprintf(out, L2P "size %d" L2N, length);
			fprintf(out, L2P "crc %08x" L2N, crc);
			switch (region & ~REGIONFLAG_MASK)
			{
			case REGION_CPU1: fprintf(out, L2P "region cpu1" L2N); break;
			case REGION_CPU2: fprintf(out, L2P "region cpu2" L2N); break;
			case REGION_CPU3: fprintf(out, L2P "region cpu3" L2N); break;
			case REGION_CPU4: fprintf(out, L2P "region cpu4" L2N); break;
			case REGION_CPU5: fprintf(out, L2P "region cpu5" L2N); break;
			case REGION_CPU6: fprintf(out, L2P "region cpu6" L2N); break;
			case REGION_CPU7: fprintf(out, L2P "region cpu7" L2N); break;
			case REGION_CPU8: fprintf(out, L2P "region cpu8" L2N); break;
			case REGION_GFX1: fprintf(out, L2P "region gfx1" L2N); break;
			case REGION_GFX2: fprintf(out, L2P "region gfx2" L2N); break;
			case REGION_GFX3: fprintf(out, L2P "region gfx3" L2N); break;
			case REGION_GFX4: fprintf(out, L2P "region gfx4" L2N); break;
			case REGION_GFX5: fprintf(out, L2P "region gfx5" L2N); break;
			case REGION_GFX6: fprintf(out, L2P "region gfx6" L2N); break;
			case REGION_GFX7: fprintf(out, L2P "region gfx7" L2N); break;
			case REGION_GFX8: fprintf(out, L2P "region gfx8" L2N); break;
			case REGION_PROMS: fprintf(out, L2P "region proms" L2N); break;
			case REGION_SOUND1: fprintf(out, L2P "region sound1" L2N); break;
			case REGION_SOUND2: fprintf(out, L2P "region sound2" L2N); break;
			case REGION_SOUND3: fprintf(out, L2P "region sound3" L2N); break;
			case REGION_SOUND4: fprintf(out, L2P "region sound4" L2N); break;
			case REGION_SOUND5: fprintf(out, L2P "region sound5" L2N); break;
			case REGION_SOUND6: fprintf(out, L2P "region sound6" L2N); break;
			case REGION_SOUND7: fprintf(out, L2P "region sound7" L2N); break;
			case REGION_SOUND8: fprintf(out, L2P "region sound8" L2N); break;
			case REGION_USER1: fprintf(out, L2P "region user1" L2N); break;
			case REGION_USER2: fprintf(out, L2P "region user2" L2N); break;
			case REGION_USER3: fprintf(out, L2P "region user3" L2N); break;
			case REGION_USER4: fprintf(out, L2P "region user4" L2N); break;
			case REGION_USER5: fprintf(out, L2P "region user5" L2N); break;
			case REGION_USER6: fprintf(out, L2P "region user6" L2N); break;
			case REGION_USER7: fprintf(out, L2P "region user7" L2N); break;
			case REGION_USER8: fprintf(out, L2P "region user8" L2N); break;
			default: fprintf(out, L2P "region 0x%x" L2N, region & ~REGIONFLAG_MASK);
            }
			switch (region & REGIONFLAG_MASK)
			{
			case 0:
				break;
			case REGIONFLAG_SOUNDONLY:
				fprintf(out, L2P "flags soundonly" L2N);
                break;
			case REGIONFLAG_DISPOSE:
				fprintf(out, L2P "flags dispose" L2N);
				break;
			default:
				fprintf(out, L2P "flags 0x%x" L2N, region & REGIONFLAG_MASK);
            }
			fprintf(out, L2P "offs %x", offset);
            fprintf(out, L2E L1N);
		}
	}
}

static void print_game_sample(FILE* out, const struct GameDriver* game) {
#if (HAS_SAMPLES)
	int i;
	for( i = 0; game->drv->sound[i].sound_type && i < MAX_SOUND; i++ )
	{
		const char **samplenames = NULL;
		if( game->drv->sound[i].sound_type != SOUND_SAMPLES )
			continue;
		samplenames = ((struct Samplesinterface *)game->drv->sound[i].sound_interface)->samplenames;
		if (samplenames != 0 && samplenames[0] != 0) {
			int k = 0;
			if (samplenames[k][0]=='*') {
				/* output sampleof only if different from game name */
				if (strcmp(samplenames[k] + 1, game->name)!=0) {
					fprintf(out, L1P "sampleof %s" L1N, samplenames[k] + 1);
				}
				++k;
			}
			while (samplenames[k] != 0) {
				/* Check if is not empty */
				if (*samplenames[k]) {
					/* Check if sample is duplicate */
					int l = 0;
					while (l<k && strcmp(samplenames[k],samplenames[l])!=0)
						++l;
					if (l==k) {
						fprintf(out, L1P "sample %s" L1N, samplenames[k]);
					}
				}
				++k;
			}
		}
	}
#endif
}


static void print_game_micro(FILE* out, const struct GameDriver* game)
{
	const struct MachineDriver* driver = game->drv;
	const struct MachineCPU* cpu = driver->cpu;
	const struct MachineSound* sound = driver->sound;
	int j;

	for(j=0;j<MAX_CPU;++j)
	{
		if (cpu[j].cpu_type!=0)
		{
			fprintf(out, L1P "chip" L2B);
			if (cpu[j].cpu_type & CPU_AUDIO_CPU)
				fprintf(out, L2P "type cpu flags audio" L2N);
			else
				fprintf(out, L2P "type cpu" L2N);

			fprintf(out, L2P "name ");
			print_statement_string(out, cputype_name(cpu[j].cpu_type));
			fprintf(out, "%s", L2N);

			fprintf(out, L2P "clock %d" L2N, cpu[j].cpu_clock);
			fprintf(out, L2E L1N);
		}
	}

	for(j=0;j<MAX_SOUND;++j) if (sound[j].sound_type)
	{
		if (sound[j].sound_type)
		{
			int num = sound_num(&sound[j]);
			int l;

			if (num == 0) num = 1;

			for(l=0;l<num;++l)
			{
				fprintf(out, L1P "chip" L2B);
				fprintf(out, L2P "type audio" L2N);
				fprintf(out, L2P "name ");
				print_statement_string(out, sound_name(&sound[j]));
				fprintf(out, "%s", L2N);
				if (sound_clock(&sound[j]))
					fprintf(out, L2P "clock %d" L2N, sound_clock(&sound[j]));
				fprintf(out, L2E L1N);
			}
		}
	}
}

static void print_game_video(FILE* out, const struct GameDriver* game)
{
	const struct MachineDriver* driver = game->drv;

	int dx;
	int dy;
	int showxy;
	int orientation;

	fprintf(out, L1P "video" L2B);
	if (driver->video_attributes & VIDEO_TYPE_VECTOR)
	{
		fprintf(out, L2P "screen vector" L2N);
		showxy = 0;
	}
	else
	{
		fprintf(out, L2P "screen raster" L2N);
		showxy = 1;
	}

	if (game->flags & ORIENTATION_SWAP_XY)
	{
		dx = driver->default_visible_area.max_y - driver->default_visible_area.min_y + 1;
		dy = driver->default_visible_area.max_x - driver->default_visible_area.min_x + 1;
		orientation = 1;
	}
	else
	{
		dx = driver->default_visible_area.max_x - driver->default_visible_area.min_x + 1;
		dy = driver->default_visible_area.max_y - driver->default_visible_area.min_y + 1;
		orientation = 0;
	}


	fprintf(out, L2P "orientation %s" L2N, orientation ? "vertical" : "horizontal" );
	if (showxy)
	{
		fprintf(out, L2P "x %d" L2N, dx);
		fprintf(out, L2P "y %d" L2N, dy);
	}

	fprintf(out, L2P "colors %d" L2N, driver->total_colors);
	fprintf(out, L2P "freq %f" L2N, driver->frames_per_second);
	fprintf(out, L2E L1N);
}

static void print_game_sound(FILE* out, const struct GameDriver* game) {
	const struct MachineDriver* driver = game->drv;
	const struct MachineCPU* cpu = driver->cpu;
	const struct MachineSound* sound = driver->sound;

	/* check if the game have sound emulation */
	int has_sound = 0;
	int i;

	i = 0;
	while (i < MAX_SOUND && !has_sound)
	{
		if (sound[i].sound_type)
			has_sound = 1;
		++i;
	}
	i = 0;
	while (i < MAX_CPU && !has_sound)
	{
		if  ((cpu[i].cpu_type & CPU_AUDIO_CPU)!=0)
			has_sound = 1;
		++i;
	}

	fprintf(out, L1P "sound" L2B);

	/* sound channel */
	if (has_sound) {
		if (driver->sound_attributes & SOUND_SUPPORTS_STEREO)
			fprintf(out, L2P "channels 2" L2N);
		else
			fprintf(out, L2P "channels 1" L2N);
	} else
		fprintf(out, L2P "channels 0" L2N);

	fprintf(out, L2E L1N);
}

#define HISTORY_BUFFER_MAX 16384

static void print_game_history(FILE* out, const struct GameDriver* game) {
	char buffer[HISTORY_BUFFER_MAX];

	if (load_driver_history(game,buffer,HISTORY_BUFFER_MAX)==0) {
		fprintf(out, L1P "history ");
		print_c_string(out, buffer);
		fprintf(out, "%s", L1N);
	}
}

static void print_game_driver(FILE* out, const struct GameDriver* game) {
	fprintf(out, L1P "driver" L2B);
	if (game->flags & GAME_NOT_WORKING)
		fprintf(out, L2P "status preliminary" L2N);
	else
		fprintf(out, L2P "status good" L2N);

	if (game->flags & GAME_WRONG_COLORS)
		fprintf(out, L2P "color preliminary" L2N);
	else if (game->flags & GAME_IMPERFECT_COLORS)
		fprintf(out, L2P "color imperfect" L2N);
	else
		fprintf(out, L2P "color good" L2N);

	if (game->flags & GAME_NO_SOUND)
		fprintf(out, L2P "sound preliminary" L2N);
	else if (game->flags & GAME_IMPERFECT_SOUND)
		fprintf(out, L2P "sound imperfect" L2N);
	else
		fprintf(out, L2P "sound good" L2N);

	if (game->flags & GAME_REQUIRES_16BIT)
		fprintf(out, L2P "colordeep 16" L2N);
	else
		fprintf(out, L2P "colordeep 8" L2N);

	fprintf(out, L2E L1N);
}

/* Print the MAME info record for a game */
static void print_game_info(FILE* out, const struct GameDriver* game) {

	#ifndef MESS
	fprintf(out, "game" L1B );
	#else
	fprintf(out, "machine" L1B );
	#endif

	fprintf(out, L1P "name %s" L1N, game->name );

	if (game->description) {
		fprintf(out, L1P "description ");
		print_c_string(out, game->description );
		fprintf(out, "%s", L1N);
	}

	/* print the year only if is a number */
	if (game->year && strspn(game->year,"0123456789")==strlen(game->year)) {
		fprintf(out, L1P "year %s" L1N, game->year );
	}

	if (game->manufacturer) {
		fprintf(out, L1P "manufacturer ");
		print_c_string(out, game->manufacturer );
		fprintf(out, "%s", L1N);
	}

	print_game_history(out,game);

	if (game->clone_of && !(game->clone_of->flags & NOT_A_DRIVER)) {
		fprintf(out, L1P "cloneof %s" L1N, game->clone_of->name);
	}

	print_game_rom(out,game);
	print_game_sample(out,game);
	print_game_micro(out,game);
	print_game_video(out,game);
	print_game_sound(out,game);
	print_game_input(out,game);
	print_game_switch(out,game);
	print_game_driver(out,game);

	fprintf(out, L1E);
}

/* Print all the MAME info database */
void print_mame_info(FILE* out, const struct GameDriver* games[]) {
	int j;

	for(j=0;games[j];++j)
		print_game_info( out, games[j] );

	#ifndef MESS
	/* addictional fixed record */
	fprintf(out, "resource" L1B);
	fprintf(out, L1P "name neogeo" L1N);
	fprintf(out, L1P "description \"Neo Geo BIOS\"" L1N);
	fprintf(out, L1P "rom" L2B);
	fprintf(out, L2P "name neo-geo.rom" L2N);
	fprintf(out, L2P "size 131072" L2N);
	fprintf(out, L2P "crc 9036d879" L2N);
	fprintf(out, L2E L1N);
	fprintf(out, L1P "rom" L2B);
	fprintf(out, L2P "name ng-sm1.rom" L2N);
	fprintf(out, L2P "size 131072" L2N);
	fprintf(out, L2P "crc 97cf998b" L2N);
	fprintf(out, L2E L1N);
	fprintf(out, L1P "rom" L2B);
	fprintf(out, L2P "name ng-sfix.rom" L2N);
	fprintf(out, L2P "size 131072" L2N);
	fprintf(out, L2P "crc 354029fc" L2N);
	fprintf(out, L2E L1N);
	fprintf(out, L1E);
	#endif
}
