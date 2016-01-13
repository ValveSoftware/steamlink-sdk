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

#ifndef __CC_TYPE_H__
#define __CC_TYPE_H__

#include <linux/slab.h>   /* kmalloc() */


#ifndef TRUE
#define TRUE			(1)
#endif
#ifndef FALSE
#define FALSE			(0)
#endif

#ifndef True
#define True			(1)
#endif
#ifndef False
#define False			(0)
#endif

#ifndef NULL
#define NULL			((void *)0)
#endif

#define MV_ASSERT( a )		BUG_ON(!(a))


typedef unsigned char		UCHAR;
typedef char			CHAR;
typedef UCHAR			BOOL;

typedef UCHAR			BOOLEAN;
typedef short			SHORT;
typedef unsigned short		USHORT;
typedef int			INT;
typedef unsigned int		UINT;
typedef long			LONG;
typedef unsigned long		ULONG;
typedef long long		LONGLONG;
typedef unsigned long long	ULONGLONG;
typedef void			VOID;
typedef void*			PTR;
typedef void**			PHANDLE;
typedef void*			HANDLE;
typedef void*			PVOID;

typedef UCHAR			BYTE;

typedef CHAR			INT8;
typedef UCHAR			UINT8;
typedef short			INT16;
typedef unsigned short		UINT16;
typedef int			INT32;
typedef unsigned int		UINT32;
typedef long long           	INT64;
typedef unsigned long long 	UINT64;
typedef unsigned int		SIZE_T;
typedef signed			intHRESULT;
typedef signed int		HRESULT;


typedef UINT8 			MV_OSAL_CPUID_U8_t;
typedef UINT8 			MV_OSAL_ProcessID_U8_t;
typedef UINT16 			MV_OSAL_PortID_U16_t;
typedef UINT32 			MV_OSAL_ServiceID_U32_t;
typedef UINT32 			MV_CC_ServiceID_U32_t;

typedef void 			*MV_CC_HANDLE;
typedef MV_CC_HANDLE 		MV_CC_HANDLE_MsgQ_t;


typedef struct _MV_CC_ICCAddr {
	MV_OSAL_CPUID_U8_t		m_CPU;
	MV_OSAL_ProcessID_U8_t		m_Process;
	MV_OSAL_PortID_U16_t 		m_Port;
} MV_CC_ICCAddr_t, *pMV_CC_ICCAddr_t;


typedef struct _MV_CC_DSS_ServiceInfo {
	// public info
	MV_CC_ServiceID_U32_t 		m_ServiceID;
	UINT8 				m_SrvLevel;
	UINT8 				m_SrvType;
	UINT8				m_Pad1;
	UINT8				m_Pad2;
	// private info
	UINT8				m_Data[24];
} MV_CC_DSS_ServiceInfo_t, *pMV_CC_DSS_ServiceInfo_t;


typedef struct _MV_CC_DSS_ServiceInfo_MsgQ {
	// public info
	MV_CC_ServiceID_U32_t 		m_ServiceID;
	UINT8 				m_SrvLevel;
	UINT8 				m_SrvType;
	UINT8				m_Pad1;
	UINT8				m_Pad2;
	// private info
	MV_CC_ICCAddr_t 		m_SrvAddr;
	UINT32 		 		m_NMsgsMax;
	UINT8				m_Blank[16];
} MV_CC_DSS_ServiceInfo_MsgQ_t, *pMV_CC_DSS_ServiceInfo_MsgQ_t;


typedef struct _MV_CC_DSS_ServiceInfo_MsgQEx {
	// public info
	MV_CC_ServiceID_U32_t 		m_ServiceID;
	UINT8 				m_SrvLevel;
	UINT8 				m_SrvType;
	UINT8				m_Pad1;
	UINT8				m_Pad2;
	// private info
	MV_CC_ICCAddr_t 		m_SrvAddr;
	UINT32 		 		m_NMsgsMax;
	UINT32				m_MsgLenMax;
	CHAR				m_Blank[12];
} MV_CC_DSS_ServiceInfo_MsgQEx_t, *pMV_CC_DSS_ServiceInfo_MsgQEx_t;


typedef struct _MV_CC_DSS_ServiceInfo_RPC {
	// public info
	MV_CC_ServiceID_U32_t 		m_ServiceID;
	UINT8 				m_SrvLevel;
	UINT8 				m_SrvType;
	UINT8				m_Pad1;
	UINT8				m_Pad2;
	// private info
	MV_CC_ICCAddr_t 		m_SrvAddr;
	UINT32 				m_RegCmdListNum;
	CHAR				m_Blank[16];
} MV_CC_DSS_ServiceInfo_RPC_t, *pMV_CC_DSS_ServiceInfo_RPC_t;


typedef struct _MV_CC_DSS_ServiceInfo_CBuf {
	// public info
	MV_CC_ServiceID_U32_t 		m_ServiceID;
	UINT8 				m_SrvLevel;
	UINT8 				m_SrvType;
	UINT8				m_Pad1;
	UINT8				m_Pad2;
	// private info
	UINT32                  	m_seqid;
	UINT32           		m_CBufBody_SHMOffset;
	UINT32 				m_BufSize;
	UINT32 				m_EndBlockSize;
	UINT32 				m_Flags;
	MV_CC_ServiceID_U32_t 		m_NoticeMsgQSID;
} MV_CC_DSS_ServiceInfo_CBuf_t, *pMV_CC_DSS_ServiceInfo_CBuf_t;


typedef struct _MV_CC_DSS_ServiceInfo_DataList {
	UINT32				m_ListNum;
	UINT32				m_DataNum;
	UINT32				m_MaxNum;
	MV_CC_DSS_ServiceInfo_t		m_SrvInfo[];
} MV_CC_DSS_ServiceInfo_DataList_t, *pMV_CC_DSS_ServiceInfo_DataList_t;


typedef enum _MV_CC_CBufCtrlType {
	//MV_CC_CBufCtrlType_None		= 0,	/* Cbuf NULL Object */
	MV_CC_CBufCtrlType_Consumer	= 1,	/* Cbuf Consumer(Server) Object */
	MV_CC_CBufCtrlType_Producer	= 2,	/* Cbuf Producer(client) Object */
} MV_CC_CBufCtrlType_t;


typedef void (*t_buffer_notifier_cb)(HANDLE hBuffer, INT fullness, UINT tag);
#define MV_GENERIC_BUF_TYPE_XBV         1


typedef struct t_buffer_notifier {
	//! consumer use here
	INT     			m_Lwm;
	t_buffer_notifier_cb    	m_LwmCallback;
	UINT    			m_ConsumerInfo;
	UINT    			m_LwmCBLast;

	//! producer register here
	INT     			m_Hwm;
	t_buffer_notifier_cb    	m_HwmCallback;
	UINT    			m_ProducerInfo;
	UINT    			m_HwmCBLast;
}Buffer_Notifier_t;


typedef enum _MV_CC_SrvLevel_U8 {
	MV_CC_SrvLevel_ITC 		= 0,	/* Inter-Thread Communication */
	MV_CC_SrvLevel_IPC 		= 1,	/* Inter-Process Communication */
	MV_CC_SrvLevel_ICC 		= 2,	/* Inter-CPU Communication */
	MV_CC_NoCCSrvLevel		= 0xFF	/*  No CC Service */
} MV_CC_SrvLevel_U8_t;


typedef enum _MV_CC_SrvType_U8 {
	MV_CC_SrvType_MsgQ 		= 0,	/* Message Queue Service */
	MV_CC_SrvType_RPC 		= 1,	/* RPC Service */
	MV_CC_SrvType_CBuf 		= 2,	/* Circular Buffer Service */
	MV_CC_SrvType_MsgQEx		= 3,	/* Message Queue Extension Service */
	MV_CC_SrvType_RPCClnt		= 4,	/* RPC Client */
	MV_CC_SrvType_Max		= 5,
	MV_CC_NoCCSrvType		= 0xFF	/*  No CC Service */
} MV_CC_SrvType_U8_t;


typedef struct {
	//! Message ID
	UINT32 m_MsgID;
	//! Message 1st Parameter
	UINT32 m_Param1;
	//! Message 2nd Parameter
	UINT32 m_Param2;
} MV_CC_MSG_t, *pMV_CC_MSG_t;


#define MV_OSAL_Malloc(x)			kmalloc((x), GFP_KERNEL)
#define MV_OSAL_Free(p)				do { \
							if ((p) != NULL) { \
								kfree(p); \
								(p) = NULL; \
							} \
						} while(0);


#define MV_CC_FlagSet(x,a,n,m)			((a)?((x)|=(n)):((x)&=(m)))
#define MV_CC_FlagGet(x,n)			(((x)&(n))?(1):(0))
#define MV_CC_FlagCopyXtoY(x,y,n,m)		(((x)&(n))?((y)|=(n)):((y)&=(m)))

#define MV_CC_BitSet(x,n)			((x)|=(n))
#define MV_CC_BitClean(x,n)			((x)&=(~n))
#define MV_CC_BitGet(x,n)			(((x)&(n))?(1):(0))


#define	MV_CC_DBG_Warning(x, s, ...)
#define	MV_CC_DBG_Info(...)
#define	MV_CC_DBG_Error(x, s, ...)		{return x;}



#define MV_CC_CBUF_FLAGS_ALIGNMENT		(UINT32)(0x00000001)
#define MV_CC_CBUF_FLAGS_ALIGNMENT_MASK		(UINT32)(0xFFFFFFFE)
#define MV_CC_CBUF_FLAGS_ALIGNMENT_No		(UINT32)(0)
#define MV_CC_CBUF_FLAGS_ALIGNMENT_Yes		(UINT32)(1)

#define MV_CC_HAL_MEMBOUNDARY			(64)
#define ERROR_SHM_MALLOC_FAILED             	(0xFFFFFFFF)



#define GaloisMemmove				memmove
#define GaloisMemcpy				memcpy
#define GaloisMemClear(buf, n)			memset((buf), 0, (n))
#define GaloisMemSet(buf, c, n)			memset((buf), (c), (n))
#define GaloisMemComp(buf1, buf2, n)		memcmp((buf1), (buf2), (n))



#define MV_CC_CCUDP_HEAD_BYTEOFFSET		(0)
#define MV_CC_CCUDP_HEAD_BYTESIZE		(0)
#define MV_CC_ICCFIFO_FRAME_SIZE		(128)


#define MV_CC_CCUDP_DATA_BYTEOFFSET		(MV_CC_CCUDP_HEAD_BYTEOFFSET + \
							MV_CC_CCUDP_HEAD_BYTESIZE)
#define MV_CC_RPC_HEAD_BYTEOFFSET		(MV_CC_CCUDP_DATA_BYTEOFFSET)
#define MV_CC_RPC_HEAD_BYTESIZE			(8)

#define MV_CC_MSGQ_HEAD_BYTEOFFSET		(MV_CC_CCUDP_DATA_BYTEOFFSET)
#define MV_CC_MSGQ_HEAD_BYTESIZE		(2)
#define MV_CC_MSGQ_DATA_BYTEOFFSET		(MV_CC_MSGQ_HEAD_BYTEOFFSET + \
							MV_CC_MSGQ_HEAD_BYTESIZE)
#define MV_CC_MSGQ_DATA_BYTESIZE_MAX		(MV_CC_ICCFIFO_FRAME_SIZE - \
							MV_CC_MSGQ_DATA_BYTEOFFSET)
#define MV_CC_MSGQ_DATA_MSGEXLEN_MAX		(MV_CC_MSGQ_DATA_BYTESIZE_MAX - 1)


typedef struct {
	UINT8 m_MsgLen;
	UINT8 m_Data[MV_CC_MSGQ_DATA_MSGEXLEN_MAX];
} MV_CC_MSGEx_t, *pMV_CC_MSGEx_t;



#endif
