#ifndef PROFILER_H
#define PROFILER_H

/* profiling */
enum {
	PROFILER_END = -1,
	PROFILER_CPU1 = 0,
	PROFILER_CPU2,
	PROFILER_CPU3,
	PROFILER_CPU4,
	PROFILER_CPU5,
	PROFILER_CPU6,
	PROFILER_CPU7,
	PROFILER_CPU8,
	PROFILER_VIDEO,
	PROFILER_DRAWGFX,
	PROFILER_COPYBITMAP,
	PROFILER_TILEMAP_UPDATE,
	PROFILER_TILEMAP_RENDER,
	PROFILER_TILEMAP_DRAW,
	PROFILER_BLIT,
	PROFILER_SOUND,
	PROFILER_MIXER,
	PROFILER_TIMER_CALLBACK,
	PROFILER_HISCORE,	/* high score load can slow things down if incorrectly written */
	PROFILER_INPUT,		/* input.c and inptport.c */
	PROFILER_EXTRA,		/* everything else */

	/* the USER types are available to driver writers to profile */
	/* custom sections of the code */
	PROFILER_USER1,
	PROFILER_USER2,
	PROFILER_USER3,
	PROFILER_USER4,

	PROFILER_PROFILER,
	PROFILER_IDLE,
	PROFILER_TOTAL
};


/*
To start profiling a certain section, e.g. video:
profiler_mark(PROFILER_VIDEO);

to end profiling the current section:
profiler_mark(PROFILER_END);

the profiler handles a FILO list so calls may be nested.
*/

void profiler_mark(int type);

/* functions called by usrintf.c */
void profiler_start(void);
void profiler_stop(void);
void profiler_show(struct osd_bitmap *bitmap);

#endif	/* PROFILER_H */
