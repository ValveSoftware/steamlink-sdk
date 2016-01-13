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

#ifndef __shm_device_h__
#define __shm_device_h__

#include "shm_type.h"

int shm_device_create(shm_device_t **device, uint base, uint size, uint threshold);

int shm_device_destroy(shm_device_t **device);

int shm_device_get_meminfo(shm_device_t *device, pMV_SHM_MemInfo_t pInfo);

int shm_device_get_baseinfo(shm_device_t *device, pMV_SHM_BaseInfo_t pInfo);

uint shm_device_allocate(shm_device_t *device, size_t size, size_t align);

int shm_device_free(shm_device_t *device, size_t offset);

int shm_device_cache_invalidate(void * virtaddr_cache, size_t size);

int shm_device_cache_clean(void * virtaddr_cache, size_t size);

int shm_device_cache_clean_and_invalidate(void * virtaddr_cache, size_t size);

int shm_device_cache(shm_device_t *device, uint cmd,
				shm_driver_operation_t op);

int shm_device_get_detail(shm_device_t *device, char *buffer, int count);

int shm_device_show_detail(shm_device_t *device, struct seq_file *file);

int shm_device_release_by_taskid(shm_device_t *device, pid_t taskid);

int shm_device_release_by_usrtaskid(shm_device_t *device, pid_t usrtaskid);

int shm_device_takeover(shm_device_t *device, size_t offset);

int shm_device_giveup(shm_device_t *device, size_t offset);

int shm_device_get_stat(shm_device_t *device, struct shm_stat_info *stat);

int shm_device_dump_stat(shm_device_t *device);

int shm_device_show_stat(shm_device_t *devce, struct seq_file  *file);
#endif /* __shm_device_h__ */
