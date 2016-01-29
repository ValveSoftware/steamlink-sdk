/*
 * Cosmic Chasm video hardware emulation
 *
 * Jul 15 1999 by Mathis Rosenhauer
 *
 */

#include "driver.h"
#include "vidhrdw/vector.h"

#define HALT   0
#define JUMP   1
#define COLOR  2
#define SCALEY 3
#define POSY   4
#define SCALEX 5
#define POSX   6
#define LENGTH 7

#define VEC_SHIFT 16

UINT8 *cchasm_ram;

static int xcenter, ycenter;

static void cchasm_refresh_end (int dummy)
{
    cpu_set_irq_line (0, 2, ASSERT_LINE);
}

static void cchasm_refresh (void)
{

	int pc = 0;
    int done = 0;
    int opcode, data;
    int currentx = 0, currenty = 0;
    int scalex = 0, scaley = 0;
    int color = 0;
    int total_length = 1;   /* length of all lines drawn in a frame */
    int move = 0;

	vector_clear_list();

	while (!done)
	{

        data = READ_WORD (&cchasm_ram[pc]);
   		opcode = data >> 12;
        data &= 0xfff;
        if ((opcode > COLOR) && (data & 0x800))
          data |= 0xfffff000;

		pc += 2;

		switch (opcode)
		{
        case HALT:
            done=1;
            break;
        case JUMP:
            pc = data - 0xb00;
            //logerror("JUMP to %x\n", data);
            break;
        case COLOR:
            data = data ^ 0xfff;
            color = ((data >> 4) & 0xe0) | ((data >> 3 ) &0x1c) | ((data >> 2 ) &0x3);
            break;
        case SCALEY:
            scaley = data << 5;
            break;
        case POSY:
            move = 1;
            currenty = ycenter + (data << 16);
            break;
        case SCALEX:
            scalex = data << 5;
            break;
        case POSX:
            move = 1;
            currentx = xcenter - (data << 16);
            break;
        case LENGTH:
            if (move)
            {
                vector_add_point (currentx, currenty, 0, 0);
                move = 0;
            }

            currentx -= data * scalex;
            currenty += data * scaley;

            total_length += abs(data);

            if (color)
                vector_add_point (currentx, currenty, color, 0xff);
            else
                move = 1;
            break;
        default:
            //logerror("Unknown refresh proc opcode %x with data %x at pc = %x\n", opcode, data, pc-2);
            done = 1;
            break;
		}
	}
    /* Refresh processor runs with 6 MHz */
    timer_set (TIME_IN_NSEC(166) * total_length, 0, cchasm_refresh_end);
}


WRITE_HANDLER( cchasm_refresh_control_w )
{
    switch (data)
    {
    case 0x37ff:
        cchasm_refresh();
        break;
    case 0xf7ff:
        cpu_set_irq_line (0, 2, CLEAR_LINE);
        break;
    }
}

void cchasm_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
    int i= 0, r, g, b;

    for (r=0; r<8; r++)
        for (g=0; g<8; g++)
            for (b=0; b<4; b++)
            {
                palette[3*i  ] = (255 * r) / 7;
                palette[3*i+1] = (255 * g) / 7;
                palette[3*i+2] = (255 * b) /3;
                i++;
            }
}

int cchasm_vh_start (void)
{
    int xmin, xmax, ymin, ymax;

	xmin=Machine->visible_area.min_x;
	ymin=Machine->visible_area.min_y;
	xmax=Machine->visible_area.max_x;
	ymax=Machine->visible_area.max_y;

	xcenter=((xmax+xmin)/2) << VEC_SHIFT;
	ycenter=((ymax+ymin)/2) << VEC_SHIFT;

	vector_set_shift (VEC_SHIFT);
	return vector_vh_start();
}


void cchasm_vh_stop (void)
{
	vector_vh_stop();
}
