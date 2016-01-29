/***************************************************************************

	Seibu Sound System v1.02, games using this include:

		Raiden
		Dynamite Duke/The Double Dynamites
		Blood Brothers
		D-Con
		Zero Team
		Legionaire (YM2151 substituted for YM3812)
		Raiden 2 (YM2151 substituted for YM3812, plus extra MSM6205)
		Raiden DX (YM2151 substituted for YM3812, plus extra MSM6205)
		Cup Soccer (YM2151 substituted for YM3812, plus extra MSM6205)

	Related sound programs (not implemented yet):
		Dead Angle
		Cabal

***************************************************************************/

WRITE_HANDLER( seibu_rst10_ack_w );
WRITE_HANDLER( seibu_rst18_ack_w );
WRITE_HANDLER( seibu_bank_w );
WRITE_HANDLER( seibu_soundclear_w );
void seibu_ym3812_irqhandler(int linestate);
READ_HANDLER( seibu_soundlatch_r );
WRITE_HANDLER( seibu_soundlatch_w );
WRITE_HANDLER( seibu_main_data_w );
void seibu_sound_init_1(void);
void seibu_sound_init_2(void);
void seibu_sound_decrypt(void);
void install_seibu_sound_speedup(int cpu);

extern unsigned char *seibu_shared_sound_ram;

/**************************************************************************/

#define SEIBU_SOUND_SYSTEM_YM3812_MEMORY_MAP(input_port)			\
																	\
static struct MemoryReadAddress sound_readmem[] =					\
{																	\
	{ 0x0000, 0x1fff, MRA_ROM },									\
	{ 0x2000, 0x27ff, MRA_RAM },									\
	{ 0x4008, 0x4008, YM3812_status_port_0_r },						\
	{ 0x4010, 0x4012, seibu_soundlatch_r }, 						\
	{ 0x4013, 0x4013, input_port }, 								\
	{ 0x6000, 0x6000, OKIM6295_status_0_r },						\
	{ 0x8000, 0xffff, MRA_BANK1 },									\
	{ -1 }	/* end of table */										\
};																	\
				  													\
static struct MemoryWriteAddress sound_writemem[] =					\
{																	\
	{ 0x0000, 0x1fff, MWA_ROM },									\
	{ 0x2000, 0x27ff, MWA_RAM },									\
	{ 0x4000, 0x4000, seibu_soundclear_w },							\
	{ 0x4002, 0x4002, seibu_rst10_ack_w }, 							\
	{ 0x4003, 0x4003, seibu_rst18_ack_w }, 							\
	{ 0x4007, 0x4007, seibu_bank_w },								\
	{ 0x4008, 0x4008, YM3812_control_port_0_w },					\
	{ 0x4009, 0x4009, YM3812_write_port_0_w },						\
	{ 0x4018, 0x401f, seibu_main_data_w },							\
	{ 0x6000, 0x6000, OKIM6295_data_0_w },							\
	{ -1 }	/* end of table */										\
}


#define SEIBU_SOUND_SYSTEM_YM3812_HARDWARE(freq1,freq2,region)		\
																	\
static struct YM3812interface ym3812_interface =					\
{																	\
	1,																\
	freq1,															\
	{ 50 },															\
	{ seibu_ym3812_irqhandler },									\
};																	\
																	\
static struct OKIM6295interface okim6295_interface =				\
{																	\
	1,																\
	{ freq2 },														\
	{ region },														\
	{ 40 }															\
}

#define SEIBU_SOUND_SYSTEM_CPU(freq)								\
	CPU_Z80 | CPU_AUDIO_CPU,										\
	freq,															\
	sound_readmem,sound_writemem,0,0,								\
	ignore_interrupt,0

#define SEIBU_SOUND_SYSTEM_YM3812_INTERFACE							\
	{																\
		SOUND_YM3812,												\
		&ym3812_interface											\
	},																\
	{																\
		SOUND_OKIM6295,												\
		&okim6295_interface											\
	}

/**************************************************************************/

