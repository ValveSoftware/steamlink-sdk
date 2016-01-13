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

typedef struct _gcoGALDEVICE
{
    /* Added for gc100. */
    gctUINT32           data;
    gctUINT32           event[32];


    /* Attributes. */
    gctPOINTER			registerBase;
    gctSIZE_T			registerSize;
    gctUINT32			baseAddress;

    /* IRQ management. */
    gctINT				irqLine;
    gctBOOL				isrInitialized;
    gctBOOL				dataReady;

    /* Thread management. */
    struct task_struct	*threadCtxt;
    struct semaphore	sema;
    gctBOOL				threadInitialized;
    gctBOOL				killThread;

    /* Signal management. */
    gctINT				signal;
}
* gcoGALDEVICE;

typedef struct _gcsHAL_PRIVATE_DATA
{
    gcoGALDEVICE		device;
}
gcsHAL_PRIVATE_DATA, * gcsHAL_PRIVATE_DATA_PTR;

typedef struct _DRIVER_ARGS
{
    gctPOINTER 				InputBuffer;
    gctUINT32  				InputBufferSize;
    gctPOINTER 				OutputBuffer;
    gctUINT32				OutputBufferSize;
}
DRIVER_ARGS;

gceSTATUS gcoCOMMAND_Commit(
    IN gcoGALDEVICE Device,
    OUT gctUINT32 *Result);

gceSTATUS gcoOS_ReadRegister(
    IN 	gcoGALDEVICE Device,
    IN gctUINT32 Address,
    OUT gctUINT32 * Data
);

gceSTATUS gcoOS_WriteRegister(
    IN	gcoGALDEVICE Device,
    IN gctUINT32 Address,
    IN gctUINT32 Data
);

gceSTATUS gcoKERNEL_Dispatch(
    IN gcoGALDEVICE Device,
    IN gctBOOL FromUser,
    IN OUT gcsHAL_INTERFACE * Interface
);

gceSTATUS gcoGALDEVICE_Start(
    IN gcoGALDEVICE Device
);

gceSTATUS gcoGALDEVICE_Stop(
    gcoGALDEVICE Device
);

gceSTATUS gcoGALDEVICE_Construct(
    IN gctINT IrqLine,
    IN gctUINT32 RegisterMemBase,
    IN gctSIZE_T RegisterMemSize,
    IN gctUINT32 BaseAddress,
    IN gctINT Signal,
    OUT gcoGALDEVICE *Device
);

gceSTATUS gcoGALDEVICE_Destroy(
    gcoGALDEVICE Device);

gceSTATUS
gcoOS_AtomicExchange(
    IN OUT gctUINT32_PTR Target,
    IN gctUINT32 NewValue,
    OUT gctUINT32_PTR OldValue
);

