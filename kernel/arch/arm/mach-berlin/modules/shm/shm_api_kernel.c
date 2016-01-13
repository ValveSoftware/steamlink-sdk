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

/*******************************************************************************
  System head files
*/

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>

/*******************************************************************************
  Local head files
*/

#include "shm_type.h"
#include "shm_device.h"
#include "shm_api.h"
/*******************************************************************************
  Module Variable
*/

MV_SHM_BaseInfo_t MV_SHM_CACHE_BASEINFO_DATA = { 0, 0, 0, 0, -1 };

MV_SHM_BaseInfo_t MV_SHM_NONCACHE_BASEINFO_DATA = { 0, 0, 0, 0, -1 };

shm_device_t *shm_api_device_cache = NULL;

shm_device_t *shm_api_device_noncache = NULL;

static unsigned long MV_SHM_ioremap(unsigned long phys_start, unsigned long size)
{
	struct vm_struct *area;

	area = get_vm_area(size, VM_IOREMAP);
	if (area == NULL)
		return ((uint) NULL);

	if (ioremap_page_range((unsigned long)area->addr,
		(unsigned long)area->addr + size, phys_start, PAGE_KERNEL)) {
		return ((uint) NULL);
	}
	return area->addr;
}


/*******************************************************************************
  Module API: for kernel module use only, mainly CBuf and Graphics driver, suppose to be removed later
*/
//the device should be match with ioremap cachablity
//we still need to map both cache and non-cache device, since kernel(driver) will use the addr convert
int MV_SHM_Init(shm_device_t * device_noncache, shm_device_t * device_cache)
{
	int res;

	if (device_noncache == NULL || device_cache == NULL)
		return -EINVAL;

	shm_api_device_noncache = device_noncache;

	/* prepare MV_SHM_BASEINFO_DATA */
	res = shm_device_get_baseinfo(shm_api_device_noncache,
				&MV_SHM_NONCACHE_BASEINFO_DATA);
	if (res != 0) {
		shm_error("shm_device_get_baseinfo failed. (%d)", res);
		goto err_exit1;
	}
	MV_SHM_NONCACHE_BASEINFO_DATA.m_size = device_noncache->m_size;

	/* map phyiscal address to kernel virtual address */
	shm_trace("memory ioremap_noncache, base:0x%08X, size:0x%08X\n",
		  MV_SHM_NONCACHE_BASEINFO_DATA.m_base_physaddr,
		  MV_SHM_NONCACHE_BASEINFO_DATA.m_size);
	MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr =
	    (uint)
	    ioremap_nocache(MV_SHM_NONCACHE_BASEINFO_DATA.m_base_physaddr,
			    MV_SHM_NONCACHE_BASEINFO_DATA.m_size);
	if (MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr == (uint) NULL) {
		shm_error("ioremap_nocache failed.");
		res = -ENOMEM;
		goto err_exit1;
	}
	device_noncache->m_engine->m_cache_or_noncache = SHM_NONCACHE;
	device_noncache->m_engine->m_virt_base =
		MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr;
/////////////////////map cache device in kernel space//////////////////////////
	shm_api_device_cache = device_cache;

	/* prepare MV_SHM_BASEINFO_DATA */
	res = shm_device_get_baseinfo(shm_api_device_cache,
				&MV_SHM_CACHE_BASEINFO_DATA);
	if (res != 0) {
		shm_error("shm_device_get_baseinfo failed. (%d)", res);
		goto err_exit1;
	}
	MV_SHM_CACHE_BASEINFO_DATA.m_size = device_cache->m_size;

	/* map phyiscal address to kernel virtual address */
	shm_trace("memory ioremap, base:0x%08X, size:0x%08X\n",
		  MV_SHM_CACHE_BASEINFO_DATA.m_base_physaddr,
		  MV_SHM_CACHE_BASEINFO_DATA.m_size);

	MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr =
	(uint)MV_SHM_ioremap(MV_SHM_CACHE_BASEINFO_DATA.m_base_physaddr,
				MV_SHM_CACHE_BASEINFO_DATA.m_size);
	if (MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr == (uint) NULL) {
		shm_error("ioremap failed.");
		res = -ENOMEM;
		goto err_exit2;
	}
	device_cache->m_engine->m_cache_or_noncache = SHM_CACHE;
	device_cache->m_engine->m_virt_base =
		MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr;

	shm_trace("memory base virt addr (cache)       = 0x%08X\n",
			MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr);
	shm_trace("memory base virt addr (non-cache)   = 0x%08X\n",
			MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr);

	shm_trace("MV_SHM_Init OK\n");

	return 0;

err_exit2:

	// unmap kernel virtual address
	iounmap((void *)(MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr));

err_exit1:

	shm_api_device_noncache = shm_api_device_cache = NULL;

	shm_error("MV_SHM_Init failed !!!\n");

	return res;
}

int MV_SHM_Exit(void)
{
	if (shm_api_device_noncache == NULL || shm_api_device_cache == NULL)
		return -EINVAL;

	/* unmap kernel virtual address */
	iounmap((void *)(MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr));
	iounmap((void *)(MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr));

	shm_api_device_noncache = shm_api_device_cache = NULL;

	shm_trace("MV_SHM_Exit OK\n");

	return 0;
}

#if 1				//no one should use this api!!!
int MV_SHM_GetCacheMemInfo(pMV_SHM_MemInfo_t pInfo)
{
	if (pInfo == NULL)
		return -EINVAL;

	if (shm_api_device_cache == NULL)
		return -ENODEV;

	return shm_device_get_meminfo(shm_api_device_cache, pInfo);
}
#endif

int MV_SHM_GetCacheBaseInfo(pMV_SHM_BaseInfo_t pInfo)
{
	if (pInfo == NULL)
		return -EINVAL;

	if (shm_api_device_cache == NULL)
		return -ENODEV;

	memcpy(pInfo, &MV_SHM_CACHE_BASEINFO_DATA,
	       sizeof(MV_SHM_CACHE_BASEINFO_DATA));

	return 0;
}

//these malloc are for cache, now in kernel space we have separate api just as user space did
size_t MV_SHM_Malloc(size_t Size, size_t Alignment)
{
	size_t Offset;

	if ((shm_api_device_cache == NULL) || (Size == 0) || (Alignment % 2))
		return ERROR_SHM_MALLOC_FAILED;

	if (shm_check_alignment(Alignment) != 0)
		return ERROR_SHM_MALLOC_FAILED;

	shm_round_size(Size);
	shm_round_alignment(Alignment);

	Offset = shm_device_allocate(shm_api_device_cache, Size, Alignment);

	return Offset;
}

int MV_SHM_Takeover(size_t Offset)
{
	if (NULL == shm_api_device_cache)
		return -ENODEV;

	return shm_device_takeover(shm_api_device_cache, Offset);
}

int MV_SHM_Giveup(size_t Offset)
{
	if (NULL == shm_api_device_cache)
		return -ENODEV;

	return shm_device_giveup(shm_api_device_cache, Offset);
}

int MV_SHM_Free(size_t Offset)
{
	if (shm_api_device_cache == NULL)
		return -ENODEV;

	return shm_device_free(shm_api_device_cache, Offset);
}

//these malloc are for non-cache
size_t MV_SHM_NONCACHE_Malloc(size_t Size, size_t Alignment)
{
	size_t Offset;

	if ((shm_api_device_noncache == NULL) || (Size == 0) || (Alignment % 2))
		return ERROR_SHM_MALLOC_FAILED;

	if (shm_check_alignment(Alignment) != 0)
		return ERROR_SHM_MALLOC_FAILED;

	shm_round_size(Size);
	shm_round_alignment(Alignment);

	Offset = shm_device_allocate(shm_api_device_noncache, Size, Alignment);

	return Offset;
}

int MV_SHM_NONCACHE_Free(size_t Offset)
{
	if (shm_api_device_noncache == NULL)
		return -ENODEV;

	return shm_device_free(shm_api_device_noncache, Offset);
}

void *MV_SHM_GetNonCacheVirtAddr(size_t Offset)
{
	if ((shm_api_device_noncache == NULL)
	    || (Offset >= MV_SHM_NONCACHE_BASEINFO_DATA.m_size))
		return NULL;
	return (void *)(Offset + MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr);
}

void *MV_SHM_GetCacheVirtAddr(size_t Offset)
{
	if ((shm_api_device_cache == NULL)
	    || (Offset >= MV_SHM_CACHE_BASEINFO_DATA.m_size))
		return NULL;

	return (void *)(Offset + MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr);
}

void *MV_SHM_GetNonCachePhysAddr(size_t Offset)
{
	if ((shm_api_device_noncache == NULL)
	    || (Offset >= MV_SHM_NONCACHE_BASEINFO_DATA.m_size))
		return NULL;

	return (void *)(Offset + MV_SHM_NONCACHE_BASEINFO_DATA.m_base_physaddr);
}

void *MV_SHM_GetCachePhysAddr(size_t Offset)
{
	if ((shm_api_device_cache == NULL)
	    || (Offset >= MV_SHM_CACHE_BASEINFO_DATA.m_size))
		return NULL;

	return (void *)(Offset + MV_SHM_CACHE_BASEINFO_DATA.m_base_physaddr);
}

size_t MV_SHM_RevertNonCacheVirtAddr(void *ptr)
{
	if (shm_api_device_noncache == NULL)
		return ERROR_SHM_MALLOC_FAILED;

	if (((size_t) ptr < MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr) ||
	    ((size_t) ptr >=
	     MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr +
	     MV_SHM_NONCACHE_BASEINFO_DATA.m_size))
		return ERROR_SHM_MALLOC_FAILED;

	return ((size_t) ptr - MV_SHM_NONCACHE_BASEINFO_DATA.m_base_virtaddr);
}

size_t MV_SHM_RevertCacheVirtAddr(void *ptr)
{
	if (shm_api_device_cache == NULL)
		return ERROR_SHM_MALLOC_FAILED;

	if (((size_t) ptr < MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr) ||
	    ((size_t) ptr >=
	     MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr +
	     MV_SHM_CACHE_BASEINFO_DATA.m_size))
		return ERROR_SHM_MALLOC_FAILED;

	return ((size_t) ptr - MV_SHM_CACHE_BASEINFO_DATA.m_base_virtaddr);
}

size_t MV_SHM_RevertNonCachePhysAddr(void *ptr)
{
	if (shm_api_device_noncache == NULL)
		return ERROR_SHM_MALLOC_FAILED;

	if (((size_t) ptr < MV_SHM_NONCACHE_BASEINFO_DATA.m_base_physaddr) ||
	    ((size_t) ptr >=
	     MV_SHM_NONCACHE_BASEINFO_DATA.m_base_physaddr +
	     MV_SHM_NONCACHE_BASEINFO_DATA.m_size))
		return ERROR_SHM_MALLOC_FAILED;

	return ((size_t) ptr - MV_SHM_NONCACHE_BASEINFO_DATA.m_base_physaddr);
}

size_t MV_SHM_RevertCachePhysAddr(void *ptr)
{
	if (shm_api_device_cache == NULL)
		return ERROR_SHM_MALLOC_FAILED;

	if (((size_t) ptr < MV_SHM_CACHE_BASEINFO_DATA.m_base_physaddr) ||
	    ((size_t) ptr >=
	     MV_SHM_CACHE_BASEINFO_DATA.m_base_physaddr +
	     MV_SHM_CACHE_BASEINFO_DATA.m_size))
		return ERROR_SHM_MALLOC_FAILED;

	return ((size_t) ptr - MV_SHM_CACHE_BASEINFO_DATA.m_base_physaddr);
}

EXPORT_SYMBOL(MV_SHM_Malloc);
EXPORT_SYMBOL(MV_SHM_Free);
EXPORT_SYMBOL(MV_SHM_Takeover);
EXPORT_SYMBOL(MV_SHM_Giveup);
EXPORT_SYMBOL(MV_SHM_NONCACHE_Malloc);
EXPORT_SYMBOL(MV_SHM_NONCACHE_Free);
EXPORT_SYMBOL(MV_SHM_GetNonCacheVirtAddr);
EXPORT_SYMBOL(MV_SHM_GetCacheVirtAddr);
EXPORT_SYMBOL(MV_SHM_GetNonCachePhysAddr);
EXPORT_SYMBOL(MV_SHM_GetCachePhysAddr);
EXPORT_SYMBOL(MV_SHM_RevertNonCacheVirtAddr);
EXPORT_SYMBOL(MV_SHM_RevertCacheVirtAddr);
EXPORT_SYMBOL(MV_SHM_RevertNonCachePhysAddr);
EXPORT_SYMBOL(MV_SHM_RevertCachePhysAddr);
EXPORT_SYMBOL(MV_SHM_GetCacheMemInfo);
EXPORT_SYMBOL(MV_SHM_GetCacheBaseInfo);
