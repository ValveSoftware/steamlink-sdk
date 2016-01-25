#include "allegro.h"
#include "driver.h"
#include <SDL.h>

int use_mouse;
int joystick;

unsigned long ExKey1=0;
unsigned long ExKey2=0;
unsigned long ExKey3=0;
unsigned long ExKey4=0;
unsigned long ExKeyKB=0;
int num_joysticks=4;

int mouse_xrel=0;
int mouse_yrel=0;
Uint8 mouse_button=0;

#include "minimal.h"

static struct KeyboardInfo keylist[] =
{
	{ "A",			KEY_A,				KEYCODE_A },
	{ "B",			KEY_B,				KEYCODE_B },
	{ "C",			KEY_C,				KEYCODE_C },
	{ "D",			KEY_D,				KEYCODE_D },
	{ "E",			KEY_E,				KEYCODE_E },
	{ "F",			KEY_F,				KEYCODE_F },
	{ "G",			KEY_G,				KEYCODE_G },
	{ "H",			KEY_H,				KEYCODE_H },
	{ "I",			KEY_I,				KEYCODE_I },
	{ "J",			KEY_J,				KEYCODE_J },
	{ "K",			KEY_K,				KEYCODE_K },
	{ "L",			KEY_L,				KEYCODE_L },
	{ "M",			KEY_M,				KEYCODE_M },
	{ "N",			KEY_N,				KEYCODE_N },
	{ "O",			KEY_O,				KEYCODE_O },
	{ "P",			KEY_P,				KEYCODE_P },
	{ "Q",			KEY_Q,				KEYCODE_Q },
	{ "R",			KEY_R,				KEYCODE_R },
	{ "S",			KEY_S,				KEYCODE_S },
	{ "T",			KEY_T,				KEYCODE_T },
	{ "U",			KEY_U,				KEYCODE_U },
	{ "V",			KEY_V,				KEYCODE_V },
	{ "W",			KEY_W,				KEYCODE_W },
	{ "X",			KEY_X,				KEYCODE_X },
	{ "Y",			KEY_Y,				KEYCODE_Y },
	{ "Z",			KEY_Z,				KEYCODE_Z },
	{ "0",			KEY_0,				KEYCODE_0 },
	{ "1",			KEY_1,				KEYCODE_1 },
	{ "2",			KEY_2,				KEYCODE_2 },
	{ "3",			KEY_3,				KEYCODE_3 },
	{ "4",			KEY_4,				KEYCODE_4 },
	{ "5",			KEY_5,				KEYCODE_5 },
	{ "6",			KEY_6,				KEYCODE_6 },
	{ "7",			KEY_7,				KEYCODE_7 },
	{ "8",			KEY_8,				KEYCODE_8 },
	{ "9",			KEY_9,				KEYCODE_9 },
	{ "0 PAD",		KEY_0_PAD,			KEYCODE_0_PAD },
	{ "1 PAD",		KEY_1_PAD,			KEYCODE_1_PAD },
	{ "2 PAD",		KEY_2_PAD,			KEYCODE_2_PAD },
	{ "3 PAD",		KEY_3_PAD,			KEYCODE_3_PAD },
	{ "4 PAD",		KEY_4_PAD,			KEYCODE_4_PAD },
	{ "5 PAD",		KEY_5_PAD,			KEYCODE_5_PAD },
	{ "6 PAD",		KEY_6_PAD,			KEYCODE_6_PAD },
	{ "7 PAD",		KEY_7_PAD,			KEYCODE_7_PAD },
	{ "8 PAD",		KEY_8_PAD,			KEYCODE_8_PAD },
	{ "9 PAD",		KEY_9_PAD,			KEYCODE_9_PAD },
	{ "F1",			KEY_F1,				KEYCODE_F1 },
	{ "F2",			KEY_F2,				KEYCODE_F2 },
	{ "F3",			KEY_F3,				KEYCODE_F3 },
	{ "F4",			KEY_F4,				KEYCODE_F4 },
	{ "F5",			KEY_F5,				KEYCODE_F5 },
	{ "F6",			KEY_F6,				KEYCODE_F6 },
	{ "F7",			KEY_F7,				KEYCODE_F7 },
	{ "F8",			KEY_F8,				KEYCODE_F8 },
	{ "F9",			KEY_F9,				KEYCODE_F9 },
	{ "F10",		KEY_F10,			KEYCODE_F10 },
	{ "F11",		KEY_F11,			KEYCODE_F11 },
	{ "F12",		KEY_F12,			KEYCODE_F12 },
	{ "ESC",		KEY_ESC,			KEYCODE_ESC },
	{ "~",			KEY_TILDE,			KEYCODE_TILDE },
	{ "-",          	KEY_MINUS,          		KEYCODE_MINUS },
	{ "=",          	KEY_EQUALS,         		KEYCODE_EQUALS },
	{ "BKSPACE",		KEY_BACKSPACE,			KEYCODE_BACKSPACE },
	{ "TAB",		KEY_TAB,			KEYCODE_TAB },
	{ "[",          	KEY_OPENBRACE,      		KEYCODE_OPENBRACE },
	{ "]",          	KEY_CLOSEBRACE,     		KEYCODE_CLOSEBRACE },
	{ "ENTER",		KEY_ENTER,			KEYCODE_ENTER },
	{ ";",          	KEY_COLON,          		KEYCODE_COLON },
	{ ":",          	KEY_QUOTE,          		KEYCODE_QUOTE },
	{ "\\",         	KEY_BACKSLASH,      		KEYCODE_BACKSLASH },
	{ "<",          	KEY_BACKSLASH2,     		KEYCODE_BACKSLASH2 },
	{ ",",          	KEY_COMMA,          		KEYCODE_COMMA },
	{ ".",          	KEY_STOP,           		KEYCODE_STOP },
	{ "/",          	KEY_SLASH,          		KEYCODE_SLASH },
	{ "SPACE",		KEY_SPACE,			KEYCODE_SPACE },
	{ "INS",		KEY_INSERT,			KEYCODE_INSERT },
	{ "DEL",		KEY_DEL,			KEYCODE_DEL },
	{ "HOME",		KEY_HOME,			KEYCODE_HOME },
	{ "END",		KEY_END,			KEYCODE_END },
	{ "PGUP",		KEY_PGUP,			KEYCODE_PGUP },
	{ "PGDN",		KEY_PGDN,			KEYCODE_PGDN },
	{ "LEFT",		KEY_LEFT,			KEYCODE_LEFT },
	{ "RIGHT",		KEY_RIGHT,			KEYCODE_RIGHT },
	{ "UP",			KEY_UP,				KEYCODE_UP },
	{ "DOWN",		KEY_DOWN,			KEYCODE_DOWN },
	{ "/ PAD",      	KEY_SLASH_PAD,      		KEYCODE_SLASH_PAD },
	{ "* PAD",      	KEY_ASTERISK,       		KEYCODE_ASTERISK },
	{ "- PAD",      	KEY_MINUS_PAD,      		KEYCODE_MINUS_PAD },
	{ "+ PAD",      	KEY_PLUS_PAD,       		KEYCODE_PLUS_PAD },
	{ ". PAD",      	KEY_DEL_PAD,        		KEYCODE_DEL_PAD },
	{ "ENTER PAD",  	KEY_ENTER_PAD,      		KEYCODE_ENTER_PAD },
	{ "PRTSCR",     	KEY_PRTSCR,         		KEYCODE_PRTSCR },
	{ "PAUSE",      	KEY_PAUSE,          		KEYCODE_PAUSE },
	{ "LSHIFT",		KEY_LSHIFT,			KEYCODE_LSHIFT },
	{ "RSHIFT",		KEY_RSHIFT,			KEYCODE_RSHIFT },
	{ "LCTRL",		KEY_LCONTROL,			KEYCODE_LCONTROL },
	{ "RCTRL",		KEY_RCONTROL,			KEYCODE_RCONTROL },
	{ "ALT",		KEY_ALT,			KEYCODE_LALT },
	{ "ALTGR",		KEY_ALTGR,			KEYCODE_RALT },
	{ "LWIN",		KEY_LWIN,			KEYCODE_OTHER },
	{ "RWIN",		KEY_RWIN,			KEYCODE_OTHER },
	{ "MENU",		KEY_MENU,			KEYCODE_OTHER },
	{ "SCRLOCK",    	KEY_SCRLOCK,        		KEYCODE_SCRLOCK },
	{ "NUMLOCK",    	KEY_NUMLOCK,        		KEYCODE_NUMLOCK },
	{ "CAPSLOCK",   	KEY_CAPSLOCK,       		KEYCODE_CAPSLOCK },
	{ 0, 0, 0 }	/* end of table */
};

struct SDLtranslate
{
    int mamekey;
    int sdlkey;
};


static struct SDLtranslate sdlkeytranslate[] =
{
	{	KEY_A,						SDLK_a },
	{	KEY_B,						SDLK_b },
	{	KEY_C,						SDLK_c },
	{	KEY_D,						SDLK_d },
	{	KEY_E,						SDLK_e },
	{	KEY_F,						SDLK_f },
	{	KEY_G,						SDLK_g },
	{	KEY_H,						SDLK_h },
	{	KEY_I,						SDLK_i },
	{	KEY_J,						SDLK_j },
	{	KEY_K,						SDLK_k },
	{	KEY_L,						SDLK_l },
	{	KEY_M,						SDLK_m },
	{	KEY_N,						SDLK_n },
	{	KEY_O,						SDLK_o },
	{	KEY_P,						SDLK_p },
	{	KEY_Q,						SDLK_q },
	{	KEY_R,						SDLK_r },
	{	KEY_S,						SDLK_s },
	{	KEY_T,						SDLK_t },
	{	KEY_U,						SDLK_u },
	{	KEY_V,						SDLK_v },
	{	KEY_W,						SDLK_w },
	{	KEY_X,						SDLK_x },
	{	KEY_Y,						SDLK_y },
	{	KEY_Z,						SDLK_z },
	{	KEY_0,						SDLK_0 },
	{	KEY_1,						SDLK_1 },
	{	KEY_2,						SDLK_2 },
	{	KEY_3,						SDLK_3 },
	{	KEY_4,						SDLK_4 },
	{	KEY_5,						SDLK_5 },
	{	KEY_6,						SDLK_6 },
	{	KEY_7,						SDLK_7 },
	{	KEY_8,						SDLK_8 },
	{	KEY_9,						SDLK_9 },
	{	KEY_0_PAD,					SDLK_KP0 },
	{	KEY_1_PAD,					SDLK_KP1 },
	{	KEY_2_PAD,					SDLK_KP2 },
	{	KEY_3_PAD,					SDLK_KP3 },
	{	KEY_4_PAD,					SDLK_KP4 },
	{	KEY_5_PAD,					SDLK_KP5 },
	{	KEY_6_PAD,					SDLK_KP6 },
	{	KEY_7_PAD,					SDLK_KP7 },
	{	KEY_8_PAD,					SDLK_KP8 },
	{	KEY_9_PAD,					SDLK_KP9 },
	{	KEY_F1,						SDLK_F1 },
	{	KEY_F2,						SDLK_F2 },
	{	KEY_F3,						SDLK_F3 },
	{	KEY_F4,						SDLK_F4 },
	{	KEY_F5,						SDLK_F5 },
	{	KEY_F6,						SDLK_F6 },
	{	KEY_F7,						SDLK_F7 },
	{	KEY_F8,						SDLK_F8 },
	{	KEY_F9,						SDLK_F9 },
	{	KEY_F10,					SDLK_F10 },
	{	KEY_F11,					SDLK_F11 },
	{	KEY_F12,					SDLK_F12 },
	{	KEY_ESC,					SDLK_ESCAPE },
	{	KEY_TILDE,					SDLK_BACKQUOTE },
	{	KEY_MINUS,		          	SDLK_MINUS },
	{	KEY_EQUALS,		         	SDLK_EQUALS },
	{	KEY_BACKSPACE,				SDLK_BACKSPACE },
	{	KEY_TAB,					SDLK_TAB },
	{	KEY_OPENBRACE,		      	SDLK_LEFTBRACKET },
	{	KEY_CLOSEBRACE,		     	SDLK_RIGHTBRACKET },
	{	KEY_ENTER,					SDLK_RETURN },
	{	KEY_COLON,		          	SDLK_COLON },
	{	KEY_QUOTE,		          	SDLK_QUOTE },
	{	KEY_BACKSLASH,		      	SDLK_BACKSLASH },
	{	KEY_COMMA,                  SDLK_COMMA },
	{	KEY_STOP,		           	SDLK_PERIOD },
	{	KEY_SLASH,		          	SDLK_SLASH },
	{	KEY_SPACE,					SDLK_SPACE },
	{	KEY_INSERT,					SDLK_INSERT },
	{	KEY_DEL,					SDLK_DELETE },
	{	KEY_HOME,					SDLK_HOME },
	{	KEY_END,					SDLK_END },
	{	KEY_PGUP,					SDLK_PAGEUP },
	{	KEY_PGDN,					SDLK_PAGEDOWN },
	{	KEY_LEFT,					SDLK_LEFT },
	{	KEY_RIGHT,					SDLK_RIGHT },
	{	KEY_UP,						SDLK_UP },
	{	KEY_DOWN,					SDLK_DOWN },
	{  	KEY_SLASH_PAD,		      	SDLK_KP_DIVIDE },
	{	KEY_ASTERISK,		       	SDLK_KP_MULTIPLY },
	{	KEY_MINUS_PAD,		      	SDLK_KP_MINUS },
	{  	KEY_PLUS_PAD,		       	SDLK_KP_PLUS },
	{ 	KEY_ENTER_PAD,		      	SDLK_KP_ENTER },
	{	KEY_LSHIFT,					SDLK_LSHIFT },
	{	KEY_RSHIFT,					SDLK_RSHIFT },
	{	KEY_LCONTROL,				SDLK_LCTRL },
	{	KEY_RCONTROL,				SDLK_RCTRL },
	{	KEY_ALT,					SDLK_LALT },
	{	KEY_ALTGR,					SDLK_RALT },
	{ 0, 0 }	/* end of table */
};


/* return a list of all available keys */
const struct KeyboardInfo *osd_get_key_list(void)
{
	return keylist;
}

static int key[KEY_MAX];

// Do the translation from SDL Key to mame key
// and set the mame keys
//
void keyprocess(SDLKey inkey, SDL_bool pressed)
{
	int i=0;

	while(sdlkeytranslate[i].mamekey)
	{	
		if(inkey == sdlkeytranslate[i].sdlkey)
		{
			key[sdlkeytranslate[i].mamekey]=pressed;
			break;
		}
		i++;
	}
}

int osd_is_sdlkey_pressed(int inkey)
{
	int i=0;

	while(sdlkeytranslate[i].mamekey)
	{
		if(inkey == (int) sdlkeytranslate[i].sdlkey)
		{
			return (key[sdlkeytranslate[i].mamekey]);
		}
		i++;
	}
}

void joyprocess(Uint8 button, SDL_bool pressed, Uint8 njoy)
{
    Uint32 val=0;
	unsigned long *mykey=0;

	if(njoy == 0) mykey = &ExKey1;
	if(njoy == 1) mykey = &ExKey2;
	if(njoy == 2) mykey = &ExKey3;
	if(njoy == 3) mykey = &ExKey4;

    switch(button)
    {
        case 0:
            val=GP2X_1; break;
        case 1:
            val=GP2X_2; break;
        case 2:
            val=GP2X_3; break;
        case 3:
            val=GP2X_4; break;
        case 4:
            val=GP2X_5; break;
        case 5:
            val=GP2X_6; break;
        case 6:
            val=GP2X_7; break;
        case 7:
            val=GP2X_8; break;
        case 8:
            val=GP2X_9; break;
			break;
		case 9:
            val=GP2X_10; break;
			break;
		case 10:
            val=GP2X_11; break;
			break;
		case 11:
            val=GP2X_12; break;
			break;
		case 12:
            val=GP2X_13; break;
			break;
		case 13:
            val=GP2X_14; break;
			break;
		case 14:
            val=GP2X_15; break;
			break;
		case 15:
            val=GP2X_16; break;
			break;
        default:
            return;
    }
    if (pressed)
        (*mykey) |= val;
    else
        (*mykey) ^= val;
}

void mouse_motion_process(int x, int y)
{
	mouse_xrel = x;
	mouse_yrel = y;	
}

void mouse_button_process(Uint8 button, SDL_bool pressed)
{
	Uint8 val=0;

	switch(button)
	{
		case SDL_BUTTON_LEFT:
			val=1<<0; break;
		case SDL_BUTTON_RIGHT:
			val=1<<1; break;
		case SDL_BUTTON_MIDDLE:
			val=1<<2; break;
		default:
			return;
	}
	if (pressed)
		mouse_button |= val;	
	else
		mouse_button ^= val;	
}
	

void gp2x_joystick_clear(void)
{
	int i;

    SDL_Event event;
    while(SDL_PollEvent(&event));
	ExKey1=0;
	ExKey2=0;
	ExKey3=0;
	ExKey4=0;

	for(i=0;i<KEY_MAX;i++) {
		key[i] = 0;
	}
}



int osd_is_key_pressed(int keycode)
{
	if (keycode >= KEY_MAX) return 0;

	if (keycode == KEY_PAUSE)
	{
		static int pressed,counter;
		int res;

		res = key[KEY_PAUSE] ^ pressed;
		if (res)
		{
			if (counter > 0)
			{
				if (--counter == 0)
					pressed = key[KEY_PAUSE];
			}
			else counter = 10;
		}

		return res;
	}

	return key[keycode];
}


int osd_wait_keypress(void)
{
	/*
	clear_keybuf();
	return readkey() >> 8;
	*/
	return 0;
}


int osd_readkey_unicode(int flush)
{
	/*
	if (flush) clear_keybuf();
	if (keypressed())
		return ureadkey(NULL);
	else
		return 0;
	*/
	return 0;
}


/*
  limits:
  - 7 joysticks
  - 15 sticks on each joystick
  - 63 buttons on each joystick

  - 256 total inputs
*/
#define JOYCODE(joy,stick,axis_or_button,dir) \
		((((dir)&0x03)<<14)|(((axis_or_button)&0x3f)<<8)|(((stick)&0x1f)<<3)|(((joy)&0x07)<<0))

#define GET_JOYCODE_JOY(code) (((code)>>0)&0x07)
#define GET_JOYCODE_STICK(code) (((code)>>3)&0x1f)
#define GET_JOYCODE_AXIS(code) (((code)>>8)&0x3f)
#define GET_JOYCODE_BUTTON(code) GET_JOYCODE_AXIS(code)
#define GET_JOYCODE_DIR(code) (((code)>>14)&0x03)

/* use otherwise unused joystick codes for the three mouse buttons */
#define MOUSE_BUTTON(button) JOYCODE(1,0,button,1)


#define MAX_JOY 256
#define MAX_JOY_NAME_LEN 40
#define MAX_BUTTONS 16

static struct JoystickInfo joylist[MAX_JOY] =
{
	/* will be filled later */
	{ 0, 0, 0 }	/* end of table */
};

static char joynames[MAX_JOY][MAX_JOY_NAME_LEN+1];	/* will be used to store names for the above */


static int joyequiv[][2] =
{
	{ JOYCODE(1,1,1,1),	JOYCODE_1_LEFT },
	{ JOYCODE(1,1,1,2),	JOYCODE_1_RIGHT },
	{ JOYCODE(1,1,2,1),	JOYCODE_1_UP },
	{ JOYCODE(1,1,2,2),	JOYCODE_1_DOWN },
	{ JOYCODE(1,0,1,0),	JOYCODE_1_BUTTON1 },
	{ JOYCODE(1,0,2,0),	JOYCODE_1_BUTTON2 },
	{ JOYCODE(1,0,3,0),	JOYCODE_1_BUTTON3 },
	{ JOYCODE(1,0,4,0),	JOYCODE_1_BUTTON4 },
	{ JOYCODE(1,0,5,0),	JOYCODE_1_BUTTON5 },
	{ JOYCODE(1,0,6,0),	JOYCODE_1_BUTTON6 },
	{ JOYCODE(1,0,7,0),	JOYCODE_1_BUTTON7 },
	{ JOYCODE(1,0,8,0),	JOYCODE_1_BUTTON8 },
	{ JOYCODE(1,0,9,0),	JOYCODE_1_BUTTON9 },
	{ JOYCODE(1,0,10,0),JOYCODE_1_BUTTON10 },
	{ JOYCODE(1,0,11,0),JOYCODE_1_BUTTON11 },
	{ JOYCODE(1,0,12,0),JOYCODE_1_BUTTON12 },
	{ JOYCODE(1,0,13,0),JOYCODE_1_BUTTON13 },
	{ JOYCODE(1,0,14,0),JOYCODE_1_BUTTON14 },
	{ JOYCODE(1,0,15,0),JOYCODE_1_BUTTON15 },
	{ JOYCODE(1,0,16,0),JOYCODE_1_BUTTON16 },
	{ JOYCODE(1,0,1,1),	JOYCODE_1_MOUSE1 },
	{ JOYCODE(1,0,2,1),	JOYCODE_1_MOUSE2 },
	{ JOYCODE(1,0,3,1),	JOYCODE_1_MOUSE3 },
	{ JOYCODE(2,1,1,1),	JOYCODE_2_LEFT },
	{ JOYCODE(2,1,1,2),	JOYCODE_2_RIGHT },
	{ JOYCODE(2,1,2,1),	JOYCODE_2_UP },
	{ JOYCODE(2,1,2,2),	JOYCODE_2_DOWN },
	{ JOYCODE(2,0,1,0),	JOYCODE_2_BUTTON1 },
	{ JOYCODE(2,0,2,0),	JOYCODE_2_BUTTON2 },
	{ JOYCODE(2,0,3,0),	JOYCODE_2_BUTTON3 },
	{ JOYCODE(2,0,4,0),	JOYCODE_2_BUTTON4 },
	{ JOYCODE(2,0,5,0),	JOYCODE_2_BUTTON5 },
	{ JOYCODE(2,0,6,0),	JOYCODE_2_BUTTON6 },
	{ JOYCODE(2,0,7,0),	JOYCODE_2_BUTTON7 },
	{ JOYCODE(2,0,8,0),	JOYCODE_2_BUTTON8 },
	{ JOYCODE(2,0,9,0),	JOYCODE_2_BUTTON9 },
	{ JOYCODE(2,0,10,0),JOYCODE_2_BUTTON10 },
	{ JOYCODE(2,0,11,0),JOYCODE_2_BUTTON11 },
	{ JOYCODE(2,0,12,0),JOYCODE_2_BUTTON12 },
	{ JOYCODE(2,0,13,0),JOYCODE_2_BUTTON13 },
	{ JOYCODE(2,0,14,0),JOYCODE_2_BUTTON14 },
	{ JOYCODE(2,0,15,0),JOYCODE_2_BUTTON15 },
	{ JOYCODE(2,0,16,0),JOYCODE_2_BUTTON16 },
	{ JOYCODE(3,1,1,1),	JOYCODE_3_LEFT },
	{ JOYCODE(3,1,1,2),	JOYCODE_3_RIGHT },
	{ JOYCODE(3,1,2,1),	JOYCODE_3_UP },
	{ JOYCODE(3,1,2,2),	JOYCODE_3_DOWN },
	{ JOYCODE(3,0,1,0),	JOYCODE_3_BUTTON1 },
	{ JOYCODE(3,0,2,0),	JOYCODE_3_BUTTON2 },
	{ JOYCODE(3,0,3,0),	JOYCODE_3_BUTTON3 },
	{ JOYCODE(3,0,4,0),	JOYCODE_3_BUTTON4 },
	{ JOYCODE(3,0,5,0),	JOYCODE_3_BUTTON5 },
	{ JOYCODE(3,0,6,0),	JOYCODE_3_BUTTON6 },
	{ JOYCODE(3,0,7,0),	JOYCODE_3_BUTTON7 },
	{ JOYCODE(3,0,8,0),	JOYCODE_3_BUTTON8 },
	{ JOYCODE(3,0,9,0),	JOYCODE_3_BUTTON9 },
	{ JOYCODE(3,0,10,0),JOYCODE_3_BUTTON10 },
	{ JOYCODE(3,0,11,0),JOYCODE_3_BUTTON11 },
	{ JOYCODE(3,0,12,0),JOYCODE_3_BUTTON12 },
	{ JOYCODE(3,0,13,0),JOYCODE_3_BUTTON13 },
	{ JOYCODE(3,0,14,0),JOYCODE_3_BUTTON14 },
	{ JOYCODE(3,0,15,0),JOYCODE_3_BUTTON15 },
	{ JOYCODE(3,0,16,0),JOYCODE_3_BUTTON16 },
	{ JOYCODE(4,1,1,1),	JOYCODE_4_LEFT },
	{ JOYCODE(4,1,1,2),	JOYCODE_4_RIGHT },
	{ JOYCODE(4,1,2,1),	JOYCODE_4_UP },
	{ JOYCODE(4,1,2,2),	JOYCODE_4_DOWN },
	{ JOYCODE(4,0,1,0),	JOYCODE_4_BUTTON1 },
	{ JOYCODE(4,0,2,0),	JOYCODE_4_BUTTON2 },
	{ JOYCODE(4,0,3,0),	JOYCODE_4_BUTTON3 },
	{ JOYCODE(4,0,4,0),	JOYCODE_4_BUTTON4 },
	{ JOYCODE(4,0,5,0),	JOYCODE_4_BUTTON5 },
	{ JOYCODE(4,0,6,0),	JOYCODE_4_BUTTON6 },
	{ JOYCODE(4,0,7,0),	JOYCODE_4_BUTTON7 },
	{ JOYCODE(4,0,8,0),	JOYCODE_4_BUTTON8 },
	{ JOYCODE(4,0,9,0),	JOYCODE_4_BUTTON9 },
	{ JOYCODE(4,0,10,0),JOYCODE_4_BUTTON10 },
	{ JOYCODE(4,0,11,0),JOYCODE_4_BUTTON11 },
	{ JOYCODE(4,0,12,0),JOYCODE_4_BUTTON12 },
	{ JOYCODE(4,0,13,0),JOYCODE_4_BUTTON13 },
	{ JOYCODE(4,0,14,0),JOYCODE_4_BUTTON14 },
	{ JOYCODE(4,0,15,0),JOYCODE_4_BUTTON15 },
	{ JOYCODE(4,0,16,0),JOYCODE_4_BUTTON16 },
	{ 0,0 }
};


static void init_joy_list(void)
{
	int tot,i,j,k;
	char buf[256];

	tot = 0;

 	/* first of all, map mouse buttons */
 	for (j = 0;j < 3;j++)
 	{
 		sprintf(buf,"MOUSE B%d",j+1);
 		strncpy(joynames[tot],buf,MAX_JOY_NAME_LEN);
 		joynames[tot][MAX_JOY_NAME_LEN] = 0;
 		joylist[tot].name = joynames[tot];
 		joylist[tot].code = MOUSE_BUTTON(j+1);
 		tot++;
 	}

	for (i = 0;i < num_joysticks;i++)
	{
			//Generate joystick axis joycodes
			for (k = 0;k < 6;k++)
			{
				//direction 1
				sprintf(buf,"J%d %s %d-",i+1,"JoyAxis", k);
				strncpy(joynames[tot],buf,MAX_JOY_NAME_LEN);
				joynames[tot][MAX_JOY_NAME_LEN] = 0;
				joylist[tot].name = joynames[tot];
				joylist[tot].code = JOYCODE(i+1,1,k+1,1);
				tot++;

				//direction 2
				sprintf(buf,"J%d %s %d+",i+1,"JoyAxis", k);
				strncpy(joynames[tot],buf,MAX_JOY_NAME_LEN);
				joynames[tot][MAX_JOY_NAME_LEN] = 0;
				joylist[tot].name = joynames[tot];
				joylist[tot].code = JOYCODE(i+1,1,k+1,2);
				tot++;
			}

		//Generate joystick button codes
		for (j = 0;j < MAX_BUTTONS;j++)
		{
			sprintf(buf,"J%d %s %d",i+1,"JoyButton", j);
			strncpy(joynames[tot],buf,MAX_JOY_NAME_LEN);
			joynames[tot][MAX_JOY_NAME_LEN] = 0;
			joylist[tot].name = joynames[tot];
			joylist[tot].code = JOYCODE(i+1,0,j+1,0);
			tot++;
		}
	}

	/* terminate array */
	joylist[tot].name = 0;
	joylist[tot].code = 0;
	joylist[tot].standardcode = 0;

	/* fill in equivalences */
	for (i = 0;i < tot;i++)
	{
		joylist[i].standardcode = JOYCODE_OTHER;

		j = 0;
		while (joyequiv[j][0] != 0)
		{
			if (joyequiv[j][0] == joylist[i].code)
			{
				joylist[i].standardcode = joyequiv[j][1];
				break;
			}
			j++;
		}
	}
}


/* return a list of all available joys */
const struct JoystickInfo *osd_get_joy_list(void)
{
	return joylist;
}

int is_joy_button_pressed (int button, int ExKey)
{
	switch (button)
	{
		case 0: return ExKey & GP2X_1; break;
		case 1: return ExKey & GP2X_2; break;
		case 2: return ExKey & GP2X_3; break;
		case 3: return ExKey & GP2X_4; break;
		case 4: return ExKey & GP2X_5; break;
		case 5: return ExKey & GP2X_6; break;
		case 6: return ExKey & GP2X_7; break;
		case 7: return ExKey & GP2X_8; break;
		case 8: return ExKey & GP2X_9; break;
		case 9: return ExKey & GP2X_10; break;
		case 10: return ExKey & GP2X_11; break;
		case 11: return ExKey & GP2X_12; break;
		case 12: return ExKey & GP2X_13; break;
		case 13: return ExKey & GP2X_14; break;
		case 14: return ExKey & GP2X_15; break;
		case 15: return ExKey & GP2X_16; break;
		default: break;
	}
	return 0; 
}

//#define JOY_LEFT_PRESSED is_joy_axis_pressed(0,1,ExKey1)
//#define JOY_RIGHT_PRESSED is_joy_axis_pressed(0,2,ExKey1)
//#define JOY_UP_PRESSED is_joy_axis_pressed(1,1,ExKey1)
//#define JOY_DOWN_PRESSED is_joy_axis_pressed(1,2,ExKey1)

extern SDL_Joystick* myjoy[4];

int is_joy_axis_pressed (int axis, int dir, int joynum)
{
	if (!myjoy[joynum]) return 0;

	/* Normal controls */
	if(dir == 1) { //left up
		if(SDL_JoystickGetAxis(myjoy[joynum], axis) < -12000) return true;	
	}
	if(dir == 2) { //right down
		if(SDL_JoystickGetAxis(myjoy[joynum], axis) > 12000) return true;	
	}

	return 0;
}

int osd_is_joy_pressed(int joycode)
{
	int joy_num,stick;

	/* special case for mouse buttons */
	switch (joycode)
	{
		case MOUSE_BUTTON(1):
			return mouse_button & 1<<0; break;
		case MOUSE_BUTTON(2):
			return mouse_button & 1<<1; break;
		case MOUSE_BUTTON(3):
			return mouse_button & 1<<2; break;
	}

	joy_num = GET_JOYCODE_JOY(joycode);

	/* do we have as many sticks? */
	if (joy_num == 0 || joy_num > num_joysticks)
		return 0;
	joy_num--;

	stick = GET_JOYCODE_STICK(joycode);

	//stick 0 is buttons, stick 1 is axis
	if (stick == 0)
	{
		/* buttons */
		int button;

		button = GET_JOYCODE_BUTTON(joycode);
		if (button == 0 || button > MAX_BUTTONS)
			return 0;
		button--;

		switch (joy_num)
		{
			case 0: return is_joy_button_pressed(button, ExKey1); break;
			case 1: return is_joy_button_pressed(button, ExKey2); break;
			case 2: return is_joy_button_pressed(button, ExKey3); break;
			case 3: return is_joy_button_pressed(button, ExKey4); break;
			default: break;
		}
	}
	else
	{
		/* axis stick */
		int axis,dir;

		if (stick > 1)
			return 0;
		stick--;

		axis = GET_JOYCODE_AXIS(joycode);
		dir = GET_JOYCODE_DIR(joycode);

		if (axis == 0 || axis > 7)
			return 0;
		axis--;

		return is_joy_axis_pressed(axis, dir, joy_num);
	}

	return 0;
}

void osd_poll_joysticks(void)
{
	//Update both the keyboard and all joysticks
	gp2x_joystick_read();
}

int pos_analog_x=0;
int pos_analog_y=0;


/* return a value in the range -128 .. 128 (yes, 128, not 127) */
void osd_analogjoy_read(int player,int *analog_x, int *analog_y)
{
	*analog_x = *analog_y = 0;

	/* is there an analog joystick at all? */
	if (player+1 > num_joysticks || joystick == JOY_TYPE_NONE)
		return;

	if (!myjoy[player]) return;

	pos_analog_x=SDL_JoystickGetAxis(myjoy[player], 0)/256;	
	pos_analog_y=SDL_JoystickGetAxis(myjoy[player], 1)/256;	

	if (pos_analog_x<-128) pos_analog_x=-128;
	if (pos_analog_x>128) pos_analog_x=128;
	if (pos_analog_y<-128) pos_analog_y=-128;
	if (pos_analog_y>128) pos_analog_y=128;
	
	*analog_x = pos_analog_x;
	*analog_y = pos_analog_y;
}

int osd_joystick_needs_calibration (void)
{
	return 0;
}


void osd_joystick_start_calibration (void)
{
}

char *osd_joystick_calibrate_next (void)
{
	return 0;
}

void osd_joystick_calibrate (void)
{
}

void osd_joystick_end_calibration (void)
{
}

void osd_trak_read(int player,int *deltax,int *deltay)
{
	*deltax = *deltay = 0;

	if (use_mouse && !myjoy[player] && player == 0) 
	{
		*deltax = mouse_xrel*2;
		*deltay = mouse_yrel*2;
		return;
	}

 	if (!myjoy[player]) return;

   	*deltax=SDL_JoystickGetAxis(myjoy[player], 0)/256/4;
   	*deltay=SDL_JoystickGetAxis(myjoy[player], 1)/256/4;
}


#ifndef MESS
#ifndef TINY_COMPILE
extern int no_of_tiles;
extern struct GameDriver driver_neogeo;
#endif
#endif

void osd_customize_inputport_defaults(struct ipd *defaults)
{
}

void osd_led_w(int led,int on) 
{
}

void msdos_init_input (void)
{
	int i;
	
	/* Initialize keyboard to not pressed */
	for (i = 0;i < KEY_MAX;i++)
	{
		key[i]=0;
	}	

	if (joystick == JOY_TYPE_NONE)
		logerror("Joystick not found\n");
	else
		logerror("Installed %s %s\n","Joystick", "GP2X");

	init_joy_list();

	if (use_mouse)
		use_mouse = 1;
	else
		use_mouse = 0;
}


void msdos_shutdown_input(void)
{
}
