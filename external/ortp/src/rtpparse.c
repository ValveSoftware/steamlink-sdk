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


#ifdef HAVE_CONFIG_H
#include "ortp-config.h"
#endif

#include "ortp/ortp.h"
#include "jitterctl.h"
#include "utils.h"
#include "rtpsession_priv.h"

static bool_t queue_packet(queue_t *q, int maxrqsz, mblk_t *mp, rtp_header_t *rtp, int *discarded, int *duplicate)
{
	mblk_t *tmp;
	int header_size;
	*discarded=0;
	*duplicate=0;
	header_size=RTP_FIXED_HEADER_SIZE+ (4*rtp->cc);
	if ((mp->b_wptr - mp->b_rptr)==header_size){
		ortp_debug("Rtp packet contains no data.");
		(*discarded)++;
		freemsg(mp);
		return FALSE;
	}

	/* and then add the packet to the queue */
	if (rtp_putq(q,mp) < 0) {
		/* It was a duplicate packet */
		(*duplicate)++;
	}

	/* make some checks: q size must not exceed RtpStream::max_rq_size */
	while (q->q_mcount > maxrqsz)
	{
		/* remove the oldest mblk_t */
		tmp=getq(q);
		if (mp!=NULL)
		{
			ortp_warning("rtp_putq: Queue is full. Discarding message with ts=%u",((rtp_header_t*)mp->b_rptr)->timestamp);
			freemsg(tmp);
			(*discarded)++;
		}
	}
	return TRUE;
}

static void compute_mean_and_deviation(uint32_t nb, double x, double *olds, double *oldm, double *news, double *newm) {
	*newm = *oldm + (x - *oldm) / nb;
	*news = *olds + ((x - *oldm) * (x - *newm));
	*oldm = *newm;
	*olds = *news;
}

static void update_rtcp_xr_stat_summary(RtpSession *session, mblk_t *mp, uint32_t local_str_ts) {
	rtp_header_t *rtp = (rtp_header_t *)mp->b_rptr;
	int64_t diff = (int64_t)rtp->timestamp - (int64_t)local_str_ts;

	/* TTL/HL statistics */
	if (session->rtcp_xr_stats.rcv_since_last_stat_summary == 1) {
		session->rtcp_xr_stats.min_ttl_or_hl_since_last_stat_summary = 255;
		session->rtcp_xr_stats.max_ttl_or_hl_since_last_stat_summary = 0;
		session->rtcp_xr_stats.olds_ttl_or_hl_since_last_stat_summary = 0;
		session->rtcp_xr_stats.oldm_ttl_or_hl_since_last_stat_summary = mp->ttl_or_hl;
		session->rtcp_xr_stats.newm_ttl_or_hl_since_last_stat_summary = mp->ttl_or_hl;
	}
	compute_mean_and_deviation(session->rtcp_xr_stats.rcv_since_last_stat_summary,
		(double)mp->ttl_or_hl,
		&session->rtcp_xr_stats.olds_ttl_or_hl_since_last_stat_summary,
		&session->rtcp_xr_stats.oldm_ttl_or_hl_since_last_stat_summary,
		&session->rtcp_xr_stats.news_ttl_or_hl_since_last_stat_summary,
		&session->rtcp_xr_stats.newm_ttl_or_hl_since_last_stat_summary);
	if (mp->ttl_or_hl < session->rtcp_xr_stats.min_ttl_or_hl_since_last_stat_summary) {
		session->rtcp_xr_stats.min_ttl_or_hl_since_last_stat_summary = mp->ttl_or_hl;
	}
	if (mp->ttl_or_hl > session->rtcp_xr_stats.max_ttl_or_hl_since_last_stat_summary) {
		session->rtcp_xr_stats.max_ttl_or_hl_since_last_stat_summary = mp->ttl_or_hl;
	}

	/* Jitter statistics */
	if (session->rtcp_xr_stats.rcv_since_last_stat_summary == 1) {
		session->rtcp_xr_stats.min_jitter_since_last_stat_summary = 0xFFFFFFFF;
		session->rtcp_xr_stats.max_jitter_since_last_stat_summary = 0;
	} else {
		int64_t signed_jitter = diff - session->rtcp_xr_stats.last_jitter_diff_since_last_stat_summary;
		uint32_t jitter;
		if (signed_jitter < 0) {
			jitter = (uint32_t)(-signed_jitter);
		} else {
			jitter = (uint32_t)(signed_jitter);
		}
		compute_mean_and_deviation(session->rtcp_xr_stats.rcv_since_last_stat_summary - 1,
			(double)jitter,
			&session->rtcp_xr_stats.olds_jitter_since_last_stat_summary,
			&session->rtcp_xr_stats.oldm_jitter_since_last_stat_summary,
			&session->rtcp_xr_stats.news_jitter_since_last_stat_summary,
			&session->rtcp_xr_stats.newm_jitter_since_last_stat_summary);
		if (jitter < session->rtcp_xr_stats.min_jitter_since_last_stat_summary) {
			session->rtcp_xr_stats.min_jitter_since_last_stat_summary = jitter;
		}
		if (jitter > session->rtcp_xr_stats.max_jitter_since_last_stat_summary) {
			session->rtcp_xr_stats.max_jitter_since_last_stat_summary = jitter;
		}
	}
	session->rtcp_xr_stats.last_jitter_diff_since_last_stat_summary = diff;
}

void rtp_session_rtp_parse(RtpSession *session, mblk_t *mp, uint32_t local_str_ts, struct sockaddr *addr, socklen_t addrlen)
{
	int i;
	int discarded;
	int duplicate;
	rtp_header_t *rtp;
	int msgsize;
	RtpStream *rtpstream=&session->rtp;
	rtp_stats_t *stats=&session->stats;

	msgsize=(int)(mp->b_wptr-mp->b_rptr);

	if (msgsize<RTP_FIXED_HEADER_SIZE){
		ortp_warning("Packet too small to be a rtp packet (%i)!",msgsize);
		session->stats.bad++;
		ortp_global_stats.bad++;
		freemsg(mp);
		return;
	}
	rtp=(rtp_header_t*)mp->b_rptr;
	if (rtp->version!=2)
	{
		/* try to see if it is a STUN packet */
		uint16_t stunlen=*((uint16_t*)(mp->b_rptr + sizeof(uint16_t)));
		stunlen = ntohs(stunlen);
		if (stunlen+20==mp->b_wptr-mp->b_rptr){
			/* this looks like a stun packet */
			if (session->eventqs!=NULL){
				OrtpEvent *ev=ortp_event_new(ORTP_EVENT_STUN_PACKET_RECEIVED);
				OrtpEventData *ed=ortp_event_get_data(ev);
				ed->packet=mp;
				memcpy(&ed->source_addr,addr,addrlen);
				ed->source_addrlen=addrlen;
				ed->info.socket_type = OrtpRTPSocket;
				rtp_session_dispatch_event(session,ev);
				return;
			}
		}
		/* discard in two case: the packet is not stun OR nobody is interested by STUN (no eventqs) */
		ortp_debug("Receiving rtp packet with version number !=2...discarded");
		stats->bad++;
		ortp_global_stats.bad++;
		freemsg(mp);
		return;
	}

	/* only count non-stun packets. */
	ortp_global_stats.packet_recv++;
	stats->packet_recv++;
	ortp_global_stats.hw_recv+=msgsize;
	stats->hw_recv+=msgsize;
	session->rtp.hwrcv_since_last_SR++;
	session->rtcp_xr_stats.rcv_since_last_stat_summary++;

	/* convert all header data from network order to host order */
	rtp->seq_number=ntohs(rtp->seq_number);
	rtp->timestamp=ntohl(rtp->timestamp);
	rtp->ssrc=ntohl(rtp->ssrc);
	/* convert csrc if necessary */
	if (rtp->cc*sizeof(uint32_t) > (uint32_t) (msgsize-RTP_FIXED_HEADER_SIZE)){
		ortp_debug("Receiving too short rtp packet.");
		stats->bad++;
		ortp_global_stats.bad++;
		freemsg(mp);
		return;
	}

#ifndef PERF
	/* Write down the last RTP/RTCP packet reception time. */
	ortp_gettimeofday(&session->last_recv_time, NULL);
#endif

	for (i=0;i<rtp->cc;i++)
		rtp->csrc[i]=ntohl(rtp->csrc[i]);
	/*the goal of the following code is to lock on an incoming SSRC to avoid
	receiving "mixed streams"*/
	if (session->ssrc_set){
		/*the ssrc is set, so we must check it */
		if (session->rcv.ssrc!=rtp->ssrc){
			if (session->inc_ssrc_candidate==rtp->ssrc){
				session->inc_same_ssrc_count++;
			}else{
				session->inc_same_ssrc_count=0;
				session->inc_ssrc_candidate=rtp->ssrc;
			}
			if (session->inc_same_ssrc_count>=session->rtp.ssrc_changed_thres){
				/* store the sender rtp address to do symmetric RTP */
				rtp_session_update_remote_sock_addr(session,mp,TRUE,FALSE);
				session->rtp.rcv_last_ts = rtp->timestamp;
				session->rcv.ssrc=rtp->ssrc;
				rtp_signal_table_emit(&session->on_ssrc_changed);
			}else{
				/*discard the packet*/
				ortp_debug("Receiving packet with unknown ssrc.");
				stats->bad++;
				ortp_global_stats.bad++;
				freemsg(mp);
				return;
			}
		} else{
			/* The SSRC change must not happen if we still receive
			ssrc from the initial source. */
			session->inc_same_ssrc_count=0;
		}
	}else{
		session->ssrc_set=TRUE;
		session->rcv.ssrc=rtp->ssrc;
		rtp_session_update_remote_sock_addr(session,mp,TRUE,FALSE);
	}

	/* update some statistics */
	{
		poly32_t *extseq=(poly32_t*)&rtpstream->hwrcv_extseq;
		if (rtp->seq_number>extseq->split.lo){
			extseq->split.lo=rtp->seq_number;
		}else if (rtp->seq_number<200 && extseq->split.lo>((1<<16) - 200)){
			/* this is a check for sequence number looping */
			extseq->split.lo=rtp->seq_number;
			extseq->split.hi++;
		}

		/* the first sequence number received should be initialized at the beginning
		or at any resync, so that the first receiver reports contains valid loss rate*/
		if (!(session->flags & RTP_SESSION_RECV_SEQ_INIT)){
			rtp_session_set_flag(session, RTP_SESSION_RECV_SEQ_INIT);
			rtpstream->hwrcv_seq_at_last_SR=rtp->seq_number-1;
			session->rtcp_xr_stats.rcv_seq_at_last_stat_summary=rtp->seq_number-1;
		}
		if (stats->packet_recv==1){
			session->rtcp_xr_stats.first_rcv_seq=extseq->one;
		}
		session->rtcp_xr_stats.last_rcv_seq=extseq->one;
	}

	/* check for possible telephone events */
	if (rtp_profile_is_telephone_event(session->snd.profile, rtp->paytype)){
		queue_packet(&session->rtp.tev_rq,session->rtp.max_rq_size,mp,rtp,&discarded,&duplicate);
		stats->discarded+=discarded;
		ortp_global_stats.discarded+=discarded;
		stats->packet_dup_recv+=duplicate;
		ortp_global_stats.packet_dup_recv+=duplicate;
		session->rtcp_xr_stats.discarded_count += discarded;
		session->rtcp_xr_stats.dup_since_last_stat_summary += duplicate;
		return;
	}

	/* check for possible payload type change, in order to update accordingly our clock-rate dependant
	parameters */
	if (session->hw_recv_pt!=rtp->paytype){
		rtp_session_update_payload_type(session,rtp->paytype);
	}

	/* Drop the packets while the RTP_SESSION_FLUSH flag is set. */
	if (session->flags & RTP_SESSION_FLUSH) {
		freemsg(mp);
		return;
	}

	jitter_control_new_packet(&session->rtp.jittctl,rtp->timestamp,local_str_ts);

	update_rtcp_xr_stat_summary(session, mp, local_str_ts);

	if (session->flags & RTP_SESSION_FIRST_PACKET_DELIVERED) {
		/* detect timestamp important jumps in the future, to workaround stupid rtp senders */
		if (RTP_TIMESTAMP_IS_NEWER_THAN(rtp->timestamp,session->rtp.rcv_last_ts+session->rtp.ts_jump)){
			ortp_warning("rtp_parse: timestamp jump in the future detected.");
			rtp_signal_table_emit2(&session->on_timestamp_jump,&rtp->timestamp);
		}
		else if (RTP_TIMESTAMP_IS_STRICTLY_NEWER_THAN(session->rtp.rcv_last_ts,rtp->timestamp) 
			|| RTP_SEQ_IS_STRICTLY_GREATER_THAN(session->rtp.rcv_last_seq,rtp->seq_number)){
			/* don't queue packets older than the last returned packet to the application, or whose sequence number
			 is behind the last packet returned to the application*/
			/* Call timstamp jumb in case of
			 * large negative Ts jump or if ts is set to 0
			*/

			if ( RTP_TIMESTAMP_IS_STRICTLY_NEWER_THAN(session->rtp.rcv_last_ts, rtp->timestamp + session->rtp.ts_jump) ){
				ortp_warning("rtp_parse: negative timestamp jump detected");
				rtp_signal_table_emit2(&session->on_timestamp_jump, &rtp->timestamp);
			}
			ortp_debug("rtp_parse: discarding too old packet (ts=%i)",rtp->timestamp);
			freemsg(mp);
			stats->outoftime++;
			ortp_global_stats.outoftime++;
			session->rtcp_xr_stats.discarded_count++;
			return;
		}
	}

	if (queue_packet(&session->rtp.rq,session->rtp.max_rq_size,mp,rtp,&discarded,&duplicate))
		jitter_control_update_size(&session->rtp.jittctl,&session->rtp.rq);
	stats->discarded+=discarded;
	ortp_global_stats.discarded+=discarded;
	stats->packet_dup_recv+=duplicate;
	ortp_global_stats.packet_dup_recv+=duplicate;
	session->rtcp_xr_stats.discarded_count += discarded;
	session->rtcp_xr_stats.dup_since_last_stat_summary += duplicate;
	if ((discarded == 0) && (duplicate == 0)) {
		session->rtcp_xr_stats.rcv_count++;
	}
}

