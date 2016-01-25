/***************************************************************************

Shanghai

driver by Nicola Salmoria

this is mostly working, but the HD63484 emulation is incomplete. The continue
yes/no display is wrong, I think this is caused by missing "window" emulation.

Also, the game locks up after you win a round.

***************************************************************************/

#include "driver.h"

/* the on-chip FIFO is 16 bytes long, but we use a larger one to simplify */
/* decoding of long commands. Commands can be up to 64KB long... but Shanghai */
/* doesn't reach that length. */
#define FIFO_LENGTH 50
static int fifo_counter;
static UINT16 fifo[FIFO_LENGTH];
static UINT8 *HD63484_ram;
static UINT16 HD63484_reg[256/2];
static int org,rwp;
static UINT16 cl0,cl1,ccmp;
static INT16 cpx,cpy;


static int instruction_length[64] =
{
	 0, 3, 2, 1,	/* 0x */
	 0, 0,-1, 2,	/* 1x */
	 0, 3, 3, 3,	/* 2x */
	 0, 0, 0, 0,	/* 3x */
	 0, 1, 2, 2,	/* 4x */
	 0, 0, 4, 4,	/* 5x */
	 5, 5, 5, 5,	/* 6x */
	 5, 5, 5, 5,	/* 7x */
	 3, 3, 3, 3, 	/* 8x */
	 3, 3,-2,-2,	/* 9x */
	-2,-2, 2, 4,	/* Ax */
	 5, 5, 7, 7,	/* Bx */
	 3, 3, 1, 1,	/* Cx */
	 2, 2, 2, 2,	/* Dx */
	 5, 5, 5, 5,	/* Ex */
	 5, 5, 5, 5 	/* Fx */
};

int HD63484_start(void)
{
	fifo_counter = 0;
	HD63484_ram = (UINT8*)malloc(0x200000);
	if (!HD63484_ram) return 1;
	memset(HD63484_ram,0,0x200000);
	return 0;
}

void HD63484_stop(void)
{
	free(HD63484_ram);
	HD63484_ram = 0;
}

static void doclr(int opcode,UINT16 fill,int *dst,INT16 _ax,INT16 _ay)
{
	INT16 ax,ay;

	ax = _ax;
	ay = _ay;

	for (;;)
	{
		for (;;)
		{
			switch (opcode & 0x0003)
			{
				case 0:
					HD63484_ram[*dst]  = fill; break;
				case 1:
					HD63484_ram[*dst] |= fill; break;
				case 2:
					HD63484_ram[*dst] &= fill; break;
				case 3:
					HD63484_ram[*dst] ^= fill; break;
			}
			if (ax == 0) break;
			else if (ax > 0)
			{
				*dst = (*dst + 1) & 0x1fffff;
				ax--;
			}
			else
			{
				*dst = (*dst - 1) & 0x1fffff;
				ax++;
			}
		}

		ax = _ax;
		if (_ay < 0)
		{
			*dst = (*dst + 384 - ax) & 0x1fffff;
			if (ay == 0) break;
			ay++;
		}
		else
		{
			*dst = (*dst - 384 - ax) & 0x1fffff;
			if (ay == 0) break;
			ay--;
		}
	}
}

static void docpy(int opcode,int src,int *dst,INT16 _ax,INT16 _ay)
{
	int dstep1,dstep2;
	int ax = _ax;
	int ay = _ay;

	switch (opcode & 0x0700)
	{
		default:
		case 0x0000: dstep1 =  1; dstep2 = -384; break;
		case 0x0100: dstep1 =  1; dstep2 =  384; break;
		case 0x0200: dstep1 = -1; dstep2 = -384; break;
		case 0x0300: dstep1 = -1; dstep2 =  384; break;
		case 0x0400: dstep1 = -384; dstep2 =  1; break;
		case 0x0500: dstep1 =  384; dstep2 =  1; break;
		case 0x0600: dstep1 = -384; dstep2 = -1; break;
		case 0x0700: dstep1 =  384; dstep2 = -1; break;
	}
	dstep2 -= ax * dstep1;

	for (;;)
	{
		for (;;)
		{
			switch (opcode & 0x0007)
			{
				case 0:
					HD63484_ram[*dst]  = HD63484_ram[src]; break;
				case 1:
					HD63484_ram[*dst] |= HD63484_ram[src]; break;
				case 2:
					HD63484_ram[*dst] &= HD63484_ram[src]; break;
				case 3:
					HD63484_ram[*dst] ^= HD63484_ram[src]; break;
				case 4:
					if (HD63484_ram[*dst] == (ccmp & 0xff))
						HD63484_ram[*dst] = HD63484_ram[src];
					break;
				case 5:
					if (HD63484_ram[*dst] != (ccmp & 0xff))
						HD63484_ram[*dst] = HD63484_ram[src];
					break;
				case 6:
					if (HD63484_ram[*dst] < HD63484_ram[src])
						HD63484_ram[*dst] = HD63484_ram[src];
					break;
				case 7:
					if (HD63484_ram[*dst] > HD63484_ram[src])
						HD63484_ram[*dst] = HD63484_ram[src];
					break;
			}

			if (opcode & 0x0800)
			{
				if (ay == 0) break;
				else if (ay > 0)
				{
					src = (src - 384) & 0x1fffff;
					*dst = (*dst + dstep1) & 0x1fffff;
					ay--;
				}
				else
				{
					src = (src + 384) & 0x1fffff;
					*dst = (*dst + dstep1) & 0x1fffff;
					ay++;
				}
			}
			else
			{
				if (ax == 0) break;
				else if (ax > 0)
				{
					src = (src + 1) & 0x1fffff;
					*dst = (*dst + dstep1) & 0x1fffff;
					ax--;
				}
				else
				{
					src = (src - 1) & 0x1fffff;
					*dst = (*dst + dstep1) & 0x1fffff;
					ax++;
				}
			}
		}

		if (opcode & 0x0800)
		{
			ay = _ay;
			if (_ax < 0)
			{
				src = (src - 1 - ay) & 0x1fffff;
				*dst = (*dst + dstep2) & 0x1fffff;
				if (ax == 0) break;
				ax++;
			}
			else
			{
				src = (src + 1 - ay) & 0x1fffff;
				*dst = (*dst + dstep2) & 0x1fffff;
				if (ax == 0) break;
				ax--;
			}
		}
		else
		{
			ax = _ax;
			if (_ay < 0)
			{
				src = (src + 384 - ax) & 0x1fffff;
				*dst = (*dst + dstep2) & 0x1fffff;
				if (ay == 0) break;
				ay++;
			}
			else
			{
				src = (src - 384 - ax) & 0x1fffff;
				*dst = (*dst + dstep2) & 0x1fffff;
				if (ay == 0) break;
				ay--;
			}
		}
	}
}


void HD63484_command_w(UINT16 cmd)
{
	int len;

	fifo[fifo_counter++] = cmd;

	len = instruction_length[fifo[0]>>10];
	if (len == -1)
	{
		if (fifo_counter < 2) return;
		else len = fifo[1]+2;
	}
	else if (len == -2)
	{
		if (fifo_counter < 2) return;
		else len = 2*fifo[1]+2;
	}

	if (fifo_counter >= len)
	{
		//int i;

		//logerror("PC %05x: HD63484 command %s (%04x) ",cpu_get_pc(),instruction_name[fifo[0]>>10],fifo[0]);
		/*for (i = 1;i < fifo_counter;i++)
			logerror("%04x ",fifo[i]);
		logerror("\n");*/

		if (fifo[0] == 0x0400)	/* ORG */
			org = ((fifo[1] & 0x00ff) << 12) | ((fifo[2] & 0xfff0) >> 4);
		else if (fifo[0] == 0x0800)
			cl0 = fifo[1];
		else if (fifo[0] == 0x0801)
			cl1 = fifo[1];
		else if (fifo[0] == 0x0802)
			ccmp = fifo[1];
		else if (fifo[0] == 0x080c)
			rwp = (rwp & 0x00fff) | ((fifo[1] & 0x00ff) << 12);
		else if (fifo[0] == 0x080d)
			rwp = (rwp & 0xff000) | ((fifo[1] & 0xfff0) >> 4);
		else if (fifo[0] == 0x4800)	/* WT */
		{
			HD63484_ram[2*rwp]   = fifo[1] & 0x00ff ;
			HD63484_ram[2*rwp+1] = (fifo[1] & 0xff00) >> 8;
			rwp = (rwp + 1) & 0xfffff;
		}
		else if (fifo[0] == 0x5800)	/* CLR */
		{
rwp *= 2;
			doclr(fifo[0],fifo[1],&rwp,2*fifo[2]+1,fifo[3]);
rwp /= 2;
		}
		else if ((fifo[0] & 0xfffc) == 0x5c00)	/* SCLR */
		{
rwp *= 2;
			doclr(fifo[0],fifo[1],&rwp,2*fifo[2]+1,fifo[3]);
rwp /= 2;
		}
		else if ((fifo[0] & 0xf0ff) == 0x6000)	/* CPY */
		{
			int src;

			src = ((fifo[1] & 0x00ff) << 12) | ((fifo[2] & 0xfff0) >> 4);
rwp *= 2;
			docpy(fifo[0],2*src,&rwp,2*fifo[3]+1,fifo[4]);
rwp /= 2;
		}
		else if ((fifo[0] & 0xf0fc) == 0x7000)	/* SCPY */
		{
			int src;

			src = ((fifo[1] & 0x00ff) << 12) | ((fifo[2] & 0xfff0) >> 4);
rwp *= 2;
			docpy(fifo[0],2*src,&rwp,2*fifo[3]+1,fifo[4]);
rwp /= 2;
		}
		else if (fifo[0] == 0x8000)	/* AMOVE */
		{
			cpx = fifo[1];
			cpy = fifo[2];
		}
//		else if ((fifo[0] & 0xff00) == 0xc000)	/* AFRCT */
		else if ((fifo[0] & 0xfff8) == 0xc000)	/* AFRCT */
		{
			INT16 pcx,pcy;
			INT16 ax,ay;
			int dst;

			pcx = fifo[1];
			pcy = fifo[2];
			ax = pcx - cpx;
			ay = pcy - cpy;
			dst = (2*org + cpx - cpy * 384) & 0x1fffff;

			for (;;)
			{
				for (;;)
				{
					switch (fifo[0] & 0x0007)
					{
						case 0:
							HD63484_ram[dst]  = cl0; break;
						case 1:
							HD63484_ram[dst] |= cl0; break;
						case 2:
							HD63484_ram[dst] &= cl0; break;
						case 3:
							HD63484_ram[dst] ^= cl0; break;
						case 4:
							if (HD63484_ram[dst] == (ccmp & 0xff))
								HD63484_ram[dst] = cl0;
							break;
						case 5:
							if (HD63484_ram[dst] != (ccmp & 0xff))
								HD63484_ram[dst] = cl0;
							break;
						case 6:
							if (HD63484_ram[dst] < (cl0 & 0xff))
								HD63484_ram[dst] = cl0;
							break;
						case 7:
							if (HD63484_ram[dst] > (cl0 & 0xff))
								HD63484_ram[dst] = cl0;
							break;
					}

					if (ax == 0) break;
					else if (ax > 0)
					{
						dst = (dst + 1) & 0x1fffff;
						ax--;
					}
					else
					{
						dst = (dst - 1) & 0x1fffff;
						ax++;
					}
				}

				ax = pcx - cpx;
				if (pcy < cpy)
				{
					dst = (dst + 384 - ax) & 0x1fffff;
					if (ay == 0) break;
					ay++;
				}
				else
				{
					dst = (dst - 384 - ax) & 0x1fffff;
					if (ay == 0) break;
					ay--;
				}
			}
		}
//		else if ((fifo[0] & 0xf000) == 0xe000)	/* AGCPY */
		else if ((fifo[0] & 0xf0f8) == 0xe000)	/* AGCPY */
		{
			INT16 pcx,pcy;
			int src,dst;

			pcx = fifo[1];
			pcy = fifo[2];
			src = (2*org + pcx - pcy * 384) & 0x1fffff;
			dst = (2*org + cpx - cpy * 384) & 0x1fffff;

			docpy(fifo[0],src,&dst,fifo[3],fifo[4]);

			cpx = (dst - 2*org) % 384;
			cpy = (dst - 2*org) / 384;
		}
/*		else
logerror("unsupported command\n");*/

		fifo_counter = 0;
	}
}

static int regno;

static READ_HANDLER( HD63484_status_r )
{
	if (offset == 1) return 0xff;	/* high 8 bits - not used */

	//if (cpu_get_pc() != 0xfced6) logerror("%05x: HD63484 status read\n",cpu_get_pc());
	return 0x22;	/* write FIFO ready + command end */
}

static WRITE_HANDLER( HD63484_address_w )
{
	static unsigned char reg[2];

	reg[offset] = data;
	regno = reg[0];	/* only low 8 bits are used */
//if (offset == 0)
//	logerror("PC %05x: HD63484 select register %02x\n",cpu_get_pc(),regno);
}

static WRITE_HANDLER( HD63484_data_w )
{
	static unsigned char dat[2];

	dat[offset] = data;
	if (offset == 1)
	{
		int val = dat[0] + 256 * dat[1];

		if (regno == 0)	/* FIFO */
			HD63484_command_w(val);
		else
		{
//logerror("PC %05x: HD63484 register %02x write %04x\n",cpu_get_pc(),regno,val);
			HD63484_reg[regno/2] = val;
			if (regno & 0x80) regno += 2;	/* autoincrement */
		}
	}
}

static READ_HANDLER( HD63484_data_r )
{
	int res;

	if (regno == 0x80)
	{
		res = cpu_getscanline();
	}
	else
	{
//logerror("%05x: HD63484 read register %02x\n",cpu_get_pc(),regno);
		res = 0;
	}

	if (offset == 0)
		return res & 0xff;
	else
		return (res >> 8) & 0xff;
}




void shanghai_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
		bit0 = (i >> 2) & 0x01;
		bit1 = (i >> 3) & 0x01;
		bit2 = (i >> 4) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (i >> 5) & 0x01;
		bit1 = (i >> 6) & 0x01;
		bit2 = (i >> 7) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (i >> 0) & 0x01;
		bit2 = (i >> 1) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	}
}

int shanghai_vh_start(void)
{
	return HD63484_start();
}

void shanghai_vh_stop(void)
{
	HD63484_stop();
}

void shanghai_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int x,y,b;
	static int base,bose=256;
if (keyboard_pressed_memory(KEYCODE_Z)) base -= 384/2*280;
if (keyboard_pressed_memory(KEYCODE_X)) base += 384/2*280;
if (keyboard_pressed_memory(KEYCODE_C)) bose--;
if (keyboard_pressed_memory(KEYCODE_V)) bose++;

	b = 2 * (base + ((HD63484_reg[0xcc/2] & 0x001f) << 16) + HD63484_reg[0xce/2]);
	for (y = 0;y < 280;y++)
	{
		for (x = 0;x < 384;x++)
		{
			b &= 0x1fffff;
			plot_pixel(bitmap,x,y,Machine->pens[HD63484_ram[b]]);
			b++;
		}
	}

	if ((HD63484_reg[0x06/2] & 0x0300) == 0x0300)
	{
		b = 2 * (base + ((HD63484_reg[0xdc/2] & 0x001f) << 16) + HD63484_reg[0xde/2]);
		for (y = 0;y < 280;y++)
		{
			for (x = 0;x < 384;x++)
			{
				b &= 0x1fffff;
				if (HD63484_ram[b])
					plot_pixel(bitmap,x,y,Machine->pens[HD63484_ram[b]]);
				b++;
			}
		}
	}
}


static int shanghai_interrupt(void)
{
	interrupt_vector_w(0,0x80);
	return interrupt();
}

static WRITE_HANDLER( shanghai_coin_w )
{
	coin_counter_w(0,data & 1);
	coin_counter_w(1,data & 2);
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x00000, 0x03fff, MRA_RAM },
	{ 0xa0000, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x00000, 0x03fff, MWA_RAM },
	{ 0xa0000, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x01, HD63484_status_r },
	{ 0x02, 0x03, HD63484_data_r },
	{ 0x20, 0x20, YM2203_status_port_0_r },
	{ 0x22, 0x22, YM2203_read_port_0_r },
	{ 0x40, 0x40, input_port_0_r },
	{ 0x44, 0x44, input_port_1_r },
	{ 0x48, 0x48, input_port_2_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x01, HD63484_address_w },
	{ 0x02, 0x03, HD63484_data_w },
	{ 0x20, 0x20, YM2203_control_port_0_w },
	{ 0x22, 0x22, YM2203_write_port_0_w },
	{ 0x4c, 0x4c, shanghai_coin_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( shanghai )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Confirmation" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x02, "Help" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "2 Players Move Time" )
	PORT_DIPSETTING(    0x0c, "8" )
	PORT_DIPSETTING(    0x08, "10" )
	PORT_DIPSETTING(    0x04, "12" )
	PORT_DIPSETTING(    0x00, "14" )
	PORT_DIPNAME( 0x30, 0x30, "Bonus Time for Making Pair" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0xc0, 0xc0, "Start Time" )
	PORT_DIPSETTING(    0xc0, "30" )
	PORT_DIPSETTING(    0x80, "60" )
	PORT_DIPSETTING(    0x40, "90" )
	PORT_DIPSETTING(    0x00, "120" )
INPUT_PORTS_END



static struct YM2203interface ym2203_interface =
{
	1,			/* 1 chip */
	1500000,	/* ??? */
	{ YM2203_VOL(80,15) },
	{ input_port_3_r },
	{ input_port_4_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_shanghai =
{
	/* basic machine hardware */
	{
		{
			CPU_V30,
			16000000,	/* ??? */
			readmem,writemem,readport,writeport,
			shanghai_interrupt,1,
			0,0
		}
	},
	30, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	384, 280, { 0, 384-1, 0, 280-1 },
	0,
	256,0,
	shanghai_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	shanghai_vh_start,
	shanghai_vh_stop,
	shanghai_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( shanghai )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_V20_EVEN( "shg-22a.rom", 0xa0000, 0x10000, 0xe0a085be )
	ROM_LOAD_V20_ODD ( "shg-21a.rom", 0xa0000, 0x10000, 0x4ab06d32 )
	ROM_LOAD_V20_EVEN( "shg-28a.rom", 0xc0000, 0x10000, 0x983ec112 )
	ROM_LOAD_V20_ODD ( "shg-27a.rom", 0xc0000, 0x10000, 0x41af0945 )
	ROM_LOAD_V20_EVEN( "shg-37b.rom", 0xe0000, 0x10000, 0x3f192da0 )
	ROM_LOAD_V20_ODD ( "shg-36b.rom", 0xe0000, 0x10000, 0xa1d6af96 )
ROM_END



GAMEX( 1988, shanghai, 0, shanghai, shanghai, 0, ROT0, "Sunsoft", "Shanghai", GAME_NOT_WORKING )
