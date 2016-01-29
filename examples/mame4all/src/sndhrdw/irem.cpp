#include "driver.h"
#include "irem.h"
#include "cpu/m6800/m6800.h"



WRITE_HANDLER( irem_sound_cmd_w )
{
	if ((data & 0x80) == 0)
		soundlatch_w(0,data & 0x7f);
	else
		cpu_set_irq_line(1,0,HOLD_LINE);
}


static int port1,port2;

static WRITE_HANDLER( irem_port1_w )
{
	port1 = data;
}

static WRITE_HANDLER( irem_port2_w )
{
	/* write latch */
	if ((port2 & 0x01) && !(data & 0x01))
	{
		/* control or data port? */
		if (port2 & 0x04)
		{
			/* PSG 0 or 1? */
			if (port2 & 0x10)
				AY8910_control_port_1_w(0,port1);
			else if (port2 & 0x08)
				AY8910_control_port_0_w(0,port1);
		}
		else
		{
			/* PSG 0 or 1? */
			if (port2 & 0x10)
				AY8910_write_port_1_w(0,port1);
			else if (port2 & 0x08)
				AY8910_write_port_0_w(0,port1);
		}
	}
	port2 = data;
}


static READ_HANDLER( irem_port1_r )
{
	/* PSG 0 or 1? */
	if (port2 & 0x10)
		return AY8910_read_port_1_r(0);
	else if (port2 & 0x08)
		return AY8910_read_port_0_r(0);
	else return 0xff;
}



static WRITE_HANDLER( irem_adpcm_reset_w )
{
	MSM5205_reset_w(0,data & 1);
	MSM5205_reset_w(1,data & 2);
}

static WRITE_HANDLER( irem_adpcm_w )
{
	MSM5205_data_w(offset,data);
}

static void irem_adpcm_int(int data)
{
	cpu_set_nmi_line(1,PULSE_LINE);
}


struct AY8910interface irem_ay8910_interface =
{
	2,	/* 2 chips */
	910000,	/* .91 MHz ?? */
	{ 20, 20 },
	{ soundlatch_r, 0 },
	{ 0 },
	{ 0 },
	{ irem_adpcm_reset_w, 0 }
};

struct MSM5205interface irem_msm5205_interface =
{
	2,					/* 2 chips            */
	384000,				/* 384KHz             */
	{ irem_adpcm_int, 0 },/* interrupt function */
	{ MSM5205_S96_4B,MSM5205_S96_4B},	/* 4KHz  */
	{ 100, 100 }
};



struct MemoryReadAddress irem_sound_readmem[] =
{
	{ 0x0000, 0x001f, m6803_internal_registers_r },
	{ 0x0080, 0x00ff, MRA_RAM },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

struct MemoryWriteAddress irem_sound_writemem[] =
{
	{ 0x0000, 0x001f, m6803_internal_registers_w },
	{ 0x0080, 0x00ff, MWA_RAM },
	{ 0x0801, 0x0802, irem_adpcm_w },
	{ 0x9000, 0x9000, MWA_NOP },    /* IACK */
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

struct IOReadPort irem_sound_readport[] =
{
	{ M6803_PORT1, M6803_PORT1, irem_port1_r },
	{ -1 }	/* end of table */
};

struct IOWritePort irem_sound_writeport[] =
{
	{ M6803_PORT1, M6803_PORT1, irem_port1_w },
	{ M6803_PORT2, M6803_PORT2, irem_port2_w },
	{ -1 }	/* end of table */
};
