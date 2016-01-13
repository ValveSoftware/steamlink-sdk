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

#ifndef _CC_STD_MAP_H_
#define _CC_STD_MAP_H_

#include "cc_type.h"
#include "cc_error.h"



/*
 * Status
 *
 * Feature      Implemented  Functioning  Reviewed  Tested
 *
 * ctor/dtor    Y            Y
 * clear        Y            Y
 * find         Y            Y
 * insert       Y            Y
 * erase        Y            Y
 * size
 * iter         Y            Y
 *
 * Implemented = code is written
 * Functioning = pass a basic test
 * Reviewed    = carefully examined. should not exist bugs
 * Tested      = passed UT
 */

typedef enum
{
	STD_MAP_NODE_VALUE,
	STD_MAP_NODE_POINTER
} std_map_node_type;

typedef struct _std_map_node
{
	PVOID pData;
	PVOID pKey;
	UINT bRed;
	struct _std_map_node* pLeft;
	struct _std_map_node* pRight;
	struct _std_map_node* pParent;
} std_map_node;

/* Compare(a,b) should return 1 if *a > *b, -1 if *a < *b, and 0 otherwise */
typedef INT (*std_map_compare_func)(PVOID a, PVOID b);
typedef VOID (*std_map_dtor_func)(PVOID obj);

typedef struct _std_map
{
	std_map_compare_func compare;
	std_map_dtor_func key_dtor;
	std_map_dtor_func data_dtor;
	std_map_node* m_pRoot;
	std_map_node* m_pNil;
	std_map_node_type m_eType;
	UINT uiSize;
} std_map;

std_map* std_map_ctor(std_map* self,
		      std_map_compare_func compare,
		      std_map_dtor_func key_dtor,
		      std_map_dtor_func data_dtor,
		      std_map_node_type eType);
VOID std_map_dtor(std_map* self);
HRESULT std_map_clear(std_map* self);
HRESULT std_map_find(std_map* self, PVOID pKey, PVOID* ppData);
HRESULT std_map_insert(std_map* self, PVOID pKey, PVOID pData);
HRESULT std_map_erase(std_map* self, PVOID pKey);
HRESULT std_map_size(std_map* self, UINT* puiSize);
HRESULT std_map_iter(std_map* self, std_map_node** ppNode);

#endif

