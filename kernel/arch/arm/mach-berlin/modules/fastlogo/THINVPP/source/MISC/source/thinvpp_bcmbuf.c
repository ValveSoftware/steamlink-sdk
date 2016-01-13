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

#define _THINVPP_BCMBUF_C

#include "thinvpp_module.h"
#include "thinvpp_common.h"
#include "Galois_memmap.h"
#include "avio.h"
#include "galois_io.h"
#include "maddr.h"

#include <linux/types.h>
#include <linux/delay.h> // for msleep()
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/io.h>

static void inner_outer_flush_dcache_area(void *addr, size_t length)
{
	phys_addr_t start, end;

    __cpuc_flush_dcache_area(addr, length);

	start = virt_to_phys(addr);
	end   = start + length;

	outer_cache.flush_range(start, end);
}

/***************************************************************
 * FUNCTION: allocate register programming buffer
 * PARAMS: *buf - pointer to a register programming buffer
 *       : size - size of the buffer to allocate
 *       :      - (should be a multiple of 4)
 * RETURN:  1 - succeed
 *          0 - failed to initialize a BCM buffer
 ****************************************************************/
#if LOGO_USE_SHM
int THINVPP_BCMBUF_Set(BCMBUF *pbcmbuf, void *addr, unsigned phys, int size)
{
    if (size <= 0)
        return (MV_THINVPP_EBADPARAM);

    /* allocate memory for the buffer */
    pbcmbuf->addr = (int)addr;
    if(!pbcmbuf->addr)
        return MV_THINVPP_ENOMEM;
    pbcmbuf->phys = phys;

    pbcmbuf->size = size;
    pbcmbuf->head = (unsigned int *)((pbcmbuf->addr+0x1f)&(~0x01f));
    if (pbcmbuf->head != (unsigned int *)pbcmbuf->addr)
        pbcmbuf->size = size-64;

    return MV_THINVPP_OK;
}
#else
int THINVPP_BCMBUF_Create(BCMBUF *pbcmbuf, int size)
{
    if (size <= 0)
        return (MV_THINVPP_EBADPARAM);

    /* allocate memory for the buffer */
    pbcmbuf->addr = (int)THINVPP_MALLOC(size);
    if(!pbcmbuf->addr)
        return MV_THINVPP_ENOMEM;

    pbcmbuf->size = size;
    pbcmbuf->head = (unsigned int *)((pbcmbuf->addr+0x1f)&(~0x01f));
    if (pbcmbuf->head != (unsigned int *)pbcmbuf->addr)
        pbcmbuf->size = size-64;

    return MV_THINVPP_OK;
}
#endif


/***************************************************************
 * FUNCTION: free register programming buffer
 * PARAMS: *buf - pointer to a register programming buffer
 * RETURN:  1 - succeed
 *          0 - failed to initialize a BCM buffer
 ****************************************************************/
int THINVPP_BCMBUF_Destroy(BCMBUF *pbcmbuf)
{
    /* allocate memory for the buffer */
    if (!pbcmbuf->addr)
        return (MV_THINVPP_EBADCALL);

#if !LOGO_USE_SHM
    THINVPP_FREE((int *)(pbcmbuf->addr));
#endif

    pbcmbuf->addr = 0;
    pbcmbuf->head = NULL;

    return MV_THINVPP_OK;
}


/***************************************************************
 * FUNCTION: reset a register programming buffer
 * PARAMS: *buf - pointer to a register programming buffer
 * RETURN:  1 - succeed
 *          0 - failed to initialize a BCM buffer
 ****************************************************************/
int THINVPP_BCMBUF_Reset(BCMBUF *pbcmbuf)
{
    pbcmbuf->tail = pbcmbuf->head + (pbcmbuf->size/4);

    /*set pointers to the head*/
    pbcmbuf->writer = pbcmbuf->head;
    pbcmbuf->dv1_head = pbcmbuf->head;
    //pbcmbuf->dv3_head = pbcmbuf->dv1_head + (pbcmbuf->size/16)*3;
    pbcmbuf->subID = -1; /* total */

    return MV_THINVPP_OK;
}

/*********************************************************
 * FUNCTION: Select sub register programming buffer
 * PARAMS: *buf - pointer to the buffer descriptor
 *         subID - CPCB_1, CPCB_2, CPCB_3 or total
 ********************************************************/
void THINVPP_BCMBUF_Select(BCMBUF *pbcmbuf, int subID)
{
    /* reset read/write pointer of the buffer */
    if (subID == CPCB_1){
        pbcmbuf->writer = pbcmbuf->dv1_head;
    //} else if (subID == CPCB_3) {
        //pbcmbuf->writer = pbcmbuf->dv3_head;
    //} else {
        //pbcmbuf->writer = pbcmbuf->head;
    }

    pbcmbuf->subID = subID;

    return;
}

#if !LOGO_USE_SHM
/*********************************************************
 * FUNCTION: write register address (4 bytes) and value (4 bytes) to the buffer
 * PARAMS: *buf - pointer to the buffer descriptor
 *               address - address of the register to be set
 *               value - the value to be written into the register
 * RETURN: 1 - succeed
 *               0 - register programming buffer is full
 ********************************************************/
int THINVPP_BCMBUF_Write(BCMBUF *pbcmbuf, unsigned int address, unsigned int value)
{
    unsigned int *end;

    /*if not enough space for storing another 8 bytes, wrap around happens*/
    if (pbcmbuf->subID == CPCB_1)
        end = pbcmbuf->dv3_head;
    else
        end = pbcmbuf->tail;

    if(pbcmbuf->writer == end){
        /*the buffer is full, no space for wrap around*/
        return MV_THINVPP_EBCMBUFFULL;
    }

    /*save the data to the buffer*/
   *pbcmbuf->writer = value;
    pbcmbuf->writer ++;
   *pbcmbuf->writer = address;
    pbcmbuf->writer ++;

    return MV_THINVPP_OK;
}
#endif

#if LOGO_USE_SHM
int THINVPP_CFGQ_Set(DHUB_CFGQ *cfgQ, void *addr, unsigned phys, int size)
{
    if (size <= 0)
        return (MV_THINVPP_EBADPARAM);

    /* allocate memory for the buffer */
    cfgQ->base_addr = (int)addr;
    if(!cfgQ->base_addr)
        return MV_THINVPP_ENOMEM;
    cfgQ->phys = phys;
    cfgQ->addr = (int *)((cfgQ->base_addr+0x1f)&(~0x01f));
    cfgQ->len = 0;

    return MV_THINVPP_OK;
}
#else
int THINVPP_CFGQ_Create(DHUB_CFGQ *cfgQ, int size)
{
    if (size <= 0)
        return (MV_THINVPP_EBADPARAM);

    /* allocate memory for the buffer */
    cfgQ->base_addr = (int)THINVPP_MALLOC(size);
    if(!cfgQ->base_addr)
        return MV_THINVPP_ENOMEM;
    cfgQ->addr = (int *)((cfgQ->base_addr+0x1f)&(~0x01f));
    cfgQ->len = 0;

    return MV_THINVPP_OK;
}
#endif

int THINVPP_CFGQ_Destroy(DHUB_CFGQ *cfgQ)
{
    int i;
    if (!cfgQ->base_addr )
        return (MV_THINVPP_EBADCALL);

    for(i=0; i<cfgQ->len; i++);
    {
        //Dummy register: system timer couter1 divider, not in use!
        *(cfgQ->addr+i*2) = 0;
        *(cfgQ->addr+i*2+1) = 0xf7f60000 + (VOP_HDMI_SEL<<2);
    }

#if !LOGO_USE_SHM
    THINVPP_FREE((int *)(cfgQ->base_addr));
#endif

    cfgQ->base_addr = 0;
    cfgQ->addr = 0;

    return MV_THINVPP_OK;
}

/*******************************************************************************
 * FUNCTION: commit cfgQ which contains BCM DHUB programming info to interrupt service routine
 * PARAMS: *cfgQ - cfgQ
 *         cpcbID - cpcb ID which this cmdQ belongs to
 *         intrType - interrupt type which this cmdQ belongs to: 0 - VBI, 1 - VDE
 * NOTE: this API is only called from VBI/VDE ISR.
 *******************************************************************************/
int THINVPP_BCMDHUB_CFGQ_Commit(DHUB_CFGQ *cfgQ, int cpcbID)
{
    unsigned int sched_qid;
    unsigned int bcm_sched_cmd[2];

    static int bcm_count = 0;
    bcm_count++;

    if (cfgQ->len <= 0)
        return MV_THINVPP_EBADPARAM;

    if (cpcbID == CPCB_1)
        sched_qid = BCM_SCHED_Q0;
    else if (cpcbID == CPCB_2)
        sched_qid = BCM_SCHED_Q1;
    else
        sched_qid = BCM_SCHED_Q2;

    {
        //check BCM Q status before use
        int sched_stat;

        BCM_SCHED_GetEmptySts(sched_qid, &sched_stat);
        if (sched_stat == 0)
        {
            printk("****************[VPP fastlogo]ERROR! Q%d SCHED QUEUE OVERFLOW!!!!*************\n", sched_qid);
            printk("[VPP fastlogo] BCM Q fulless status: %X\n", MV_MEMIO32_READ(MEMMAP_AVIO_BCM_REG_BASE+RA_AVIO_BCM_FULL_STS));
            return MV_THINVPP_EIOFAIL;
        }
    }

#if LOGO_USE_SHM
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, cfgQ->phys, (int)cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#else
    inner_outer_flush_dcache_area(cfgQ->addr, cfgQ->len*8);
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)virt_to_phys(cfgQ->addr), (int)cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#endif
    while( !BCM_SCHED_PushCmd(sched_qid, bcm_sched_cmd, NULL));

    return MV_THINVPP_OK;
}

/*********************************************************************
 * FUNCTION: send a BCM BUF info to a BCM cfgQ
 * PARAMS: *pbcmbuf - pointer to the BCMBUF
 *         *cfgQ - target BCM cfgQ
 * NOTE: this API is only called from VBI/VDE ISR.
 ********************************************************************/
int THINVPP_BCMBUF_To_CFGQ(BCMBUF *pbcmbuf, DHUB_CFGQ *cfgQ)
{
    int size;
    unsigned int bcm_sched_cmd[2];
    unsigned int *start;

#if LOGO_USE_SHM
    start = pbcmbuf->dv1_head;
#else
    if (pbcmbuf->subID == CPCB_1)
        start = pbcmbuf->dv1_head;
    else if (pbcmbuf->subID == CPCB_3)
        start = pbcmbuf->dv3_head;
    else
        start = pbcmbuf->head;
#endif

    size = (int)pbcmbuf->writer-(int)start;

    if (size <= 0)
        return MV_THINVPP_EBADPARAM;

#if LOGO_USE_SHM
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, pbcmbuf->phys, size, 0, 0, 0, 1, bcm_sched_cmd);
#else
    inner_outer_flush_dcache_area(start, size);
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)virt_to_phys(start), size, 0, 0, 0, 1, bcm_sched_cmd);
#endif
    while( !BCM_SCHED_PushCmd(BCM_SCHED_Q13, bcm_sched_cmd, cfgQ->addr + cfgQ->len*2));
    cfgQ->len += 2;

    return MV_THINVPP_OK;
}

/*********************************************************************
 * FUNCTION: send a BCM cfgQ info to a BCM cfgQ
 * PARAMS: src_cfgQ - pointer to the source BCM cfgQ
 *         *cfgQ - target BCM cfgQ
 * NOTE: this API is only called from VBI/VDE ISR.
 ********************************************************************/
void THINVPP_CFGQ_To_CFGQ(DHUB_CFGQ *src_cfgQ, DHUB_CFGQ *cfgQ)
{
    unsigned int bcm_sched_cmd[2];

    if (src_cfgQ->len <= 0)
        return;

#if LOGO_USE_SHM
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, src_cfgQ->phys, (int)src_cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#else
    inner_outer_flush_dcache_area(src_cfgQ->addr, src_cfgQ->len*8);
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)virt_to_phys(src_cfgQ->addr), (int)src_cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#endif
    while( !BCM_SCHED_PushCmd(BCM_SCHED_Q13, bcm_sched_cmd, cfgQ->addr + cfgQ->len*2));
    cfgQ->len += 2;
}

