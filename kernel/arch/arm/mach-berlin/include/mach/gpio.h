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
#ifndef _GPIO_H_
#define _GPIO_H_

/*
 * The GPIO inst0 is located at (MEMMAP_APBPERIF_REG_BASE + 0x0C00)
 * i.e. 0xf7f30c00.
 */
#define APB_GPIO_SWPORTA_DR		0x00
#define APB_GPIO_SWPORTA_DDR		0x04
#define APB_GPIO_PORTA_CTL		0x08
#define APB_GPIO_SWPORTB_DR		0x0c
#define APB_GPIO_SWPORTB_DDR		0x10
#define APB_GPIO_PORTB_CTL		0x14
#define APB_GPIO_SWPORTC_DR		0x18
#define APB_GPIO_SWPORTC_DDR		0x1c
#define APB_GPIO_PORTC_CTL		0x20
#define APB_GPIO_SWPORTD_DR		0x24
#define APB_GPIO_SWPORTD_DDR		0x28
#define APB_GPIO_PORTD_CTL		0x2c
#define APB_GPIO_INTEN			0x30
#define APB_GPIO_INTMASK		0x34
#define APB_GPIO_INTTYPE_LEVEL		0x38
#define APB_GPIO_INT_POLARITY		0x3c
#define APB_GPIO_INTSTATUS		0x40
#define APB_GPIO_RAWINTSTATUS		0x44
#define APB_GPIO_DEBOUNCE		0x48
#define APB_GPIO_PORTA_EOI		0x4c
#define APB_GPIO_EXT_PORTA		0x50
#define APB_GPIO_EXT_PORTB		0x54
#define APB_GPIO_EXT_PORTC		0x58
#define APB_GPIO_EXT_PORTD		0x5c
#define APB_GPIO_LS_SYNC		0x60
#define APB_GPIO_ID_CODE		0x64
#define APB_GPIO_RESERVED		0x68
#define APB_GPIO_COMP_VERSION		0x6c

#define PORT_DDR_IN	0
#define PORT_DDR_OUT	1

/****************************************************
 * FUNCTION: Mutex lock for SoC GPIO
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: N/A
 ***************************************************/
void GPIO_PortLock(int port);

/****************************************************
 * FUNCTION: Mutex unlock for SoC GPIO
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: N/A
 ***************************************************/
void GPIO_PortUnlock(int port);

/****************************************************
 * FUNCTION: toggle GPIO port between high and low
 * PARAMS: port - GPIO port # (0 ~ 31)
 *         value - 1: high; 0: low
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortWrite(int port, int value);


/****************************************************
 * FUNCTION: read GPIO port status
 * PARAMS: port - GPIO port # (0 ~ 31)
 *         *value - pointer to port status
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortRead(int port, int *value);


/****************************************************
 * FUNCTION: pinmux init for the pin of GPIO port
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 0 - succeed
 *        -1 - fail
 * NOTE: Be sure that spi_master_init_iomapper is done.
 ***************************************************/
int GPIO_PinmuxInit(int port);

/****************************************************
 * FUNCTION: Configure IOmapper for GPIO port
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   in - Set GPIO pin as IN or OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 * NOTE: Be sure that spi_master_init_iomapper is done.
 ***************************************************/
int GPIO_IOmapperSetInOut(int port, int in);

/****************************************************
 * FUNCTION: Get the in/out status of GPIO pin at IOmapper
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   *inout - return PORT_DDR_IN or PORT_DDR_OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 * NOTE: Be sure that spi_master_init_iomapper is done.
 ***************************************************/
int GPIO_IOmapperGetInOut(int port, int *inout);

/****************************************************
 * FUNCTION: Set Galois GPIO pin as in or out
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   in - 1: IN, 0: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortSetInOut(int port, int in);

/****************************************************
 * FUNCTION: Get direction of Galois GPIO pin: in or out
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   *inout - PORT_DDR_IN: IN, PORT_DDR_OUT: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortGetInOut(int port, int *inout);

/****************************************************
 * FUNCTION: Get data of Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   *data - the data in APB_GPIO_SWPORTA_DR
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortGetData(int port, int *data);

/****************************************************
 * FUNCTION: Init interrupt for Galois GPIO pin, and set
 *			 interrupt level or edge, but keep interrupt closed.
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   int_edge - 1: edge triggered, 0: level triggered.
 *		   int_polarity - 1: rise edge/high level triggered.
 *						  0: fall edge/low level triggered.
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortInitIRQ(int port, int int_edge, int int_polarity);

/****************************************************
 * FUNCTION: Enable interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 0 - succeed
 *        -1 - fail
 * NOTE: You also need to enable GPIO interrupt in ICTL.
 ***************************************************/
int GPIO_PortEnableIRQ(int port);

/****************************************************
 * FUNCTION: Disable interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortDisableIRQ(int port);

/****************************************************
 * FUNCTION: Lookup if there's interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 1 - yes, there's interrupt pending.
 *		   0 - no, there's no interrupt pending.
 *        -1 - fail.
 ***************************************************/
int GPIO_PortHasInterrupt(int port);

/****************************************************
 * FUNCTION: Clear interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 0 - succeed.
 *        -1 - fail.
 ***************************************************/
int GPIO_PortClearInterrupt(int port);

//////////////////////////////////////////////////////////
// Only port 0-7 can support SM_GPIO interrupt
//////////////////////////////////////////////////////////

/****************************************************
 * FUNCTION: Mutex lock for SM GPIO
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 * RETURN: N/A
 ***************************************************/
void SM_GPIO_PortLock(int port);

/****************************************************
 * FUNCTION: Mutex unlock for SM GPIO
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 * RETURN: N/A
 ***************************************************/
void SM_GPIO_PortUnlock(int port);

/****************************************************
 * FUNCTION: toggle SM_GPIO port between high and low
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *         value - 1: high; 0: low
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortWrite(int port, int value);

/****************************************************
 * FUNCTION: read SM_GPIO port status
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *         *value - pointer to port status
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortRead(int port, int *value);

/****************************************************
 * FUNCTION: Set Galois SM_GPIO pin as in or out
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *		   in - 1: IN, 0: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortSetInOut(int port, int in);

/****************************************************
 * FUNCTION: Get direction of Galois SM_GPIO pin: in or out
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *		   *inout - PORT_DDR_IN: IN, PORT_DDR_OUT: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortGetInOut(int port, int *inout);

/****************************************************
 * FUNCTION: Get data of Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *		   *data - the data in APB_GPIO_SWPORTA_DR
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortGetData(int port, int *data);

/****************************************************
 * FUNCTION: Init interrupt for Galois SM_GPIO pin, and set
 *			 interrupt level or edge, but keep interrupt closed.
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 *		   int_edge - 1: edge triggered, 0: level triggered.
 *		   int_polarity - 1: rise edge/high level triggered.
 *						  0: fall edge/low level triggered.
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortInitIRQ(int port, int int_edge, int int_polarity);

/****************************************************
 * FUNCTION: Enable interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 0 - succeed
 *        -1 - fail
 * NOTE: You also need to enable SM_GPIO interrupt in ICTL.
 ***************************************************/
int SM_GPIO_PortEnableIRQ(int port);

/****************************************************
 * FUNCTION: Disable interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortDisableIRQ(int port);

/****************************************************
 * FUNCTION: Lookup if there's interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 1 - yes, there's interrupt pending.
 *		   0 - no, there's no interrupt pending.
 *        -1 - fail.
 ***************************************************/
int SM_GPIO_PortHasInterrupt(int port);

/****************************************************
 * FUNCTION: Clear interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 0 - succeed.
 *        -1 - fail.
 ***************************************************/
int SM_GPIO_PortClearInterrupt(int port);

#endif /* _GPIO_H_ */
