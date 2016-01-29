#ifndef ui_text_h
#define ui_text_h

/* Important: this must match the default_text list in ui_text.c! */
enum
{
	UI_first_entry = -1,

	UI_mame,

	/* copyright stuff */
	UI_copyright1,
	UI_copyright2,
	UI_copyright3,

	/* misc menu stuff */
	UI_returntomain,
	UI_returntoprior,
	UI_anykey,
	UI_on,
	UI_off,
	UI_NA,
	UI_INVALID,
	UI_none,
	UI_cpu,
	UI_address,
	UI_value,
	UI_sound,
	UI_sound_lc, /* lower-case version */
	UI_stereo,
	UI_vectorgame,
	UI_screenres,
	UI_text,
	UI_volume,
	UI_relative,
	UI_allchannels,
	UI_brightness,
	UI_gamma,
	UI_vectorintensity,
	UI_overclock,
	UI_allcpus,
	UI_historymissing,

	/* special characters */
	UI_leftarrow,
	UI_rightarrow,
	UI_uparrow,
	UI_downarrow,
	UI_lefthilight,
	UI_righthilight,

	/* warnings */
	UI_knownproblems,
	UI_imperfectcolors,
	UI_wrongcolors,
	UI_imperfectsound,
	UI_nosound,
	UI_nococktail,
	UI_brokengame,
	UI_brokenprotection,
	UI_workingclones,
	UI_typeok,
#ifdef MESS
	UI_comp1,
	UI_comp2,
#endif

	/* main menu */
	UI_inputgeneral,
	UI_dipswitches,
	UI_analogcontrols,
	UI_calibrate,
	UI_bookkeeping,
	UI_inputspecific,
	UI_gameinfo,
	UI_history,
	UI_resetgame,
	UI_returntogame,
#ifdef MESS
	UI_imageinfo,
	UI_filemanager,
	UI_tapecontrol,
#endif
	UI_cheat,
	UI_memorycard,

	/* input stuff */
	UI_keyjoyspeed,
	UI_reverse,
	UI_sensitivity,

	/* stats */
	UI_tickets,
	UI_coin,
	UI_locked,

	/* memory card */
	UI_loadcard,
	UI_ejectcard,
	UI_createcard,
	UI_resetcard,
	UI_loadfailed,
	UI_loadok,
	UI_cardejected,
	UI_cardcreated,
	UI_cardcreatedfailed,
	UI_cardcreatedfailed2,
	UI_carderror,

	/* cheat stuff */
	UI_enablecheat,
	UI_addeditcheat,
	UI_startcheat,
	UI_continuesearch,
	UI_viewresults,
	UI_restoreresults,
	UI_memorywatch,
	UI_generalhelp,
	UI_watchpoint,
	UI_disabled,
	UI_cheats,
	UI_watchpoints,
	UI_moreinfo,
	UI_moreinfoheader,
	UI_cheatname,
	UI_cheatdescription,
	UI_code,

	/* watchpoint stuff */
	UI_watchlength,
	UI_watchlabeltype,
	UI_watchlabel,
	UI_watchx,
	UI_watchy,
	UI_watch,

	/* search stuff */
	UI_search_lives,
	UI_search_timers,
	UI_search_energy,
	UI_search_status,
	UI_search_slow,
	UI_search_speed,
	UI_search_matches_found,
	UI_search_noinit,
	UI_search_nosave,
	UI_search_done,
	UI_search_OK,

	UI_last_entry
};

struct lang_struct
{
	int version;
	int multibyte;			/* UNUSED: 1 if this is a multibyte font/language */
	UINT8 *fontdata;		/* pointer to the raw font data to be decoded */
	UINT16 fontglyphs;		/* total number of glyps in the external font - 1 */
	char langname[255];
	char fontname[255];
	char author[255];
};

extern struct lang_struct lang;

int uistring_init (void *language_file);
void uistring_shutdown (void);

const char * ui_getstring (int string_num);

#endif

