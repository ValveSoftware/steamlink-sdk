/*******************************************************************************

Data East machine functions - Bryan McPhail, mish@tendril.co.uk

* Control reads, protection chip emulations & cycle skipping patches

*******************************************************************************/

#include "driver.h"

extern unsigned char *dec0_ram;
static int GAME,i8751_return,slyspy_state;

WRITE_HANDLER( dec0_pf1_control_0_w );
WRITE_HANDLER( dec0_pf1_control_1_w );
WRITE_HANDLER( dec0_pf1_rowscroll_w );
WRITE_HANDLER( dec0_pf1_colscroll_w );
WRITE_HANDLER( dec0_pf1_data_w );
READ_HANDLER( dec0_pf1_data_r );
WRITE_HANDLER( dec0_pf2_control_0_w );
WRITE_HANDLER( dec0_pf2_control_1_w );
WRITE_HANDLER( dec0_pf2_rowscroll_w );
WRITE_HANDLER( dec0_pf2_colscroll_w );
WRITE_HANDLER( dec0_pf2_data_w );
READ_HANDLER( dec0_pf2_data_r );
WRITE_HANDLER( dec0_pf3_control_1_w );
WRITE_HANDLER( dec0_pf3_control_0_w );
WRITE_HANDLER( dec0_pf3_data_w );

/******************************************************************************/

READ_HANDLER( dec0_controls_r )
{
	switch (offset)
	{
		case 0: /* Player 1 & 2 joystick & buttons */
			return (readinputport(0) + (readinputport(1) << 8));

		case 2: /* Credits, start buttons */
			return readinputport(2);

		case 4: /* Byte 4: Dipswitch bank 2, Byte 5: Dipswitch Bank 1 */
			return (readinputport(3) + (readinputport(4) << 8));

		case 8: /* Intel 8751 mc, Bad Dudes & Heavy Barrel only */
			//logerror("CPU #0 PC %06x: warning - read unmapped memory address %06x\n",cpu_get_pc(),0x30c000+offset);
			return i8751_return;
	}

	//logerror("CPU #0 PC %06x: warning - read unmapped memory address %06x\n",cpu_get_pc(),0x30c000+offset);
	return 0xffff;
}

/******************************************************************************/

READ_HANDLER( dec0_rotary_r )
{
	switch (offset)
	{
		case 0: /* Player 1 rotary */
			return ~(1 << (readinputport(5) * 12 / 256));

		case 8: /* Player 2 rotary */
			return ~(1 << (readinputport(6) * 12 / 256));

		default:
			//logerror("Unknown rotary read at 300000 %02x\n",offset);
			break;
	}

	return 0;
}

/******************************************************************************/

READ_HANDLER( midres_controls_r )
{
	switch (offset)
	{
		case 0: /* Player 1 Joystick + start, Player 2 Joystick + start */
			return (readinputport(0) + (readinputport(1) << 8));

		case 2: /* Dipswitches */
			return (readinputport(3) + (readinputport(4) << 8));

		case 4: /* Player 1 rotary */
			return ~(1 << (readinputport(5) * 12 / 256));

		case 6: /* Player 2 rotary */
			return ~(1 << (readinputport(6) * 12 / 256));

		case 8: /* Credits, start buttons */
			return readinputport(2);

		case 12:
			return 0;	/* ?? watchdog ?? */
	}

	//logerror("PC %06x unknown control read at %02x\n",cpu_get_pc(),0x180000+offset);
	return 0xffff;
}

/******************************************************************************/

READ_HANDLER( slyspy_controls_r )
{
	switch (offset)
	{
		case 0: /* Dip Switches */
			return (readinputport(3) + (readinputport(4) << 8));

		case 2: /* Player 1 & Player 2 joysticks & fire buttons */
			return (readinputport(0) + (readinputport(1) << 8));

		case 4: /* Credits */
			return readinputport(2);
	}

	//logerror("Unknown control read at 30c000 %d\n",offset);
	return 0xffff;
}

READ_HANDLER( slyspy_protection_r )
{
	/* These values are for Boulderdash, I have no idea what they do in Slyspy */
	switch (offset) {
		case 0: 	return 0;
		case 2: 	return 0x13;
		case 4:		return 0;
		case 6:		return 0x2;
	}

	//logerror("%04x, Unknown protection read at 30c000 %d\n",cpu_get_pc(),offset);
	return 0;
}

/*
	The memory map in Sly Spy can change between 4 states according to the protection!

	Default state (called by Traps 1, 3, 4, 7, C)

	240000 - 24001f = control   (Playfield 2 area)
	242000 - 24207f = colscroll
	242400 - 2425ff = rowscroll
	246000 - 2467ff = data

	248000 - 24801f = control  (Playfield 1 area)
	24c000 - 24c07f = colscroll
	24c400 - 24c4ff = rowscroll
	24e000 - 24e7ff = data

	State 1 (Called by Trap 9) uses this memory map:

	248000 = pf1 data
	24c000 = pf2 data

	State 2 (Called by Trap A) uses this memory map:

	240000 = pf2 data
	242000 = pf1 data
	24e000 = pf1 data

	State 3 (Called by Trap B) uses this memory map:

	240000 = pf1 data
	248000 = pf2 data

*/

WRITE_HANDLER( slyspy_state_w )
{
	slyspy_state=0;
}

READ_HANDLER( slyspy_state_r )
{
	slyspy_state++;
	slyspy_state=slyspy_state%4;
	return 0; /* Value doesn't mater */
}

WRITE_HANDLER( slyspy_240000_w )
{
	switch (slyspy_state) {
		case 0x3:
			dec0_pf1_data_w(offset,data);
			return;
		case 0x2:
			dec0_pf2_data_w(offset,data);
			return;
		case 0x0:
			if (offset<0x10) dec0_pf2_control_0_w(offset,data);
			else if (offset<0x20) dec0_pf2_control_1_w(offset-0x10,data);
			return;
	}
	//logerror("Wrote to 240000 %02x at %04x %04x (Trap %02x)\n",offset,cpu_get_pc(),data,slyspy_state);
}

WRITE_HANDLER( slyspy_242000_w )
{
	switch (slyspy_state) {
		case 0x2: /* Trap A */
			dec0_pf1_data_w(offset,data);
			return;
		case 0x0: /* Trap C */
			if (offset<0x80) dec0_pf2_colscroll_w(offset,data);
			else if (offset<0x600) dec0_pf2_rowscroll_w(offset-0x400,data);
			return;
	}
	//logerror("Wrote to 242000 %02x at %04x %04x (Trap %02x)\n",offset,cpu_get_pc(),data,slyspy_state);
}

WRITE_HANDLER( slyspy_246000_w )
{
	switch (slyspy_state) {
		case 0x0:
			dec0_pf2_data_w(offset,data);
			return;
	}
	//logerror("Wrote to 246000 %02x at %04x %04x (Trap %02x)\n",offset,cpu_get_pc(),data,slyspy_state);
}

WRITE_HANDLER( slyspy_248000_w )
{
	switch (slyspy_state) {
		case 0x1:
			dec0_pf1_data_w(offset,data);
			return;
		case 0x3:
			dec0_pf2_data_w(offset,data);
			return;
		case 0x0:
			if (offset<0x10) dec0_pf1_control_0_w(offset,data);
			else if (offset<0x20) dec0_pf1_control_1_w(offset-0x10,data);
			return;
	}
	//logerror("Wrote to 248000 %02x at %04x %04x (Trap %02x)\n",offset,cpu_get_pc(),data,slyspy_state);
}

WRITE_HANDLER( slyspy_24c000_w )
{
	switch (slyspy_state) {
		case 0x1: /* Trap 9 */
			dec0_pf2_data_w(offset,data);
			return;
		case 0x0: /* Trap C */
			if (offset<0x80) dec0_pf1_colscroll_w(offset,data);
			else if (offset<0x600) dec0_pf1_rowscroll_w(offset-0x400,data);
			return;
	}
	//logerror("Wrote to 24c000 %02x at %04x %04x (Trap %02x)\n",offset,cpu_get_pc(),data,slyspy_state);
}

WRITE_HANDLER( slyspy_24e000_w )
{
	switch (slyspy_state) {
		case 0x2:
		case 0x0:
			dec0_pf1_data_w(offset,data);
			return;
	}
	//logerror("Wrote to 24e000 %02x at %04x %04x (Trap %02x)\n",offset,cpu_get_pc(),data,slyspy_state);
}

/******************************************************************************/

static int share[0xff];
static int hippodrm_msb,hippodrm_lsb;

READ_HANDLER( hippodrm_prot_r )
{
//logerror("6280 PC %06x - Read %06x\n",cpu_getpc(),offset+0x1d0000);
	if (hippodrm_lsb==0x45) return 0x4e;
	if (hippodrm_lsb==0x92) return 0x15;
	return 0;
}

WRITE_HANDLER( hippodrm_prot_w )
{
	switch (offset) {
		case 4:	hippodrm_msb=data; break;
		case 5:	hippodrm_lsb=data; break;
	}
//logerror("6280 PC %06x - Wrote %06x to %04x\n",cpu_getpc(),data,offset+0x1d0000);
}

READ_HANDLER( hippodrm_shared_r )
{
	return share[offset];
}

WRITE_HANDLER( hippodrm_shared_w )
{
	share[offset]=data;
}

static READ_HANDLER( hippodrm_68000_share_r )
{
	if (offset==0) cpu_yield(); /* A wee helper */
	return share[offset/2];
}

static WRITE_HANDLER( hippodrm_68000_share_w )
{
	share[offset/2]=data&0xff;
}

/******************************************************************************/

static void hbarrel_i8751_write(int data)
{
	static int level,state;

	int title[]={  1, 2, 5, 6, 9,10,13,14,17,18,21,22,25,26,29,30,33,34,37,38,41,42,0,
                 3, 4, 7, 8,11,12,15,16,19,20,23,24,27,28,31,32,35,36,39,40,43,44,0,
                45,46,49,50,53,54,57,58,61,62,65,66,69,70,73,74,77,78,81,82,0,
                47,48,51,52,55,56,59,60,63,64,67,68,71,72,75,76,79,80,83,84,0,
                85,86,89,90,93,94,97,98,101,102,105,106,109,110,113,114,117,118,121,122,125,126,0,
                87,88,91,92,95,96,99,100,103,104,107,108,111,112,115,116,119,120,123,124,127,128,0,
                129,130,133,134,137,138,141,142,145,146,149,150,153,154,157,158,161,162,165,166,169,170,173,174,0,
                131,132,135,136,139,140,143,144,147,148,151,152,155,156,159,160,163,164,167,168,171,172,175,176,0,
                0x10b1,0x10b2,0,0x10b3,0x10b4,-1
	};

	/* This table is from the USA version - others could be different.. */
	int weapons_table[][0x20]={
		{ 0x558,0x520,0x5c0,0x600,0x520,0x540,0x560,0x5c0,0x688,0x688,0x7a8,0x850,0x880,0x880,0x990,0x9b0,0x9b0,0x9e0,0xffff }, /* Level 1 */
		{ 0x330,0x370,0x3d8,0x580,0x5b0,0x640,0x6a0,0x8e0,0x8e0,0x940,0x9f0,0xa20,0xa50,0xa80,0xffff }, /* Level 2 */
		{ 0xb20,0xbd0,0xb20,0xb20,0xbd8,0xb50,0xbd8,0xb20,0xbe0,0xb40,0xb80,0xa18,0xa08,0xa08,0x980,0x8e0,0x780,0x790,0x650,0x600,0x5d0,0x5a0,0x570,0x590,0x5e0,0xffff }, /* Level 3 */
		{ 0x530,0x5d0,0x5e0,0x5c8,0x528,0x520,0x5d8,0x5e0,0x5d8,0x540,0x570,0x5a0,0x658,0x698,0x710,0x7b8,0x8e0,0x8e0,0x8d8,0x818,0x8e8,0x820,0x8e0,0x848,0x848,0xffff }, /* Level 4 */
		{ 0x230,0x280,0x700,0x790,0x790,0x7e8,0x7e8,0x8d0,0x920,0x950,0xad0,0xb90,0xb50,0xb10,0xbe0,0xbe0,0xffff }, /* Level 5 */
		{ 0xd20,0xde0,0xd20,0xde0,0xd80,0xd80,0xd90,0xdd0,0xdb0,0xb20,0xa40,0x9e0,0x960,0x8a0,0x870,0x840,0x7e0,0x7b0,0x780,0xffff }, /* Level 6 */
		{ 0x730,0x7e0,0x720,0x7e0,0x740,0x7c0,0x730,0x7d0,0x740,0x7c0,0x730,0x7d0,0x720,0x7e0,0x720,0x7e0,0x720,0x7e0,0x720,0x7e0,0x730,0x7d0,0xffff } /* Level 7 */
	};

	switch (data>>8) {
		case 0x2:	/* Selects level */
			i8751_return=level;
			break;
		case 0x3:	/* Increment level counter */
			level++;
			i8751_return=0x301;
			break;
		case 0x5:	/* Set level 0 */
			i8751_return=0xb3b;
			level=0;
			break;
		case 0x06:	/* Controls appearance & placement of special weapons */
			i8751_return=weapons_table[level][data&0x1f];
			//logerror("CPU #0 PC %06x: warning - write %02x to i8751, returning %04x\n",cpu_get_pc(),data,i8751_return);
			break;
		case 0xb:	/* Initialise the variables? */
			i8751_return=0;
			break;
		default:
			i8751_return=0;
	}

	/* Protection */
	if (data==7) i8751_return=0xc000; /* Stack pointer */
	if (data==0x175) i8751_return=0x68b; /* ID check - USA version */
	if (data==0x174) i8751_return=0x68c; /* ID check - World version */

	/* All commands in range 4xx are related to title screen.. */
	if (data==0x4ff) state=0;
	if (data>0x3ff && data<0x4ff) {
		state++;

		if (title[state-1]==0) i8751_return=0xfffe;
		else if (title[state-1]==-1) i8751_return=0xffff;
		else if (title[state-1]>0x1000) i8751_return=(title[state-1]&0xfff)+128+15;
		else i8751_return=title[state-1]+128+15+0x2000;

		/* We have to use a state as the microcontroller remembers previous commands */
	}

//logerror("CPU #0 PC %06x: warning - write %02x to i8751\n",cpu_get_pc(),data);
}

static void baddudes_i8751_write(int data)
{
	i8751_return=0;

	switch (data&0xffff) {
		case 0x714: i8751_return=0x700; break;
		case 0x73b: i8751_return=0x701; break;
		case 0x72c: i8751_return=0x702; break;
		case 0x73f: i8751_return=0x703; break;
		case 0x755: i8751_return=0x704; break;
		case 0x722: i8751_return=0x705; break;
		case 0x72b: i8751_return=0x706; break;
		case 0x724: i8751_return=0x707; break;
		case 0x728: i8751_return=0x708; break;
		case 0x735: i8751_return=0x709; break;
		case 0x71d: i8751_return=0x70a; break;
		case 0x721: i8751_return=0x70b; break;
		case 0x73e: i8751_return=0x70c; break;
		case 0x761: i8751_return=0x70d; break;
		case 0x753: i8751_return=0x70e; break;
		case 0x75b: i8751_return=0x70f; break;
	}

	//if (!i8751_return) logerror("%04x: warning - write unknown command %02x to 8571\n",cpu_get_pc(),data);
}

static void birdtry_i8751_write(int data)
{
	i8751_return=0;

	//logerror("%04x: warning - write unknown command %02x to 8571\n",cpu_get_pc(),data);

if ((data&0xff00)==0x200) i8751_return=0x300;
if ((data&0xff00)==0x300) i8751_return=0x200;


//i8751_return=0x200;
	return;
/*
	switch (data&0xffff) {
		case 0x714: i8751_return=0x700; break;
		case 0x73b: i8751_return=0x701; break;
		case 0x72c: i8751_return=0x702; break;
		case 0x73f: i8751_return=0x703; break;
		case 0x755: i8751_return=0x704; break;
		case 0x722: i8751_return=0x705; break;
		case 0x72b: i8751_return=0x706; break;
		case 0x724: i8751_return=0x707; break;
		case 0x728: i8751_return=0x708; break;
		case 0x735: i8751_return=0x709; break;
		case 0x71d: i8751_return=0x70a; break;
		case 0x721: i8751_return=0x70b; break;
		case 0x73e: i8751_return=0x70c; break;
		case 0x761: i8751_return=0x70d; break;
		case 0x753: i8751_return=0x70e; break;
		case 0x75b: i8751_return=0x70f; break;
	}

	if (!i8751_return) logerror("%04x: warning - write unknown command %02x to 8571\n",cpu_get_pc(),data);
*/
}

static void *i8751_timer;

void dec0_i8751_write(int data)
{
	/* Writes to this address cause an IRQ to the i8751 microcontroller */
	if (GAME==1) hbarrel_i8751_write(data);
	if (GAME==2) baddudes_i8751_write(data);
	if (GAME==3) birdtry_i8751_write(data);

	cpu_cause_interrupt(0,5);

	/* Simulate the processing time of the i8751, time value is guessed
	if (i8751_timer) {
		logerror("i8751:  Missed a timer!!!\n");
	}
	else
		i8751_timer = timer_set(TIME_NOW, 0, i8751_callback);*/

/* There is a timing problem in Heavy Barrel if the processing time is not
simulated - if the interrupt is triggered straight away then HB will reset
at the end of level one, the processing time needs to be at least the cycles
of a TST.W + BMI.S (ie, not very much at all).

See the code about 0xb60 (USA version)

*/

//logerror("CPU #0 PC %06x: warning - write %02x to i8751\n",cpu_get_pc(),data);


}

void dec0_i8751_reset(void)
{
	i8751_return=0;
}

/******************************************************************************/

static READ_HANDLER( hbarrel_cycle_r )
{
	if (cpu_get_pc()==0x130ca) {cpu_spinuntil_int(); return 0;} return READ_WORD(&dec0_ram[0x10]);
}

static READ_HANDLER( hbarrelu_cycle_r )
{
	if (cpu_get_pc()==0x131a0) {cpu_spinuntil_int(); return 0;} return READ_WORD(&dec0_ram[0x10]);
}

static WRITE_HANDLER( sprite_mirror_w )
{
	extern unsigned char *spriteram;
	WRITE_WORD (&spriteram[offset], data);
}

/******************************************************************************/

static void hbarrel_custom_memory(void)
{
	install_mem_read_handler(0, 0xff8010, 0xff8011, hbarrel_cycle_r);
	GAME=1;
{ /* Remove this patch once processing time of i8751 is simulated */
unsigned char *RAM = memory_region(REGION_CPU1);
WRITE_WORD (&RAM[0xb3e],0x8008);
}
}

static void hbarrelu_custom_memory(void)
{
	install_mem_read_handler(0, 0xff8010, 0xff8011, hbarrelu_cycle_r);
	GAME=1;

{ /* Remove this patch once processing time of i8751 is simulated */
unsigned char *RAM = memory_region(REGION_CPU1);
WRITE_WORD (&RAM[0xb68],0x8008);
}
}

static void robocop_custom_memory(void)
{
	/* To do */
}

static void hippodrm_custom_memory(void)
{
	install_mem_read_handler(0, 0x180000, 0x180fff, hippodrm_68000_share_r);
	install_mem_write_handler(0, 0x180000, 0x180fff, hippodrm_68000_share_w);
	install_mem_write_handler(0, 0xffc800, 0xffcfff, sprite_mirror_w);
}

void dec0_custom_memory(void)
{
	GAME=0;
	i8751_timer=NULL;

	if (!strcmp(Machine->gamedrv->name,"hbarrelw")) hbarrel_custom_memory();
	if (!strcmp(Machine->gamedrv->name,"hbarrel")) hbarrelu_custom_memory();
	if (!strcmp(Machine->gamedrv->name,"baddudes")) GAME=2;
	if (!strcmp(Machine->gamedrv->name,"drgninja"))	GAME=2;
	if (!strcmp(Machine->gamedrv->name,"birdtry")) 	GAME=3;
	if (!strcmp(Machine->gamedrv->name,"robocop")) robocop_custom_memory();
	if (!strcmp(Machine->gamedrv->name,"hippodrm")) hippodrm_custom_memory();
	if (!strcmp(Machine->gamedrv->name,"ffantasy")) hippodrm_custom_memory();
}
