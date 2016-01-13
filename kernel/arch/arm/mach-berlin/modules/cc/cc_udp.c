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

#include "cc_udp.h"
#include "cc_error.h"
#include "cc_dss.h"
#include "cc_driver.h"


pMV_CC_UDPPort_t pUDP_DEFAULT_PORT = NULL;

static void MV_CC_UDP_RecvMsg_Handle(struct sk_buff* skb);

HRESULT MV_CC_UDP_Init(void)
{
	if (pUDP_DEFAULT_PORT != NULL)
		return E_NOTREADY;

	pUDP_DEFAULT_PORT = MV_CC_UDP_Open(MV_CC_ServiceID_None);
	if (pUDP_DEFAULT_PORT == NULL) {
		MV_CC_DBG_Error(E_FAIL, "MV_CC_UDP_Init"
				" MV_CC_UDP_Open", NULL);
	}

	return S_OK;
}

HRESULT MV_CC_UDP_Exit(void)
{
	HRESULT res;

	if (pUDP_DEFAULT_PORT == NULL)
		return E_NOTREADY;

	res = MV_CC_UDP_Close(pUDP_DEFAULT_PORT);
	if (res != S_OK) {
		MV_CC_DBG_Error(res, "MV_CC_UDP_Exit"
			" MV_CC_UDP_Close", NULL);
	}

	pUDP_DEFAULT_PORT = NULL;

	return S_OK;
}

pMV_CC_UDPPort_t MV_CC_UDP_Open(MV_CC_ServiceID_U32_t ServiceID)
{
	pMV_CC_UDPPort_t pPort;

	struct netlink_kernel_cfg cfg = {
		.input	= MV_CC_UDP_RecvMsg_Handle,
		.groups	= 0,
		.flags = NL_CFG_F_NONROOT_RECV | NL_CFG_F_NONROOT_SEND,
	};

	pPort = MV_OSAL_Malloc(sizeof(MV_CC_UDPPort_t));
	if (pPort == NULL) {
		MV_CC_DBG_Warning(E_FAIL, "MV_CC_UDP_Open"
				" MV_OSAL_Malloc", NULL);
		return NULL;
	}

	pPort->m_Socket = netlink_kernel_create(&init_net,
			NETLINK_GALOIS_CC_SINGLECPU, &cfg);
	if (pPort->m_Socket == NULL) {
		MV_CC_DBG_Warning(E_FAIL, "MV_CC_UDP_Open"
				" socket failed", NULL);
		goto error_exit_1;
	}

	MV_CC_DBG_Info("MV_CC_UDP_Open Socket = 0x%08X, SID = 0x%08X\n",
			(unsigned int)pPort->m_Socket, 0);

	return pPort;

error_exit_1:

	MV_OSAL_Free(pPort);

	return NULL;
}

HRESULT MV_CC_UDP_Close(pMV_CC_UDPPort_t pPort)
{
	/* Parameter Check */
	if (pPort == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_UDP_Close", NULL);

	MV_CC_DBG_Info("MV_CC_UDP_Close Socket = 0x%08X\n",
				(unsigned int)pPort->m_Socket);

	sock_release(pPort->m_Socket->sk_socket);

	MV_OSAL_Free(pPort);

	return S_OK;
}

HRESULT MV_CC_UDP_SendMsg(pMV_CC_UDPPort_t	pPort,
				MV_CC_ServiceID_U32_t DstServiceID,
				UCHAR	*pMsgBuf,
				UINT 	MsgLen )
{
	struct sk_buff * skb;
	struct nlmsghdr *nlhdr;

	/* Parameter Check */
	if ((pMsgBuf == NULL) || (MsgLen > MV_CC_ICCFIFO_FRAME_SIZE))
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_UDP_SendMsg", NULL);

	if (pPort == NULL)
		pPort = pUDP_DEFAULT_PORT;

	skb = nlmsg_new(MsgLen, GFP_ATOMIC);
	if (skb == NULL) {
		MV_CC_DBG_Error(E_OUTOFMEMORY, "MV_CC_UDP_SendMsg"
					" nlmsg_new", NULL);
		return E_OUTOFMEMORY;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = NETLINK_GALOIS_CC_GROUP_SINGLECPU;

	if (unlikely(skb_tailroom(skb) < (int)NLMSG_SPACE(MsgLen)))
		goto nlmsg_failure;
	nlhdr = __nlmsg_put(skb, 0, 0, NLMSG_DONE, MsgLen, 0);
	GaloisMemcpy(NLMSG_DATA(nlhdr), pMsgBuf, MsgLen);

	netlink_unicast(pPort->m_Socket, skb, DstServiceID, MSG_DONTWAIT);

	MV_CC_DBG_Info("MV_CC_UDP_SendMsg (SID = 0x%08X, size = %d )\n",
				DstServiceID, MsgLen);

	return S_OK;

nlmsg_failure:

	MV_CC_DBG_Info("MV_CC_UDP_SendMsg (SID = 0x%08X, size = %d )"
			" nlmsg_failure !!!\n", DstServiceID, MsgLen);
	kfree_skb(skb);

	return E_FAIL;
}

static void MV_CC_UDP_RecvMsg_Handle(struct sk_buff* skb)
{
	//wake_up_interruptible(sk->sk_sleep);
	return;
}

