/********************************************************************************
 * Marvell GPL License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute and/or
 * modify this File in accordance with the terms and conditions of the General
 * Public License Version 2, June 1991 (the "GPL License"), a copy of which is
 * available along with the File in the license.txt file or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
 * on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED.  The GPL License provides additional details about this warranty
 * disclaimer.
 ********************************************************************************/
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <mach/galois_platform.h>

#include "i2c_master.h"

/* for debug purpose only */
/*#define POLLING_MODE*/

/* number of I2C masters in system */
#define I2C_MASTER_NUM		4

/*
 * I2C master system clock
 * minimum sysmte clock are required
 * Standard Mode:   2 MHz
 * Fast Mode:       10 MHz
 * High-Speed Mode: 100 MHz
 */
#if defined(FPGA)
#define I2C_SYS_CLOCK		5
#define I2C_SM_CLOCK		5
#else
#define I2C_SYS_CLOCK		100
#define I2C_SM_CLOCK		25
#endif

/* register definitions of SPI master */
#define I2C_REG_CON		0x00
#define I2C_REG_TAR		0x04
#define I2C_REG_SAR		0x08
#define I2C_REG_HS_MADDR	0x0C
#define I2C_REG_DATA_CMD	0x10
#define I2C_REG_SS_SCL_HCNT	0x14
#define I2C_REG_SS_SCL_LCNT	0x18
#define I2C_REG_FS_SCL_HCNT	0x1C
#define I2C_REG_FS_SCL_LCNT	0x20
#define I2C_REG_HS_SCL_HCNT	0x24
#define I2C_REG_HS_SCL_LCNT	0x28
#define I2C_REG_INTR_STAT	0x2C
#define I2C_REG_INTR_MASK	0x30
#define I2C_REG_RINTR_STAT	0x34
#define I2C_REG_RX_TL		0x38
#define I2C_REG_TX_TL		0x3C
#define I2C_REG_CLR_INTR	0x40
#define I2C_REG_CLR_RX_UNDER	0x44
#define I2C_REG_CLR_RX_OVER	0x48
#define I2C_REG_CLR_TX_OVER	0x4C
#define I2C_REG_CLR_RD_REQ	0x50
#define I2C_REG_CLR_TX_ABRT	0x54
#define I2C_REG_CLR_RX_DONE	0x58
#define I2C_REG_CLR_ACTIVITY	0x5C
#define I2C_REG_CLR_STOP_DET	0x60
#define I2C_REG_CLR_START_DET	0x64
#define I2C_REG_CLR_GEN_CALL	0x68
#define I2C_REG_ENABLE		0x6C
#define I2C_REG_STATUS		0x70
#define I2C_REG_TXFLR		0x74
#define I2C_REG_RXFLR		0x78
#define I2C_REG_TX_ABRT_STAT	0x80
#define I2C_REG_DMA_CR		0x88
#define I2C_REG_DMA_TDLR	0x8C
#define I2C_REG_DMA_RDLR	0x90
#define I2C_REG_COMP_PARAM	0xF4
#define I2C_REG_COMP_VERSION	0xF8
#define I2C_REG_COMP_TYPE	0xFC /* reset value: 0x44570140 */

/* macros of I2C_REG_CON bit offset */
#define CON_SLAVE_DISABLE	6
#define CON_RESTART_EN		5
#define CON_10BITADDR		4
#define CON_SPEED		1
#define CON_MASTER_ENABLE	0

/* macros of I2C_REG_TAR bit offset */
#define TAR_10BITADDR		12
#define TAR_SPECIAL		11
#define TAR_GC_OR_START		10

/* macros of I2C_REG_INTR_* bit definition */
#define INTR_GEN_CALL		0x800
#define INTR_START_DET		0x400
#define INTR_STOP_DET		0x200
#define INTR_ACTIVITY		0x100
#define INTR_RX_DONE		0x080
#define INTR_TX_ABRT		0x040
#define INTR_RD_REQ		0x020
#define INTR_TX_EMPTY		0x010
#define INTR_TX_OVER		0x008
#define INTR_RX_FULL		0x004
#define INTR_RX_OVER		0x002
#define INTR_RX_UNDER		0x001

/* macros of I2C_REG_ENABLE bit definition */
#define EN_ENABLE		0x01

/* macros of read-command */
#define READ_CMD		0x100

/* macros of interrupt mask flag */
#define MASK			0
#define UNMASK			1

/* macros of read/write I2C master registers */
#define I2C_RegRead(a, pv)	(*pv = __raw_readl(IOMEM(base_addr+(a))))
#define I2C_RegWrite(a, v)	__raw_writel(v, IOMEM(base_addr+(a)))

#ifndef MAX
#define MAX(a, b)		(((a)>(b))?(a):(b))
#define MIN(a, b)		(((a)>(b))?(b):(a))
#endif

/* FIFO size: SM is 6 bytes, SoC is 64 bytes */
unsigned int I2C_RX_FIFO_SIZE[I2C_MASTER_NUM] = {64, 64, 8, 8};
unsigned int I2C_TX_FIFO_SIZE[I2C_MASTER_NUM] = {64, 64, 8, 8};

/* tx/rx buffer address used in ISR */
unsigned char *tx_buf_addr[I2C_MASTER_NUM];
unsigned char *rx_buf_addr[I2C_MASTER_NUM];

/*
 * number of command which have been written to TX fifo so far, this will
 * equal to bytes_of_write_cmd + bytes_of_read_cmd at the end of transaction
 */
int bytes_written_to_txfifo[I2C_MASTER_NUM];
/* number of bytes read out from RX fifo so far */
int bytes_read_from_rxfifo[I2C_MASTER_NUM];
/* number of write-command which need to be written to TX fifo */
int bytes_of_write_cmd[I2C_MASTER_NUM];
/* number of read-command which need to be written to TX fifo */
int bytes_of_read_cmd[I2C_MASTER_NUM];
/* variable indicating transaction finishing or not */
volatile int transaction_done[I2C_MASTER_NUM];

volatile int write_done[I2C_MASTER_NUM];
volatile int read_done[I2C_MASTER_NUM];

/* I2C master's base-address vector */
const unsigned int I2C_MASTER_BASEADDR[I2C_MASTER_NUM] = {
	APB_I2C_INST0_BASE,
	APB_I2C_INST1_BASE,
	APB_I2C_INST2_BASE,
	APB_I2C_INST3_BASE
};

static int abort_status;
static int handle_read_cmd = 0;
static DECLARE_WAIT_QUEUE_HEAD(i2c_wait);

/*************************************************
 *  mask/unmask interrupt
 *  intr - interrupt
 *  flag: MASK - mask interrupt
 *        UNMASK - unmask interrupt
 *************************************************/
static void i2c_mask_int(int master_id, int intr, int flag)
{
	unsigned int base_addr;
	int i;

	if ((master_id < 0) || (master_id > I2C_MASTER_NUM-1))
		return;

	base_addr = I2C_MASTER_BASEADDR[master_id];

	I2C_RegRead(I2C_REG_INTR_MASK, &i);
	if (flag == MASK)
		i &= ~intr;
	else
		i |= intr;
	I2C_RegWrite(I2C_REG_INTR_MASK, i);

	return;
}

/*****************************************************
 * set I2C master TX FIFO underflow threshold
 * thrld: 0 - 255
 *****************************************************/
static void set_txfifo_threshold(int master_id, int thrld)
{
	unsigned int base_addr;

	if (thrld<0 || thrld>255) return;

	base_addr = I2C_MASTER_BASEADDR[master_id];

	I2C_RegWrite(I2C_REG_TX_TL, thrld);

	return;
}

/*****************************************************
 * set I2C master RX FIFO overflow threshold
 * thrld: 1 - 256
 *
 *****************************************************/
static void set_rxfifo_threshold(int master_id, int thrld)
{
	unsigned int base_addr;

	if (thrld<1 || thrld>256) return;

	base_addr = I2C_MASTER_BASEADDR[master_id];

	I2C_RegWrite(I2C_REG_RX_TL, thrld-1);

	return;
}

/*****************************************************
 * judge if a master is idled, it should be called
 * before you init the master.
 *****************************************************/
int i2c_master_is_idle(int master_id)
{
	unsigned int base_addr;
	int i;

	if ((master_id < 0) || (master_id > I2C_MASTER_NUM-1))
		return (I2C_EBADPARAM);

	base_addr = I2C_MASTER_BASEADDR[master_id];

	/* check whether I2C module is disabled */
	I2C_RegRead(I2C_REG_ENABLE, &i);
	if (i & EN_ENABLE)
		return (I2C_EBUSY);
	return I2C_OK;
}

/*********************************************************************
 * FUNCTION: set I2C master bus speed
 * PARAMS: master_id - id of I2C master to config
 *         type - STANDARD or CUSTOM
 *         speed - in unit of KHz
 * RETURN: I2C_OK - succeed
 *     I2C_EBUSY - I2C module is enabled
 *         I2C_EUNSUPPORT - not support
 * NOTE: in STANDARD type, only 100 and 400 KHz speed are supported
 ********************************************************************/
int i2c_master_set_speed(int master_id, int type, int speed)
{
	unsigned int base_addr;
	int i, sysClk;

	if ((master_id < 0) || (master_id > I2C_MASTER_NUM-1))
		return (I2C_EBADPARAM);

	base_addr = I2C_MASTER_BASEADDR[master_id];

	/* check whether I2C module is disabled */
	I2C_RegRead(I2C_REG_ENABLE, &i);
	if (i & EN_ENABLE)
		return (I2C_EBUSY);

	if (master_id >= 2)
		sysClk = I2C_SM_CLOCK;
	else
		sysClk = I2C_SYS_CLOCK;

	if (type == I2C_STANDARD_SPEED) {
		switch (speed) {
		case 100: /* standard-speed */
			I2C_RegRead(I2C_REG_CON, &i);
			i &= ~(3<<CON_SPEED);
			i |= (1<<CON_SPEED);
			I2C_RegWrite(I2C_REG_CON, i); /* set master to operate in standard-speed mode */

			I2C_RegWrite(I2C_REG_SS_SCL_HCNT, 4500*sysClk/1000); /* 4000-ns minimum HIGH-period*/
			I2C_RegWrite(I2C_REG_SS_SCL_LCNT, 5400*sysClk/1000); /* 4700-ns minimum LOW-period*/
			break;
		case 400: /* fast-speed */
			I2C_RegRead(I2C_REG_CON, &i);
			i &= ~(3<<CON_SPEED);
			i |= (2<<CON_SPEED);
			I2C_RegWrite(I2C_REG_CON, i); /* set master to operate in fast-speed mode */

			I2C_RegWrite(I2C_REG_FS_SCL_HCNT, 700*sysClk/1000); /* 600-ns minimum HIGH-period*/
			I2C_RegWrite(I2C_REG_FS_SCL_LCNT, 1450*sysClk/1000); /* 1300-ns minimum LOW-period*/
			break;
		case 3400: // high-speed
			I2C_RegRead(I2C_REG_CON, &i);
			i &= ~(3<<CON_SPEED);
			i |= (3<<CON_SPEED);
			I2C_RegWrite(I2C_REG_CON, i); /* set master to operate in high-speed mode */

			I2C_RegWrite(I2C_REG_HS_SCL_HCNT, 100*sysClk/1000); /* 60-ns minimum HIGH-period for 100pF loading, and 120-ns for 400pF loading. */
			I2C_RegWrite(I2C_REG_HS_SCL_LCNT, 195*sysClk/1000); /* 160-ns minimum LOW-period for 100pF loading, and 320-ns for 400pF loading. */
			break;
		default:
			return (I2C_EUNSUPPORT);
		}
	} else if (type == I2C_CUSTOM_SPEED) {
		/* set master to operate in fast-speed mode */
		I2C_RegRead(I2C_REG_CON, &i);
		i &= ~(3<<CON_SPEED);

		if (speed <= 100) {
			i |= (1<<CON_SPEED);
			I2C_RegWrite(I2C_REG_CON, i); /* set master to operate in standard-speed mode */
			i = (4500 * 100) / speed;
			I2C_RegWrite(I2C_REG_SS_SCL_HCNT, i*sysClk/1000); /* 4000-ns minimum HIGH-period*/
			i = (5400 * 100) / speed;
			I2C_RegWrite(I2C_REG_SS_SCL_LCNT, i*sysClk/1000); /* 4700-ns minimum LOW-period*/
		} else if (speed <= 400) {
			i |= (2<<CON_SPEED);
			I2C_RegWrite(I2C_REG_CON, i); /* set master to operate in fast-speed mode */
			i = (700 * 400) / speed;
			I2C_RegWrite(I2C_REG_FS_SCL_HCNT, i*sysClk/1000); /* 600-ns minimum HIGH-period*/
			i = (1450 * 400) / speed;
			I2C_RegWrite(I2C_REG_FS_SCL_LCNT, i*sysClk/1000); /* 1300-ns minimum LOW-period*/
		} else {
			i |= (3<<CON_SPEED);
			I2C_RegWrite(I2C_REG_CON, i); /* set master to operate in high-speed mode */
			i = (100 * 3400) / speed;
			I2C_RegWrite(I2C_REG_HS_SCL_HCNT, i*sysClk/1000); /* 60-ns minimum HIGH-period for 100pF loading, and 120-ns for 400pF loading. */
			i = (195 * 3400) / speed;
			I2C_RegWrite(I2C_REG_HS_SCL_LCNT, i*sysClk/1000); /* 160-ns minimum LOW-period for 100pF loading, and 320-ns for 400pF loading. */
		}
	}

	return (I2C_OK);
}


/*********************************************************
 * FUNCTION: init I2C master: set default bus speed
 * PARAMS: master_id - id of I2C master to init
 * RETURN: I2C_OK - succeed
 *         I2C_EBADPARAM - invalid parameter
 ********************************************************/
int i2c_master_init(int master_id)
{
	unsigned int base_addr;
	int i;

	if ((master_id < 0) || (master_id > I2C_MASTER_NUM-1))
		return (I2C_EBADPARAM);

	base_addr = I2C_MASTER_BASEADDR[master_id];

	// disable I2C module first
	I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);

	// set TX/RX fifo default threshold
	set_txfifo_threshold(master_id, 0);
	set_rxfifo_threshold(master_id, I2C_RX_FIFO_SIZE[master_id]/2);

	// disable all I2C interrupts.
	I2C_RegWrite(I2C_REG_INTR_MASK, 0);

	// disable slave mode, enable restart function, enable master mode
	I2C_RegRead(I2C_REG_CON, &i);
	i |= (1<<CON_SLAVE_DISABLE)|(1<<CON_RESTART_EN)|(1<<CON_MASTER_ENABLE);
	I2C_RegWrite(I2C_REG_CON, i);

	// set default I2C master speed
	i2c_master_set_speed(master_id, I2C_STANDARD_SPEED, 400);

	I2C_RegRead(I2C_REG_INTR_MASK, &i);

	return (I2C_OK);
}

/***********************************************************************
 * FUNCTION: read bytes from slave device
 * PARAMS: master_id - id of I2C master to operate
 *         slave_addr - address of slave device
 *         addr_type - address type: I2C_7BIT_SLAVE_ADDR or I2C_10BIT_SLAVE_ADDR
 *         pwbuf - pointer of write buffer
 *         wnum - number of bytes to write
 *         prbuf - pointer of read buffer
 *         rnum - number of bytes to read
 * RETURN: I2C_OK - succeed
 *     I2C_EBUSY - I2C module is enabled
 *         I2C_EBUSRW - read fail
 * NOTE: maximum write bytes is 260, maximum read bytes is 256 in a single
 *       transaction
 ***********************************************************************/

int i2c_master_writeread_bytes(int master_id, int slave_addr, int addr_type, unsigned char *pwbuf, int wnum, unsigned char *prbuf, int rnum, int dum )
{
	unsigned int base_addr;
	int i;
#ifdef POLLING_MODE
	int j, k;
#endif

	if ((master_id < 0) || (master_id > I2C_MASTER_NUM - 1))
		return (I2C_EBADPARAM);
	if (wnum > 260)
		return (I2C_EBADPARAM);
	if (addr_type != I2C_7BIT_SLAVE_ADDR && addr_type != I2C_10BIT_SLAVE_ADDR)
		return (I2C_EBADPARAM);

	base_addr = I2C_MASTER_BASEADDR[master_id];

	/* check whether I2C module is disabled */
	I2C_RegRead(I2C_REG_ENABLE, &i);
	if (i & EN_ENABLE)
		return (I2C_EBUSY);

	if (addr_type == I2C_7BIT_SLAVE_ADDR) {
		/* set 7-bit target address, normal transfer mode */
		I2C_RegRead(I2C_REG_TAR, &i);
		i &= ~((1<<TAR_10BITADDR) | (1<<TAR_SPECIAL) | 0x3ff);
		i |= slave_addr;
		I2C_RegWrite(I2C_REG_TAR, i);

		/* for SM TWSI, dynamic address change is 0. so manually set 7-bit mode */
		I2C_RegRead(I2C_REG_CON, &i);
		i &= ~(1<<CON_10BITADDR);
		I2C_RegWrite(I2C_REG_CON, i);
	} else {
		/* set 10-bit target address, normal transfer mode */
		I2C_RegRead(I2C_REG_TAR, &i);
		i &= ~((1<<TAR_SPECIAL) | 0x3ff);
		i |= ((1<<TAR_10BITADDR) | slave_addr);
		I2C_RegWrite(I2C_REG_TAR, i);

		/* for SM TWSI, dynamic address change is 0. so manually set 10-bit mode */
		I2C_RegRead(I2C_REG_CON, &i);
		i |= (1<<CON_10BITADDR);
		I2C_RegWrite(I2C_REG_CON, i);
	}

	/* initiate transaction status flag */
	transaction_done[master_id] = 0;
	write_done[master_id] = 0;
	read_done[master_id] = 0;
	handle_read_cmd = 0;

	/*
	 * set TX fifo threshold to make sure TXE interrupt will be triggered
	 * as soon as we unmask the TXE interupt, we will start transaction by
	 * writing CMD_DATA register then.
	 *
	 * Set to 5/8 if use SM i2c to improve safety
	 * while transferring large quantity of data.
	 */
	if (master_id >= 2)
		set_txfifo_threshold(master_id, I2C_TX_FIFO_SIZE[master_id]*5/8);
	else
		set_txfifo_threshold(master_id, I2C_TX_FIFO_SIZE[master_id]/2);


	if ((pwbuf && wnum>0) && (prbuf && rnum>0)) {
		/* write & read transaction */
		/* set RX fifo threshold */
		if (rnum<=I2C_RX_FIFO_SIZE[master_id])
			set_rxfifo_threshold(master_id, rnum);
		else
			set_rxfifo_threshold(master_id, I2C_RX_FIFO_SIZE[master_id]/2);

		/* initiate number of write and read commands which are goding to be written to TX fifo */
		bytes_written_to_txfifo[master_id] = 0;
		bytes_read_from_rxfifo[master_id] = 0;
		bytes_of_write_cmd[master_id] = wnum;
		bytes_of_read_cmd[master_id] = rnum;

		tx_buf_addr[master_id] = pwbuf;
		rx_buf_addr[master_id] = prbuf;

#ifndef POLLING_MODE
		/* now we can unmask TXE and RXF interrupts, to initiate transaction within TXE interrupt routine */
		i2c_mask_int(master_id, INTR_TX_EMPTY, UNMASK);
		i2c_mask_int(master_id, INTR_RX_FULL, UNMASK);
		i2c_mask_int(master_id, INTR_TX_ABRT, UNMASK);
#endif

		/* enable I2C master */
		I2C_RegWrite(I2C_REG_ENABLE, EN_ENABLE);

#ifdef POLLING_MODE
		/* issue write commands to TX fifo */
		for (i=0; i<wnum; i++)
		{
			do {
				I2C_RegRead(I2C_REG_STATUS, &j);
			} while (!(j&0x02)); /* TX fifo is full */

			I2C_RegWrite(I2C_REG_DATA_CMD, pwbuf[i]);
		}
		// issue read commands to TX fifo and receive data from RX fifo
		i = 0;
		while (bytes_read_from_rxfifo[master_id]<rnum)
		{
			// if there are received data in RX fifo, read out to buffer
			I2C_RegRead(I2C_REG_STATUS, &j);
			if (j&0x08) /* RX fifo is not empty */
				I2C_RegRead(I2C_REG_DATA_CMD, &(prbuf[bytes_read_from_rxfifo[master_id]++]));

			// if TX fifo not full, issue remaining read commands to TX fifo
			if (i<rnum){
				I2C_RegRead(I2C_REG_STATUS, &j);
				if (j&0x02){    /* TX fifo is not full */
					I2C_RegWrite(I2C_REG_DATA_CMD, READ_CMD);
					i ++;
				}
			}
		}
		transaction_done[master_id] = 1;
#endif
	} else if (pwbuf && wnum > 0) {
		/* write-only transaction */
		/* initiate number of write commands which are goding to be written to TX fifo */
		bytes_written_to_txfifo[master_id] = 0;
		bytes_read_from_rxfifo[master_id] = 0;
		bytes_of_write_cmd[master_id] = wnum;
		bytes_of_read_cmd[master_id] = 0;

		tx_buf_addr[master_id] = pwbuf;

#ifndef POLLING_MODE
		/* now we can unmask TXE interrupts, to initiate transaction within TXE interrupt routine */
		i2c_mask_int(master_id, INTR_TX_EMPTY, UNMASK);
		i2c_mask_int(master_id, INTR_TX_ABRT, UNMASK);
#endif

		/* enable I2C master */
		I2C_RegWrite(I2C_REG_ENABLE, EN_ENABLE);

#ifdef POLLING_MODE
		/* issue write commands to TX fifo */
		for (i=0; i<wnum; i++)
		{
			I2C_RegRead(I2C_REG_STATUS, &j);
			while (!(j&0x02)) /* TX fifo is full */
			{
				if (!(j&0x01)) /* but I2C master is idle */
				{
					/* disable I2C master */
					I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);
					/* somehow error happen !!! */
					return (I2C_EBUSRW);
				}
				I2C_RegRead(I2C_REG_STATUS, &j);
			}
			I2C_RegWrite(I2C_REG_DATA_CMD, pwbuf[i]);
		}

		I2C_RegRead(I2C_REG_STATUS, &j);
		while (!(j&0x04)){ /* TX fifo is not empty */
			if(!(j&0x01)) /* but I2C master is idle */
			{
				/* disable I2C master */
				I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);
				/* somehow error happen !!! */
				return (I2C_EBUSRW);
			}
			I2C_RegRead(I2C_REG_STATUS, &j);
		}

		do{
			I2C_RegRead(I2C_REG_STATUS, &i);
		}while (i&0x01); /* wait until transfer finish */

		transaction_done[master_id] = 1;
#endif
	} else if (prbuf && rnum > 0) {
		/* read-only transaction */
		/* set RX fifo threshold */
		if (rnum<=I2C_RX_FIFO_SIZE[master_id])
			set_rxfifo_threshold(master_id, rnum);
		else
			set_rxfifo_threshold(master_id, I2C_RX_FIFO_SIZE[master_id]/2);
		/* initiate number of read commands which are goding to be written to TX fifo */
		bytes_written_to_txfifo[master_id] = 0;
		bytes_read_from_rxfifo[master_id] = 0;
		bytes_of_write_cmd[master_id] = 0;
		bytes_of_read_cmd[master_id] = rnum;

		rx_buf_addr[master_id] = prbuf;

#ifndef POLLING_MODE
		/* now we can unmask TXE and RXF interrupts, to initiate transaction within TXE interrupt routine */
		i2c_mask_int(master_id, INTR_TX_EMPTY, UNMASK);
		i2c_mask_int(master_id, INTR_RX_FULL, UNMASK);
		i2c_mask_int(master_id, INTR_TX_ABRT, UNMASK);
#endif

		/* enable I2C master */
		I2C_RegWrite(I2C_REG_ENABLE, EN_ENABLE);

#ifdef POLLING_MODE
		/* issue read commands to TX fifo and receive data from RX fifo */
		i = 0;
		while (bytes_read_from_rxfifo[master_id]<rnum)
		{
			/* if there are received data in RX fifo, read out to buffer */
			I2C_RegRead(I2C_REG_STATUS, &j);
			if (j&0x08) /* RX fifo is not empty */
				I2C_RegRead(I2C_REG_DATA_CMD, &(prbuf[bytes_read_from_rxfifo[master_id]++]));
			/* if TX fifo not full, issue remaining read commands to TX fifo */
			if (i<rnum){
				I2C_RegRead(I2C_REG_STATUS, &j);
				if (j&0x02){    /* TX fifo is not full */
					I2C_RegWrite(I2C_REG_DATA_CMD, READ_CMD);
					i ++;
				}
			}
		}
		transaction_done[master_id] = 1;
#endif
	} else {
		/* FIXME should not be here! */
		printk(KERN_WARNING "%s: should not be here\n", __func__);
	}

	/* Linux will sleep until transaction_done[master_id] is true */
	/* write & read transaction */
	if ((pwbuf && wnum > 0) && (prbuf && rnum > 0)) {
		if ((wait_event_interruptible_timeout(i2c_wait, (read_done[master_id] & write_done[master_id]), HZ/10)) <= 0) {
			I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);
			return (I2C_EBUSRW);
		}
	} else if (pwbuf && wnum > 0) {
		/* write  transaction */
		if (wait_event_interruptible_timeout(i2c_wait, write_done[master_id], HZ/10) <= 0) {
			I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);
			return -ERESTARTSYS;
		}
	} else if (prbuf && rnum > 0) {
		/* read transaction */
		if (wait_event_interruptible_timeout(i2c_wait, read_done[master_id], HZ/10) <= 0) {
			I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);
			return -ERESTARTSYS;
		}
	} else {
		/* FIXME no such case */
		printk(KERN_WARNING "%s: should not be here\n", __func__);
	}
#ifndef POLLING_MODE
	/* disable TX_ABRT interrupt */
	i2c_mask_int(master_id, INTR_TX_ABRT, MASK);
#endif

	if (transaction_done[master_id] == -1 ||
	    write_done[master_id] == -1 ||
	    read_done[master_id] == -1) {
		/* disable I2C master */
		I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);
		printk("abort_status: %d, %d\n", abort_status, bytes_written_to_txfifo[master_id]);
		return (I2C_EBUSRW);
	}

	/* if it is a write only transaction, wait until transfer finish */
	do {
		I2C_RegRead(I2C_REG_STATUS, &i);
	} while (i&0x01); /* wait until transfer finish */

	/* disable I2C master */
	I2C_RegWrite(I2C_REG_ENABLE, ~EN_ENABLE);
	return (I2C_OK);
}

void i2c_master_irs(int master_id)
{
	int base_addr;
	int bytes_in_fifo, bytes_to_be_written;
	int i, j, k, istat;
	int clear_irq = 0;

	base_addr = I2C_MASTER_BASEADDR[master_id];

	I2C_RegRead(I2C_REG_INTR_STAT, &istat);
	if (istat & INTR_TX_ABRT) {
		i2c_mask_int(master_id, INTR_TX_EMPTY, MASK);
		i2c_mask_int(master_id, INTR_RX_FULL, MASK);
		i2c_mask_int(master_id, INTR_TX_ABRT, MASK);
		transaction_done[master_id] = -1;
		write_done[master_id] = -1;
		read_done[master_id] = -1;
		I2C_RegRead(I2C_REG_TX_ABRT_STAT, &abort_status);
		I2C_RegRead(I2C_REG_CLR_TX_ABRT, &clear_irq);
		wake_up_interruptible(&i2c_wait);
		return;
	} else if (istat & INTR_RX_FULL) {
		/* RX fifo equal or above threshold */
		/* number of valid bytes in RX fifo */
		I2C_RegRead(I2C_REG_RXFLR, &bytes_in_fifo);
		/* Changed to handle read size greater than 256 */
		for (i = 0; i < MIN(bytes_in_fifo,bytes_of_read_cmd[master_id] - bytes_read_from_rxfifo[master_id]); i++) {
			I2C_RegRead(I2C_REG_DATA_CMD, &(rx_buf_addr[master_id][bytes_read_from_rxfifo[master_id]+i]));
			handle_read_cmd--;
		}

		/* this is the totally bytes in buffer read out from RX fifo */
		bytes_read_from_rxfifo[master_id] += i;
		/* check if this is the last bunch of received data */
		if (bytes_read_from_rxfifo[master_id]==bytes_of_read_cmd[master_id]) {
			/* disable the RXF interrupt */
			i2c_mask_int(master_id, INTR_RX_FULL, MASK);

			/* transaction finished */
			read_done[master_id] = 1;
			I2C_RegRead(I2C_REG_CLR_RX_DONE, &clear_irq);
			wake_up_interruptible(&i2c_wait);
			return;

		} else if (bytes_of_read_cmd[master_id]-bytes_read_from_rxfifo[master_id] <= I2C_RX_FIFO_SIZE[master_id]) {
		/* otherwise, check if RX fifo can handle all the remaining received data */
			/* set RX fifo full threshold to the remaining bytes number */
			set_rxfifo_threshold(master_id, MAX(bytes_of_read_cmd[master_id]-bytes_read_from_rxfifo[master_id], 1));
		} else {
			/* do nothing */
		}
	}

	if (istat & INTR_TX_EMPTY) {
		/* TX fifo equal or below threshold */
		/* number of bytes still exit in TX fifo */
		I2C_RegRead(I2C_REG_TXFLR, &bytes_in_fifo);

		/* check if this is the last TXE interrupt for this transaction */
		if (!bytes_in_fifo) {
			if (bytes_written_to_txfifo[master_id] == bytes_of_write_cmd[master_id]+bytes_of_read_cmd[master_id]) {
				/* disable TXE interrupt */
				i2c_mask_int(master_id, INTR_TX_EMPTY, MASK);
				if (bytes_of_write_cmd[master_id]){
					if (bytes_read_from_rxfifo[master_id] < bytes_of_read_cmd[master_id]){
						set_rxfifo_threshold(master_id, MAX(bytes_of_read_cmd[master_id]-bytes_read_from_rxfifo[master_id], 1) );
						I2C_RegWrite(I2C_REG_DATA_CMD, READ_CMD);
					}
					write_done[master_id] = 1;
					wake_up_interruptible(&i2c_wait);
				}
				return;
			}
		}
		/* totally bytes to be written to the TX fifo */
		bytes_to_be_written = bytes_of_write_cmd[master_id] + bytes_of_read_cmd[master_id] - bytes_written_to_txfifo[master_id];
		for (i = 0, j = 0, k = 0; ((j < I2C_RX_FIFO_SIZE[master_id]) && (i < MIN(I2C_TX_FIFO_SIZE[master_id] - bytes_in_fifo, bytes_to_be_written))); i++) {
			/* copy write-command first if there is */
			if (bytes_written_to_txfifo[master_id]+i < bytes_of_write_cmd[master_id]) {
				I2C_RegWrite(I2C_REG_DATA_CMD, tx_buf_addr[master_id][bytes_written_to_txfifo[master_id]+i]);
				k++;
			} else {
				/* copy read-command if there is */
				if (handle_read_cmd < I2C_RX_FIFO_SIZE[master_id]){
					I2C_RegWrite(I2C_REG_DATA_CMD, READ_CMD);
					handle_read_cmd++;
					j++;
				}
			}
		}

		/* this is the totally bytes of commands which have been written to TX fifo so far */
		bytes_written_to_txfifo[master_id] += k;
		bytes_written_to_txfifo[master_id] += j;
		/* check if the TX fifo can handle all the remaining bytes */
		if (bytes_written_to_txfifo[master_id] == bytes_of_write_cmd[master_id] + bytes_of_read_cmd[master_id])
			set_txfifo_threshold(master_id, 0);
	}
}
