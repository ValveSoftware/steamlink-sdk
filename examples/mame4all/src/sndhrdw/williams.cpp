/***************************************************************************

	Midway/Williams Audio Board
	---------------------------

	6809 MEMORY MAP

	Function                                  Address     R/W  Data
	---------------------------------------------------------------
	Program RAM                               0000-07FF   R/W  D0-D7

	Music (YM-2151)                           2000-2001   R/W  D0-D7

	6821 PIA                                  4000-4003   R/W  D0-D7

	HC55516 clock low, digit latch            6000        W    D0
	HC55516 clock high                        6800        W    xx

	Bank select                               7800        W    D0-D2

	Banked Program ROM                        8000-FFFF   R    D0-D7

****************************************************************************/

#include "driver.h"
#include "machine/6821pia.h"
#include "cpu/m6809/m6809.h"
#include "cpu/adsp2100/adsp2100.h"
#include "williams.h"
#include "osinline.h"

#include <math.h>


/***************************************************************************
	COMPILER SWITCHES
****************************************************************************/

#define DISABLE_FIRQ_SPEEDUP		0
#define DISABLE_LOOP_CATCHERS		0
#define DISABLE_DCS_SPEEDUP			0


/***************************************************************************
	CONSTANTS (from HC55516.c)
****************************************************************************/

#define	INTEGRATOR_LEAK_TC		0.001
#define	FILTER_DECAY_TC			0.004
#define	FILTER_CHARGE_TC		0.004
#define	FILTER_MIN				0.0416
#define	FILTER_MAX				1.0954
#define	SAMPLE_GAIN				10000.0

#define WILLIAMS_CVSD			0
#define WILLIAMS_ADPCM			1
#define WILLIAMS_NARC			2
#define WILLIAMS_DCS			3

#define DCS_BUFFER_SIZE			4096
#define DCS_BUFFER_MASK			(DCS_BUFFER_SIZE - 1)


/***************************************************************************
	STRUCTURES
****************************************************************************/

struct ym2151_state
{
	timer_tm	timer_base;
	timer_tm	timer_period[3];
	UINT16	timer_value[2];
	UINT8	timer_is_active[2];
	UINT8	current_register;
	UINT8	active_timer;
};

struct counter_state
{
	UINT8 *	downcount;
	UINT8 *	divisor;
	UINT8 *	value;
	UINT16	adjusted_divisor;
	UINT16	last_hotspot_counter;
	UINT16	hotspot_hit_count;
	UINT16	hotspot_start;
	UINT16	hotspot_stop;
	UINT16	last_read_pc;
	timer_tm	time_leftover;
	void *	update_timer;
	void *	enable_timer;
	UINT8	invalid;
};

struct cvsd_state
{
	UINT8 *	state;
	UINT8 *	address;
	UINT8 *	end;
	UINT8 *	bank;
	UINT32	bits_per_firq;
	UINT32	sample_step;
	UINT32	sample_position;
	UINT16	current_sample;
	UINT8	invalid;
	float	charge;
	float	decay;
	float	leak;
	float	integrator;
	float	filter;
	UINT8	shiftreg;
};

struct dac_state
{
	UINT8 *	state_bank;
	UINT8 *	address;
	UINT8 *	end;
	UINT8 *	volume;
	UINT32	bytes_per_firq;
	UINT32	sample_step;
	UINT32	sample_position;
	UINT16	current_sample;
	UINT8	invalid;
};

struct dcs_state
{
	UINT8 * mem;
	UINT16	size;
	UINT16	incs;
	void  * reg_timer;
	int		ireg;
	UINT16	ireg_base;
	UINT16	control_regs[32];
	UINT16	bank;
	UINT8	enabled;

	INT16 *	buffer;
	UINT32	buffer_in;
	UINT32	sample_step;
	UINT32	sample_position;
	INT16	current_sample;
	UINT16	latch_control;
};


/***************************************************************************
	STATIC GLOBALS
****************************************************************************/

UINT8 williams_sound_int_state;

static UINT8 williams_cpunum;
static UINT8 williams_pianum;

static UINT8 williams_audio_type;
static UINT8 adpcm_bank_count;

static struct counter_state counter;
static struct ym2151_state ym2151;
static struct cvsd_state cvsd;
static struct dac_state dac;
static struct dcs_state dcs;

static int dac_stream;
static int cvsd_stream;

static UINT8 *dcs_speedup1;
static UINT8 *dcs_speedup2;


/***************************************************************************
	PROTOTYPES
****************************************************************************/

static void init_audio_state(int first_time);
static void locate_audio_hotspot(UINT8 *base, UINT16 start);

static int williams_custom_start(const struct MachineSound *msound);

static int dcs_custom_start(const struct MachineSound *msound);
static void dcs_custom_stop(void);

static void williams_cvsd_ym2151_irq(int state);
static void williams_adpcm_ym2151_irq(int state);
static void williams_cvsd_irqa(int state);
static void williams_cvsd_irqb(int state);

static READ_HANDLER( williams_cvsd_pia_r );
static READ_HANDLER( williams_ym2151_r );
static READ_HANDLER( counter_down_r );
static READ_HANDLER( counter_value_r );

static WRITE_HANDLER( williams_dac_data_w );
static WRITE_HANDLER( williams_dac2_data_w );
static WRITE_HANDLER( williams_cvsd_pia_w );
static WRITE_HANDLER( williams_ym2151_w );
static WRITE_HANDLER( williams_cvsd_bank_select_w );

static READ_HANDLER( williams_adpcm_command_r );
static WRITE_HANDLER( williams_adpcm_bank_select_w );
static WRITE_HANDLER( williams_adpcm_6295_bank_select_w );

static READ_HANDLER( williams_narc_command_r );
static READ_HANDLER( williams_narc_command2_r );
static WRITE_HANDLER( williams_narc_command2_w );
static WRITE_HANDLER( williams_narc_master_bank_select_w );
static WRITE_HANDLER( williams_narc_slave_bank_select_w );

static void counter_enable(int param);
static WRITE_HANDLER( counter_divisor_w );
static WRITE_HANDLER( counter_down_w );
static WRITE_HANDLER( counter_value_w );

static WRITE_HANDLER( cvsd_state_w );
static WRITE_HANDLER( dac_state_bank_w );

static void update_counter(void);
static void cvsd_update(int num, INT16 *buffer, int length);
static void dac_update(int num, INT16 *buffer, int length);

static INT16 get_next_cvsd_sample(int bit);

/* ADSP */
static WRITE_HANDLER( williams_dcs_bank_select_w );
static READ_HANDLER( williams_dcs_bank_r );
static WRITE_HANDLER( williams_dcs_control_w );
static READ_HANDLER( williams_dcs_latch_r );
static WRITE_HANDLER( williams_dcs_latch_w );

static void sound_tx_callback( int port, INT32 data );

static void dcs_dac_update(int num, INT16 *buffer, int length);
static WRITE_HANDLER( dcs_speedup1_w );
static WRITE_HANDLER( dcs_speedup2_w );
static void dcs_speedup_common(void);


/***************************************************************************
	PROCESSOR STRUCTURES
****************************************************************************/

/* CVSD readmem/writemem structures */
struct MemoryReadAddress williams_cvsd_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x2000, 0x2001, williams_ym2151_r },
	{ 0x4000, 0x4003, williams_cvsd_pia_r },
	{ 0x8000, 0xffff, MRA_BANK6 },
	{ -1 }
};

struct MemoryWriteAddress williams_cvsd_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x2000, 0x2001, williams_ym2151_w },
	{ 0x4000, 0x4003, williams_cvsd_pia_w },
	{ 0x6000, 0x6000, hc55516_0_digit_clock_clear_w },
	{ 0x6800, 0x6800, hc55516_0_clock_set_w },
	{ 0x7800, 0x7800, williams_cvsd_bank_select_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};


/* ADPCM readmem/writemem structures */
struct MemoryReadAddress williams_adpcm_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2401, 0x2401, williams_ym2151_r },
	{ 0x2c00, 0x2c00, OKIM6295_status_0_r },
	{ 0x3000, 0x3000, williams_adpcm_command_r },
	{ 0x4000, 0xbfff, MRA_BANK6 },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }
};

struct MemoryWriteAddress williams_adpcm_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, williams_adpcm_bank_select_w },
	{ 0x2400, 0x2401, williams_ym2151_w },
	{ 0x2800, 0x2800, williams_dac_data_w },
	{ 0x2c00, 0x2c00, OKIM6295_data_0_w },
	{ 0x3400, 0x3400, williams_adpcm_6295_bank_select_w },
	{ 0x3c00, 0x3c00, MWA_NOP },/*mk_sound_talkback_w }, -- talkback port? */
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }
};


/* NARC master readmem/writemem structures */
struct MemoryReadAddress williams_narc_master_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2001, 0x2001, williams_ym2151_r },
	{ 0x3000, 0x3000, MRA_NOP },
	{ 0x3400, 0x3400, williams_narc_command_r },
	{ 0x4000, 0xbfff, MRA_BANK6 },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }
};

struct MemoryWriteAddress williams_narc_master_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2001, williams_ym2151_w },
	{ 0x2800, 0x2800, MWA_NOP },/*mk_sound_talkback_w }, -- talkback port? */
	{ 0x2c00, 0x2c00, williams_narc_command2_w },
	{ 0x3000, 0x3000, williams_dac_data_w },
	{ 0x3800, 0x3800, williams_narc_master_bank_select_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }
};


/* NARC slave readmem/writemem structures */
struct MemoryReadAddress williams_narc_slave_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x3000, 0x3000, MRA_NOP },
	{ 0x3400, 0x3400, williams_narc_command2_r },
	{ 0x4000, 0xbfff, MRA_BANK5 },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }
};

struct MemoryWriteAddress williams_narc_slave_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, hc55516_0_clock_set_w },
	{ 0x2400, 0x2400, hc55516_0_digit_clock_clear_w },
	{ 0x3000, 0x3000, williams_dac2_data_w },
	{ 0x3800, 0x3800, williams_narc_slave_bank_select_w },
	{ 0x3c00, 0x3c00, MWA_NOP },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }
};


/* DCS readmem/writemem structures */
struct MemoryReadAddress williams_dcs_readmem[] =
{
	{ ADSP_DATA_ADDR_RANGE(0x0000, 0x1fff), MRA_RAM },						/* ??? */
	{ ADSP_DATA_ADDR_RANGE(0x2000, 0x2fff), williams_dcs_bank_r },			/* banked roms read */
	{ ADSP_DATA_ADDR_RANGE(0x3400, 0x3400), williams_dcs_latch_r },			/* soundlatch read */
	{ ADSP_DATA_ADDR_RANGE(0x3800, 0x39ff), MRA_RAM },						/* internal data ram */
	{ ADSP_PGM_ADDR_RANGE (0x0000, 0x1fff), MRA_RAM },						/* internal/external program ram */
	{ -1 }  /* end of table */
};


struct MemoryWriteAddress williams_dcs_writemem[] =
{
	{ ADSP_DATA_ADDR_RANGE(0x0000, 0x1fff), MWA_RAM },						/* ??? */
	{ ADSP_DATA_ADDR_RANGE(0x3000, 0x3000), williams_dcs_bank_select_w },	/* bank selector */
	{ ADSP_DATA_ADDR_RANGE(0x3400, 0x3400), williams_dcs_latch_w },			/* soundlatch write */
	{ ADSP_DATA_ADDR_RANGE(0x3800, 0x39ff), MWA_RAM },						/* internal data ram */
	{ ADSP_DATA_ADDR_RANGE(0x3fe0, 0x3fff), williams_dcs_control_w },		/* adsp control regs */
	{ ADSP_PGM_ADDR_RANGE (0x0000, 0x1fff), MWA_RAM },						/* internal/external program ram */
	{ -1 }  /* end of table */
};


/* PIA structure */
static struct pia6821_interface williams_cvsd_pia_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ 0, 0, 0, 0, 0, 0,
	/*outputs: A/B,CA/B2       */ williams_dac_data_w, 0, 0, 0,
	/*irqs   : A/B             */ williams_cvsd_irqa, williams_cvsd_irqb
};


/***************************************************************************
	AUDIO STRUCTURES
****************************************************************************/

/* Custom structure (all non-DCS variants) */
struct CustomSound_interface williams_custom_interface =
{
	williams_custom_start,0,0
};

/* YM2151 structure (CVSD variant) */
struct YM2151interface williams_cvsd_ym2151_interface =
{
	1,			/* 1 chip */
	3579580,
	{ YM3012_VOL(10,MIXER_PAN_CENTER,10,MIXER_PAN_CENTER) },
	{ williams_cvsd_ym2151_irq }
};

/* YM2151 structure (ADPCM variant) */
struct YM2151interface williams_adpcm_ym2151_interface =
{
	1,			/* 1 chip */
	3579580,
	{ YM3012_VOL(10,MIXER_PAN_CENTER,10,MIXER_PAN_CENTER) },
	{ williams_adpcm_ym2151_irq }
};

/* DAC structure (CVSD variant) */
struct DACinterface williams_cvsd_dac_interface =
{
	1,
	{ 50 }
};

/* DAC structure (ADPCM variant) */
struct DACinterface williams_adpcm_dac_interface =
{
	1,
	{ 50 }
};

/* DAC structure (NARC variant) */
struct DACinterface williams_narc_dac_interface =
{
	2,
	{ 50, 50 }
};

/* CVSD structure */
struct hc55516_interface williams_cvsd_interface =
{
	1,			/* 1 chip */
	{ 80 }
};

/* OKIM6295 structure(s) */
struct OKIM6295interface williams_adpcm_6295_interface_REGION_SOUND1 =
{
	1,          	/* 1 chip */
	{ 8000 },       /* 8000 Hz frequency */
	{ REGION_SOUND1 },  /* memory */
	{ 50 }
};

/* Custom structure (DCS variant) */
struct CustomSound_interface williams_dcs_custom_interface =
{
	dcs_custom_start,dcs_custom_stop,0
};


/***************************************************************************
	INLINES
****************************************************************************/

INLINE UINT16 get_cvsd_address(void)
{
	if (cvsd.address)
		return cvsd.address[0] * 256 + cvsd.address[1];
	else
		return cpunum_get_reg(williams_cpunum, M6809_Y);
}

INLINE void set_cvsd_address(UINT16 address)
{
	if (cvsd.address)
	{
		cvsd.address[0] = address >> 8;
		cvsd.address[1] = address;
	}
	else
		cpunum_set_reg(williams_cpunum, M6809_Y, address);
}

INLINE UINT16 get_dac_address(void)
{
	if (dac.address)
		return dac.address[0] * 256 + dac.address[1];
	else
		return cpunum_get_reg(williams_cpunum, M6809_Y);
}

INLINE void set_dac_address(UINT16 address)
{
	if (dac.address)
	{
		dac.address[0] = address >> 8;
		dac.address[1] = address;
	}
	else
		cpunum_set_reg(williams_cpunum, M6809_Y, address);
}

INLINE UINT8 *get_cvsd_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1+williams_cpunum);
	int bank = data & 3;
	int quarter = (data >> 2) & 3;
	if (bank == 3) bank = 0;
	return &RAM[0x10000 + (bank * 0x20000) + (quarter * 0x8000)];
}

INLINE UINT8 *get_adpcm_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1+williams_cpunum);
	int bank = data & 7;
	return &RAM[0x10000 + (bank * 0x8000)];
}

INLINE UINT8 *get_narc_master_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1+williams_cpunum);
	int bank = data & 3;
	if (!(data & 4)) bank = 0;
	return &RAM[0x10000 + (bank * 0x8000)];
}

INLINE UINT8 *get_narc_slave_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1+williams_cpunum + 1);
	int bank = data & 7;
	return &RAM[0x10000 + (bank * 0x8000)];
}


/***************************************************************************
	INITIALIZATION
****************************************************************************/

void williams_cvsd_init(int cpunum, int pianum)
{
	UINT16 entry_point;
	UINT8 *RAM;

	/* configure the CPU */
	williams_cpunum = cpunum;
	williams_audio_type = WILLIAMS_CVSD;

	/* configure the PIA */
	williams_pianum = pianum;
	pia_config(pianum, PIA_STANDARD_ORDERING | PIA_8BIT, &williams_cvsd_pia_intf);

	/* reset the chip */
	williams_cvsd_reset_w(1);
	williams_cvsd_reset_w(0);

	/* determine the entry point; from there, we can choose the speedup addresses */
	RAM = memory_region(REGION_CPU1+cpunum);
	entry_point = RAM[0x17ffe] * 256 + RAM[0x17fff];
	switch (entry_point)
	{
		/* Joust 2 case */
		case 0x8045:
			counter.downcount = (UINT8*)install_mem_write_handler(cpunum, 0x217, 0x217, counter_down_w);
			counter.divisor = (UINT8*)install_mem_write_handler(cpunum, 0x216, 0x216, counter_divisor_w);
			counter.value 	= (UINT8*)install_mem_write_handler(cpunum, 0x214, 0x215, counter_value_w);

			install_mem_read_handler(cpunum, 0x217, 0x217, counter_down_r);
			install_mem_read_handler(cpunum, 0x214, 0x215, counter_value_r);

			cvsd.state		= (UINT8*)install_mem_write_handler(cpunum, 0x220, 0x221, cvsd_state_w);
			cvsd.address	= NULL;	/* in Y */
			cvsd.end		= &RAM[0x21d];
			cvsd.bank		= &RAM[0x21f];
			cvsd.bits_per_firq = 1;

			dac.state_bank	= NULL;
			dac.address		= NULL;
			dac.end			= NULL;
			dac.volume		= NULL;
			dac.bytes_per_firq = 0;
			break;

		/* Arch Rivals case */
		case 0x8067:
			counter.downcount = (UINT8*)install_mem_write_handler(cpunum, 0x239, 0x239, counter_down_w);
			counter.divisor = (UINT8*)install_mem_write_handler(cpunum, 0x238, 0x238, counter_divisor_w);
			counter.value 	= (UINT8*)install_mem_write_handler(cpunum, 0x236, 0x237, counter_value_w);

			install_mem_read_handler(cpunum, 0x239, 0x239, counter_down_r);
			install_mem_read_handler(cpunum, 0x236, 0x237, counter_value_r);

			cvsd.state		= (UINT8*)install_mem_write_handler(cpunum, 0x23e, 0x23f, cvsd_state_w);
			cvsd.address	= NULL;	/* in Y */
			cvsd.end		= &RAM[0x242];
			cvsd.bank		= &RAM[0x22b];
			cvsd.bits_per_firq = 1;

			dac.state_bank	= NULL;
			dac.address		= NULL;
			dac.end			= NULL;
			dac.volume		= NULL;
			dac.bytes_per_firq = 0;
			break;

		/* General CVSD case */
		case 0x80c8:
			counter.downcount = (UINT8*)install_mem_write_handler(cpunum, 0x23a, 0x23a, counter_down_w);
			counter.divisor = (UINT8*)install_mem_write_handler(cpunum, 0x238, 0x238, counter_divisor_w);
			counter.value 	= (UINT8*)install_mem_write_handler(cpunum, 0x236, 0x237, counter_value_w);

			install_mem_read_handler(cpunum, 0x23a, 0x23a, counter_down_r);
			install_mem_read_handler(cpunum, 0x236, 0x237, counter_value_r);

			cvsd.state		= (UINT8*)install_mem_write_handler(cpunum, 0x23f, 0x240, cvsd_state_w);
			cvsd.address	= &RAM[0x241];
			cvsd.end		= &RAM[0x243];
			cvsd.bank		= &RAM[0x22b];
			cvsd.bits_per_firq = 4;

			dac.state_bank	= (UINT8*)install_mem_write_handler(cpunum, 0x22c, 0x22c, dac_state_bank_w);
			dac.address		= NULL;	/* in Y */
			dac.end			= &RAM[0x234];
			dac.volume		= &RAM[0x231];
			dac.bytes_per_firq = 2;
			break;
	}

	/* find the hotspot for optimization */
	locate_audio_hotspot(&RAM[0x8000], 0x8000);

	/* reset the IRQ state */
	pia_set_input_ca1(williams_pianum, 1);

	/* initialize the global variables */
	init_audio_state(1);
}

void williams_adpcm_init(int cpunum)
{
	UINT16 entry_point;
	UINT8 *RAM;
	int i;

	/* configure the CPU */
	williams_cpunum = cpunum;
	williams_audio_type = WILLIAMS_ADPCM;

	/* install the fixed ROM */
	RAM = memory_region(REGION_CPU1+cpunum);
	memcpy(&RAM[0xc000], &RAM[0x4c000], 0x4000);

	/* reset the chip */
	williams_adpcm_reset_w(1);
	williams_adpcm_reset_w(0);

	/* determine the entry point; from there, we can choose the speedup addresses */
	entry_point = RAM[0xfffe] * 256 + RAM[0xffff];
	switch (entry_point)
	{
		/* General ADPCM case */
		case 0xdc51:
		case 0xdd51:
		case 0xdd55:
			counter.downcount 	= (UINT8*)install_mem_write_handler(cpunum, 0x238, 0x238, counter_down_w);
			counter.divisor = (UINT8*)install_mem_write_handler(cpunum, 0x236, 0x236, counter_divisor_w);
			counter.value 	= (UINT8*)install_mem_write_handler(cpunum, 0x234, 0x235, counter_value_w);

			install_mem_read_handler(cpunum, 0x238, 0x238, counter_down_r);
			install_mem_read_handler(cpunum, 0x234, 0x235, counter_value_r);

			cvsd.state		= NULL;
			cvsd.address	= NULL;
			cvsd.end		= NULL;
			cvsd.bank		= NULL;
			cvsd.bits_per_firq = 0;

			dac.state_bank	= (UINT8*)install_mem_write_handler(cpunum, 0x22a, 0x22a, dac_state_bank_w);
			dac.address		= NULL;	/* in Y */
			dac.end			= &RAM[0x232];
			dac.volume		= &RAM[0x22f];
			dac.bytes_per_firq = 1;
			break;

		/* Unknown case */
		default:
			break;
	}
	
	/* find the number of banks in the ADPCM space */
	for (i = 0; i < MAX_SOUND; i++)
		if (Machine->drv->sound[i].sound_type == SOUND_OKIM6295)
		{
			struct OKIM6295interface *intf = (struct OKIM6295interface *)Machine->drv->sound[i].sound_interface;
			adpcm_bank_count = memory_region_length(intf->region[0]) / 0x40000;
		}

	/* find the hotspot for optimization */
//	locate_audio_hotspot(&RAM[0x40000], 0xc000);

	/* initialize the global variables */
	init_audio_state(1);
}

void williams_narc_init(int cpunum)
{
	UINT16 entry_point;
	UINT8 *RAM;

	/* configure the CPU */
	williams_cpunum = cpunum;
	williams_audio_type = WILLIAMS_NARC;

	/* install the fixed ROM */
	RAM = memory_region(REGION_CPU1+cpunum + 1);
	memcpy(&RAM[0xc000], &RAM[0x4c000], 0x4000);
	RAM = memory_region(REGION_CPU1+cpunum);
	memcpy(&RAM[0xc000], &RAM[0x2c000], 0x4000);

	/* reset the chip */
	williams_narc_reset_w(1);
	williams_narc_reset_w(0);

	/* determine the entry point; from there, we can choose the speedup addresses */
	entry_point = RAM[0xfffe] * 256 + RAM[0xffff];
	switch (entry_point)
	{
		/* General ADPCM case */
		case 0xc060:
			counter.downcount 	= (UINT8*)install_mem_write_handler(cpunum, 0x249, 0x249, counter_down_w);
			counter.divisor = (UINT8*)install_mem_write_handler(cpunum, 0x248, 0x248, counter_divisor_w);
			counter.value 	= (UINT8*)install_mem_write_handler(cpunum, 0x246, 0x247, counter_value_w);

			install_mem_read_handler(cpunum, 0x249, 0x249, counter_down_r);
			install_mem_read_handler(cpunum, 0x246, 0x247, counter_value_r);

			cvsd.state		= NULL;
			cvsd.address	= NULL;
			cvsd.end		= NULL;
			cvsd.bank		= NULL;
			cvsd.bits_per_firq = 0;

			dac.state_bank	= (UINT8*)install_mem_write_handler(cpunum, 0x23c, 0x23c, dac_state_bank_w);
			dac.address		= NULL;	/* in Y */
			dac.end			= &RAM[0x244];
			dac.volume		= &RAM[0x241];
			dac.bytes_per_firq = 1;
			break;

		/* Unknown case */
		default:
			break;
	}

	/* find the hotspot for optimization */
	locate_audio_hotspot(&RAM[0x0000], 0xc000);

	/* initialize the global variables */
	init_audio_state(1);
}

static void williams_dcs_boot( void )
{
	UINT32	bank_offset = ( dcs.bank & 0x7ff ) << 12;
	UINT32	*src = ( UINT32 * )( ( memory_region( REGION_CPU1+williams_cpunum ) + ADSP2100_SIZE + bank_offset ) );
	UINT32	*dst = ( UINT32 * )( ( memory_region( REGION_CPU1+williams_cpunum ) + ADSP2100_PGM_OFFSET ) );
	UINT32	size;
	UINT32	i, data;

	/* see how many words we need to copy */
	data = src[0];
#ifdef LSB_FIRST // ************** not really tested yet ****************
	data = ( ( data & 0xff ) << 24 ) | ( ( data & 0xff00 ) << 8 ) | ( ( data >> 8 ) & 0xff00 ) | ( ( data >> 24 ) & 0xff );
#endif
	size = ( ( data & 0xff ) + 1 ) * 8;

	for( i = 0; i < size; i++ )
	{
		data = src[i];
#ifdef LSB_FIRST // ************** not really tested yet ****************
		data = ( ( data & 0xff ) << 24 ) | ( ( data & 0xff00 ) << 8 ) | ( ( data >> 8 ) & 0xff00 ) | ( ( data >> 24 ) & 0xff );
#endif
		data >>= 8;
		dst[i] = data;	
	}
}

void williams_dcs_init(int cpunum)
{
	int i;

	/* configure the CPU */
	williams_cpunum = cpunum;
	williams_audio_type = WILLIAMS_DCS;
	
	/* initialize our state structure and install the transmit callback */
	dcs.mem = 0;
	dcs.size = 0;
	dcs.incs = 0;
	dcs.ireg = 0;
	
	/* initialize the ADSP control regs */
	for( i = 0; i < sizeof(dcs.control_regs) / sizeof(dcs.control_regs[0]); i++ )
		dcs.control_regs[i] = 0;

	/* initialize banking */
	dcs.bank = 0;
	
	/* start with no sound output */
	dcs.enabled = 0;
	
	/* reset DAC generation */
	dcs.buffer_in = 0;
	dcs.sample_step = 0x10000;
	dcs.sample_position = 0;
	dcs.current_sample = 0;

	/* initialize the ADSP Tx callback */
	adsp2105_set_tx_callback( sound_tx_callback );
	
	/* clear all interrupts */
	cpu_set_irq_line( williams_cpunum, ADSP2105_IRQ0, CLEAR_LINE );
	cpu_set_irq_line( williams_cpunum, ADSP2105_IRQ1, CLEAR_LINE );
	cpu_set_irq_line( williams_cpunum, ADSP2105_IRQ2, CLEAR_LINE );

	/* install the speedup handler */
#if (!DISABLE_DCS_SPEEDUP)
	dcs_speedup1 = (UINT8*)install_mem_write_handler(williams_cpunum, ADSP_DATA_ADDR_RANGE(0x04f8, 0x04f8), dcs_speedup1_w);
	dcs_speedup2 = (UINT8*)install_mem_write_handler(williams_cpunum, ADSP_DATA_ADDR_RANGE(0x063d, 0x063d), dcs_speedup2_w);
#endif
	
	/* initialize the comm bits */
	dcs.latch_control = 0x0c00;

	/* boot */	
	williams_dcs_boot();
}

static void init_audio_state(int first_time)
{
	/* reset the YM2151 state */
	ym2151.timer_base = TIME_IN_SEC(1.0 / (3579580.0 / 64.0));
	ym2151.timer_period[0] = ym2151.timer_period[1] = ym2151.timer_period[2] = TIME_IN_SEC(1.0);
	ym2151.timer_value[0] = ym2151.timer_value[1] = 0;
	ym2151.timer_is_active[0] = ym2151.timer_is_active[1] = 0;
	ym2151.current_register = 0x13;
	ym2151.active_timer = 2;
	YM2151_sh_reset();

	/* reset the counter state */
	counter.adjusted_divisor = 256;
	counter.last_hotspot_counter = 0xffff;
	counter.hotspot_hit_count = 0;
	counter.time_leftover = 0;
	counter.last_read_pc = 0x0000;
	if (first_time)
		counter.update_timer = timer_set(TIME_NEVER, 0, NULL);
	counter.invalid = 1;
	if (!first_time && counter.enable_timer)
		timer_remove(counter.enable_timer);
	counter.enable_timer = timer_set(TIME_NEVER-1, 0, counter_enable);

	/* reset the CVSD generator */
	cvsd.sample_step = 0;
	cvsd.sample_position = 0x10000;
	cvsd.current_sample = 0;
	cvsd.invalid = 1;
	cvsd.charge = pow(exp(-1), 1.0 / (FILTER_CHARGE_TC * 16000.0));
	cvsd.decay = pow(exp(-1), 1.0 / (FILTER_DECAY_TC * 16000.0));
	cvsd.leak = pow(exp(-1), 1.0 / (INTEGRATOR_LEAK_TC * 16000.0));
	cvsd.integrator = 0;
	cvsd.filter = FILTER_MIN;
	cvsd.shiftreg = 0xaa;

	/* reset the DAC generator */
	dac.sample_step = 0;
	dac.sample_position = 0x10000;
	dac.current_sample = 0;
	dac.invalid = 1;

	/* clear all the interrupts */
	williams_sound_int_state = 0;
	cpu_set_irq_line(williams_cpunum, M6809_FIRQ_LINE, CLEAR_LINE);
	cpu_set_irq_line(williams_cpunum, M6809_IRQ_LINE, CLEAR_LINE);
	cpu_set_nmi_line(williams_cpunum, CLEAR_LINE);
	if (williams_audio_type == WILLIAMS_NARC)
	{
		cpu_set_irq_line(williams_cpunum + 1, M6809_FIRQ_LINE, CLEAR_LINE);
		cpu_set_irq_line(williams_cpunum + 1, M6809_IRQ_LINE, CLEAR_LINE);
		cpu_set_nmi_line(williams_cpunum + 1, CLEAR_LINE);
	}
}

static void locate_audio_hotspot(UINT8 *base, UINT16 start)
{
	int i;

	/* search for the loop that kills performance so we can optimize it */
	for (i = start; i < 0x10000; i++)
	{
		if (base[i + 0] == 0x1a && base[i + 1] == 0x50 &&			/* 1A 50       ORCC  #$0050  */
			base[i + 2] == 0x93 &&									/* 93 xx       SUBD  $xx     */
			base[i + 4] == 0xe3 && base[i + 5] == 0x4c &&			/* E3 4C       ADDD  $000C,U */
			base[i + 6] == 0x9e && base[i + 7] == base[i + 3] &&	/* 9E xx       LDX   $xx     */
			base[i + 8] == 0xaf && base[i + 9] == 0x4c &&			/* AF 4C       STX   $000C,U */
			base[i +10] == 0x1c && base[i +11] == 0xaf)				/* 1C AF       ANDCC #$00AF  */
		{
			counter.hotspot_start = i;
			counter.hotspot_stop = i + 12;
			//logerror("Found hotspot @ %04X", i);
			return;
		}
	}
	//logerror("Found no hotspot!");
}


/***************************************************************************
	CUSTOM SOUND INTERFACES
****************************************************************************/

static int williams_custom_start(const struct MachineSound *msound)
{
	(void)msound;

	/* allocate a DAC stream */
	dac_stream = stream_init("Accelerated DAC", 50, Machine->sample_rate, 0, dac_update);

	/* allocate a CVSD stream */
	cvsd_stream = stream_init("Accelerated CVSD", 40, Machine->sample_rate, 0, cvsd_update);

	return 0;
}

static int dcs_custom_start(const struct MachineSound *msound)
{
	/* allocate a DAC stream */
	dac_stream = stream_init("DCS DAC", 100, Machine->sample_rate, 0, dcs_dac_update);
	
	/* allocate memory for our buffer */
	dcs.buffer = (INT16*)malloc(DCS_BUFFER_SIZE * sizeof(INT16));
	if (!dcs.buffer)
		return 1;

	return 0;
}

static void dcs_custom_stop(void)
{
	if (dcs.buffer)
		free(dcs.buffer);
	dcs.buffer = NULL;
}


/***************************************************************************
	CVSD IRQ GENERATION CALLBACKS
****************************************************************************/

static void williams_cvsd_ym2151_irq(int state)
{
	pia_set_input_ca1(williams_pianum, !state);
}

static void williams_cvsd_irqa(int state)
{
	cpu_set_irq_line(williams_cpunum, M6809_FIRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}

static void williams_cvsd_irqb(int state)
{
	cpu_set_nmi_line(williams_cpunum, state ? ASSERT_LINE : CLEAR_LINE);
}


/***************************************************************************
	ADPCM IRQ GENERATION CALLBACKS
****************************************************************************/

static void williams_adpcm_ym2151_irq(int state)
{
	cpu_set_irq_line(williams_cpunum, M6809_FIRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}


/***************************************************************************
	CVSD BANK SELECT
****************************************************************************/

WRITE_HANDLER( williams_cvsd_bank_select_w )
{
	cpu_setbank(6, get_cvsd_bank_base(data));
}


/***************************************************************************
	ADPCM BANK SELECT
****************************************************************************/

WRITE_HANDLER( williams_adpcm_bank_select_w )
{
	cpu_setbank(6, get_adpcm_bank_base(data));
}

WRITE_HANDLER( williams_adpcm_6295_bank_select_w )
{
	if (adpcm_bank_count <= 3)
	{
		if (!(data & 0x04))
			OKIM6295_set_bank_base(0, ALL_VOICES, 0x00000);
		else if (data & 0x01)
			OKIM6295_set_bank_base(0, ALL_VOICES, 0x40000);
		else
			OKIM6295_set_bank_base(0, ALL_VOICES, 0x80000);
	}
	else
	{
		data &= 7;
		if (data != 0)
			OKIM6295_set_bank_base(0, ALL_VOICES, (data - 1) * 0x40000);
	}
}


/***************************************************************************
	NARC BANK SELECT
****************************************************************************/

WRITE_HANDLER( williams_narc_master_bank_select_w )
{
	cpu_setbank(6, get_narc_master_bank_base(data));
}

WRITE_HANDLER( williams_narc_slave_bank_select_w )
{
	cpu_setbank(5, get_narc_slave_bank_base(data));
}


/***************************************************************************
	DCS BANK SELECT
****************************************************************************/

WRITE_HANDLER( williams_dcs_bank_select_w )
{
	dcs.bank = data & 0x7ff;
	
	/* bit 11 = sound board led */
#if (!DISABLE_DCS_SPEEDUP)
	/* they write 0x800 here just before entering the stall loop */
	if (data == 0x800)
	{
		/* calculate the next buffer address */
		int source = cpu_get_reg( ADSP2100_I0+dcs.ireg );
		int ar = source + dcs.size / 2;
		
		/* check for wrapping */
		if ( ar >= ( dcs.ireg_base + dcs.size ) )
			ar = dcs.ireg_base;
		
		/* set it */
		cpu_set_reg( ADSP2100_AR, ar );

		/* go around the buffer syncing code, we sync manually */
		cpu_set_reg( ADSP2100_PC, cpu_get_pc() + 8 );
		cpu_spinuntil_int();
	}
#endif
}

READ_HANDLER( williams_dcs_bank_r )
{
	UINT8	*banks = memory_region( REGION_CPU1+williams_cpunum ) + ADSP2100_SIZE;

	offset >>= 1;
	offset += ( dcs.bank & 0x7ff ) << 12;
	return banks[offset];
}


/***************************************************************************
	CVSD COMMUNICATIONS
****************************************************************************/

static void williams_cvsd_delayed_data_w(int param)
{
	pia_set_input_b(williams_pianum, param & 0xff);
	pia_set_input_cb1(williams_pianum, param & 0x100);
	pia_set_input_cb2(williams_pianum, param & 0x200);
}

WRITE_HANDLER( williams_cvsd_data_w )
{
	timer_set(TIME_NOW, data, williams_cvsd_delayed_data_w);
}

void williams_cvsd_reset_w(int state)
{
	/* going high halts the CPU */
	if (state)
	{
		williams_cvsd_bank_select_w(0, 0);
		init_audio_state(0);
		cpu_set_reset_line(williams_cpunum, ASSERT_LINE);
	}
	/* going low resets and reactivates the CPU */
	else
		cpu_set_reset_line(williams_cpunum, CLEAR_LINE);
}


/***************************************************************************
	ADPCM COMMUNICATIONS
****************************************************************************/

READ_HANDLER( williams_adpcm_command_r )
{
	cpu_set_irq_line(williams_cpunum, M6809_IRQ_LINE, CLEAR_LINE);
	williams_sound_int_state = 0;
	return soundlatch_r(0);
}

WRITE_HANDLER( williams_adpcm_data_w )
{
	soundlatch_w(0, data & 0xff);
	if (!(data & 0x200))
	{
		cpu_set_irq_line(williams_cpunum, M6809_IRQ_LINE, ASSERT_LINE);
		williams_sound_int_state = 1;
	}
}

void williams_adpcm_reset_w(int state)
{
	/* going high halts the CPU */
	if (state)
	{
		williams_adpcm_bank_select_w(0, 0);
		init_audio_state(0);
		cpu_set_reset_line(williams_cpunum, ASSERT_LINE);
	}
	/* going low resets and reactivates the CPU */
	else
		cpu_set_reset_line(williams_cpunum, CLEAR_LINE);
}


/***************************************************************************
	NARC COMMUNICATIONS
****************************************************************************/

READ_HANDLER( williams_narc_command_r )
{
	cpu_set_nmi_line(williams_cpunum, CLEAR_LINE);
	cpu_set_irq_line(williams_cpunum, M6809_IRQ_LINE, CLEAR_LINE);
	williams_sound_int_state = 0;
	return soundlatch_r(0);
}

WRITE_HANDLER( williams_narc_data_w )
{
	soundlatch_w(0, data & 0xff);
	if (!(data & 0x100))
		cpu_set_nmi_line(williams_cpunum, ASSERT_LINE);
	if (!(data & 0x200))
	{
		cpu_set_irq_line(williams_cpunum, M6809_IRQ_LINE, ASSERT_LINE);
		williams_sound_int_state = 1;
	}
}

void williams_narc_reset_w(int state)
{
	/* going high halts the CPU */
	if (state)
	{
		williams_narc_master_bank_select_w(0, 0);
		williams_narc_slave_bank_select_w(0, 0);
		init_audio_state(0);
		cpu_set_reset_line(williams_cpunum, ASSERT_LINE);
		cpu_set_reset_line(williams_cpunum + 1, ASSERT_LINE);
	}
	/* going low resets and reactivates the CPU */
	else
	{
		cpu_set_reset_line(williams_cpunum, CLEAR_LINE);
		cpu_set_reset_line(williams_cpunum + 1, CLEAR_LINE);
	}
}

READ_HANDLER( williams_narc_command2_r )
{
	cpu_set_irq_line(williams_cpunum + 1, M6809_FIRQ_LINE, CLEAR_LINE);
	return soundlatch2_r(0);
}

WRITE_HANDLER( williams_narc_command2_w )
{
	soundlatch2_w(0, data & 0xff);
	cpu_set_irq_line(williams_cpunum + 1, M6809_FIRQ_LINE, ASSERT_LINE);
}


/***************************************************************************
	DCS COMMUNICATIONS
****************************************************************************/

READ_HANDLER( williams_dcs_data_r )
{
	/* data is actually only 8 bit (read from d8-d15) */
	dcs.latch_control |= 0x0400;
	
	return soundlatch2_r( 0 ) & 0xff;
}

WRITE_HANDLER( williams_dcs_data_w )
{
	/* data is actually only 8 bit (read from d8-d15) */
	dcs.latch_control &= ~0x800;
	soundlatch_w( 0, data );
	cpu_set_irq_line( williams_cpunum, ADSP2105_IRQ2, ASSERT_LINE );
}

READ_HANDLER( williams_dcs_control_r )
{
	/* this is read by the TMS before issuing a command to check */
	/* if the ADSP has read the last one yet. We give 50 usec to */
	/* the ADSP to read the latch and thus preventing any sound  */
	/* loss */
	if ( ( dcs.latch_control & 0x800 ) == 0 )
		cpu_spinuntil_time( TIME_IN_USEC(50) );
	
	return dcs.latch_control;
}

void williams_dcs_reset_w(int state)
{
	//logerror( "%08x: DCS reset\n", cpu_get_pc() );	
	
	/* going high halts the CPU */
	if ( state )
	{
		/* just run through the init code again */
		williams_dcs_init( williams_cpunum );
		cpu_set_reset_line(williams_cpunum, ASSERT_LINE);
	}
	/* going low resets and reactivates the CPU */
	else
	{
		cpu_set_reset_line(williams_cpunum, CLEAR_LINE);
	}
}

static READ_HANDLER( williams_dcs_latch_r )
{
	dcs.latch_control |= 0x800;
	/* clear the irq line */
	cpu_set_irq_line( williams_cpunum, ADSP2105_IRQ2, CLEAR_LINE );
	return soundlatch_r( 0 );
}

static WRITE_HANDLER( williams_dcs_latch_w )
{
	dcs.latch_control &= ~0x400;
	soundlatch2_w( 0, data );
}


/***************************************************************************
	YM2151 INTERFACES
****************************************************************************/

static READ_HANDLER( williams_ym2151_r )
{
	return YM2151_status_port_0_r(offset);
}

static WRITE_HANDLER( williams_ym2151_w )
{
	if (offset & 1)
	{
		/* handle timer registers here */
		switch (ym2151.current_register)
		{
			case 0x10:	/* timer A, upper 8 bits */
				update_counter();
				ym2151.timer_value[0] = (ym2151.timer_value[0] & 0x003) | (data << 2);
				ym2151.timer_period[0] = ym2151.timer_base * (timer_tm)(1024 - ym2151.timer_value[0]);
				break;

			case 0x11:	/* timer A, upper 8 bits */
				update_counter();
				ym2151.timer_value[0] = (ym2151.timer_value[0] & 0x3fc) | (data & 0x03);
				ym2151.timer_period[0] = ym2151.timer_base * (timer_tm)(1024 - ym2151.timer_value[0]);
				break;

			case 0x12:	/* timer B */
				update_counter();
				ym2151.timer_value[1] = data;
				ym2151.timer_period[1] = ym2151.timer_base * (timer_tm)((256 - ym2151.timer_value[1]) << 4);
				break;

			case 0x14:	/* timer control */

				/* enable/disable timer A */
				if ((data & 0x01) && (data & 0x04) && !ym2151.timer_is_active[0])
				{
					update_counter();
					ym2151.timer_is_active[0] = 1;
					ym2151.active_timer = 0;
				}
				else if (!(data & 0x01) && ym2151.timer_is_active[0])
				{
					ym2151.timer_is_active[0] = 0;
					ym2151.active_timer = ym2151.timer_is_active[1] ? 1 : 2;
				}

				/* enable/disable timer B */
				if ((data & 0x02) && (data & 0x08) && !ym2151.timer_is_active[1])
				{
					update_counter();
					ym2151.timer_is_active[1] = 1;
					ym2151.active_timer = 1;
				}
				else if (!(data & 0x02) && ym2151.timer_is_active[1])
				{
					ym2151.timer_is_active[1] = 0;
					ym2151.active_timer = ym2151.timer_is_active[0] ? 0 : 2;
				}
				break;

			default:	/* pass through everything else */
				YM2151_data_port_0_w(offset, data);
				break;
		}
	}
	else
	{
		if (!DISABLE_FIRQ_SPEEDUP)
			ym2151.current_register = data;

		/* only pass through register writes for non-timer registers */
		if (DISABLE_FIRQ_SPEEDUP || ym2151.current_register < 0x10 || ym2151.current_register > 0x14)
			YM2151_register_port_0_w(offset, data);
	}
}


/***************************************************************************
	PIA INTERFACES
****************************************************************************/

static READ_HANDLER( williams_cvsd_pia_r )
{
	return pia_read(williams_pianum, offset);
}

static WRITE_HANDLER( williams_cvsd_pia_w )
{
	pia_write(williams_pianum, offset, data);
}


/***************************************************************************
	DAC INTERFACES
****************************************************************************/

static WRITE_HANDLER( williams_dac_data_w )
{
	DAC_data_w(0, data);
}

static WRITE_HANDLER( williams_dac2_data_w )
{
	DAC_data_w(1, data);
}


/***************************************************************************
	SPEEDUP KLUDGES
****************************************************************************/

static void counter_enable(int param)
{
	counter.invalid = 0;
	counter.enable_timer = NULL;
	timer_reset(counter.update_timer, TIME_NEVER);

	/* the counter routines all reset the bank, but NARC is the only one that cares */
	if (williams_audio_type == WILLIAMS_NARC)
		williams_narc_master_bank_select_w(0, 5);
}

static void update_counter(void)
{
	timer_tm time_since_update, timer_period = ym2151.timer_period[ym2151.active_timer];
	int firqs_since_update, complete_ticks, downcounter;

	if (DISABLE_FIRQ_SPEEDUP || ym2151.active_timer == 2 || counter.invalid)
		return;

	/* compute the time since we last updated; if it's less than one period, do nothing */
	time_since_update = timer_timeelapsed(counter.update_timer) + counter.time_leftover;
	if (time_since_update < timer_period)
		return;

	/* compute the integral number of FIRQ interrupts that have occured since the last update */
	firqs_since_update = (int)(time_since_update / timer_period);

	/* keep track of any additional time */
	counter.time_leftover = time_since_update - (timer_tm)firqs_since_update * timer_period;

	/* reset the timer */
	timer_reset(counter.update_timer, TIME_NEVER);

	/* determine the number of complete ticks that have occurred */
	complete_ticks = firqs_since_update / counter.adjusted_divisor;

	/* subtract any remainder from the down counter, and carry if appropriate */
	downcounter = counter.downcount[0] - (firqs_since_update - complete_ticks * counter.adjusted_divisor);
	if (downcounter < 0)
	{
		downcounter += counter.adjusted_divisor;
		complete_ticks++;
	}

	/* add in the previous value of the counter */
	complete_ticks += counter.value[0] * 256 + counter.value[1];

	/* store the results */
	counter.value[0] = complete_ticks >> 8;
	counter.value[1] = complete_ticks;
	counter.downcount[0] = downcounter;
}

static WRITE_HANDLER( counter_divisor_w )
{
	update_counter();
	counter.divisor[offset] = data;
	counter.adjusted_divisor = data ? data : 256;
}

static READ_HANDLER( counter_down_r )
{
	update_counter();
	return counter.downcount[offset];
}

static WRITE_HANDLER( counter_down_w )
{
	update_counter();
	counter.downcount[offset] = data;
}

static READ_HANDLER( counter_value_r )
{
	UINT16 pc = cpu_getpreviouspc();

	/* only update the counter on the MSB */
	/* also, don't update it if we just read from it a few instructions back */
	if (offset == 0 && (pc <= counter.last_read_pc || pc > counter.last_read_pc + 16))
		update_counter();
	counter.last_read_pc = pc;

	/* on the second LSB read in the hotspot, check to see if we will be looping */
	if (offset == 1 && pc == counter.hotspot_start + 6 && !DISABLE_LOOP_CATCHERS)
	{
		UINT16 new_counter = counter.value[0] * 256 + counter.value[1];

		/* count how many hits in a row we got the same value */
		if (new_counter == counter.last_hotspot_counter)
			counter.hotspot_hit_count++;
		else
			counter.hotspot_hit_count = 0;
		counter.last_hotspot_counter = new_counter;

		/* more than 3 hits in a row and we optimize */
		if (counter.hotspot_hit_count > 3)
			*cpuintf[Machine->drv->cpu[williams_cpunum].cpu_type & ~CPU_FLAGS_MASK].icount -= 50;
	}

	return counter.value[offset];
}

static WRITE_HANDLER( counter_value_w )
{
	/* only update the counter after the LSB is written */
	if (offset == 1)
		update_counter();
	counter.value[offset] = data;
	counter.hotspot_hit_count = 0;
}


/***************************************************************************
	CVSD KLUDGES
****************************************************************************/

static void cvsd_start(int param)
{
	float sample_rate;
	int start, end;

	/* if interrupts are disabled, try again later */
	if (cpunum_get_reg(williams_cpunum, M6809_CC) & 0x40)
	{
		timer_set(TIME_IN_MSEC(1), 0, cvsd_start);
		return;
	}

	/* determine the start and end addresses */
	start = get_cvsd_address();
	end = cvsd.end[0] * 256 + cvsd.end[1];

	/* compute the effective sample rate */
	sample_rate = (float)cvsd.bits_per_firq / (((float)ym2151.timer_period[ym2151.active_timer])/((float)TIME_ONE_SEC));
	cvsd.sample_step = (int)(sample_rate * 65536.0 / (float)Machine->sample_rate);
	cvsd.invalid = 0;
}

static WRITE_HANDLER( cvsd_state_w )
{
	/* if we write a value here with a non-zero high bit, prepare to start playing */
	stream_update(cvsd_stream, 0);
	if (offset == 0 && !(data & 0x80) && ym2151.active_timer != 2)
	{
		cvsd.invalid = 1;
		timer_set(TIME_IN_MSEC(1), 0, cvsd_start);
	}

	cvsd.state[offset] = data;
}


/***************************************************************************
	DAC KLUDGES
****************************************************************************/

static void dac_start(int param)
{
	float sample_rate;
	int start, end;

	/* if interrupts are disabled, try again later */
	if (cpunum_get_reg(williams_cpunum, M6809_CC) & 0x40)
	{
		timer_set(TIME_IN_MSEC(1), 0, dac_start);
		return;
	}

	/* determine the start and end addresses */
	start = get_dac_address();
	end = dac.end[0] * 256 + dac.end[1];

	/* compute the effective sample rate */
	sample_rate = (float)dac.bytes_per_firq / (((float)ym2151.timer_period[ym2151.active_timer])/((float)TIME_ONE_SEC));
	dac.sample_step = (int)(sample_rate * 65536.0 / (float)Machine->sample_rate);
	dac.invalid = 0;
}

static WRITE_HANDLER( dac_state_bank_w )
{
	/* if we write a value here with a non-zero high bit, prepare to start playing */
	stream_update(dac_stream, 0);
	if (!(data & 0x80) && ym2151.active_timer != 2)
	{
		dac.invalid = 1;
		timer_set(TIME_IN_MSEC(1), 0, dac_start);
	}

	dac.state_bank[offset] = data;
}


/***************************************************************************
	SOUND GENERATION
****************************************************************************/

static void cvsd_update(int num, INT16 *buffer, int length)
{
	UINT8 *bank_base, *source, *end;
	UINT32 current;
	int i;

	/* CVSD generation */
	if (cvsd.state && !(cvsd.state[0] & 0x80))
	{
		UINT8 bits_left = cvsd.state[0];
		UINT8 current_byte = cvsd.state[1];

		/* determine start and end points */
		bank_base = get_cvsd_bank_base(cvsd.bank[0]) - 0x8000;
		source = bank_base + get_cvsd_address();
		end = bank_base + cvsd.end[0] * 256 + cvsd.end[1];

		current = cvsd.sample_position;

		/* fill in with samples until we hit the end or run out */
		for (i = 0; i < length; i++)
		{
			/* if we overflow to the next sample, process it */
			while (current > 0xffff)
			{
				if (bits_left-- == 0)
				{
					if (source >= end)
					{
						bits_left |= 0x80;
						cvsd.sample_position = 0x10000;
						cvsd.integrator = 0;
						cvsd.filter = FILTER_MIN;
						cvsd.shiftreg = 0xaa;
						memset(buffer, 0, (length - i) * sizeof(INT16));
						goto finished;
					}
					current_byte = *source++;
					bits_left = 7;
				}
				cvsd.current_sample = get_next_cvsd_sample(current_byte & (0x80 >> bits_left));
				current -= 0x10000;
			}

			*buffer++ = cvsd.current_sample;
			current += cvsd.sample_step;
		}

		/* update the final values */
	finished:
		set_cvsd_address(source - bank_base);
		cvsd.sample_position = current;
		cvsd.state[0] = bits_left;
		cvsd.state[1] = current_byte;
	}
	else
		memset(buffer, 0, length * sizeof(INT16));
}

static void dac_update(int num, INT16 *buffer, int length)
{
	UINT8 *bank_base, *source, *end;
	UINT32 current;
	int i;

	/* DAC generation */
	if (dac.state_bank && !(dac.state_bank[0] & 0x80))
	{
		UINT8 volume = dac.volume[0] / 2;

		/* determine start and end points */
		if (williams_audio_type == WILLIAMS_CVSD)
			bank_base = get_cvsd_bank_base(dac.state_bank[0]) - 0x8000;
		else if (williams_audio_type == WILLIAMS_ADPCM)
			bank_base = get_adpcm_bank_base(dac.state_bank[0]) - 0x4000;
		else
			bank_base = get_narc_master_bank_base(dac.state_bank[0]) - 0x4000;
		source = bank_base + get_dac_address();
		end = bank_base + dac.end[0] * 256 + dac.end[1];

		current = dac.sample_position;

		/* fill in with samples until we hit the end or run out */
		for (i = 0; i < length; i++)
		{
			/* if we overflow to the next sample, process it */
			while (current > 0xffff)
			{
				if (source >= end)
				{
					dac.state_bank[0] |= 0x80;
					dac.sample_position = 0x10000;
					memset(buffer, 0, (length - i) * sizeof(INT16));
					goto finished;
				}
				dac.current_sample = *source++ * volume;
				current -= 0x10000;
			}

			*buffer++ = dac.current_sample;
			current += dac.sample_step;
		}

		/* update the final values */
	finished:
		set_dac_address(source - bank_base);
		dac.sample_position = current;
	}
	else
		memset(buffer, 0, length * sizeof(INT16));
}

static void dcs_dac_update(int num, INT16 *buffer, int length)
{
	UINT32 current, step, indx;
	INT16 *source;
	int i;

	/* DAC generation */
	if (dcs.enabled)
	{
		source = dcs.buffer;
		current = dcs.sample_position;
		step = dcs.sample_step;

		/* fill in with samples until we hit the end or run out */
		for (i = 0; i < length; i++)
		{
			indx = current >> 16;
			if (indx >= dcs.buffer_in)
				break;
			current += step;
			*buffer++ = source[indx & DCS_BUFFER_MASK];
		}
		
		/* fill the rest with the last sample */
		for ( ; i < length; i++)
			*buffer++ = source[(dcs.buffer_in - 1) & DCS_BUFFER_MASK];
		
		/* mask off extra bits */
		while (current >= (DCS_BUFFER_SIZE << 16))
		{
			current -= DCS_BUFFER_SIZE << 16;
			dcs.buffer_in -= DCS_BUFFER_SIZE;
		}

		/* update the final values */
		dcs.sample_position = current;
	}
	else
		memset(buffer, 0, length * sizeof(INT16));
}


/***************************************************************************
	CVSD DECODING (cribbed from HC55516.c)
****************************************************************************/

INT16 get_next_cvsd_sample(int bit)
{
	float temp;

	/* move the estimator up or down a step based on the bit */
	if (bit)
	{
		cvsd.shiftreg = ((cvsd.shiftreg << 1) | 1) & 7;
		cvsd.integrator += cvsd.filter;
	}
	else
	{
		cvsd.shiftreg = (cvsd.shiftreg << 1) & 7;
		cvsd.integrator -= cvsd.filter;
	}

	/* simulate leakage */
	cvsd.integrator *= cvsd.leak;

	/* if we got all 0's or all 1's in the last n bits, bump the step up */
	if (cvsd.shiftreg == 0 || cvsd.shiftreg == 7)
	{
		cvsd.filter = FILTER_MAX - ((FILTER_MAX - cvsd.filter) * cvsd.charge);
		if (cvsd.filter > FILTER_MAX)
			cvsd.filter = FILTER_MAX;
	}

	/* simulate decay */
	else
	{
		cvsd.filter *= cvsd.decay;
		if (cvsd.filter < FILTER_MIN)
			cvsd.filter = FILTER_MIN;
	}

	/* compute the sample as a 32-bit word */
	temp = cvsd.integrator * SAMPLE_GAIN;

	/* compress the sample range to fit better in a 16-bit word */
	if (temp < 0)
		return (int)(temp / (-temp * (1.0 / 32768.0) + 1.0));
	else
		return (int)(temp / (temp * (1.0 / 32768.0) + 1.0));
}


/***************************************************************************
	ADSP CONTROL & TRANSMIT CALLBACK
****************************************************************************/

/*
	The ADSP2105 memory map when in boot rom mode is as follows:
	
	Program Memory:
	0x0000-0x03ff = Internal Program Ram (contents of boot rom gets copied here)
	0x0400-0x07ff = Reserved
	0x0800-0x3fff = External Program Ram
	
	Data Memory:
	0x0000-0x03ff = External Data - 0 Waitstates
	0x0400-0x07ff = External Data - 1 Waitstates
	0x0800-0x2fff = External Data - 2 Waitstates
	0x3000-0x33ff = External Data - 3 Waitstates
	0x3400-0x37ff = External Data - 4 Waitstates
	0x3800-0x39ff = Internal Data Ram
	0x3a00-0x3bff = Reserved (extra internal ram space on ADSP2101, etc)
	0x3c00-0x3fff = Memory Mapped control registers & reserved.
*/

/* These are the some of the control register, we dont use them all */
enum {
	S1_AUTOBUF_REG = 15,
	S1_RFSDIV_REG,
	S1_SCLKDIV_REG,
	S1_CONTROL_REG,
	S0_AUTOBUF_REG,
	S0_RFSDIV_REG,
	S0_SCLKDIV_REG,
	S0_CONTROL_REG,
	S0_MCTXLO_REG,
	S0_MCTXHI_REG,
	S0_MCRXLO_REG,
	S0_MCRXHI_REG,
	TIMER_SCALE_REG,
	TIMER_COUNT_REG,
	TIMER_PERIOD_REG,
	WAITSTATES_REG,
	SYSCONTROL_REG
};

WRITE_HANDLER( williams_dcs_control_w )
{
	dcs.control_regs[offset>>1] = data;
	
	switch( offset >> 1 ) {
		case SYSCONTROL_REG:
			if ( data & 0x0200 ) {
				/* boot force */
				cpu_set_reset_line( williams_cpunum, PULSE_LINE );
				williams_dcs_boot();
				dcs.control_regs[SYSCONTROL_REG] &= ~0x0200;
			}

			/* see if SPORT1 got disabled */
			stream_update(dac_stream, 0);
			if ( ( data & 0x0800 ) == 0 ) {
				dcs.enabled = 0;

				/* nuke the timer */
				if ( dcs.reg_timer ) {
					timer_remove( dcs.reg_timer );
					dcs.reg_timer = 0;
				}
			}
		break;
		
		case S1_AUTOBUF_REG:
			/* autobuffer off: nuke the timer, and disable the DAC */
			stream_update(dac_stream, 0);
			if ( ( data & 0x0002 ) == 0 ) {
				dcs.enabled = 0;
				
				if ( dcs.reg_timer ) {
					timer_remove( dcs.reg_timer );
					dcs.reg_timer = 0;
				}
			}
		break;
		
		case S1_CONTROL_REG:
			/*if ( ( ( data >> 4 ) & 3 ) == 2 )
				logerror( "Oh no!, the data is compresed with u-law encoding\n" );
			if ( ( ( data >> 4 ) & 3 ) == 3 )
				logerror( "Oh no!, the data is compresed with A-law encoding\n" );*/
		break;
	}
}


/***************************************************************************
	DCS IRQ GENERATION CALLBACKS
****************************************************************************/

static void williams_dcs_irq(int state)
{
	/* get the index register */
	int reg = cpunum_get_reg( williams_cpunum, ADSP2100_I0+dcs.ireg );

	/* translate into data memory bus address */
	int source = ADSP2100_DATA_OFFSET + ( reg << 1 );
	int i;
	
	/* copy the current data into the buffer */
	for (i = 0; i < dcs.size / 2; i++)
		dcs.buffer[dcs.buffer_in++ & DCS_BUFFER_MASK] = READ_WORD(&dcs.mem[source + i * 2]);
	
	/* increment it */
	reg += dcs.incs * dcs.size / 2;
	
	/* check for wrapping */
	if ( reg >= ( dcs.ireg_base + dcs.size ) )
	{
		/* reset the base pointer */
		reg = dcs.ireg_base;
		
		/* generate the (internal, thats why the pulse) irq */
		cpu_set_irq_line( williams_cpunum, ADSP2105_IRQ1, PULSE_LINE );
	}
	
	/* store it */
	cpunum_set_reg( williams_cpunum, ADSP2100_I0+dcs.ireg, reg );
	
#if (!DISABLE_DCS_SPEEDUP)
	/* this is the same trigger as an interrupt */
	cpu_trigger( -2000 + williams_cpunum );
#endif
}


static void sound_tx_callback( int port, INT32 data )
{
	/* check if it's for SPORT1 */
	if ( port != 1 )
		return;
	
	/* check if SPORT1 is enabled */
	if ( dcs.control_regs[SYSCONTROL_REG] & 0x0800 ) /* bit 11 */
	{
		/* we only support autobuffer here (wich is what this thing uses), bail if not enabled */
		if ( dcs.control_regs[S1_AUTOBUF_REG] & 0x0002 ) /* bit 1 */
		{
			/* get the autobuffer registers */
			int		mreg, lreg;
			UINT16	source;
			int		sample_rate;
			
			stream_update(dac_stream, 0);

			dcs.ireg = ( dcs.control_regs[S1_AUTOBUF_REG] >> 9 ) & 7;
			mreg = ( dcs.control_regs[S1_AUTOBUF_REG] >> 7 ) & 3;
			mreg |= dcs.ireg & 0x04; /* msb comes from ireg */
			lreg = dcs.ireg;
			
			/* now get the register contents in a more legible format */
			/* we depend on register indexes to be continuous (wich is the case in our core) */
			source = cpunum_get_reg( williams_cpunum, ADSP2100_I0+dcs.ireg );
			dcs.incs = cpunum_get_reg( williams_cpunum, ADSP2100_M0+mreg );
			dcs.size = cpunum_get_reg( williams_cpunum, ADSP2100_L0+lreg );
			
			/* get the base value, since we need to keep it around for wrapping */
			source--;
			
			/* make it go back one so we dont loose the first sample */
			cpunum_set_reg( williams_cpunum, ADSP2100_I0+dcs.ireg, source );
			
			/* save it as it is now */
			dcs.ireg_base = source;
			
			/* get the memory chunk to read the data from */
			dcs.mem = memory_region( REGION_CPU1+williams_cpunum );
			
			/* enable the dac playing */
			dcs.enabled = 1;
			
			/* calculate how long until we generate an interrupt */
			
			/* frequency in Hz per each bit sent */
			sample_rate = Machine->drv->cpu[williams_cpunum].cpu_clock / ( 2 * ( dcs.control_regs[S1_SCLKDIV_REG] + 1 ) );

			/* now put it down to samples, so we know what the channel frequency has to be */
			sample_rate /= 16;
			
			/* fire off a timer wich will hit every half-buffer */
			dcs.reg_timer = timer_pulse(TIME_IN_HZ(sample_rate) * (dcs.size / 2), 0, williams_dcs_irq);
			
			/* configure the DAC generator */
			dcs.sample_step = (int)(sample_rate * 65536.0 / (float)Machine->sample_rate);
			dcs.sample_position = 0;
			dcs.buffer_in = 0;

			return;
		}
		/*else
			logerror( "ADSP SPORT1: trying to transmit and autobuffer not enabled!\n" );*/
	}
	
	/* if we get there, something went wrong. Disable playing */
	stream_update(dac_stream, 0);
	dcs.enabled = 0;
	
	/* remove timer */
	if ( dcs.reg_timer )
	{
		timer_remove( dcs.reg_timer );
		dcs.reg_timer = 0;
	}
}

static WRITE_HANDLER( dcs_speedup1_w )
{
/*
	UMK3:	trigger = $04F8 = 2, PC = $00FD, SKIPTO = $0128
	MAXHANG:trigger = $04F8 = 2, PC = $00FD, SKIPTO = $0128
	OPENICE:trigger = $04F8 = 2, PC = $00FD, SKIPTO = $0128
*/
	COMBINE_WORD_MEM(&dcs_speedup1[offset], data);
	if (data == 2 && cpu_get_pc() == 0xfd)
		dcs_speedup_common();
}

static WRITE_HANDLER( dcs_speedup2_w )
{
/*
	MK2:	trigger = $063D = 2, PC = $00F6, SKIPTO = $0121
*/
	COMBINE_WORD_MEM(&dcs_speedup2[offset], data);
	if (data == 2 && cpu_get_pc() == 0xf6)
		dcs_speedup_common();
}

static void dcs_speedup_common()
{
/*
	00F4: AR = $0002
	00F5: DM($063D) = AR
	00F6: SI = $0040
	00F7: DM($063E) = SI
	00F8: SR = LSHIFT SI BY -1 (LO)
	00F9: DM($063F) = SR0
	00FA: M0 = $3FFF
	00FB: CNTR = $0006
	00FC: DO $0120 UNTIL CE
		00FD: I4 = $0780
		00FE: I5 = $0700
		00FF: I0 = $3800
		0100: I1 = $3800
		0101: AY0 = DM($063E)
		0102: M2 = AY0
		0103: MODIFY (I1,M2)
		0104: I2 = I1
		0105: AR = AY0 - 1
		0106: M3 = AR
		0107: CNTR = DM($063D)
		0108: DO $0119 UNTIL CE
			0109: CNTR = DM($063F)
			010A: MY0 = DM(I4,M5)
			010B: MY1 = DM(I5,M5)
			010C: MX0 = DM(I1,M1)
			010D: DO $0116 UNTIL CE
				010E: MR = MX0 * MY0 (SS), MX1 = DM(I1,M1)
				010F: MR = MR - MX1 * MY1 (RND), AY0 = DM(I0,M1)
				0110: MR = MX1 * MY0 (SS), AX0 = MR1
				0111: MR = MR + MX0 * MY1 (RND), AY1 = DM(I0,M0)
				0112: AR = AY0 - AX0, MX0 = DM(I1,M1)
				0113: AR = AX0 + AY0, DM(I0,M1) = AR
				0114: AR = AY1 - MR1, DM(I2,M1) = AR
				0115: AR = MR1 + AY1, DM(I0,M1) = AR
				0116: DM(I2,M1) = AR
			0117: MODIFY (I2,M2)
			0118: MODIFY (I1,M3)
			0119: MODIFY (I0,M2)
		011A: SI = DM($063D)
		011B: SR = LSHIFT SI BY 1 (LO)
		011C: DM($063D) = SR0
		011D: SI = DM($063F)
		011E: DM($063E) = SI
		011F: SR = LSHIFT SI BY -1 (LO)
		0120: DM($063F) = SR0
*/

	INT16 *source = (INT16 *)memory_region(REGION_CPU1 + williams_cpunum);
	int mem63d = 2;
	int mem63e = 0x40;
	int mem63f = mem63e >> 1;
	int i, j, k;
#ifdef clip_short
	clip_short_pre();
#endif
	
	for (i = 0; i < 6; i++)
	{
		INT16 *i4 = &source[0x780];
		INT16 *i5 = &source[0x700];
		INT16 *i0 = &source[0x3800];
		INT16 *i1 = &source[0x3800 + mem63e];
		INT16 *i2 = i1;

		for (j = 0; j < mem63d; j++)
		{
			INT32 mx0, mx1, my0, my1, ax0, ay0, ay1, mr1, temp;
		
			my0 = *i4++;
			my1 = *i5++;
			
			for (k = 0; k < mem63f; k++)
			{
				mx0 = *i1++;
				mx1 = *i1++;
				ax0 = (mx0 * my0 - mx1 * my1) >> 15;
				mr1 = (mx1 * my0 + mx0 * my1) >> 15;
				ay0 = i0[0];
				ay1 = i0[1];

				temp = ay0 - ax0;
#ifndef clip_short
				if (temp < -32768) temp = -32768;
				else if (temp > 32767) temp = 32767;
#else
                clip_short(temp);
#endif
				*i0++ = temp;

				temp = ax0 + ay0;
#ifndef clip_short
				if (temp < -32768) temp = -32768;
				else if (temp > 32767) temp = 32767;
#else
                clip_short(temp);
#endif
				*i2++ = temp;

				temp = ay1 - mr1;
#ifndef clip_short
				if (temp < -32768) temp = -32768;
				else if (temp > 32767) temp = 32767;
#else
                clip_short(temp);
#endif
				*i0++ = temp;

				temp = ay1 + mr1;
#ifndef clip_short
				if (temp < -32768) temp = -32768;
				else if (temp > 32767) temp = 32767;
#else
                clip_short(temp);
#endif
				*i2++ = temp;
			}
			i2 += mem63e;
			i1 += mem63e;
			i0 += mem63e;
		}
		mem63d <<= 1;
		mem63e = mem63f;
		mem63f >>= 1;
	}
	cpu_set_reg(ADSP2100_PC, cpu_get_pc() + 0x121 - 0xf6);
}
