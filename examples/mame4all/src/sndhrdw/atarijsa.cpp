/***************************************************************************

Atari Audio Board II
--------------------

6502 MEMORY MAP

Function                                  Address     R/W  Data
---------------------------------------------------------------
Program RAM                               0000-1FFF   R/W  D0-D7

Music (YM-2151)                           2000-2001   R/W  D0-D7

Read 68010 Port (Input Buffer)            280A        R    D0-D7

Self-test                                 280C        R    D7
Output Buffer Full (@2A02) (Active High)              R    D5
Left Coin Switch                                      R    D1
Right Coin Switch                                     R    D0

Interrupt acknowledge                     2A00        W    xx
Write 68010 Port (Outbut Buffer)          2A02        W    D0-D7
Banked ROM select (at 3000-3FFF)          2A04        W    D6-D7
???                                       2A06        W

Effects                                   2C00-2C0F   R/W  D0-D7

Banked Program ROM (4 pages)              3000-3FFF   R    D0-D7
Static Program ROM (48K bytes)            4000-FFFF   R    D0-D7

****************************************************************************/

#include "machine/atarigen.h"
#include "sndhrdw/atarijsa.h"
#include "cpu/m6502/m6502.h"

static UINT8 *bank_base;
static UINT8 *bank_source_data;

static UINT8 speech_data;
static UINT8 last_ctl;

static UINT8 cpu_num;
static UINT8 input_port;
static UINT8 test_port;
static UINT16 test_mask;

static UINT8 has_pokey;
static UINT8 has_ym2151;
static UINT8 has_tms5220;
static UINT8 has_oki6295;

static UINT32 oki6295_bank_base;

static UINT8 overall_volume;
static UINT8 pokey_volume;
static UINT8 ym2151_volume;
static UINT8 tms5220_volume;
static UINT8 oki6295_volume;

static void update_all_volumes(void);

static READ_HANDLER( jsa1_io_r );
static WRITE_HANDLER( jsa1_io_w );
static READ_HANDLER( jsa2_io_r );
static WRITE_HANDLER( jsa2_io_w );
static READ_HANDLER( jsa3_io_r );
static WRITE_HANDLER( jsa3_io_w );


/*************************************
 *
 *	External interfaces
 *
 *************************************/

void atarijsa_init(int cpunum, int inputport, int testport, int testmask)
{
	int i;

	/* copy in the parameters */
	cpu_num = cpunum;
	input_port = inputport;
	test_port = testport;
	test_mask = testmask;

	/* predetermine the bank base */
	bank_base = &memory_region(REGION_CPU1+cpunum)[0x03000];
	bank_source_data = &memory_region(REGION_CPU1+cpunum)[0x10000];

	/* determine which sound hardware is installed */
	has_tms5220 = has_oki6295 = has_pokey = has_ym2151 = 0;
	for (i = 0; i < MAX_SOUND; i++)
	{
		switch (Machine->drv->sound[i].sound_type)
		{
			case SOUND_TMS5220:
				has_tms5220 = 1;
				break;
			case SOUND_OKIM6295:
				has_oki6295 = 1;
				break;
			case SOUND_POKEY:
				has_pokey = 1;
				break;
			case SOUND_YM2151:
				has_ym2151 = 1;
				break;
		}
	}

	/* install POKEY memory handlers */
	if (has_pokey)
	{
		install_mem_read_handler(cpunum, 0x2c00, 0x2c0f, pokey1_r);
		install_mem_write_handler(cpunum, 0x2c00, 0x2c0f, pokey1_w);
	}

	atarijsa_reset();
}


void atarijsa_reset(void)
{
	/* reset the sound I/O system */
	atarigen_sound_io_reset(cpu_num);

	/* reset the static states */
	speech_data = 0;
	last_ctl = 0;
	oki6295_bank_base = 0x00000;
	overall_volume = 100;
	pokey_volume = 100;
	ym2151_volume = 100;
	tms5220_volume = 100;
	oki6295_volume = 100;

	/* Guardians of the Hood assumes we're reset to bank 0 on startup */
	memcpy(bank_base, &bank_source_data[0x0000], 0x1000);
}



/*************************************
 *
 *	JSA I I/O handlers
 *
 *************************************/

static READ_HANDLER( jsa1_io_r )
{
	int result = 0xff;

	switch (offset & 0x206)
	{
		case 0x000:		/* n/c */
			//logerror("atarijsa: Unknown read at %04X\n", offset & 0x206);
			break;

		case 0x002:		/* /RDP */
			result = atarigen_6502_sound_r(offset);
			break;

		case 0x004:		/* /RDIO */
			/*
				0x80 = self test
				0x40 = NMI line state (active low)
				0x20 = sound output full
				0x10 = TMS5220 ready (active low)
				0x08 = +5V
				0x04 = +5V
				0x02 = coin 2
				0x01 = coin 1
			*/
			result = readinputport(input_port);
			if (!(readinputport(test_port) & test_mask)) result ^= 0x80;
			if (atarigen_cpu_to_sound_ready) result ^= 0x40;
			if (atarigen_sound_to_cpu_ready) result ^= 0x20;
			if (!has_tms5220 || tms5220_ready_r()) result ^= 0x10;
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /VOICE */
		case 0x202:		/* /WRP */
		case 0x204:		/* /WRIO */
		case 0x206:		/* /MIX */
			//logerror("atarijsa: Unknown read at %04X\n", offset & 0x206);
			break;
	}

	return result;
}


static WRITE_HANDLER( jsa1_io_w )
{
	switch (offset & 0x206)
	{
		case 0x000:		/* n/c */
		case 0x002:		/* /RDP */
		case 0x004:		/* /RDIO */
			//logerror("atarijsa: Unknown write (%02X) at %04X\n", data & 0xff, offset & 0x206);
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /VOICE */
			speech_data = data;
			break;

		case 0x202:		/* /WRP */
			atarigen_6502_sound_w(offset, data);
			break;

		case 0x204:		/* WRIO */
			/*
				0xc0 = bank address
				0x20 = coin counter 2
				0x10 = coin counter 1
				0x08 = squeak (tweaks the 5220 frequency)
				0x04 = TMS5220 reset (active low)
				0x02 = TMS5220 write strobe (active low)
				0x01 = YM2151 reset (active low)
			*/

			/* handle TMS5220 I/O */
			if (has_tms5220)
			{
				int count;

				if (((data ^ last_ctl) & 0x02) && (data & 0x02))
					tms5220_data_w(0, speech_data);
				count = 5 | ((data >> 2) & 2);
				tms5220_set_frequency(ATARI_CLOCK_14MHz/2 / (16 - count));
			}

			/* update the bank */
			memcpy(bank_base, &bank_source_data[0x1000 * ((data >> 6) & 3)], 0x1000);
			last_ctl = data;
			break;

		case 0x206:		/* MIX */
			/*
				0xc0 = TMS5220 volume (0-3)
				0x30 = POKEY volume (0-3)
				0x0e = YM2151 volume (0-7)
				0x01 = low-pass filter enable
			*/
			tms5220_volume = ((data >> 6) & 3) * 100 / 3;
			pokey_volume = ((data >> 4) & 3) * 100 / 3;
			ym2151_volume = ((data >> 1) & 7) * 100 / 7;
			update_all_volumes();
			break;
	}
}



/*************************************
 *
 *	JSA II I/O handlers
 *
 *************************************/

static READ_HANDLER( jsa2_io_r )
{
	int result = 0xff;

	switch (offset & 0x206)
	{
		case 0x000:		/* /RDV */
			if (has_oki6295)
				result = OKIM6295_status_0_r(offset);
			/*else
				logerror("atarijsa: Unknown read at %04X\n", offset & 0x206);*/
			break;

		case 0x002:		/* /RDP */
			result = atarigen_6502_sound_r(offset);
			break;

		case 0x004:		/* /RDIO */
			/*
				0x80 = self test
				0x40 = NMI line state (active low)
				0x20 = sound output full
				0x10 = +5V
				0x08 = +5V
				0x04 = +5V
				0x02 = coin 2
				0x01 = coin 1
			*/
			result = readinputport(input_port);
			if (!(readinputport(test_port) & test_mask)) result ^= 0x80;
			if (atarigen_cpu_to_sound_ready) result ^= 0x40;
			if (atarigen_sound_to_cpu_ready) result ^= 0x20;
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /WRV */
		case 0x202:		/* /WRP */
		case 0x204:		/* /WRIO */
		case 0x206:		/* /MIX */
			//logerror("atarijsa: Unknown read at %04X\n", offset & 0x206);
			break;
	}

	return result;
}


static WRITE_HANDLER( jsa2_io_w )
{
	switch (offset & 0x206)
	{
		case 0x000:		/* /RDV */
		case 0x002:		/* /RDP */
		case 0x004:		/* /RDIO */
			//logerror("atarijsa: Unknown write (%02X) at %04X\n", data & 0xff, offset & 0x206);
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /WRV */
			if (has_oki6295)
				OKIM6295_data_0_w(offset, data);
			/*else
				logerror("atarijsa: Unknown write (%02X) at %04X\n", data & 0xff, offset & 0x206);*/
			break;

		case 0x202:		/* /WRP */
			atarigen_6502_sound_w(offset, data);
			break;

		case 0x204:		/* /WRIO */
			/*
				0xc0 = bank address
				0x20 = coin counter 2
				0x10 = coin counter 1
				0x08 = voice frequency (tweaks the OKI6295 frequency)
				0x04 = OKI6295 reset (active low)
				0x02 = n/c
				0x01 = YM2151 reset (active low)
			*/

			/* update the bank */
			memcpy(bank_base, &bank_source_data[0x1000 * ((data >> 6) & 3)], 0x1000);
			last_ctl = data;

			/* update the OKI frequency */
			OKIM6295_set_frequency(0, ALL_VOICES, ATARI_CLOCK_14MHz/4/3 / ((data & 8) ? 132 : 165));
			break;

		case 0x206:		/* /MIX */
			/*
				0xc0 = n/c
				0x20 = low-pass filter enable
				0x10 = n/c
				0x0e = YM2151 volume (0-7)
				0x01 = OKI6295 volume (0-1)
			*/
			ym2151_volume = ((data >> 1) & 7) * 100 / 7;
			oki6295_volume = 50 + (data & 1) * 50;
			update_all_volumes();
			break;
	}
}



/*************************************
 *
 *	JSA III I/O handlers
 *
 *************************************/

static READ_HANDLER( jsa3_io_r )
{
	int result = 0xff;

	switch (offset & 0x206)
	{
		case 0x000:		/* /RDV */
			if (has_oki6295)
				result = OKIM6295_status_0_r(offset);
			break;

		case 0x002:		/* /RDP */
			result = atarigen_6502_sound_r(offset);
			break;

		case 0x004:		/* /RDIO */
			/*
				0x80 = self test (active high)
				0x40 = NMI line state (active high)
				0x20 = sound output full (active high)
				0x10 = self test (active high)
				0x08 = service (active high)
				0x04 = tilt (active high)
				0x02 = coin L (active high)
				0x01 = coin R (active high)
			*/
			result = readinputport(input_port);
			if (!(readinputport(test_port) & test_mask)) result ^= 0x90;
			if (atarigen_cpu_to_sound_ready) result ^= 0x40;
			if (atarigen_sound_to_cpu_ready) result ^= 0x20;
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /WRV */
		case 0x202:		/* /WRP */
		case 0x204:		/* /WRIO */
		case 0x206:		/* /MIX */
			//logerror("atarijsa: Unknown read at %04X\n", offset & 0x206);
			break;
	}

	return result;
}


static WRITE_HANDLER( jsa3_io_w )
{
	switch (offset & 0x206)
	{
		case 0x000:		/* /RDV */
			overall_volume = data * 100 / 127;
			update_all_volumes();
			break;

		case 0x002:		/* /RDP */
		case 0x004:		/* /RDIO */
			//logerror("atarijsa: Unknown write (%02X) at %04X\n", data & 0xff, offset & 0x206);
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /WRV */
			if (has_oki6295)
				OKIM6295_data_0_w(offset, data);
			break;

		case 0x202:		/* /WRP */
			atarigen_6502_sound_w(offset, data);
			break;

		case 0x204:		/* /WRIO */
			/*
				0xc0 = bank address
				0x20 = coin counter 2
				0x10 = coin counter 1
				0x08 = voice frequency (tweaks the OKI6295 frequency)
				0x04 = OKI6295 reset (active low)
				0x02 = OKI6295 bank bit 0
				0x01 = YM2151 reset (active low)
			*/

			/* update the OKI bank */
			oki6295_bank_base = (0x40000 * ((data >> 1) & 1)) | (oki6295_bank_base & 0x80000);
			OKIM6295_set_bank_base(0, ALL_VOICES, oki6295_bank_base);

			/* update the bank */
			memcpy(bank_base, &bank_source_data[0x1000 * ((data >> 6) & 3)], 0x1000);
			last_ctl = data;

			/* update the OKI frequency */
			OKIM6295_set_frequency(0, ALL_VOICES, ATARI_CLOCK_14MHz/4/3 / ((data & 8) ? 132 : 165));
			break;

		case 0x206:		/* /MIX */
			/*
				0xc0 = n/c
				0x20 = low-pass filter enable
				0x10 = OKI6295 bank bit 1
				0x0e = YM2151 volume (0-7)
				0x01 = OKI6295 volume (0-1)
			*/

			/* update the OKI bank */
			oki6295_bank_base = (0x80000 * ((data >> 4) & 1)) | (oki6295_bank_base & 0x40000);
			OKIM6295_set_bank_base(0, ALL_VOICES, oki6295_bank_base);

			/* update the volumes */
			ym2151_volume = ((data >> 1) & 7) * 100 / 7;
			oki6295_volume = 50 + (data & 1) * 50;
			update_all_volumes();
			break;
	}
}



/*************************************
 *
 *	JSA IIIS I/O handlers
 *
 *************************************/

static READ_HANDLER( jsa3s_io_r )
{
	int result = 0xff;

	switch (offset & 0x206)
	{
		case 0x000:		/* /RDV */
			if (has_oki6295)
			{
				if (offset & 1)
					result = OKIM6295_status_1_r(offset);
				else
					result = OKIM6295_status_0_r(offset);
			}
			break;

		case 0x002:		/* /RDP */
			result = atarigen_6502_sound_r(offset);
			break;

		case 0x004:		/* /RDIO */
			/*
				0x80 = self test (active high)
				0x40 = NMI line state (active high)
				0x20 = sound output full (active high)
				0x10 = self test (active high)
				0x08 = service (active high)
				0x04 = tilt (active high)
				0x02 = coin L (active high)
				0x01 = coin R (active high)
			*/
			result = readinputport(input_port);
			if (!(readinputport(test_port) & test_mask)) result ^= 0x90;
			if (atarigen_cpu_to_sound_ready) result ^= 0x40;
			if (atarigen_sound_to_cpu_ready) result ^= 0x20;
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /WRV */
		case 0x202:		/* /WRP */
		case 0x204:		/* /WRIO */
		case 0x206:		/* /MIX */
			//logerror("atarijsa: Unknown read at %04X\n", offset & 0x206);
			break;
	}

	return result;
}


static WRITE_HANDLER( jsa3s_io_w )
{
	switch (offset & 0x206)
	{
		case 0x000:		/* /RDV */
			overall_volume = data * 100 / 127;
			update_all_volumes();
			break;

		case 0x002:		/* /RDP */
		case 0x004:		/* /RDIO */
			//logerror("atarijsa: Unknown write (%02X) at %04X\n", data & 0xff, offset & 0x206);
			break;

		case 0x006:		/* /IRQACK */
			atarigen_6502_irq_ack_r(0);
			break;

		case 0x200:		/* /WRV */
			if (has_oki6295)
			{
				if (offset & 1)
					OKIM6295_data_1_w(offset, data);
				else
					OKIM6295_data_0_w(offset, data);
			}
			break;

		case 0x202:		/* /WRP */
			atarigen_6502_sound_w(offset, data);
			break;

		case 0x204:		/* /WRIO */
			/*
				0xc0 = bank address
				0x20 = coin counter 2
				0x10 = coin counter 1
				0x08 = voice frequency (tweaks the OKI6295 frequency)
				0x04 = OKI6295 reset (active low)
				0x02 = left OKI6295 bank bit 0
				0x01 = YM2151 reset (active low)
			*/

			/* update the OKI bank */
			oki6295_bank_base = (0x40000 * ((data >> 1) & 1)) | (oki6295_bank_base & 0x80000);
			OKIM6295_set_bank_base(0, ALL_VOICES, oki6295_bank_base);

			/* update the bank */
			memcpy(bank_base, &bank_source_data[0x1000 * ((data >> 6) & 3)], 0x1000);
			last_ctl = data;

			/* update the OKI frequency */
			OKIM6295_set_frequency(0, ALL_VOICES, ATARI_CLOCK_14MHz/4/3 / ((data & 8) ? 132 : 165));
			OKIM6295_set_frequency(1, ALL_VOICES, ATARI_CLOCK_14MHz/4/3 / ((data & 8) ? 132 : 165));
			break;

		case 0x206:		/* /MIX */
			/*
				0xc0 = right OKI6295 bank bits 0-1
				0x20 = low-pass filter enable
				0x10 = left OKI6295 bank bit 1
				0x0e = YM2151 volume (0-7)
				0x01 = OKI6295 volume (0-1)
			*/

			/* update the OKI bank */
			oki6295_bank_base = (0x80000 * ((data >> 4) & 1)) | (oki6295_bank_base & 0x40000);
			OKIM6295_set_bank_base(0, ALL_VOICES, oki6295_bank_base);
			OKIM6295_set_bank_base(1, ALL_VOICES, 0x40000 * (data >> 6));

			/* update the volumes */
			ym2151_volume = ((data >> 1) & 7) * 100 / 7;
			oki6295_volume = 50 + (data & 1) * 50;
			update_all_volumes();
			break;
	}
}



/*************************************
 *
 *	Volume helpers
 *
 *************************************/

static void update_all_volumes(void)
{
	if (has_pokey) atarigen_set_pokey_vol(overall_volume * pokey_volume / 100);
	if (has_ym2151) atarigen_set_ym2151_vol(overall_volume * ym2151_volume / 100);
	if (has_tms5220) atarigen_set_tms5220_vol(overall_volume * tms5220_volume / 100);
	if (has_oki6295) atarigen_set_oki6295_vol(overall_volume * oki6295_volume / 100);
}



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

struct MemoryReadAddress atarijsa1_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2000, 0x2001, YM2151_status_port_0_r },
	{ 0x2800, 0x2bff, jsa1_io_r },
	{ 0x3000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryWriteAddress atarijsa1_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x2800, 0x2bff, jsa1_io_w },
	{ 0x3000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryReadAddress atarijsa2_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2000, 0x2001, YM2151_status_port_0_r },
	{ 0x2800, 0x2bff, jsa2_io_r },
	{ 0x3000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryWriteAddress atarijsa2_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x2800, 0x2bff, jsa2_io_w },
	{ 0x3000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryReadAddress atarijsa3_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2000, 0x2001, YM2151_status_port_0_r },
	{ 0x2800, 0x2bff, jsa3_io_r },
	{ 0x3000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryWriteAddress atarijsa3_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x2800, 0x2bff, jsa3_io_w },
	{ 0x3000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryReadAddress atarijsa3s_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2000, 0x2001, YM2151_status_port_0_r },
	{ 0x2800, 0x2bff, jsa3s_io_r },
	{ 0x3000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryWriteAddress atarijsa3s_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x2800, 0x2bff, jsa3s_io_w },
	{ 0x3000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

struct TMS5220interface atarijsa_tms5220_interface =
{
	ATARI_CLOCK_14MHz/2/11,	/* potentially ATARI_CLOCK_14MHz/2/9 as well */
	100,
	0
};


struct POKEYinterface atarijsa_pokey_interface =
{
	1,			/* 1 chip */
	ATARI_CLOCK_14MHz/8,
	{ 40 },
};


struct YM2151interface atarijsa_ym2151_interface_mono =
{
	1,			/* 1 chip */
	ATARI_CLOCK_14MHz/4,
	{ YM3012_VOL(30,MIXER_PAN_CENTER,30,MIXER_PAN_CENTER) },
	{ atarigen_ym2151_irq_gen }
};


struct YM2151interface atarijsa_ym2151_interface_stereo =
{
	1,			/* 1 chip */
	ATARI_CLOCK_14MHz/4,
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ atarigen_ym2151_irq_gen }
};


struct YM2151interface atarijsa_ym2151_interface_stereo_swapped =
{
	1,			/* 1 chip */
	ATARI_CLOCK_14MHz/4,
	{ YM3012_VOL(60,MIXER_PAN_RIGHT,60,MIXER_PAN_LEFT) },
	{ atarigen_ym2151_irq_gen }
};


struct OKIM6295interface atarijsa_okim6295_interface_REGION_SOUND1 =
{
	1,              /* 1 chip */
	{ ATARI_CLOCK_14MHz/4/3/132 },
	{ REGION_SOUND1 },
	{ 75 }
};


struct OKIM6295interface atarijsa_okim6295s_interface_REGION_SOUND1 =
{
	2, 				/* 2 chips */
	{ ATARI_CLOCK_14MHz/4/3/132, ATARI_CLOCK_14MHz/4/3/132 },
	{ REGION_SOUND1, REGION_SOUND1 },
	{ MIXER(75,MIXER_PAN_LEFT), MIXER(75,MIXER_PAN_RIGHT) }
};
