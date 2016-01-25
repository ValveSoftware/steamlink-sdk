#ifndef INPUT_H
#define INPUT_H

typedef unsigned InputCode;

struct KeyboardInfo
{
	char *name; /* OS dependant name; 0 terminates the list */
	unsigned code; /* OS dependant code */
	InputCode standardcode;	/* CODE_xxx equivalent from list below, or CODE_OTHER if n/a */
};

struct JoystickInfo
{
	char *name; /* OS dependant name; 0 terminates the list */
	unsigned code; /* OS dependant code */
	InputCode standardcode;	/* CODE_xxx equivalent from list below, or CODE_OTHER if n/a */
};

enum
{
	/* key */
	KEYCODE_A, KEYCODE_B, KEYCODE_C, KEYCODE_D, KEYCODE_E, KEYCODE_F,
	KEYCODE_G, KEYCODE_H, KEYCODE_I, KEYCODE_J, KEYCODE_K, KEYCODE_L,
	KEYCODE_M, KEYCODE_N, KEYCODE_O, KEYCODE_P, KEYCODE_Q, KEYCODE_R,
	KEYCODE_S, KEYCODE_T, KEYCODE_U, KEYCODE_V, KEYCODE_W, KEYCODE_X,
	KEYCODE_Y, KEYCODE_Z, KEYCODE_0, KEYCODE_1, KEYCODE_2, KEYCODE_3,
	KEYCODE_4, KEYCODE_5, KEYCODE_6, KEYCODE_7, KEYCODE_8, KEYCODE_9,
	KEYCODE_0_PAD, KEYCODE_1_PAD, KEYCODE_2_PAD, KEYCODE_3_PAD, KEYCODE_4_PAD,
	KEYCODE_5_PAD, KEYCODE_6_PAD, KEYCODE_7_PAD, KEYCODE_8_PAD, KEYCODE_9_PAD,
	KEYCODE_F1, KEYCODE_F2, KEYCODE_F3, KEYCODE_F4, KEYCODE_F5,
	KEYCODE_F6, KEYCODE_F7, KEYCODE_F8, KEYCODE_F9, KEYCODE_F10,
	KEYCODE_F11, KEYCODE_F12,
	KEYCODE_ESC, KEYCODE_TILDE, KEYCODE_MINUS, KEYCODE_EQUALS, KEYCODE_BACKSPACE,
	KEYCODE_TAB, KEYCODE_OPENBRACE, KEYCODE_CLOSEBRACE, KEYCODE_ENTER, KEYCODE_COLON,
	KEYCODE_QUOTE, KEYCODE_BACKSLASH, KEYCODE_BACKSLASH2, KEYCODE_COMMA, KEYCODE_STOP,
	KEYCODE_SLASH, KEYCODE_SPACE, KEYCODE_INSERT, KEYCODE_DEL,
	KEYCODE_HOME, KEYCODE_END, KEYCODE_PGUP, KEYCODE_PGDN, KEYCODE_LEFT,
	KEYCODE_RIGHT, KEYCODE_UP, KEYCODE_DOWN,
	KEYCODE_SLASH_PAD, KEYCODE_ASTERISK, KEYCODE_MINUS_PAD, KEYCODE_PLUS_PAD,
	KEYCODE_DEL_PAD, KEYCODE_ENTER_PAD, KEYCODE_PRTSCR, KEYCODE_PAUSE,
	KEYCODE_LSHIFT, KEYCODE_RSHIFT, KEYCODE_LCONTROL, KEYCODE_RCONTROL,
	KEYCODE_LALT, KEYCODE_RALT, KEYCODE_SCRLOCK, KEYCODE_NUMLOCK, KEYCODE_CAPSLOCK,
	KEYCODE_LWIN, KEYCODE_RWIN, KEYCODE_MENU,
#define __code_key_first KEYCODE_A
#define __code_key_last KEYCODE_MENU

	/* joy */
	JOYCODE_1_LEFT,JOYCODE_1_RIGHT,JOYCODE_1_UP,JOYCODE_1_DOWN,
	JOYCODE_1_BUTTON1,JOYCODE_1_BUTTON2,JOYCODE_1_BUTTON3,
	JOYCODE_1_BUTTON4,JOYCODE_1_BUTTON5,JOYCODE_1_BUTTON6,
	JOYCODE_1_BUTTON7,JOYCODE_1_BUTTON8,JOYCODE_1_BUTTON9,JOYCODE_1_BUTTON10,
	JOYCODE_1_BUTTON11,JOYCODE_1_BUTTON12,JOYCODE_1_BUTTON13,JOYCODE_1_BUTTON14,
	JOYCODE_1_BUTTON15,JOYCODE_1_BUTTON16,
	JOYCODE_2_LEFT,JOYCODE_2_RIGHT,JOYCODE_2_UP,JOYCODE_2_DOWN,
	JOYCODE_2_BUTTON1,JOYCODE_2_BUTTON2,JOYCODE_2_BUTTON3,
	JOYCODE_2_BUTTON4,JOYCODE_2_BUTTON5,JOYCODE_2_BUTTON6,
	JOYCODE_2_BUTTON7,JOYCODE_2_BUTTON8,JOYCODE_2_BUTTON9,JOYCODE_2_BUTTON10,
	JOYCODE_2_BUTTON11,JOYCODE_2_BUTTON12,JOYCODE_2_BUTTON13,JOYCODE_2_BUTTON14,
	JOYCODE_2_BUTTON15,JOYCODE_2_BUTTON16,
	JOYCODE_3_LEFT,JOYCODE_3_RIGHT,JOYCODE_3_UP,JOYCODE_3_DOWN,
	JOYCODE_3_BUTTON1,JOYCODE_3_BUTTON2,JOYCODE_3_BUTTON3,
	JOYCODE_3_BUTTON4,JOYCODE_3_BUTTON5,JOYCODE_3_BUTTON6,
	JOYCODE_3_BUTTON7,JOYCODE_3_BUTTON8,JOYCODE_3_BUTTON9,JOYCODE_3_BUTTON10,
	JOYCODE_3_BUTTON11,JOYCODE_3_BUTTON12,JOYCODE_3_BUTTON13,JOYCODE_3_BUTTON14,
	JOYCODE_3_BUTTON15,JOYCODE_3_BUTTON16,
	JOYCODE_4_LEFT,JOYCODE_4_RIGHT,JOYCODE_4_UP,JOYCODE_4_DOWN,
	JOYCODE_4_BUTTON1,JOYCODE_4_BUTTON2,JOYCODE_4_BUTTON3,
	JOYCODE_4_BUTTON4,JOYCODE_4_BUTTON5,JOYCODE_4_BUTTON6,
	JOYCODE_4_BUTTON7,JOYCODE_4_BUTTON8,JOYCODE_4_BUTTON9,JOYCODE_4_BUTTON10,
	JOYCODE_4_BUTTON11,JOYCODE_4_BUTTON12,JOYCODE_4_BUTTON13,JOYCODE_4_BUTTON14,
	JOYCODE_4_BUTTON15,JOYCODE_4_BUTTON16,
	JOYCODE_1_MOUSE1, JOYCODE_1_MOUSE2, JOYCODE_1_MOUSE3,
#define __code_joy_first JOYCODE_1_LEFT
#define __code_joy_last JOYCODE_1_MOUSE3

	__code_max, /* Temination of standard code */

	/* special */
	CODE_NONE = 0x8000, /* no code, also marker of sequence end */
	CODE_OTHER, /* OS code not mapped to any other code */
	CODE_DEFAULT, /* special for input port definitions */
        CODE_PREVIOUS, /* special for input port definitions */
	CODE_NOT, /* operators for sequences */
	CODE_OR /* operators for sequences */
};

/* Wrapper for compatibility */
#define KEYCODE_OTHER CODE_OTHER
#define JOYCODE_OTHER CODE_OTHER
#define KEYCODE_NONE CODE_NONE
#define JOYCODE_NONE CODE_NONE

/***************************************************************************/
/* Single code functions */

int code_init(void);
void code_close(void);

InputCode keyoscode_to_code(unsigned oscode);
InputCode joyoscode_to_code(unsigned oscode);
InputCode savecode_to_code(unsigned savecode);
unsigned code_to_savecode(InputCode code);

const char *code_name(InputCode code);
int code_pressed(InputCode code);
int code_pressed_memory(InputCode code);
int code_pressed_memory_repeat(InputCode code, int speed);
InputCode code_read_async(void);
InputCode code_read_sync(void);
INT8 code_read_hex_async(void);

/* Wrappers for compatibility */
#define keyboard_name                   code_name
#define keyboard_pressed                code_pressed
#define keyboard_pressed_memory         code_pressed_memory
#define keyboard_pressed_memory_repeat  code_pressed_memory_repeat
#define keyboard_read_async	        code_read_async
#define keyboard_read_sync              code_read_sync

/***************************************************************************/
/* Sequence code funtions */

/* NOTE: If you modify this value you need also to modify the SEQ_DEF declarations */
#define SEQ_MAX 16

typedef InputCode InputSeq[SEQ_MAX];

INLINE InputCode seq_get_1(InputSeq* a) {
	return (*a)[0];
}

void seq_set_0(InputSeq* seq);
void seq_set_1(InputSeq* seq, InputCode code);
void seq_set_2(InputSeq* seq, InputCode code1, InputCode code2);
void seq_set_3(InputSeq* seq, InputCode code1, InputCode code2, InputCode code3);
void seq_copy(InputSeq* seqdst, InputSeq* seqsrc);
int seq_cmp(InputSeq* seq1, InputSeq* seq2);
void seq_name(InputSeq* seq, char* buffer, unsigned max);
int seq_pressed(InputSeq* seq);
void seq_read_async_start(void);
int seq_read_async(InputSeq* code, int first);

/* NOTE: It's very important that this sequence is EXACLY long SEQ_MAX */
#define SEQ_DEF_6(a,b,c,d,e,f) { a, b, c, d, e, f, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE }
#define SEQ_DEF_5(a,b,c,d,e) SEQ_DEF_6(a,b,c,d,e,CODE_NONE)
#define SEQ_DEF_4(a,b,c,d) SEQ_DEF_5(a,b,c,d,CODE_NONE)
#define SEQ_DEF_3(a,b,c) SEQ_DEF_4(a,b,c,CODE_NONE)
#define SEQ_DEF_2(a,b) SEQ_DEF_3(a,b,CODE_NONE)
#define SEQ_DEF_1(a) SEQ_DEF_2(a,CODE_NONE)
#define SEQ_DEF_0 SEQ_DEF_1(CODE_NONE)

/***************************************************************************/
/* input_ui */

int input_ui_pressed(int code);
int input_ui_pressed_repeat(int code, int speed);

#endif
