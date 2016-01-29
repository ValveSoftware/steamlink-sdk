/***************************************************************************

	speaker.c
	Sound driver to emulate a simple speaker,
	driven by one or more output bits

****************************************************************************/
#include "driver.h"

static INT16 default_levels[2] = {0,32767};

struct speaker
{
	int channel;
	INT16 *levels;
	int num_levels;
	int level;
	int mixing_level;
};

static struct Speaker_interface *intf;
static struct speaker speaker[MAX_SPEAKER];

void speaker_sh_init(int which, int speaker_num_levels, INT16 *speaker_levels)
{
	struct speaker *sp = &speaker[which];
	sp->levels = speaker_levels;
	sp->num_levels = speaker_num_levels;
}

static void speaker_sound_update(int param, INT16 *buffer, int length)
{
	struct speaker *sp = &speaker[param];
	int volume = sp->levels[sp->level] * sp->mixing_level / 100;

    while( length-- > 0 )
		*buffer++ = volume;
}

int speaker_sh_start(const struct MachineSound *msound)
{
	int i;

    intf = msound->sound_interface;

    for( i = 0; i < intf->num; i++ )
	{
        char buf[32];
		struct speaker *sp = &speaker[i];
        sp->mixing_level = intf->mixing_level[i];
		if( intf->num > 1 )
			sprintf(buf, "Speaker #%d", i+1);
		else
			strcpy(buf, "Speaker");
		sp->channel = stream_init(buf, sp->mixing_level, Machine->sample_rate, 0, speaker_sound_update);
		if( sp->channel == -1 )
			return 1;
		sp->num_levels = 2;
		sp->levels = default_levels;
		sp->level = 0;
    }
	return 0;
}

void speaker_sh_stop(void)
{
	/* nothing */
}

void speaker_sh_update(void)
{
	int i;
	for( i = 0; i < intf->num; i++ )
		stream_update(speaker[i].channel, 0);
}

void speaker_level_w(int which, int new_level)
{
	struct speaker *sp = &speaker[which];

    if( new_level < 0 )
		new_level = 0;
	else
	if( new_level >= sp->num_levels )
		new_level = sp->num_levels - 1;

	if( new_level == sp->level )
		return;

    /* force streams.c to update sound until this point in time now */
	stream_update(sp->channel, 0);

	sp->level = new_level;
}


