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


#include "ortp/ortp.h"
#include "utils.h"

static size_t rtcp_get_size(const mblk_t *m){
	const rtcp_common_header_t *ch=rtcp_get_common_header(m);
	if (ch==NULL) return 0;
	return (1+rtcp_common_header_get_length(ch))*4;
}

/*in case of compound packet, set read pointer of m to the beginning of the next RTCP
packet */
bool_t rtcp_next_packet(mblk_t *m){
	size_t nextlen=rtcp_get_size(m);
	if ((nextlen > 0) && (m->b_rptr + nextlen < m->b_wptr)){
		m->b_rptr+=nextlen;
		return TRUE;
	}
	return FALSE;
}

void rtcp_rewind(mblk_t *m){
	m->b_rptr=m->b_datap->db_base;
}

/* get common header; this function will also check the sanity of the packet*/
const rtcp_common_header_t * rtcp_get_common_header(const mblk_t *m){
	size_t size=msgdsize(m);
	rtcp_common_header_t *ch;
	if (m->b_cont!=NULL){
		ortp_fatal("RTCP parser does not work on fragmented mblk_t. Use msgpullup() before to re-assemble the packet.");
		return NULL;
	}
	if (size<sizeof(rtcp_common_header_t)){
		ortp_warning("Bad RTCP packet, too short [%i b]. on block [%p]",(int)size,m);
		return NULL;
	}
	ch=(rtcp_common_header_t*)m->b_rptr;
	return ch;
}

bool_t rtcp_is_SR(const mblk_t *m){
	const rtcp_common_header_t *ch=rtcp_get_common_header(m);
	if (ch!=NULL && rtcp_common_header_get_packet_type(ch)==RTCP_SR){
		if (msgdsize(m)<(sizeof(rtcp_sr_t)-sizeof(report_block_t))){
			ortp_warning("Too short RTCP SR packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/*Sender Report accessors */
uint32_t rtcp_SR_get_ssrc(const mblk_t *m){
	rtcp_sr_t *sr=(rtcp_sr_t*)m->b_rptr;
	return ntohl(sr->ssrc);
}

const sender_info_t * rtcp_SR_get_sender_info(const mblk_t *m){
	rtcp_sr_t *sr=(rtcp_sr_t*)m->b_rptr;
	return &sr->si;
}

const report_block_t * rtcp_SR_get_report_block(const mblk_t *m, int idx){
	rtcp_sr_t *sr=(rtcp_sr_t*)m->b_rptr;
	report_block_t *rb=&sr->rb[idx];
	size_t size=rtcp_get_size(m);
	if ( ( (uint8_t*)rb)+sizeof(report_block_t) <= m->b_rptr + size ) {
		return rb;
	}else{
		if (idx<rtcp_common_header_get_rc(&sr->ch)){
			ortp_warning("RTCP packet should include a report_block_t at pos %i but has no space for it.",idx);
		}
	}
	return NULL;
}

/*Receiver report accessors*/
bool_t rtcp_is_RR(const mblk_t *m){
	const rtcp_common_header_t *ch=rtcp_get_common_header(m);
	if (ch!=NULL && rtcp_common_header_get_packet_type(ch)==RTCP_RR){
		if (msgdsize(m)<sizeof(rtcp_rr_t)){
			ortp_warning("Too short RTCP RR packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

uint32_t rtcp_RR_get_ssrc(const mblk_t *m){
	rtcp_rr_t *rr=(rtcp_rr_t*)m->b_rptr;
	return ntohl(rr->ssrc);
}

const report_block_t * rtcp_RR_get_report_block(const mblk_t *m,int idx){
	rtcp_rr_t *rr=(rtcp_rr_t*)m->b_rptr;
	report_block_t *rb=&rr->rb[idx];
	size_t size=rtcp_get_size(m);
	if ( ( (uint8_t*)rb)+sizeof(report_block_t) <= (m->b_rptr + size ) ){
		return rb;
	}else{
		if (idx<rtcp_common_header_get_rc(&rr->ch)){
			ortp_warning("RTCP packet should include a report_block_t at pos %i but has no space for it.",idx);
		}
	}
	return NULL;
}

/*SDES accessors */
bool_t rtcp_is_SDES(const mblk_t *m){
	const rtcp_common_header_t *ch=rtcp_get_common_header(m);
	if (ch && rtcp_common_header_get_packet_type(ch)==RTCP_SDES){
		if (msgdsize(m)<rtcp_get_size(m)){
			ortp_warning("Too short RTCP SDES packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

void rtcp_sdes_parse(const mblk_t *m, SdesItemFoundCallback cb, void *user_data){
	uint8_t *rptr=(uint8_t*)m->b_rptr+sizeof(rtcp_common_header_t);
	const rtcp_common_header_t *ch=(rtcp_common_header_t*)m->b_rptr;
	uint8_t *end=rptr+(4*(rtcp_common_header_get_length(ch)+1));
	uint32_t ssrc=0;
	int nchunk=0;
	bool_t chunk_start=TRUE;

	if (end>(uint8_t*)m->b_wptr) end=(uint8_t*)m->b_wptr;

	while(rptr<end){
		if (chunk_start){
			if (rptr+4<=end){
				ssrc=ntohl(*(uint32_t*)rptr);
				rptr+=4;
			}else{
				ortp_warning("incorrect chunk start in RTCP SDES");
				break;
			}
			chunk_start=FALSE;
		}else{
			if (rptr+2<=end){
				uint8_t type=rptr[0];
				uint8_t len=rptr[1];

				if (type==RTCP_SDES_END){
					/* pad to next 32bit boundary*/
					rptr=(uint8_t *)((intptr_t)(rptr+4) & ~0x3);
					nchunk++;
					if (nchunk<rtcp_common_header_get_rc(ch)){
						chunk_start=TRUE;
						continue;
					}else break;
				}
				rptr+=2;
				if (rptr+len<=end){
					cb(user_data,ssrc,type,(char*)rptr,len);
					rptr+=len;
				}else{
					ortp_warning("bad item length in RTCP SDES");
					break;
				}
			}else{
				/*end of packet */
				break;
			}
		}
	}
}

/*BYE accessors */
bool_t rtcp_is_BYE(const mblk_t *m){
	const rtcp_common_header_t *ch=rtcp_get_common_header(m);
	if (ch && rtcp_common_header_get_packet_type(ch)==RTCP_BYE){
		if (msgdsize(m)<rtcp_get_size(m)){
			ortp_warning("Too short RTCP BYE packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

bool_t rtcp_BYE_get_ssrc(const mblk_t *m, int idx, uint32_t *ssrc){
	rtcp_bye_t *bye=(rtcp_bye_t*)m->b_rptr;
	int rc=rtcp_common_header_get_rc(&bye->ch);
	if (idx<rc){
		if ((uint8_t*)&bye->ssrc[idx]<=(m->b_rptr
				+ rtcp_get_size(m)-4)) {
			*ssrc=ntohl(bye->ssrc[idx]);
			return TRUE;
		}else{
			ortp_warning("RTCP BYE should contain %i ssrc, but there is not enough room for it.",rc);
		}
	}
	return FALSE;
}

bool_t rtcp_BYE_get_reason(const mblk_t *m, const char **reason, int *reason_len){
	rtcp_bye_t *bye=(rtcp_bye_t*)m->b_rptr;
	int rc=rtcp_common_header_get_rc(&bye->ch);
	uint8_t *rptr=(uint8_t*)m->b_rptr+sizeof(rtcp_common_header_t)+rc*4;
	uint8_t *end=(uint8_t*)(m->b_rptr+rtcp_get_size(m));
	if (rptr<end){
		uint8_t content_len=rptr[0];
		if (rptr+1+content_len<=end){
			*reason=(char*)rptr+1;
			*reason_len=content_len;
			return TRUE;
		}else{
			ortp_warning("RTCP BYE has not enough space for reason phrase.");
			return FALSE;
		}
	}
	return FALSE;
}

/*APP accessors */
bool_t rtcp_is_APP(const mblk_t *m){
	const rtcp_common_header_t *ch=rtcp_get_common_header(m);
	size_t size=rtcp_get_size(m);
	if (ch!=NULL && rtcp_common_header_get_packet_type(ch)==RTCP_APP){
		if (msgdsize(m)<size){
			ortp_warning("Too short RTCP APP packet.");
			return FALSE;
		}
		if (size < sizeof(rtcp_app_t)){
			ortp_warning("Bad RTCP APP packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

int rtcp_APP_get_subtype(const mblk_t *m){
	rtcp_app_t *app=(rtcp_app_t*)m->b_rptr;
	return rtcp_common_header_get_rc(&app->ch);
}

uint32_t rtcp_APP_get_ssrc(const mblk_t *m){
	rtcp_app_t *app=(rtcp_app_t*)m->b_rptr;
	return ntohl(app->ssrc);
}
/* name argument is supposed to be at least 4 characters (note: no '\0' written)*/
void rtcp_APP_get_name(const mblk_t *m, char *name){
	rtcp_app_t *app=(rtcp_app_t*)m->b_rptr;
	memcpy(name,app->name,4);
}
/* retrieve the data. when returning, data points directly into the mblk_t */
void rtcp_APP_get_data(const mblk_t *m, uint8_t **data, int *len){
	int datalen=(int)rtcp_get_size(m)-sizeof(rtcp_app_t);
	if (datalen>0){
		*data=(uint8_t*)m->b_rptr+sizeof(rtcp_app_t);
		*len=datalen;
	}else{
		*len=0;
		*data=NULL;
	}
}


/* RTCP XR accessors */
bool_t rtcp_is_XR(const mblk_t *m) {
	const rtcp_common_header_t *ch = rtcp_get_common_header(m);
	if ((ch != NULL) && (rtcp_common_header_get_packet_type(ch) == RTCP_XR)) {
		if (msgdsize(m) < MIN_RTCP_XR_PACKET_SIZE) {
			ortp_warning("Too short RTCP XR packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

rtcp_xr_block_type_t rtcp_XR_get_block_type(const mblk_t *m) {
	rtcp_xr_generic_block_header_t *bh = (rtcp_xr_generic_block_header_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return bh->bt;
}

uint32_t rtcp_XR_get_ssrc(const mblk_t *m) {
	rtcp_xr_header_t *xh = (rtcp_xr_header_t *)m->b_rptr;
	return ntohl(xh->ssrc);
}

uint64_t rtcp_XR_rcvr_rtt_get_ntp_timestamp(const mblk_t *m) {
	uint64_t ts = 0;
	rtcp_xr_rcvr_rtt_report_block_t *b = (rtcp_xr_rcvr_rtt_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	ts = ntohl(b->ntp_timestamp_msw);
	ts <<= 32;
	ts |= ntohl(b->ntp_timestamp_lsw);
	return ts;
}

uint32_t rtcp_XR_dlrr_get_ssrc(const mblk_t *m) {
	rtcp_xr_dlrr_report_subblock_t *b = (rtcp_xr_dlrr_report_subblock_t *)(m->b_rptr + sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_generic_block_header_t));
	return ntohl(b->ssrc);
}

uint32_t rtcp_XR_dlrr_get_lrr(const mblk_t *m) {
	rtcp_xr_dlrr_report_subblock_t *b = (rtcp_xr_dlrr_report_subblock_t *)(m->b_rptr + sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_generic_block_header_t));
	return ntohl(b->lrr);
}

uint32_t rtcp_XR_dlrr_get_dlrr(const mblk_t *m) {
	rtcp_xr_dlrr_report_subblock_t *b = (rtcp_xr_dlrr_report_subblock_t *)(m->b_rptr + sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_generic_block_header_t));
	return ntohl(b->dlrr);
}

uint8_t rtcp_XR_stat_summary_get_flags(const mblk_t *m) {
	rtcp_xr_generic_block_header_t *bh = (rtcp_xr_generic_block_header_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return bh->flags;
}

uint32_t rtcp_XR_stat_summary_get_ssrc(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->ssrc);
}

uint16_t rtcp_XR_stat_summary_get_begin_seq(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->begin_seq);
}

uint16_t rtcp_XR_stat_summary_get_end_seq(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->end_seq);
}

uint32_t rtcp_XR_stat_summary_get_lost_packets(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->lost_packets);
}

uint32_t rtcp_XR_stat_summary_get_dup_packets(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->dup_packets);
}

uint32_t rtcp_XR_stat_summary_get_min_jitter(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->min_jitter);
}

uint32_t rtcp_XR_stat_summary_get_max_jitter(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->max_jitter);
}

uint32_t rtcp_XR_stat_summary_get_mean_jitter(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->mean_jitter);
}

uint32_t rtcp_XR_stat_summary_get_dev_jitter(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->dev_jitter);
}

uint8_t rtcp_XR_stat_summary_get_min_ttl_or_hl(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->min_ttl_or_hl;
}

uint8_t rtcp_XR_stat_summary_get_max_ttl_or_hl(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->max_ttl_or_hl;
}

uint8_t rtcp_XR_stat_summary_get_mean_ttl_or_hl(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->mean_ttl_or_hl;
}

uint8_t rtcp_XR_stat_summary_get_dev_ttl_or_hl(const mblk_t *m) {
	rtcp_xr_stat_summary_report_block_t *b = (rtcp_xr_stat_summary_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->dev_ttl_or_hl;
}

uint32_t rtcp_XR_voip_metrics_get_ssrc(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohl(b->ssrc);
}

uint8_t rtcp_XR_voip_metrics_get_loss_rate(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->loss_rate;
}

uint8_t rtcp_XR_voip_metrics_get_discard_rate(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->discard_rate;
}

uint8_t rtcp_XR_voip_metrics_get_burst_density(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->burst_density;
}

uint8_t rtcp_XR_voip_metrics_get_gap_density(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->gap_density;
}

uint16_t rtcp_XR_voip_metrics_get_burst_duration(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->burst_duration);
}

uint16_t rtcp_XR_voip_metrics_get_gap_duration(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->gap_duration);
}

uint16_t rtcp_XR_voip_metrics_get_round_trip_delay(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->round_trip_delay);
}

uint16_t rtcp_XR_voip_metrics_get_end_system_delay(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->end_system_delay);
}

uint8_t rtcp_XR_voip_metrics_get_signal_level(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->signal_level;
}

uint8_t rtcp_XR_voip_metrics_get_noise_level(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->noise_level;
}

uint8_t rtcp_XR_voip_metrics_get_rerl(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->rerl;
}

uint8_t rtcp_XR_voip_metrics_get_gmin(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->gmin;
}

uint8_t rtcp_XR_voip_metrics_get_r_factor(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->r_factor;
}

uint8_t rtcp_XR_voip_metrics_get_ext_r_factor(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->ext_r_factor;
}

uint8_t rtcp_XR_voip_metrics_get_mos_lq(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->mos_lq;
}

uint8_t rtcp_XR_voip_metrics_get_mos_cq(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->mos_cq;
}

uint8_t rtcp_XR_voip_metrics_get_rx_config(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return b->rx_config;
}

uint16_t rtcp_XR_voip_metrics_get_jb_nominal(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->jb_nominal);
}

uint16_t rtcp_XR_voip_metrics_get_jb_maximum(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->jb_maximum);
}

uint16_t rtcp_XR_voip_metrics_get_jb_abs_max(const mblk_t *m) {
	rtcp_xr_voip_metrics_report_block_t *b = (rtcp_xr_voip_metrics_report_block_t *)(m->b_rptr + sizeof(rtcp_xr_header_t));
	return ntohs(b->jb_abs_max);
}


/* RTCP RTPFB accessors */
bool_t rtcp_is_RTPFB(const mblk_t *m) {
	const rtcp_common_header_t *ch = rtcp_get_common_header(m);
	if ((ch != NULL) && (rtcp_common_header_get_packet_type(ch) == RTCP_RTPFB)) {
		if (msgdsize(m) < MIN_RTCP_RTPFB_PACKET_SIZE) {
			ortp_warning("Too short RTCP RTPFB packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/* Same as rtcp_is_RTPFB but not needing msgpullup. To be used internally only. */
bool_t rtcp_is_RTPFB_internal(const mblk_t *m) {
	rtcp_common_header_t *ch = (rtcp_common_header_t *)m->b_rptr;
	return (rtcp_common_header_get_packet_type(ch) == RTCP_RTPFB) ? TRUE : FALSE;
}

rtcp_rtpfb_type_t rtcp_RTPFB_get_type(const mblk_t *m) {
	rtcp_common_header_t *ch = (rtcp_common_header_t *)m->b_rptr;
	return (rtcp_rtpfb_type_t)rtcp_common_header_get_rc(ch);
}

uint32_t rtcp_RTPFB_get_packet_sender_ssrc(const mblk_t *m) {
	rtcp_fb_header_t *fbh = (rtcp_fb_header_t *)(m->b_rptr + sizeof(rtcp_common_header_t));
	return ntohl(fbh->packet_sender_ssrc);
}

uint32_t rtcp_RTPFB_get_media_source_ssrc(const mblk_t *m) {
	rtcp_fb_header_t *fbh = (rtcp_fb_header_t *)(m->b_rptr + sizeof(rtcp_common_header_t));
	return ntohl(fbh->media_source_ssrc);
}

rtcp_fb_generic_nack_fci_t * rtcp_RTPFB_generic_nack_get_fci(const mblk_t *m) {
	size_t size = sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t) + sizeof(rtcp_fb_generic_nack_fci_t);
	size_t rtcp_size = rtcp_get_size(m);
	if (size > rtcp_size) {
		return NULL;
	}
	return (rtcp_fb_generic_nack_fci_t *)(m->b_rptr + size - sizeof(rtcp_fb_generic_nack_fci_t));
}

rtcp_fb_tmmbr_fci_t * rtcp_RTPFB_tmmbr_get_fci(const mblk_t *m) {
	size_t size = sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t) + sizeof(rtcp_fb_tmmbr_fci_t);
	size_t rtcp_size = rtcp_get_size(m);
	if (size > rtcp_size) {
		return NULL;
	}
	return (rtcp_fb_tmmbr_fci_t *)(m->b_rptr + size - sizeof(rtcp_fb_tmmbr_fci_t));
}


/* RTCP PSFB accessors */
bool_t rtcp_is_PSFB(const mblk_t *m) {
	const rtcp_common_header_t *ch = rtcp_get_common_header(m);
	if ((ch != NULL) && (rtcp_common_header_get_packet_type(ch) == RTCP_PSFB)) {
		if (msgdsize(m) < MIN_RTCP_PSFB_PACKET_SIZE) {
			ortp_warning("Too short RTCP PSFB packet.");
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/* Same as rtcp_is_PSFB but not needing msgpullup. To be used internally only. */
bool_t rtcp_is_PSFB_internal(const mblk_t *m) {
	rtcp_common_header_t *ch = (rtcp_common_header_t *)m->b_rptr;
	return (rtcp_common_header_get_packet_type(ch) == RTCP_PSFB) ? TRUE : FALSE;
}

rtcp_psfb_type_t rtcp_PSFB_get_type(const mblk_t *m) {
	rtcp_common_header_t *ch = (rtcp_common_header_t *)m->b_rptr;
	return (rtcp_psfb_type_t)rtcp_common_header_get_rc(ch);
}

uint32_t rtcp_PSFB_get_packet_sender_ssrc(const mblk_t *m) {
	rtcp_fb_header_t *fbh = (rtcp_fb_header_t *)(m->b_rptr + sizeof(rtcp_common_header_t));
	return ntohl(fbh->packet_sender_ssrc);
}

uint32_t rtcp_PSFB_get_media_source_ssrc(const mblk_t *m) {
	rtcp_fb_header_t *fbh = (rtcp_fb_header_t *)(m->b_rptr + sizeof(rtcp_common_header_t));
	return ntohl(fbh->media_source_ssrc);
}

rtcp_fb_fir_fci_t * rtcp_PSFB_fir_get_fci(const mblk_t *m, unsigned int idx) {
	size_t size = sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t) + ((idx + 1) * sizeof(rtcp_fb_fir_fci_t));
	size_t rtcp_size = rtcp_get_size(m);
	if (size > rtcp_size) {
		return NULL;
	}
	return (rtcp_fb_fir_fci_t *)(m->b_rptr + size - sizeof(rtcp_fb_fir_fci_t));
}

rtcp_fb_sli_fci_t * rtcp_PSFB_sli_get_fci(const mblk_t *m, unsigned int idx) {
	size_t size = sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t) + ((idx + 1) * sizeof(rtcp_fb_sli_fci_t));
	size_t rtcp_size = rtcp_get_size(m);
	if (size > rtcp_size) {
		return NULL;
	}
	return (rtcp_fb_sli_fci_t *)(m->b_rptr + size - sizeof(rtcp_fb_sli_fci_t));
}

rtcp_fb_rpsi_fci_t * rtcp_PSFB_rpsi_get_fci(const mblk_t *m) {
	return (rtcp_fb_rpsi_fci_t *)(m->b_rptr + sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t));
}

uint16_t rtcp_PSFB_rpsi_get_fci_bit_string_len(const mblk_t *m) {
	rtcp_fb_rpsi_fci_t *fci = rtcp_PSFB_rpsi_get_fci(m);
	uint16_t bit_string_len_in_bytes = (uint16_t)(rtcp_get_size(m) - (sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t) + 2));
	return ((bit_string_len_in_bytes * 8) - fci->pb);
}
