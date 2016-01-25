#include "driver.h"
#include "eeprom.h"

#define VERBOSE 0

#define SERIAL_BUFFER_LENGTH 40

static struct EEPROM_interface *intf;

static int serial_count;
static char serial_buffer[SERIAL_BUFFER_LENGTH];
static unsigned char eeprom_data[256];
static int eeprom_data_bits;
static int eeprom_read_address;
static int eeprom_clock_count;
static int latch,reset_line,clock_line,sending;
static int locked;


void EEPROM_init(struct EEPROM_interface *interface)
{
	intf = interface;

	memset(eeprom_data,0xff,(1 << intf->address_bits) * intf->data_bits / 8);
	serial_count = 0;
	latch = 0;
	reset_line = ASSERT_LINE;
	clock_line = ASSERT_LINE;
	sending = 0;
	if (intf->cmd_unlock) locked = 1;
	else locked = 0;
}

static void EEPROM_write(int bit)
{
#if VERBOSE
logerror("EEPROM write bit %d\n",bit);
#endif

	if (serial_count >= SERIAL_BUFFER_LENGTH-1)
	{
//logerror("error: EEPROM serial buffer overflow\n");
		return;
	}

	serial_buffer[serial_count++] = (bit ? '1' : '0');
	serial_buffer[serial_count] = 0;	/* nul terminate so we can treat it as a string */

	if (intf->cmd_read && serial_count == (strlen(intf->cmd_read) + intf->address_bits) &&
			!strncmp(serial_buffer,intf->cmd_read,strlen(intf->cmd_read)))
	{
		int i,address;

		address = 0;
		for (i = 0;i < intf->address_bits;i++)
		{
			address <<= 1;
			if (serial_buffer[i + strlen(intf->cmd_read)] == '1') address |= 1;
		}
		if (intf->data_bits == 16)
			eeprom_data_bits = (eeprom_data[2*address+0] << 8) + eeprom_data[2*address+1];
		else
			eeprom_data_bits = eeprom_data[address];
		eeprom_read_address = address;
		eeprom_clock_count = 0;
		sending = 1;
		serial_count = 0;
//logerror("EEPROM read %04x from address %02x\n",eeprom_data_bits,address);
	}
	else if (intf->cmd_erase && serial_count == (strlen(intf->cmd_erase) + intf->address_bits) &&
			!strncmp(serial_buffer,intf->cmd_erase,strlen(intf->cmd_erase)))
	{
		int i,address;

		address = 0;
		for (i = 0;i < intf->address_bits;i++)
		{
			address <<= 1;
			if (serial_buffer[i + strlen(intf->cmd_erase)] == '1') address |= 1;
		}
//logerror("EEPROM erase address %02x\n",address);
		if (locked == 0)
		{
			if (intf->data_bits == 16)
			{
				eeprom_data[2*address+0] = 0x00;
				eeprom_data[2*address+1] = 0x00;
			}
			else
				eeprom_data[address] = 0x00;
		}
		else
//logerror("Error: EEPROM is locked\n");
		serial_count = 0;
	}
	else if (intf->cmd_write && serial_count == (strlen(intf->cmd_write) + intf->address_bits + intf->data_bits) &&
			!strncmp(serial_buffer,intf->cmd_write,strlen(intf->cmd_write)))
	{
		int i,address,data;

		address = 0;
		for (i = 0;i < intf->address_bits;i++)
		{
			address <<= 1;
			if (serial_buffer[i + strlen(intf->cmd_write)] == '1') address |= 1;
		}
		data = 0;
		for (i = 0;i < intf->data_bits;i++)
		{
			data <<= 1;
			if (serial_buffer[i + strlen(intf->cmd_write) + intf->address_bits] == '1') data |= 1;
		}
//logerror("EEPROM write %04x to address %02x\n",data,address);
		if (locked == 0)
		{
			if (intf->data_bits == 16)
			{
				eeprom_data[2*address+0] = data >> 8;
				eeprom_data[2*address+1] = data & 0xff;
			}
			else
				eeprom_data[address] = data;
		}
		else
//logerror("Error: EEPROM is locked\n");
		serial_count = 0;
	}
	else if (intf->cmd_lock && serial_count == strlen(intf->cmd_lock) &&
			!strncmp(serial_buffer,intf->cmd_lock,strlen(intf->cmd_lock)))
	{
//logerror("EEPROM lock\n");
		locked = 1;
		serial_count = 0;
	}
	else if (intf->cmd_unlock && serial_count == strlen(intf->cmd_unlock) &&
			!strncmp(serial_buffer,intf->cmd_unlock,strlen(intf->cmd_unlock)))
	{
//logerror("EEPROM unlock\n");
		locked = 0;
		serial_count = 0;
	}
}

static void EEPROM_reset(void)
{
/*if (serial_count)
	logerror("EEPROM reset, buffer = %s\n",serial_buffer);*/

	serial_count = 0;
	sending = 0;
}


void EEPROM_write_bit(int bit)
{
#if VERBOSE
logerror("write bit %d\n",bit);
#endif
	latch = bit;
}

int EEPROM_read_bit(void)
{
	int res;

	if (sending)
		res = (eeprom_data_bits >> intf->data_bits) & 1;
	else res = 1;

#if VERBOSE
logerror("read bit %d\n",res);
#endif

	return res;
}

void EEPROM_set_cs_line(int state)
{
#if VERBOSE
logerror("set reset line %d\n",state);
#endif
	reset_line = state;

	if (reset_line != CLEAR_LINE)
		EEPROM_reset();
}

void EEPROM_set_clock_line(int state)
{
#if VERBOSE
logerror("set clock line %d\n",state);
#endif
	if (state == PULSE_LINE || (clock_line == CLEAR_LINE && state != CLEAR_LINE))
	{
		if (reset_line == CLEAR_LINE)
		{
			if (sending)
			{
				if (eeprom_clock_count == intf->data_bits && intf->enable_multi_read)
				{
					eeprom_read_address = (eeprom_read_address + 1) & ((1 << intf->address_bits) - 1);
					if (intf->data_bits == 16)
						eeprom_data_bits = (eeprom_data[2*eeprom_read_address+0] << 8) + eeprom_data[2*eeprom_read_address+1];
					else
						eeprom_data_bits = eeprom_data[eeprom_read_address];
					eeprom_clock_count = 0;
//logerror("EEPROM read %04x from address %02x\n",eeprom_data_bits,eeprom_read_address);
				}
				eeprom_data_bits = (eeprom_data_bits << 1) | 1;
				eeprom_clock_count++;
			}
			else
				EEPROM_write(latch);
		}
	}

	clock_line = state;
}


void EEPROM_load(void *f)
{
	osd_fread(f,eeprom_data,(1 << intf->address_bits) * intf->data_bits / 8);
}

void EEPROM_save(void *f)
{
	osd_fwrite(f,eeprom_data,(1 << intf->address_bits) * intf->data_bits / 8);
}

void EEPROM_set_data(UINT8 *data, int length)
{
	memcpy(eeprom_data, data, length);
}
