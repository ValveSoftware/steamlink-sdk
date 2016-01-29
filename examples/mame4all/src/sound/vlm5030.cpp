/*
	vlm5030.c

	VLM5030 emulator (preliminary)

	Written by Tatsuyuki Satoh

  note:
	memory read cycle(sampling rate ?) = 122.9u(440clock)
	interpolator (LC8109 = 2.5ms)      = 20 * samples(125us)
	frame time (20ms)                  =  8 * interpolator

----------- command format (Analytical result) ----------

1)end of speech (8bit)
:00000011:

2)silent some frame (8bit)
:????LL01:

LL : number of silent frames
   00 = 2 frame
   01 = 4 frame
   10 = 6 frame
   11 = 8 frame

3)play one frame (48bit/frame)
: 1st    :   2nd  :   3rd  :   4th  :  5th   :   6th  :
:EEPPPPP0:99AAAEEE:67778889:44455566:22233334:11111112:

energy and pitch bits are MSB first.

EEEEE  : energy ( volume 0=off,0x1f=max)
PPPPP  : pitch  (0=noize?, 1=fast,0x1f=slow)
1111111: stage 1?
2222   : stage 2?
3333   : stage 3?
4444   : stage 4?
555    : stage 5?
666    : stage 6?
777    : stage 7?
888    : stage 8?
999    : stage 9?
AAA    : stage 10?

 ---------- chirp table information ----------

digital filter sampling rate = 88 systemclock = 40.6KHz
sampling clock = 88systemclock(40.6KHz)
one chirp      = 5 sampling clocks = 440systemclock(8.12KHz)

chirp  0   : volume 10- 8 : with filter
chirp  1   : volume  8- 6 : with filter
chirp  2   : volume  6- 4 : with filter
chirp  3   : volume   4   : no filter ??
chirp  4- 5: volume  4- 2 : with filter
chirp  6-11: volume  2- 0 : with filter
chirp 12-..: vokume   0   : silent

 ---------- pitch table information ----------
0 = random
1 = 22stage(2700usec)
 2-09 1stage(120usec)
0a-11 2stage(240usec)
12-19 4stage(480usec)
1A-1E 8stage(960usec)

*/
#include "driver.h"
#include "vlm5030.h"

#define IP_SIZE 20		/* samples per interpolator */
#define FR_SIZE 8		/* interpolator per frame   */

static const struct VLM5030interface *intf;

static int channel;
static int schannel;

static unsigned char *VLM5030_rom;
static int VLM5030_address_mask;
static int VLM5030_address;
static int pin_BSY;
static int pin_ST;
static int pin_RST;
static int latch_data = 0;
static int sampling_mode;

static int table_h;

#define PH_RESET 0
#define PH_IDLE  1
#define PH_SETUP 2
#define PH_WAIT  3
#define PH_RUN   4
#define PH_STOP  5
static int phase;

/* these contain data describing the current and previous voice frames */
static unsigned short old_energy = 0;
static unsigned short old_pitch = 0;
static int old_k[10] = {0,0,0,0,0,0,0,0,0,0};

static unsigned short new_energy = 0;
static unsigned short new_pitch = 0;
static int new_k[10] = {0,0,0,0,0,0,0,0,0,0};

/* these are all used to contain the current state of the sound generation */
static unsigned short current_energy = 0;
static unsigned short current_pitch = 0;
static int current_k[10] = {0,0,0,0,0,0,0,0,0,0};

static unsigned short target_energy = 0;
static unsigned short target_pitch = 0;
static int target_k[10] = {0,0,0,0,0,0,0,0,0,0};

static int interp_count = 0;       /* number of interp periods (0-7) */
static int sample_count = 0;       /* sample number within interp (0-19) */
static int pitch_count = 0;

static int u[11] = {0,0,0,0,0,0,0,0,0,0,0};
static int x[10] = {0,0,0,0,0,0,0,0,0,0};

/* ROM Tables */


/* This is the energy lookup table */
/* !!!!!!!!!! preliminary !!!!!!!!!! */
static unsigned short energytable[0x20];

/* This is the pitch lookup table */
static const unsigned char pitchtable [0x20]=
{
   0,                               /* 0     : random mode */
   22,                              /* 1     : start=22    */
   23, 24, 25, 26, 27, 28, 29, 30,  /*  2- 9 : 1step       */
   32, 34, 36, 38, 40, 42, 44, 46,  /* 10-17 : 2step       */
   50, 54, 58, 62, 66, 70, 74, 78,  /* 18-25 : 4step       */
   86, 94, 102,110,118,             /* 26-30 : 8step       */
   255                              /* 31    : only one time ?? */
};

/* These are the reflection coefficient lookup tables */
/* 2's comp. */

/* !!!!!!!!!! preliminary !!!!!!!!!! */

/* 7bit */
#define K1_RANGE  0x6000
/* 4bit */
#define K2_RANGE  0x4000
#define K3_RANGE  0x6000
#define K4_RANGE  0x4000
/* 3bit */
#define K5_RANGE  0x6000
#define K6_RANGE  0x6000
#define K7_RANGE  0x5000
#define K8_RANGE  0x4000
#define K9_RANGE  0x5000
#define K10_RANGE 0x4000

static int k1table[0x80];
static int k2table[0x10];
static int k3table[0x10];
static int k4table[0x10];
static int k5table[0x08];
static int k6table[0x08];
static int k7table[0x08];
static int k8table[0x08];
static int k9table[0x08];
static int k10table[0x08];

/* chirp table */
static unsigned char chirptable[12]=
{
  0xff*9/10,
  0xff*7/10,
  0xff*5/10,
  0xff*4/10, /* non digital filter ? */
  0xff*3/10,
  0xff*3/10,
  0xff*1/10,
  0xff*1/10,
  0xff*1/10,
  0xff*1/10,
  0xff*1/10,
  0xff*1/10
};

/* interpolation coefficients */
static int interp_coeff[8] = {
//8, 8, 8, 4, 4, 2, 2, 1
8, 8, 8, 4, 4, 2, 2, 1
};

/* //////////////////////////////////////////////////////// */

/* check sample file */
static int check_samplefile(int num)
{
	if (Machine->samples == 0) return 0;
	if (Machine->samples->total <= num ) return 0;
	if (Machine->samples->sample[num] == 0) return 0;
	/* sample file is found */
	return 1;
}

static int get_bits(int sbit,int bits)
{
	int offset = VLM5030_address + (sbit>>3);
	int data;

	data = VLM5030_rom[offset&VLM5030_address_mask] |
	       (((int)VLM5030_rom[(offset+1)&VLM5030_address_mask])<<8);
	data >>= sbit;
	data &= (0xff>>(8-bits));

	return data;
}

/* get next frame */
static int parse_frame (void)
{
	unsigned char cmd;

	/* remember previous frame */
	old_energy = new_energy;
	old_pitch = new_pitch;
	memcpy( old_k , new_k , sizeof(old_k) );
	/* command byte check */
	cmd = VLM5030_rom[VLM5030_address&VLM5030_address_mask];
	if( cmd & 0x01 )
	{	/* extend frame */
		new_energy = new_pitch = 0;
		memset( new_k , 0 , sizeof(new_k));
		VLM5030_address++;
		if( cmd & 0x02 )
		{	/* end of speech */
			logerror("VLM5030 %04X end \n",VLM5030_address );
			return 0;
		}
		else
		{	/* silent frame */
			int nums = ( (cmd>>2)+1 )*2;
			logerror("VLM5030 %04X silent %d frame\n",VLM5030_address,nums );
			return nums * FR_SIZE;
		}
	}
	/* normal frame */

	new_pitch  = pitchtable[get_bits( 1,5)];
	new_energy = energytable[get_bits( 6,5)] >> 6;

	/* 10 K's */
	new_k[9] = k10table[get_bits(11,3)];
	new_k[8] = k9table[get_bits(14,3)];
	new_k[7] = k8table[get_bits(17,3)];
	new_k[6] = k7table[get_bits(20,3)];
	new_k[5] = k6table[get_bits(23,3)];
	new_k[4] = k5table[get_bits(26,3)];
	new_k[3] = k4table[get_bits(29,4)];
	new_k[2] = k3table[get_bits(33,4)];
	new_k[1] = k2table[get_bits(37,4)];
	new_k[0] = k1table[get_bits(41,7)];

	VLM5030_address+=6;
	logerror("VLM5030 %04X voice \n",VLM5030_address );
	return FR_SIZE;
}

/* decode and buffering data */
static void vlm5030_update_callback(int num,INT16 *buffer, int length)
{
	int buf_count=0;
	int interp_effect;

	/* running */
	if( phase == PH_RUN )
	{
		/* playing speech */
		while (length > 0)
		{
			int current_val;

			/* check new interpolator or  new frame */
			if( sample_count == 0 )
			{
				sample_count = IP_SIZE;
				/* interpolator changes */
				if ( interp_count == 0 )
				{
					/* change to new frame */
					interp_count = parse_frame(); /* with change phase */
					if ( interp_count == 0 )
					{
						sample_count = 160; /* end -> stop time */
						phase = PH_STOP;
						goto phase_stop; /* continue to stop phase */
					}
					/* Set old target as new start of frame */
					current_energy = old_energy;
					current_pitch = old_pitch;
					memcpy( current_k , old_k , sizeof(current_k) );
					/* is this a zero energy frame? */
					if (current_energy == 0)
					{
						/*printf("processing frame: zero energy\n");*/
						target_energy = 0;
						target_pitch = current_pitch;
						memcpy( target_k , current_k , sizeof(target_k) );
					}
					else
					{
						/*printf("processing frame: Normal\n");*/
						/*printf("*** Energy = %d\n",current_energy);*/
						/*printf("proc: %d %d\n",last_fbuf_head,fbuf_head);*/
						target_energy = new_energy;
						target_pitch = new_pitch;
						memcpy( target_k , new_k , sizeof(target_k) );
					}
				}
				/* next interpolator */
				/* Update values based on step values */
				/*printf("\n");*/
				interp_effect = (int)(interp_coeff[(FR_SIZE-1) - (interp_count%FR_SIZE)]);

				current_energy += (target_energy - current_energy) / interp_effect;
				if (old_pitch != 0)
					current_pitch += (target_pitch - current_pitch) / interp_effect;
				/*printf("*** Energy = %d\n",current_energy);*/
				current_k[0] += (target_k[0] - current_k[0]) / interp_effect;
				current_k[1] += (target_k[1] - current_k[1]) / interp_effect;
				current_k[2] += (target_k[2] - current_k[2]) / interp_effect;
				current_k[3] += (target_k[3] - current_k[3]) / interp_effect;
				current_k[4] += (target_k[4] - current_k[4]) / interp_effect;
				current_k[5] += (target_k[5] - current_k[5]) / interp_effect;
				current_k[6] += (target_k[6] - current_k[6]) / interp_effect;
				current_k[7] += (target_k[7] - current_k[7]) / interp_effect;
				current_k[8] += (target_k[8] - current_k[8]) / interp_effect;
				current_k[9] += (target_k[9] - current_k[9]) / interp_effect;
				interp_count --;
			}
			/* calcrate digital filter */
			if (old_energy == 0)
			{
				/* generate silent samples here */
				current_val = 0x00;
			}
			else if (old_pitch == 0)
			{
				/* generate unvoiced samples here */
				int randvol = (rand () % 10);
				current_val = (randvol * current_energy) / 10;
			}
			else
			{
				/* generate voiced samples here */
				if (pitch_count < sizeof (chirptable))
					current_val = (chirptable[pitch_count] * current_energy) / 256;
				else
					current_val = 0x00;
			}

			/* Lattice filter here */
			u[10] = current_val;
			u[9] = u[10] - ((current_k[9] * x[9]) / 32768);
			u[8] = u[ 9] - ((current_k[8] * x[8]) / 32768);
			u[7] = u[ 8] - ((current_k[7] * x[7]) / 32768);
			u[6] = u[ 7] - ((current_k[6] * x[6]) / 32768);
			u[5] = u[ 6] - ((current_k[5] * x[5]) / 32768);
			u[4] = u[ 5] - ((current_k[4] * x[4]) / 32768);
			u[3] = u[ 4] - ((current_k[3] * x[3]) / 32768);
			u[2] = u[ 3] - ((current_k[2] * x[2]) / 32768);
			u[1] = u[ 2] - ((current_k[1] * x[1]) / 32768);
			u[0] = u[ 1] - ((current_k[0] * x[0]) / 32768);

			x[9] = x[8] + ((current_k[8] * u[8]) / 32768);
			x[8] = x[7] + ((current_k[7] * u[7]) / 32768);
			x[7] = x[6] + ((current_k[6] * u[6]) / 32768);
			x[6] = x[5] + ((current_k[5] * u[5]) / 32768);
			x[5] = x[4] + ((current_k[4] * u[4]) / 32768);
			x[4] = x[3] + ((current_k[3] * u[3]) / 32768);
			x[3] = x[2] + ((current_k[2] * u[2]) / 32768);
			x[2] = x[1] + ((current_k[1] * u[1]) / 32768);
			x[1] = x[0] + ((current_k[0] * u[0]) / 32768);
			x[0] = u[0];
			/* clipping, buffering */
			if (u[0] > 511)
				buffer[buf_count] = 127<<8;
			else if (u[0] < -512)
				buffer[buf_count] = -128<<8;
			else
				buffer[buf_count] = u[0] << 6;
			buf_count++;

			/* sample count */
			sample_count--;
			/* pitch */
			pitch_count++;
			if (pitch_count >= current_pitch )
				pitch_count = 0;
			/* size */
			length--;
		}
/*		return;*/
	}
	/* stop phase */
phase_stop:
	switch( phase )
	{
	case PH_SETUP:
		sample_count -= length;
		if( sample_count <= 0 )
		{
			logerror("VLM5030 BSY=H\n" );
			/* pin_BSY = 1; */
			phase = PH_WAIT;
		}
		break;
	case PH_STOP:
		sample_count -= length;
		if( sample_count <= 0 )
		{
			logerror("VLM5030 BSY=L\n" );
			pin_BSY = 0;
			phase = PH_IDLE;
		}
	}
	/* silent buffering */
	while (length > 0)
	{
		buffer[buf_count++] = 0x00;
		length--;
	}
}

/* realtime update */
static void VLM5030_update(void)
{
	if( !sampling_mode )
	{
		/* docode mode */
		stream_update(channel,0);
	}
	else
	{
		/* sampling mode (check  busy flag) */
		if( pin_ST == 0 && pin_BSY == 1 )
		{
			if( !mixer_is_sample_playing(schannel) )
				pin_BSY = 0;
		}
	}
}

/* set speech rom address */
void VLM5030_set_rom(void *speech_rom)
{
	VLM5030_rom = (unsigned char*)speech_rom;
}

/* get BSY pin level */
int VLM5030_BSY(void)
{
	VLM5030_update();
	return pin_BSY;
}

/* latch contoll data */
WRITE_HANDLER( VLM5030_data_w )
{
	latch_data = data;
}

/* set RST pin level : reset / set table address A8-A15 */
void VLM5030_RST (int pin )
{
	if( pin_RST )
	{
		if( !pin )
		{	/* H -> L : latch high address table */
			pin_RST = 0;
/*			table_h = latch_data * 256; */
			table_h = 0;
		}
	}
	else
	{
		if( pin )
		{	/* L -> H : reset chip */
			pin_RST = 1;
			if( pin_BSY )
			{
				if( sampling_mode )
					mixer_stop_sample( schannel );
				phase = PH_RESET;
				pin_BSY = 0;
			}
		}
	}
}

/* set VCU pin level : ?? unknown */
void VLM5030_VCU(int pin)
{
	/* unknown */
/*	intf->vcu = pin; */
	return;
}

/* set ST pin level  : set table address A0-A7 / start speech */
void VLM5030_ST(int pin )
{
	int table = table_h | latch_data;

	if( pin_ST != pin )
	{
		/* pin level is change */
		if( !pin )
		{	/* H -> L */
			pin_ST = 0;
			/* start speech */

			if (Machine->sample_rate == 0)
			{
				pin_BSY = 0;
				return;
			}
			/* set play mode samplingfile or emulate */
			sampling_mode = check_samplefile(table/2);
			if( !sampling_mode )
			{
				VLM5030_update();

				logerror("VLM5030 %02X start adr=%04X\n",table/2,VLM5030_address );

				/* docode mode */
				VLM5030_address = (((int)VLM5030_rom[table&VLM5030_address_mask])<<8)
				                |        VLM5030_rom[(table+1)&VLM5030_address_mask];
				/* reset process status */
				interp_count = sample_count = 0;
				/* clear filter */
				/* after 3 sampling start */
				phase = PH_RUN;
			}
			else
			{
				/* sampling mode */
				int num = table>>1;

				mixer_play_sample(schannel,
					Machine->samples->sample[num]->data,
					Machine->samples->sample[num]->length,
					Machine->samples->sample[num]->smpfreq,
					0);
			}
		}
		else
		{	/* L -> H */
			pin_ST = 1;
			/* setup speech , BSY on after 30ms? */
			phase = PH_SETUP;
			sample_count = 1; /* wait time for busy on */
			pin_BSY = 1; /* */
		}
	}
}

/* start VLM5030 with sound rom              */
/* speech_rom == 0 -> use sampling data mode */
int VLM5030_sh_start(const struct MachineSound *msound)
{
	int emulation_rate;

	intf = (const struct VLM5030interface*)msound->sound_interface;

	Machine->samples = readsamples(intf->samplenames,Machine->gamedrv->name);

	emulation_rate = intf->baseclock / 440;
	pin_BSY = pin_RST = pin_ST  = 0;
	phase = PH_IDLE;
/*	VLM5030_VCU(intf->vcu); */

	VLM5030_rom = memory_region(intf->memory_region);
	/* memory size */
	if( intf->memory_size == 0)
		VLM5030_address_mask = memory_region_length(intf->memory_region)-1;
	else
		VLM5030_address_mask = intf->memory_size-1;

	channel = stream_init("VLM5030",intf->volume,emulation_rate /* Machine->sample_rate */,
				0,vlm5030_update_callback);
	if (channel == -1) return 1;

	schannel = mixer_allocate_channel(intf->volume);

#if 1
	{
	int i;

	/* initialize energy table */
	for(i=0;i<0x20;i++)
	{
		energytable[i]=0x7fff*i/0x1f;
	}

	/* initialize filter table */
	for(i=-0x40 ; i<0x40 ; i++)
	{
		k1table[(i>=0) ? i : i+0x80] = i*K1_RANGE/0x40;
	}
	for(i=-0x08 ; i<0x08 ; i++)
	{
		k2table[(i>=0) ? i : i+0x10] = i*K2_RANGE/0x08;
		k3table[(i>=0) ? i : i+0x10] = i*K3_RANGE/0x08;
		k4table[(i>=0) ? i : i+0x10] = i*K4_RANGE/0x08;
	}
	for(i=-0x04 ; i<0x04 ; i++)
	{
		k5table[(i>=0) ? i : i+0x08] = i*K5_RANGE/0x04;
		k6table[(i>=0) ? i : i+0x08] = i*K6_RANGE/0x04;
		k7table[(i>=0) ? i : i+0x08] = i*K7_RANGE/0x04;
		k8table[(i>=0) ? i : i+0x08] = i*K8_RANGE/0x04;
		k9table[(i>=0) ? i : i+0x08] = i*K9_RANGE/0x04;
		k10table[(i>=0) ? i : i+0x08] = i*K10_RANGE/0x04;
	}

	}
#endif
	return 0;
}

/* update VLM5030 */
void VLM5030_sh_update( void )
{
	VLM5030_update();
}

/* stop VLM5030 */
void VLM5030_sh_stop( void )
{
}
