/***************************************************************************

  inptport.c

  Input ports handling

TODO:	remove the 1 analog device per port limitation
		support for inputports producing interrupts
		support for extra "real" hardware (PC throttle's, spinners etc)

***************************************************************************/

#include "driver.h"
#include <math.h>

#ifdef MAME_NET
#include "network.h"

static unsigned short input_port_defaults[MAX_INPUT_PORTS];
static int default_player;
static int analog_player_port[MAX_INPUT_PORTS];
#endif /* MAME_NET */

/* Use the MRU code for 4way joysticks */
#define MRU_JOYSTICK

/* header identifying the version of the game.cfg file */
/* mame 0.36b11 */
#define MAMECFGSTRING_V5 "MAMECFG\5"
#define MAMEDEFSTRING_V5 "MAMEDEF\4"

/* mame 0.36b12 with multi key/joy extension */
#define MAMECFGSTRING_V6 "MAMECFG\6"
#define MAMEDEFSTRING_V6 "MAMEDEF\5"

/* mame 0.36b13 with and/or/not combination */
#define MAMECFGSTRING_V7 "MAMECFG\7"
#define MAMEDEFSTRING_V7 "MAMEDEF\6"

/* mame 0.36b16 with key/joy merge */
#define MAMECFGSTRING_V8 "MAMECFG\x8"
#define MAMEDEFSTRING_V8 "MAMEDEF\7"

extern void *record;
extern void *playback;

extern unsigned int dispensed_tickets;
extern unsigned int coins[COIN_COUNTERS];
extern unsigned int lastcoin[COIN_COUNTERS];
extern unsigned int coinlockedout[COIN_COUNTERS];

static unsigned short input_port_value[MAX_INPUT_PORTS];
static unsigned short input_vblank[MAX_INPUT_PORTS];

/* Assuming a maxium of one analog input device per port BW 101297 */
static struct InputPort *input_analog[MAX_INPUT_PORTS];
static int input_analog_current_value[MAX_INPUT_PORTS],input_analog_previous_value[MAX_INPUT_PORTS];
static int input_analog_init[MAX_INPUT_PORTS];

static int mouse_delta_x[OSD_MAX_JOY_ANALOG], mouse_delta_y[OSD_MAX_JOY_ANALOG];
static int analog_current_x[OSD_MAX_JOY_ANALOG], analog_current_y[OSD_MAX_JOY_ANALOG];
static int analog_previous_x[OSD_MAX_JOY_ANALOG], analog_previous_y[OSD_MAX_JOY_ANALOG];

#ifdef ENABLE_AUTOFIRE
int seq_pressed_with_autofire(struct InputPort *in, InputSeq *code);
#endif


/***************************************************************************

  Configuration load/save

***************************************************************************/

/* this must match the enum in inptport.h */
char ipdn_defaultstrings[][MAX_DEFSTR_LEN] =
{
	"Off",
	"On",
	"No",
	"Yes",
	"Lives",
	"Bonus Life",
	"Difficulty",
	"Demo Sounds",
	"Coinage",
	"Coin A",
	"Coin B",
	"9 Coins/1 Credit",
	"8 Coins/1 Credit",
	"7 Coins/1 Credit",
	"6 Coins/1 Credit",
	"5 Coins/1 Credit",
	"4 Coins/1 Credit",
	"3 Coins/1 Credit",
	"8 Coins/3 Credits",
	"4 Coins/2 Credits",
	"2 Coins/1 Credit",
	"5 Coins/3 Credits",
	"3 Coins/2 Credits",
	"4 Coins/3 Credits",
	"4 Coins/4 Credits",
	"3 Coins/3 Credits",
	"2 Coins/2 Credits",
	"1 Coin/1 Credit",
	"4 Coins/5 Credits",
	"3 Coins/4 Credits",
	"2 Coins/3 Credits",
	"4 Coins/7 Credits",
	"2 Coins/4 Credits",
	"1 Coin/2 Credits",
	"2 Coins/5 Credits",
	"2 Coins/6 Credits",
	"1 Coin/3 Credits",
	"2 Coins/7 Credits",
	"2 Coins/8 Credits",
	"1 Coin/4 Credits",
	"1 Coin/5 Credits",
	"1 Coin/6 Credits",
	"1 Coin/7 Credits",
	"1 Coin/8 Credits",
	"1 Coin/9 Credits",
	"Free Play",
	"Cabinet",
	"Upright",
	"Cocktail",
	"Flip Screen",
	"Service Mode",
	"Unused",
	"Unknown"
};

struct ipd inputport_defaults[] =
{
	{ IPT_UI_CONFIGURE,         "Config Menu",       SEQ_DEF_1(KEYCODE_TAB) },
	{ IPT_UI_ON_SCREEN_DISPLAY, "On Screen Display", SEQ_DEF_1(KEYCODE_TILDE) },
	{ IPT_UI_PAUSE,             "Pause",             SEQ_DEF_1(KEYCODE_P) },
	{ IPT_UI_RESET_MACHINE,     "Reset Game",        SEQ_DEF_1(KEYCODE_F3) },
	{ IPT_UI_SHOW_GFX,          "Show Gfx",          SEQ_DEF_1(KEYCODE_F4) },
	{ IPT_UI_FRAMESKIP_DEC,     "Frameskip Dec",     SEQ_DEF_1(KEYCODE_F8) },
	{ IPT_UI_FRAMESKIP_INC,     "Frameskip Inc",     SEQ_DEF_1(KEYCODE_F9) },
	{ IPT_UI_THROTTLE,          "Throttle",          SEQ_DEF_1(KEYCODE_F10) },
	{ IPT_UI_SHOW_FPS,          "Show FPS",          SEQ_DEF_5(KEYCODE_F11, CODE_NOT, KEYCODE_LCONTROL, CODE_NOT, KEYCODE_LSHIFT) },
	{ IPT_UI_SHOW_PROFILER,     "Show Profiler",     SEQ_DEF_2(KEYCODE_F11, KEYCODE_LSHIFT) },
#ifdef MESS
	{ IPT_UI_TOGGLE_UI,         "UI Toggle",         SEQ_DEF_1(KEYCODE_SCRLOCK) },
#endif
	{ IPT_UI_SNAPSHOT,          "Save Snapshot",     SEQ_DEF_1(KEYCODE_F12) },
	{ IPT_UI_TOGGLE_CHEAT,      "Toggle Cheat",      SEQ_DEF_1(KEYCODE_F5) },
	{ IPT_UI_UP,                "UI Up",             SEQ_DEF_3(KEYCODE_UP, CODE_OR, JOYCODE_1_UP) },
	{ IPT_UI_DOWN,              "UI Down",           SEQ_DEF_3(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN) },
	{ IPT_UI_LEFT,              "UI Left",           SEQ_DEF_3(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT) },
	{ IPT_UI_RIGHT,             "UI Right",          SEQ_DEF_3(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT) },
	{ IPT_UI_SELECT,            "UI Select",         SEQ_DEF_3(KEYCODE_ENTER, CODE_OR, JOYCODE_1_BUTTON1) },
	{ IPT_UI_CANCEL,            "UI Cancel",         SEQ_DEF_1(KEYCODE_ESC) },
	{ IPT_UI_PAN_UP,            "Pan Up",            SEQ_DEF_3(KEYCODE_PGUP, CODE_NOT, KEYCODE_LSHIFT) },
	{ IPT_UI_PAN_DOWN,          "Pan Down",          SEQ_DEF_3(KEYCODE_PGDN, CODE_NOT, KEYCODE_LSHIFT) },
	{ IPT_UI_PAN_LEFT,          "Pan Left",          SEQ_DEF_2(KEYCODE_PGUP, KEYCODE_LSHIFT) },
	{ IPT_UI_PAN_RIGHT,         "Pan Right",         SEQ_DEF_2(KEYCODE_PGDN, KEYCODE_LSHIFT) },
	{ IPT_START1, "1 Player Start",  SEQ_DEF_1(KEYCODE_1) },
	{ IPT_START2, "2 Players Start", SEQ_DEF_1(KEYCODE_2) },
	{ IPT_START3, "3 Players Start", SEQ_DEF_1(KEYCODE_3) },
	{ IPT_START4, "4 Players Start", SEQ_DEF_1(KEYCODE_4) },
	{ IPT_COIN1,  "Coin 1",          SEQ_DEF_1(KEYCODE_5) },
	{ IPT_COIN2,  "Coin 2",          SEQ_DEF_1(KEYCODE_6) },
	{ IPT_COIN3,  "Coin 3",          SEQ_DEF_1(KEYCODE_7) },
	{ IPT_COIN4,  "Coin 4",          SEQ_DEF_1(KEYCODE_8) },
	{ IPT_SERVICE1, "Service 1",     SEQ_DEF_1(KEYCODE_9) },
	{ IPT_SERVICE2, "Service 2",     SEQ_DEF_1(KEYCODE_0) },
	{ IPT_SERVICE3, "Service 3",     SEQ_DEF_1(KEYCODE_MINUS) },
	{ IPT_SERVICE4, "Service 4",     SEQ_DEF_1(KEYCODE_EQUALS) },
#ifndef MESS
	{ IPT_TILT,   "Tilt",            SEQ_DEF_1(KEYCODE_T) },
#else
	{ IPT_TILT,   "Tilt",            SEQ_DEF_0 },
#endif

	{ IPT_JOYSTICK_UP         | IPF_PLAYER1, "P1 Up",          SEQ_DEF_3(KEYCODE_UP, CODE_OR, JOYCODE_1_UP)    },
	{ IPT_JOYSTICK_DOWN       | IPF_PLAYER1, "P1 Down",        SEQ_DEF_3(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN)  },
	{ IPT_JOYSTICK_LEFT       | IPF_PLAYER1, "P1 Left",        SEQ_DEF_3(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT)  },
	{ IPT_JOYSTICK_RIGHT      | IPF_PLAYER1, "P1 Right",       SEQ_DEF_3(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT) },
	{ IPT_BUTTON1             | IPF_PLAYER1, "P1 Button 1",    SEQ_DEF_5(KEYCODE_LCONTROL, CODE_OR, JOYCODE_1_BUTTON1, CODE_OR, JOYCODE_1_MOUSE1) },
	{ IPT_BUTTON2             | IPF_PLAYER1, "P1 Button 2",    SEQ_DEF_5(KEYCODE_LALT, CODE_OR, JOYCODE_1_BUTTON2, CODE_OR, JOYCODE_1_MOUSE2) },
	{ IPT_BUTTON3             | IPF_PLAYER1, "P1 Button 3",    SEQ_DEF_5(KEYCODE_SPACE, CODE_OR, JOYCODE_1_BUTTON3, CODE_OR, JOYCODE_1_MOUSE3) },
	{ IPT_BUTTON4             | IPF_PLAYER1, "P1 Button 4",    SEQ_DEF_3(KEYCODE_LSHIFT, CODE_OR, JOYCODE_1_BUTTON4) },
	{ IPT_BUTTON5             | IPF_PLAYER1, "P1 Button 5",    SEQ_DEF_3(KEYCODE_Z, CODE_OR, JOYCODE_1_BUTTON5) },
	{ IPT_BUTTON6             | IPF_PLAYER1, "P1 Button 6",    SEQ_DEF_3(KEYCODE_X, CODE_OR, JOYCODE_1_BUTTON6) },
	{ IPT_BUTTON7             | IPF_PLAYER1, "P1 Button 7",    SEQ_DEF_3(KEYCODE_C, CODE_OR, JOYCODE_1_BUTTON7) },
	{ IPT_BUTTON8             | IPF_PLAYER1, "P1 Button 8",    SEQ_DEF_3(KEYCODE_V, CODE_OR, JOYCODE_1_BUTTON8) },
	{ IPT_BUTTON9             | IPF_PLAYER1, "P1 Button 9",    SEQ_DEF_3(KEYCODE_B, CODE_OR, JOYCODE_1_BUTTON9) },
	{ IPT_BUTTON10            | IPF_PLAYER1, "P1 Button 10",   SEQ_DEF_3(KEYCODE_N, CODE_OR, JOYCODE_1_BUTTON10) },
	{ IPT_BUTTON11            | IPF_PLAYER2, "P1 Button 11",   SEQ_DEF_1(JOYCODE_1_BUTTON11) },
	{ IPT_BUTTON12            | IPF_PLAYER2, "P1 Button 12",   SEQ_DEF_1(JOYCODE_1_BUTTON12) },
	{ IPT_BUTTON13            | IPF_PLAYER2, "P1 Button 13",   SEQ_DEF_1(JOYCODE_1_BUTTON13) },
	{ IPT_BUTTON14            | IPF_PLAYER2, "P1 Button 14",   SEQ_DEF_1(JOYCODE_1_BUTTON14) },
	{ IPT_BUTTON15            | IPF_PLAYER2, "P1 Button 15",   SEQ_DEF_1(JOYCODE_1_BUTTON15) },
	{ IPT_BUTTON16            | IPF_PLAYER2, "P1 Button 16",   SEQ_DEF_1(JOYCODE_1_BUTTON16) },
	{ IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER1, "P1 Right/Up",    SEQ_DEF_3(KEYCODE_I, CODE_OR, JOYCODE_1_BUTTON4) },
	{ IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER1, "P1 Right/Down",  SEQ_DEF_3(KEYCODE_K, CODE_OR, JOYCODE_1_BUTTON2) },
	{ IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER1, "P1 Right/Left",  SEQ_DEF_3(KEYCODE_J, CODE_OR, JOYCODE_1_BUTTON3) },
	{ IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER1, "P1 Right/Right", SEQ_DEF_3(KEYCODE_L, CODE_OR, JOYCODE_1_BUTTON1) },
	{ IPT_JOYSTICKLEFT_UP     | IPF_PLAYER1, "P1 Left/Up",     SEQ_DEF_3(KEYCODE_E, CODE_OR, JOYCODE_1_UP) },
	{ IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER1, "P1 Left/Down",   SEQ_DEF_3(KEYCODE_D, CODE_OR, JOYCODE_1_DOWN) },
	{ IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER1, "P1 Left/Left",   SEQ_DEF_3(KEYCODE_S, CODE_OR, JOYCODE_1_LEFT) },
	{ IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER1, "P1 Left/Right",  SEQ_DEF_3(KEYCODE_F, CODE_OR, JOYCODE_1_RIGHT) },

	{ IPT_JOYSTICK_UP         | IPF_PLAYER2, "P2 Up",          SEQ_DEF_3(KEYCODE_R, CODE_OR, JOYCODE_2_UP)    },
	{ IPT_JOYSTICK_DOWN       | IPF_PLAYER2, "P2 Down",        SEQ_DEF_3(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN)  },
	{ IPT_JOYSTICK_LEFT       | IPF_PLAYER2, "P2 Left",        SEQ_DEF_3(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT)  },
	{ IPT_JOYSTICK_RIGHT      | IPF_PLAYER2, "P2 Right",       SEQ_DEF_3(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT) },
	{ IPT_BUTTON1             | IPF_PLAYER2, "P2 Button 1",    SEQ_DEF_3(KEYCODE_A, CODE_OR, JOYCODE_2_BUTTON1) },
	{ IPT_BUTTON2             | IPF_PLAYER2, "P2 Button 2",    SEQ_DEF_3(KEYCODE_S, CODE_OR, JOYCODE_2_BUTTON2) },
	{ IPT_BUTTON3             | IPF_PLAYER2, "P2 Button 3",    SEQ_DEF_3(KEYCODE_Q, CODE_OR, JOYCODE_2_BUTTON3) },
	{ IPT_BUTTON4             | IPF_PLAYER2, "P2 Button 4",    SEQ_DEF_3(KEYCODE_W, CODE_OR, JOYCODE_2_BUTTON4) },
	{ IPT_BUTTON5             | IPF_PLAYER2, "P2 Button 5",    SEQ_DEF_1(JOYCODE_2_BUTTON5) },
	{ IPT_BUTTON6             | IPF_PLAYER2, "P2 Button 6",    SEQ_DEF_1(JOYCODE_2_BUTTON6) },
	{ IPT_BUTTON7             | IPF_PLAYER2, "P2 Button 7",    SEQ_DEF_1(JOYCODE_2_BUTTON7) },
	{ IPT_BUTTON8             | IPF_PLAYER2, "P2 Button 8",    SEQ_DEF_1(JOYCODE_2_BUTTON8) },
	{ IPT_BUTTON9             | IPF_PLAYER2, "P2 Button 9",    SEQ_DEF_1(JOYCODE_2_BUTTON9) },
	{ IPT_BUTTON10            | IPF_PLAYER2, "P2 Button 10",   SEQ_DEF_1(JOYCODE_2_BUTTON10) },
	{ IPT_BUTTON11            | IPF_PLAYER2, "P2 Button 11",   SEQ_DEF_1(JOYCODE_2_BUTTON11) },
	{ IPT_BUTTON12            | IPF_PLAYER2, "P2 Button 12",   SEQ_DEF_1(JOYCODE_2_BUTTON12) },
	{ IPT_BUTTON13            | IPF_PLAYER2, "P2 Button 13",   SEQ_DEF_1(JOYCODE_2_BUTTON13) },
	{ IPT_BUTTON14            | IPF_PLAYER2, "P2 Button 14",   SEQ_DEF_1(JOYCODE_2_BUTTON14) },
	{ IPT_BUTTON15            | IPF_PLAYER2, "P2 Button 15",   SEQ_DEF_1(JOYCODE_2_BUTTON15) },
	{ IPT_BUTTON16            | IPF_PLAYER2, "P2 Button 16",   SEQ_DEF_1(JOYCODE_2_BUTTON16) },
	{ IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER2, "P2 Right/Up",    SEQ_DEF_0 },
	{ IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER2, "P2 Right/Down",  SEQ_DEF_0 },
	{ IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER2, "P2 Right/Left",  SEQ_DEF_0 },
	{ IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2, "P2 Right/Right", SEQ_DEF_0 },
	{ IPT_JOYSTICKLEFT_UP     | IPF_PLAYER2, "P2 Left/Up",     SEQ_DEF_0 },
	{ IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER2, "P2 Left/Down",   SEQ_DEF_0 },
	{ IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER2, "P2 Left/Left",   SEQ_DEF_0 },
	{ IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER2, "P2 Left/Right",  SEQ_DEF_0 },

	{ IPT_JOYSTICK_UP         | IPF_PLAYER3, "P3 Up",          SEQ_DEF_3(KEYCODE_I, CODE_OR, JOYCODE_3_UP)    },
	{ IPT_JOYSTICK_DOWN       | IPF_PLAYER3, "P3 Down",        SEQ_DEF_3(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN)  },
	{ IPT_JOYSTICK_LEFT       | IPF_PLAYER3, "P3 Left",        SEQ_DEF_3(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT)  },
	{ IPT_JOYSTICK_RIGHT      | IPF_PLAYER3, "P3 Right",       SEQ_DEF_3(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT) },
	{ IPT_BUTTON1             | IPF_PLAYER3, "P3 Button 1",    SEQ_DEF_3(KEYCODE_RCONTROL, CODE_OR, JOYCODE_3_BUTTON1) },
	{ IPT_BUTTON2             | IPF_PLAYER3, "P3 Button 2",    SEQ_DEF_3(KEYCODE_RSHIFT, CODE_OR, JOYCODE_3_BUTTON2) },
	{ IPT_BUTTON3             | IPF_PLAYER3, "P3 Button 3",    SEQ_DEF_3(KEYCODE_ENTER, CODE_OR, JOYCODE_3_BUTTON3) },
	{ IPT_BUTTON4             | IPF_PLAYER3, "P3 Button 4",    SEQ_DEF_1(JOYCODE_3_BUTTON4) },

	{ IPT_JOYSTICK_UP         | IPF_PLAYER4, "P4 Up",          SEQ_DEF_1(JOYCODE_4_UP) },
	{ IPT_JOYSTICK_DOWN       | IPF_PLAYER4, "P4 Down",        SEQ_DEF_1(JOYCODE_4_DOWN) },
	{ IPT_JOYSTICK_LEFT       | IPF_PLAYER4, "P4 Left",        SEQ_DEF_1(JOYCODE_4_LEFT) },
	{ IPT_JOYSTICK_RIGHT      | IPF_PLAYER4, "P4 Right",       SEQ_DEF_1(JOYCODE_4_RIGHT) },
	{ IPT_BUTTON1             | IPF_PLAYER4, "P4 Button 1",    SEQ_DEF_1(JOYCODE_4_BUTTON1) },
	{ IPT_BUTTON2             | IPF_PLAYER4, "P4 Button 2",    SEQ_DEF_1(JOYCODE_4_BUTTON2) },
	{ IPT_BUTTON3             | IPF_PLAYER4, "P4 Button 3",    SEQ_DEF_1(JOYCODE_4_BUTTON3) },
	{ IPT_BUTTON4             | IPF_PLAYER4, "P4 Button 4",    SEQ_DEF_1(JOYCODE_4_BUTTON4) },

	{ IPT_PEDAL	                | IPF_PLAYER1, "Pedal 1",        SEQ_DEF_3(KEYCODE_LCONTROL, CODE_OR, JOYCODE_1_BUTTON1) },
	{ (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER1, "P1 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
	{ IPT_PEDAL                 | IPF_PLAYER2, "Pedal 2",        SEQ_DEF_3(KEYCODE_A, CODE_OR, JOYCODE_2_BUTTON1) },
	{ (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER2, "P2 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
	{ IPT_PEDAL                 | IPF_PLAYER3, "Pedal 3",        SEQ_DEF_3(KEYCODE_RCONTROL, CODE_OR, JOYCODE_3_BUTTON1) },
	{ (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER3, "P3 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
	{ IPT_PEDAL                 | IPF_PLAYER4, "Pedal 4",        SEQ_DEF_1(JOYCODE_4_BUTTON1) },
	{ (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER4, "P4 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },

	{ IPT_PADDLE | IPF_PLAYER1,  "Paddle",        SEQ_DEF_3(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT) },
	{ (IPT_PADDLE | IPF_PLAYER1)+IPT_EXTENSION,             "Paddle",        SEQ_DEF_3(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT)  },
	{ IPT_PADDLE | IPF_PLAYER2,  "Paddle 2",      SEQ_DEF_3(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT) },
	{ (IPT_PADDLE | IPF_PLAYER2)+IPT_EXTENSION,             "Paddle 2",      SEQ_DEF_3(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT) },
	{ IPT_PADDLE | IPF_PLAYER3,  "Paddle 3",      SEQ_DEF_3(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT) },
	{ (IPT_PADDLE | IPF_PLAYER3)+IPT_EXTENSION,             "Paddle 3",      SEQ_DEF_3(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT) },
	{ IPT_PADDLE | IPF_PLAYER4,  "Paddle 4",      SEQ_DEF_1(JOYCODE_4_LEFT) },
	{ (IPT_PADDLE | IPF_PLAYER4)+IPT_EXTENSION,             "Paddle 4",      SEQ_DEF_1(JOYCODE_4_RIGHT) },
	{ IPT_PADDLE_V | IPF_PLAYER1,  "Paddle V",          SEQ_DEF_3(KEYCODE_UP, CODE_OR, JOYCODE_1_UP) },
	{ (IPT_PADDLE_V | IPF_PLAYER1)+IPT_EXTENSION,             "Paddle V",          SEQ_DEF_3(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN) },
	{ IPT_PADDLE_V | IPF_PLAYER2,  "Paddle V 2",        SEQ_DEF_3(KEYCODE_R, CODE_OR, JOYCODE_2_UP) },
	{ (IPT_PADDLE_V | IPF_PLAYER2)+IPT_EXTENSION,             "Paddle V 2",      SEQ_DEF_3(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN) },
	{ IPT_PADDLE_V | IPF_PLAYER3,  "Paddle V 3",        SEQ_DEF_3(KEYCODE_I, CODE_OR, JOYCODE_3_UP) },
	{ (IPT_PADDLE_V | IPF_PLAYER3)+IPT_EXTENSION,             "Paddle V 3",      SEQ_DEF_3(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN) },
	{ IPT_PADDLE_V | IPF_PLAYER4,  "Paddle V 4",        SEQ_DEF_1(JOYCODE_4_UP) },
	{ (IPT_PADDLE_V | IPF_PLAYER4)+IPT_EXTENSION,             "Paddle V 4",      SEQ_DEF_1(JOYCODE_4_DOWN) },
	{ IPT_DIAL | IPF_PLAYER1,    "Dial",          SEQ_DEF_3(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT) },
	{ (IPT_DIAL | IPF_PLAYER1)+IPT_EXTENSION,               "Dial",          SEQ_DEF_3(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT) },
	{ IPT_DIAL | IPF_PLAYER2,    "Dial 2",        SEQ_DEF_3(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT) },
	{ (IPT_DIAL | IPF_PLAYER2)+IPT_EXTENSION,               "Dial 2",      SEQ_DEF_3(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT) },
	{ IPT_DIAL | IPF_PLAYER3,    "Dial 3",        SEQ_DEF_3(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT) },
	{ (IPT_DIAL | IPF_PLAYER3)+IPT_EXTENSION,               "Dial 3",      SEQ_DEF_3(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT) },
	{ IPT_DIAL | IPF_PLAYER4,    "Dial 4",        SEQ_DEF_1(JOYCODE_4_LEFT) },
	{ (IPT_DIAL | IPF_PLAYER4)+IPT_EXTENSION,               "Dial 4",      SEQ_DEF_1(JOYCODE_4_RIGHT) },
	{ IPT_DIAL_V | IPF_PLAYER1,  "Dial V",          SEQ_DEF_3(KEYCODE_UP, CODE_OR, JOYCODE_1_UP) },
	{ (IPT_DIAL_V | IPF_PLAYER1)+IPT_EXTENSION,             "Dial V",          SEQ_DEF_3(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN) },
	{ IPT_DIAL_V | IPF_PLAYER2,  "Dial V 2",        SEQ_DEF_3(KEYCODE_R, CODE_OR, JOYCODE_2_UP) },
	{ (IPT_DIAL_V | IPF_PLAYER2)+IPT_EXTENSION,             "Dial V 2",      SEQ_DEF_3(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN) },
	{ IPT_DIAL_V | IPF_PLAYER3,  "Dial V 3",        SEQ_DEF_3(KEYCODE_I, CODE_OR, JOYCODE_3_UP) },
	{ (IPT_DIAL_V | IPF_PLAYER3)+IPT_EXTENSION,             "Dial V 3",      SEQ_DEF_3(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN) },
	{ IPT_DIAL_V | IPF_PLAYER4,  "Dial V 4",        SEQ_DEF_1(JOYCODE_4_UP) },
	{ (IPT_DIAL_V | IPF_PLAYER4)+IPT_EXTENSION,             "Dial V 4",      SEQ_DEF_1(JOYCODE_4_DOWN) },

	{ IPT_TRACKBALL_X | IPF_PLAYER1, "Track X",   SEQ_DEF_3(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT) },
	{ (IPT_TRACKBALL_X | IPF_PLAYER1)+IPT_EXTENSION,                 "Track X",   SEQ_DEF_3(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT) },
	{ IPT_TRACKBALL_X | IPF_PLAYER2, "Track X 2", SEQ_DEF_3(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT) },
	{ (IPT_TRACKBALL_X | IPF_PLAYER2)+IPT_EXTENSION,                 "Track X 2", SEQ_DEF_3(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT) },
	{ IPT_TRACKBALL_X | IPF_PLAYER3, "Track X 3", SEQ_DEF_3(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT) },
	{ (IPT_TRACKBALL_X | IPF_PLAYER3)+IPT_EXTENSION,                 "Track X 3", SEQ_DEF_3(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT) },
	{ IPT_TRACKBALL_X | IPF_PLAYER4, "Track X 4", SEQ_DEF_1(JOYCODE_4_LEFT) },
	{ (IPT_TRACKBALL_X | IPF_PLAYER4)+IPT_EXTENSION,                 "Track X 4", SEQ_DEF_1(JOYCODE_4_RIGHT) },

	{ IPT_TRACKBALL_Y | IPF_PLAYER1, "Track Y",   SEQ_DEF_3(KEYCODE_UP, CODE_OR, JOYCODE_1_UP) },
	{ (IPT_TRACKBALL_Y | IPF_PLAYER1)+IPT_EXTENSION,                 "Track Y",   SEQ_DEF_3(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN) },
	{ IPT_TRACKBALL_Y | IPF_PLAYER2, "Track Y 2", SEQ_DEF_3(KEYCODE_R, CODE_OR, JOYCODE_2_UP) },
	{ (IPT_TRACKBALL_Y | IPF_PLAYER2)+IPT_EXTENSION,                 "Track Y 2", SEQ_DEF_3(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN) },
	{ IPT_TRACKBALL_Y | IPF_PLAYER3, "Track Y 3", SEQ_DEF_3(KEYCODE_I, CODE_OR, JOYCODE_3_UP) },
	{ (IPT_TRACKBALL_Y | IPF_PLAYER3)+IPT_EXTENSION,                 "Track Y 3", SEQ_DEF_3(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN) },
	{ IPT_TRACKBALL_Y | IPF_PLAYER4, "Track Y 4", SEQ_DEF_1(JOYCODE_4_UP) },
	{ (IPT_TRACKBALL_Y | IPF_PLAYER4)+IPT_EXTENSION,                 "Track Y 4", SEQ_DEF_1(JOYCODE_4_DOWN) },

	{ IPT_AD_STICK_X | IPF_PLAYER1, "AD Stick X",   SEQ_DEF_3(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT) },
	{ (IPT_AD_STICK_X | IPF_PLAYER1)+IPT_EXTENSION,                "AD Stick X",   SEQ_DEF_3(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT) },
	{ IPT_AD_STICK_X | IPF_PLAYER2, "AD Stick X 2", SEQ_DEF_3(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT) },
	{ (IPT_AD_STICK_X | IPF_PLAYER2)+IPT_EXTENSION,                "AD Stick X 2", SEQ_DEF_3(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT) },
	{ IPT_AD_STICK_X | IPF_PLAYER3, "AD Stick X 3", SEQ_DEF_3(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT) },
	{ (IPT_AD_STICK_X | IPF_PLAYER3)+IPT_EXTENSION,                "AD Stick X 3", SEQ_DEF_3(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT) },
	{ IPT_AD_STICK_X | IPF_PLAYER4, "AD Stick X 4", SEQ_DEF_1(JOYCODE_4_LEFT) },
	{ (IPT_AD_STICK_X | IPF_PLAYER4)+IPT_EXTENSION,                "AD Stick X 4", SEQ_DEF_1(JOYCODE_4_RIGHT) },

	{ IPT_AD_STICK_Y | IPF_PLAYER1, "AD Stick Y",   SEQ_DEF_3(KEYCODE_UP, CODE_OR, JOYCODE_1_UP) },
	{ (IPT_AD_STICK_Y | IPF_PLAYER1)+IPT_EXTENSION,                "AD Stick Y",   SEQ_DEF_3(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN) },
	{ IPT_AD_STICK_Y | IPF_PLAYER2, "AD Stick Y 2", SEQ_DEF_3(KEYCODE_R, CODE_OR, JOYCODE_2_UP) },
	{ (IPT_AD_STICK_Y | IPF_PLAYER2)+IPT_EXTENSION,                "AD Stick Y 2", SEQ_DEF_3(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN) },
	{ IPT_AD_STICK_Y | IPF_PLAYER3, "AD Stick Y 3", SEQ_DEF_3(KEYCODE_I, CODE_OR, JOYCODE_3_UP) },
	{ (IPT_AD_STICK_Y | IPF_PLAYER3)+IPT_EXTENSION,                "AD Stick Y 3", SEQ_DEF_3(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN) },
	{ IPT_AD_STICK_Y | IPF_PLAYER4, "AD Stick Y 4", SEQ_DEF_1(JOYCODE_4_UP) },
	{ (IPT_AD_STICK_Y | IPF_PLAYER4)+IPT_EXTENSION,                "AD Stick Y 4", SEQ_DEF_1(JOYCODE_4_DOWN) },

	{ IPT_UNKNOWN,             "UNKNOWN",         SEQ_DEF_0 },
	{ IPT_END,                 0,                 SEQ_DEF_0 }	/* returned when there is no match */
};

struct ipd inputport_defaults_backup[sizeof(inputport_defaults)/sizeof(struct ipd)];

/***************************************************************************/
/* Generic IO */

static int readint(void *f,UINT32 *num)
{
	unsigned i;

	*num = 0;
	for (i = 0;i < sizeof(UINT32);i++)
	{
		unsigned char c;


		*num <<= 8;
		if (osd_fread(f,&c,1) != 1)
			return -1;
		*num |= c;
	}

	return 0;
}

static void writeint(void *f,UINT32 num)
{
	unsigned i;

	for (i = 0;i < sizeof(UINT32);i++)
	{
		unsigned char c;


		c = (num >> 8 * (sizeof(UINT32)-1)) & 0xff;
		osd_fwrite(f,&c,1);
		num <<= 8;
	}
}

static int readword(void *f,UINT16 *num)
{
	unsigned i;
	int res;

	res = 0;
	for (i = 0;i < sizeof(UINT16);i++)
	{
		unsigned char c;


		res <<= 8;
		if (osd_fread(f,&c,1) != 1)
			return -1;
		res |= c;
	}

	*num = res;
	return 0;
}

static void writeword(void *f,UINT16 num)
{
	unsigned i;

	for (i = 0;i < sizeof(UINT16);i++)
	{
		unsigned char c;


		c = (num >> 8 * (sizeof(UINT16)-1)) & 0xff;
		osd_fwrite(f,&c,1);
		num <<= 8;
	}
}

#ifndef NOLEGACY
#include "legacy.h"
#endif

static int seq_read_ver_8(void* f, InputSeq* seq)
{
	int j,len;
	UINT32 i;
	UINT16 w;

	if (readword(f,&w) != 0)
		return -1;

	len = w;
	seq_set_0(seq);
	for(j=0;j<len;++j)
	{
		if (readint(f,&i) != 0)
 			return -1;
		(*seq)[j] = savecode_to_code(i);
 	}

 	return 0;
  }

static int seq_read(void* f, InputSeq* seq, int ver)
  {
#ifdef NOLEGACY
	if (ver==8)
		return seq_read_ver_8(f,seq);
#else
	switch (ver) {
		case 5 : return seq_read_ver_5(f,seq);
		case 6 : return seq_read_ver_6(f,seq);
		case 7 : return seq_read_ver_7(f,seq);
		case 8 : return seq_read_ver_8(f,seq);
	}
#endif
	return -1;
  }

static void seq_write(void* f, InputSeq* seq)
  {
	int j,len;
        for(len=0;len<SEQ_MAX;++len)
		if ((*seq)[len] == CODE_NONE)
			break;
	writeword(f,len);
	for(j=0;j<len;++j)
		writeint(f, code_to_savecode( (*seq)[j] ));
  }

/***************************************************************************/
/* Load */

static void load_default_keys(void)
{
	void *f;


	osd_customize_inputport_defaults(inputport_defaults);
	memcpy(inputport_defaults_backup,inputport_defaults,sizeof(inputport_defaults));

	if ((f = osd_fopen("default",0,OSD_FILETYPE_CONFIG,0)) != 0)
	{
		char buf[8];
		int version;

		/* read header */
		if (osd_fread(f,buf,8) != 8)
			goto getout;

		if (memcmp(buf,MAMEDEFSTRING_V5,8) == 0)
			version = 5;
		else if (memcmp(buf,MAMEDEFSTRING_V6,8) == 0)
			version = 6;
		else if (memcmp(buf,MAMEDEFSTRING_V7,8) == 0)
			version = 7;
		else if (memcmp(buf,MAMEDEFSTRING_V8,8) == 0)
			version = 8;
		else
			goto getout;	/* header invalid */

		for (;;)
		{
			UINT32 type;
			InputSeq def_seq;
			InputSeq seq;
			int i;

			if (readint(f,&type) != 0)
				goto getout;

			if (seq_read(f,&def_seq,version)!=0)
				goto getout;
			if (seq_read(f,&seq,version)!=0)
				goto getout;

			i = 0;
			while (inputport_defaults[i].type != IPT_END)
			{
				if (inputport_defaults[i].type == type)
				{
					/* load stored settings only if the default hasn't changed */
					if (seq_cmp(&inputport_defaults[i].seq,&def_seq)==0)
						seq_copy(&inputport_defaults[i].seq,&seq);
				}

				i++;
			}
		}

getout:
		osd_fclose(f);
	}
}

static void save_default_keys(void)
{
	void *f;


	if ((f = osd_fopen("default",0,OSD_FILETYPE_CONFIG,1)) != 0)
	{
		int i;


		/* write header */
		osd_fwrite(f,MAMEDEFSTRING_V8,8);

		i = 0;
		while (inputport_defaults[i].type != IPT_END)
		{
			writeint(f,inputport_defaults[i].type);

			seq_write(f,&inputport_defaults_backup[i].seq);
			seq_write(f,&inputport_defaults[i].seq);

			i++;
		}

		osd_fclose(f);
		osd_fsync();
	}
	memcpy(inputport_defaults,inputport_defaults_backup,sizeof(inputport_defaults_backup));
}

static int input_port_read_ver_8(void *f,struct InputPort *in)
{
	UINT32 i;
	UINT16 w;
	if (readint(f,&i) != 0)
		return -1;
	in->type = i;

	if (readword(f,&w) != 0)
		return -1;
	in->mask = w;

	if (readword(f,&w) != 0)
		return -1;
	in->default_value = w;

	if (seq_read_ver_8(f,&in->seq) != 0)
		return -1;

	return 0;
}

static int input_port_read(void *f,struct InputPort *in, int ver)
{
#ifdef NOLEGACY
	if (ver==8)
		return input_port_read_ver_8(f,in);
#else
	switch (ver) {
		case 5 : return	input_port_read_ver_5(f,in);
		case 6 : return	input_port_read_ver_6(f,in);
		case 7 : return	input_port_read_ver_7(f,in);
		case 8 : return	input_port_read_ver_8(f,in);
	}
#endif
	return -1;
}

static void input_port_write(void *f,struct InputPort *in)
{
	writeint(f,in->type);
	writeword(f,in->mask);
	writeword(f,in->default_value);
	seq_write(f,&in->seq);
}


int load_input_port_settings(void)
{
	void *f;
#ifdef MAME_NET
    struct InputPort *in;
    int port, player;
#endif /* MAME_NET */


	load_default_keys();

	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_CONFIG,0)) != 0)
	{
#ifndef MAME_NET
		struct InputPort *in;
#endif
		unsigned int total,savedtotal;
		char buf[8];
		int i;
		int version;

		in = Machine->input_ports_default;

		/* calculate the size of the array */
		total = 0;
		while (in->type != IPT_END)
		{
			total++;
			in++;
		}

		/* read header */
		if (osd_fread(f,buf,8) != 8)
			goto getout;

		if (memcmp(buf,MAMECFGSTRING_V5,8) == 0)
			version = 5;
		else if (memcmp(buf,MAMECFGSTRING_V6,8) == 0)
			version = 6;
		else if (memcmp(buf,MAMECFGSTRING_V7,8) == 0)
			version = 7;
		else if (memcmp(buf,MAMECFGSTRING_V8,8) == 0)
			version = 8;
		else
			goto getout;	/* header invalid */

		/* read array size */
		if (readint(f,&savedtotal) != 0)
			goto getout;
		if (total != savedtotal)
			goto getout;	/* different size */

		/* read the original settings and compare them with the ones defined in the driver */
		in = Machine->input_ports_default;
		while (in->type != IPT_END)
		{
			struct InputPort saved;

			if (input_port_read(f,&saved,version) != 0)
				goto getout;

			if (in->mask != saved.mask ||
				in->default_value != saved.default_value ||
				in->type != saved.type ||
				seq_cmp(&in->seq,&saved.seq) !=0 )
			goto getout;	/* the default values are different */

			in++;
		}

		/* read the current settings */
		in = Machine->input_ports;
		while (in->type != IPT_END)
		{
			if (input_port_read(f,in,version) != 0)
				goto getout;
			in++;
		}

		/* Clear the coin & ticket counters/flags - LBO 042898 */
		for (i = 0; i < COIN_COUNTERS; i ++)
			coins[i] = lastcoin[i] = coinlockedout[i] = 0;
		dispensed_tickets = 0;

		/* read in the coin/ticket counters */
		for (i = 0; i < COIN_COUNTERS; i ++)
		{
			if (readint(f,&coins[i]) != 0)
				goto getout;
		}
		if (readint(f,&dispensed_tickets) != 0)
			goto getout;

		mixer_read_config(f);

getout:
		osd_fclose(f);
	}

	/* All analog ports need initialization */
	{
		int i;
		for (i = 0; i < MAX_INPUT_PORTS; i++)
			input_analog_init[i] = 1;
	}
#ifdef MAME_NET
	/* Find out what port is used by what player and swap regular inputs */
	in = Machine->input_ports;

//	if (in->type == IPT_END) return; 	/* nothing to do */

	/* make sure the InputPort definition is correct */
//	if (in->type != IPT_PORT)
//	{
//		logerror("Error in InputPort definition: expecting PORT_START\n");
//		return;
//	}
//	else in++;
	in++;

	/* scan all the input ports */
	port = 0;
	while (in->type != IPT_END && port < MAX_INPUT_PORTS)
	{
		/* now check the input bits. */
		while (in->type != IPT_END && in->type != IPT_PORT)
		{
			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING &&	/* skip dipswitch definitions */
				(in->type & ~IPF_MASK) != IPT_EXTENSION &&			/* skip analog extension fields */
				(in->type & IPF_UNUSED) == 0 &&						/* skip unused bits */
				!(!options.cheat && (in->type & IPF_CHEAT)) &&				/* skip cheats if cheats disabled */
				(in->type & ~IPF_MASK) != IPT_VBLANK &&				/* skip vblank stuff */
				((in->type & ~IPF_MASK) >= IPT_COIN1 &&				/* skip if coin input and it's locked out */
				(in->type & ~IPF_MASK) <= IPT_COIN4 &&
                 coinlockedout[(in->type & ~IPF_MASK) - IPT_COIN1]))
			{
				player = 0;
				if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER2) player = 1;
				else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER3) player = 2;
				else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER4) player = 3;

				if (((in->type & ~IPF_MASK) > IPT_ANALOG_START)
					&& ((in->type & ~IPF_MASK) < IPT_ANALOG_END))
				{
					analog_player_port[port] = player;
				}
				if (((in->type & ~IPF_MASK) == IPT_BUTTON1) ||
					((in->type & ~IPF_MASK) == IPT_BUTTON2) ||
					((in->type & ~IPF_MASK) == IPT_BUTTON3) ||
					((in->type & ~IPF_MASK) == IPT_BUTTON4) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_UP) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_DOWN) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_LEFT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_RIGHT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_UP) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_DOWN) ||
 					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_LEFT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_RIGHT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_UP) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_DOWN) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_LEFT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_RIGHT) ||
					((in->type & ~IPF_MASK) == IPT_PADDLE) ||
					((in->type & ~IPF_MASK) == IPT_DIAL) ||
					((in->type & ~IPF_MASK) == IPT_TRACKBALL_X) ||
					((in->type & ~IPF_MASK) == IPT_TRACKBALL_Y) ||
					((in->type & ~IPF_MASK) == IPT_AD_STICK_X) ||
					((in->type & ~IPF_MASK) == IPT_AD_STICK_Y))
				{
					switch (default_player)
					{
						case 0:
							/* do nothing */
							break;
						case 1:
							if (player == 0)
							{
								in->type &= ~IPF_PLAYER1;
								in->type |= IPF_PLAYER2;
							}
							else if (player == 1)
							{
								in->type &= ~IPF_PLAYER2;
								in->type |= IPF_PLAYER1;
							}
							break;
						case 2:
							if (player == 0)
							{
								in->type &= ~IPF_PLAYER1;
								in->type |= IPF_PLAYER3;
							}
							else if (player == 2)
							{
								in->type &= ~IPF_PLAYER3;
								in->type |= IPF_PLAYER1;
							}
							break;
						case 3:
							if (player == 0)
							{
								in->type &= ~IPF_PLAYER1;
								in->type |= IPF_PLAYER4;
							}
							else if (player == 3)
							{
								in->type &= ~IPF_PLAYER4;
								in->type |= IPF_PLAYER1;
							}
							break;
					}
				}
			}
			in++;
		}
		port++;
		if (in->type == IPT_PORT) in++;
	}

	/* TODO: at this point the games should initialize peers to same as server */

#endif /* MAME_NET */

	update_input_ports();

	/* if we didn't find a saved config, return 0 so the main core knows that it */
	/* is the first time the game is run and it should diplay the disclaimer. */
	if (f) return 1;
	else return 0;
}

/***************************************************************************/
/* Save */

void save_input_port_settings(void)
{
	void *f;
#ifdef MAME_NET
	struct InputPort *in;
	int port, player;

	/* Swap input port definitions back to defaults */
	in = Machine->input_ports;

	if (in->type == IPT_END) return; 	/* nothing to do */

	/* make sure the InputPort definition is correct */
	if (in->type != IPT_PORT)
	{
		logerror("Error in InputPort definition: expecting PORT_START\n");
		return;
	}
	else in++;

	/* scan all the input ports */
	port = 0;
	while (in->type != IPT_END && port < MAX_INPUT_PORTS)
	{
		/* now check the input bits. */
		while (in->type != IPT_END && in->type != IPT_PORT)
		{
			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING &&	/* skip dipswitch definitions */
				(in->type & ~IPF_MASK) != IPT_EXTENSION &&			/* skip analog extension fields */
				(in->type & IPF_UNUSED) == 0 &&						/* skip unused bits */
				!(!options.cheat && (in->type & IPF_CHEAT)) &&				/* skip cheats if cheats disabled */
				(in->type & ~IPF_MASK) != IPT_VBLANK &&				/* skip vblank stuff */
				((in->type & ~IPF_MASK) >= IPT_COIN1 &&				/* skip if coin input and it's locked out */
				(in->type & ~IPF_MASK) <= IPT_COIN4 &&
                 coinlockedout[(in->type & ~IPF_MASK) - IPT_COIN1]))
			{
				player = 0;
				if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER2) player = 1;
				else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER3) player = 2;
				else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER4) player = 3;

				if (((in->type & ~IPF_MASK) == IPT_BUTTON1) ||
					((in->type & ~IPF_MASK) == IPT_BUTTON2) ||
					((in->type & ~IPF_MASK) == IPT_BUTTON3) ||
					((in->type & ~IPF_MASK) == IPT_BUTTON4) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_UP) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_DOWN) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_LEFT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICK_RIGHT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_UP) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_DOWN) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_LEFT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKRIGHT_RIGHT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_UP) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_DOWN) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_LEFT) ||
					((in->type & ~IPF_MASK) == IPT_JOYSTICKLEFT_RIGHT) ||
					((in->type & ~IPF_MASK) == IPT_PADDLE) ||
					((in->type & ~IPF_MASK) == IPT_DIAL) ||
					((in->type & ~IPF_MASK) == IPT_TRACKBALL_X) ||
					((in->type & ~IPF_MASK) == IPT_TRACKBALL_Y) ||
					((in->type & ~IPF_MASK) == IPT_AD_STICK_X) ||
					((in->type & ~IPF_MASK) == IPT_AD_STICK_Y))
				{
					switch (default_player)
					{
						case 0:
							/* do nothing */
							analog_player_port[port] = player;
							break;
						case 1:
							if (player == 0)
							{
								in->type &= ~IPF_PLAYER1;
								in->type |= IPF_PLAYER2;
								analog_player_port[port] = 1;
							}
							else if (player == 1)
							{
								in->type &= ~IPF_PLAYER2;
								in->type |= IPF_PLAYER1;
								analog_player_port[port] = 0;
							}
							break;
						case 2:
							if (player == 0)
							{
								in->type &= ~IPF_PLAYER1;
								in->type |= IPF_PLAYER3;
								analog_player_port[port] = 2;
							}
							else if (player == 2)
							{
								in->type &= ~IPF_PLAYER3;
								in->type |= IPF_PLAYER1;
								analog_player_port[port] = 0;
							}
							break;
						case 3:
							if (player == 0)
							{
								in->type &= ~IPF_PLAYER1;
								in->type |= IPF_PLAYER4;
								analog_player_port[port] = 3;
							}
							else if (player == 3)
							{
								in->type &= ~IPF_PLAYER4;
								in->type |= IPF_PLAYER1;
								analog_player_port[port] = 0;
							}
							break;
					}
				}
			}
			in++;
		}
		port++;
		if (in->type == IPT_PORT) in++;
	}
#endif /* MAME_NET */

	save_default_keys();

	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_CONFIG,1)) != 0)
	{
#ifndef MAME_NET
		struct InputPort *in;
#endif /* MAME_NET */
		int total;
		int i;


		in = Machine->input_ports_default;

		/* calculate the size of the array */
		total = 0;
		while (in->type != IPT_END)
		{
			total++;
			in++;
		}

		/* write header */
		osd_fwrite(f,MAMECFGSTRING_V8,8);
		/* write array size */
		writeint(f,total);
		/* write the original settings as defined in the driver */
		in = Machine->input_ports_default;
		while (in->type != IPT_END)
		{
			input_port_write(f,in);
			in++;
		}
		/* write the current settings */
		in = Machine->input_ports;
		while (in->type != IPT_END)
		{
			input_port_write(f,in);
			in++;
		}

		/* write out the coin/ticket counters for this machine - LBO 042898 */
		for (i = 0; i < COIN_COUNTERS; i ++)
			writeint(f,coins[i]);
		writeint(f,dispensed_tickets);

		mixer_write_config(f);

		osd_fclose(f);
		osd_fsync();
	}
}



/* Note that the following 3 routines have slightly different meanings with analog ports */
const char *input_port_name(const struct InputPort *in)
{
	int i;
	unsigned type;

	if (in->name != IP_NAME_DEFAULT) return in->name;

	i = 0;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
		type = (in-1)->type & (~IPF_MASK | IPF_PLAYERMASK);
	else
		type = in->type & (~IPF_MASK | IPF_PLAYERMASK);

	while (inputport_defaults[i].type != IPT_END &&
			inputport_defaults[i].type != type)
		i++;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
		return inputport_defaults[i+1].name;
	else
		return inputport_defaults[i].name;
}

InputSeq* input_port_type_seq(int type)
{
	unsigned i;

	i = 0;

	while (inputport_defaults[i].type != IPT_END &&
			inputport_defaults[i].type != type)
		i++;

	return &inputport_defaults[i].seq;
}

InputSeq* input_port_seq(const struct InputPort *in)
{
	int i,type;

	static InputSeq ip_none = SEQ_DEF_1(CODE_NONE);

	while (seq_get_1((InputSeq*)&in->seq) == CODE_PREVIOUS) in--;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
	{
		type = (in-1)->type & (~IPF_MASK | IPF_PLAYERMASK);
		/* if port is disabled, or cheat with cheats disabled, return no key */
		if (((in-1)->type & IPF_UNUSED) || (!options.cheat && ((in-1)->type & IPF_CHEAT)))
			return &ip_none;
	}
	else
	{
		type = in->type & (~IPF_MASK | IPF_PLAYERMASK);
		/* if port is disabled, or cheat with cheats disabled, return no key */
		if ((in->type & IPF_UNUSED) || (!options.cheat && (in->type & IPF_CHEAT)))
			return &ip_none;
	}

	if (seq_get_1((InputSeq*)&in->seq) != CODE_DEFAULT)
		return (InputSeq*)&in->seq;

	i = 0;

	while (inputport_defaults[i].type != IPT_END &&
			inputport_defaults[i].type != type)
		i++;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
		return &inputport_defaults[i+1].seq;
	else
		return &inputport_defaults[i].seq;
}

void update_analog_port(int port)
{
	struct InputPort *in;
	int current, delta, type, sensitivity, min, max, default_value;
	int axis, is_stick, check_bounds;
	InputSeq* incseq;
	InputSeq* decseq;
	int keydelta;
	int player;

	/* get input definition */
	in = input_analog[port];

	/* if we're not cheating and this is a cheat-only port, bail */
	if (!options.cheat && (in->type & IPF_CHEAT)) return;
	type=(in->type & ~IPF_MASK);

	decseq = input_port_seq(in);
	incseq = input_port_seq(in+1);

	keydelta = IP_GET_DELTA(in);

	switch (type)
	{
		case IPT_PADDLE:
			axis = X_AXIS; is_stick = 0; check_bounds = 1; break;
		case IPT_PADDLE_V:
			axis = Y_AXIS; is_stick = 0; check_bounds = 1; break;
		case IPT_DIAL:
			axis = X_AXIS; is_stick = 0; check_bounds = 0; break;
		case IPT_DIAL_V:
			axis = Y_AXIS; is_stick = 0; check_bounds = 0; break;
		case IPT_TRACKBALL_X:
			axis = X_AXIS; is_stick = 0; check_bounds = 0; break;
		case IPT_TRACKBALL_Y:
			axis = Y_AXIS; is_stick = 0; check_bounds = 0; break;
		case IPT_AD_STICK_X:
			axis = X_AXIS; is_stick = 1; check_bounds = 1; break;
		case IPT_AD_STICK_Y:
			axis = Y_AXIS; is_stick = 1; check_bounds = 1; break;
		case IPT_PEDAL:
			axis = Y_AXIS; is_stick = 0; check_bounds = 1; break;
		default:
			/* Use some defaults to prevent crash */
			axis = X_AXIS; is_stick = 0; check_bounds = 0;
			logerror("Oops, polling non analog device in update_analog_port()????\n");
	}


	sensitivity = IP_GET_SENSITIVITY(in);
	min = IP_GET_MIN(in);
	max = IP_GET_MAX(in);
	default_value = in->default_value * 100 / sensitivity;
	/* extremes can be either signed or unsigned */
	if (min > max)
	{
		if (in->mask > 0xff) min = min - 0x10000;
		else min = min - 0x100;
	}


	input_analog_previous_value[port] = input_analog_current_value[port];

	/* if IPF_CENTER go back to the default position */
	/* sticks are handled later... */
	if ((in->type & IPF_CENTER) && (!is_stick))
		input_analog_current_value[port] = in->default_value * 100 / sensitivity;

	current = input_analog_current_value[port];

	delta = 0;

	switch (in->type & IPF_PLAYERMASK)
	{
		case IPF_PLAYER2:          player = 1; break;
		case IPF_PLAYER3:          player = 2; break;
		case IPF_PLAYER4:          player = 3; break;
		case IPF_PLAYER1: default: player = 0; break;
	}

	if (axis == X_AXIS)
		delta = mouse_delta_x[player];
	else
		delta = mouse_delta_y[player];

	if (seq_pressed(decseq)) delta -= keydelta;

	if (type != IPT_PEDAL)
	{
		if (seq_pressed(incseq)) delta += keydelta;
	}
	else
	{
		/* is this cheesy or what? */
		if (!delta && seq_get_1(incseq) == KEYCODE_Y) delta += keydelta;
		delta = -delta;
	}

	if (in->type & IPF_REVERSE) delta = -delta;

	if (is_stick)
	{
		int _new, prev;

		/* center stick */
		if ((delta == 0) && (in->type & IPF_CENTER))
		{
			if (current > default_value)
			delta = -100 / sensitivity;
			if (current < default_value)
			delta = 100 / sensitivity;
		}

		/* An analog joystick which is not at zero position (or has just */
		/* moved there) takes precedence over all other computations */
		/* analog_x/y holds values from -128 to 128 (yes, 128, not 127) */

		if (axis == X_AXIS)
		{
			_new  = analog_current_x[player];
			prev = analog_previous_x[player];
		}
		else
		{
			_new  = analog_current_y[player];
			prev = analog_previous_y[player];
		}

		if ((_new != 0) || (_new-prev != 0))
		{
			delta=0;

			if (in->type & IPF_REVERSE)
			{
				_new  = -_new;
				prev = -prev;
			}

			/* apply sensitivity using a logarithmic scale */
			if (in->mask > 0xff)
			{
				if (_new > 0)
				{
					current = (pow(_new / 32768.0, 100.0 / sensitivity) * (max-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
				else
				{
					current = (pow(-_new / 32768.0, 100.0 / sensitivity) * (min-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
			}
			else
			{
				if (_new > 0)
				{
					current = (pow(_new / 128.0, 100.0 / sensitivity) * (max-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
				else
				{
					current = (pow(-_new / 128.0, 100.0 / sensitivity) * (min-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
			}
		}
	}

	current += delta;

	if (check_bounds)
	{
		if ((current * sensitivity + 50) / 100 < min)
			current = (min * 100 + sensitivity/2) / sensitivity;
		if ((current * sensitivity + 50) / 100 > max)
			current = (max * 100 + sensitivity/2) / sensitivity;
	}

	input_analog_current_value[port] = current;
}

static void scale_analog_port(int port)
{
	struct InputPort *in;
	int delta,current,sensitivity;

profiler_mark(PROFILER_INPUT);
	in = input_analog[port];
	sensitivity = IP_GET_SENSITIVITY(in);

	delta = cpu_scalebyfcount(input_analog_current_value[port] - input_analog_previous_value[port]);

	current = input_analog_previous_value[port] + delta;

	input_port_value[port] &= ~in->mask;
	input_port_value[port] |= ((current * sensitivity + 50) / 100) & in->mask;

	if (playback)
		readword(playback,&input_port_value[port]);
	if (record)
		writeword(record,input_port_value[port]);
#ifdef MAME_NET
	if ( net_active() && (default_player != NET_SPECTATOR) )
		net_analog_sync((unsigned char *) input_port_value, port, analog_player_port, default_player);
#endif /* MAME_NET */
profiler_mark(PROFILER_END);
}


void update_input_ports(void)
{
	int port,ib;
	struct InputPort *in;
#define MAX_INPUT_BITS 1024
static int impulsecount[MAX_INPUT_BITS];
static int waspressed[MAX_INPUT_BITS];
#define MAX_JOYSTICKS 3
#define MAX_PLAYERS 4
#ifdef MRU_JOYSTICK
static int update_serial_number = 1;
static int joyserial[MAX_JOYSTICKS*MAX_PLAYERS][4];
#else
int joystick[MAX_JOYSTICKS*MAX_PLAYERS][4];
#endif

#ifdef MAME_NET
int player;
#endif /* MAME_NET */


profiler_mark(PROFILER_INPUT);

	/* clear all the values before proceeding */
	for (port = 0;port < MAX_INPUT_PORTS;port++)
	{
		input_port_value[port] = 0;
		input_vblank[port] = 0;
		input_analog[port] = 0;
	}

#ifndef MRU_JOYSTICK
	for (i = 0;i < 4*MAX_JOYSTICKS*MAX_PLAYERS;i++)
		joystick[i/4][i%4] = 0;
#endif

	in = Machine->input_ports;

	if (in->type == IPT_END) return; 	/* nothing to do */

	/* make sure the InputPort definition is correct */
	if (in->type != IPT_PORT)
	{
		logerror("Error in InputPort definition: expecting PORT_START\n");
		return;
	}
	else in++;

#ifdef MRU_JOYSTICK
	/* scan all the joystick ports */
	port = 0;
	while (in->type != IPT_END && port < MAX_INPUT_PORTS)
	{
		while (in->type != IPT_END && in->type != IPT_PORT)
		{
			if ((in->type & ~IPF_MASK) >= IPT_JOYSTICK_UP &&
				(in->type & ~IPF_MASK) <= IPT_JOYSTICKLEFT_RIGHT)
			{
				InputSeq* seq;

				seq = input_port_seq(in);

				if (seq_get_1(seq) != 0 && seq_get_1(seq) != CODE_NONE)
				{
					int joynum,joydir,player;

					player = 0;
					if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER2)
						player = 1;
					else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER3)
						player = 2;
					else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER4)
						player = 3;

					joynum = player * MAX_JOYSTICKS +
							 ((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) / 4;
					joydir = ((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) % 4;

#ifdef ENABLE_AUTOFIRE
					if (seq_pressed_with_autofire(in, seq))
#else
					if (seq_pressed(seq))
#endif
					{
						if (joyserial[joynum][joydir] == 0)
							joyserial[joynum][joydir] = update_serial_number;
					}
					else
						joyserial[joynum][joydir] = 0;
				}
			}
			in++;
		}

		port++;
		if (in->type == IPT_PORT) in++;
	}
	update_serial_number += 1;

	in = Machine->input_ports;

	/* already made sure the InputPort definition is correct */
	in++;
#endif


	/* scan all the input ports */
	port = 0;
	ib = 0;
	while (in->type != IPT_END && port < MAX_INPUT_PORTS)
	{
		struct InputPort *start;


		/* first of all, scan the whole input port definition and build the */
		/* default value. I must do it before checking for input because otherwise */
		/* multiple keys associated with the same input bit wouldn't work (the bit */
		/* would be reset to its default value by the second entry, regardless if */
		/* the key associated with the first entry was pressed) */
		start = in;
		while (in->type != IPT_END && in->type != IPT_PORT)
		{
			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING &&	/* skip dipswitch definitions */
				(in->type & ~IPF_MASK) != IPT_EXTENSION)			/* skip analog extension fields */
			{
				input_port_value[port] =
						(input_port_value[port] & ~in->mask) | (in->default_value & in->mask);
#ifdef MAME_NET
				if ( net_active() )
					input_port_defaults[port] = input_port_value[port];
#endif /* MAME_NET */
			}

			in++;
		}

		/* now get back to the beginning of the input port and check the input bits. */
		for (in = start;
			 in->type != IPT_END && in->type != IPT_PORT;
			 in++, ib++)
		{
#ifdef MAME_NET
			player = 0;
			if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER2) player = 1;
			else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER3) player = 2;
			else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER4) player = 3;
#endif /* MAME_NET */
			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING &&	/* skip dipswitch definitions */
					(in->type & ~IPF_MASK) != IPT_EXTENSION)		/* skip analog extension fields */
			{
				if ((in->type & ~IPF_MASK) == IPT_VBLANK)
				{
					input_vblank[port] ^= in->mask;
					input_port_value[port] ^= in->mask;
if (Machine->drv->vblank_duration == 0)
	logerror("Warning: you are using IPT_VBLANK with vblank_duration = 0. You need to increase vblank_duration for IPT_VBLANK to work.\n");
				}
				/* If it's an analog control, handle it appropriately */
				else if (((in->type & ~IPF_MASK) > IPT_ANALOG_START)
					  && ((in->type & ~IPF_MASK) < IPT_ANALOG_END  )) /* LBO 120897 */
				{
					input_analog[port]=in;
					/* reset the analog port on first access */
					if (input_analog_init[port])
					{
						input_analog_init[port] = 0;
						input_analog_current_value[port] = input_analog_previous_value[port]
							= in->default_value * 100 / IP_GET_SENSITIVITY(in);
					}
				}
				else
				{
					InputSeq* seq;

					seq = input_port_seq(in);

#ifdef ENABLE_AUTOFIRE
					if (seq_pressed_with_autofire(in, seq))
#else
					if (seq_pressed(seq))
#endif
					{
						/* skip if coin input and it's locked out */
						if ((in->type & ~IPF_MASK) >= IPT_COIN1 &&
							(in->type & ~IPF_MASK) <= IPT_COIN4 &&
                            coinlockedout[(in->type & ~IPF_MASK) - IPT_COIN1])
						{
							continue;
						}

						/* if IPF_RESET set, reset the first CPU */
						if ((in->type & IPF_RESETCPU) && waspressed[ib] == 0)
							cpu_set_reset_line(0,PULSE_LINE);

						if (in->type & IPF_IMPULSE)
						{
if (IP_GET_IMPULSE(in) == 0)
	logerror("error in input port definition: IPF_IMPULSE with length = 0\n");
							if (waspressed[ib] == 0)
								impulsecount[ib] = IP_GET_IMPULSE(in);
								/* the input bit will be toggled later */
						}
						else if (in->type & IPF_TOGGLE)
						{
							if (waspressed[ib] == 0)
							{
								in->default_value ^= in->mask;
								input_port_value[port] ^= in->mask;
							}
						}
						else if ((in->type & ~IPF_MASK) >= IPT_JOYSTICK_UP &&
								(in->type & ~IPF_MASK) <= IPT_JOYSTICKLEFT_RIGHT)
						{
#ifndef MAME_NET
							int joynum,joydir,mask,player;


							player = 0;
							if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER2) player = 1;
							else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER3) player = 2;
							else if ((in->type & IPF_PLAYERMASK) == IPF_PLAYER4) player = 3;
#else
							int joynum,joydir,mask;
#endif /* !MAME_NET */
							joynum = player * MAX_JOYSTICKS +
									((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) / 4;
							joydir = ((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) % 4;

							mask = in->mask;

#ifndef MRU_JOYSTICK
							/* avoid movement in two opposite directions */
							if (joystick[joynum][joydir ^ 1] != 0)
								mask = 0;
							else if (in->type & IPF_4WAY)
							{
								int dir;


								/* avoid diagonal movements */
								for (dir = 0;dir < 4;dir++)
								{
									if (joystick[joynum][dir] != 0)
										mask = 0;
								}
							}

							joystick[joynum][joydir] = 1;
#else
							/* avoid movement in two opposite directions */
							if (joyserial[joynum][joydir ^ 1] != 0)
								mask = 0;
							else if (in->type & IPF_4WAY)
							{
								int mru_dir = joydir;
								int mru_serial = 0;
								int dir;


								/* avoid diagonal movements, use mru button */
								for (dir = 0;dir < 4;dir++)
								{
									if (joyserial[joynum][dir] > mru_serial)
									{
										mru_serial = joyserial[joynum][dir];
										mru_dir = dir;
									}
								}

								if (mru_dir != joydir)
									mask = 0;
							}
#endif

							input_port_value[port] ^= mask;
						}
						else
							input_port_value[port] ^= in->mask;

						waspressed[ib] = 1;
					}
					else
						waspressed[ib] = 0;

					if ((in->type & IPF_IMPULSE) && impulsecount[ib] > 0)
					{
						impulsecount[ib]--;
						waspressed[ib] = 1;
						input_port_value[port] ^= in->mask;
					}
				}
			}
		}

		port++;
		if (in->type == IPT_PORT) in++;
	}

	if (playback)
	{
		int i;

		for (i = 0; i < MAX_INPUT_PORTS; i ++)
			readword(playback,&input_port_value[i]);
	}
	if (record)
	{
		int i;

		for (i = 0; i < MAX_INPUT_PORTS; i ++)
			writeword(record,input_port_value[i]);
	}
#ifdef MAME_NET
	if ( net_active() && (default_player != NET_SPECTATOR) )
		net_input_sync((unsigned char *) input_port_value, (unsigned char *) input_port_defaults, MAX_INPUT_PORTS);
#endif /* MAME_NET */

profiler_mark(PROFILER_END);
}



/* used the the CPU interface to notify that VBlank has ended, so we can update */
/* IPT_VBLANK input ports. */
void inputport_vblank_end(void)
{
	int port;
	int i;


profiler_mark(PROFILER_INPUT);
	for (port = 0;port < MAX_INPUT_PORTS;port++)
	{
		if (input_vblank[port])
		{
			input_port_value[port] ^= input_vblank[port];
			input_vblank[port] = 0;
		}
	}

	/* poll all the analog joysticks */
	osd_poll_joysticks();

	/* update the analog devices */
	for (i = 0;i < OSD_MAX_JOY_ANALOG;i++)
	{
		/* update the analog joystick position */
		analog_previous_x[i] = analog_current_x[i];
		analog_previous_y[i] = analog_current_y[i];
		osd_analogjoy_read (i, &(analog_current_x[i]), &(analog_current_y[i]));

		/* update mouse/trackball position */
		osd_trak_read (i, &mouse_delta_x[i], &mouse_delta_y[i]);
	}

	for (i = 0;i < MAX_INPUT_PORTS;i++)
	{
		struct InputPort *in;

		in=input_analog[i];
		if (in)
		{
			update_analog_port(i);
		}
	}
profiler_mark(PROFILER_END);
}



int readinputport(int port)
{
	struct InputPort *in;

	/* Update analog ports on demand */
	in=input_analog[port];
	if (in)
	{
		scale_analog_port(port);
	}

	return input_port_value[port];
}

READ_HANDLER( input_port_0_r ) { return readinputport(0); }
READ_HANDLER( input_port_1_r ) { return readinputport(1); }
READ_HANDLER( input_port_2_r ) { return readinputport(2); }
READ_HANDLER( input_port_3_r ) { return readinputport(3); }
READ_HANDLER( input_port_4_r ) { return readinputport(4); }
READ_HANDLER( input_port_5_r ) { return readinputport(5); }
READ_HANDLER( input_port_6_r ) { return readinputport(6); }
READ_HANDLER( input_port_7_r ) { return readinputport(7); }
READ_HANDLER( input_port_8_r ) { return readinputport(8); }
READ_HANDLER( input_port_9_r ) { return readinputport(9); }
READ_HANDLER( input_port_10_r ) { return readinputport(10); }
READ_HANDLER( input_port_11_r ) { return readinputport(11); }
READ_HANDLER( input_port_12_r ) { return readinputport(12); }
READ_HANDLER( input_port_13_r ) { return readinputport(13); }
READ_HANDLER( input_port_14_r ) { return readinputport(14); }
READ_HANDLER( input_port_15_r ) { return readinputport(15); }
READ_HANDLER( input_port_16_r ) { return readinputport(16); }
READ_HANDLER( input_port_17_r ) { return readinputport(17); }
READ_HANDLER( input_port_18_r ) { return readinputport(18); }
READ_HANDLER( input_port_19_r ) { return readinputport(19); }


#ifdef MAME_NET
void set_default_player_controls(int player)
{
	if (player == NET_SPECTATOR)
		default_player = NET_SPECTATOR;
	else
		default_player = player - 1;
}
#endif /* MAME_NET */

/***************************************************************************/
/* InputPort conversion */

static unsigned input_port_count(const struct InputPortTiny *src)
{
	unsigned total;

	total = 0;
	while (src->type != IPT_END)
	{
		int type = src->type & ~IPF_MASK;
		if (type > IPT_ANALOG_START && type < IPT_ANALOG_END)
			total += 2;
		else if (type != IPT_EXTENSION)
			++total;
		++src;
	}

	++total; /* for IPT_END */

	return total;
}

struct InputPort* input_port_allocate(const struct InputPortTiny *src)
{
	struct InputPort* dst;
	struct InputPort* base;
	unsigned total;

	total = input_port_count(src);

	base = (struct InputPort*)malloc(total * sizeof(struct InputPort));
	dst = base;

	while (src->type != IPT_END)
	{
		int type = src->type & ~IPF_MASK;
		const struct InputPortTiny *ext;
		const struct InputPortTiny *src_end;
		InputCode seq_default;

		if (type > IPT_ANALOG_START && type < IPT_ANALOG_END)
			src_end = src + 2;
		else
			src_end = src + 1;

		switch (type)
		{
			case IPT_END :
			case IPT_PORT :
			case IPT_DIPSWITCH_NAME :
			case IPT_DIPSWITCH_SETTING :
				seq_default = CODE_NONE;
			break;
			default:
				seq_default = CODE_DEFAULT;
		}

		ext = src_end;
		while (src != src_end)
		{
			dst->type = src->type;
			dst->mask = src->mask;
			dst->default_value = src->default_value;
			dst->name = src->name;

  			if (ext->type == IPT_EXTENSION)
  			{
				InputCode or1 =	IP_GET_CODE_OR1(ext);
				InputCode or2 =	IP_GET_CODE_OR2(ext);

				if (or1 < __code_max)
				{
					if (or2 < __code_max)
						seq_set_3(&dst->seq, or1, CODE_OR, or2);
					else
						seq_set_1(&dst->seq, or1);
				} else {
					if (or1 == CODE_NONE)
						seq_set_1(&dst->seq, or2);
					else
						seq_set_1(&dst->seq, or1);
				}

  				++ext;
  			} else {
				seq_set_1(&dst->seq,seq_default);
  			}

			++src;
			++dst;
		}

		src = ext;
	}

	dst->type = IPT_END;

	return base;
}

void input_port_free(struct InputPort* dst)
{
	free(dst);
}

/*
 * added for autofire
 */
#ifdef ENABLE_AUTOFIRE

extern void ui_displaymenu(struct osd_bitmap *bitmap, const char **items, const char **subitems, char *flag, int selected, int arrowize_subitem);
extern int need_to_clear_bitmap;	/* used to tell updatescreen() to clear the bitmap */

#define MAX_AUTOFIRE_BUTTON 6

static int af_ipt_button[] = {
    IPT_BUTTON1,IPT_BUTTON2,IPT_BUTTON3,IPT_BUTTON4,
    IPT_BUTTON5,IPT_BUTTON6,IPT_BUTTON7,IPT_BUTTON8
};

InputSeq af_fire_on = { KEYCODE_INSERT, CODE_NONE };
InputSeq af_fire_off= { KEYCODE_DEL, CODE_NONE };

static char af_autofire[MAX_AUTOFIRE_BUTTON];
static char af_noautofire[MAX_AUTOFIRE_BUTTON];
static char af_autopressed[MAX_AUTOFIRE_BUTTON]; 
//static int af_record_first_insert = 1;

int autofire_menu(struct osd_bitmap *bitmap, int selected)
{
    const char *menu_item[100];
    const char *menu_subitem[100];
    char menu_item_sub[100][256];
    char menu_subitem_sub[100][256];
    char flag[100];
    int sel,sel_af_on,sel_af_off,sel_return;
    int total;
    int arrowize;

    sel = selected - 1;
    arrowize = 0;

    /* auto-fire button menu */
    for (total = 0; total < MAX_AUTOFIRE_BUTTON; total ++) {
        flag[total] = 0;
        sprintf(menu_item_sub[total] , "Auto-Fire on Button %d", total + 1);
        sprintf(menu_subitem_sub[total], "Delay %02d", af_autofire[total]);
        menu_item[total] = menu_item_sub[total];
        if (af_autofire[total] > 0)
            menu_subitem[total] = menu_subitem_sub[total];
        else
            menu_subitem[total] = "      No";

	if (sel == total) {
            if (af_autofire[sel] == 0)
                arrowize = 2;
            else if (af_autofire[sel] == 99)
                arrowize = 1;
            else
                arrowize = 3;
	}
    }

    /* auto-fire on menu */
    sel_af_on = total;
    flag[total] = 0;
    menu_subitem[total] = code_name(af_fire_on[0]);
    menu_item[total ++] = "Auto-Fire On  ";

    /* auto-fire off menu */
    sel_af_off = total;
    flag[total] = 0;
    menu_subitem[total] = code_name(af_fire_off[0]);
    menu_item[total ++] = "Auto-Fire Off ";

    /* Return to Main Menu */
    sel_return = total;
    flag[total] =0;
    menu_subitem[total] = "";
    menu_item[total ++] = "Return to Main Menu";

    menu_item[total] = 0;	/* End of array */

    if (sel > SEL_MASK) {
        int ret;
        InputSeq seq;

        menu_subitem[sel & SEL_MASK] = "    ";
        ui_displaymenu(bitmap, menu_item, menu_subitem,
                       flag, sel & SEL_MASK, 3);

        seq_set_1(&seq, CODE_NONE);
        ret = seq_read_async(&seq, 0);

        if (ret >= 0) {
            sel &= 0xff;
            schedule_full_refresh();
            if (seq_get_1(&seq) != CODE_NONE) {
                if (sel == sel_af_on)
                    af_fire_on[0] = seq_get_1(&seq);
                else if (sel==sel_af_off)
                    af_fire_off[0] = seq_get_1(&seq);
            }
        }
        return sel + 1;
    }

    ui_displaymenu(bitmap, menu_item, menu_subitem, flag, sel, arrowize);

    if (input_ui_pressed_repeat(IPT_UI_DOWN, 8)) {
        if (sel < sel_return)
            sel++;
        else
            sel = 0;
    }

    if (input_ui_pressed_repeat(IPT_UI_UP, 8)) {
        if (sel > 0)
            sel--;
        else
            sel = sel_return;
    }

    if (input_ui_pressed_repeat(IPT_UI_RIGHT, 8)) {
        if ((sel >= 0) && (sel < MAX_AUTOFIRE_BUTTON))
            if (af_autofire[sel] ++ > 98)
                af_autofire[sel] = 99;
        schedule_full_refresh();
    }

    if (input_ui_pressed_repeat(IPT_UI_LEFT, 8)) {
        if ((sel >= 0) && (sel < MAX_AUTOFIRE_BUTTON))
            if (af_autofire[sel] -- < 1)
                af_autofire[sel] = 0;
        schedule_full_refresh();
    }

    if (input_ui_pressed(IPT_UI_SELECT)) {
        if (sel == sel_return)
            sel = -1;
        else if ((sel == sel_af_on) || (sel == sel_af_off)) {
            seq_read_async_start();
            sel |= 1 << SEL_BITS;	/* we'll ask for a key */
/*			sel |= 0x100;	   we'll ask for a key */
            schedule_full_refresh();
        }
    }

    if (input_ui_pressed(IPT_UI_CANCEL))
        sel = -1;

    if (input_ui_pressed(IPT_UI_CONFIGURE))
        sel = -2;

    if (sel == -1 || sel == -2)
        schedule_full_refresh();

    return sel + 1;
}

void AfButton(int impport)
{
    int no;
#if 0
    int i;
    char msg[100];
    char tmp[10];
#endif
	
    switch (impport) {
    case IPT_BUTTON1:
        no = 0;
        break;
    case IPT_BUTTON2:
        no = 1;
        break;
    case IPT_BUTTON3:
        no = 2;
        break;
    case IPT_BUTTON4:
        no = 3;
        break;
    case IPT_BUTTON5:
        no = 4;
        break;
    case IPT_BUTTON6:
        no = 5;
        break;
    case IPT_BUTTON7:
        no = 6;
        break;
    case IPT_BUTTON8:
        no = 7;
        break;
    default:
        return;
    }
    if (no >= MAX_AUTOFIRE_BUTTON)
        return;
	
    if (seq_pressed(&af_fire_on)) 
        af_noautofire[no] = 0;
    else if (seq_pressed(&af_fire_off))
        af_noautofire[no] = 1;
    else
        return;

#if 0
    strcpy(msg, "Autofire ");
    for (i = 0; i < MAX_AUTOFIRE_BUTTON; i ++) {
        if (!af_noautofire[i]) {
            if (af_autofire[i])
                sprintf(tmp, "%d=%02d  ", i + 1, af_autofire[i]);
            else
                sprintf(tmp, "%d=NO  ", i + 1);
        } else
            sprintf(tmp, "%d=OFF ", i + 1);
        strcat(msg, tmp);
    }
    usrintf_showmessage(msg);
#endif
}

int seq_pressed_with_autofire(struct InputPort *in,InputSeq* code)
{
    int i, impport, ret;

    impport = in->type & ~IPF_MASK;
	
    ret = seq_pressed(code);
    if (ret != 0) {
        for (i = 0; i < MAX_AUTOFIRE_BUTTON; i ++)
            if (impport == af_ipt_button[i]) {
                if (!af_noautofire[i] && (af_autofire[i] > 0)
                    && (af_autopressed[i] >= af_autofire[i])) {
                    ret = 0;
                    af_autopressed[i] = 0;
                }
                else
                    af_autopressed[i] ++;
            }

        AfButton(impport);
    }

    return ret;
}

#endif
