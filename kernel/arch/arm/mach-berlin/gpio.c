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
/*
 * Programming sample:
 * 1. spi_master_init_iomapper(IOMAPPER_SPI_MASTER), init IOmapper
 * 2. GPIO_PinmuxInit(port), setting both Galois and IOmapper pinmux
 * 3.1 GPIO_IOmapperSetInOut(port, in), setting GPIO pin as in / out.
 * 3.2 GPIO_PortSetInOut(port, in), setting GPIO pin as in /out.
 * 4. GPIO_PortWrite, GPIOPortRead
 *
 * NOTE: GPIO_PinmuxInit shouldn't setup GPIO[13:10], it's SPI#0 for
 * programming IOmapper. Galois GPIO in / out setting is done in
 * GPIO_PortWrite / GPIOPortRead, just for campatible forward with Berlin.
 *
 * NOTE: for Berlin, #1.spi_master_init_iomapper and #3.1 is descarded.
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/hardirq.h>
#include <asm/io.h>
#include <mach/galois_platform.h>
#include <mach/gpio.h>

#define MAX_PORT	16

#ifndef UNUSED
#define UNUSED(x)	((x)=(x))
#endif

#define GA_REG_WORD32_READ(a, pv)	(*pv = __raw_readl(IOMEM(a)))
#define GA_REG_WORD32_WRITE(a, v)	__raw_writel(v, IOMEM(a))

static DEFINE_MUTEX(gpio_mutex);

static inline void gpio_lock(void)
{
	if (likely(!in_interrupt()))
		mutex_lock(&gpio_mutex);
}

static inline void gpio_unlock(void)
{
	if (likely(!in_interrupt()))
		mutex_unlock(&gpio_mutex);
}

static DEFINE_MUTEX(sm_gpio_mutex);

static inline void sm_gpio_lock(void)
{
	if (likely(!in_interrupt()))
		mutex_lock(&sm_gpio_mutex);
}

static inline void sm_gpio_unlock(void)
{
	if (likely(!in_interrupt()))
		mutex_unlock(&sm_gpio_mutex);
}

/****************************************************
 * FUNCTION: Mutex lock
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: N/A
 ***************************************************/
void GPIO_PortLock(int port)
{
	UNUSED(port);
	gpio_lock();
}

/****************************************************
 * FUNCTION: Mutex unlock
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: N/A
 ***************************************************/
void GPIO_PortUnlock(int port)
{
	UNUSED(port);
	gpio_unlock();
}

/****************************************************
 * FUNCTION: toggle GPIO port between high and low
 * PARAMS: port - GPIO port # (0 ~ 31)
 *         value - 1: high; 0: low
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortWrite(int port, int value)
{
	int reg_ddr, reg_dr, gpio_port = port;
	int ddr, dr;

	if((port >= 0) && (port < 8)){
		reg_ddr = APB_GPIO_INST0_BASE + APB_GPIO_SWPORTA_DDR;
		reg_dr = APB_GPIO_INST0_BASE + APB_GPIO_SWPORTA_DR;
	} else if ((port >= 8) && (port < 16)){
		reg_ddr = APB_GPIO_INST1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_dr = APB_GPIO_INST1_BASE + APB_GPIO_SWPORTA_DR;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_ddr = APB_GPIO_INST2_BASE + APB_GPIO_SWPORTA_DDR;
		reg_dr = APB_GPIO_INST2_BASE + APB_GPIO_SWPORTA_DR;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_ddr = APB_GPIO_INST3_BASE + APB_GPIO_SWPORTA_DDR;
		reg_dr = APB_GPIO_INST3_BASE + APB_GPIO_SWPORTA_DR;
		port -= 24;
	} else
		return -1;

	GPIO_PortLock(gpio_port);

	/* set port to output mode */
	GA_REG_WORD32_READ(reg_ddr, &ddr);
	ddr |= (1<<port);
	GA_REG_WORD32_WRITE(reg_ddr, ddr);

	/* set port value */
	GA_REG_WORD32_READ(reg_dr, &dr);
	if (value){
		dr |= (1<<port);
	} else {
		dr &= ~(1<<port);
	}
	GA_REG_WORD32_WRITE(reg_dr, dr);

	GPIO_PortUnlock(gpio_port);

	return 0;
}


/****************************************************
 * FUNCTION: read GPIO port status
 * PARAMS: port - GPIO port # (0 ~ 31)
 *         *value - pointer to port status
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortRead(int port, int *value)
{
	int reg_ddr, reg_ext, gpio_port = port;
	int ddr, ext;

	if((port >= 0) && (port < 8)){
		reg_ddr = APB_GPIO_INST0_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ext = APB_GPIO_INST0_BASE + APB_GPIO_EXT_PORTA;
	} else if ((port >= 8) && (port < 16)){
		reg_ddr = APB_GPIO_INST1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ext = APB_GPIO_INST1_BASE + APB_GPIO_EXT_PORTA;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_ddr = APB_GPIO_INST2_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ext = APB_GPIO_INST2_BASE + APB_GPIO_EXT_PORTA;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_ddr = APB_GPIO_INST3_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ext = APB_GPIO_INST3_BASE + APB_GPIO_EXT_PORTA;
		port -= 24;
	} else
		return -1;

	GPIO_PortLock(gpio_port);

	/* set port to input mode */
	GA_REG_WORD32_READ(reg_ddr, &ddr);
	ddr &= ~(1<<port);
	GA_REG_WORD32_WRITE(reg_ddr, ddr);

	/* get port value */
	GA_REG_WORD32_READ(reg_ext, &ext);
	if (ext & (1<<port))
		*value = 1;
	else
		*value = 0;

	GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Set Galois GPIO pin as in or out
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   in - 1: IN, 0: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortSetInOut(int port, int in)
{
	int reg_ddr, reg_ctl, gpio_port = port;
	int ddr, ctl;

	if((port >= 0) && (port < 8)){
		reg_ddr = APB_GPIO_INST0_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ctl = APB_GPIO_INST0_BASE + APB_GPIO_PORTA_CTL;
	} else if ((port >= 8) && (port < 16)){
		reg_ddr = APB_GPIO_INST1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ctl = APB_GPIO_INST1_BASE + APB_GPIO_PORTA_CTL;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_ddr = APB_GPIO_INST2_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ctl = APB_GPIO_INST2_BASE + APB_GPIO_PORTA_CTL;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_ddr = APB_GPIO_INST3_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ctl = APB_GPIO_INST3_BASE + APB_GPIO_PORTA_CTL;
		port -= 24;
	} else
		return -1;

	GPIO_PortLock(gpio_port);

	/* software mode */
	GA_REG_WORD32_READ(reg_ctl, &ctl);
	ctl &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_ctl, ctl);

	/* set port to output mode */
	GA_REG_WORD32_READ(reg_ddr, &ddr);
	if (in)
		ddr &= ~(1 << port);
	else
		ddr |= (1 << port);
	GA_REG_WORD32_WRITE(reg_ddr, ddr);

	GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Get direction of Galois GPIO pin: in or out
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   *inout - PORT_DDR_IN: IN, PORT_DDR_OUT: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortGetInOut(int port, int *inout)
{
	int reg_ddr;

	if((port >= 0) && (port < 8)){
		reg_ddr = APB_GPIO_INST0_BASE + APB_GPIO_SWPORTA_DDR;
	} else if ((port >= 8) && (port < 16)){
		reg_ddr = APB_GPIO_INST1_BASE + APB_GPIO_SWPORTA_DDR;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_ddr = APB_GPIO_INST2_BASE + APB_GPIO_SWPORTA_DDR;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_ddr = APB_GPIO_INST3_BASE + APB_GPIO_SWPORTA_DDR;
		port -= 24;
	} else
		return -1;

	GA_REG_WORD32_READ(reg_ddr, inout);
	*inout = (*inout & (1 << port))? PORT_DDR_OUT : PORT_DDR_IN;

	return 0;
}

/****************************************************
 * FUNCTION: Get data of Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 *		   *data - the data in APB_GPIO_SWPORTA_DR
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortGetData(int port, int *data)
{
	int reg_dr;

	if((port >= 0) && (port < 8)){
		reg_dr = APB_GPIO_INST0_BASE + APB_GPIO_SWPORTA_DR;
	} else if ((port >= 8) && (port < 16)){
		reg_dr = APB_GPIO_INST1_BASE + APB_GPIO_SWPORTA_DR;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_dr = APB_GPIO_INST2_BASE + APB_GPIO_SWPORTA_DR;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_dr = APB_GPIO_INST3_BASE + APB_GPIO_SWPORTA_DR;
		port -= 24;
	} else
		return -1;

	/* set port to output mode */
	GA_REG_WORD32_READ(reg_dr, data);
	*data = (*data & (1 << port))? 1: 0;

	return 0;
}

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
int GPIO_PortInitIRQ(int port, int int_edge, int int_polarity)
{
	int reg_ddr, reg_debounce, reg_edge, reg_polarity;
	int reg_mask;//, reg_en, reg_eoi;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_ddr = APB_GPIO_INST0_BASE + APB_GPIO_SWPORTA_DDR;
		reg_mask = APB_GPIO_INST0_BASE + APB_GPIO_INTMASK;
		//reg_en = APB_GPIO_INST0_BASE + APB_GPIO_INTEN;
		//reg_eoi = APB_GPIO_INST0_BASE + APB_GPIO_PORTA_EOI;
		reg_debounce = APB_GPIO_INST0_BASE + APB_GPIO_DEBOUNCE;
		reg_edge = APB_GPIO_INST0_BASE + APB_GPIO_INTTYPE_LEVEL;
		reg_polarity = APB_GPIO_INST0_BASE + APB_GPIO_INT_POLARITY;
	} else if ((port >= 8) && (port < 16)){
		reg_ddr = APB_GPIO_INST1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_mask = APB_GPIO_INST1_BASE + APB_GPIO_INTMASK;
		//reg_en = APB_GPIO_INST1_BASE + APB_GPIO_INTEN;
		//reg_eoi = APB_GPIO_INST1_BASE + APB_GPIO_PORTA_EOI;
		reg_debounce = APB_GPIO_INST1_BASE + APB_GPIO_DEBOUNCE;
		reg_edge = APB_GPIO_INST1_BASE + APB_GPIO_INTTYPE_LEVEL;
		reg_polarity = APB_GPIO_INST1_BASE + APB_GPIO_INT_POLARITY;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_ddr = APB_GPIO_INST2_BASE + APB_GPIO_SWPORTA_DDR;
		reg_mask = APB_GPIO_INST2_BASE + APB_GPIO_INTMASK;
		//reg_en = APB_GPIO_INST2_BASE + APB_GPIO_INTEN;
		//reg_eoi = APB_GPIO_INST2_BASE + APB_GPIO_PORTA_EOI;
		reg_debounce = APB_GPIO_INST2_BASE + APB_GPIO_DEBOUNCE;
		reg_edge = APB_GPIO_INST2_BASE + APB_GPIO_INTTYPE_LEVEL;
		reg_polarity = APB_GPIO_INST2_BASE + APB_GPIO_INT_POLARITY;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_ddr = APB_GPIO_INST3_BASE + APB_GPIO_SWPORTA_DDR;
		reg_mask = APB_GPIO_INST3_BASE + APB_GPIO_INTMASK;
		//reg_en = APB_GPIO_INST3_BASE + APB_GPIO_INTEN;
		//reg_eoi = APB_GPIO_INST3_BASE + APB_GPIO_PORTA_EOI;
		reg_debounce = APB_GPIO_INST3_BASE + APB_GPIO_DEBOUNCE;
		reg_edge = APB_GPIO_INST3_BASE + APB_GPIO_INTTYPE_LEVEL;
		reg_polarity = APB_GPIO_INST3_BASE + APB_GPIO_INT_POLARITY;
		port -= 24;
	} else
		return -1;

	GPIO_PortLock(gpio_port);

	/* Mask interrupt */
	GA_REG_WORD32_READ(reg_mask, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_mask, value);

	/* Direction is input */
	GA_REG_WORD32_READ(reg_ddr, &value);
	value &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_ddr, value);

	/* Enable debounce */
	GA_REG_WORD32_READ(reg_debounce, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_debounce, value);

	/* int_edge=1: Edge triggered interrupt */
	GA_REG_WORD32_READ(reg_edge, &value);
	value |= (int_edge << port);
	GA_REG_WORD32_WRITE(reg_edge, value);

	/* int_polarity=1: rise-edge triggered interrupt */
	GA_REG_WORD32_READ(reg_polarity, &value);
	value |= (int_polarity << port);
	GA_REG_WORD32_WRITE(reg_polarity, value);

	/* Enable interrupt */
	//GA_REG_WORD32_READ(reg_en, &value);
	//value |= (1 << port);
	//GA_REG_WORD32_WRITE(reg_en, value);

	/* Write-only, write 1 to clear interrupt */
	//value = (1 << port);
	//GA_REG_WORD32_WRITE(reg_eoi, value);

	/* Unmask interrupt */
	//GA_REG_WORD32_READ(reg_mask, &value);
	//value &= ~(1 << port);
	//GA_REG_WORD32_WRITE(reg_mask, value);

	GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Enable interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 0 - succeed
 *        -1 - fail
 * NOTE: You also need to enable GPIO interrupt in ICTL.
 ***************************************************/
int GPIO_PortEnableIRQ(int port)
{
	int reg_mask, reg_en, reg_eoi;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_mask = APB_GPIO_INST0_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST0_BASE + APB_GPIO_INTEN;
		reg_eoi = APB_GPIO_INST0_BASE + APB_GPIO_PORTA_EOI;
	} else if ((port >= 8) && (port < 16)){
		reg_mask = APB_GPIO_INST1_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST1_BASE + APB_GPIO_INTEN;
		reg_eoi = APB_GPIO_INST1_BASE + APB_GPIO_PORTA_EOI;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_mask = APB_GPIO_INST2_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST2_BASE + APB_GPIO_INTEN;
		reg_eoi = APB_GPIO_INST2_BASE + APB_GPIO_PORTA_EOI;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_mask = APB_GPIO_INST3_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST3_BASE + APB_GPIO_INTEN;
		reg_eoi = APB_GPIO_INST3_BASE + APB_GPIO_PORTA_EOI;
		port -= 24;
	} else
		return -1;

	GPIO_PortLock(gpio_port);

	/* Enable interrupt */
	GA_REG_WORD32_READ(reg_en, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_en, value);

	/* Write-only, write 1 to clear interrupt */
	value = (1 << port);
	GA_REG_WORD32_WRITE(reg_eoi, value);

	/* Unmask interrupt */
	GA_REG_WORD32_READ(reg_mask, &value);
	value &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_mask, value);

	GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Disable interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int GPIO_PortDisableIRQ(int port)
{
	int reg_mask, reg_en;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_mask = APB_GPIO_INST0_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST0_BASE + APB_GPIO_INTEN;
	} else if ((port >= 8) && (port < 16)){
		reg_mask = APB_GPIO_INST1_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST1_BASE + APB_GPIO_INTEN;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_mask = APB_GPIO_INST2_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST2_BASE + APB_GPIO_INTEN;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_mask = APB_GPIO_INST3_BASE + APB_GPIO_INTMASK;
		reg_en = APB_GPIO_INST3_BASE + APB_GPIO_INTEN;
		port -= 24;
	} else
		return -1;

	GPIO_PortLock(gpio_port);

	/* Mask interrupt */
	GA_REG_WORD32_READ(reg_mask, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_mask, value);

	/* Disable interrupt */
	GA_REG_WORD32_READ(reg_en, &value);
	value &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_en, value);

	GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Lookup if there's interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 1 - yes, there's interrupt pending.
 *		   0 - no, there's no interrupt pending.
 *        -1 - fail.
 ***************************************************/
int GPIO_PortHasInterrupt(int port)
{
	int reg_status;
	int value;

	if((port >= 0) && (port < 8)){
		reg_status = APB_GPIO_INST0_BASE + APB_GPIO_INTSTATUS;
	} else if ((port >= 8) && (port < 16)){
		reg_status = APB_GPIO_INST1_BASE + APB_GPIO_INTSTATUS;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_status = APB_GPIO_INST2_BASE + APB_GPIO_INTSTATUS;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_status = APB_GPIO_INST3_BASE + APB_GPIO_INTSTATUS;
		port -= 24;
	} else
		return -1;

	GA_REG_WORD32_READ(reg_status, &value);
	if (value & (0x1 << port))
		return 1;
	else
		return 0;
}

/****************************************************
 * FUNCTION: Clear interrupt for Galois GPIO pin
 * PARAMS: port - GPIO port # (0 ~ 31)
 * RETURN: 0 - succeed.
 *        -1 - fail.
 ***************************************************/
int GPIO_PortClearInterrupt(int port)
{
	int reg_eoi;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_eoi = APB_GPIO_INST0_BASE + APB_GPIO_PORTA_EOI;
	} else if ((port >= 8) && (port < 16)){
		reg_eoi = APB_GPIO_INST1_BASE + APB_GPIO_PORTA_EOI;
		port -= 8;
	} else if ((port >= 16) && (port < 24)){
		reg_eoi = APB_GPIO_INST2_BASE + APB_GPIO_PORTA_EOI;
		port -= 16;
	} else if ((port >= 24) && (port < 32)){
		reg_eoi = APB_GPIO_INST3_BASE + APB_GPIO_PORTA_EOI;
		port -= 24;
	} else
		return -1;

	GPIO_PortLock(gpio_port);

	/* see above, write 1 to clear interrupt */
	value = (1 << port);
	GA_REG_WORD32_WRITE(reg_eoi, value);

	GPIO_PortUnlock(gpio_port);

	return 0;
}

//////////////////////////////////////////////////////////
// Only port 0-7 can support SM_GPIO interrupt
//////////////////////////////////////////////////////////

/****************************************************
 * FUNCTION: Mutex lock
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 * RETURN: N/A
 ***************************************************/
void SM_GPIO_PortLock(int port)
{
	UNUSED(port);
	sm_gpio_lock();
}

/****************************************************
 * FUNCTION: Mutex unlock
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 * RETURN: N/A
 ***************************************************/
void SM_GPIO_PortUnlock(int port)
{
	UNUSED(port);
	sm_gpio_unlock();
}

/****************************************************
 * FUNCTION: toggle SM_GPIO port between high and low
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *         value - 1: high; 0: low
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortWrite(int port, int value)
{
	int reg_ddr, reg_dr, reg_ctl;
	int ddr, dr, ctl;
	int gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_ddr = SM_APB_GPIO_BASE + APB_GPIO_SWPORTA_DDR;
		reg_dr = SM_APB_GPIO_BASE + APB_GPIO_SWPORTA_DR;
		reg_ctl = SM_APB_GPIO_BASE + APB_GPIO_PORTA_CTL;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_ddr = SM_APB_GPIO1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_dr = SM_APB_GPIO1_BASE + APB_GPIO_SWPORTA_DR;
		reg_ctl = SM_APB_GPIO1_BASE + APB_GPIO_PORTA_CTL;
		port -= 8;
	} else
		return -1;

	SM_GPIO_PortLock(gpio_port);

	/* software mode */
	GA_REG_WORD32_READ(reg_ctl, &ctl);
	ctl &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_ctl, ctl);

	/* set port to output mode */
	GA_REG_WORD32_READ(reg_ddr, &ddr);
	ddr |= (1<<port);
	GA_REG_WORD32_WRITE(reg_ddr, ddr);

	/* set port value */
	GA_REG_WORD32_READ(reg_dr, &dr);
	if (value){
		dr |= (1<<port);
	} else {
		dr &= ~(1<<port);
	}
	GA_REG_WORD32_WRITE(reg_dr, dr);

	SM_GPIO_PortUnlock(gpio_port);

	return 0;
}


/****************************************************
 * FUNCTION: read SM_GPIO port status
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *         *value - pointer to port status
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortRead(int port, int *value)
{
	int reg_ddr, reg_ext, reg_ctl;
	int ddr, ext, ctl;
	int gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_ddr = SM_APB_GPIO_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ext = SM_APB_GPIO_BASE + APB_GPIO_EXT_PORTA;
		reg_ctl = SM_APB_GPIO_BASE + APB_GPIO_PORTA_CTL;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_ddr = SM_APB_GPIO1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ext = SM_APB_GPIO1_BASE + APB_GPIO_EXT_PORTA;
		reg_ctl = SM_APB_GPIO1_BASE + APB_GPIO_PORTA_CTL;
		port -= 8;
	} else
		return -1;

	SM_GPIO_PortLock(gpio_port);

	/* software mode */
	GA_REG_WORD32_READ(reg_ctl, &ctl);
	ctl &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_ctl, ctl);

	/* set port to input mode */
	GA_REG_WORD32_READ(reg_ddr, &ddr);
	ddr &= ~(1<<port);
	GA_REG_WORD32_WRITE(reg_ddr, ddr);

	/* get port value */
	GA_REG_WORD32_READ(reg_ext, &ext);
	if (ext & (1<<port))
		*value = 1;
	else
		*value = 0;

	SM_GPIO_PortUnlock(gpio_port);

	return 0;
}


/****************************************************
 * FUNCTION: Set Galois SM_GPIO pin as in or out
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *		   in - 1: IN, 0: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortSetInOut(int port, int in)
{
	int reg_ddr, reg_ctl;
	int ddr, ctl;
	int gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_ddr = SM_APB_GPIO_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ctl = SM_APB_GPIO_BASE + APB_GPIO_PORTA_CTL;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_ddr = SM_APB_GPIO1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_ctl = SM_APB_GPIO1_BASE + APB_GPIO_PORTA_CTL;
		port -= 8;
	} else
		return -1;

	SM_GPIO_PortLock(gpio_port);

	/* software mode */
	GA_REG_WORD32_READ(reg_ctl, &ctl);
	ctl &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_ctl, ctl);

	/* set port to output mode */
	GA_REG_WORD32_READ(reg_ddr, &ddr);
	if (in)
		ddr &= ~(1 << port);
	else
		ddr |= (1 << port);
	GA_REG_WORD32_WRITE(reg_ddr, ddr);

	SM_GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Get direction of Galois SM_GPIO pin: in or out
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *		   *inout - PORT_DDR_IN: IN, PORT_DDR_OUT: OUT
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortGetInOut(int port, int *inout)
{
	int reg_ddr;

	if((port >= 0) && (port < 8)){
		reg_ddr = SM_APB_GPIO_BASE + APB_GPIO_SWPORTA_DDR;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_ddr = SM_APB_GPIO1_BASE + APB_GPIO_SWPORTA_DDR;
		port -= 8;
	} else
		return -1;

	GA_REG_WORD32_READ(reg_ddr, inout);
	*inout = (*inout & (1 << port))? PORT_DDR_OUT : PORT_DDR_IN;

	return 0;
}

/****************************************************
 * FUNCTION: Get data of Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 11)
 *		   *data - the data in APB_GPIO_SWPORTA_DR
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortGetData(int port, int *data)
{
	int reg_dr;

	if((port >= 0) && (port < 8)){
		reg_dr = SM_APB_GPIO_BASE + APB_GPIO_SWPORTA_DR;
	} else if ((port >= 8) && (port < MAX_PORT )){
		reg_dr = SM_APB_GPIO1_BASE + APB_GPIO_SWPORTA_DR;
		port -= 8;
	} else
		return -1;

	GA_REG_WORD32_READ(reg_dr, data);
	*data = (*data & (1 << port))? 1: 0;

	return 0;
}

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
int SM_GPIO_PortInitIRQ(int port, int int_edge, int int_polarity)
{
	int reg_ddr, reg_debounce, reg_edge, reg_polarity;
	int reg_mask;//, reg_en, reg_eoi;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_ddr = SM_APB_GPIO_BASE + APB_GPIO_SWPORTA_DDR;
		reg_mask = SM_APB_GPIO_BASE + APB_GPIO_INTMASK;
		//reg_en = SM_APB_GPIO_BASE + APB_GPIO_INTEN;
		//reg_eoi = SM_APB_GPIO_BASE + APB_GPIO_PORTA_EOI;
		reg_debounce = SM_APB_GPIO_BASE + APB_GPIO_DEBOUNCE;
		reg_edge = SM_APB_GPIO_BASE + APB_GPIO_INTTYPE_LEVEL;
		reg_polarity = SM_APB_GPIO_BASE + APB_GPIO_INT_POLARITY;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_ddr = SM_APB_GPIO1_BASE + APB_GPIO_SWPORTA_DDR;
		reg_mask = SM_APB_GPIO1_BASE + APB_GPIO_INTMASK;
		//reg_en = SM_APB_GPIO1_BASE + APB_GPIO_INTEN;
		//reg_eoi = SM_APB_GPIO1_BASE + APB_GPIO_PORTA_EOI;
		reg_debounce = SM_APB_GPIO1_BASE + APB_GPIO_DEBOUNCE;
		reg_edge = SM_APB_GPIO1_BASE + APB_GPIO_INTTYPE_LEVEL;
		reg_polarity = SM_APB_GPIO1_BASE + APB_GPIO_INT_POLARITY;
		port -= 8;
	} else {
		return -1;
	}

	SM_GPIO_PortLock(gpio_port);

	/* Mask interrupt */
	GA_REG_WORD32_READ(reg_mask, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_mask, value);

	/* Direction is input */
	GA_REG_WORD32_READ(reg_ddr, &value);
	value &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_ddr, value);

	/* Enable debounce */
	GA_REG_WORD32_READ(reg_debounce, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_debounce, value);

	/* int_edge=1: Edge triggered interrupt */
	GA_REG_WORD32_READ(reg_edge, &value);
	value |= (int_edge << port);
	GA_REG_WORD32_WRITE(reg_edge, value);

	/* int_polarity=1: rise-edge triggered interrupt */
	GA_REG_WORD32_READ(reg_polarity, &value);
	value |= (int_polarity << port);
	GA_REG_WORD32_WRITE(reg_polarity, value);

	/* Enable interrupt */
	//GA_REG_WORD32_READ(reg_en, &value);
	//value |= (1 << port);
	//GA_REG_WORD32_WRITE(reg_en, value);

	/* Write-only, write 1 to clear interrupt */
	//value = (1 << port);
	//GA_REG_WORD32_WRITE(reg_eoi, value);

	/* Unmask interrupt */
	//GA_REG_WORD32_READ(reg_mask, &value);
	//value &= ~(1 << port);
	//GA_REG_WORD32_WRITE(reg_mask, value);
	SM_GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Enable interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 0 - succeed
 *        -1 - fail
 * NOTE: You also need to enable SM_GPIO interrupt in ICTL.
 ***************************************************/
int SM_GPIO_PortEnableIRQ(int port)
{
	int reg_mask, reg_en, reg_eoi;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_mask = SM_APB_GPIO_BASE + APB_GPIO_INTMASK;
		reg_en = SM_APB_GPIO_BASE + APB_GPIO_INTEN;
		reg_eoi = SM_APB_GPIO_BASE + APB_GPIO_PORTA_EOI;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_mask = SM_APB_GPIO1_BASE + APB_GPIO_INTMASK;
		reg_en = SM_APB_GPIO1_BASE + APB_GPIO_INTEN;
		reg_eoi = SM_APB_GPIO1_BASE + APB_GPIO_PORTA_EOI;
		port -= 8;
	} else {
		return -1;
	}

	SM_GPIO_PortLock(gpio_port);

	/* Enable interrupt */
	GA_REG_WORD32_READ(reg_en, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_en, value);

	/* Write-only, write 1 to clear interrupt */
	value = (1 << port);
	GA_REG_WORD32_WRITE(reg_eoi, value);

	/* Unmask interrupt */
	GA_REG_WORD32_READ(reg_mask, &value);
	value &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_mask, value);

	SM_GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Disable interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 0 - succeed
 *        -1 - fail
 ***************************************************/
int SM_GPIO_PortDisableIRQ(int port)
{
	int reg_mask, reg_en;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_mask = SM_APB_GPIO_BASE + APB_GPIO_INTMASK;
		reg_en = SM_APB_GPIO_BASE + APB_GPIO_INTEN;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_mask = SM_APB_GPIO1_BASE + APB_GPIO_INTMASK;
		reg_en = SM_APB_GPIO1_BASE + APB_GPIO_INTEN;
		port -= 8;
	} else {
		return -1;
	}

	SM_GPIO_PortLock(gpio_port);

	/* Mask interrupt */
	GA_REG_WORD32_READ(reg_mask, &value);
	value |= (1 << port);
	GA_REG_WORD32_WRITE(reg_mask, value);

	/* Disable interrupt */
	GA_REG_WORD32_READ(reg_en, &value);
	value &= ~(1 << port);
	GA_REG_WORD32_WRITE(reg_en, value);

	SM_GPIO_PortUnlock(gpio_port);

	return 0;
}

/****************************************************
 * FUNCTION: Lookup if there's interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 1 - yes, there's interrupt pending.
 *		   0 - no, there's no interrupt pending.
 *        -1 - fail.
 ***************************************************/
int SM_GPIO_PortHasInterrupt(int port)
{
	int reg_status;
	int value;

	if((port >= 0) && (port < 8)){
		reg_status = SM_APB_GPIO_BASE + APB_GPIO_INTSTATUS;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_status = SM_APB_GPIO1_BASE + APB_GPIO_INTSTATUS;
		port -= 8;
	} else {
		return -1;
	}

	GA_REG_WORD32_READ(reg_status, &value);
	if (value & (0x1 << port))
		return 1;
	else
		return 0;
}

/****************************************************
 * FUNCTION: Clear interrupt for Galois SM_GPIO pin
 * PARAMS: port - SM_GPIO port # (0 ~ 7)
 * RETURN: 0 - succeed.
 *        -1 - fail.
 ***************************************************/
int SM_GPIO_PortClearInterrupt(int port)
{
	int reg_eoi;
	int value, gpio_port = port;

	if((port >= 0) && (port < 8)){
		reg_eoi = SM_APB_GPIO_BASE + APB_GPIO_PORTA_EOI;
	} else if ((port >= 8) && (port < MAX_PORT)){
		reg_eoi = SM_APB_GPIO1_BASE + APB_GPIO_PORTA_EOI;
		port -= 8;
	} else {
		return -1;
	}

	SM_GPIO_PortLock(gpio_port);
	/* see above, write 1 to clear interrupt */
	value = (1 << port);
	GA_REG_WORD32_WRITE(reg_eoi, value);
	SM_GPIO_PortUnlock(gpio_port);
	return 0;
}


/*
 * the low-level GPIO api may be used by linux kernel driver.
 * so it's exported.
 */
EXPORT_SYMBOL(GPIO_PortWrite);
EXPORT_SYMBOL(GPIO_PortRead);
EXPORT_SYMBOL(GPIO_PortSetInOut);
EXPORT_SYMBOL(GPIO_PortGetInOut);
EXPORT_SYMBOL(GPIO_PortGetData);
EXPORT_SYMBOL(GPIO_PortInitIRQ);
EXPORT_SYMBOL(GPIO_PortEnableIRQ);
EXPORT_SYMBOL(GPIO_PortDisableIRQ);
EXPORT_SYMBOL(GPIO_PortHasInterrupt);
EXPORT_SYMBOL(GPIO_PortClearInterrupt);
EXPORT_SYMBOL(GPIO_PortLock);
EXPORT_SYMBOL(GPIO_PortUnlock);
EXPORT_SYMBOL(SM_GPIO_PortWrite);
EXPORT_SYMBOL(SM_GPIO_PortRead);
EXPORT_SYMBOL(SM_GPIO_PortSetInOut);
EXPORT_SYMBOL(SM_GPIO_PortGetInOut);
EXPORT_SYMBOL(SM_GPIO_PortGetData);
EXPORT_SYMBOL(SM_GPIO_PortInitIRQ);
EXPORT_SYMBOL(SM_GPIO_PortEnableIRQ);
EXPORT_SYMBOL(SM_GPIO_PortDisableIRQ);
EXPORT_SYMBOL(SM_GPIO_PortHasInterrupt);
EXPORT_SYMBOL(SM_GPIO_PortClearInterrupt);
EXPORT_SYMBOL(SM_GPIO_PortLock);
EXPORT_SYMBOL(SM_GPIO_PortUnlock);
