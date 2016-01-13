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

#ifndef __memory_engine_h__
#define __memory_engine_h__

#include "shm_type.h"

#define MEMNODE_ALIGN_ADDR(x)    ((x)->m_addr + (x)->m_offset)
#define MEMNODE_ALIGN_SIZE(x)    ((x)->m_size - (x)->m_offset)

int memory_engine_create(memory_engine_t **engine, uint base,
							uint size, uint threshold);

int memory_engine_destroy(memory_engine_t **engine);

int memory_engine_allocate(memory_engine_t *engine, uint size,
							uint alignment, memory_node_t **node);

int memory_engine_free(memory_engine_t *engine, int alignaddr);

int memory_engine_cache(memory_engine_t *engine, uint cmd,
				shm_driver_operation_t op);

static int memory_engine_show_debug(memory_engine_t *engine);

int memory_engine_gothrough(memory_engine_t *engine, char *buffer,
								int count);

int memory_engine_show(memory_engine_t *engine, struct seq_file *file);

/* release shm resource once task was killed*/
int memory_engine_release_by_taskid(memory_engine_t *engine, pid_t taskid);

int memory_engine_release_by_usrtaskid(memory_engine_t *engine, pid_t usrtaskid);

int memory_engine_takeover(memory_engine_t *engine, int alignaddr);

int memory_engine_giveup(memory_engine_t *engine, int alignaddr);

int memory_engine_get_stat(memory_engine_t *engine, struct shm_stat_info *stat);

int memory_engine_dump_stat(memory_engine_t *engine);

int memory_engine_show_stat(memory_engine_t *engine, struct seq_file *file);

#endif /* __memory_engine_h__ */
