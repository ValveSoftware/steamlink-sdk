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

#ifndef rtpsession_priv_h
#define rtpsession_priv_h

#include "ortp/rtpsession.h"

#define IP_UDP_OVERHEAD (20 + 8)
#define IP6_UDP_OVERHEAD (40 + 8)

#define RTCP_XR_GMIN 16 /* Recommended value of Gmin from RFC3611, section 4.7.6 */

typedef enum {
	RTP_SESSION_RECV_SYNC=1,	/* the rtp session is synchronising in the incoming stream */
	RTP_SESSION_FIRST_PACKET_DELIVERED=1<<1,
	RTP_SESSION_SCHEDULED=1<<2,/* the scheduler controls this session*/
	RTP_SESSION_BLOCKING_MODE=1<<3, /* in blocking mode */
	RTP_SESSION_RECV_NOT_STARTED=1<<4,	/* the application has not started to try to recv */
	RTP_SESSION_SEND_NOT_STARTED=1<<5,  /* the application has not started to send something */
	RTP_SESSION_IN_SCHEDULER=1<<6,	/* the rtp session is in the scheduler list */
	RTP_SESSION_USING_EXT_SOCKETS=1<<7, /* the session is using externaly supplied sockets */
	RTP_SOCKET_CONNECTED=1<<8,
	RTCP_SOCKET_CONNECTED=1<<9,
	RTP_SESSION_USING_TRANSPORT=1<<10,
	RTCP_OVERRIDE_LOST_PACKETS=1<<11,
	RTCP_OVERRIDE_JITTER=1<<12,
	RTCP_OVERRIDE_DELAY=1<<13,
	RTP_SESSION_RECV_SEQ_INIT=1<<14,
	RTP_SESSION_FLUSH=1<<15,
	RTP_SESSION_SOCKET_REFRESH_REQUESTED=1<<16
}RtpSessionFlags;

#define rtp_session_using_transport(s, stream) (((s)->flags & RTP_SESSION_USING_TRANSPORT) && (s->stream.gs.tr != 0))

int rtp_session_rtp_recv_abstract(ortp_socket_t socket, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen);

void rtp_session_update_payload_type(RtpSession * session, int pt);
int rtp_putq(queue_t *q, mblk_t *mp);
mblk_t * rtp_getq(queue_t *q, uint32_t ts, int *rejected);
int rtp_session_rtp_recv(RtpSession * session, uint32_t ts);
int rtp_session_rtcp_recv(RtpSession * session);
int rtp_session_rtp_send (RtpSession * session, mblk_t * m);

#define rtp_session_rtcp_send rtp_session_rtcp_sendm_raw

void rtp_session_rtp_parse(RtpSession *session, mblk_t *mp, uint32_t local_str_ts, struct sockaddr *addr, socklen_t addrlen);

void rtp_session_run_rtcp_send_scheduler(RtpSession *session);
void update_avg_rtcp_size(RtpSession *session, int bytes);

mblk_t * rtp_session_network_simulate(RtpSession *session, mblk_t *input, bool_t *is_rtp_packet);
void ortp_network_simulator_destroy(OrtpNetworkSimulatorCtx *sim);

void rtcp_common_header_init(rtcp_common_header_t *ch, RtpSession *s,int type, int rc, size_t bytes_len);

mblk_t * make_xr_rcvr_rtt(RtpSession *session);
mblk_t * make_xr_dlrr(RtpSession *session);
mblk_t * make_xr_stat_summary(RtpSession *session);
mblk_t * make_xr_voip_metrics(RtpSession *session);

bool_t rtcp_is_RTPFB_internal(const mblk_t *m);
bool_t rtcp_is_PSFB_internal(const mblk_t *m);
bool_t rtp_session_has_fb_packets_to_send(RtpSession *session);
void rtp_session_send_regular_rtcp_packet_and_reschedule(RtpSession *session, uint64_t tc);
void rtp_session_send_fb_rtcp_packet_and_reschedule(RtpSession *session);

void ortp_stream_clear_aux_addresses(OrtpStream *os);
/*
 * no more public, use modifier instead
 * */
void rtp_session_set_transports(RtpSession *session, RtpTransport *rtptr, RtpTransport *rtcptr);

bool_t rtp_profile_is_telephone_event(const RtpProfile *prof, int pt);

ortp_socket_t rtp_session_get_socket(RtpSession *session, bool_t is_rtp);

void rtp_session_do_splice(RtpSession *session, mblk_t *packet, bool_t is_rtp);

/*
 * Update remote addr in the following case:
 * rtp symetric == TRUE && socket not connected && remote addr has changed && ((rtp/rtcp packet && not only at start) or (no rtp/rtcp packets received))
 * @param[in] session  on which to perform change
 * @param[in] mp packet where remote addr is retreived
 * @param[in] is_rtp true if rtp
 * @param[in] only_at_start only perform changes if no valid packets received yet
 * @return 0 if chaged was performed
 *
 */
	
int rtp_session_update_remote_sock_addr(RtpSession * session, mblk_t * mp, bool_t is_rtp,bool_t only_at_start);

void rtp_session_process_incoming(RtpSession * session, mblk_t *mp, bool_t is_rtp_packet, uint32_t ts, bool_t received_via_rtcp_mux);
void update_sent_bytes(OrtpStream *os, int nbytes);

void _rtp_session_apply_socket_sizes(RtpSession *session);


#endif
