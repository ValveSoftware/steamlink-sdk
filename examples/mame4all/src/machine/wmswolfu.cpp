/*************************************************************************

	Driver for Williams/Midway Wolf-unit games.

	Hints for finding speedups:
	
		search disassembly for ": CAF9"

**************************************************************************/

#include "driver.h"
#include "cpu/tms34010/tms34010.h"
#include "cpu/m6809/m6809.h"
#include "sndhrdw/williams.h"


/* constant definitions */
#define SOUND_DCS				1


/* speedup installation macros */
#define INSTALL_SPEEDUP_1_16BIT(addr, pc, spin1, offs1, offs2) \
	wms_speedup_pc = (pc); \
	wms_speedup_offset = ((addr) & 0x10) >> 3; \
	wms_speedup_spin[0] = spin1; \
	wms_speedup_spin[1] = offs1; \
	wms_speedup_spin[2] = offs2; \
	wms_speedup_base = (UINT8*)install_mem_read_handler(0, TOBYTE((addr) & ~0x1f), TOBYTE((addr) | 0x1f), wms_generic_speedup_1_16bit);
	
#define INSTALL_SPEEDUP_3(addr, pc, spin1, spin2, spin3) \
	wms_speedup_pc = (pc); \
	wms_speedup_offset = ((addr) & 0x10) >> 3; \
	wms_speedup_spin[0] = spin1; \
	wms_speedup_spin[1] = spin2; \
	wms_speedup_spin[2] = spin3; \
	wms_speedup_base = (UINT8*)install_mem_read_handler(0, TOBYTE((addr) & ~0x1f), TOBYTE((addr) | 0x1f), wms_generic_speedup_3);

#define INSTALL_SPEEDUP_1_ADDRESS( addr, pc ) \
	wms_speedup_pc = (pc); \
	wms_speedup_offset = ((addr) & 0x10) >> 3; \
	wms_speedup_base = (UINT8*)install_mem_read_handler(0, TOBYTE((addr) & ~0x1f), TOBYTE((addr) | 0x1f), wms_generic_speedup_1_address);



/* code-related variables */
extern UINT8 *	wms_code_rom;
       UINT8 *	wms_wolfu_decode_memory;

/* CMOS-related variables */
extern UINT8 *	wms_cmos_ram;
static UINT8	cmos_write_enable;

/* graphics-related variables */
extern UINT8 *	wms_gfx_rom;
extern size_t	wms_gfx_rom_size;

/* I/O-related variables */
static UINT16	iodata[8];

/* sound-related variables */
static UINT8	sound_type;

/* protection-related variables */
static UINT8	security_data[16];
static UINT8	security_buffer;
static UINT8	security_index;
static UINT8	security_status;

/* speedup-related variables */
extern offs_t 	wms_speedup_pc;
extern offs_t 	wms_speedup_offset;
extern offs_t 	wms_speedup_spin[3];
extern UINT8 *	wms_speedup_base;


/* prototype */
static READ_HANDLER( wms_wolfu_sound_state_r );

/* speedup-related prototypes */
extern READ_HANDLER( wms_generic_speedup_1_16bit );
extern READ_HANDLER( wms_generic_speedup_1_32bit );
extern READ_HANDLER( wms_generic_speedup_3 );



/*************************************
 *
 *	CMOS reads/writes
 *
 *************************************/

WRITE_HANDLER( wms_wolfu_cmos_enable_w )
{
	cmos_write_enable = 1;
}

WRITE_HANDLER( wms_wolfu_cmos_w )
{
	if (cmos_write_enable)
	{
		COMBINE_WORD_MEM(&wms_cmos_ram[offset], data);
		cmos_write_enable = 0;
	}
	/*else
	{
		logerror("%08X:Unexpected CMOS W @ %05X\n", cpu_get_pc(), offset);
		usrintf_showmessage("Bad CMOS write");
	}*/
}

READ_HANDLER( wms_wolfu_cmos_r )
{
	//logerror("%08X:CMOS R @ %05X\n", cpu_get_pc(), offset);
	return READ_WORD(&wms_cmos_ram[offset]);
}



/*************************************
 *
 *	General I/O writes
 *
 *************************************/

static WRITE_HANDLER( wms_wolfu_io_w )
{
	int oldword = READ_WORD(&iodata[offset / 2]);
	int newword = COMBINE_WORD(oldword, data);
	
	switch (offset / 2)
	{
		case 1:
			//logerror("%08X:Control W @ %05X = %04X\n", cpu_get_pc(), offset, data);

			/* bit 4 reset sound CPU */
			williams_dcs_reset_w(data & 0x10);

			/* bit 5 (active low) reset security chip */
			if (!(oldword & 0x20) && (newword & 0x20))
			{
				security_index = 0;
				security_status = 0;
				security_buffer = 0;
			}
			break;
		
		case 3:
			/* watchdog reset */
			/* MK3 resets with this enabled */
/*			watchdog_reset_w(0,0);*/
			break;

		default:
			//logerror("%08X:Unknown I/O write to %d = %04X\n", cpu_get_pc(), offset / 2, data);
			break;
	}
	WRITE_WORD(&iodata[offset / 2], newword);
}



/*************************************
 *
 *	General I/O reads
 *
 *************************************/

static READ_HANDLER( wms_wolfu_io_r )
{
	switch (offset / 2)
	{
		case 0:
		case 1:
		case 2:
		case 3:
			return readinputport(offset / 2);
		
		case 4:
			return (security_status << 12) | wms_wolfu_sound_state_r(0);
			
		default:
			//logerror("%08X:Unknown I/O read from %d\n", cpu_get_pc(), offset / 2);
			break;
	}
	return 0xffff;
}



/*************************************
 *
 *	Serial number encoding
 *
 *************************************/

static void generate_serial(int upper)
{
	int year = atoi(Machine->gamedrv->year), month = 12, day = 11;
	UINT32 serial_number, temp;
	UINT8 serial_digit[9];
	
	serial_number = 123456;
	serial_number += upper * 1000000;

	serial_digit[0] = (serial_number / 100000000) % 10;
	serial_digit[1] = (serial_number / 10000000) % 10;
	serial_digit[2] = (serial_number / 1000000) % 10;
	serial_digit[3] = (serial_number / 100000) % 10;
	serial_digit[4] = (serial_number / 10000) % 10;
	serial_digit[5] = (serial_number / 1000) % 10;
	serial_digit[6] = (serial_number / 100) % 10;
	serial_digit[7] = (serial_number / 10) % 10;
	serial_digit[8] = (serial_number / 1) % 10;

	security_data[12] = rand() & 0xff;
	security_data[13] = rand() & 0xff;

	security_data[14] = 0; /* ??? */
	security_data[15] = 0; /* ??? */

	temp = 0x174 * (year - 1980) + 0x1f * (month - 1) + day;
	security_data[10] = (temp >> 8) & 0xff;
	security_data[11] = temp & 0xff;

	temp = serial_digit[4] + serial_digit[7] * 10 + serial_digit[1] * 100;
	temp = (temp + 5 * security_data[13]) * 0x1bcd + 0x1f3f0;
	security_data[7] = temp & 0xff;
	security_data[8] = (temp >> 8) & 0xff;
	security_data[9] = (temp >> 16) & 0xff;

	temp = serial_digit[6] + serial_digit[8] * 10 + serial_digit[0] * 100 + serial_digit[2] * 10000;
	temp = (temp + 2 * security_data[13] + security_data[12]) * 0x107f + 0x71e259;
	security_data[3] = temp & 0xff;
	security_data[4] = (temp >> 8) & 0xff;
	security_data[5] = (temp >> 16) & 0xff;
	security_data[6] = (temp >> 24) & 0xff;

	temp = serial_digit[5] * 10 + serial_digit[3] * 100;
	temp = (temp + security_data[12]) * 0x245 + 0x3d74;
	security_data[0] = temp & 0xff;
	security_data[1] = (temp >> 8) & 0xff;
	security_data[2] = (temp >> 16) & 0xff;
}



/*************************************
 *
 *	Generic driver init
 *
 *************************************/

static void init_wolfu_generic(int sound)
{
	UINT8 *base;
	int i, j;
	
	/* set up code ROMs */
	memcpy(wms_code_rom, memory_region(REGION_USER1), memory_region_length(REGION_USER1));

	/* load the graphics ROMs -- quadruples */
	wms_gfx_rom = base = memory_region(REGION_GFX1);
	for (i = 0; i < memory_region_length(REGION_GFX1) / 0x400000; i++)
	{
		memcpy(wms_wolfu_decode_memory, base, 0x400000);
		for (j = 0; j < 0x100000; j++)
		{
			*base++ = wms_wolfu_decode_memory[0x000000 + j];
			*base++ = wms_wolfu_decode_memory[0x100000 + j];
			*base++ = wms_wolfu_decode_memory[0x200000 + j];
			*base++ = wms_wolfu_decode_memory[0x300000 + j];
		}
	}
	
	/* load sound ROMs and set up sound handlers */
	sound_type = sound;
	switch (sound)
	{
		case SOUND_DCS:
			break;
	}
}




/*************************************
 *
 *	Wolf-unit init (DCS)
 *
 * 	music: ADSP2101
 *
 *************************************/

/********************** Mortal Kombat 3 **********************/

static void init_mk3_common(void)
{
	/* common init */
	init_wolfu_generic(SOUND_DCS);

	/* serial prefixes 439, 528 */
	generate_serial(528);

	/* handle I/O & serial number */
	install_mem_read_handler (0, TOBYTE(0x187ff80), TOBYTE(0x187ffff), wms_wolfu_io_r);
	install_mem_write_handler(0, TOBYTE(0x187ff80), TOBYTE(0x187ffff), wms_wolfu_io_w);
} 

void init_mk3(void)
{
	init_mk3_common();
	INSTALL_SPEEDUP_3(0x1069bd0, 0xff926810, 0x105dc10, 0x105dc30, 0x105dc50);
}

void init_mk3r20(void)
{
	init_mk3_common();
	INSTALL_SPEEDUP_3(0x1069bd0, 0xff926790, 0x105dc10, 0x105dc30, 0x105dc50);
}

void init_mk3r10(void)
{
	init_mk3_common();
	INSTALL_SPEEDUP_3(0x1078e50, 0xff923e30, 0x105d490, 0x105d4b0, 0x105d4d0);
}

void init_umk3(void)
{
	init_mk3_common();
	INSTALL_SPEEDUP_3(0x106a0e0, 0xff9696a0, 0x105dc10, 0x105dc30, 0x105dc50);
}

void init_umk3r11(void)
{
	init_mk3_common();
	INSTALL_SPEEDUP_3(0x106a0e0, 0xff969680, 0x105dc10, 0x105dc30, 0x105dc50);
}


/********************** 2 On 2 Open Ice Challenge **********************/

void init_openice(void)
{
	/* common init */
	init_wolfu_generic(SOUND_DCS);

	/* serial prefixes 438, 528 */
	generate_serial(528);

	/* handle I/O & serial number */
	install_mem_read_handler (0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_r);
	install_mem_write_handler(0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_w);
}


/********************** NBA Maximum Hangtime **********************/

void init_nbamaxht(void)
{
	/* common init */
	init_wolfu_generic(SOUND_DCS);

	/* serial prefixes 459, 470, 528 */
	generate_serial(528);

	/* handle I/O & serial number */
	install_mem_read_handler (0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_r);
	install_mem_write_handler(0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_w);

	INSTALL_SPEEDUP_1_16BIT(0x10731f0, 0xff8a5510, 0x1002040, 0xd0, 0xb0);
}


/********************** WWF Wrestlemania **********************/

static READ_HANDLER( wms_generic_speedup_1_address )
{
	UINT16 value = READ_WORD(&wms_speedup_base[offset]);

	/* just return if this isn't the offset we're looking for */
	if (offset != wms_speedup_offset)
		return value;

	/* suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == wms_speedup_pc && !value)
		cpu_spinuntil_int();
	
	return value;
}

void init_wwfmania(void)
{
	/* common init */
	init_wolfu_generic(SOUND_DCS);

	/* serial prefixes 430, 528 */
	generate_serial(528);

	/* handle I/O & serial number */
	install_mem_read_handler (0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_r);
	install_mem_write_handler(0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_w);

	INSTALL_SPEEDUP_1_ADDRESS(0x105c250, 0xff8189d0);
}


/********************** Rampage World Tour **********************/

void init_rmpgwt(void)
{
	/* common init */
	init_wolfu_generic(SOUND_DCS);

	/* serial prefixes 465, 528 */
	generate_serial(528);

	/* handle I/O & serial number */
	install_mem_read_handler (0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_r);
	install_mem_write_handler(0, TOBYTE(0x1860000), TOBYTE(0x186007f), wms_wolfu_io_w);
}



/*************************************
 *
 *	Machine init
 *
 *************************************/

void wms_wolfu_init_machine(void)
{
	/* reset sound */
	switch (sound_type)
	{
		case SOUND_DCS:
			williams_dcs_init(1);
			break;
	}
}



/*************************************
 *
 *	Security chip I/O
 *
 *************************************/

READ_HANDLER( wms_wolfu_security_r )
{
	//logerror("%08X:Security R = %04X\n", cpu_get_pc(), security_buffer);
	security_status = 1;
	return security_buffer;
}


WRITE_HANDLER( wms_wolfu_security_w )
{
	//logerror("%08X:Security W = %04X\n", cpu_get_pc(), data);
	if (!(data & 0x00ff0000))
	{
		/* status seems to reflect the clock bit */
		security_status = (data >> 4) & 1;
		
		/* on the falling edge, clock the next data byte through */
		if (!(data & 0x10))
		{
			/* the self-test writes 1F, 0F, and expects to read an F in the low 4 bits */
			if (data & 0x0f)
				security_buffer = data;
			else
				security_buffer = security_data[security_index++ % sizeof(security_data)];
		}
	}
}



/*************************************
 *
 *	Sound write handlers
 *
 *************************************/

READ_HANDLER( wms_wolfu_sound_r )
{
	//logerror("%08X:Sound read\n", cpu_get_pc());

	if ( sound_type == SOUND_DCS && Machine->sample_rate )
		return williams_dcs_data_r(0);

	return 0x0000;
}

READ_HANDLER( wms_wolfu_sound_state_r )
{
	if ( sound_type == SOUND_DCS && Machine->sample_rate )
		return williams_dcs_control_r(0);

	return 0x0800;
}

WRITE_HANDLER( wms_wolfu_sound_w )
{
	/* check for out-of-bounds accesses */
	if (offset)
	{
		//logerror("%08X:Unexpected write to sound (hi) = %04X\n", cpu_get_pc(), data);
		return;
	}
	
	/* call through based on the sound type */
	switch (sound_type)
	{
		case SOUND_DCS:
			//logerror("%08X:Sound write = %04X\n", cpu_get_pc(), data);
			williams_dcs_data_w(0, data & 0xff);
			break;
	}
}
