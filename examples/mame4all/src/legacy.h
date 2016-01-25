#ifndef LEGACY_H
#define LEGACY_H

/* Functions and Tables used for converting old file to the new format */
/* Define NOLEGACY for excluding the inclusion */

enum
{
	ver_567_KEYCODE_START = 0xff00,
	ver_567_KEYCODE_A, ver_567_KEYCODE_B, ver_567_KEYCODE_C, ver_567_KEYCODE_D, ver_567_KEYCODE_E, ver_567_KEYCODE_F,
	ver_567_KEYCODE_G, ver_567_KEYCODE_H, ver_567_KEYCODE_I, ver_567_KEYCODE_J, ver_567_KEYCODE_K, ver_567_KEYCODE_L,
	ver_567_KEYCODE_M, ver_567_KEYCODE_N, ver_567_KEYCODE_O, ver_567_KEYCODE_P, ver_567_KEYCODE_Q, ver_567_KEYCODE_R,
	ver_567_KEYCODE_S, ver_567_KEYCODE_T, ver_567_KEYCODE_U, ver_567_KEYCODE_V, ver_567_KEYCODE_W, ver_567_KEYCODE_X,
	ver_567_KEYCODE_Y, ver_567_KEYCODE_Z, ver_567_KEYCODE_0, ver_567_KEYCODE_1, ver_567_KEYCODE_2, ver_567_KEYCODE_3,
	ver_567_KEYCODE_4, ver_567_KEYCODE_5, ver_567_KEYCODE_6, ver_567_KEYCODE_7, ver_567_KEYCODE_8, ver_567_KEYCODE_9,
	ver_567_KEYCODE_0_PAD, ver_567_KEYCODE_1_PAD, ver_567_KEYCODE_2_PAD, ver_567_KEYCODE_3_PAD, ver_567_KEYCODE_4_PAD,
	ver_567_KEYCODE_5_PAD, ver_567_KEYCODE_6_PAD, ver_567_KEYCODE_7_PAD, ver_567_KEYCODE_8_PAD, ver_567_KEYCODE_9_PAD,
	ver_567_KEYCODE_F1, ver_567_KEYCODE_F2, ver_567_KEYCODE_F3, ver_567_KEYCODE_F4, ver_567_KEYCODE_F5,
	ver_567_KEYCODE_F6, ver_567_KEYCODE_F7, ver_567_KEYCODE_F8, ver_567_KEYCODE_F9, ver_567_KEYCODE_F10,
	ver_567_KEYCODE_F11, ver_567_KEYCODE_F12,
	ver_567_KEYCODE_ESC, ver_567_KEYCODE_TILDE, ver_567_KEYCODE_MINUS, ver_567_KEYCODE_EQUALS, ver_567_KEYCODE_BACKSPACE,
	ver_567_KEYCODE_TAB, ver_567_KEYCODE_OPENBRACE, ver_567_KEYCODE_CLOSEBRACE, ver_567_KEYCODE_ENTER, ver_567_KEYCODE_COLON,
	ver_567_KEYCODE_QUOTE, ver_567_KEYCODE_BACKSLASH, ver_567_KEYCODE_BACKSLASH2, ver_567_KEYCODE_COMMA, ver_567_KEYCODE_STOP,
	ver_567_KEYCODE_SLASH, ver_567_KEYCODE_SPACE, ver_567_KEYCODE_INSERT, ver_567_KEYCODE_DEL,
	ver_567_KEYCODE_HOME, ver_567_KEYCODE_END, ver_567_KEYCODE_PGUP, ver_567_KEYCODE_PGDN, ver_567_KEYCODE_LEFT,
	ver_567_KEYCODE_RIGHT, ver_567_KEYCODE_UP, ver_567_KEYCODE_DOWN,
	ver_567_KEYCODE_SLASH_PAD, ver_567_KEYCODE_ASTERISK, ver_567_KEYCODE_MINUS_PAD, ver_567_KEYCODE_PLUS_PAD,
	ver_567_KEYCODE_DEL_PAD, ver_567_KEYCODE_ENTER_PAD, ver_567_KEYCODE_PRTSCR, ver_567_KEYCODE_PAUSE,
	ver_567_KEYCODE_LSHIFT, ver_567_KEYCODE_RSHIFT, ver_567_KEYCODE_LCONTROL, ver_567_KEYCODE_RCONTROL,
	ver_567_KEYCODE_LALT, ver_567_KEYCODE_RALT, ver_567_KEYCODE_SCRLOCK, ver_567_KEYCODE_NUMLOCK, ver_567_KEYCODE_CAPSLOCK,
	ver_567_KEYCODE_OTHER,
	ver_567_KEYCODE_NONE
};

enum
{
	ver_567_JOYCODE_START = 0xff00,
	ver_567_JOYCODE_1_LEFT,ver_567_JOYCODE_1_RIGHT,ver_567_JOYCODE_1_UP,ver_567_JOYCODE_1_DOWN,
	ver_567_JOYCODE_1_BUTTON1,ver_567_JOYCODE_1_BUTTON2,ver_567_JOYCODE_1_BUTTON3,
	ver_567_JOYCODE_1_BUTTON4,ver_567_JOYCODE_1_BUTTON5,ver_567_JOYCODE_1_BUTTON6,
	ver_567_JOYCODE_2_LEFT,ver_567_JOYCODE_2_RIGHT,ver_567_JOYCODE_2_UP,ver_567_JOYCODE_2_DOWN,
	ver_567_JOYCODE_2_BUTTON1,ver_567_JOYCODE_2_BUTTON2,ver_567_JOYCODE_2_BUTTON3,
	ver_567_JOYCODE_2_BUTTON4,ver_567_JOYCODE_2_BUTTON5,ver_567_JOYCODE_2_BUTTON6,
	ver_567_JOYCODE_3_LEFT,ver_567_JOYCODE_3_RIGHT,ver_567_JOYCODE_3_UP,ver_567_JOYCODE_3_DOWN,
	ver_567_JOYCODE_3_BUTTON1,ver_567_JOYCODE_3_BUTTON2,ver_567_JOYCODE_3_BUTTON3,
	ver_567_JOYCODE_3_BUTTON4,ver_567_JOYCODE_3_BUTTON5,ver_567_JOYCODE_3_BUTTON6,
	ver_567_JOYCODE_4_LEFT,ver_567_JOYCODE_4_RIGHT,ver_567_JOYCODE_4_UP,ver_567_JOYCODE_4_DOWN,
	ver_567_JOYCODE_4_BUTTON1,ver_567_JOYCODE_4_BUTTON2,ver_567_JOYCODE_4_BUTTON3,
	ver_567_JOYCODE_4_BUTTON4,ver_567_JOYCODE_4_BUTTON5,ver_567_JOYCODE_4_BUTTON6,
	ver_567_JOYCODE_OTHER,
	ver_567_JOYCODE_NONE
};

#define ver_567_IP_KEY_DEFAULT 0xffff
#define ver_567_IP_KEY_NONE 0xfffe
#define ver_567_IP_KEY_PREVIOUS 0xfffd
#define ver_567_IP_JOY_DEFAULT 0xffff
#define ver_567_IP_JOY_NONE 0xfffe
#define ver_567_IP_JOY_PREVIOUS 0xfffd
#define ver_567_IP_CODE_NOT 0xfffc
#define ver_567_IP_CODE_OR 0xfffb

struct ver_table {
	int pre;
	int post;
};

static struct ver_table ver_567_table_keyboard[] = {
	{ 0, CODE_NONE },
	{ ver_567_KEYCODE_A, KEYCODE_A },
	{ ver_567_KEYCODE_B, KEYCODE_B },
	{ ver_567_KEYCODE_C, KEYCODE_C },
	{ ver_567_KEYCODE_D, KEYCODE_D },
	{ ver_567_KEYCODE_E, KEYCODE_E },
	{ ver_567_KEYCODE_F, KEYCODE_F },
	{ ver_567_KEYCODE_G, KEYCODE_G },
	{ ver_567_KEYCODE_H, KEYCODE_H },
	{ ver_567_KEYCODE_I, KEYCODE_I },
	{ ver_567_KEYCODE_J, KEYCODE_J },
	{ ver_567_KEYCODE_K, KEYCODE_K },
	{ ver_567_KEYCODE_L, KEYCODE_L },
	{ ver_567_KEYCODE_M, KEYCODE_M },
	{ ver_567_KEYCODE_N, KEYCODE_N },
	{ ver_567_KEYCODE_O, KEYCODE_O },
	{ ver_567_KEYCODE_P, KEYCODE_P },
	{ ver_567_KEYCODE_Q, KEYCODE_Q },
	{ ver_567_KEYCODE_R, KEYCODE_R },
	{ ver_567_KEYCODE_S, KEYCODE_S },
	{ ver_567_KEYCODE_T, KEYCODE_T },
	{ ver_567_KEYCODE_U, KEYCODE_U },
	{ ver_567_KEYCODE_V, KEYCODE_V },
	{ ver_567_KEYCODE_W, KEYCODE_W },
	{ ver_567_KEYCODE_X, KEYCODE_X },
	{ ver_567_KEYCODE_Y, KEYCODE_Y },
	{ ver_567_KEYCODE_Z, KEYCODE_Z },
	{ ver_567_KEYCODE_0, KEYCODE_0 },
	{ ver_567_KEYCODE_1, KEYCODE_1 },
	{ ver_567_KEYCODE_2, KEYCODE_2 },
	{ ver_567_KEYCODE_3, KEYCODE_3 },
	{ ver_567_KEYCODE_4, KEYCODE_4 },
	{ ver_567_KEYCODE_5, KEYCODE_5 },
	{ ver_567_KEYCODE_6, KEYCODE_6 },
	{ ver_567_KEYCODE_7, KEYCODE_7 },
	{ ver_567_KEYCODE_8, KEYCODE_8 },
	{ ver_567_KEYCODE_9, KEYCODE_9 },
	{ ver_567_KEYCODE_0_PAD, KEYCODE_0_PAD },
	{ ver_567_KEYCODE_1_PAD, KEYCODE_1_PAD },
	{ ver_567_KEYCODE_2_PAD, KEYCODE_2_PAD },
	{ ver_567_KEYCODE_3_PAD, KEYCODE_3_PAD },
	{ ver_567_KEYCODE_4_PAD, KEYCODE_4_PAD },
	{ ver_567_KEYCODE_5_PAD, KEYCODE_5_PAD },
	{ ver_567_KEYCODE_6_PAD, KEYCODE_6_PAD },
	{ ver_567_KEYCODE_7_PAD, KEYCODE_7_PAD },
	{ ver_567_KEYCODE_8_PAD, KEYCODE_8_PAD },
	{ ver_567_KEYCODE_9_PAD, KEYCODE_9_PAD },
	{ ver_567_KEYCODE_F1, KEYCODE_F1 },
	{ ver_567_KEYCODE_F2, KEYCODE_F2 },
	{ ver_567_KEYCODE_F3, KEYCODE_F3 },
	{ ver_567_KEYCODE_F4, KEYCODE_F4 },
	{ ver_567_KEYCODE_F5, KEYCODE_F5 },
	{ ver_567_KEYCODE_F6, KEYCODE_F6 },
	{ ver_567_KEYCODE_F7, KEYCODE_F7 },
	{ ver_567_KEYCODE_F8, KEYCODE_F8 },
	{ ver_567_KEYCODE_F9, KEYCODE_F9 },
	{ ver_567_KEYCODE_F10, KEYCODE_F10 },
	{ ver_567_KEYCODE_F11, KEYCODE_F11 },
	{ ver_567_KEYCODE_F12, KEYCODE_F12 },
	{ ver_567_KEYCODE_ESC, KEYCODE_ESC },
	{ ver_567_KEYCODE_TILDE, KEYCODE_TILDE },
	{ ver_567_KEYCODE_MINUS, KEYCODE_MINUS },
	{ ver_567_KEYCODE_EQUALS, KEYCODE_EQUALS },
	{ ver_567_KEYCODE_BACKSPACE, KEYCODE_BACKSPACE },
	{ ver_567_KEYCODE_TAB, KEYCODE_TAB },
	{ ver_567_KEYCODE_OPENBRACE, KEYCODE_OPENBRACE },
	{ ver_567_KEYCODE_CLOSEBRACE, KEYCODE_CLOSEBRACE },
	{ ver_567_KEYCODE_ENTER, KEYCODE_ENTER },
	{ ver_567_KEYCODE_COLON, KEYCODE_COLON },
	{ ver_567_KEYCODE_QUOTE, KEYCODE_QUOTE },
	{ ver_567_KEYCODE_BACKSLASH, KEYCODE_BACKSLASH },
	{ ver_567_KEYCODE_BACKSLASH2, KEYCODE_BACKSLASH2 },
	{ ver_567_KEYCODE_COMMA, KEYCODE_COMMA },
	{ ver_567_KEYCODE_STOP, KEYCODE_STOP },
	{ ver_567_KEYCODE_SLASH, KEYCODE_SLASH },
	{ ver_567_KEYCODE_SPACE, KEYCODE_SPACE },
	{ ver_567_KEYCODE_INSERT, KEYCODE_INSERT },
	{ ver_567_KEYCODE_DEL, KEYCODE_DEL },
	{ ver_567_KEYCODE_HOME, KEYCODE_HOME },
	{ ver_567_KEYCODE_END, KEYCODE_END },
	{ ver_567_KEYCODE_PGUP, KEYCODE_PGUP },
	{ ver_567_KEYCODE_PGDN, KEYCODE_PGDN },
	{ ver_567_KEYCODE_LEFT, KEYCODE_LEFT },
	{ ver_567_KEYCODE_RIGHT, KEYCODE_RIGHT },
	{ ver_567_KEYCODE_UP, KEYCODE_UP },
	{ ver_567_KEYCODE_DOWN, KEYCODE_DOWN },
	{ ver_567_KEYCODE_SLASH_PAD, KEYCODE_SLASH_PAD },
	{ ver_567_KEYCODE_ASTERISK, KEYCODE_ASTERISK },
	{ ver_567_KEYCODE_MINUS_PAD, KEYCODE_MINUS_PAD },
	{ ver_567_KEYCODE_PLUS_PAD, KEYCODE_PLUS_PAD },
	{ ver_567_KEYCODE_DEL_PAD, KEYCODE_DEL_PAD },
	{ ver_567_KEYCODE_ENTER_PAD, KEYCODE_ENTER_PAD },
	{ ver_567_KEYCODE_PRTSCR, KEYCODE_PRTSCR },
	{ ver_567_KEYCODE_PAUSE, KEYCODE_PAUSE },
	{ ver_567_KEYCODE_LSHIFT, KEYCODE_LSHIFT },
	{ ver_567_KEYCODE_RSHIFT, KEYCODE_RSHIFT },
	{ ver_567_KEYCODE_LCONTROL, KEYCODE_LCONTROL },
	{ ver_567_KEYCODE_RCONTROL, KEYCODE_RCONTROL },
	{ ver_567_KEYCODE_LALT, KEYCODE_LALT },
	{ ver_567_KEYCODE_RALT, KEYCODE_RALT },
	{ ver_567_KEYCODE_SCRLOCK, KEYCODE_SCRLOCK },
	{ ver_567_KEYCODE_NUMLOCK, KEYCODE_NUMLOCK },
	{ ver_567_KEYCODE_CAPSLOCK, KEYCODE_CAPSLOCK },
	{ ver_567_KEYCODE_OTHER, CODE_OTHER },
	{ ver_567_KEYCODE_NONE, CODE_NONE },
	{ ver_567_IP_KEY_DEFAULT, CODE_DEFAULT },
	{ ver_567_IP_KEY_NONE, CODE_NONE },
	{ ver_567_IP_KEY_PREVIOUS, CODE_PREVIOUS },
	{ ver_567_IP_CODE_NOT, CODE_NOT },
	{ ver_567_IP_CODE_OR, CODE_OR },
	{ -1, -1 }
};

static struct ver_table ver_567_table_joystick[] = {
	{ 0, CODE_NONE },
	{ ver_567_JOYCODE_1_LEFT, JOYCODE_1_LEFT },
	{ ver_567_JOYCODE_1_RIGHT, JOYCODE_1_RIGHT },
	{ ver_567_JOYCODE_1_UP, JOYCODE_1_UP },
	{ ver_567_JOYCODE_1_DOWN, JOYCODE_1_DOWN },
	{ ver_567_JOYCODE_1_BUTTON1, JOYCODE_1_BUTTON1 },
	{ ver_567_JOYCODE_1_BUTTON2, JOYCODE_1_BUTTON2 },
	{ ver_567_JOYCODE_1_BUTTON3, JOYCODE_1_BUTTON3 },
	{ ver_567_JOYCODE_1_BUTTON4, JOYCODE_1_BUTTON4 },
	{ ver_567_JOYCODE_1_BUTTON5, JOYCODE_1_BUTTON5 },
	{ ver_567_JOYCODE_1_BUTTON6, JOYCODE_1_BUTTON6 },
	{ ver_567_JOYCODE_2_LEFT, JOYCODE_2_LEFT },
	{ ver_567_JOYCODE_2_RIGHT, JOYCODE_2_RIGHT },
	{ ver_567_JOYCODE_2_UP, JOYCODE_2_UP },
	{ ver_567_JOYCODE_2_DOWN, JOYCODE_2_DOWN },
	{ ver_567_JOYCODE_2_BUTTON1, JOYCODE_2_BUTTON1 },
	{ ver_567_JOYCODE_2_BUTTON2, JOYCODE_2_BUTTON2 },
	{ ver_567_JOYCODE_2_BUTTON3, JOYCODE_2_BUTTON3 },
	{ ver_567_JOYCODE_2_BUTTON4, JOYCODE_2_BUTTON4 },
	{ ver_567_JOYCODE_2_BUTTON5, JOYCODE_2_BUTTON5 },
	{ ver_567_JOYCODE_2_BUTTON6, JOYCODE_2_BUTTON6 },
	{ ver_567_JOYCODE_3_LEFT, JOYCODE_3_LEFT },
	{ ver_567_JOYCODE_3_RIGHT, JOYCODE_3_RIGHT },
	{ ver_567_JOYCODE_3_UP, JOYCODE_3_UP },
	{ ver_567_JOYCODE_3_DOWN, JOYCODE_3_DOWN },
	{ ver_567_JOYCODE_3_BUTTON1, JOYCODE_3_BUTTON1 },
	{ ver_567_JOYCODE_3_BUTTON2, JOYCODE_3_BUTTON2 },
	{ ver_567_JOYCODE_3_BUTTON3, JOYCODE_3_BUTTON3 },
	{ ver_567_JOYCODE_3_BUTTON4, JOYCODE_3_BUTTON4 },
	{ ver_567_JOYCODE_3_BUTTON5, JOYCODE_3_BUTTON5 },
	{ ver_567_JOYCODE_3_BUTTON6, JOYCODE_3_BUTTON6 },
	{ ver_567_JOYCODE_4_LEFT, JOYCODE_4_LEFT },
	{ ver_567_JOYCODE_4_RIGHT, JOYCODE_4_RIGHT },
	{ ver_567_JOYCODE_4_UP, JOYCODE_4_UP },
	{ ver_567_JOYCODE_4_DOWN, JOYCODE_4_DOWN },
	{ ver_567_JOYCODE_4_BUTTON1, JOYCODE_4_BUTTON1 },
	{ ver_567_JOYCODE_4_BUTTON2, JOYCODE_4_BUTTON2 },
	{ ver_567_JOYCODE_4_BUTTON3, JOYCODE_4_BUTTON3 },
	{ ver_567_JOYCODE_4_BUTTON4, JOYCODE_4_BUTTON4 },
	{ ver_567_JOYCODE_4_BUTTON5, JOYCODE_4_BUTTON5 },
	{ ver_567_JOYCODE_4_BUTTON6, JOYCODE_4_BUTTON6 },
	{ ver_567_JOYCODE_OTHER, CODE_OTHER },
	{ ver_567_JOYCODE_NONE, CODE_NONE },
	{ ver_567_IP_JOY_DEFAULT, CODE_DEFAULT },
	{ ver_567_IP_JOY_NONE, CODE_NONE },
	{ ver_567_IP_JOY_PREVIOUS, CODE_PREVIOUS },
	{ ver_567_IP_CODE_NOT, CODE_NOT },
	{ ver_567_IP_CODE_OR, CODE_OR },
	{ -1, -1 }
};

static int code_table_ver_567_keyboard(int v) {
	struct ver_table* i = ver_567_table_keyboard;

	while (i->pre!=-1 || i->post!=-1) {
		if (i->pre == v)
			return i->post;
		++i;
	}

	v = keyoscode_to_code(v);
	if (v != CODE_NONE)
		return v;

	return -1;
}

static int code_table_ver_567_joystick(int v) {
	struct ver_table* i = ver_567_table_joystick;

	while (i->pre!=-1 || i->post!=-1) {
		if (i->pre == v)
			return i->post;
		++i;
	}

	v = joyoscode_to_code(v);
	if (v != CODE_NONE)
		return v;

	return -1;
}

static int seq_partial_is_special_code(InputCode code) {
	if (code < __code_max)
		return 0;
	if (code == CODE_NOT)
		return 0;
	return 1;
}

static int seq_partial_read(void* f, InputSeq* seq, unsigned* pos, unsigned len, int (*code_table)(int))
{
	UINT16 w;
	unsigned j = 0;
	int code;

	if (readword(f,&w) != 0)
		return -1;
	++j;
	code = code_table(w);
	if (code==-1)
		return -1;

	/* if default + standard code, use the standard code */
	if (*pos == 1 && (*seq)[0] == CODE_DEFAULT && !seq_partial_is_special_code(code))
		*pos = 0;

	if (
		/* if a special code is present don't insert another code */
		(*pos == 0 || !seq_partial_is_special_code((*seq)[0])) &&
		/* don't insert NONE code */
		(code != CODE_NONE) &&
		/* don't insert a special code after another code */
		(*pos == 0 || !seq_partial_is_special_code(code) ))
	{
		if (*pos)
		{
			(*seq)[*pos] = CODE_OR;
			++*pos;
		}

		(*seq)[*pos] = code;
		++*pos;

		while (j<len)
		{
			if (readword(f,&w) != 0)
				return -1;
			++j;

			code = code_table(w);
			if (code==-1)
				return -1;
			if (code == CODE_NONE)
				break;

			(*seq)[*pos] = code;
			++*pos;
		}
	}

	while (j<len)
	{
		if (readword(f,&w) != 0)
			return -1;
		++j;
	}

	return 0;
}

static int seq_read_ver_5(void* f, InputSeq* seq)
{
	unsigned pos = 0;
	seq_set_0(seq);

	if (seq_partial_read(f,seq,&pos,1,code_table_ver_567_keyboard) != 0)
		return -1;
	if (seq_partial_read(f,seq,&pos,1,code_table_ver_567_joystick) != 0)
		return -1;
	return 0;
}

static int seq_read_ver_6(void* f, InputSeq* seq)
{
	unsigned pos = 0;
	seq_set_0(seq);

	if (seq_partial_read(f,seq,&pos,2,code_table_ver_567_keyboard) != 0)
		return -1;
	if (seq_partial_read(f,seq,&pos,2,code_table_ver_567_joystick) != 0)
		return -1;
	return 0;
}

static int seq_read_ver_7(void* f, InputSeq* seq)
{
	unsigned pos = 0;
	seq_set_0(seq);

	if (seq_partial_read(f,seq,&pos,8,code_table_ver_567_keyboard) != 0)
		return -1;
	if (seq_partial_read(f,seq,&pos,8,code_table_ver_567_joystick) != 0)
		return -1;
	return 0;
}

static int input_port_read_ver_5(void *f, struct InputPort *in)
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

	if (seq_read_ver_5(f,&in->seq) != 0)
		return -1;
	return 0;
}

static int input_port_read_ver_6(void *f, struct InputPort *in)
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

	if (seq_read_ver_6(f,&in->seq) != 0)
		return -1;
	return 0;
}

static int input_port_read_ver_7(void *f, struct InputPort *in)
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

	if (seq_read_ver_7(f,&in->seq) != 0)
		return -1;
	return 0;
}

#endif
