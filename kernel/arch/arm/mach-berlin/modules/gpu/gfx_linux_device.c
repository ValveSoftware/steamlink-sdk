/******************************************************************************* 
* Copyright (C) Marvell International Ltd. and its affiliates 
*
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

#include "PreComp.h"
#include <linux/pagemap.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include "gfx_linux_common.h"
#include "gfx_linux_kernel.h"

#define GFXHW_intr_vec	IRQ_intrGfxM0
#define GFXHW_cpu_id 0

/******************************************************************************\
******************************** gcoGALDEVICE Code *******************************
\******************************************************************************/

/*******************************************************************************
**
**	gcoOS_AtomicExchange
**
**	Atomically exchange a pair of 32-bit values.
**
**	INPUT:
**
**		gcoOS Os
**			Pointer to an gcoOS object.
**
**      IN OUT gctINT32_PTR Target
**          Pointer to the 32-bit value to exchange.
**
**		IN gctINT32 NewValue
**			Specifies a new value for the 32-bit value pointed to by Target.
**
**      OUT gctINT32_PTR OldValue
**          The old value of the 32-bit value pointed to by Target.
**
**	OUTPUT:
**
**		Nothing.
*/
gceSTATUS
gcoOS_AtomicExchange(
    IN OUT gctUINT32_PTR Target,
    IN gctUINT32 NewValue,
    OUT gctUINT32_PTR OldValue
)
{
    /* Exchange the pair of 32-bit values. */
    *OldValue = (gctUINT32) atomic_xchg((atomic_t *) Target, (int) NewValue);

    /* Success. */
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**	gcoEVENT_Notify
**
**	Callback function to notify the gcoEVENT object an event has occured.
**
**	INPUT:
**
**		gcoEVENT Event
**			Pointer to an gcoEVENT object.
**
**		gctUINT32 IDs
**			Bit mask of event IDs that occured.
**
**	OUTPUT:
**
**		Nothing.
*/
gceSTATUS
gcoEVENT_Notify(
    IN gcoGALDEVICE Device,
    IN gctUINT32 IDs
)
{
    gceSTATUS status = gcvSTATUS_OK;
    gctINT id;

    while (IDs != 0)
    {
        gctINT current_id = 0;

        for (id = 0; id < 32; ++id)
        {
            if (IDs & (1 << id))
            {
                current_id = id;
            }
        }
        Device->event[current_id] = 1;

        IDs &= ~(1 << current_id);
    }

    /* Return the status. */
    return status;
}

gceSTATUS gcoKERNEL_Notify(
    IN gcoGALDEVICE Device
)
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 data = 0;
    gctUINT32 storedData;

    /* Read AQIntrAcknowledge register. */
    gcoOS_ReadRegister(Device, 0x00010, &data);

    /* Combine with stored interrupt acknowledge register. */
    gcoOS_AtomicExchange(&Device->data, 0, &storedData);
    data |= storedData;

    /* Success. */
    return status;
}


irqreturn_t isrRoutine(int irq, void *ctxt)
{
    gcoGALDEVICE device = (gcoGALDEVICE)ctxt;
    int handled = 0;
    gctUINT32 data;
    gceSTATUS hasInterrupt;

    /* Read AQIntrAcknowledge register. */
    gcoOS_ReadRegister(device, 0x00010, &data);

    device->data |= data;
    gcmTRACE_ZONE("[isr]data 0x%x\n", device->data);
    hasInterrupt = (data == 0) ? gcvSTATUS_NOT_OUR_INTERRUPT : gcvSTATUS_OK;
    /* Call kernel interrupt notification. */
    if (hasInterrupt == gcvSTATUS_OK)
    {
        device->dataReady = gcvTRUE;

        up(&device->sema);

        handled = 1;
    }

    return IRQ_RETVAL(handled);
}

int threadRoutine(void *ctxt)
{
    gcoGALDEVICE device = (gcoGALDEVICE)ctxt;

    gcmTRACE_ZONE("Starting isr Thread with extension->0x%x\n",
                  (gctUINT32)device);

    while (1)
    {
        static int down;
        down = down_interruptible(&device->sema);
        device->dataReady = gcvFALSE;

        if (device->killThread == gcvTRUE)
        {
            return 0;
        }

        /* Wait for the interrupt. */
        if (kthread_should_stop())
        {
            return 0;
        }
    }

    return 0;
}

/*******************************************************************************
**
**	gcoGALDEVICE_Setup_ISR
**
**	Start the ISR routine.
**
**	INPUT:
**
**		gcoGALDEVICE Device
**			Pointer to an gcoGALDEVICE object.
**
**	OUTPUT:
**
**		Nothing.
**
**	RETURNS:
**
**		gcvSTATUS_OK
**			Setup successfully.
**		gcvSTATUS_GENERIC_IO
**			Setup failed.
*/
gceSTATUS
gcoGALDEVICE_Setup_ISR(
    IN gcoGALDEVICE Device
)
{
    gctINT ret;
    gctUINT32 data;

    gcmVERIFY_ARGUMENT(Device != NULL);

    if (Device->irqLine == 0)
    {
        return gcvSTATUS_GENERIC_IO;
    }

    /* Hook up the isr based on the irq line. */
    ret = request_irq(Device->irqLine,
                      isrRoutine,
                      IRQF_DISABLED,
                      "galcore interrupt service",
                      Device);

    if (ret != 0)
    {
        gcmTRACE_ZONE("[galcore] gcoGALDEVICE_Setup_ISR: "
                      "Could not register irq line->%d\n",
                      Device->irqLine);

        Device->isrInitialized = gcvFALSE;

        return gcvSTATUS_GENERIC_IO;
    }

    Device->isrInitialized = gcvTRUE;

    gcmTRACE_ZONE("[galcore] gcoGALDEVICE_Setup_ISR: "
                  "Setup the irq line->%d\n",
                  Device->irqLine);

    gcoOS_ReadRegister(Device, 0x00628, &data);

    //WriteGiQuilaReg(AQ_INTR_ENBL_Address, 0);
    gcoOS_WriteRegister(Device, 0x00014, 0);

    // clean up IAK to pull INTR low
    //signalGiQuila = ReadGiQuilaReg(AQ_INTR_ACKNOWLEDGE_Address);
    gcoOS_ReadRegister(Device, 0x00010/*AQ_INTR_ACKNOWLEDGE_Address*/, &data);
    printk("Clear GiQuila INTR = %08x\n", data);

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**	gcoGALDEVICE_Release_ISR
**
**	Release the irq line.
**
**	INPUT:
**
**		gcoGALDEVICE Device
**			Pointer to an gcoGALDEVICE object.
**
**	OUTPUT:
**
**		Nothing.
**
**	RETURNS:
**
**		Nothing.
*/
gceSTATUS
gcoGALDEVICE_Release_ISR(
    IN gcoGALDEVICE Device
)
{
    gcmVERIFY_ARGUMENT(Device != NULL);

    /* release the irq */
    if (Device->isrInitialized)
    {
        free_irq(Device->irqLine, Device);
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**	gcoGALDEVICE_Start_Thread
**
**	Start the daemon thread.
**
**	INPUT:
**
**		gcoGALDEVICE Device
**			Pointer to an gcoGALDEVICE object.
**
**	OUTPUT:
**
**		Nothing.
**
**	RETURNS:
**
**		gcvSTATUS_OK
**			Start successfully.
**		gcvSTATUS_GENERIC_IO
**			Start failed.
*/
gceSTATUS
gcoGALDEVICE_Start_Thread(
    IN gcoGALDEVICE Device
)
{
    gcmVERIFY_ARGUMENT(Device != NULL);

    /* start the kernel thread */
    Device->threadCtxt = kthread_run(threadRoutine,
                                     Device,
                                     "galcore daemon thread");

    Device->threadInitialized = gcvTRUE;
    gcmTRACE_ZONE("[galcore] gcoGALDEVICE_Start_Thread: "
                  "Stat the daemon thread.\n");

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**	gcoGALDEVICE_Stop_Thread
**
**	Stop the gal device, including the following actions: stop the daemon
**	thread, release the irq.
**
**	INPUT:
**
**		gcoGALDEVICE Device
**			Pointer to an gcoGALDEVICE object.
**
**	OUTPUT:
**
**		Nothing.
**
**	RETURNS:
**
**		Nothing.
*/
gceSTATUS
gcoGALDEVICE_Stop_Thread(
    gcoGALDEVICE Device
)
{
    gcmVERIFY_ARGUMENT(Device != NULL);

    /* stop the thread */
    if (Device->threadInitialized)
    {
        Device->killThread = gcvTRUE;
        up(&Device->sema);

        kthread_stop(Device->threadCtxt);
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**	gcoGALDEVICE_Start
**
**	Start the gal device, including the following actions: setup the isr routine
**  and start the daemoni thread.
**
**	INPUT:
**
**		gcoGALDEVICE Device
**			Pointer to an gcoGALDEVICE object.
**
**	OUTPUT:
**
**		Nothing.
**
**	RETURNS:
**
**		gcvSTATUS_OK
**			Start successfully.
*/
gceSTATUS
gcoGALDEVICE_Start(
    IN gcoGALDEVICE Device
)
{
    /* start the daemon thread */
#ifndef USE_NEW_INTERRUPT
    gcoGALDEVICE_Start_Thread(Device);
#endif
    /* setup the isr routine */
    gcoGALDEVICE_Setup_ISR(Device);

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**	gcoGALDEVICE_Stop
**
**	Stop the gal device, including the following actions: stop the daemon
**	thread, release the irq.
**
**	INPUT:
**
**		gcoGALDEVICE Device
**			Pointer to an gcoGALDEVICE object.
**
**	OUTPUT:
**
**		Nothing.
**
**	RETURNS:
**
**		Nothing.
*/
gceSTATUS
gcoGALDEVICE_Stop(
    gcoGALDEVICE Device
)
{
    gcmVERIFY_ARGUMENT(Device != NULL);

    if (Device->isrInitialized)
    {
        gcoGALDEVICE_Release_ISR(Device);
    }

    if (Device->threadInitialized)
    {
#ifndef USE_NEW_INTERRUPT
        gcoGALDEVICE_Stop_Thread(Device);
#endif
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoGALDEVICE_Construct
**
**  Constructor.
**
**  INPUT:
**
**  OUTPUT:
**
**  	gcoGALDEVICE * Device
**  	    Pointer to a variable receiving the gcoGALDEVICE object pointer on
**  	    success.
*/
gceSTATUS
gcoGALDEVICE_Construct(
    IN gctINT IrqLine,
    IN gctUINT32 RegisterMemBase,
    IN gctSIZE_T RegisterMemSize,
    IN gctUINT32 BaseAddress,
    IN gctINT Signal,
    OUT gcoGALDEVICE *Device
)
{
    gctUINT32 physical;
    gcoGALDEVICE device;
    //gceSTATUS status;

    gcmTRACE("[galcore] Enter gcoGALDEVICE_Construct\n");

    /* Allocate device structure. */
    device = (gcoGALDEVICE)kzalloc(sizeof(struct _gcoGALDEVICE), GFP_KERNEL);
    if (!device)
    {
        gcmTRACE_ZONE("[galcore] gcoGALDEVICE_Construct: Can't allocate memory.\n");

        return gcvSTATUS_OUT_OF_MEMORY;
    }

    physical = RegisterMemBase;

    /* Set up register memory region */
    if (physical != 0)
    {
        /* Request a region. */
        request_mem_region(RegisterMemBase, RegisterMemSize, "galcore register region");
        device->registerBase = (gctPOINTER) ioremap_nocache(RegisterMemBase,
                               RegisterMemSize);
        if (!device->registerBase)
        {
            gcmTRACE_ZONE("[galcore] %s: Unable to map location->0x%X for size->%ld\n",
                          __FUNCTION__,
                          RegisterMemBase,
                          RegisterMemSize);

            return gcvSTATUS_OUT_OF_RESOURCES;
        }

        physical += RegisterMemSize;

        gcmTRACE_ZONE("[galcore] gcoGALDEVICE_Construct: "
                      "RegisterBase after mapping Address->0x%x is 0x%x\n",
                      (gctUINT32)RegisterMemBase,
                      (gctUINT32)device->registerBase);
    }

    /* construct the gcoOS object */
    device->baseAddress = BaseAddress;

    /* initialize the thread daemon */
    sema_init(&device->sema, 0);
    device->threadInitialized = gcvFALSE;
    device->killThread = gcvFALSE;

    /* initialize the isr */
    device->isrInitialized = gcvFALSE;
    device->dataReady = gcvFALSE;
    device->irqLine = IrqLine;

    device->signal = Signal;
    *Device = device;

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**	gcoGALDEVICE_Destroy
**
**	Class destructor.
**
**	INPUT:
**
**		Nothing.
**
**	OUTPUT:
**
**		Nothing.
**
**	RETURNS:
**
**		Nothing.
*/
gceSTATUS
gcoGALDEVICE_Destroy(
    gcoGALDEVICE Device)
{
    gcmVERIFY_ARGUMENT(Device != NULL);

    gcmTRACE("[ENTER] gcoGALDEVICE_Destroy\n");

    if (Device->registerBase)
    {
        iounmap(Device->registerBase);
    }

    kfree(Device);

    gcmTRACE("[galcore] Leave gcoGALDEVICE_Destroy\n");

    return gcvSTATUS_OK;
}

