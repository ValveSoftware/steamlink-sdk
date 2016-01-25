/**********************************************************************

    speaker.h
    Sound driver to emulate a simple speaker,
    driven by one or more output bits

**********************************************************************/
#ifndef SPEAKER_H
#define SPEAKER_H
#define MAX_SPEAKER 2

struct Speaker_interface
{
	int num;
	int mixing_level[MAX_SPEAKER];	/* mixing level in percent */
	int num_level[MAX_SPEAKER]; 	/* optional: number of levels (if not two) */
	INT16 *levels[MAX_SPEAKER]; 	/* optional: pointer to level lookup table */
};

int speaker_sh_start (const struct MachineSound *msound);
void speaker_sh_stop (void);
void speaker_sh_update (void);
void speaker_level_w (int which, int new_level);
#endif

