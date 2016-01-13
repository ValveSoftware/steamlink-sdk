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

#ifndef __shm_api_h__
#define __shm_api_h__

#include <linux/types.h>

#define ERROR_SHM_MALLOC_FAILED             (0xFFFFFFFF)

typedef struct _MV_SHM_MemInfo {

		size_t m_totalmem;                      // total size of entire shared memory
		size_t m_usedmem;                       // total memory in use
		size_t m_freemem;                       // total space in bytes not in use

		size_t m_peak_usedmem;                  // the peak of memory in use history
		size_t m_max_freeblock;                 // size of largest unused block
		size_t m_min_freeblock;                 // size of smallest unused block
		size_t m_max_usedblock;                 // size of largest memory used
		size_t m_min_usedblock;                 // size of smallest memory used
		uint m_num_freeblock;                 // amount of unused block
		uint m_num_usedblock;                 // amount of unused block

} MV_SHM_MemInfo_t, *pMV_SHM_MemInfo_t;

typedef struct _MV_SHM_BaseInfo {

		size_t m_size;                          // [shm_device user/kernel] size of shared memory
		size_t m_threshold;                     // [shm_device user/kernel] threshold of shared memory

		size_t m_base_physaddr;           // [shm_device user/kernel] physical address of cacheable memory base
		size_t m_base_virtaddr;           // [shm_api user/kernel] virtual address of cacheable memory base

		int m_fd;                         // [shm_api user] file descriptor associated with shared memory cache

} MV_SHM_BaseInfo_t, *pMV_SHM_BaseInfo_t;

size_t  MV_SHM_Malloc(size_t Size, size_t Alignment);
size_t MV_SHM_NONCACHE_Malloc( size_t Size, size_t Alignment);
int MV_SHM_Free( size_t Offset);
int MV_SHM_NONCACHE_Free( size_t Offset);
int MV_SHM_Takeover(size_t offset);
int MV_SHM_Giveup(size_t offset);
////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetCacheMemInfo
//!
//! Description: Get cache memory information of shared memory
//!
//! \param pMV_SHM_MemInfo_t 	(IN): -- the pointer to memory information data structure
//!
//! \return Return:		S_OK
//!				        E_INVALIDARG
//!				        E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_GetCacheMemInfo( pMV_SHM_MemInfo_t pInfo );

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetCacheBaseInfo
//!
//! Description: Get base information of cacheable shared memory
//!
//! \param pMV_SHM_BaseInfo_t 	(IN): -- the pointer to base information data structure
//!
//! \return Return:		S_OK
//!				        E_INVALIDARG
//!				        E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_GetCacheBaseInfo( pMV_SHM_BaseInfo_t pInfo );

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetCacheMemInfo
//!
//! Description: Get non-cache memory information of shared memory
//!
//! \param pMV_SHM_MemInfo_t 	(IN): -- the pointer to memory information data structure
//!
//! \return Return:		S_OK
//!				        E_INVALIDARG
//!				        E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_GetNonCacheMemInfo( pMV_SHM_MemInfo_t pInfo );

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetNonCacheBaseInfo
//!
//! Description: Get base information of non-cacheable shared memory
//!
//! \param pMV_SHM_BaseInfo_t 	(IN): -- the pointer to base information data structure
//!
//! \return Return:		S_OK
//!				        E_INVALIDARG
//!				        E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_GetNonCacheBaseInfo( pMV_SHM_BaseInfo_t pInfo );

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_Malloc
//!
//! Description: Allocate contiguous space in shared memory
//!
//! \param Size 	    (IN):   -- the number of bytes allocated.
//! \param Alignment    (IN):   -- the number of bytes aligned.
//! \param pPhysAddr 	(OUT):  -- the pointer to a variable of physical address of the space allocated
//! \param pVirtAddr 	(OUT):  -- the pointer to a variable of virtual address of the space allocated
//!
//! \return Return:		Not ERROR_SHM_MALLOC_FAILED     |      ok, return memory offset of shared memory base address
//!                     ERROR_SHM_MALLOC_FAILED         |      failed.
//!
////////////////////////////////////////////////////////////////////////////////
size_t MV_SHM_Malloc( size_t Size, size_t Alignment);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_Free
//!
//! Description: release contiguous space in shared memory
//!
//! \param Offset 	(IN):  -- memory offset of shared memory base address
//!
//! \return Return:		S_OK
//!				        E_INVALIDARG
//!						E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_Free( size_t Offset);
//int MV_SHM_Free( size_t Offset, char *pFunc, int line);
////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_InvalidateCache
//!
//! Description: Invalidate L1 and L2 Cahce Region data.
//!              The operation need to make sure
//!              that the adress and size and cache line based. Use this one with
//!              cautions.(Typically, it is used on DMA write, CPU read only case)
//!				 All cache lines which are in address rage will be operated. So the addrres
//!				 and size are not requested to be aligned in word or cache line.
//! Allowed by : Cache Memory Address On Galois_Linux
//!
//! \param Offset 	    (IN): -- contain memory offset of shared memory base address
//! \param Size	        (IN): -- contains size of Region in bytes
//!
//! \return Return:		S_OK
//!						E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_InvalidateCache(size_t Offset, size_t Size);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_CleanCache
//!
//! Description: Clean L1 and L2 Cahce Region data
//!				 All cache lines which are in address rage will be operated. So the addrres
//!				 and size are not requested to be aligned in word or cache line.
//! Allowed by : Cache Memory Address On Galois_Linux
//!
//! \param Offset 	    (IN): -- contain memory offset of shared memory base address
//! \param Size	        (IN): -- contains size of Region in bytes
//!
//! \return Return:		S_OK
//!						E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_CleanCache(size_t Offset, size_t Size);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_CleanAndInvalidateCache
//!
//! Description: Clean L1 and L2 Cahce Region data
//!				 All cache lines which are in address rage will be operated. So the addrres
//!				 and size are not requested to be aligned in word or cache line.
//! Allowed by : Cache Memory Address On Galois_Linux
//!
//! \param Offset 	    (IN): -- contain memory offset of shared memory base address
//! \param Size	        (IN): -- contains size of Region in bytes
//!
//! \return Return:		S_OK
//!						E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_CleanAndInvalidateCache(size_t Offset, size_t Size);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetVirtAddr
//!
//! Description: get virtual address of non-cacheable memory from offset
//!
//! \param Offset 	(IN): -- contain memory offset of shared memory base address
//!
//! \return Return:		NULL     |      system not ready or input is error
//!                     Not NULL |      ok
//!
////////////////////////////////////////////////////////////////////////////////
void * MV_SHM_GetNonCacheVirtAddr(size_t Offset);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetVirtAddr
//!
//! Description: get virtual address of cacheable memory from offset
//!
//! \param Offset 	(IN): -- contain memory offset of shared memory base address
//!
//! \return Return:		NULL     |      system not ready or input is error
//!                     Not NULL |      ok
//!
////////////////////////////////////////////////////////////////////////////////
void * MV_SHM_GetCacheVirtAddr(size_t Offset);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetNonCachePhysAddr
//!
//! Description: get physical address of non-cacheable memory from offset
//!
//! \param Offset 	(IN): -- contain memory offset of shared memory base address
//!
//!
//! \return Return:		NULL     |      system not ready or input is error
//!                     Not NULL |      ok
//!
////////////////////////////////////////////////////////////////////////////////
void * MV_SHM_GetNonCachePhysAddr(size_t Offset);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_GetCachePhysAddr
//!
//! Description: get physical address of cacheable memory from offset
//!
//! \param Offset 	(IN): -- contain memory offset of shared memory base address
//!
//!
//! \return Return:		NULL     |      system not ready or input is error
//!                     Not NULL |      ok
//!
////////////////////////////////////////////////////////////////////////////////
void * MV_SHM_GetCachePhysAddr(size_t Offset);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_RevertNonCacheVirtAddr
//!
//! Description: virtual address of non-cacheable memory reverts to offset
//!
//! \param ptr 	(IN): -- virtual address of non-cacheable memory
//!
//! \return Return:		ERROR_SHM_MALLOC_FAILED     |      system not ready or input is error
//!                     Not ERROR_SHM_MALLOC_FAILED |      offset
//!
////////////////////////////////////////////////////////////////////////////////
size_t MV_SHM_RevertNonCacheVirtAddr(void * ptr);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_RevertCacheVirtAddr
//!
//! Description: virtual address of cacheable memory reverts to offset
//!
//! \param ptr 	(IN): -- virtual address of cacheable memory
//!
//! \return Return:		ERROR_SHM_MALLOC_FAILED     |      system not ready or input is error
//!                     Not ERROR_SHM_MALLOC_FAILED |      offset
//!
////////////////////////////////////////////////////////////////////////////////
size_t MV_SHM_RevertCacheVirtAddr(void * ptr);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_RevertNonCachePhysAddr
//!
//! Description: physical address of non-cacheable memory reverts to offset
//!
//! \param ptr 	(IN): -- physical address of non-cacheable memory
//!
//!
//! \return Return:		ERROR_SHM_MALLOC_FAILED     |      system not ready or input is error
//!                     Not ERROR_SHM_MALLOC_FAILED |      offset
//!
////////////////////////////////////////////////////////////////////////////////
size_t MV_SHM_RevertNonCachePhysAddr(void * ptr);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_RevertCachePhysAddr
//!
//! Description: physical address of cacheable memory reverts to offset
//!
//! \param ptr 	(IN): -- physical address of cacheable memory
//!
//!
//! \return Return:		ERROR_SHM_MALLOC_FAILED     |      system not ready or input is error
//!                     Not ERROR_SHM_MALLOC_FAILED |      offset
//!
////////////////////////////////////////////////////////////////////////////////
size_t MV_SHM_RevertCachePhysAddr(void * ptr);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_NONCACHE_Malloc
//!
//! Description: Allocate contiguous space in shared memory
//!
//! \param Size 	    (IN):   -- the number of bytes allocated.
//! \param Alignment    (IN):   -- the number of bytes aligned.
//!
//! \return Return:		Not ERROR_SHM_MALLOC_FAILED     |      ok, return memory offset of shared memory base address
//!                     ERROR_SHM_MALLOC_FAILED         |      failed.
//!
////////////////////////////////////////////////////////////////////////////////
size_t MV_SHM_NONCACHE_Malloc( size_t Size, size_t Alignment);

////////////////////////////////////////////////////////////////////////////////
//! \brief Function:    MV_SHM_NONCACHE_Free
//!
//! Description: release contiguous space in shared memory
//!
//! \param Offset 	(IN):  -- memory offset of shared memory base address
//!
//! \return Return:		S_OK
//!				        E_INVALIDARG
//!						E_FAIL
//!
////////////////////////////////////////////////////////////////////////////////
int MV_SHM_NONCACHE_Free( size_t Offset);

#endif /* __shm_api_h__ */
