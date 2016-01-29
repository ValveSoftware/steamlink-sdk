/***************************************************************************

	Irem audio interface

****************************************************************************/


WRITE_HANDLER( irem_sound_cmd_w );


extern struct MemoryReadAddress irem_sound_readmem[];
extern struct MemoryWriteAddress irem_sound_writemem[];
extern struct IOReadPort irem_sound_readport[];
extern struct IOWritePort irem_sound_writeport[];

extern struct AY8910interface irem_ay8910_interface;
extern struct MSM5205interface irem_msm5205_interface;

#define IREM_AUDIO_CPU										\
	{														\
		CPU_M6803 | CPU_AUDIO_CPU,							\
		3579545/4,											\
		irem_sound_readmem,irem_sound_writemem,				\
		irem_sound_readport,irem_sound_writeport,			\
		0,0													\
	}

#define IREM_AUDIO											\
	{														\
		SOUND_AY8910,										\
		&irem_ay8910_interface								\
	},														\
	{														\
		SOUND_MSM5205,										\
		&irem_msm5205_interface								\
	}
