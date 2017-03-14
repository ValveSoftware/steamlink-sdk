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

/***************************************************************************
 *            rtcp.c
 *
 *  Wed Dec  1 11:45:30 2004
 *  Copyright  2004  Simon Morlat
 *  Email simon dot morlat at linphone dot org
 ****************************************************************************/

#include "ortp/ortp.h"
#include "ortp/rtpsession.h"
#include "ortp/rtcp.h"
#include "utils.h"
#include "rtpsession_priv.h"
#include "jitterctl.h"

#define rtcp_bye_set_ssrc(b,pos,ssrc)	(b)->ssrc[pos]=htonl(ssrc)
#define rtcp_bye_get_ssrc(b,pos)		ntohl((b)->ssrc[pos])


static mblk_t *rtcp_create_simple_bye_packet(uint32_t ssrc, const char *reason) {
	int packet_size;
	int strsize = 0;
	int strpadding = 0;
	mblk_t *mp;
	rtcp_bye_t *rtcp;

	packet_size = RTCP_BYE_HEADER_SIZE;
	if (reason!=NULL) {
		strsize=(int)MIN(strlen(reason),RTCP_BYE_REASON_MAX_STRING_SIZE);
		if (strsize > 0) {
			strpadding = 3 - (strsize % 4);
			packet_size += 1 + strsize + strpadding;
		}
	}
	mp	= allocb(packet_size, 0);

	rtcp = (rtcp_bye_t*)mp->b_rptr;
	rtcp_common_header_init(&rtcp->ch,NULL,RTCP_BYE,1,packet_size);
	rtcp->ssrc[0] = htonl(ssrc);
	mp->b_wptr += RTCP_BYE_HEADER_SIZE;
	/* append the reason if any*/
	if (reason!=NULL) {
		const char pad[] = {0, 0, 0};
		unsigned char strsize_octet = (unsigned char)strsize;

		appendb(mp, (const char*)&strsize_octet, 1, FALSE);
		appendb(mp, reason,strsize, FALSE);
		appendb(mp, pad,strpadding, FALSE);
	}
	return mp;
}

static mblk_t *sdes_chunk_new(uint32_t ssrc){
	mblk_t *m=allocb(RTCP_SDES_CHUNK_DEFAULT_SIZE,0);
	sdes_chunk_t *sc=(sdes_chunk_t*)m->b_rptr;
	sc->csrc=htonl(ssrc);
	m->b_wptr+=sizeof(sc->csrc);
	return m;
}

static mblk_t * sdes_chunk_append_item(mblk_t *m, rtcp_sdes_type_t sdes_type, const char *content) {
	if ( content )
	{
		sdes_item_t si;
		si.item_type=sdes_type;
		si.len=(uint8_t) MIN(strlen(content),RTCP_SDES_MAX_STRING_SIZE);
		m=appendb(m,(char*)&si,RTCP_SDES_ITEM_HEADER_SIZE,FALSE);
		m=appendb(m,content,si.len,FALSE);
	}
	return m;
}

static void sdes_chunk_set_ssrc(mblk_t *m, uint32_t ssrc){
	sdes_chunk_t *sc=(sdes_chunk_t*)m->b_rptr;
	sc->csrc=htonl(ssrc);
}

#define sdes_chunk_get_ssrc(m) ntohl(((sdes_chunk_t*)((m)->b_rptr))->csrc)

static mblk_t * sdes_chunk_pad(mblk_t *m){
	return appendb(m,"",1,TRUE);
}

static mblk_t * sdes_chunk_set_minimal_items(mblk_t *m, const char *cname) {
	if (cname == NULL) {
		cname = "Unknown";
	}
	return sdes_chunk_append_item(m, RTCP_SDES_CNAME, cname);
}

static mblk_t * sdes_chunk_set_full_items(mblk_t *m, const char *cname,
	const char *name, const char *email, const char *phone, const char *loc,
	const char *tool, const char *note) {
	m = sdes_chunk_set_minimal_items(m, cname);
	m = sdes_chunk_append_item(m, RTCP_SDES_NAME, name);
	m = sdes_chunk_append_item(m, RTCP_SDES_EMAIL, email);
	m = sdes_chunk_append_item(m, RTCP_SDES_PHONE, phone);
	m = sdes_chunk_append_item(m, RTCP_SDES_LOC, loc);
	m = sdes_chunk_append_item(m, RTCP_SDES_TOOL, tool);
	m = sdes_chunk_append_item(m, RTCP_SDES_NOTE, note);
	m = sdes_chunk_pad(m);
	return m;
}

/**
 * Set session's SDES item for automatic sending of RTCP compound packets.
 * If some items are not specified, use NULL.
**/
void rtp_session_set_source_description(RtpSession *session, const char *cname,
	const char *name, const char *email, const char *phone, const char *loc,
	const char *tool, const char *note) {
	mblk_t *m;
	mblk_t *chunk = sdes_chunk_new(session->snd.ssrc);
	if (strlen(cname)>255) {
		/*
		 * rfc3550,
		 * 6.5 SDES: Source Description RTCP Packet
		 * ...
		 * Note that the text can be no longer than 255 octets,
		 *
		 * */
		ortp_warning("Cname [%s] too long for session [%p]",cname,session);
	}

	sdes_chunk_set_full_items(chunk, cname, name, email, phone, loc, tool, note);
	if (session->full_sdes != NULL)
		freemsg(session->full_sdes);
	session->full_sdes = chunk;
	chunk = sdes_chunk_new(session->snd.ssrc);
	m = sdes_chunk_set_minimal_items(chunk, cname);
	m = sdes_chunk_pad(m);
	if (session->minimal_sdes != NULL)
		freemsg(session->minimal_sdes);
	session->minimal_sdes = chunk;
}

void rtp_session_add_contributing_source(RtpSession *session, uint32_t csrc,
	const char *cname, const char *name, const char *email, const char *phone,
	const char *loc, const char *tool, const char *note) {
	mblk_t *chunk = sdes_chunk_new(csrc);
	sdes_chunk_set_full_items(chunk, cname, name, email, phone, loc, tool, note);
	putq(&session->contributing_sources, chunk);
}

void rtp_session_remove_contributing_source(RtpSession *session, uint32_t ssrc) {
	queue_t *q=&session->contributing_sources;
	mblk_t *tmp;
	for (tmp=qbegin(q); !qend(q,tmp); tmp=qnext(q,tmp)){
		uint32_t csrc=sdes_chunk_get_ssrc(tmp);
		if (csrc==ssrc) {
			remq(q,tmp);
			break;
		}
	}
	tmp=rtcp_create_simple_bye_packet(ssrc, NULL);
	rtp_session_rtcp_send(session,tmp);
}

void rtcp_common_header_init(rtcp_common_header_t *ch, RtpSession *s,int type, int rc, size_t bytes_len){
	rtcp_common_header_set_version(ch,2);
	rtcp_common_header_set_padbit(ch,0);
	rtcp_common_header_set_packet_type(ch,type);
	rtcp_common_header_set_rc(ch,rc);	/* as we don't yet support multi source receiving */
	rtcp_common_header_set_length(ch,(unsigned short)((bytes_len/4)-1));
}

mblk_t* rtp_session_create_rtcp_sdes_packet(RtpSession *session, bool_t full) {
	mblk_t *mp = allocb(sizeof(rtcp_common_header_t), 0);
	rtcp_common_header_t *rtcp;
	mblk_t *tmp;
	mblk_t *m = mp;
	mblk_t *sdes;
	queue_t *q;
	int rc = 0;

	sdes = (full == TRUE) ? session->full_sdes : session->minimal_sdes;
	rtcp = (rtcp_common_header_t *)mp->b_wptr;
	mp->b_wptr += sizeof(rtcp_common_header_t);

	/* Concatenate all sdes chunks. */
	sdes_chunk_set_ssrc(sdes, session->snd.ssrc);
	m = concatb(m, dupmsg(sdes));
	rc++;

	if (full == TRUE) {
		q = &session->contributing_sources;
		for (tmp = qbegin(q); !qend(q, tmp); tmp = qnext(q, mp)) {
			m = concatb(m, dupmsg(tmp));
			rc++;
		}
	}
	rtcp_common_header_init(rtcp, session, RTCP_SDES, rc, msgdsize(mp));

	return mp;
}

static void sender_info_init(sender_info_t *info, RtpSession *session){
	struct timeval tv;
	uint64_t ntp;
	ortp_gettimeofday(&tv,NULL);
	ntp=ortp_timeval_to_ntp(&tv);
	info->ntp_timestamp_msw=htonl(ntp >>32);
	info->ntp_timestamp_lsw=htonl(ntp & 0xFFFFFFFF);
	info->rtp_timestamp=htonl(session->rtp.snd_last_ts);
	info->senders_packet_count=(uint32_t) htonl((u_long) session->stats.packet_sent);
	info->senders_octet_count=(uint32_t) htonl((u_long) session->rtp.sent_payload_bytes);
	session->rtp.last_rtcp_packet_count=(uint32_t)session->stats.packet_sent;
}

static void report_block_init(report_block_t *b, RtpSession *session){
	int packet_loss=0;
	int loss_fraction=0;
	RtpStream *stream=&session->rtp;
	uint32_t delay_snc_last_sr=0;

	/* compute the statistics */
	if (stream->hwrcv_since_last_SR!=0){
		int expected_packets=(int)((int64_t)stream->hwrcv_extseq - (int64_t)stream->hwrcv_seq_at_last_SR);

		if ( session->flags & RTCP_OVERRIDE_LOST_PACKETS ) {
			/* If the test mode is enabled, replace the lost packet field with
			the test vector value set by rtp_session_rtcp_set_lost_packet_value() */
			packet_loss = session->lost_packets_test_vector;
			/* The test value is the definite cumulative one, no need to increment
			it each time a packet is sent */
			session->stats.cum_packet_loss = packet_loss;
		}else {
			/* Normal mode */
			packet_loss = (int)((int64_t)expected_packets - (int64_t)stream->hwrcv_since_last_SR);
			session->stats.cum_packet_loss += packet_loss;
		}
		if (expected_packets>0){/*prevent division by zero and negative loss fraction*/
			loss_fraction=(int)( 256 * packet_loss) / expected_packets;
			/*make sure this fits into 8 bit unsigned*/
			if (loss_fraction>255) loss_fraction=255;
			else if (loss_fraction<0) loss_fraction=0;
		}else{
			loss_fraction=0;
		}
	}
	ortp_debug("report_block_init[%p]:\n"
		"\texpected_packets=%d=%u-%u\n"
		"\thwrcv_since_last_SR=%u\n"
		"\tpacket_loss=%d\n"
		"\tcum_packet_loss=%lld\n"
		"\tloss_fraction=%f%%\n"
		, session
		, stream->hwrcv_extseq - stream->hwrcv_seq_at_last_SR, stream->hwrcv_extseq, stream->hwrcv_seq_at_last_SR
		, stream->hwrcv_since_last_SR
		, packet_loss
		, (long long)session->stats.cum_packet_loss
		, loss_fraction/2.56
	);

	/* reset them */
	stream->hwrcv_since_last_SR=0;
	stream->hwrcv_seq_at_last_SR=stream->hwrcv_extseq;

	if (stream->last_rcv_SR_time.tv_sec!=0){
		struct timeval now;
		double delay;
		ortp_gettimeofday(&now,NULL);
		delay= (now.tv_sec-stream->last_rcv_SR_time.tv_sec)+ ((now.tv_usec-stream->last_rcv_SR_time.tv_usec)*1e-6);
		delay= (delay*65536);
		delay_snc_last_sr=(uint32_t) delay;
	}

	b->ssrc=htonl(session->rcv.ssrc);


	report_block_set_cum_packet_lost(b, session->stats.cum_packet_loss);
	report_block_set_fraction_lost(b, loss_fraction);

	if ( session->flags & RTCP_OVERRIDE_JITTER ) {
		/* If the test mode is enabled, replace the interarrival jitter field with the test vector value set by rtp_session_rtcp_set_jitter_value() */
		b->interarrival_jitter = htonl( session->interarrival_jitter_test_vector );
	}
	else {
		/* Normal mode */
		b->interarrival_jitter = htonl( (uint32_t) stream->jittctl.inter_jitter );
	}
	b->ext_high_seq_num_rec=htonl(stream->hwrcv_extseq);
	b->delay_snc_last_sr=htonl(delay_snc_last_sr);
	if ( session->flags & RTCP_OVERRIDE_DELAY ) {
		/* If the test mode is enabled, modifies the returned ts (LSR) so it matches the value of the delay test value */
		/* refer to the rtp_session_rtcp_set_delay_value() documentation for further explanations */
		double new_ts = ( (double)stream->last_rcv_SR_time.tv_sec + (double)stream->last_rcv_SR_time.tv_usec * 1e-6 ) - ( (double)session->delay_test_vector / 1000.0 );
		uint32_t new_ts2;

		/* Converting the time format in RFC3550 (par. 4) format */
		new_ts += 2208988800.0; /* 2208988800 is the number of seconds from 1900 to 1970 (January 1, Oh TU) */
		new_ts = 65536.0 * new_ts;
		/* This non-elegant way of coding fits with the gcc and the icc compilers */
		new_ts2 = (uint32_t)( (uint64_t)new_ts & 0xffffffff );
		b->lsr = htonl( new_ts2 );
	}
	else {
		/* Normal mode */
		b->lsr = htonl( stream->last_rcv_SR_ts );
	}
}

static void extended_statistics( RtpSession *session, report_block_t * rb ) {
	/* the jitter raw value is kept in stream clock units */
	uint32_t jitter = (uint32_t)session->rtp.jittctl.inter_jitter;
	session->stats.sent_rtcp_packets ++;
	session->rtp.jitter_stats.sum_jitter += jitter;
	session->rtp.jitter_stats.jitter=jitter;
	/* stores the biggest jitter for that session and its date (in millisecond) since Epoch */
	if ( jitter > session->rtp.jitter_stats.max_jitter ) {
		struct timeval now;

		session->rtp.jitter_stats.max_jitter = jitter ;

		ortp_gettimeofday( &now, NULL );
		session->rtp.jitter_stats.max_jitter_ts = ( now.tv_sec * 1000LL ) + ( now.tv_usec / 1000LL );
	}
	/* compute mean jitter buffer size */
	session->rtp.jitter_stats.jitter_buffer_size_ms=jitter_control_compute_mean_size(&session->rtp.jittctl);
}

static size_t rtcp_sr_init(RtpSession *session, uint8_t *buf, size_t size){
	rtcp_sr_t *sr=(rtcp_sr_t*)buf;
	int rr=(session->stats.packet_recv>0);
	size_t sr_size=sizeof(rtcp_sr_t)-sizeof(report_block_t)+(rr*sizeof(report_block_t));
	if (size<sr_size) return 0;
	rtcp_common_header_init(&sr->ch,session,RTCP_SR,rr,sr_size);
	sr->ssrc=htonl(session->snd.ssrc);
	sender_info_init(&sr->si,session);
	/*only include a report block if packets were received*/
	if (rr) {
		report_block_init( &sr->rb[0], session );
		extended_statistics( session, &sr->rb[0] );
	}
	return sr_size;
}

static size_t rtcp_rr_init(RtpSession *session, uint8_t *buf, size_t size){
	rtcp_rr_t *rr=(rtcp_rr_t*)buf;
	if (size<sizeof(rtcp_rr_t)) return 0;
	rtcp_common_header_init(&rr->ch,session,RTCP_RR,1,sizeof(rtcp_rr_t));
	rr->ssrc=htonl(session->snd.ssrc);
	report_block_init(&rr->rb[0],session);
	extended_statistics( session, &rr->rb[0] );
	return sizeof(rtcp_rr_t);
}

static size_t rtcp_app_init(RtpSession *session, uint8_t *buf, uint8_t subtype, const char *name, size_t size){
	rtcp_app_t *app=(rtcp_app_t*)buf;
	if (size<sizeof(rtcp_app_t)) return 0;
	rtcp_common_header_init(&app->ch,session,RTCP_APP,subtype,size);
	app->ssrc=htonl(session->snd.ssrc);
	memset(app->name,0,4);
	strncpy(app->name,name,4);
	return sizeof(rtcp_app_t);
}

static mblk_t * make_rr(RtpSession *session) {
	mblk_t *cm = allocb(sizeof(rtcp_sr_t), 0);
	cm->b_wptr += rtcp_rr_init(session, cm->b_wptr, sizeof(rtcp_rr_t));
	return cm;
}

static mblk_t * make_sr(RtpSession *session) {
	mblk_t *cm = allocb(sizeof(rtcp_sr_t), 0);
	cm->b_wptr += rtcp_sr_init(session, cm->b_wptr, sizeof(rtcp_sr_t));
	return cm;
}

static mblk_t * append_sdes(RtpSession *session, mblk_t *m, bool_t full) {
	mblk_t *sdes = NULL;

	if ((full == TRUE) && (session->full_sdes != NULL)) {
		sdes = rtp_session_create_rtcp_sdes_packet(session, full);
	} else if ((full == FALSE) && (session->minimal_sdes != NULL)) {
		sdes = rtp_session_create_rtcp_sdes_packet(session, full);
	}
	return concatb(m, sdes);
}

static void notify_sent_rtcp(RtpSession *session, mblk_t *rtcp){
	if (session->eventqs!=NULL){
		OrtpEvent *ev;
		OrtpEventData *evd;
		ev=ortp_event_new(ORTP_EVENT_RTCP_PACKET_EMITTED);
		evd=ortp_event_get_data(ev);
		evd->packet=dupmsg(rtcp);
		msgpullup(evd->packet,-1);
		rtp_session_dispatch_event(session,ev);
	}
}

static void append_xr_packets(RtpSession *session, mblk_t *m) {
	if (session->rtcp.xr_conf.rcvr_rtt_mode != OrtpRtcpXrRcvrRttNone) {
		concatb(m, make_xr_rcvr_rtt(session));
	}
	if (session->rtcp.rtcp_xr_dlrr_to_send == TRUE) {
		concatb(m, make_xr_dlrr(session));
		session->rtcp.rtcp_xr_dlrr_to_send = FALSE;
	}
	if (session->rtcp.xr_conf.stat_summary_enabled == TRUE) {
		concatb(m, make_xr_stat_summary(session));
	}
	if (session->rtcp.xr_conf.voip_metrics_enabled == TRUE) {
		concatb(m, make_xr_voip_metrics(session));
	}
}

static void append_fb_packets(RtpSession *session, mblk_t *m) {
	if (session->rtcp.send_algo.fb_packets != NULL) {
		concatb(m, session->rtcp.send_algo.fb_packets);
		session->rtcp.send_algo.fb_packets = NULL;
	}

	/* Repeat TMMBR packets until they are acknowledged with a TMMBN unless a TMMBN is being sent. */
	if (rtp_session_avpf_feature_enabled(session, ORTP_AVPF_FEATURE_TMMBR)
		&& (session->rtcp.tmmbr_info.sent != NULL)
		&& (session->rtcp.send_algo.tmmbr_scheduled != TRUE)
		&& (session->rtcp.send_algo.tmmbn_scheduled != TRUE)) {
		concatb(m, copymsg(session->rtcp.tmmbr_info.sent));
	}

	session->rtcp.send_algo.tmmbr_scheduled = FALSE;
	session->rtcp.send_algo.tmmbn_scheduled = FALSE;
}

static void rtp_session_create_and_send_rtcp_packet(RtpSession *session, bool_t full) {
	mblk_t *m=NULL;
	bool_t is_sr = FALSE;

	if (session->rtp.last_rtcp_packet_count < session->stats.packet_sent) {
		m = make_sr(session);
		session->rtp.last_rtcp_packet_count = (uint32_t)session->stats.packet_sent;
		is_sr = TRUE;
	} else if (session->stats.packet_recv > 0) {
		/* Don't send RR when no packet are received yet */
		m = make_rr(session);
		is_sr = FALSE;
	}
	if (m != NULL) {
		append_sdes(session, m, full);
		if ((full == TRUE) && (session->rtcp.xr_conf.enabled == TRUE)) {
			append_xr_packets(session, m);
		}
		if (rtp_session_avpf_enabled(session) == TRUE) {
			append_fb_packets(session, m);
		}
		/* Send the compound packet */
		notify_sent_rtcp(session, m);
		ortp_message("Sending RTCP %s compound message on session [%p].",(is_sr ? "SR" : "RR"), session);
		rtp_session_rtcp_send(session, m);
	}
}

static float rtcp_rand(float t) {
	return t * ((rand() / (RAND_MAX * 1.0f)) + 0.5f);
}

/**
 * This is a simplified version with this limit of the algorithm described in
 * the appendix A.7 of RFC3550.
 */
void compute_rtcp_interval(RtpSession *session) {
	float t;
	float rtcp_min_time;
	float rtcp_bw;

	if (session->target_upload_bandwidth == 0) return;

	/* Compute target RTCP bandwidth in bits/s. */
	rtcp_bw = 0.05f * session->target_upload_bandwidth;

	if (rtp_session_avpf_enabled(session) == TRUE) {
		session->rtcp.send_algo.T_rr_interval = rtp_session_get_avpf_rr_interval(session);
		rtcp_min_time = (float)session->rtcp.send_algo.Tmin;
	} else {
		rtcp_min_time = (float)session->rtcp.send_algo.T_rr_interval;
		if (session->rtcp.send_algo.initial == TRUE) {
			rtcp_min_time /= 2.;
		}
	}

	t = ((session->rtcp.send_algo.avg_rtcp_size * 8 * 2) / rtcp_bw) * 1000;
	if (t < rtcp_min_time) t = rtcp_min_time;
	t = rtcp_rand(t);
	t = t / (2.71828f - 1.5f); /* Compensation */
	session->rtcp.send_algo.T_rr = (uint32_t)t;
}

void update_avg_rtcp_size(RtpSession *session, int bytes) {
	int overhead = (ortp_stream_is_ipv6(&session->rtcp.gs) == TRUE) ? IP6_UDP_OVERHEAD : IP_UDP_OVERHEAD;
	int size = bytes + overhead;
	session->rtcp.send_algo.avg_rtcp_size = ((size + (15 * session->rtcp.send_algo.avg_rtcp_size)) / 16.f);
}

static void rtp_session_schedule_first_rtcp_send(RtpSession *session) {
	uint64_t tc;
	size_t overhead;
	size_t report_size;
	size_t sdes_size;
	size_t xr_size = 0;
	OrtpRtcpSendAlgorithm *sa = &session->rtcp.send_algo;

	if ((session->rtcp.enabled == FALSE) || (session->target_upload_bandwidth == 0) || (sa->initialized == TRUE))
		return;

	overhead = (ortp_stream_is_ipv6(&session->rtcp.gs) == TRUE) ? IP6_UDP_OVERHEAD : IP_UDP_OVERHEAD;
	sdes_size = (session->full_sdes != NULL) ? msgdsize(session->full_sdes) + sizeof(rtcp_common_header_t) : 0;
	switch (session->mode) {
		case RTP_SESSION_RECVONLY:
			report_size = sizeof(rtcp_rr_t);
			break;
		case RTP_SESSION_SENDONLY:
			report_size = sizeof(rtcp_sr_t) - sizeof(report_block_t);
			break;
		case RTP_SESSION_SENDRECV:
		default:
			report_size = sizeof(rtcp_sr_t);
			break;
	}
	if (session->rtcp.xr_conf.enabled == TRUE) {
		if (session->rtcp.xr_conf.rcvr_rtt_mode != OrtpRtcpXrRcvrRttNone)
			xr_size += sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_rcvr_rtt_report_block_t);
		if (session->rtcp.xr_conf.stat_summary_enabled == TRUE)
			xr_size += sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_stat_summary_report_block_t);
		if (session->rtcp.xr_conf.voip_metrics_enabled == TRUE)
			xr_size += sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_voip_metrics_report_block_t);
	}
	sa->avg_rtcp_size = (float)(overhead + report_size + sdes_size + xr_size);
	sa->initialized = TRUE;

	tc = ortp_get_cur_time_ms();
	compute_rtcp_interval(session);
	if (sa->T_rr > 0) sa->tn = tc + sa->T_rr;
	sa->tp = tc;
	sa->t_rr_last = tc;
	sa->Tmin = 0;
}

static void rtp_session_reschedule(RtpSession *session, uint64_t tc) {
	OrtpRtcpSendAlgorithm *sa = &session->rtcp.send_algo;
	if (rtp_session_avpf_enabled(session) == TRUE) {
		sa->tp = tc;
		sa->tn = tc + sa->T_rr;
	}
}

void rtp_session_send_regular_rtcp_packet_and_reschedule(RtpSession *session, uint64_t tc) {
	OrtpRtcpSendAlgorithm *sa = &session->rtcp.send_algo;
	rtp_session_create_and_send_rtcp_packet(session, TRUE);
	sa->tp = tc;
	sa->t_rr_last = sa->tn;
	compute_rtcp_interval(session);
	sa->tn = tc + sa->T_rr;
	sa->initial = FALSE;
}

void rtp_session_send_fb_rtcp_packet_and_reschedule(RtpSession *session) {
	uint64_t previous_tn;
	OrtpRtcpSendAlgorithm *sa = &session->rtcp.send_algo;
	rtp_session_create_and_send_rtcp_packet(session, FALSE);
	sa->allow_early = FALSE;
	previous_tn = sa->tn;
	sa->tn = sa->tp + 2 * sa->T_rr;
	sa->tp = previous_tn;
}

void rtp_session_run_rtcp_send_scheduler(RtpSession *session) {
	uint64_t tc = ortp_get_cur_time_ms();
	OrtpRtcpSendAlgorithm *sa = &session->rtcp.send_algo;

	if (tc >= sa->tn) {
		compute_rtcp_interval(session);
		sa->tn = sa->tp + sa->T_rr;
		if (tc >= sa->tn) {
			if (sa->t_rr_last == 0) {
				rtp_session_schedule_first_rtcp_send(session);
			} else {
				if (sa->T_rr_interval != 0) {
					sa->T_rr_current_interval = (uint32_t)rtcp_rand((float)sa->T_rr_interval);
				} else {
					sa->T_rr_current_interval = 0;
				}
				if (sa->tn >= (sa->t_rr_last + sa->T_rr_current_interval)) {
					rtp_session_send_regular_rtcp_packet_and_reschedule(session, tc);
				} else if (rtp_session_has_fb_packets_to_send(session) == TRUE) {
					rtp_session_send_fb_rtcp_packet_and_reschedule(session);
				} else {
					rtp_session_reschedule(session, tc);
				}
			}
		}
	}
}

void rtp_session_rtcp_process_send(RtpSession *session) {
	rtp_session_run_rtcp_send_scheduler(session);
}

void rtp_session_rtcp_process_recv(RtpSession *session){
	rtp_session_run_rtcp_send_scheduler(session);
}

void rtp_session_send_rtcp_APP(RtpSession *session, uint8_t subtype, const char *name, const uint8_t *data, int datalen){
	mblk_t *h=allocb(sizeof(rtcp_app_t),0);
	mblk_t *d;
	h->b_wptr+=rtcp_app_init(session,h->b_wptr,subtype,name,datalen+sizeof(rtcp_app_t));
	d=esballoc((uint8_t*)data,datalen,0,NULL);
	d->b_wptr+=datalen;
	h->b_cont=d;
	rtp_session_rtcp_send(session,h);
}

/**
 * Sends a RTCP bye packet.
 *@param session RtpSession
 *@param reason the reason phrase.
**/
int rtp_session_bye(RtpSession *session, const char *reason) {
    mblk_t *cm;
    mblk_t *sdes = NULL;
    mblk_t *bye = NULL;
    int ret;

    /* Make a BYE packet (will be on the end of the compund packet). */
    bye = rtcp_create_simple_bye_packet(session->snd.ssrc, reason);

    /* SR or RR is determined by the fact whether stream was sent*/
    if (session->stats.packet_sent>0)
    {
        cm = allocb(sizeof(rtcp_sr_t), 0);
        cm->b_wptr += rtcp_sr_init(session,cm->b_wptr, sizeof(rtcp_sr_t));
        /* make a SDES packet */
        sdes = rtp_session_create_rtcp_sdes_packet(session, TRUE);
        /* link them */
        concatb(concatb(cm, sdes), bye);
    } else if (session->stats.packet_recv>0){
        /* make a RR packet */
        cm = allocb(sizeof(rtcp_rr_t), 0);
        cm->b_wptr += rtcp_rr_init(session, cm->b_wptr, sizeof(rtcp_rr_t));
        /* link them */
        cm->b_cont = bye;
    }else cm=bye;

    /* Send compound packet. */
    ret = rtp_session_rtcp_send(session, cm);

    return ret;
}

OrtpLossRateEstimator * ortp_loss_rate_estimator_new(int min_packet_count_interval, uint64_t min_time_ms_interval, RtpSession *session){
	OrtpLossRateEstimator *obj=ortp_malloc(sizeof(OrtpLossRateEstimator));
	ortp_loss_rate_estimator_init(obj,min_packet_count_interval, min_time_ms_interval, session);
	return obj;
}

void ortp_loss_rate_estimator_init(OrtpLossRateEstimator *obj, int min_packet_count_interval, uint64_t min_time_ms_interval, RtpSession *session){
	memset(obj,0,sizeof(*obj));
	obj->min_packet_count_interval=min_packet_count_interval;
	obj->last_ext_seq=rtp_session_get_seq_number(session);
	obj->last_cum_loss=rtp_session_get_cum_loss(session);
	obj->last_packet_sent_count=session->stats.packet_sent;
	obj->last_dup_packet_sent_count=session->stats.packet_dup_sent;
	obj->min_time_ms_interval=min_time_ms_interval;
	obj->last_estimate_time_ms=(uint64_t)-1;
}

bool_t ortp_loss_rate_estimator_process_report_block(OrtpLossRateEstimator *obj, const RtpSession *session, const report_block_t *rb){
	int32_t cum_loss=report_block_get_cum_packet_lost(rb);
	int32_t extseq=report_block_get_high_ext_seq(rb);
	int32_t diff_unique_outgoing=(int32_t)(session->stats.packet_sent-obj->last_packet_sent_count);
	int32_t diff_total_outgoing=diff_unique_outgoing+(int32_t)(session->stats.packet_dup_sent-obj->last_dup_packet_sent_count);
	int32_t diff;
	uint64_t curtime;
	bool_t got_value=FALSE;

	if (obj->last_ext_seq==-1 || obj->last_estimate_time_ms==(uint64_t)-1){
		/*first report cannot be considered, since we don't know the interval it covers*/
		obj->last_ext_seq=extseq;
		obj->last_cum_loss=cum_loss;
		obj->last_estimate_time_ms=ortp_get_cur_time_ms();
		return FALSE;
	}
	diff=extseq-obj->last_ext_seq;
	curtime=ortp_get_cur_time_ms();
	if (diff<0 || diff>obj->min_packet_count_interval * 100){
		ortp_warning("ortp_loss_rate_estimator_process %p: Suspected discontinuity in sequence numbering from %d to %d.", obj, obj->last_ext_seq, extseq);
		obj->last_ext_seq=extseq;
		obj->last_cum_loss=cum_loss;
		obj->last_packet_sent_count=session->stats.packet_sent;
		obj->last_dup_packet_sent_count=session->stats.packet_dup_sent;
	}else if (diff>obj->min_packet_count_interval && curtime-obj->last_estimate_time_ms>=obj->min_time_ms_interval){
		/*we have sufficient interval*/
		int32_t new_losses=cum_loss-obj->last_cum_loss;
		/*if we are using duplicates, they will not be visible in 'diff' variable.
		But since we are the emitter, we can retrieve the total count of packet we
		sent and use this value to compute the loss rate instead.*/
		obj->loss_rate = 100.f * (1.f - MAX(0, (diff_unique_outgoing - new_losses) * 1.f / diff_total_outgoing));

		/*update last values with current*/
		got_value=TRUE;
		obj->last_estimate_time_ms=curtime;

		if (obj->loss_rate>100.f){
			ortp_error("ortp_loss_rate_estimator_process %p: Loss rate MUST NOT be greater than 100%%", obj);
		}
		obj->last_ext_seq=extseq;
		obj->last_cum_loss=cum_loss;
		obj->last_packet_sent_count=session->stats.packet_sent;
		obj->last_dup_packet_sent_count=session->stats.packet_dup_sent;
	}
	return got_value;

}

float ortp_loss_rate_estimator_get_value(OrtpLossRateEstimator *obj){
	return obj->loss_rate;
}

void ortp_loss_rate_estimator_uninit(OrtpLossRateEstimator *obj){
}

void ortp_loss_rate_estimator_destroy(OrtpLossRateEstimator *obj){
	ortp_free(obj);
}
