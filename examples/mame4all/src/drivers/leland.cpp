#include "../vidhrdw/leland.cpp"
#include "../sndhrdw/leland.cpp"

/***************************************************************************

	Cinemat/Leland driver

	driver by Aaron Giles and Paul Leaman

	-------------------------------------

	To enter service mode in most games, press 1P start and then press
	the service switch (F2).

	For Redline Racer, hold the service switch down and reset the machine.

	For Super Offroad, press the blue nitro button (3P button 1) and then
	press the service switch.

	-------------------------------------

	Still to do:
		- memory map
		- generate fake serial numbers
		- kludge Quarterback sound
		- work around Alley Master crash if you enter no name
			and the battery ram is cleared to all 0

***************************************************************************/


#include "driver.h"

#include "vidhrdw/generic.h"
#include "machine/eeprom.h"

#include "cpu/z80/z80.h"


/* define these to 0 to disable, or to 1 to enable */
#define LOG_KEYCARDS		0
#define LOG_KEYCARDS_FULL	0
#define LOG_BANKSWITCHING_M	0
#define LOG_BANKSWITCHING_S	0
#define LOG_SOUNDPORT		0
#define LOG_EEPROM			0
#define LOG_BATTERY_RAM		0


/* Helps document the input ports. */
#define IPT_SLAVEHALT 	IPT_SPECIAL
#define IPT_EEPROM_DATA	IPT_SPECIAL
#define PORT_SERVICE_NO_TOGGLE(mask,default)	\
	PORT_BITX(    mask, mask & default, IPT_SERVICE1, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )


static UINT8 leland_gfx_control;
UINT8 leland_dac_control;

static UINT8 wcol_enable;

static void *master_int_timer;

static UINT8 *master_base;
static UINT8 *slave_base;
static UINT32 master_length;
static UINT32 slave_length;

static int dangerz_x, dangerz_y;
static UINT8 analog_result;
static UINT8 dial_last_input[4];
static UINT8 dial_last_result[4];

static UINT8 keycard_shift;
static UINT8 keycard_bit;
static UINT8 keycard_state;
static UINT8 keycard_clock;
static UINT8 keycard_command[3];

static UINT8 top_board_bank;
static UINT8 sound_port_bank;
static UINT8 alternate_bank;
static void (*update_master_bank)(void);

#define battery_ram_size 0x4000
static UINT8 battery_ram_enable;
static UINT8 *battery_ram;

static UINT8 eeprom_data[64*2];
static struct EEPROM_interface eeprom_interface =
{
	6,
	16,
	"110",
	"101",
	"111"
};


/* Sound routines */
WRITE_HANDLER( leland_i86_control_w );
WRITE_HANDLER( leland_i86_command_lo_w );
WRITE_HANDLER( leland_i86_command_hi_w );
READ_HANDLER( leland_i86_response_r );

int  leland_sh_start(const struct MachineSound *msound);
void leland_sh_stop(void);

int  leland_i186_sh_start(const struct MachineSound *msound);
int  redline_i186_sh_start(const struct MachineSound *msound);
void leland_i186_sound_init(void);

void leland_i86_optimize_address(offs_t offset);

extern struct MemoryReadAddress leland_i86_readmem[];
extern struct MemoryWriteAddress leland_i86_writemem[];
extern struct IOReadPort leland_i86_readport[];
extern struct IOWritePort redline_i86_writeport[];
extern struct IOWritePort leland_i86_writeport[];

/* Video routines */
READ_HANDLER( leland_mvram_port_r );
READ_HANDLER( leland_svram_port_r );
WRITE_HANDLER( leland_mvram_port_w );
WRITE_HANDLER( leland_svram_port_w );
WRITE_HANDLER( leland_master_video_addr_w );
WRITE_HANDLER( leland_slave_video_addr_w );

WRITE_HANDLER( leland_gfx_port_w );

void leland_vh_eof(void);
int leland_vh_start(void);
void leland_vh_stop(void);
void leland_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);

/* Internal routines */
static void interrupt_callback(int scanline);



/*************************************
 *
 *	Generic dial encoding
 *
 *************************************/

static int dial_compute_value(int new_val, int indx)
{
	int delta = new_val - (int)dial_last_input[indx];
	UINT8 result = dial_last_result[indx] & 0x80;

	dial_last_input[indx] = new_val;

	if (delta > 0x80)
		delta -= 0x100;
	else if (delta < -0x80)
		delta += 0x100;

	if (delta < 0)
	{
		result = 0x80;
		delta = -delta;
	}
	else if (delta > 0)
		result = 0x00;

	if (delta > 0x1f)
		delta = 0x1f;
	result |= (dial_last_result[indx] + delta) & 0x1f;

	dial_last_result[indx] = result;
	return result;
}



/*************************************
 *
 *	Cerberus inputs
 *
 *************************************/

static READ_HANDLER( cerberus_dial_1_r )
{
	int original = readinputport(0);
	int modified = dial_compute_value(readinputport(4), 0);
	return (original & 0xc0) | ((modified & 0x80) >> 2) | (modified & 0x1f);
}

static READ_HANDLER( cerberus_dial_2_r )
{
	int original = readinputport(2);
	int modified = dial_compute_value(readinputport(5), 1);
	return (original & 0xc0) | ((modified & 0x80) >> 2) | (modified & 0x1f);
}



/*************************************
 *
 *	Alley Master inputs
 *
 *************************************/

static UINT8 *alleymas_kludge_mem;

static WRITE_HANDLER( alleymas_joystick_kludge )
{
	/* catch the case where they clear this memory location at PC $1827 and change */
	/* the value written to be a 1 */
	if (cpu_getpreviouspc() == 0x1827)
		*alleymas_kludge_mem = 1;
	else
		*alleymas_kludge_mem = data;

	/* while we're here, make sure the first 3 characters in battery RAM are a */
	/* valid name; otherwise, it will crash if you start a game and don't enter */
	/* your name */
	if (battery_ram[0] == 0)
	{
		battery_ram[0] = 'C';
		battery_ram[1] = 'I';
		battery_ram[2] = 'N';
	}
}



/*************************************
 *
 *	Danger Zone inputs
 *
 *************************************/

static void update_dangerz_xy(void)
{
	UINT8 newy = readinputport(4);
	UINT8 newx = readinputport(5);
	int deltay = newy - dial_last_input[0];
	int deltax = newx - dial_last_input[1];

	if (deltay <= -128) deltay += 256;
	else if (deltay >= 128) deltay -= 256;
	if (deltax <= -128) deltax += 256;
	else if (deltax >= 128) deltax -= 256;

	dangerz_y += deltay;
	dangerz_x += deltax;
	if (dangerz_y < 0) dangerz_y = 0;
	else if (dangerz_y >= 1024) dangerz_y = 1023;
	if (dangerz_x < 0) dangerz_x = 0;
	else if (dangerz_x >= 1024) dangerz_x = 1023;

	dial_last_input[0] = newy;
	dial_last_input[1] = newx;
}

static READ_HANDLER( dangerz_input_y_r )
{
	update_dangerz_xy();
	return dangerz_y & 0xff;
}

static READ_HANDLER( dangerz_input_x_r )
{
	update_dangerz_xy();
	return dangerz_x & 0xff;
}

static READ_HANDLER( dangerz_input_upper_r )
{
	update_dangerz_xy();
	return ((dangerz_y >> 2) & 0xc0) | ((dangerz_x >> 8) & 0x03);
}



/*************************************
 *
 *	Red Line Racer inputs
 *
 *************************************/

static const UINT8 redline_pedal_value[8] = { 0xf0, 0xe0, 0xc0, 0xd0, 0x90, 0xb0, 0x30, 0x70 };

static READ_HANDLER( redline_pedal_1_r )
{
	int pedal = readinputport(0);
	return redline_pedal_value[pedal >> 5] | 0x0f;
}

static READ_HANDLER( redline_pedal_2_r )
{
	int pedal = readinputport(2);
	return redline_pedal_value[pedal >> 5] | 0x0f;
}

static READ_HANDLER( redline_wheel_1_r )
{
	return dial_compute_value(readinputport(4), 0);
}

static READ_HANDLER( redline_wheel_2_r )
{
	return dial_compute_value(readinputport(5), 1);
}



/*************************************
 *
 *	Super Offroad inputs
 *
 *************************************/

static READ_HANDLER( offroad_wheel_1_r )
{
	return dial_compute_value(readinputport(7), 0);
}

static READ_HANDLER( offroad_wheel_2_r )
{
	return dial_compute_value(readinputport(8), 1);
}

static READ_HANDLER( offroad_wheel_3_r )
{
	return dial_compute_value(readinputport(9), 2);
}



/*************************************
 *
 *	Machine initialization
 *
 *************************************/

static void init_machine(void)
{
	/* set the odd data bank */
	battery_ram = memory_region(REGION_USER2);

	/* start scanline interrupts going */
	master_int_timer = timer_set(cpu_getscanlinetime(8), 8, interrupt_callback);

	/* reset globals */
	leland_gfx_control = 0;
	wcol_enable = 0;

	dangerz_x = 512;
	dangerz_y = 512;
	analog_result = 0xff;
	memset(dial_last_input, 0, sizeof(dial_last_input));
	memset(dial_last_result, 0, sizeof(dial_last_result));

	keycard_shift = 0;
	keycard_bit = 0;
	keycard_state = 0;
	keycard_clock = 0;
	memset(keycard_command, 0, sizeof(keycard_command));

	top_board_bank = 0;
	sound_port_bank = 0;
	alternate_bank = 0;

	/* initialize the master banks */
	master_length = memory_region_length(REGION_CPU1);
	master_base = memory_region(REGION_CPU1);
	(*update_master_bank)();

	/* initialize the slave banks */
	slave_length = memory_region_length(REGION_CPU2);
	slave_base = memory_region(REGION_CPU2);
	if (slave_length > 0x10000)
		cpu_setbank(3, &slave_base[0x10000]);

	/* if we have an I86 CPU, reset it */
	if ((Machine->drv->cpu[2].cpu_type & ~CPU_FLAGS_MASK) == CPU_I186)
		leland_i186_sound_init();
}



/*************************************
 *
 *	Master CPU interrupt handling
 *
 *************************************/

static void interrupt_callback(int scanline)
{
	extern UINT8 leland_last_scanline_int;
	leland_last_scanline_int = scanline;

	/* interrupts generated on the VA10 line, which is every */
	/* 16 scanlines starting with scanline #8 */
	cpu_cause_interrupt(0, 0);

	/* set a timer for the next one */
	scanline += 16;
	if (scanline > 248)
		scanline = 8;
	master_int_timer = timer_set(cpu_getscanlinetime(scanline), scanline, interrupt_callback);
}


static int master_interrupt(void)
{
	/* check for coins here */
	if ((readinputport(1) & 0x0e) != 0x0e)
		cpu_set_nmi_line(0, ASSERT_LINE);

	/* generate an interrupt if requested */
	return ignore_interrupt();
}

/*************************************
 *
 *	Master CPU bankswitch handlers
 *
 *************************************/

static WRITE_HANDLER( master_alt_bankswitch_w )
{
	/* update any bankswitching */
	/*if (LOG_BANKSWITCHING_M)
		if ((alternate_bank ^ data) & 0x0f)
			logerror("%04X:alternate_bank = %02X\n", cpu_getpreviouspc(), data & 0x0f);*/
	alternate_bank = data & 15;
	(*update_master_bank)();

	/* sound control is in the rest */
	leland_i86_control_w(offset, data);
}


/* bankswitching for Cerberus */
static void cerberus_bankswitch(void)
{
	/* no bankswitching */
}


/* bankswitching for Mayhem 2002, World Series Baseball, and Alley Master */
static void mayhem_bankswitch(void)
{
	UINT8 *address;

	battery_ram_enable = ((sound_port_bank & 0x24) == 0);

	address = (!(sound_port_bank & 0x04)) ? &master_base[0x10000] : &master_base[0x1c000];
	cpu_setbank(1, address);

	address = battery_ram_enable ? battery_ram : &address[0x8000];
	cpu_setbank(2, address);
}


/* bankswitching for Danger Zone */
static void dangerz_bankswitch(void)
{
	UINT8 *address;

	battery_ram_enable = ((top_board_bank & 0x80) != 0);

	address = (!(alternate_bank & 1)) ? &master_base[0x02000] : &master_base[0x12000];
	cpu_setbank(1, address);

	address = battery_ram_enable ? battery_ram : &address[0x8000];
	cpu_setbank(2, address);
}


/* bankswitching for Baseball the Season II, Super Baseball, and Strike Zone */
static void basebal2_bankswitch(void)
{
	UINT8 *address;

	battery_ram_enable = (top_board_bank & 0x80);

	if (!battery_ram_enable)
		address = (!(sound_port_bank & 0x04)) ? &master_base[0x10000] : &master_base[0x1c000];
	else
		address = (!(top_board_bank & 0x40)) ? &master_base[0x28000] : &master_base[0x30000];
	cpu_setbank(1, address);

	address = battery_ram_enable ? battery_ram : &address[0x8000];
	cpu_setbank(2, address);
}


/* bankswitching for Red Line Racer */
static void redline_bankswitch(void)
{
	static const UINT32 bank_list[] = { 0x10000, 0x18000, 0x02000, 0x02000 };
	UINT8 *address;

	battery_ram_enable = ((alternate_bank & 3) == 1);

	address = &master_base[bank_list[alternate_bank & 3]];
	cpu_setbank(1, address);

	address = battery_ram_enable ? battery_ram : &master_base[0xa000];
	cpu_setbank(2, address);
}


/* bankswitching for Viper, Quarterback, Team Quarterback, and All American Football */
static void viper_bankswitch(void)
{
	static const UINT32 bank_list[] = { 0x02000, 0x10000, 0x18000, 0x02000 };
	UINT8 *address;

	battery_ram_enable = ((alternate_bank & 0x04) != 0);

	address = &master_base[bank_list[alternate_bank & 3]];
	if (bank_list[alternate_bank & 3] >= master_length)
	{
		//logerror("%04X:Master bank %02X out of range!\n", cpu_getpreviouspc(), alternate_bank & 3);
		address = &master_base[bank_list[0]];
	}
	cpu_setbank(1, address);

	address = battery_ram_enable ? battery_ram : &master_base[0xa000];
	cpu_setbank(2, address);
}


/* bankswitching for Super Offroad, Super Offroad Track Pack, and Pig Out */
static void offroad_bankswitch(void)
{
	static const UINT32 bank_list[] = { 0x02000, 0x02000, 0x10000, 0x18000, 0x20000, 0x28000, 0x30000, 0x38000 };
	UINT8 *address;

	battery_ram_enable = ((alternate_bank & 7) == 1);

	address = &master_base[bank_list[alternate_bank & 7]];
	if (bank_list[alternate_bank & 7] >= master_length)
	{
		//logerror("%04X:Master bank %02X out of range!\n", cpu_getpreviouspc(), alternate_bank & 7);
		address = &master_base[bank_list[0]];
	}
	cpu_setbank(1, address);

	address = battery_ram_enable ? battery_ram : &master_base[0xa000];
	cpu_setbank(2, address);
}



/*************************************
 *
 *	EEPROM handling (64 x 16bits)
 *
 *************************************/

#define SERIAL_TYPE_NONE		0
#define SERIAL_TYPE_ADD			1
#define SERIAL_TYPE_ENCRYPT		2
#define SERIAL_TYPE_ENCRYPT_XOR	3

static void init_eeprom(UINT8 default_val, const UINT16 *data, UINT8 serial_offset, UINT8 serial_type)
{
	UINT32 serial;

	/* initialize everything to the default value */
	memset(eeprom_data, default_val, sizeof(eeprom_data));

	/* fill in the preset data */
	while (*data != 0xffff)
	{
		int offset = *data++;
		int value = *data++;
		eeprom_data[offset * 2 + 0] = value >> 8;
		eeprom_data[offset * 2 + 1] = value & 0xff;
	}

	/* pick a serial number -- examples of real serial numbers:

		Team QB:      21101957
		AAFB:         26101119 and 26101039
	*/
	serial = 0x12345678;

	/* switch off the serial number type */
	switch (serial_type)
	{
		case SERIAL_TYPE_ADD:
		{
			int i;
			for (i = 0; i < 10; i++)
			{
				int digit;

				if (i >= 8)
					digit = 0;
				else
					digit = ((serial << (i * 4)) >> 28) & 15;
				digit = ('0' + digit) * 2;
				eeprom_data[serial_offset * 2 +  0 + (i ^ 1)] = digit / 3;
				eeprom_data[serial_offset * 2 + 10 + (i ^ 1)] = digit / 3;
				eeprom_data[serial_offset * 2 + 20 + (i ^ 1)] = digit - (2 * (digit / 3));
			}
			break;
		}

		case SERIAL_TYPE_ENCRYPT:
		case SERIAL_TYPE_ENCRYPT_XOR:
		{
			int d, e, h, l;

			/* break the serial number out into pieces */
			l = (serial >> 24) & 0xff;
			h = (serial >> 16) & 0xff;
			e = (serial >> 8) & 0xff;
			d = serial & 0xff;

			/* decrypt the data */
			h = ((h ^ 0x2a ^ l) ^ 0xff) + 5;
			d = ((d + 0x2a) ^ e) ^ 0xff;
			l ^= e;
			e ^= 0x2a;

			/* optionally XOR the result */
			if (serial_type == SERIAL_TYPE_ENCRYPT_XOR)
			{
				h ^= 0xff;
				l ^= 0xff;
				d ^= 0xff;
				e ^= 0xff;
			}

			/* store the bytes */
			eeprom_data[serial_offset * 2 + 0] = h;
			eeprom_data[serial_offset * 2 + 1] = l;
			eeprom_data[serial_offset * 2 + 2] = d;
			eeprom_data[serial_offset * 2 + 3] = e;
			break;
		}
	}

	EEPROM_init(&eeprom_interface);
}



/*************************************
 *
 *	Battery backed RAM
 *
 *************************************/

static WRITE_HANDLER( battery_ram_w )
{
	if (battery_ram_enable)
	{
		//if (LOG_BATTERY_RAM) logerror("%04X:BatteryW@%04X=%02X\n", cpu_getpreviouspc(), offset, data);
		battery_ram[offset] = data;
	}
	/*else
		logerror("%04X:BatteryW@%04X (invalid!)\n", cpu_getpreviouspc(), offset, data);*/
}


static void nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
	{
		EEPROM_save(file);
		osd_fwrite(file, memory_region(REGION_USER2), battery_ram_size);
	}
	else if (file)
	{
		EEPROM_load(file);
		osd_fread(file, memory_region(REGION_USER2), battery_ram_size);
	}
	else
	{
		EEPROM_set_data(eeprom_data, 64*2);
		memset(memory_region(REGION_USER2), 0x00, battery_ram_size);
	}
}



/*************************************
 *
 *	Master CPU keycard I/O
 *
 ************************************/

/*--------------------------------------------------------------------

  A note about keycards:

  These were apparently static programmable ROMs that could be
  inserted into certain games. The games would then save/load
  statistics on them.

  The data is accessed via a serial protocol, which is
  implemented below. There are two known commands that are
  written; each command is 3 bytes and accesses 128 bytes of
  data from the keycard:

  		62 00 80
  		9D 00 80

  the last byte appears to specify the length of data to transfer.

  The format of the data on the card is pretty heavily encrypted.
  The first 7 bytes read serves as a header:

  		D5 43 49 4E 2A xx yy

  where xx is a game-specific key, and yy is the complement of the
  key. For example, World Series Baseball uses 04/FB. Alley Master
  uses 05/FA.

  The last 112 bytes of data is encrypted via the following method:

		for (i = 16, b = 0x70, r = 0x08; i < 128; i++, b--, r += 0x10)
		{
			UINT8 a = original_data[i] ^ 0xff;
			a = (a >> 3) | (a << 5);
			a = (((a ^ r) + 1 + b) ^ b) - b;
			encrypted_data[i] = a;
		}

  The data that is encrypted is stored alternating with a checksum
  byte. The checksum for a value A is ((A ^ 0xa5) + 0x27) ^ 0x34.

--------------------------------------------------------------------*/

static int keycard_r(void)
{
	int result = 0;

	//if (LOG_KEYCARDS_FULL) logerror("  (%04X:keycard_r)\n", cpu_getpreviouspc());

	/* if we have a valid keycard read state, we're reading from the keycard */
	if (keycard_state & 0x80)
	{
		/* clock in new data */
		if (keycard_bit == 1)
		{
			keycard_shift = 0xff;	/* no data, but this is where we would clock it in */
			//if (LOG_KEYCARDS) logerror("  (clocked in %02X)\n", keycard_shift);
		}

		/* clock in the bit */
		result = (~keycard_shift & 1) << ((keycard_state >> 4) & 3);
		//if (LOG_KEYCARDS) logerror("  (read %02X)\n", result);
	}
	return result;
}

static void keycard_w(int data)
{
	int new_state = data & 0xb0;
	int new_clock = data & 0x40;

	//if (LOG_KEYCARDS_FULL) logerror("  (%04X:keycard_w=%02X)\n", cpu_getpreviouspc(), data);

	/* check for going active */
	if (!keycard_state && new_state)
	{
		keycard_command[0] = keycard_command[1] = keycard_command[2] = 0;
		//if (LOG_KEYCARDS) logerror("keycard going active (state=%02X)\n", new_state);
	}

	/* check for going inactive */
	else if (keycard_state && !new_state)
	{
		keycard_command[0] = keycard_command[1] = keycard_command[2] = 0;
		//if (LOG_KEYCARDS) logerror("keycard going inactive\n");
	}

	/* check for clocks */
	else if (keycard_state == new_state)
	{
		/* work off of falling edge */
		if (!new_clock && keycard_clock)
		{
			keycard_shift >>= 1;
			keycard_bit = (keycard_bit + 1) & 7;
		}

		/* look for a bit write */
		else if (!new_clock && !keycard_clock && !(data & 0x80))
		{
			//if (LOG_KEYCARDS) logerror("  (write %02X)\n", data);

			keycard_shift &= ~0x80;
			if (data & (1 << ((new_state >> 4) & 3)))
				keycard_shift |= 0x80;

			/* clock out the data on the last bit */
			if (keycard_bit == 7)
			{
				//if (LOG_KEYCARDS) logerror("  (clocked out %02X)\n", keycard_shift);
				keycard_command[0] = keycard_command[1];
				keycard_command[1] = keycard_command[2];
				keycard_command[2] = keycard_shift;
				if (keycard_command[0] == 0x62 && keycard_command[1] == 0x00 && keycard_command[2] == 0x80)
				{
					//if (LOG_KEYCARDS) logerror("  (got command $62)\n");
				}
			}
		}
	}

	/* error case */
	//else
	//{
		/* only an error if the selected bit changes; read/write transitions are okay */
		//if ((new_state & 0x30) != (keycard_state & 0x30))
			//if (LOG_KEYCARDS) logerror("ERROR: Caught keycard state transition %02X -> %02X\n", keycard_state, new_state);
	//}

	keycard_state = new_state;
	keycard_clock = new_clock;
}



/*************************************
 *
 *	Master CPU analog and keycard I/O
 *
 *************************************/

static READ_HANDLER( master_analog_key_r )
{
	int result = 0;

	switch (offset)
	{
		case 0x00:	/* FD = analog data read */
			result = analog_result;
			break;

		case 0x01:	/* FE = analog status read */
			/* bit 7 indicates the analog input is busy for some games */
			result = 0x00;
			break;

		case 0x02:	/* FF = keycard serial data read */
			result = keycard_r();

			/* bit 7 indicates the analog input is busy for some games */
			result &= ~0x80;
			break;
	}
	return result;
}



static WRITE_HANDLER( master_analog_key_w )
{
	switch (offset)
	{
		case 0x00:	/* FD = analog port trigger */
			break;

		case 0x01:	/* FE = analog port select/bankswitch */
			analog_result = readinputport((data & 15) + 4);

			/* update top board banking for some games */
			/*if (LOG_BANKSWITCHING_M)
				if ((top_board_bank ^ data) & 0xc0)
					logerror("%04X:top_board_bank = %02X\n", cpu_getpreviouspc(), data & 0xc0);*/
			top_board_bank = data & 0xc0;
			(*update_master_bank)();
			break;

		case 0x02:	/* FF = keycard data write */
			keycard_w(data);
			break;
	}
}



/*************************************
 *
 *	Master CPU internal I/O
 *
 *************************************/

static READ_HANDLER( master_input_r )
{
	int result = 0xff;

	switch (offset)
	{
		case 0x00:	/* /GIN0 */
			result = readinputport(0);
			break;

		case 0x01:	/* /GIN1 */
			result = readinputport(1);
			if (cpunum_get_reg(1, Z80_HALT))
				result ^= 0x01;
			break;

		case 0x02:	/* /GIN2 */
		case 0x12:
			cpu_set_nmi_line(0, CLEAR_LINE);
			break;

		case 0x03:	/* /IGID */
		case 0x13:
			result = AY8910_read_port_0_r(offset);
			break;

		case 0x10:	/* /GIN0 */
			result = readinputport(2);
			break;

		case 0x11:	/* /GIN1 */
			result = readinputport(3);
			//if (LOG_EEPROM) logerror("%04X:EE read\n", cpu_getpreviouspc());
			result = (result & ~0x01) | EEPROM_read_bit();
			break;

		default:
			//logerror("Master I/O read offset %02X\n", offset);
			break;
	}
	return result;
}


static WRITE_HANDLER( master_output_w )
{
	switch (offset)
	{
		case 0x09:	/* /MCONT */
			cpu_set_reset_line(1,    (data & 0x01) ? CLEAR_LINE : ASSERT_LINE);
			wcol_enable = (data & 0x02);
			cpu_set_nmi_line  (1,    (data & 0x04) ? CLEAR_LINE : ASSERT_LINE);
			cpu_set_irq_line  (1, 0, (data & 0x08) ? CLEAR_LINE : ASSERT_LINE);

			/*if (LOG_EEPROM) logerror("%04X:EE write %d%d%d\n", cpu_getpreviouspc(),
					(data >> 6) & 1, (data >> 5) & 1, (data >> 4) & 1);*/
			EEPROM_write_bit     ((data & 0x10) >> 4);
			EEPROM_set_clock_line((data & 0x20) ? ASSERT_LINE : CLEAR_LINE);
			EEPROM_set_cs_line  ((~data & 0x40) ? ASSERT_LINE : CLEAR_LINE);
			break;

		case 0x0a:	/* /OGIA */
			AY8910_control_port_0_w(0, data);
			break;

		case 0x0b:	/* /OGID */
			AY8910_write_port_0_w(0, data);
			break;

		case 0x0c:	/* /BKXL */
		case 0x0d:	/* /BKXH */
		case 0x0e:	/* /BKYL */
		case 0x0f:	/* /BKYH */
			leland_gfx_port_w(offset - 0x0c, data);
			break;

		default:
			//logerror("Master I/O write offset %02X=%02X\n", offset, data);
			break;
	}
}



/*************************************
 *
 *	Master CPU palette gates
 *
 *************************************/

static WRITE_HANDLER( gated_paletteram_w )
{
	if (wcol_enable)
		paletteram_BBGGGRRR_w(offset, data);
}


static READ_HANDLER( gated_paletteram_r )
{
	if (wcol_enable)
		return paletteram_r(offset);
	return 0xff;
}



/*************************************
 *
 *	AY8910-controlled graphics latch
 *
 *************************************/

static READ_HANDLER( sound_port_r )
{
    return leland_gfx_control;
}


static WRITE_HANDLER( sound_port_w )
{
	int gfx_banks = Machine->gfx[0]->total_elements / 0x400;
	int gfx_bank_mask = (gfx_banks - 1) << 4;
	int diff = data ^ leland_gfx_control;

    /* update the graphics banking if necessary */
    if ((diff & 0x08) || (diff & gfx_bank_mask))
    	leland_gfx_port_w(-1, data);

	/* set the new value */
    leland_gfx_control = data;
	leland_dac_control &= data & 3;

    /* some bankswitching occurs here */
	/*if (LOG_BANKSWITCHING_M)
		if ((sound_port_bank ^ data) & 0x24)
			logerror("%04X:sound_port_bank = %02X\n", cpu_getpreviouspc(), data & 0x24);*/
    sound_port_bank = data & 0x24;
    (*update_master_bank)();
}



/*************************************
 *
 *	Slave CPU bankswitching
 *
 *************************************/

static WRITE_HANDLER( slave_small_banksw_w )
{
	int bankaddress = 0x10000 + 0xc000 * (data & 1);

	if (bankaddress >= slave_length)
	{
		//logerror("%04X:Slave bank %02X out of range!", cpu_getpreviouspc(), data & 1);
		bankaddress = 0x10000;
	}
	cpu_setbank(3, &slave_base[bankaddress]);

	//if (LOG_BANKSWITCHING_S) logerror("%04X:Slave bank = %02X (%05X)\n", cpu_getpreviouspc(), data & 1, bankaddress);
}


static WRITE_HANDLER( slave_large_banksw_w )
{
	int bankaddress = 0x10000 + 0x8000 * (data & 15);

	if (bankaddress >= slave_length)
	{
		//logerror("%04X:Slave bank %02X out of range!", cpu_getpreviouspc(), data & 15);
		bankaddress = 0x10000;
	}
	cpu_setbank(3, &slave_base[bankaddress]);

	//if (LOG_BANKSWITCHING_S) logerror("%04X:Slave bank = %02X (%05X)\n", cpu_getpreviouspc(), data & 15, bankaddress);
}



/*************************************
 *
 *	Slave CPU I/O
 *
 *************************************/

static READ_HANDLER( raster_r )
{
	int scanline = cpu_getscanline();
	return (scanline < 255) ? scanline : 255;
}



/*************************************
 *
 *	Master CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress master_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x9fff, MRA_BANK1 },
	{ 0xa000, 0xdfff, MRA_BANK2 },
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0xf000, 0xf3ff, gated_paletteram_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress master_writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xdfff, battery_ram_w },
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf000, 0xf3ff, gated_paletteram_w, &paletteram },
	{ 0xf800, 0xf801, leland_master_video_addr_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort master_readport[] =
{
    { 0xf2, 0xf2, leland_i86_response_r },
    { 0xfd, 0xff, master_analog_key_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort master_writeport[] =
{
	{ 0xf0, 0xf0, master_alt_bankswitch_w },
	{ 0xf2, 0xf2, leland_i86_command_lo_w },
	{ 0xf4, 0xf4, leland_i86_command_hi_w },
    { 0xfd, 0xff, master_analog_key_w },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Slave CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress slave_small_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0xdfff, MRA_BANK3 },
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0xf802, 0xf802, raster_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress slave_small_writemem[] =
{
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf800, 0xf801, leland_slave_video_addr_w },
	{ 0xf803, 0xf803, slave_small_banksw_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress slave_large_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0xbfff, MRA_BANK3 },
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0xf802, 0xf802, raster_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress slave_large_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc000, slave_large_banksw_w },
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf800, 0xf801, leland_slave_video_addr_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort slave_readport[] =
{
	{ 0x00, 0x1f, leland_svram_port_r },
	{ 0x40, 0x5f, leland_svram_port_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort slave_writeport[] =
{
	{ 0x00, 0x1f, leland_svram_port_w },
	{ 0x40, 0x5f, leland_svram_port_w },
	{ -1 }  /* end of table */
};


/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( cerberus )		/* complete, verified from code */
	PORT_START      /* 0x80 */
    PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_SPECIAL | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )

	PORT_START      /* 0x81 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x90 */
    PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_SPECIAL | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )

	PORT_START      /* 0x91 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 | IPF_PLAYER1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog joystick 1 */
    PORT_ANALOG( 0xff, 0, IPT_DIAL | IPF_PLAYER1, 50, 10, 0, 0 )
	PORT_START
    PORT_ANALOG( 0xff, 0, IPT_DIAL | IPF_PLAYER2, 50, 10, 0, 0 )
	PORT_START      /* Analog joystick 2 */
	PORT_START
	PORT_START      /* Analog joystick 3 */
	PORT_START
INPUT_PORTS_END


INPUT_PORTS_START( mayhem )		/* complete, verified from code */
	PORT_START      /* 0xC0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )

	PORT_START      /* 0xC1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0xD0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )

	PORT_START      /* 0xD1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog joystick 1 */
	PORT_START
	PORT_START      /* Analog joystick 2 */
	PORT_START
	PORT_START      /* Analog joystick 3 */
	PORT_START
INPUT_PORTS_END


INPUT_PORTS_START( wseries )		/* complete, verified from code */
	PORT_START      /* 0x80 */
	PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1, "Extra Base", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, "Go Back", IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START      /* 0x81 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x90 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1, "Aim", IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START      /* 0x91 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 | IPF_PLAYER1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog joystick 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog joystick 2 */
	PORT_START
	PORT_START      /* Analog joystick 3 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER2, 100, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER2, 100, 10, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( alleymas )		/* complete, verified from code */
	PORT_START      /* 0xC0 */
	PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* 0xC1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0xD0 */
	PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )		/* redundant inputs */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )		/* redundant inputs */

	PORT_START      /* 0xD1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 | IPF_PLAYER1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog joystick 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0, 224 )
	PORT_START      /* Analog joystick 2 */
	PORT_START
	PORT_START      /* Analog joystick 3 */
	PORT_START
INPUT_PORTS_END


INPUT_PORTS_START( dangerz )		/* complete, verified from code */
	PORT_START      /* 0x80 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x81 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x90 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x91 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog 1 */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog 2 */
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1, 100, 10, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( basebal2 )		/* complete, verified from code */
	PORT_START      /* 0x40/C0 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x30, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* read by strkzone, but never referenced */
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1, "Extra Base", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, "Go Back", IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START      /* 0x41/C1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x50/D0 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1, "R Run/Steal", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1, "L Run/Steal", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1, "Run/Aim", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1, "Run/Cutoff", IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START      /* 0x51/D1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 | IPF_PLAYER1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog joystick 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog joystick 2 */
	PORT_START
	PORT_START      /* Analog joystick 3 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER2, 100, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER2, 100, 10, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( redline )		/* complete, verified in code */
	PORT_START      /* 0xC0 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_ANALOG( 0xe0, 0xe0, IPT_PEDAL | IPF_PLAYER1, 100, 64, 0x00, 0xff )

	PORT_START      /* 0xC1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* used, but for what purpose? */

	PORT_START      /* 0xD0 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_ANALOG( 0xe0, 0xe0, IPT_PEDAL | IPF_PLAYER2, 100, 64, 0x00, 0xff )

	PORT_START      /* 0xD1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog wheel 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_DIAL | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog wheel 2 */
	PORT_ANALOG( 0xff, 0x80, IPT_DIAL | IPF_PLAYER2, 100, 10, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( quarterb )		/* complete, verified in code */
	PORT_START      /* 0x80 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x81 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x90 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START      /* 0x91 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog spring stick 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 2 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 3 */
	PORT_START      /* Analog spring stick 4 */
	PORT_START      /* Analog spring stick 5 */
	PORT_START      /* Analog spring stick 6 */
INPUT_PORTS_END


INPUT_PORTS_START( teamqb )		/* complete, verified in code */
	PORT_START      /* 0x80 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x81 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x90 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START      /* 0x91 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog spring stick 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 2 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 3 */
	PORT_START      /* Analog spring stick 4 */
	PORT_START      /* Analog spring stick 5 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER3, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 6 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER3, 100, 10, 0, 255 )

	PORT_START      /* 0x7C */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START      /* 0x7F */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 )
INPUT_PORTS_END


INPUT_PORTS_START( aafb2p )		/* complete, verified in code */
	PORT_START      /* 0x80 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x81 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x90 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START      /* 0x91 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog spring stick 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 2 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 3 */
	PORT_START      /* Analog spring stick 4 */
	PORT_START      /* Analog spring stick 5 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER2, 100, 10, 0, 255 )
	PORT_START      /* Analog spring stick 6 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER2, 100, 10, 0, 255 )

	PORT_START      /* 0x7C */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x7F */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
INPUT_PORTS_END


INPUT_PORTS_START( offroad )		/* complete, verified from code */
	PORT_START      /* 0xC0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* read */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* read */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* read */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0xC1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0xD0 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0xD1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE_NO_TOGGLE( 0x08, IP_ACTIVE_LOW )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* Analog pedal 1 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog pedal 2 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER2, 100, 10, 0, 255 )
	PORT_START      /* Analog pedal 3 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER3, 100, 10, 0, 255 )
	PORT_START      /* Analog wheel 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_DIAL | IPF_PLAYER1, 100, 10, 0, 255 )
	PORT_START      /* Analog wheel 2 */
	PORT_ANALOG( 0xff, 0x80, IPT_DIAL | IPF_PLAYER2, 100, 10, 0, 255 )
	PORT_START      /* Analog wheel 3 */
	PORT_ANALOG( 0xff, 0x80, IPT_DIAL | IPF_PLAYER3, 100, 10, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( pigout )		/* complete, verified from code */
	PORT_START      /* 0x40 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP   | IPF_8WAY | IPF_PLAYER2 )

	PORT_START      /* 0x41 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SLAVEHALT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* read, but never referenced */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x50 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP   | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START      /* 0x51 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_EEPROM_DATA )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* 0x7F */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout bklayout =
{
	8,8,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(0,3), RGN_FRAC(1,3), RGN_FRAC(2,3) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &bklayout, 0, 8 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

/*
   2 AY8910 chips - Actually, one of these is an 8912
   (8910 with only 1 output port)

   Port A of both chips is connected to a banking control
   register.
*/

static struct AY8910interface ay8910_interface =
{
	2,
	10000000/6, /* 1.666 MHz */
	{ 25, 25 },
    { sound_port_r, sound_port_r },
	{ 0 },
    { sound_port_w, sound_port_w },
	{ 0 }
};

static struct CustomSound_interface dac_custom_interface =
{
    leland_sh_start,
    leland_sh_stop
};

static struct CustomSound_interface i186_custom_interface =
{
    leland_i186_sh_start
};

static struct CustomSound_interface redline_custom_interface =
{
  	redline_i186_sh_start
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_leland =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,
			master_readmem,master_writemem,
			master_readport,master_writeport,
			master_interrupt,1
		},
		{
			CPU_Z80,
			6000000,
			slave_small_readmem,slave_small_writemem,
			slave_readport,slave_writeport,
	    	ignore_interrupt,1
		}
	},
	60, (1000000*16)/(256*60),
	1,
	init_machine,

	/* video hardware */
	40*8, 30*8, { 0*8, 40*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	leland_vh_eof,
	leland_vh_start,
	leland_vh_stop,
	leland_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_AY8910, &ay8910_interface },
		{ SOUND_CUSTOM, &dac_custom_interface }
	},
	nvram_handler
};


static struct MachineDriver machine_driver_redline =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,
			master_readmem,master_writemem,
			master_readport,master_writeport,
			master_interrupt,1
		},
		{
			CPU_Z80,
			6000000,
			slave_small_readmem,slave_small_writemem,
			slave_readport,slave_writeport,
	    	ignore_interrupt,1
		},
		{
	    	CPU_I186 | CPU_AUDIO_CPU,
			16000000/2,
			leland_i86_readmem,leland_i86_writemem,
			leland_i86_readport,redline_i86_writeport,
			ignore_interrupt,1
		}
	},
	60, (1000000*16)/(256*60),
	1,
	init_machine,

	/* video hardware */
	40*8, 30*8, { 0*8, 40*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	leland_vh_eof,
	leland_vh_start,
	leland_vh_stop,
	leland_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_AY8910, &ay8910_interface },
		{ SOUND_CUSTOM, &redline_custom_interface }
	},
	nvram_handler
};


static struct MachineDriver machine_driver_quarterb =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,
			master_readmem,master_writemem,
			master_readport,master_writeport,
			master_interrupt,1
		},
		{
			CPU_Z80,
			6000000,
			slave_small_readmem,slave_small_writemem,
			slave_readport,slave_writeport,
	    	ignore_interrupt,1
		},
		{
	    	CPU_I186 | CPU_AUDIO_CPU,
			16000000/2,
			leland_i86_readmem,leland_i86_writemem,
			leland_i86_readport,leland_i86_writeport,
			ignore_interrupt,1
		}
	},
	60, (1000000*16)/(256*60),
	1,
	init_machine,

	/* video hardware */
	40*8, 30*8, { 0*8, 40*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	leland_vh_eof,
	leland_vh_start,
	leland_vh_stop,
	leland_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_AY8910, &ay8910_interface },
		{ SOUND_CUSTOM, &i186_custom_interface }
	},
	nvram_handler
};


static struct MachineDriver machine_driver_lelandi =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,
			master_readmem,master_writemem,
			master_readport,master_writeport,
			master_interrupt,1
		},
		{
			CPU_Z80,
			6000000,
			slave_large_readmem,slave_large_writemem,
			slave_readport,slave_writeport,
	    	ignore_interrupt,1
		},
		{
	    	CPU_I186 | CPU_AUDIO_CPU,
			16000000/2,
			leland_i86_readmem,leland_i86_writemem,
			leland_i86_readport,leland_i86_writeport,
			ignore_interrupt,1
		}
	},
	60, (1000000*16)/(256*60),
	1,
	init_machine,

	/* video hardware */
	40*8, 30*8, { 0*8, 40*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	1024,1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	leland_vh_eof,
	leland_vh_start,
	leland_vh_stop,
	leland_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_AY8910, &ay8910_interface },
		{ SOUND_CUSTOM, &i186_custom_interface }
	},
	nvram_handler
};



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( cerberus )
    ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "3-23u101", 0x00000, 0x02000, 0xd78210df )
	ROM_LOAD( "3-23u102", 0x02000, 0x02000, 0xeed121ef )
	ROM_LOAD( "3-23u103", 0x04000, 0x02000, 0x45b82bf7 )
	ROM_LOAD( "3-23u104", 0x06000, 0x02000, 0xe133d6bf )
	ROM_LOAD( "3-23u105", 0x08000, 0x02000, 0xa12c2c79 )
	ROM_LOAD( "3-23u106", 0x0a000, 0x02000, 0xd64110d2 )
	ROM_LOAD( "3-23u107", 0x0c000, 0x02000, 0x24e41c34 )

    ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "3-23u3",  0x00000, 0x02000, 0xb0579138 )
	ROM_LOAD( "3-23u4",  0x02000, 0x02000, 0xba0dc990 )
	ROM_LOAD( "3-23u5",  0x04000, 0x02000, 0xf8d6cc5d )
	ROM_LOAD( "3-23u6",  0x06000, 0x02000, 0x42cdd393 )
	ROM_LOAD( "3-23u7",  0x08000, 0x02000, 0xc020148a )
	ROM_LOAD( "3-23u8",  0x0a000, 0x02000, 0xdbabdbde )
	ROM_LOAD( "3-23u9",  0x0c000, 0x02000, 0xeb992385 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "3-23u93", 0x00000, 0x02000, 0x14a1a4b0 )
	ROM_LOAD( "3-23u94", 0x02000, 0x02000, 0x207a1709 )
	ROM_LOAD( "3-23u95", 0x04000, 0x02000, 0xe9c86267 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "3-23u70",  0x02000, 0x2000, 0x96499983 )
	ROM_LOAD( "3-23_u92", 0x06000, 0x2000, 0x497bb717 )
	ROM_LOAD( "3-23u69",  0x0a000, 0x2000, 0xebd14d9e )
	ROM_LOAD( "3-23u91",  0x0e000, 0x2000, 0xb592d2e5 )
	ROM_LOAD( "3-23u68",  0x12000, 0x2000, 0xcfa7b8bf )
	ROM_LOAD( "3-23u90",  0x16000, 0x2000, 0xb7566f8a )
	ROM_LOAD( "3-23u67",  0x1a000, 0x2000, 0x02b079a8 )
	ROM_LOAD( "3-23u89",  0x1e000, 0x2000, 0x7e5e82bb )

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( mayhem )
	ROM_REGION( 0x28000, REGION_CPU1 )
	ROM_LOAD( "13208.101",   0x00000, 0x04000, 0x04306973 )
	ROM_LOAD( "13215.102",   0x10000, 0x02000, 0x06e689ae )
	ROM_CONTINUE(            0x1c000, 0x02000 )
	ROM_LOAD( "13216.103",   0x12000, 0x02000, 0x6452a82c )
	ROM_CONTINUE(            0x1e000, 0x02000 )
	ROM_LOAD( "13217.104",   0x14000, 0x02000, 0x62f6036e )
	ROM_CONTINUE(            0x20000, 0x02000 )
	ROM_LOAD( "13218.105",   0x16000, 0x02000, 0x162f5eb1 )
	ROM_CONTINUE(            0x22000, 0x02000 )
	ROM_LOAD( "13219.106",   0x18000, 0x02000, 0xc0a74d6f )
	ROM_CONTINUE(            0x24000, 0x02000 )

	ROM_REGION( 0x28000, REGION_CPU2 )
	ROM_LOAD( "13207.3",  0x00000, 0x04000, 0xbe1df6aa ) /* DO NOT TRIM THIS ROM */
	ROM_LOAD( "13209.4",  0x10000, 0x02000, 0x39fcd7c6 )
	ROM_CONTINUE(         0x1c000, 0x02000 )
	ROM_LOAD( "13210.5",  0x12000, 0x02000, 0x630ed136 )
	ROM_CONTINUE(         0x1e000, 0x02000 )
	ROM_LOAD( "13211.6",  0x14000, 0x02000, 0x28b4aecd )
	ROM_CONTINUE(         0x20000, 0x02000 )
	ROM_LOAD( "13212.7",  0x16000, 0x02000, 0x1d6b39ab )
	ROM_CONTINUE(         0x22000, 0x02000 )
	ROM_LOAD( "13213.8",  0x18000, 0x02000, 0xf3b2ea05 )
	ROM_CONTINUE(         0x24000, 0x02000 )
	ROM_LOAD( "13214.9",  0x1a000, 0x02000, 0x96f3e8d9 )
	ROM_CONTINUE(         0x26000, 0x02000 )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "13204.93", 0x00000, 0x04000, 0xde183518 )
	ROM_LOAD( "13205.94", 0x04000, 0x04000, 0xc61f63ac )
	ROM_LOAD( "13206.95", 0x08000, 0x04000, 0x8e7bd2fd )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* U70 = Empty */
	ROM_LOAD( "13203.92",  0x04000, 0x4000, 0x121ed5bf )
	ROM_LOAD( "13201.69",  0x08000, 0x4000, 0x90283e29 )
	/* U91 = Empty */
	/* U68 = Empty */
	/* U90 = Empty */
	/* U67 = Empty */
	ROM_LOAD( "13202.89",  0x1c000, 0x4000, 0xc5eaa4e3 )

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( wseries )
	ROM_REGION( 0x28000, REGION_CPU1 )
	ROM_LOAD( "13409-01.101",   0x00000, 0x02000, 0xb5eccf5c )
	ROM_LOAD( "13410-01.102",   0x10000, 0x02000, 0xdd1ec091 )
	ROM_CONTINUE(               0x1c000, 0x02000 )
	ROM_LOAD( "13411-01.103",   0x12000, 0x02000, 0xec867a0e )
	ROM_CONTINUE(               0x1e000, 0x02000 )
	ROM_LOAD( "13412-01.104",   0x14000, 0x02000, 0x2977956d )
	ROM_CONTINUE(               0x20000, 0x02000 )
	ROM_LOAD( "13413-01.105",   0x16000, 0x02000, 0x569468a6 )
	ROM_CONTINUE(               0x22000, 0x02000 )
	ROM_LOAD( "13414-01.106",   0x18000, 0x02000, 0xb178632d )
	ROM_CONTINUE(               0x24000, 0x02000 )
	ROM_LOAD( "13415-01.107",   0x1a000, 0x02000, 0x20b92eff )
	ROM_CONTINUE(               0x26000, 0x02000 )

	ROM_REGION( 0x28000, REGION_CPU2 )
	ROM_LOAD( "13416-00.u3",  0x00000, 0x02000, 0x37c960cf )
	ROM_LOAD( "13417-00.u4",  0x10000, 0x02000, 0x97f044b5 )
	ROM_CONTINUE(             0x1c000, 0x02000 )
	ROM_LOAD( "13418-00.u5",  0x12000, 0x02000, 0x0931cfc0 )
	ROM_CONTINUE(             0x1e000, 0x02000 )
	ROM_LOAD( "13419-00.u6",  0x14000, 0x02000, 0xa7962b5a )
	ROM_CONTINUE(             0x20000, 0x02000 )
	ROM_LOAD( "13420-00.u7",  0x16000, 0x02000, 0x3c275262 )
	ROM_CONTINUE(             0x22000, 0x02000 )
	ROM_LOAD( "13421-00.u8",  0x18000, 0x02000, 0x86f57c80 )
	ROM_CONTINUE(             0x24000, 0x02000 )
	ROM_LOAD( "13422-00.u9",  0x1a000, 0x02000, 0x222e8405 )
	ROM_CONTINUE(             0x26000, 0x02000 )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "13401-00.u93", 0x00000, 0x04000, 0x4ea3e641 )
	ROM_LOAD( "13402-00.u94", 0x04000, 0x04000, 0x71a8a56c )
	ROM_LOAD( "13403-00.u95", 0x08000, 0x04000, 0x8077ae25 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* U70 = Empty */
	ROM_LOAD( "13404-00.u92",  0x04000, 0x4000, 0x22da40aa )
	ROM_LOAD( "13405-00.u69",  0x08000, 0x4000, 0x6f65b313 )
	/* U91 = Empty */
	ROM_LOAD( "13406-00.u68",  0x12000, 0x2000, 0xbb568693 )
	ROM_LOAD( "13407-00.u90",  0x14000, 0x4000, 0xe46ca57f )
	ROM_LOAD( "13408-00.u67",  0x18000, 0x4000, 0xbe637305 )
	/* 89 = Empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( alleymas )
	ROM_REGION( 0x28000, REGION_CPU1 )
	ROM_LOAD( "101",   0x00000, 0x02000, 0x4273e260 )
	ROM_LOAD( "102",   0x10000, 0x02000, 0xeb6575aa )
	ROM_CONTINUE(      0x1c000, 0x02000 )
	ROM_LOAD( "103",   0x12000, 0x02000, 0xcc9d778c )
	ROM_CONTINUE(      0x1e000, 0x02000 )
	ROM_LOAD( "104",   0x14000, 0x02000, 0x8edb129b )
	ROM_CONTINUE(      0x20000, 0x02000 )
	ROM_LOAD( "105",   0x16000, 0x02000, 0xa342dc8e )
	ROM_CONTINUE(      0x22000, 0x02000 )
	ROM_LOAD( "106",   0x18000, 0x02000, 0xb396c254 )
	ROM_CONTINUE(      0x24000, 0x02000 )
	ROM_LOAD( "107",   0x1a000, 0x02000, 0x3ca13e8c )
	ROM_CONTINUE(      0x26000, 0x02000 )

	ROM_REGION( 0x28000, REGION_CPU2 )
	ROM_LOAD( "003",  0x00000, 0x02000, 0x3fee63ae )
	ROM_LOAD( "004",  0x10000, 0x02000, 0xd302b5d1 )
	ROM_CONTINUE(     0x1c000, 0x02000 )
	ROM_LOAD( "005",  0x12000, 0x02000, 0x79bdb24d )
	ROM_CONTINUE(     0x1e000, 0x02000 )
	ROM_LOAD( "006",  0x14000, 0x02000, 0xf0b15d68 )
	ROM_CONTINUE(     0x20000, 0x02000 )
	ROM_LOAD( "007",  0x16000, 0x02000, 0x6974036c )
	ROM_CONTINUE(     0x22000, 0x02000 )
	ROM_LOAD( "008",  0x18000, 0x02000, 0xa4357b5a )
	ROM_CONTINUE(     0x24000, 0x02000 )
	ROM_LOAD( "009",  0x1a000, 0x02000, 0x6d74274e )
	ROM_CONTINUE(     0x26000, 0x02000 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "093", 0x00000, 0x02000, 0x54456e6f )
	ROM_LOAD( "094", 0x02000, 0x02000, 0xedc240da )
	ROM_LOAD( "095", 0x04000, 0x02000, 0x19793ed0 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* U70 = Empty */
	ROM_LOAD( "092",  0x04000, 0x2000, 0xa020eab5 )
	ROM_LOAD( "069",  0x08000, 0x2000, 0x79abb979 )
	/* U91 = Empty */
	ROM_LOAD( "068",  0x10000, 0x2000, 0x0c583385 )
	ROM_LOAD( "090",  0x14000, 0x2000, 0x0e1769e3 )
	/* U67 = Empty */
	/* U89 = Empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( dangerz )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "13823.12t",   0x00000, 0x10000, 0x31604634 )
	ROM_LOAD( "13824.13t",   0x10000, 0x10000, 0x381026c6 )

	ROM_REGION( 0x28000, REGION_CPU2 )
	ROM_LOAD( "13818.3",   0x00000, 0x04000, 0x71863c5b )
	ROM_LOAD( "13817.4",   0x10000, 0x02000, 0x924bead3 )
	ROM_CONTINUE(          0x1c000, 0x02000 )
	ROM_LOAD( "13818.5",   0x12000, 0x02000, 0x403bdfea )
	ROM_CONTINUE(          0x1e000, 0x02000 )
	ROM_LOAD( "13819.6",   0x14000, 0x02000, 0x1fee5f10 )
	ROM_CONTINUE(          0x20000, 0x02000 )
	ROM_LOAD( "13820.7",   0x16000, 0x02000, 0x42657a1e )
	ROM_CONTINUE(          0x22000, 0x02000 )
	ROM_LOAD( "13821.8",   0x18000, 0x02000, 0x92f3e006 )
	ROM_CONTINUE(          0x24000, 0x02000 )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "13801.93", 0x00000, 0x04000, 0xf9ff55ec )
	ROM_LOAD( "13802.94", 0x04000, 0x04000, 0xd4adbcbb )
	ROM_LOAD( "13803.95", 0x08000, 0x04000, 0x9178ed76 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "13809.70",  0x00000, 0x4000, 0xe44eb9f5 )
	ROM_LOAD( "13804.92",  0x04000, 0x4000, 0x6c23f1a5 )
	ROM_LOAD( "13805.69",  0x08000, 0x4000, 0xe9c9f38b )
	ROM_LOAD( "13808.91",  0x0c000, 0x4000, 0x035534ad )
	ROM_LOAD( "13806.68",  0x10000, 0x4000, 0x2dbd64d2 )
	ROM_LOAD( "13808.90",  0x14000, 0x4000, 0xd5b4985d )
	ROM_LOAD( "13822.67",  0x18000, 0x4000, 0x00ff3033 )
	ROM_LOAD( "13810.89",  0x1c000, 0x4000, 0x4f645973 )

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( basebal2 )
	ROM_REGION( 0x38000, REGION_CPU1 )
	ROM_LOAD( "14115-00.101",   0x00000, 0x02000, 0x05231fee )
	ROM_LOAD( "14116-00.102",   0x10000, 0x02000, 0xe1482ea3 )
	ROM_CONTINUE(               0x1c000, 0x02000 )
	ROM_LOAD( "14117-01.103",   0x12000, 0x02000, 0x677181dd )
	ROM_CONTINUE(               0x1e000, 0x02000 )
	ROM_LOAD( "14118-01.104",   0x14000, 0x02000, 0x5f570264 )
	ROM_CONTINUE(               0x20000, 0x02000 )
	ROM_LOAD( "14119-01.105",   0x16000, 0x02000, 0x90822145 )
	ROM_CONTINUE(               0x22000, 0x02000 )
	ROM_LOAD( "14120-00.106",   0x18000, 0x02000, 0x4d2b7217 )
	ROM_CONTINUE(               0x24000, 0x02000 )
	ROM_LOAD( "14121-01.107",   0x1a000, 0x02000, 0xb987b97c )
	ROM_CONTINUE(               0x26000, 0x02000 )
	/* Extra banks ( referred to as the "top" board). Probably an add-on */
	ROM_LOAD( "14122-01.u2t",   0x28000, 0x02000, 0xa89882d8 )
	ROM_RELOAD(                 0x30000, 0x02000 )
	ROM_LOAD( "14123-01.u3t",   0x2a000, 0x02000, 0xf9c51e5a )
	ROM_RELOAD(                 0x32000, 0x02000 )

	ROM_REGION( 0x28000, REGION_CPU2 )
	ROM_LOAD( "14100-01.u3",  0x00000, 0x02000, 0x1dffbdaf )
	ROM_LOAD( "14101-01.u4",  0x10000, 0x02000, 0xc585529c )
	ROM_CONTINUE(             0x1c000, 0x02000 )
	ROM_LOAD( "14102-01.u5",  0x12000, 0x02000, 0xace3f918 )
	ROM_CONTINUE(             0x1e000, 0x02000 )
	ROM_LOAD( "14103-01.u6",  0x14000, 0x02000, 0xcd41cf7a )
	ROM_CONTINUE(             0x20000, 0x02000 )
	ROM_LOAD( "14104-01.u7",  0x16000, 0x02000, 0x9b169e78 )
	ROM_CONTINUE(             0x22000, 0x02000 )
	ROM_LOAD( "14105-01.u8",  0x18000, 0x02000, 0xec596b43 )
	ROM_CONTINUE(             0x24000, 0x02000 )
	ROM_LOAD( "14106-01.u9",  0x1a000, 0x02000, 0xb9656baa )
	ROM_CONTINUE(             0x26000, 0x02000 )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "14112-00.u93", 0x00000, 0x04000, 0x8ccb1404 )
	ROM_LOAD( "14113-00.u94", 0x04000, 0x04000, 0x9941a55b )
	ROM_LOAD( "14114-00.u95", 0x08000, 0x04000, 0xb68baf47 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* U70 = Empty */
	ROM_LOAD( "14111-01.u92",  0x04000, 0x4000, 0x2508a9ad )
	ROM_LOAD( "14109-00.u69",  0x08000, 0x4000, 0xb123a28e )
	/* U91 = Empty */
	ROM_LOAD( "14108-01.u68",  0x10000, 0x4000, 0xa1a51383 )
	ROM_LOAD( "14110-01.u90",  0x14000, 0x4000, 0xef01d997 )
	ROM_LOAD( "14107-00.u67",  0x18000, 0x4000, 0x976334e6 )
	/* 89 = Empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( dblplay )
	ROM_REGION( 0x38000, REGION_CPU1 )
	ROM_LOAD( "15018-01.101",   0x00000, 0x02000, 0x17b6af29 )
	ROM_LOAD( "15019-01.102",   0x10000, 0x02000, 0x9fc8205e )
	ROM_CONTINUE(               0x1c000, 0x02000 )
	ROM_LOAD( "15020-01.103",   0x12000, 0x02000, 0x4edcc091 )
	ROM_CONTINUE(               0x1e000, 0x02000 )
	ROM_LOAD( "15021-01.104",   0x14000, 0x02000, 0xa0eba1c7 )
	ROM_CONTINUE(               0x20000, 0x02000 )
	ROM_LOAD( "15022-01.105",   0x16000, 0x02000, 0x7bbfe0b7 )
	ROM_CONTINUE(               0x22000, 0x02000 )
	ROM_LOAD( "15023-01.106",   0x18000, 0x02000, 0xbbedae34 )
	ROM_CONTINUE(               0x24000, 0x02000 )
	ROM_LOAD( "15024-01.107",   0x1a000, 0x02000, 0x02afcf52 )
	ROM_CONTINUE(               0x26000, 0x02000 )
	/* Extra banks ( referred to as the "top" board). Probably an add-on */
	ROM_LOAD( "15025-01.u2t",   0x28000, 0x02000, 0x1c959895 )
	ROM_RELOAD(                 0x30000, 0x02000 )
	ROM_LOAD( "15026-01.u3t",   0x2a000, 0x02000, 0xed5196d6 )
	ROM_RELOAD(                 0x32000, 0x02000 )
	ROM_LOAD( "15027-01.u4t",   0x2c000, 0x02000, 0x9b1e72e9 )
	ROM_CONTINUE(               0x34000, 0x02000 )

	ROM_REGION( 0x28000, REGION_CPU2 )
	ROM_LOAD( "15000-01.u03",  0x00000, 0x02000, 0x208a920a )
	ROM_LOAD( "15001-01.u04",  0x10000, 0x02000, 0x751c40d6 )
	ROM_CONTINUE(              0x1c000, 0x02000 )
	ROM_LOAD( "14402-01.u05",  0x12000, 0x02000, 0x5ffaec36 )
	ROM_CONTINUE(              0x1e000, 0x02000 )
	ROM_LOAD( "14403-01.u06",  0x14000, 0x02000, 0x48d6d9d3 )
	ROM_CONTINUE(              0x20000, 0x02000 )
	ROM_LOAD( "15004-01.u07",  0x16000, 0x02000, 0x6a7acebc )
	ROM_CONTINUE(              0x22000, 0x02000 )
	ROM_LOAD( "15005-01.u08",  0x18000, 0x02000, 0x69d487c9 )
	ROM_CONTINUE(              0x24000, 0x02000 )
	ROM_LOAD( "15006-01.u09",  0x1a000, 0x02000, 0xab3aac49 )
	ROM_CONTINUE(              0x26000, 0x02000 )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15015-01.u93", 0x00000, 0x04000, 0x8ccb1404 )
	ROM_LOAD( "15016-01.u94", 0x04000, 0x04000, 0x9941a55b )
	ROM_LOAD( "15017-01.u95", 0x08000, 0x04000, 0xb68baf47 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* U70 = Empty */
	ROM_LOAD( "15014-01.u92",  0x04000, 0x4000, 0x2508a9ad )
	ROM_LOAD( "15009-01.u69",  0x08000, 0x4000, 0xb123a28e )
	/* U91 = Empty */
	ROM_LOAD( "15008-01.u68",  0x10000, 0x4000, 0xa1a51383 )
	ROM_LOAD( "15012-01.u90",  0x14000, 0x4000, 0xef01d997 )
	ROM_LOAD( "15007-01.u67",  0x18000, 0x4000, 0x976334e6 )
	/* 89 = Empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( strkzone )
	ROM_REGION( 0x38000, REGION_CPU1 )
	ROM_LOAD( "strkzone.101",   0x00000, 0x04000, 0x8d83a611 )
	ROM_LOAD( "strkzone.102",   0x10000, 0x02000, 0x3859e67d )
	ROM_CONTINUE(               0x1c000, 0x02000 )
	ROM_LOAD( "strkzone.103",   0x12000, 0x02000, 0xcdd83bfb )
	ROM_CONTINUE(               0x1e000, 0x02000 )
	ROM_LOAD( "strkzone.104",   0x14000, 0x02000, 0xbe280212 )
	ROM_CONTINUE(               0x20000, 0x02000 )
	ROM_LOAD( "strkzone.105",   0x16000, 0x02000, 0xafb63390 )
	ROM_CONTINUE(               0x22000, 0x02000 )
	ROM_LOAD( "strkzone.106",   0x18000, 0x02000, 0xe853b9f6 )
	ROM_CONTINUE(               0x24000, 0x02000 )
	ROM_LOAD( "strkzone.107",   0x1a000, 0x02000, 0x1b4b6c2d )
	ROM_CONTINUE(               0x26000, 0x02000 )
	/* Extra banks ( referred to as the "top" board). Probably an add-on */
	ROM_LOAD( "strkzone.u2t",   0x28000, 0x02000, 0x8e0af06f )
	ROM_RELOAD(                 0x30000, 0x02000 )
	ROM_LOAD( "strkzone.u3t",   0x2a000, 0x02000, 0x909d35f3 )
	ROM_RELOAD(                 0x32000, 0x02000 )
	ROM_LOAD( "strkzone.u4t",   0x2c000, 0x02000, 0x9b1e72e9 )
	ROM_CONTINUE(               0x34000, 0x02000 )

	ROM_REGION( 0x28000, REGION_CPU2 )
	ROM_LOAD( "strkzone.u3",  0x00000, 0x02000, 0x40258fbe )
	ROM_LOAD( "strkzone.u4",  0x10000, 0x02000, 0xdf7f2604 )
	ROM_CONTINUE(             0x1c000, 0x02000 )
	ROM_LOAD( "strkzone.u5",  0x12000, 0x02000, 0x37885206 )
	ROM_CONTINUE(             0x1e000, 0x02000 )
	ROM_LOAD( "strkzone.u6",  0x14000, 0x02000, 0x6892dc4f )
	ROM_CONTINUE(             0x20000, 0x02000 )
	ROM_LOAD( "strkzone.u7",  0x16000, 0x02000, 0x6ac8f87c )
	ROM_CONTINUE(             0x22000, 0x02000 )
	ROM_LOAD( "strkzone.u8",  0x18000, 0x02000, 0x4b6d3725 )
	ROM_CONTINUE(             0x24000, 0x02000 )
	ROM_LOAD( "strkzone.u9",  0x1a000, 0x02000, 0xab3aac49 )
	ROM_CONTINUE(             0x26000, 0x02000 )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "strkzone.u93", 0x00000, 0x04000, 0x8ccb1404 )
	ROM_LOAD( "strkzone.u94", 0x04000, 0x04000, 0x9941a55b )
	ROM_LOAD( "strkzone.u95", 0x08000, 0x04000, 0xb68baf47 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* U70 = Empty */
	ROM_LOAD( "strkzone.u92",  0x04000, 0x4000, 0x2508a9ad )
	ROM_LOAD( "strkzone.u69",  0x08000, 0x4000, 0xb123a28e )
	/* U91 = Empty */
	ROM_LOAD( "strkzone.u68",  0x10000, 0x4000, 0xa1a51383 )
	ROM_LOAD( "strkzone.u90",  0x14000, 0x4000, 0xef01d997 )
	ROM_LOAD( "strkzone.u67",  0x18000, 0x4000, 0x976334e6 )
	/* 89 = Empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( redlin2p )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "13932-01.23t", 0x00000, 0x10000, 0xecdf0fbe )
	ROM_LOAD( "13931-01.22t", 0x10000, 0x10000, 0x16d01978 )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "13907-01.u3",  0x00000, 0x04000, 0xb760d63e )
	ROM_LOAD( "13908-01.u4",  0x10000, 0x02000, 0xa30739d3 )
	ROM_CONTINUE(             0x1c000, 0x02000 )
	ROM_LOAD( "13909-01.u5",  0x12000, 0x02000, 0xaaf16ad7 )
	ROM_CONTINUE(             0x1e000, 0x02000 )
	ROM_LOAD( "13910-01.u6",  0x14000, 0x02000, 0xd03469eb )
	ROM_CONTINUE(             0x20000, 0x02000 )
	ROM_LOAD( "13911-01.u7",  0x16000, 0x02000, 0x8ee1f547 )
	ROM_CONTINUE(             0x22000, 0x02000 )
	ROM_LOAD( "13912-01.u8",  0x18000, 0x02000, 0xe5b57eac )
	ROM_CONTINUE(             0x24000, 0x02000 )
	ROM_LOAD( "13913-01.u9",  0x1a000, 0x02000, 0x02886071 )
	ROM_CONTINUE(             0x26000, 0x02000 )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "17t",    0x0e0000, 0x10000, 0x8d26f221 )
	ROM_LOAD_V20_ODD ( "28t",    0x0e0000, 0x10000, 0x7aa21b2c )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "13930-01.u93", 0x00000, 0x04000, 0x0721f42e )
	ROM_LOAD( "13929-01.u94", 0x04000, 0x04000, 0x1522e7b2 )
	ROM_LOAD( "13928-01.u95", 0x08000, 0x04000, 0xc321b5d1 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "13920-01.u70",  0x00000, 0x4000, 0xf343d34a )
	ROM_LOAD( "13921-01.u92",  0x04000, 0x4000, 0xc9ba8d41 )
	ROM_LOAD( "13922-01.u69",  0x08000, 0x4000, 0x276cfba0 )
	ROM_LOAD( "13923-01.u91",  0x0c000, 0x4000, 0x4a88ea34 )
	ROM_LOAD( "13924-01.u68",  0x10000, 0x4000, 0x3995cb7e )
	/* 90 = empty / missing */
	ROM_LOAD( "13926-01.u67",  0x18000, 0x4000, 0xdaa30add )
	ROM_LOAD( "13927-01.u89",  0x1c000, 0x4000, 0x30e60fb5 )

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( quarterb )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "15219-05.49t", 0x00000, 0x10000, 0xff653e4f )
	ROM_LOAD( "15218-05.48t", 0x10000, 0x10000, 0x34b83d81 )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "15200-01.u3",  0x00000, 0x04000, 0x83297861 )
	ROM_LOAD( "15201-01.u4",  0x10000, 0x02000, 0xaf8dbdab )
	ROM_CONTINUE(             0x1c000, 0x02000 )
	ROM_LOAD( "15202-01.u5",  0x12000, 0x02000, 0x3eeecb3d )
	ROM_CONTINUE(             0x1e000, 0x02000 )
	ROM_LOAD( "15203-01.u6",  0x14000, 0x02000, 0xb9c5b663 )
	ROM_CONTINUE(             0x20000, 0x02000 )
	ROM_LOAD( "15204-01.u7",  0x16000, 0x02000, 0xc68821b7 )
	ROM_CONTINUE(             0x22000, 0x02000 )
	ROM_LOAD( "15205-01.u8",  0x18000, 0x02000, 0x2be843a9 )
	ROM_CONTINUE(             0x24000, 0x02000 )
	ROM_LOAD( "15206-01.u9",  0x1a000, 0x02000, 0x6bf8d4ab )
	ROM_CONTINUE(             0x26000, 0x02000 )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "15222-02.45t", 0x040000, 0x10000, 0x710bdc76 )
	ROM_LOAD_V20_ODD ( "15225-02.62t", 0x040000, 0x10000, 0x041cecde )
	ROM_LOAD_V20_EVEN( "15221-02.44t", 0x060000, 0x10000, 0xe0459ddb )
	ROM_LOAD_V20_ODD ( "15224-02.61t", 0x060000, 0x10000, 0x9027c579 )
	ROM_LOAD_V20_EVEN( "15220-02.43t", 0x0e0000, 0x10000, 0x48a8a018 )
	ROM_LOAD_V20_ODD ( "15223-02.60t", 0x0e0000, 0x10000, 0x6a299766 )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15215-01.u93", 0x00000, 0x04000, 0x4fb678d7 )
	ROM_LOAD( "lelqb.94",     0x04000, 0x04000, 0x7b57a44c )
	ROM_LOAD( "lelqb.95",     0x08000, 0x04000, 0x29bc33fd )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "15210-01.u70",  0x00000, 0x4000, 0xa5aea20e )
	ROM_LOAD( "15214-01.u92",  0x04000, 0x4000, 0x36f261ca )
	ROM_LOAD( "15209-01.u69",  0x08000, 0x4000, 0x0f5d74a4 )
	/* 91 = empty */
	ROM_LOAD( "15208-01.u68",  0x10000, 0x4000, 0x0319aec7 )
	ROM_LOAD( "15212-01.u90",  0x14000, 0x4000, 0x38b298d6 )
	ROM_LOAD( "15207-01.u67",  0x18000, 0x4000, 0x5ff86aad )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( quartrba )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "15219-02.49t",   0x00000, 0x10000, 0x7fbe1e5a )
	ROM_LOAD( "15218-02.48t",   0x10000, 0x10000, 0x6fbd4b27 )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "15200-01.u3",  0x00000, 0x04000, 0x83297861 )
	ROM_LOAD( "15201-01.u4",  0x10000, 0x02000, 0xaf8dbdab )
	ROM_CONTINUE(             0x1c000, 0x02000 )
	ROM_LOAD( "15202-01.u5",  0x12000, 0x02000, 0x3eeecb3d )
	ROM_CONTINUE(             0x1e000, 0x02000 )
	ROM_LOAD( "15203-01.u6",  0x14000, 0x02000, 0xb9c5b663 )
	ROM_CONTINUE(             0x20000, 0x02000 )
	ROM_LOAD( "15204-01.u7",  0x16000, 0x02000, 0xc68821b7 )
	ROM_CONTINUE(             0x22000, 0x02000 )
	ROM_LOAD( "15205-01.u8",  0x18000, 0x02000, 0x2be843a9 )
	ROM_CONTINUE(             0x24000, 0x02000 )
	ROM_LOAD( "15206-01.u9",  0x1a000, 0x02000, 0x6bf8d4ab )
	ROM_CONTINUE(             0x26000, 0x02000 )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "15222-01.45t", 0x040000, 0x10000, 0x722d1a19 )
	ROM_LOAD_V20_ODD ( "15225-01.62t", 0x040000, 0x10000, 0xf8c20496 )
	ROM_LOAD_V20_EVEN( "15221-01.44t", 0x060000, 0x10000, 0xbc6abaaf )
	ROM_LOAD_V20_ODD ( "15224-01.61t", 0x060000, 0x10000, 0x7ce3c3b7 )
	ROM_LOAD_V20_EVEN( "15220-01.43t", 0x0e0000, 0x10000, 0xccb6c8d7 )
	ROM_LOAD_V20_ODD ( "15223-01.60t", 0x0e0000, 0x10000, 0xc0ee425d )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15215-01.u93", 0x00000, 0x04000, 0x4fb678d7 )
	ROM_LOAD( "lelqb.94",     0x04000, 0x04000, 0x7b57a44c )
	ROM_LOAD( "lelqb.95",     0x08000, 0x04000, 0x29bc33fd )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "15210-01.u70",  0x00000, 0x4000, 0xa5aea20e )
	ROM_LOAD( "15214-01.u92",  0x04000, 0x4000, 0x36f261ca )
	ROM_LOAD( "15209-01.u69",  0x08000, 0x4000, 0x0f5d74a4 )
	/* 91 = empty */
	ROM_LOAD( "15208-01.u68",  0x10000, 0x4000, 0x0319aec7 )
	ROM_LOAD( "15212-01.u90",  0x14000, 0x4000, 0x38b298d6 )
	ROM_LOAD( "15207-01.u67",  0x18000, 0x4000, 0x5ff86aad )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( viper )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "15617-03.49t",   0x00000, 0x10000, 0x7e4688a6 )
	ROM_LOAD( "15616-03.48t",   0x10000, 0x10000, 0x3fe2f0bf )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "15600-02.u3", 0x00000, 0x02000, 0x0f57f68a )
	ROM_LOAD( "viper.u2t",   0x10000, 0x10000, 0x4043d4ee )
	ROM_LOAD( "viper.u3t",   0x20000, 0x10000, 0x213bc02b )
	ROM_LOAD( "viper.u4t",   0x30000, 0x10000, 0xce0b95b4 )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "15620-02.45t", 0x040000, 0x10000, 0x7380ece1 )
	ROM_LOAD_V20_ODD ( "15623-02.62t", 0x040000, 0x10000, 0x2921d8f9 )
	ROM_LOAD_V20_EVEN( "15619-02.44t", 0x060000, 0x10000, 0xc8507cc2 )
	ROM_LOAD_V20_ODD ( "15622-02.61t", 0x060000, 0x10000, 0x32dfda37 )
	ROM_LOAD_V20_EVEN( "15618-02.43t", 0x0e0000, 0x10000, 0x5562e0c3 )
	ROM_LOAD_V20_ODD ( "15621-02.60t", 0x0e0000, 0x10000, 0xcb468f2b )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15609-01.u93", 0x00000, 0x04000, 0x08ad92e9 )
	ROM_LOAD( "15610-01.u94", 0x04000, 0x04000, 0xd4e56dfb )
	ROM_LOAD( "15611-01.u95", 0x08000, 0x04000, 0x3a2c46fb )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "15604-01.u70",  0x00000, 0x4000, 0x7e3b0cce )
	ROM_LOAD( "15608-01.u92",  0x04000, 0x4000, 0xa9bde0ef )
	ROM_LOAD( "15603-01.u69",  0x08000, 0x4000, 0xaecc9516 )
	ROM_LOAD( "15607-01.u91",  0x0c000, 0x4000, 0x14f06f88 )
	ROM_LOAD( "15602-01.u68",  0x10000, 0x4000, 0x4ef613ad )
	ROM_LOAD( "15606-01.u90",  0x14000, 0x4000, 0x3c2e8e76 )
	ROM_LOAD( "15601-01.u67",  0x18000, 0x4000, 0xdc7006cd )
	ROM_LOAD( "15605-01.u89",  0x1c000, 0x4000, 0x4aa9c788 )

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( teamqb )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "15618-03.58t",   0x00000, 0x10000, 0xb32568dc )
	ROM_LOAD( "15619-03.59t",   0x10000, 0x10000, 0x40b3319f )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "15600-01.u3",   0x00000, 0x02000, 0x46615844 )
	ROM_LOAD( "15601-01.u2t",  0x10000, 0x10000, 0x8e523c58 )
	ROM_LOAD( "15602-01.u3t",  0x20000, 0x10000, 0x545b27a1 )
	ROM_LOAD( "15603-01.u4t",  0x30000, 0x10000, 0xcdc9c09d )
	ROM_LOAD( "15604-01.u5t",  0x40000, 0x10000, 0x3c03e92e )
	ROM_LOAD( "15605-01.u6t",  0x50000, 0x10000, 0xcdf7d19c )
	ROM_LOAD( "15606-01.u7t",  0x60000, 0x10000, 0x8eeb007c )
	ROM_LOAD( "15607-01.u8t",  0x70000, 0x10000, 0x57cb6d2d )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "15623-01.25t", 0x040000, 0x10000, 0x710bdc76 )
	ROM_LOAD_V20_ODD ( "15620-01.13t", 0x040000, 0x10000, 0x7e5cb8ad )
	ROM_LOAD_V20_EVEN( "15624-01.26t", 0x060000, 0x10000, 0xdd090d33 )
	ROM_LOAD_V20_ODD ( "15621-01.14t", 0x060000, 0x10000, 0xf68c68c9 )
	ROM_LOAD_V20_EVEN( "15625-01.27t", 0x0e0000, 0x10000, 0xac442523 )
	ROM_LOAD_V20_ODD ( "15622-01.15t", 0x0e0000, 0x10000, 0x9e84509a )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15615-01.u93", 0x00000, 0x04000, 0xa7ea6a87 )
	ROM_LOAD( "15616-01.u94", 0x04000, 0x04000, 0x4a9b3900 )
	ROM_LOAD( "15617-01.u95", 0x08000, 0x04000, 0x2cd95edb )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "15611-01.u70",  0x00000, 0x4000, 0xbf2695fb )
	ROM_LOAD( "15614-01.u92",  0x04000, 0x4000, 0xc93fd870 )
	ROM_LOAD( "15610-01.u69",  0x08000, 0x4000, 0x3e5b786f )
	/* 91 = empty */
	ROM_LOAD( "15609-01.u68",  0x10000, 0x4000, 0x0319aec7 )
	ROM_LOAD( "15613-01.u90",  0x14000, 0x4000, 0x4805802e )
	ROM_LOAD( "15608-01.u67",  0x18000, 0x4000, 0x78f0fd2b )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( teamqb2 )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "15618-03.58t",   0x00000, 0x10000, 0xb32568dc )
	ROM_LOAD( "15619-02.59t",   0x10000, 0x10000, 0x6d533714 )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "15600-01.u3",   0x00000, 0x02000, 0x46615844 )
	ROM_LOAD( "15601-01.u2t",  0x10000, 0x10000, 0x8e523c58 )
	ROM_LOAD( "15602-01.u3t",  0x20000, 0x10000, 0x545b27a1 )
	ROM_LOAD( "15603-01.u4t",  0x30000, 0x10000, 0xcdc9c09d )
	ROM_LOAD( "15604-01.u5t",  0x40000, 0x10000, 0x3c03e92e )
	ROM_LOAD( "15605-01.u6t",  0x50000, 0x10000, 0xcdf7d19c )
	ROM_LOAD( "15606-01.u7t",  0x60000, 0x10000, 0x8eeb007c )
	ROM_LOAD( "15607-01.u8t",  0x70000, 0x10000, 0x57cb6d2d )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "15623-01.25t", 0x040000, 0x10000, 0x710bdc76 )
	ROM_LOAD_V20_ODD ( "15620-01.13t", 0x040000, 0x10000, 0x7e5cb8ad )
	ROM_LOAD_V20_EVEN( "15624-01.26t", 0x060000, 0x10000, 0xdd090d33 )
	ROM_LOAD_V20_ODD ( "15621-01.14t", 0x060000, 0x10000, 0xf68c68c9 )
	ROM_LOAD_V20_EVEN( "15625-01.27t", 0x0e0000, 0x10000, 0xac442523 )
	ROM_LOAD_V20_ODD ( "15622-01.15t", 0x0e0000, 0x10000, 0x9e84509a )

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15615-01.u93", 0x00000, 0x04000, 0xa7ea6a87 )
	ROM_LOAD( "15616-01.u94", 0x04000, 0x04000, 0x4a9b3900 )
	ROM_LOAD( "15617-01.u95", 0x08000, 0x04000, 0x2cd95edb )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "15611-01.u70",  0x00000, 0x4000, 0xbf2695fb )
	ROM_LOAD( "15614-01.u92",  0x04000, 0x4000, 0xc93fd870 )
	ROM_LOAD( "15610-01.u69",  0x08000, 0x4000, 0x3e5b786f )
	/* 91 = empty */
	ROM_LOAD( "15609-01.u68",  0x10000, 0x4000, 0x0319aec7 )
	ROM_LOAD( "15613-01.u90",  0x14000, 0x4000, 0x4805802e )
	ROM_LOAD( "15608-01.u67",  0x18000, 0x4000, 0x78f0fd2b )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( aafb )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "aafbu58t.bin",   0x00000, 0x10000, 0xfa75a4a0 )
	ROM_LOAD( "aafbu59t.bin",   0x10000, 0x10000, 0xab6a606f )

	/* Everything from here down may be from the wrong version */
	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "24000-02.u3",   0x00000, 0x02000, 0x52df0354 )
	ROM_LOAD( "24001-02.u2t",  0x10000, 0x10000, 0x9b20697d )
	ROM_LOAD( "24002-02.u3t",  0x20000, 0x10000, 0xbbb92184 )
	ROM_LOAD( "15603-01.u4t",  0x30000, 0x10000, 0xcdc9c09d )
	ROM_LOAD( "15604-01.u5t",  0x40000, 0x10000, 0x3c03e92e )
	ROM_LOAD( "15605-01.u6t",  0x50000, 0x10000, 0xcdf7d19c )
	ROM_LOAD( "15606-01.u7t",  0x60000, 0x10000, 0x8eeb007c )
	ROM_LOAD( "24002-02.u8t",  0x70000, 0x10000, 0x3d9747c9 )

	ROM_REGION( 0x100000, REGION_CPU3 )
    ROM_LOAD_V20_EVEN( "24019-01.u25", 0x040000, 0x10000, 0x9e344768 )
    ROM_LOAD_V20_ODD ( "24016-01.u13", 0x040000, 0x10000, 0x6997025f )
    ROM_LOAD_V20_EVEN( "24020-01.u26", 0x060000, 0x10000, 0x0788f2a5 )
    ROM_LOAD_V20_ODD ( "24017-01.u14", 0x060000, 0x10000, 0xa48bd721 )
    ROM_LOAD_V20_EVEN( "24021-01.u27", 0x0e0000, 0x10000, 0x94081899 )
    ROM_LOAD_V20_ODD ( "24018-01.u15", 0x0e0000, 0x10000, 0x76eb6077 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "24011-02.u93", 0x00000, 0x08000, 0x00000000 )
	ROM_LOAD( "24012-02.u94", 0x08000, 0x08000, 0x00000000 )
	ROM_LOAD( "24013-02.u95", 0x10000, 0x08000, 0x00000000 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "24007-01.u70",  0x00000, 0x4000, 0x40e46aa4 )
	ROM_LOAD( "24010-01.u92",  0x04000, 0x4000, 0x78705f42 )
	ROM_LOAD( "24006-01.u69",  0x08000, 0x4000, 0x6a576aa9 )
	ROM_LOAD( "24009-02.u91",  0x0c000, 0x4000, 0xb857a1ad )
	ROM_LOAD( "24005-02.u68",  0x10000, 0x4000, 0x8ea75319 )
	ROM_LOAD( "24008-01.u90",  0x14000, 0x4000, 0x4538bc58 )
	ROM_LOAD( "24004-02.u67",  0x18000, 0x4000, 0xcd7a3338 )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( aafbb )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "24014-02.u58",   0x00000, 0x10000, 0x5db4a3d0 )
	ROM_LOAD( "24015-02.u59",   0x10000, 0x10000, 0x00000000 )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "24000-02.u3",   0x00000, 0x02000, 0x52df0354 )
	ROM_LOAD( "24001-02.u2t",  0x10000, 0x10000, 0x9b20697d )
	ROM_LOAD( "24002-02.u3t",  0x20000, 0x10000, 0xbbb92184 )
	ROM_LOAD( "15603-01.u4t",  0x30000, 0x10000, 0xcdc9c09d )
	ROM_LOAD( "15604-01.u5t",  0x40000, 0x10000, 0x3c03e92e )
	ROM_LOAD( "15605-01.u6t",  0x50000, 0x10000, 0xcdf7d19c )
	ROM_LOAD( "15606-01.u7t",  0x60000, 0x10000, 0x8eeb007c )
	ROM_LOAD( "24002-02.u8t",  0x70000, 0x10000, 0x3d9747c9 )

	ROM_REGION( 0x100000, REGION_CPU3 )
    ROM_LOAD_V20_EVEN( "24019-01.u25", 0x040000, 0x10000, 0x9e344768 )
    ROM_LOAD_V20_ODD ( "24016-01.u13", 0x040000, 0x10000, 0x6997025f )
    ROM_LOAD_V20_EVEN( "24020-01.u26", 0x060000, 0x10000, 0x0788f2a5 )
    ROM_LOAD_V20_ODD ( "24017-01.u14", 0x060000, 0x10000, 0xa48bd721 )
    ROM_LOAD_V20_EVEN( "24021-01.u27", 0x0e0000, 0x10000, 0x94081899 )
    ROM_LOAD_V20_ODD ( "24018-01.u15", 0x0e0000, 0x10000, 0x76eb6077 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "24011-02.u93", 0x00000, 0x08000, 0x71f4425b )
	ROM_LOAD( "24012-02.u94", 0x08000, 0x08000, 0xb2499547 )
	ROM_LOAD( "24013-02.u95", 0x10000, 0x08000, 0x0a604e0d )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "24007-01.u70",  0x00000, 0x4000, 0x40e46aa4 )
	ROM_LOAD( "24010-01.u92",  0x04000, 0x4000, 0x78705f42 )
	ROM_LOAD( "24006-01.u69",  0x08000, 0x4000, 0x6a576aa9 )
	ROM_LOAD( "24009-02.u91",  0x0c000, 0x4000, 0xb857a1ad )
	ROM_LOAD( "24005-02.u68",  0x10000, 0x4000, 0x8ea75319 )
	ROM_LOAD( "24008-01.u90",  0x14000, 0x4000, 0x4538bc58 )
	ROM_LOAD( "24004-02.u67",  0x18000, 0x4000, 0xcd7a3338 )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( aafbd2p )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "26014-01.58t", 0x00000, 0x10000, 0x79fd14cd )
	ROM_LOAD( "26015-01.59t", 0x10000, 0x10000, 0x3b0382f0 )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "26000-01.u3",   0x00000, 0x02000, 0x98c06c63 )
	ROM_LOAD( "26001-01.2t",   0x10000, 0x10000, 0xf118b9b4 )
	ROM_LOAD( "24002-02.u3t",  0x20000, 0x10000, 0xbbb92184 )
	ROM_LOAD( "15603-01.u4t",  0x30000, 0x10000, 0xcdc9c09d )
	ROM_LOAD( "15604-01.u5t",  0x40000, 0x10000, 0x3c03e92e )
	ROM_LOAD( "15605-01.u6t",  0x50000, 0x10000, 0xcdf7d19c )
	ROM_LOAD( "15606-01.u7t",  0x60000, 0x10000, 0x8eeb007c )
	ROM_LOAD( "24002-02.u8t",  0x70000, 0x10000, 0x3d9747c9 )

	ROM_REGION( 0x100000, REGION_CPU3 )
    ROM_LOAD_V20_EVEN( "24019-01.u25", 0x040000, 0x10000, 0x9e344768 )
    ROM_LOAD_V20_ODD ( "24016-01.u13", 0x040000, 0x10000, 0x6997025f )
    ROM_LOAD_V20_EVEN( "24020-01.u26", 0x060000, 0x10000, 0x0788f2a5 )
    ROM_LOAD_V20_ODD ( "24017-01.u14", 0x060000, 0x10000, 0xa48bd721 )
    ROM_LOAD_V20_EVEN( "24021-01.u27", 0x0e0000, 0x10000, 0x94081899 )
    ROM_LOAD_V20_ODD ( "24018-01.u15", 0x0e0000, 0x10000, 0x76eb6077 )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "24011-02.u93", 0x00000, 0x08000, 0x71f4425b )
	ROM_LOAD( "24012-02.u94", 0x08000, 0x08000, 0xb2499547 )
	ROM_LOAD( "24013-02.u95", 0x10000, 0x08000, 0x0a604e0d )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "24007-01.u70",  0x00000, 0x4000, 0x40e46aa4 )
	ROM_LOAD( "24010-01.u92",  0x04000, 0x4000, 0x78705f42 )
	ROM_LOAD( "24006-01.u69",  0x08000, 0x4000, 0x6a576aa9 )
	ROM_LOAD( "24009-02.u91",  0x0c000, 0x4000, 0xb857a1ad )
	ROM_LOAD( "24005-02.u68",  0x10000, 0x4000, 0x8ea75319 )
	ROM_LOAD( "24008-01.u90",  0x14000, 0x4000, 0x4538bc58 )
	ROM_LOAD( "24004-02.u67",  0x18000, 0x4000, 0xcd7a3338 )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( offroad )
	ROM_REGION( 0x40000, REGION_CPU1 )
	ROM_LOAD( "22121-04.u58",   0x00000, 0x10000, 0xc5790988 )
	ROM_LOAD( "22122-03.u59",   0x10000, 0x10000, 0xae862fdc )
	ROM_LOAD( "22120-01.u57",   0x20000, 0x10000, 0xe9f0f175 )
	ROM_LOAD( "22119-02.u56",   0x30000, 0x10000, 0x38642f22 )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "22100-01.u2",  0x00000, 0x02000, 0x08c96a4b )
	ROM_LOAD( "22108-02.u4",  0x30000, 0x10000, 0x0d72780a )
	ROM_LOAD( "22109-02.u5",  0x40000, 0x10000, 0x5429ce2c )
	ROM_LOAD( "22110-02.u6",  0x50000, 0x10000, 0xf97bad5c )
	ROM_LOAD( "22111-01.u7",  0x60000, 0x10000, 0xf79157a1 )
	ROM_LOAD( "22112-01.u8",  0x70000, 0x10000, 0x3eef38d3 )

	ROM_REGION( 0x100000, REGION_CPU3 )
    ROM_LOAD_V20_EVEN( "22116-03.u25", 0x040000, 0x10000, 0x95bb31d3 )
    ROM_LOAD_V20_ODD ( "22113-03.u13", 0x040000, 0x10000, 0x71b28df6 )
    ROM_LOAD_V20_EVEN( "22117-03.u26", 0x060000, 0x10000, 0x703d81ce )
    ROM_LOAD_V20_ODD ( "22114-03.u14", 0x060000, 0x10000, 0xf8b31bf8 )
    ROM_LOAD_V20_EVEN( "22118-03.u27", 0x0e0000, 0x10000, 0x806ccf8b )
    ROM_LOAD_V20_ODD ( "22115-03.u15", 0x0e0000, 0x10000, 0xc8439a7a )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "22105-01.u93", 0x00000, 0x08000, 0x4426e367 )
	ROM_LOAD( "22106-02.u94", 0x08000, 0x08000, 0x687dc1fc )
	ROM_LOAD( "22107-02.u95", 0x10000, 0x08000, 0xcee6ee5f )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* 70 = empty */
	ROM_LOAD( "22104-01.u92",  0x04000, 0x4000, 0x03e0497d )
	ROM_LOAD( "22102-01.u69",  0x08000, 0x4000, 0xc3f2e443 )
	/* 91 = empty */
	/* 68 = empty */
	ROM_LOAD( "22103-02.u90",  0x14000, 0x4000, 0x2266757a )
	ROM_LOAD( "22101-01.u67",  0x18000, 0x4000, 0xecab0527 )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( offroadt )
	ROM_REGION( 0x040000, REGION_CPU1 )
	ROM_LOAD( "ortpu58.bin",   0x00000, 0x10000, 0xadbc6211 )
	ROM_LOAD( "ortpu59.bin",   0x10000, 0x10000, 0x296dd3b6 )
	ROM_LOAD( "ortpu57.bin",   0x20000, 0x10000, 0xe9f0f175 )
	ROM_LOAD( "ortpu56.bin",   0x30000, 0x10000, 0x2c1a22b3 )

	ROM_REGION( 0x90000, REGION_CPU2 )
	ROM_LOAD( "ortpu3b.bin", 0x00000, 0x02000, 0x95abb9f1 )
	ROM_LOAD( "ortpu2.bin",  0x10000, 0x10000, 0xc46c1627 )
	ROM_LOAD( "ortpu3.bin",  0x20000, 0x10000, 0x2276546f )
	ROM_LOAD( "ortpu4.bin",  0x30000, 0x10000, 0xaa4b5975 )
	ROM_LOAD( "ortpu5.bin",  0x40000, 0x10000, 0x69100b06 )
	ROM_LOAD( "ortpu6.bin",  0x50000, 0x10000, 0xb75015b8 )
	ROM_LOAD( "ortpu7.bin",  0x60000, 0x10000, 0xa5af5b4f )
	ROM_LOAD( "ortpu8.bin",  0x70000, 0x10000, 0x0f735078 )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "ortpu25.bin", 0x040000, 0x10000, 0xf952f800 )
	ROM_LOAD_V20_ODD ( "ortpu13.bin", 0x040000, 0x10000, 0x7beec9fc )
	ROM_LOAD_V20_EVEN( "ortpu26.bin", 0x060000, 0x10000, 0x6227ea94 )
	ROM_LOAD_V20_ODD ( "ortpu14.bin", 0x060000, 0x10000, 0x0a44331d )
	ROM_LOAD_V20_EVEN( "ortpu27.bin", 0x0e0000, 0x10000, 0xb80c5f99 )
	ROM_LOAD_V20_ODD ( "ortpu15.bin", 0x0e0000, 0x10000, 0x2a1a1c3c )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ortpu93b.bin", 0x00000, 0x08000, 0xf0c1d8b0 )
	ROM_LOAD( "ortpu94b.bin", 0x08000, 0x08000, 0x7460d8c0 )
	ROM_LOAD( "ortpu95b.bin", 0x10000, 0x08000, 0x081ee7a8 )

	ROM_REGION( 0x20000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	/* 70 = empty */
	ROM_LOAD( "ortpu92b.bin",  0x04000, 0x4000, 0xf9988e28 )
	ROM_LOAD( "ortpu69b.bin",  0x08000, 0x4000, 0xfe5f8d8f )
	/* 91 = empty */
	/* 68 = empty */
	ROM_LOAD( "ortpu90b.bin",  0x14000, 0x4000, 0xbda2ecb1 )
	ROM_LOAD( "ortpu67b.bin",  0x18000, 0x4000, 0x38c9bf29 )
	/* 89 = empty */

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( pigout )
	ROM_REGION( 0x040000, REGION_CPU1 )
	ROM_LOAD( "poutu58t.bin",  0x00000, 0x10000, 0x8fe4b683 )
	ROM_LOAD( "poutu59t.bin",  0x10000, 0x10000, 0xab907762 )
	ROM_LOAD( "poutu57t.bin",  0x20000, 0x10000, 0xc22be0ff )

	ROM_REGION( 0x080000, REGION_CPU2 )
	ROM_LOAD( "poutu3.bin",   0x00000, 0x02000, 0xaf213cb7 )
	ROM_LOAD( "poutu2t.bin",  0x10000, 0x10000, 0xb23164c6 )
	ROM_LOAD( "poutu3t.bin",  0x20000, 0x10000, 0xd93f105f )
	ROM_LOAD( "poutu4t.bin",  0x30000, 0x10000, 0xb7c47bfe )
	ROM_LOAD( "poutu5t.bin",  0x40000, 0x10000, 0xd9b9dfbf )
	ROM_LOAD( "poutu6t.bin",  0x50000, 0x10000, 0x728c7c1a )
	ROM_LOAD( "poutu7t.bin",  0x60000, 0x10000, 0x393bd990 )
	ROM_LOAD( "poutu8t.bin",  0x70000, 0x10000, 0xcb9ffaad )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "poutu25t.bin", 0x040000, 0x10000, 0x92cd2617 )
	ROM_LOAD_V20_ODD ( "poutu13t.bin", 0x040000, 0x10000, 0x9448c389 )
	ROM_LOAD_V20_EVEN( "poutu26t.bin", 0x060000, 0x10000, 0xab57de8f )
	ROM_LOAD_V20_ODD ( "poutu14t.bin", 0x060000, 0x10000, 0x30678e93 )
	ROM_LOAD_V20_EVEN( "poutu27t.bin", 0x0e0000, 0x10000, 0x37a8156e )
	ROM_LOAD_V20_ODD ( "poutu15t.bin", 0x0e0000, 0x10000, 0x1c60d58b )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "poutu93.bin", 0x000000, 0x08000, 0xf102a04d )
	ROM_LOAD( "poutu94.bin", 0x008000, 0x08000, 0xec63c015 )
	ROM_LOAD( "poutu95.bin", 0x010000, 0x08000, 0xba6e797e )

	ROM_REGION( 0x40000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "poutu70.bin",  0x00000, 0x4000, 0x7db4eaa1 )
	ROM_LOAD( "poutu92.bin",  0x04000, 0x4000, 0x20fa57bb )
	ROM_LOAD( "poutu69.bin",  0x08000, 0x4000, 0xa16886f3 )
	ROM_LOAD( "poutu91.bin",  0x0c000, 0x4000, 0x482a3581 )
	ROM_LOAD( "poutu68.bin",  0x10000, 0x4000, 0x7b62a3ed )
	ROM_LOAD( "poutu90.bin",  0x14000, 0x4000, 0x9615d710 )
	ROM_LOAD( "poutu67.bin",  0x18000, 0x4000, 0xaf85ce79 )
	ROM_LOAD( "poutu89.bin",  0x1c000, 0x4000, 0x6c874a05 )

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END

ROM_START( pigouta )
	ROM_REGION( 0x040000, REGION_CPU1 )
	ROM_LOAD( "03-29020.01", 0x00000, 0x10000, 0x6c815982 )
	ROM_LOAD( "03-29021.01", 0x10000, 0x10000, 0x9de7a763 )
	ROM_LOAD( "poutu57t.bin", 0x20000, 0x10000, 0xc22be0ff )

	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "poutu3.bin",   0x00000, 0x02000, 0xaf213cb7 )
	ROM_LOAD( "poutu2t.bin",  0x10000, 0x10000, 0xb23164c6 )
	ROM_LOAD( "poutu3t.bin",  0x20000, 0x10000, 0xd93f105f )
	ROM_LOAD( "poutu4t.bin",  0x30000, 0x10000, 0xb7c47bfe )
	ROM_LOAD( "poutu5t.bin",  0x40000, 0x10000, 0xd9b9dfbf )
	ROM_LOAD( "poutu6t.bin",  0x50000, 0x10000, 0x728c7c1a )
	ROM_LOAD( "poutu7t.bin",  0x60000, 0x10000, 0x393bd990 )
	ROM_LOAD( "poutu8t.bin",  0x70000, 0x10000, 0xcb9ffaad )

	ROM_REGION( 0x100000, REGION_CPU3 )
	ROM_LOAD_V20_EVEN( "poutu25t.bin", 0x040000, 0x10000, 0x92cd2617 )
	ROM_LOAD_V20_ODD ( "poutu13t.bin", 0x040000, 0x10000, 0x9448c389 )
	ROM_LOAD_V20_EVEN( "poutu26t.bin", 0x060000, 0x10000, 0xab57de8f )
	ROM_LOAD_V20_ODD ( "poutu14t.bin", 0x060000, 0x10000, 0x30678e93 )
	ROM_LOAD_V20_EVEN( "poutu27t.bin", 0x0e0000, 0x10000, 0x37a8156e )
	ROM_LOAD_V20_ODD ( "poutu15t.bin", 0x0e0000, 0x10000, 0x1c60d58b )

	ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "poutu93.bin", 0x000000, 0x08000, 0xf102a04d )
	ROM_LOAD( "poutu94.bin", 0x008000, 0x08000, 0xec63c015 )
	ROM_LOAD( "poutu95.bin", 0x010000, 0x08000, 0xba6e797e )

	ROM_REGION( 0x40000, REGION_USER1 )   /* Ordering: 70/92/69/91/68/90/67/89 */
	ROM_LOAD( "poutu70.bin",  0x00000, 0x4000, 0x7db4eaa1 )
	ROM_LOAD( "poutu92.bin",  0x04000, 0x4000, 0x20fa57bb )
	ROM_LOAD( "poutu69.bin",  0x08000, 0x4000, 0xa16886f3 )
	ROM_LOAD( "poutu91.bin",  0x0c000, 0x4000, 0x482a3581 )
	ROM_LOAD( "poutu68.bin",  0x10000, 0x4000, 0x7b62a3ed )
	ROM_LOAD( "poutu90.bin",  0x14000, 0x4000, 0x9615d710 )
	ROM_LOAD( "poutu67.bin",  0x18000, 0x4000, 0xaf85ce79 )
	ROM_LOAD( "poutu89.bin",  0x1c000, 0x4000, 0x6c874a05 )

    ROM_REGION( battery_ram_size, REGION_USER2 ) /* extra RAM regions */
ROM_END



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

/* also called by Ataxx */
void leland_rotate_memory(int cpunum)
{
	int startaddr = 0x10000;
	int banks = (memory_region_length(REGION_CPU1 + cpunum) - startaddr) / 0x8000;
	UINT8 *ram = memory_region(REGION_CPU1 + cpunum);
	UINT8 temp[0x2000];
	int i;

	for (i = 0; i < banks; i++)
	{
		memmove(temp, &ram[startaddr + 0x0000], 0x2000);
		memmove(&ram[startaddr + 0x0000], &ram[startaddr + 0x2000], 0x6000);
		memmove(&ram[startaddr + 0x6000], temp, 0x2000);
		startaddr += 0x8000;
	}
}

static void init_master_ports(UINT8 mvram_base, UINT8 io_base)
{
	/* set up the master CPU VRAM I/O */
	install_port_read_handler(0, mvram_base, mvram_base + 0x1f, leland_mvram_port_r);
	install_port_write_handler(0, mvram_base, mvram_base + 0x1f, leland_mvram_port_w);

	/* set up the master CPU I/O ports */
	install_port_read_handler(0, io_base, io_base + 0x1f, master_input_r);
	install_port_write_handler(0, io_base, io_base + 0x0f, master_output_w);
}


static void init_cerberus(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 cerberus_eeprom_data[] =
	{
		0x05,0x0001,
		0x06,0x0001,
		0x07,0x0001,
		0x08,0x0001,
		0x09,0x0004,
		0x0a,0x0004,
		0x0e,0x0001,
		0x0f,0x0003,
		0x10,0x0500,
		0x12,0x0005,
		0x13,0x0003,
		0x3f,0x001d,
		0xffff
	};
	init_eeprom(0x00, cerberus_eeprom_data, 0, SERIAL_TYPE_NONE);

	/* master CPU bankswitching */
	update_master_bank = cerberus_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x40, 0x80);

	/* set up additional input ports */
	install_port_read_handler(0, 0x80, 0x80, cerberus_dial_1_r);
	install_port_read_handler(0, 0x90, 0x90, cerberus_dial_2_r);
}

static void init_mayhem(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 mayhem_eeprom_data[] =
	{
		0x05,0x0001,
		0x06,0x0001,
		0x07,0x0001,
		0x08,0x0001,
		0x09,0x0004,
		0x0a,0x0004,
		0x0c,0xff00,
		0x13,0x28ff,
		0x14,0x0023,
		0x15,0x0005,
		0x1b,0x0060,
		0x1c,0x4a00,
		0x1d,0x4520,
		0x1e,0x4943,
		0x1f,0x454e,
		0x20,0x414d,
		0x21,0x5254,
		0x22,0x4e4f,
		0x23,0x4349,
		0x24,0x2053,
		0x25,0x2020,
		0x26,0x2020,
		0x27,0x2020,
		0x3f,0x0818,
		0xffff
	};
	init_eeprom(0x00, mayhem_eeprom_data, 0x28, SERIAL_TYPE_ADD);

	/* master CPU bankswitching */
	update_master_bank = mayhem_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0xc0);
}

static void init_wseries(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 wseries_eeprom_data[] =
	{
		0x19,0xfefe,
		0x1a,0xfefe,
		0x1b,0xfbfb,
		0x1d,0x00ff,
		0xffff
	};
	init_eeprom(0xff, wseries_eeprom_data, 0x12, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = mayhem_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x40, 0x80);
}

static void init_alleymas(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 alleymas_eeprom_data[] =
	{
		0x13,0xfefe,
		0x14,0xfefe,
		0x15,0xfbfb,
		0x17,0x00ff,
		0x18,0xff00,
		0x37,0x00ff,
		0xffff
	};
	init_eeprom(0xff, alleymas_eeprom_data, 0x0c, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = mayhem_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0xc0);

	/* kludge warning: the game uses location E0CA to determine if the joysticks are available */
	/* it gets cleared by the code, but there is no obvious way for the value to be set to a */
	/* non-zero value. If the value is zero, the joystick is never read. */
	alleymas_kludge_mem = (UINT8*)install_mem_write_handler(0, 0xe0ca, 0xe0ca, alleymas_joystick_kludge);
}

static void init_dangerz(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 dangerz_eeprom_data[] =
	{
		0x17,0xfefe,
		0x18,0xfefe,
		0x19,0xfbfb,
		0x1b,0x00ff,
		0x1c,0xfffa,
		0x38,0xb6bc,
		0x39,0xffb1,
		0x3a,0x8007,
		0xffff
	};
	init_eeprom(0xff, dangerz_eeprom_data, 0x10, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = dangerz_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x40, 0x80);

	/* set up additional input ports */
	install_port_read_handler(0, 0xf4, 0xf4, dangerz_input_upper_r);
	install_port_read_handler(0, 0xf8, 0xf8, dangerz_input_y_r);
	install_port_read_handler(0, 0xfc, 0xfc, dangerz_input_x_r);
}

static void init_basebal2(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 basebal2_eeprom_data[] =
	{
		0x19,0xfefe,
		0x1a,0xfefe,
		0x1b,0xfbfb,
		0x1d,0x00ff,
		0xffff
	};
	init_eeprom(0xff, basebal2_eeprom_data, 0x12, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = basebal2_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0xc0);
}

static void init_dblplay(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 dblplay_eeprom_data[] =
	{
		0x18,0xfefe,
		0x19,0xfefe,
		0x1a,0xfbfb,
		0x1c,0x00ff,
		0x3b,0xffe1,
		0xffff
	};
	init_eeprom(0xff, dblplay_eeprom_data, 0x11, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = basebal2_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x80, 0x40);
}

static void init_strkzone(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 strkzone_eeprom_data[] =
	{
		0x16,0xfefe,
		0x17,0xfefe,
		0x18,0xfbfb,
		0x1a,0x00ff,
		0x1b,0xffe1,
		0xffff
	};
	init_eeprom(0xff, strkzone_eeprom_data, 0x0f, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = basebal2_bankswitch;

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0x40);
}

static void init_redlin2p(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 redlin2p_eeprom_data[] =
	{
		0x1f,0xfefe,
		0x20,0xfffb,
		0x21,0xfa00,
		0x22,0xfffe,
		0xffff
	};
	init_eeprom(0xff, redlin2p_eeprom_data, 0x18, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = redline_bankswitch;

	leland_rotate_memory(0);

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0xc0);

	/* set up additional input ports */
	install_port_read_handler(0, 0xc0, 0xc0, redline_pedal_1_r);
	install_port_read_handler(0, 0xd0, 0xd0, redline_pedal_2_r);
	install_port_read_handler(0, 0xf8, 0xf8, redline_wheel_2_r);
	install_port_read_handler(0, 0xfb, 0xfb, redline_wheel_1_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x828);
}

static void init_quarterb(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 quarterb_eeprom_data[] =
	{
		0x34,0xfefe,
		0x35,0xfefe,
		0x36,0xfbfb,
		0x38,0x00ff,
		0x39,0x53ff,
		0x3a,0xffd9,
		0xffff
	};
	init_eeprom(0xff, quarterb_eeprom_data, 0x24, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = viper_bankswitch;

	leland_rotate_memory(0);

	/* set up the master CPU I/O ports */
	init_master_ports(0x40, 0x80);

	/* optimize the sound */
	leland_i86_optimize_address(0x9bc);
}

static void init_viper(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 viper_eeprom_data[] =
	{
		0x13,0xfefe,
		0x14,0xfefe,
		0x15,0xfbfb,
		0x17,0x00ff,
		0x18,0xfcfa,
		0x1b,0xfffe,
		0xffff
	};
	init_eeprom(0xff, viper_eeprom_data, 0x0c, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = viper_bankswitch;

	leland_rotate_memory(0);
	leland_rotate_memory(1);
	leland_rotate_memory(1);

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0xc0);

	/* set up additional input ports */
	install_port_read_handler(0, 0xa4, 0xa4, dangerz_input_upper_r);
	install_port_read_handler(0, 0xb8, 0xb8, dangerz_input_y_r);
	install_port_read_handler(0, 0xbc, 0xbc, dangerz_input_x_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x788);
}

static void init_teamqb(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 teamqb_eeprom_data[] =
	{
		0x36,0xfefe,
		0x37,0xfefe,
		0x38,0xfbfb,
		0x3a,0x5300,
		0x3b,0xffd9,
		0xffff
	};
	init_eeprom(0xff, teamqb_eeprom_data, 0x1a, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = viper_bankswitch;

	leland_rotate_memory(0);
	leland_rotate_memory(1);
	leland_rotate_memory(1);

	/* set up the master CPU I/O ports */
	init_master_ports(0x40, 0x80);

	/* set up additional input ports */
	install_port_read_handler(0, 0x7c, 0x7c, input_port_10_r);
	install_port_read_handler(0, 0x7f, 0x7f, input_port_11_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x788);
}

static void init_aafb(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 aafb_eeprom_data[] =
	{
		0x36,0xfefe,
		0x37,0xfefe,
		0x38,0xfbfb,
		0x3a,0x5300,
		0x3b,0xffd9,
		0xffff
	};
	init_eeprom(0xff, aafb_eeprom_data, 0x1a, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = viper_bankswitch;

	leland_rotate_memory(0);
	leland_rotate_memory(1);
	leland_rotate_memory(1);

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0xc0);

	/* set up additional input ports */
	install_port_read_handler(0, 0x7c, 0x7c, input_port_10_r);
	install_port_read_handler(0, 0x7f, 0x7f, input_port_11_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x788);
}

static void init_aafbb(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 aafb_eeprom_data[] =
	{
		0x36,0xfefe,
		0x37,0xfefe,
		0x38,0xfbfb,
		0x3a,0x5300,
		0x3b,0xffd9,
		0xffff
	};
	init_eeprom(0xff, aafb_eeprom_data, 0x1a, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = viper_bankswitch;

	leland_rotate_memory(0);
	leland_rotate_memory(1);
	leland_rotate_memory(1);

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0x40);

	/* set up additional input ports */
	install_port_read_handler(0, 0x7c, 0x7c, input_port_10_r);
	install_port_read_handler(0, 0x7f, 0x7f, input_port_11_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x788);
}

static void init_offroad(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 offroad_eeprom_data[] =
	{
		0x09,0xfefe,
		0x0a,0xfffb,
		0x0d,0x00ff,
		0x0e,0xfffb,
		0x36,0xfeff,
		0x37,0xfefe,
		0x38,0xfffe,
		0x39,0x50ff,
		0x3a,0x976c,
		0x3b,0xffad,
		0xffff
	};
	init_eeprom(0xff, offroad_eeprom_data, 0x00, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = offroad_bankswitch;

	leland_rotate_memory(0);
	leland_rotate_memory(1);
	leland_rotate_memory(1);

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0xc0);
	init_master_ports(0x40, 0x80);	/* yes, this is intentional */

	/* set up additional input ports */
	install_port_read_handler(0, 0xf8, 0xf8, offroad_wheel_3_r);
	install_port_read_handler(0, 0xf9, 0xf9, offroad_wheel_1_r);
	install_port_read_handler(0, 0xfb, 0xfb, offroad_wheel_2_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x788);
}

static void init_offroadt(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 offroadt_eeprom_data[] =
	{
		0x09,0xfefe,
		0x0a,0xfffb,
		0x0d,0x00ff,
		0x0e,0xfffb,
		0x36,0xfeff,
		0x37,0xfefe,
		0x38,0xfffe,
		0x39,0x50ff,
		0x3a,0x976c,
		0x3b,0xffad,
		0xffff
	};
	init_eeprom(0xff, offroadt_eeprom_data, 0x00, SERIAL_TYPE_ENCRYPT_XOR);

	/* master CPU bankswitching */
	update_master_bank = offroad_bankswitch;

	leland_rotate_memory(0);
	leland_rotate_memory(1);
	leland_rotate_memory(1);

	/* set up the master CPU I/O ports */
	init_master_ports(0x80, 0x40);

	/* set up additional input ports */
	install_port_read_handler(0, 0xf8, 0xf8, offroad_wheel_3_r);
	install_port_read_handler(0, 0xf9, 0xf9, offroad_wheel_1_r);
	install_port_read_handler(0, 0xfb, 0xfb, offroad_wheel_2_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x788);
}

static void init_pigout(void)
{
	/* initialize the default EEPROM state */
	static const UINT16 pigout_eeprom_data[] =
	{
		0x09,0xfefe,
		0x0a,0xfefb,
		0x0b,0xfffe,
		0x0c,0xfefe,
		0x0d,0xfffb,
		0x39,0xfcff,
		0x3a,0xfb00,
		0x3b,0xfffc,
		0xffff
	};
	init_eeprom(0xff, pigout_eeprom_data, 0x00, SERIAL_TYPE_ENCRYPT);

	/* master CPU bankswitching */
	update_master_bank = offroad_bankswitch;

	leland_rotate_memory(0);
	leland_rotate_memory(1);
	leland_rotate_memory(1);

	/* set up the master CPU I/O ports */
	init_master_ports(0x00, 0x40);

	/* set up additional input ports */
	install_port_read_handler(0, 0x7f, 0x7f, input_port_4_r);

	/* optimize the sound */
	leland_i86_optimize_address(0x788);
}



/*************************************
 *
 *	Game drivers
 *
 *************************************/

/* small master banks, small slave banks */
GAME( 1985, cerberus, 0,       leland,  cerberus, cerberus, ROT0,   "Cinematronics", "Cerberus" )
GAME( 1985, mayhem,   0,       leland,  mayhem,   mayhem,   ROT0,   "Cinematronics", "Mayhem 2002" )
GAME( 1985, wseries,  0,       leland,  wseries,  wseries,  ROT0,   "Cinematronics", "World Series: The Season" )
GAME( 1986, alleymas, 0,       leland,  alleymas, alleymas, ROT270, "Cinematronics", "Alley Master" )

/* odd master banks, small slave banks */
GAME( 1986, dangerz,  0,       leland,  dangerz,  dangerz,  ROT0,   "Cinematronics", "Danger Zone" )

/* small master banks + extra top board, small slave banks */
GAME( 1987, basebal2, 0,       leland,  basebal2, basebal2, ROT0,   "Cinematronics", "Baseball The Season II" )
GAME( 1987, dblplay,  0,       leland,  basebal2, dblplay,  ROT0,   "Leland Corp. / Tradewest", "Super Baseball Double Play Home Run Derby" )
GAME( 1988, strkzone, 0,       leland,  basebal2, strkzone, ROT0,   "Leland Corp.", "Strike Zone" )

/* large master banks, small slave banks, I86 sound */
GAME( 1987, redlin2p, 0,       redline, redline,  redlin2p, ROT270, "Cinematronics (Tradewest license)", "Redline Racer (2 players)" )
GAME( 1987, quarterb, 0,       quarterb,quarterb, quarterb, ROT270, "Leland Corp.", "Quarterback" )
GAME( 1987, quartrba, quarterb,quarterb,quarterb, quarterb, ROT270, "Leland Corp.", "Quarterback (set 2)" )

/* large master banks, large slave banks, I86 sound */
GAME( 1988, viper,    0,       lelandi, dangerz,  viper,    ROT0,   "Leland Corp.", "Viper" )
GAME( 1988, teamqb,   0,       lelandi, teamqb,   teamqb,   ROT270, "Leland Corp.", "John Elway's Team Quarterback" )
GAME( 1988, teamqb2,  teamqb,  lelandi, teamqb,   teamqb,   ROT270, "Leland Corp.", "John Elway's Team Quarterback (set 2)" )
GAME( 1989, aafb,     0,       lelandi, teamqb,   aafb,     ROT270, "Leland Corp.", "All American Football (rev E)" )
GAME( 1989, aafbd2p,  aafb,    lelandi, aafb2p,   aafbb,    ROT270, "Leland Corp.", "All American Football (rev D, 2 Players)" )
GAME( 1989, aafbb,    aafb,    lelandi, teamqb,   aafbb,    ROT270, "Leland Corp.", "All American Football (rev B)" )

/* huge master banks, large slave banks, I86 sound */
GAME( 1989, offroad,  0,       lelandi, offroad,  offroad,  ROT0,   "Leland Corp.", "Ironman Stewart's Super Off-Road" )
GAME( 1989, offroadt, 0,       lelandi, offroad,  offroadt, ROT0,   "Leland Corp.", "Ironman Stewart's Super Off-Road Track Pack" )
GAME( 1990, pigout,   0,       lelandi, pigout,   pigout,   ROT0,   "Leland Corp.", "Pigout" )
GAME( 1990, pigouta,  pigout,  lelandi, pigout,   pigout,   ROT0,   "Leland Corp.", "Pigout (alternate)" )
