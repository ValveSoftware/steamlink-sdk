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

#include "cc_cbuf.h"
#include "cc_error.h"


HRESULT  MV_CC_CBufBody_Create(UINT32	*pSHMOffset,
			   	   UINT32	BufSize,
			   	   UINT32	EndBlockSize,
			   	   UINT32	Flags)
{
	pMV_CC_CBufBody_t pCBufBody;
	UINT32 CBuf_FullSize, SHMOffset;
	HRESULT res;

#ifdef ENABLE_DEBUG_OSAL
	//Fill the end padding
	UINT32 buflen = BufSize;
#endif

	/* Parameter Check */
	if (pSHMOffset == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_CBufBody_Create", NULL);

	CBuf_FullSize = sizeof(MV_CC_CBufBody_t) + BufSize;

	/* Check Flag->MV_CC_CBUF_FLAGS_ALIGNMENT */
	if ((MV_CC_FlagGet(Flags, MV_CC_CBUF_FLAGS_ALIGNMENT)) \
				== MV_CC_CBUF_FLAGS_ALIGNMENT_Yes)
		CBuf_FullSize += MV_CC_HAL_MEMBOUNDARY;

	/* If EndBlockSize == 0, no End Block for the MV_CC_CBufBody_t
		and no limitation to the Block Operation */
	if (EndBlockSize != 0)
		CBuf_FullSize += EndBlockSize;
	//Cbuf use cacheable memory in kernel driver, then use related virt addr
	SHMOffset = MV_SHM_Malloc(CBuf_FullSize, MV_CC_HAL_MEMBOUNDARY);
	if (SHMOffset == ERROR_SHM_MALLOC_FAILED) {
		*pSHMOffset = ERROR_SHM_MALLOC_FAILED;
		res = E_OUTOFMEMORY;
		MV_CC_DBG_Error(res, "MV_CC_CBufBody_Create MV_SHM_Malloc", NULL);
		return res;
	}

	pCBufBody = (pMV_CC_CBufBody_t)MV_SHM_GetCacheVirtAddr(SHMOffset);
	if (pCBufBody == NULL) {
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_CBufBody_Create", NULL);
	}
	GaloisMemClear((void *)pCBufBody, CBuf_FullSize);

	pCBufBody->m_StartOffset = sizeof(MV_CC_CBufBody_t);

	if ((MV_CC_FlagGet(Flags, MV_CC_CBUF_FLAGS_ALIGNMENT))
		== MV_CC_CBUF_FLAGS_ALIGNMENT_Yes) {
		pCBufBody->m_StartOffset += MV_CC_HAL_MEMBOUNDARY -
		(((UINT32)pCBufBody + pCBufBody->m_StartOffset)
		% MV_CC_HAL_MEMBOUNDARY );
	}

	//Fill the padding with 0xFF for debug easily
#ifdef ENABLE_DEBUG_OSAL

	//Fill the front padding
	GaloisMemSet((void *)((UCHAR *)pCBufBody +
		sizeof(MV_CC_CBufBody_t)), 0xFF,
		(pCBufBody->m_StartOffset -
		sizeof(MV_CC_CBufBody_t)) );

	if ( EndBlockSize != 0 )
		buflen += EndBlockSize;

	GaloisMemSet((void *)((UCHAR *)pCBufBody +
		pCBufBody->m_StartOffset + buflen),
		0xFF, CBuf_FullSize - pCBufBody->m_StartOffset
		- buflen);
#endif

	pCBufBody->m_BufSize = BufSize;
	pCBufBody->m_EndBlockSize = EndBlockSize;
	pCBufBody->m_Flags = Flags;

	*pSHMOffset = SHMOffset;

	MV_CC_DBG_Info("DSS MV_CC_CBufBody_Create"
		" SHM Offset = [0x%08X]",SHMOffset);

	return S_OK;
}

HRESULT  MV_CC_CBufBody_Destroy(UINT32	SHMOffset)
{
	HRESULT res;

	MV_CC_DBG_Info("MV_CC_CBufBody_Destroy"
		" SHM Offset = [0x%08X]", SHMOffset);

	/* Parameter Check */
	if (SHMOffset == ERROR_SHM_MALLOC_FAILED) {
		MV_CC_DBG_Error(E_INVALIDARG,
			"MV_CC_CBufBody_Destroy", NULL);
	}

	res = MV_SHM_Free( SHMOffset );
	if (res != S_OK) {
		MV_CC_DBG_Error(res, "MV_CC_CBufBody_Destroy"
					" MV_SHM_Free", NULL);
	}

	return res;
}
