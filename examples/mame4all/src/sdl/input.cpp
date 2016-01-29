#include "driver.h"
#include <SDL.h>

int use_mouse=1;
int joystick=0;

unsigned long ExKey1=0;
unsigned long ExKey2=0;
unsigned long ExKey3=0;
unsigned long ExKey4=0;
int num_joysticks=4;

int mouse_xrel=0;
int mouse_yrel=0;
Uint8 mouse_button=0;

#include "minimal.h"

static struct KeyboardInfo keylist[] =
{
	{ "A",			SDL_SCANCODE_A,				KEYCODE_A },
	{ "B",			SDL_SCANCODE_B,				KEYCODE_B },
	{ "C",			SDL_SCANCODE_C,				KEYCODE_C },
	{ "D",			SDL_SCANCODE_D,				KEYCODE_D },
	{ "E",			SDL_SCANCODE_E,				KEYCODE_E },
	{ "F",			SDL_SCANCODE_F,				KEYCODE_F },
	{ "G",			SDL_SCANCODE_G,				KEYCODE_G },
	{ "H",			SDL_SCANCODE_H,				KEYCODE_H },
	{ "I",			SDL_SCANCODE_I,				KEYCODE_I },
	{ "J",			SDL_SCANCODE_J,				KEYCODE_J },
	{ "K",			SDL_SCANCODE_K,				KEYCODE_K },
	{ "L",			SDL_SCANCODE_L,				KEYCODE_L },
	{ "M",			SDL_SCANCODE_M,				KEYCODE_M },
	{ "N",			SDL_SCANCODE_N,				KEYCODE_N },
	{ "O",			SDL_SCANCODE_O,				KEYCODE_O },
	{ "P",			SDL_SCANCODE_P,				KEYCODE_P },
	{ "Q",			SDL_SCANCODE_Q,				KEYCODE_Q },
	{ "R",			SDL_SCANCODE_R,				KEYCODE_R },
	{ "S",			SDL_SCANCODE_S,				KEYCODE_S },
	{ "T",			SDL_SCANCODE_T,				KEYCODE_T },
	{ "U",			SDL_SCANCODE_U,				KEYCODE_U },
	{ "V",			SDL_SCANCODE_V,				KEYCODE_V },
	{ "W",			SDL_SCANCODE_W,				KEYCODE_W },
	{ "X",			SDL_SCANCODE_X,				KEYCODE_X },
	{ "Y",			SDL_SCANCODE_Y,				KEYCODE_Y },
	{ "Z",			SDL_SCANCODE_Z,				KEYCODE_Z },
	{ "0",			SDL_SCANCODE_0,				KEYCODE_0 },
	{ "1",			SDL_SCANCODE_1,				KEYCODE_1 },
	{ "2",			SDL_SCANCODE_2,				KEYCODE_2 },
	{ "3",			SDL_SCANCODE_3,				KEYCODE_3 },
	{ "4",			SDL_SCANCODE_4,				KEYCODE_4 },
	{ "5",			SDL_SCANCODE_5,				KEYCODE_5 },
	{ "6",			SDL_SCANCODE_6,				KEYCODE_6 },
	{ "7",			SDL_SCANCODE_7,				KEYCODE_7 },
	{ "8",			SDL_SCANCODE_8,				KEYCODE_8 },
	{ "9",			SDL_SCANCODE_9,				KEYCODE_9 },
	{ "0 PAD",		SDL_SCANCODE_KP_0,			KEYCODE_0_PAD },
	{ "1 PAD",		SDL_SCANCODE_KP_1,			KEYCODE_1_PAD },
	{ "2 PAD",		SDL_SCANCODE_KP_2,			KEYCODE_2_PAD },
	{ "3 PAD",		SDL_SCANCODE_KP_3,			KEYCODE_3_PAD },
	{ "4 PAD",		SDL_SCANCODE_KP_4,			KEYCODE_4_PAD },
	{ "5 PAD",		SDL_SCANCODE_KP_5,			KEYCODE_5_PAD },
	{ "6 PAD",		SDL_SCANCODE_KP_6,			KEYCODE_6_PAD },
	{ "7 PAD",		SDL_SCANCODE_KP_7,			KEYCODE_7_PAD },
	{ "8 PAD",		SDL_SCANCODE_KP_8,			KEYCODE_8_PAD },
	{ "9 PAD",		SDL_SCANCODE_KP_9,			KEYCODE_9_PAD },
	{ "F1",			SDL_SCANCODE_F1,			KEYCODE_F1 },
	{ "F2",			SDL_SCANCODE_F2,			KEYCODE_F2 },
	{ "F3",			SDL_SCANCODE_F3,			KEYCODE_F3 },
	{ "F4",			SDL_SCANCODE_F4,			KEYCODE_F4 },
	{ "F5",			SDL_SCANCODE_F5,			KEYCODE_F5 },
	{ "F6",			SDL_SCANCODE_F6,			KEYCODE_F6 },
	{ "F7",			SDL_SCANCODE_F7,			KEYCODE_F7 },
	{ "F8",			SDL_SCANCODE_F8,			KEYCODE_F8 },
	{ "F9",			SDL_SCANCODE_F9,			KEYCODE_F9 },
	{ "F10",		SDL_SCANCODE_F10,			KEYCODE_F10 },
	{ "F11",		SDL_SCANCODE_F11,			KEYCODE_F11 },
	{ "F12",		SDL_SCANCODE_F12,			KEYCODE_F12 },
	{ "ESC",		SDL_SCANCODE_ESCAPE,		KEYCODE_ESC },
	{ "~",			SDL_SCANCODE_GRAVE,			KEYCODE_TILDE },
	{ "-",         	SDL_SCANCODE_MINUS,    		KEYCODE_MINUS },
	{ "=",         	SDL_SCANCODE_EQUALS,   		KEYCODE_EQUALS },
	{ "BKSPACE",	SDL_SCANCODE_BACKSPACE,		KEYCODE_BACKSPACE },
	{ "TAB",		SDL_SCANCODE_TAB,			KEYCODE_TAB },
	{ "[",         	SDL_SCANCODE_LEFTBRACKET,	KEYCODE_OPENBRACE },
	{ "]",         	SDL_SCANCODE_RIGHTBRACKET,	KEYCODE_CLOSEBRACE },
	{ "ENTER",		SDL_SCANCODE_RETURN,		KEYCODE_ENTER },
	{ ";",         	SDL_SCANCODE_SEMICOLON,    	KEYCODE_COLON },
	{ ":",         	SDL_SCANCODE_APOSTROPHE,   	KEYCODE_QUOTE },
	{ "\\",        	SDL_SCANCODE_BACKSLASH,    	KEYCODE_BACKSLASH },
	{ "<",         	SDL_SCANCODE_NONUSHASH,   	KEYCODE_BACKSLASH2 },
	{ ",",         	SDL_SCANCODE_COMMA,        	KEYCODE_COMMA },
	{ ".",         	SDL_SCANCODE_STOP,         	KEYCODE_STOP },
	{ "/",         	SDL_SCANCODE_SLASH,        	KEYCODE_SLASH },
	{ "SPACE",		SDL_SCANCODE_SPACE,			KEYCODE_SPACE },
	{ "INS",		SDL_SCANCODE_INSERT,		KEYCODE_INSERT },
	{ "DEL",		SDL_SCANCODE_DELETE,		KEYCODE_DEL },
	{ "HOME",		SDL_SCANCODE_HOME,			KEYCODE_HOME },
	{ "END",		SDL_SCANCODE_END,			KEYCODE_END },
	{ "PGUP",		SDL_SCANCODE_PAGEUP,		KEYCODE_PGUP },
	{ "PGDN",		SDL_SCANCODE_PAGEDOWN,		KEYCODE_PGDN },
	{ "LEFT",		SDL_SCANCODE_LEFT,			KEYCODE_LEFT },
	{ "RIGHT",		SDL_SCANCODE_RIGHT,			KEYCODE_RIGHT },
	{ "UP",			SDL_SCANCODE_UP,			KEYCODE_UP },
	{ "DOWN",		SDL_SCANCODE_DOWN,			KEYCODE_DOWN },
	{ "/ PAD",     	SDL_SCANCODE_KP_DIVIDE,    	KEYCODE_SLASH_PAD },
	{ "* PAD",     	SDL_SCANCODE_KP_MULTIPLY,  	KEYCODE_ASTERISK },
	{ "- PAD",     	SDL_SCANCODE_KP_MINUS,    	KEYCODE_MINUS_PAD },
	{ "+ PAD",     	SDL_SCANCODE_KP_PLUS,     	KEYCODE_PLUS_PAD },
	{ ". PAD",     	SDL_SCANCODE_KP_PERIOD,   	KEYCODE_DEL_PAD },
	{ "ENTER PAD", 	SDL_SCANCODE_KP_ENTER,    	KEYCODE_ENTER_PAD },
	{ "PRTSCR",    	SDL_SCANCODE_PRINTSCREEN,  	KEYCODE_PRTSCR },
	{ "PAUSE",     	SDL_SCANCODE_PAUSE,        	KEYCODE_PAUSE },
	{ "LSHIFT",		SDL_SCANCODE_LSHIFT,		KEYCODE_LSHIFT },
	{ "RSHIFT",		SDL_SCANCODE_RSHIFT,		KEYCODE_RSHIFT },
	{ "LCTRL",		SDL_SCANCODE_LCTRL,			KEYCODE_LCONTROL },
	{ "RCTRL",		SDL_SCANCODE_RCTRL,			KEYCODE_RCONTROL },
	{ "ALT",		SDL_SCANCODE_LALT,			KEYCODE_LALT },
	{ "ALTGR",		SDL_SCANCODE_RALT,			KEYCODE_RALT },
	{ "LWIN",		SDL_SCANCODE_LGUI,			KEYCODE_OTHER },
	{ "RWIN",		SDL_SCANCODE_RGUI,			KEYCODE_OTHER },
	{ "MENU",		SDL_SCANCODE_MENU,			KEYCODE_OTHER },
	{ "SCRLOCK",   	SDL_SCANCODE_SCROLLLOCK,	KEYCODE_SCRLOCK },
	{ "NUMLOCK",   	SDL_SCANCODE_NUMLOCKCLEAR,	KEYCODE_NUMLOCK },
	{ "CAPSLOCK",  	SDL_SCANCODE_CAPSLOCK, 		KEYCODE_CAPSLOCK },
	{ 0, 0, 0 }	/* end of table */
};

/* return a list of all available keys */
const struct KeyboardInfo *osd_get_key_list(void)
{
	return keylist;
}

static bool key[SDL_NUM_SCANCODES];

// Do the translation from SDL Key to mame key
// and set the mame keys
//
void keyprocess(SDL_Scancode inkey, bool pressed)
{
	key[inkey] = pressed;
}

int osd_is_sdlkey_pressed(int inkey)
{
	if (inkey >= SDL_arraysize(key)) {
		return 0;
	}
	return key[inkey];
}

void joyprocess(Uint8 button, SDL_bool pressed, Uint8 njoy)
{
    Uint32 val=0;
	unsigned long *mykey=0;

	if(njoy == 0) mykey = &ExKey1;
	else if(njoy == 1) mykey = &ExKey2;
	else if(njoy == 2) mykey = &ExKey3;
	else if(njoy == 3) mykey = &ExKey4;
	else return;

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
		case 9:
            val=GP2X_10; break;
		case 10:
            val=GP2X_11; break;
		case 11:
            val=GP2X_12; break;
		case 12:
            val=GP2X_13; break;
		case 13:
            val=GP2X_14; break;
		case 14:
            val=GP2X_15; break;
		case 15:
            val=GP2X_16; break;
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
	SDL_FlushEvents(SDL_KEYDOWN, SDL_MOUSEWHEEL);
	SDL_FlushEvents(SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERBUTTONUP);
	SDL_FlushEvents(SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERBUTTONUP);

	ExKey1=0;
	ExKey2=0;
	ExKey3=0;
	ExKey4=0;

	memset(key, 0, sizeof(key));
}



int osd_is_key_pressed(int keycode)
{
	if (keycode >= SDL_arraysize(key)) return 0;

	if (keycode == SDL_SCANCODE_PAUSE)
	{
		static int pressed,counter;
		int res;

		res = key[SDL_SCANCODE_PAUSE] ^ pressed;
		if (res)
		{
			if (counter > 0)
			{
				if (--counter == 0)
					pressed = key[SDL_SCANCODE_PAUSE];
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

int is_joy_axis_pressed (int axis, int dir, int joynum)
{
	/* Normal controls */
	if(dir == 1) { //left up
		if(gp2x_joystick_getaxis(joynum, axis) < -12000) return true;	
	}
	if(dir == 2) { //right down
		if(gp2x_joystick_getaxis(joynum, axis) > 12000) return true;	
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
	// Update both the keyboard and all joysticks
	gp2x_joystick_read();
}

int pos_analog_x=0;
int pos_analog_y=0;


/* return a value in the range -128 .. 128 (yes, 128, not 127) */
void osd_analogjoy_read(int player,int *analog_x, int *analog_y)
{
	*analog_x = *analog_y = 0;

	/* is there an analog joystick at all? */
	if (player+1 > num_joysticks || !joystick)
		return;

	if (!gp2x_joystick_connected(player)) return;

	pos_analog_x=gp2x_joystick_getaxis(player, 0)/256;	
	pos_analog_y=gp2x_joystick_getaxis(player, 1)/256;	

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
	if (use_mouse && player == 0 && !gp2x_joystick_connected(player)) 
	{
		*deltax = mouse_xrel*2;
		*deltay = mouse_yrel*2;
		return;
	}

   	*deltax=gp2x_joystick_getaxis(player, 0)/256/4;
   	*deltay=gp2x_joystick_getaxis(player, 1)/256/4;
}

void osd_customize_inputport_defaults(struct ipd *defaults)
{
}

void osd_led_w(int led,int on) 
{
}

void msdos_init_input (void)
{
	/* Initialize keyboard to not pressed */
	memset(key, 0, sizeof(key));

	if (joystick)
		logerror("Installed %s %s\n","Joystick", "GP2X");
	else
		logerror("Joystick not found\n");

	init_joy_list();

	if (use_mouse)
		use_mouse = 1;
	else
		use_mouse = 0;
}


void msdos_shutdown_input(void)
{
}
