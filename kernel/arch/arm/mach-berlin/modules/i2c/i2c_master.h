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
#ifndef _I2C_MASTER_H_
#define _I2C_MASTER_H_

#if 0
/* slv_addr */
#define TWSI_SLVADDR_THS8200a	0x20
#define TWSI_SLVADDR_CS4382A	0x18

/* addr_type */
#define TWSI_7BIT_SLAVE_ADDR	0
#define TWSI_10BIT_SLAVE_ADDR	1

/* speed_type, negleted currently */
#define TWSI_STANDARD_SPEED		0 /* 100K, 400K and 3.4MHz in standard type */
#define TWSI_CUSTOM_SPEED		1 /* any speed in custom type */
#endif

/* speed */
#define TWSI_SPEED_100			100
#define TWSI_SPEED_400			400
#define TWSI_SPEED_3400			3400

/*
 * ioctl commands
 */
#define TWSI_IOCTL_READWRITE	0x1619
#define TWSI_IOCTL_SETSPEED		0x1234

/*
 * struction maintained by linux kernel, user doesn't use it
 */
typedef struct galois_twsi_info {
	uint mst_id;	/* : master num you're operating */
	uint slv_addr;	/* : the slave's address you're operating */
	uint addr_type;	/* : the slave is 7Bit address or 10 Bit address */
	uint xd_mode; 	/* : defined by Xiaodong, see above TWSIDEV_XDMODExxx */
	uint speed_type;/* : TWSI_STANDARD_SPEED or TWSI_CUSTOM_SPEED, ignored now */
	uint speed;		/* : 100K, 400K or 3400 or..., ignored now */
	int irq;	/* : kernel info, don't touch */
	int usable;	/* : kernel info, don't touch */
} galois_twsi_info_t;

/*
 * read-write data from/to TWSI slave, TWSI_IOCTL_READWRITE
 */
typedef struct galois_twsi_rw {
	uint mst_id;	/* user input: master num you're operating */
	uint slv_addr;	/* user input: the slave's address you're operating */
	uint addr_type;	/* user input: the slave is 7Bit address or 10 Bit address */

	uint rd_cnt;			/* read number in byte */
	unsigned char *rd_buf;	/* data buffer for read */
	uint wr_cnt;			/* write number in byte */
	unsigned char *wr_buf;	/* data buffer for write */
} galois_twsi_rw_t;

/*
 * set twsi speed
 */
typedef struct galois_twsi_speed {
	uint mst_id;	/* user input: master num you're operating */
	uint speed_type;/* user input: TWSI_STANDARD_SPEED or TWSI_CUSTOM_SPEED */
	uint speed; 	/* user input: if speed==0, user old setting, else update twsi speed */
} galois_twsi_speed_t;

// error code definitions
#ifndef I2C_OK
#define I2C_OK		1
#endif
#ifndef I2C_EBADPARAM
#define I2C_EBADPARAM	-1
#endif
#ifndef I2C_EUNSUPPORT
#define I2C_EUNSUPPORT	-2
#endif
#ifndef I2C_EBUSRW
#define I2C_EBUSRW	-8
#endif
#ifndef I2C_EBUSY
#define I2C_EBUSY	-9
#endif

// I2C bus speed types
#define I2C_STANDARD_SPEED	0 // only 100K, 400K and 3.4MHz are supported in standard type
#define I2C_CUSTOM_SPEED	1 // any speed can be specified by users in custom type

// I2C slave address type
#define I2C_7BIT_SLAVE_ADDR	0
#define I2C_10BIT_SLAVE_ADDR	1

/******************************************
 *  global function declarations
 *****************************************/
/*********************************************************
 * FUNCTION: judge if the i2c_bus is idle
 * PARAMS: master_id - id of I2C master to init
 * RETURN: I2C_OK - idle
 *         I2C_EBADPARAM - invalid parameter
 *         I2C_EBUSY - used by others
 ********************************************************/
int i2c_master_is_idle(int master_id);

/*********************************************************
 * FUNCTION: init I2C master: set default bus speed
 * PARAMS: master_id - id of I2C master to init
 * RETURN: I2C_OK - succeed
 *         I2C_EBADPARAM - invalid parameter
 ********************************************************/
int i2c_master_init(int master_id);

/********************************************************************
 * FUNCTION: set I2C master bus speed
 * PARAMS: master_id - id of I2C master to config
 *         type - STANDARD or CUSTOM
 *         speed - in unit of KHz
 * RETURN: I2C_OK - succeed
 *         I2C_EBUSY - I2C module is enabled
 *         I2C_EUNSUPPORT - not support
 * NOTE: in STANDARD type, only 100 and 400 KHz speed are supported
 ********************************************************************/
int i2c_master_set_speed(int master_id, int type, int speed);

/***********************************************************************
 * FUNCTION: read bytes from slave device
 * PARAMS: master_id - id of I2C master to operate
 *         slave_addr - address of slave device
 *         addr_type - address type: I2C_7BIT_SLAVE_ADDR or I2C_10BIT_SLAVE_ADDR
 *         pwbuf - pointer of write buffer
 *         wnum - number of bytes to write
 *         prbuf - pointer of read buffer
 *         rnum - number of bytes to read
 * RETURN: I2C_OK - succeed  *         I2C_EBUSY - I2C module is enabled
 *         I2C_EBUSRW - read fail
 * NOTE: maximum write bytes is 260, maximum read bytes is 256 in a single
 *       transaction
 ***********************************************************************/
int i2c_master_writeread_bytes(int master_id, int slave_addr, int addr_type, unsigned char *pwbuf, int wnum, unsigned char *prbuf, int rnum, int dum );

/************************************************
 *  i2c master interrupt service routine
 ***********************************************/
void i2c_master_irs(int master_id);

#endif
