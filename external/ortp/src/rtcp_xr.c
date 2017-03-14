/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2011-2014 Belledonne Communications

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


#include <math.h>

#include "ortp/ortp.h"
#include "ortp/rtpsession.h"
#include "ortp/rtcp.h"
#include "rtpsession_priv.h"
#include "utils.h"


static uint8_t calc_rate(double d1, double d2) {
	double rate = (d1 / d2) * 256;
	uint32_t int_rate = (uint32_t)rate;
	if (int_rate > 255) int_rate = 255;
	return (uint8_t)int_rate;
}

static int rtcp_xr_header_init(uint8_t *buf, RtpSession *session, int bytes_len) {
	rtcp_xr_header_t *header = (rtcp_xr_header_t *)buf;
	rtcp_common_header_init(&header->ch, session, RTCP_XR, 0, bytes_len);
	header->ssrc = htonl(session->snd.ssrc);
	return sizeof(rtcp_xr_header_t);
}

static int rtcp_xr_rcvr_rtt_init(uint8_t *buf, RtpSession *session) {
	struct timeval tv;
	uint64_t ntp;
	rtcp_xr_rcvr_rtt_report_block_t *block = (rtcp_xr_rcvr_rtt_report_block_t *)buf;

	block->bh.bt = RTCP_XR_RCVR_RTT;
	block->bh.flags = 0; // Reserved bits
	block->bh.length = htons(2);
	ortp_gettimeofday(&tv, NULL);
	ntp = ortp_timeval_to_ntp(&tv);
	block->ntp_timestamp_msw = htonl(ntp >> 32);
	block->ntp_timestamp_lsw = htonl(ntp & 0xFFFFFFFF);
	return sizeof(rtcp_xr_rcvr_rtt_report_block_t);
}

static int rtcp_xr_dlrr_init(uint8_t *buf, RtpSession *session) {
	uint32_t dlrr = 0;
	rtcp_xr_dlrr_report_block_t *block = (rtcp_xr_dlrr_report_block_t *)buf;

	block->bh.bt = RTCP_XR_DLRR;
	block->bh.flags = 0; // Reserved bits
	block->bh.length = htons(3);
	block->content[0].ssrc = htonl(rtp_session_get_recv_ssrc(session));
	block->content[0].lrr = htonl(session->rtcp_xr_stats.last_rcvr_rtt_ts);
	if (session->rtcp_xr_stats.last_rcvr_rtt_time.tv_sec != 0) {
		struct timeval now;
		double delay;
		ortp_gettimeofday(&now, NULL);
		delay = ((now.tv_sec - session->rtcp_xr_stats.last_rcvr_rtt_time.tv_sec)
			+ ((now.tv_usec - session->rtcp_xr_stats.last_rcvr_rtt_time.tv_usec) * 1e-6)) * 65536;
		dlrr = (uint32_t) delay;
	}
	block->content[0].dlrr = htonl(dlrr);
	return sizeof(rtcp_xr_dlrr_report_block_t);
}

static int rtcp_xr_stat_summary_init(uint8_t *buf, RtpSession *session) {
	rtcp_xr_stat_summary_report_block_t *block = (rtcp_xr_stat_summary_report_block_t *)buf;
	uint16_t last_rcv_seq = session->rtp.hwrcv_extseq & 0xFFFF;
	uint8_t flags = session->rtcp.xr_conf.stat_summary_flags;
	uint32_t expected_packets;
	uint32_t lost_packets = 0;
	uint32_t dup_packets = session->rtcp_xr_stats.dup_since_last_stat_summary;

	/* Compute lost and duplicate packets statistics */
	if (flags & OrtpRtcpXrStatSummaryLoss) {
		uint32_t no_duplicate_received = session->rtcp_xr_stats.rcv_since_last_stat_summary - dup_packets;
		expected_packets = last_rcv_seq - session->rtcp_xr_stats.rcv_seq_at_last_stat_summary;
		lost_packets = (expected_packets > session->rtcp_xr_stats.rcv_since_last_stat_summary)
			? (expected_packets - no_duplicate_received) : 0;
	}

	block->bh.bt = RTCP_XR_STAT_SUMMARY;
	block->bh.flags = flags;
	block->bh.length = htons(9);
	block->ssrc = htonl(rtp_session_get_recv_ssrc(session));
	block->begin_seq = htons(session->rtcp_xr_stats.rcv_seq_at_last_stat_summary + 1);
	block->end_seq = htons(last_rcv_seq + 1);
	block->lost_packets = htonl(lost_packets);
	block->dup_packets = htonl(dup_packets);
	if ((flags & OrtpRtcpXrStatSummaryJitt)
		&& (session->rtcp_xr_stats.rcv_since_last_stat_summary > 0)) {
		block->min_jitter = htonl(session->rtcp_xr_stats.min_jitter_since_last_stat_summary);
		block->max_jitter = htonl(session->rtcp_xr_stats.max_jitter_since_last_stat_summary);
		block->mean_jitter = htonl((session->rtcp_xr_stats.rcv_since_last_stat_summary > 1)
			? (uint32_t)session->rtcp_xr_stats.newm_jitter_since_last_stat_summary : 0);
		block->dev_jitter = htonl((session->rtcp_xr_stats.rcv_since_last_stat_summary > 2)
			? (uint32_t)sqrt(session->rtcp_xr_stats.news_jitter_since_last_stat_summary / (session->rtcp_xr_stats.rcv_since_last_stat_summary - 2)) : 0);
	} else {
		block->min_jitter = htonl(0);
		block->max_jitter = htonl(0);
		block->mean_jitter = htonl(0);
		block->dev_jitter = htonl(0);
	}
	if ((flags & (OrtpRtcpXrStatSummaryTTL | OrtpRtcpXrStatSummaryHL))
		&& (session->rtcp_xr_stats.rcv_since_last_stat_summary > 0)) {
		block->min_ttl_or_hl = session->rtcp_xr_stats.min_ttl_or_hl_since_last_stat_summary;
		block->max_ttl_or_hl = session->rtcp_xr_stats.max_ttl_or_hl_since_last_stat_summary;
		block->mean_ttl_or_hl = (session->rtcp_xr_stats.rcv_since_last_stat_summary > 0)
			? (uint8_t)session->rtcp_xr_stats.newm_ttl_or_hl_since_last_stat_summary : 0;
		block->dev_ttl_or_hl = (session->rtcp_xr_stats.rcv_since_last_stat_summary > 1)
			? (uint8_t)sqrt(session->rtcp_xr_stats.news_ttl_or_hl_since_last_stat_summary / (session->rtcp_xr_stats.rcv_since_last_stat_summary - 1)) : 0;
	} else {
		block->min_ttl_or_hl = 0;
		block->max_ttl_or_hl = 0;
		block->mean_ttl_or_hl = 0;
		block->dev_ttl_or_hl = 0;
	}

	session->rtcp_xr_stats.rcv_seq_at_last_stat_summary = last_rcv_seq;
	session->rtcp_xr_stats.rcv_since_last_stat_summary = 0;
	session->rtcp_xr_stats.dup_since_last_stat_summary = 0;

	return sizeof(rtcp_xr_stat_summary_report_block_t);
}

static int rtcp_xr_voip_metrics_init(uint8_t *buf, RtpSession *session) {
	JBParameters jbparams;
	uint32_t expected_packets;
	uint32_t lost_packets;
	rtcp_xr_voip_metrics_report_block_t *block = (rtcp_xr_voip_metrics_report_block_t *)buf;
	float rtt = rtp_session_get_round_trip_propagation(session);
	uint16_t int_rtt = (uint16_t)((rtt >= 0) ? (rtt * 1000) : 0);
	float qi = -1;
	float lq_qi = -1;

	rtp_session_get_jitter_buffer_params(session, &jbparams);
	if (session->rtcp.xr_media_callbacks.average_qi != NULL) {
		qi = session->rtcp.xr_media_callbacks.average_qi(session->rtcp.xr_media_callbacks.userdata);
	}
	if (session->rtcp.xr_media_callbacks.average_lq_qi != NULL) {
		lq_qi = session->rtcp.xr_media_callbacks.average_lq_qi(session->rtcp.xr_media_callbacks.userdata);
	}

	block->bh.bt = RTCP_XR_VOIP_METRICS;
	block->bh.flags = 0; // Reserved bits
	block->bh.length = htons(8);
	block->ssrc = htonl(rtp_session_get_recv_ssrc(session));
	block->gmin = RTCP_XR_GMIN;
	block->reserved2 = 0;

	// Fill RX config
	block->rx_config = 0;
	if (jbparams.adaptive) {
		block->rx_config |= RTCP_XR_VOIP_METRICS_CONFIG_JBA_ADA;
	} else {
		block->rx_config |= RTCP_XR_VOIP_METRICS_CONFIG_JBA_NON;
	}
	if (session->rtcp.xr_media_callbacks.plc != NULL) {
		switch (session->rtcp.xr_media_callbacks.plc(session->rtcp.xr_media_callbacks.userdata)) {
			default:
			case OrtpRtcpXrNoPlc:
				block->rx_config |= RTCP_XR_VOIP_METRICS_CONFIG_PLC_UNS;
				break;
			case OrtpRtcpXrSilencePlc:
				block->rx_config |= RTCP_XR_VOIP_METRICS_CONFIG_PLC_DIS;
				break;
			case OrtpRtcpXrEnhancedPlc:
				block->rx_config |= RTCP_XR_VOIP_METRICS_CONFIG_PLC_ENH;
				break;
		}
	} else {
		block->rx_config |= RTCP_XR_VOIP_METRICS_CONFIG_PLC_UNS;
	}

	// Fill JB fields
	block->jb_nominal = htons((uint16_t)jbparams.nom_size);
	if (jbparams.adaptive) {
		block->jb_maximum = htons((session->rtp.jittctl.adapt_jitt_comp_ts * 1000) / session->rtp.jittctl.clock_rate);
	} else {
		block->jb_maximum = block->jb_nominal;
	}
	block->jb_abs_max = htons(65535);

	if (session->rtcp_xr_stats.rcv_count > 0) {
		expected_packets = session->rtcp_xr_stats.last_rcv_seq - session->rtcp_xr_stats.first_rcv_seq + 1;
		lost_packets = expected_packets - session->rtcp_xr_stats.rcv_count;
		block->loss_rate = calc_rate((double)lost_packets, (double)expected_packets);
		block->discard_rate = calc_rate((double)session->rtcp_xr_stats.discarded_count, (double)expected_packets);
		// TODO: fill burst_density, gap_density, burst_duration, gap_duration
		block->burst_density = 0;
		block->gap_density = 0;
		block->burst_duration = htons(0);
		block->gap_duration = htons(0);
		block->round_trip_delay = htons(int_rtt);
		// TODO: fill end_system_delay
		block->end_system_delay = htons(0);
		if (session->rtcp.xr_media_callbacks.signal_level != NULL) {
			block->signal_level = session->rtcp.xr_media_callbacks.signal_level(session->rtcp.xr_media_callbacks.userdata);
		} else {
			block->signal_level = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		}
		if (session->rtcp.xr_media_callbacks.noise_level != NULL) {
			block->noise_level = session->rtcp.xr_media_callbacks.noise_level(session->rtcp.xr_media_callbacks.userdata);
		} else {
			block->noise_level = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		}
		block->rerl = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		if (qi < 0) {
			block->r_factor = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		} else {
			block->r_factor = (uint8_t)(qi * 20);
		}
		block->ext_r_factor = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		if (lq_qi < 0) {
			block->mos_lq = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		} else {
			block->mos_lq = (uint8_t)(lq_qi * 10);
			if (block->mos_lq < 10) block->mos_lq = 10;
		}
		if (qi < 0) {
			block->mos_cq = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		} else {
			block->mos_cq = (uint8_t)(qi * 10);
			if (block->mos_cq < 10) block->mos_cq = 10;
		}
	} else {
		block->loss_rate = 0;
		block->discard_rate = 0;
		block->burst_density = 0;
		block->gap_density = 0;
		block->burst_duration = htons(0);
		block->gap_duration = htons(0);
		block->round_trip_delay = htons(0);
		block->end_system_delay = htons(0);
		block->signal_level = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		block->noise_level = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		block->rerl = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		block->r_factor = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		block->ext_r_factor = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		block->mos_lq = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
		block->mos_cq = ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
	}
	return sizeof(rtcp_xr_voip_metrics_report_block_t);
}


mblk_t * make_xr_rcvr_rtt(RtpSession *session) {
	int size = sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_rcvr_rtt_report_block_t);
	mblk_t *h = allocb(size, 0);
	h->b_wptr += rtcp_xr_header_init(h->b_wptr, session, size);
	h->b_wptr += rtcp_xr_rcvr_rtt_init(h->b_wptr, session);
	return h;
}

mblk_t * make_xr_dlrr(RtpSession *session) {
	int size = sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_dlrr_report_block_t);
	mblk_t *h = allocb(size, 0);
	h->b_wptr += rtcp_xr_header_init(h->b_wptr, session, size);
	h->b_wptr += rtcp_xr_dlrr_init(h->b_wptr, session);
	return h;
}

mblk_t * make_xr_stat_summary(RtpSession *session) {
	int size = sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_stat_summary_report_block_t);
	mblk_t *h = allocb(size, 0);
	h->b_wptr += rtcp_xr_header_init(h->b_wptr, session, size);
	h->b_wptr += rtcp_xr_stat_summary_init(h->b_wptr, session);
	return h;
}

mblk_t * make_xr_voip_metrics(RtpSession *session) {
	int size = sizeof(rtcp_xr_header_t) + sizeof(rtcp_xr_voip_metrics_report_block_t);
	mblk_t *h = allocb(size, 0);
	h->b_wptr += rtcp_xr_header_init(h->b_wptr, session, size);
	h->b_wptr += rtcp_xr_voip_metrics_init(h->b_wptr, session);
	return h;
}

void rtp_session_configure_rtcp_xr(RtpSession *session, const OrtpRtcpXrConfiguration *config) {
	if (config != NULL) {
		session->rtcp.xr_conf = *config;
	}
}

void rtp_session_set_rtcp_xr_media_callbacks(RtpSession *session, const OrtpRtcpXrMediaCallbacks *cbs) {
	if (cbs != NULL) {
		memcpy(&session->rtcp.xr_media_callbacks, cbs, sizeof(session->rtcp.xr_media_callbacks));
	} else {
		memset(&session->rtcp.xr_media_callbacks, 0, sizeof(session->rtcp.xr_media_callbacks));
	}
}
