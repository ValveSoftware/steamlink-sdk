 /*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef ortp_events_h
#define ortp_events_h

#include <ortp/str_utils.h>
#include <ortp/rtcp.h>

typedef mblk_t OrtpEvent;

typedef unsigned long OrtpEventType;

typedef enum {
	OrtpRTPSocket,
	OrtpRTCPSocket
} OrtpSocketType;

struct _OrtpEventData{
	mblk_t *packet;	/* most events are associated to a received packet */
	struct sockaddr_storage source_addr;
	socklen_t source_addrlen;
	ortpTimeSpec ts;
	union {
		int telephone_event;
		int payload_type;
		bool_t dtls_stream_encrypted;
		bool_t zrtp_stream_encrypted;
		bool_t ice_processing_successful;
		struct _ZrtpSas{
			char sas[32]; // up to 31 + null characters
			bool_t verified;
			bool_t pad[3];
		} zrtp_sas;
		OrtpSocketType socket_type;
		uint64_t tmmbr_mxtbr;
		uint32_t received_rtt_character;
	} info;
};

typedef struct _OrtpEventData OrtpEventData;



#ifdef __cplusplus
extern "C"{
#endif

ORTP_PUBLIC OrtpEvent * ortp_event_new(OrtpEventType tp);
ORTP_PUBLIC OrtpEventType ortp_event_get_type(const OrtpEvent *ev);
/* type is one of the following*/
#define ORTP_EVENT_STUN_PACKET_RECEIVED		1
#define ORTP_EVENT_PAYLOAD_TYPE_CHANGED 	2
#define ORTP_EVENT_TELEPHONE_EVENT		3
#define ORTP_EVENT_RTCP_PACKET_RECEIVED		4 /**<when a RTCP packet is received from far end */
#define ORTP_EVENT_RTCP_PACKET_EMITTED		5 /**<fired when oRTP decides to send an automatic RTCP SR or RR */
#define ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED	6
#define ORTP_EVENT_ZRTP_SAS_READY		7
#define ORTP_EVENT_ICE_CHECK_LIST_PROCESSING_FINISHED	8
#define ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED	9
#define ORTP_EVENT_ICE_GATHERING_FINISHED		10
#define ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED		11
#define ORTP_EVENT_ICE_RESTART_NEEDED			12
#define ORTP_EVENT_DTLS_ENCRYPTION_CHANGED		13
#define ORTP_EVENT_TMMBR_RECEIVED		14

ORTP_PUBLIC OrtpEventData * ortp_event_get_data(OrtpEvent *ev);
ORTP_PUBLIC void ortp_event_destroy(OrtpEvent *ev);
ORTP_PUBLIC OrtpEvent *ortp_event_dup(OrtpEvent *ev);

typedef struct OrtpEvQueue{
	queue_t q;
	ortp_mutex_t mutex;
} OrtpEvQueue;

ORTP_PUBLIC OrtpEvQueue * ortp_ev_queue_new(void);
ORTP_PUBLIC void ortp_ev_queue_destroy(OrtpEvQueue *q);
ORTP_PUBLIC OrtpEvent * ortp_ev_queue_get(OrtpEvQueue *q);
ORTP_PUBLIC void ortp_ev_queue_flush(OrtpEvQueue * qp);

struct _RtpSession;

/**
 * Callback function when a RTCP packet of the interested type is found.
 *
 * @param evd the packet. Read-only, must NOT be changed.
 * @param user_data user data provided when registered the callback
 *
 */
typedef void (*OrtpEvDispatcherCb)(const OrtpEventData *evd, void *user_data);
typedef struct OrtpEvDispatcherData{
	OrtpEventType type;
	rtcp_type_t subtype;
	OrtpEvDispatcherCb on_found;
	void* user_data;
} OrtpEvDispatcherData;

typedef struct OrtpEvDispatcher{
	OrtpEvQueue *q;
	struct _RtpSession* session;
	OList *cbs;
} OrtpEvDispatcher;

/**
 * Constructs an OrtpEvDispatcher object. This object can be used to be notified
 * when any RTCP type packet is received or emitted on the rtp session,
 * given a callback registered with \a ortp_ev_dispatcher_connect
 *
 * @param session RTP session to listen on. Cannot be NULL.
 *
 * @return OrtpEvDispatcher object newly created.
 */
ORTP_PUBLIC OrtpEvDispatcher * ortp_ev_dispatcher_new(struct _RtpSession* session);
/**
 * Frees the memory for the given dispatcher. Note that user_data must be freed
 * by caller, and so does the OrtpEvQueue.
 *
 * @param d OrtpEvDispatcher object
 */
ORTP_PUBLIC void ortp_ev_dispatcher_destroy(OrtpEvDispatcher *d);
/**
 * Iterate method to be called periodically. If a RTCP packet is found and
 * its type matches one of the callback connected with \a ortp_ev_dispatcher_connect,
 * this callback will be invoked in the current thread.
 *
 * @param d OrtpEvDispatcher object
 */
ORTP_PUBLIC void ortp_ev_dispatcher_iterate(OrtpEvDispatcher *d);
/**
 * Connects a callback to the given type of event packet and, in case of RTCP event,
 * for the given RTCP subtype.
 * When any event is found, the callback is invoked. Multiple
 * callbacks can be connected to the same type of event, and the same callback
 * can be connected to multiple type of event.
 *
 * @param d OrtpEvDispatcher object
 * @param type type of event to be notified of.
 * @param subtype when type is set to ORTP_EVENT_RTCP_PACKET_RECEIVED or ORTP_EVENT_RTCP_PACKET_EMITTED, subtype of RTCP packet to be notified of. Otherwise this parameter is not used.
 * @param on_receive function to call when a RTCP packet of the given type is found.
 * @param user_data user data given as last argument of the callback. Can be NULL. MUST be freed by user.
 */
ORTP_PUBLIC void ortp_ev_dispatcher_connect(OrtpEvDispatcher *d
											, OrtpEventType type
											, rtcp_type_t subtype
											, OrtpEvDispatcherCb on_receive
											, void *user_data);

/**
 * Disconnects the given callback for the given type and subtype on the given dispatcher.
*/
void ortp_ev_dispatcher_disconnect(OrtpEvDispatcher *d
								, OrtpEventType type
								, rtcp_type_t subtype
								, OrtpEvDispatcherCb cb);

#ifdef __cplusplus
}
#endif

#endif

