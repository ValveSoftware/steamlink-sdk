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


#ifndef RTCP_H
#define RTCP_H

#include <ortp/port.h>

#define RTCP_MAX_RECV_BUFSIZE 1500

#define RTCP_SENDER_INFO_SIZE 20
#define RTCP_REPORT_BLOCK_SIZE 24
#define RTCP_COMMON_HEADER_SIZE 4
#define RTCP_SSRC_FIELD_SIZE 4

#ifdef __cplusplus
extern "C"{
#endif

/* RTCP common header */

typedef enum {
	RTCP_SR = 200,
	RTCP_RR = 201,
	RTCP_SDES = 202,
	RTCP_BYE = 203,
	RTCP_APP = 204,
	RTCP_RTPFB = 205,
	RTCP_PSFB = 206,
	RTCP_XR = 207
} rtcp_type_t;


typedef struct rtcp_common_header
{
#ifdef ORTP_BIGENDIAN
	uint16_t version:2;
	uint16_t padbit:1;
	uint16_t rc:5;
	uint16_t packet_type:8;
#else
	uint16_t rc:5;
	uint16_t padbit:1;
	uint16_t version:2;
	uint16_t packet_type:8;
#endif
	uint16_t length:16;
} rtcp_common_header_t;

#define rtcp_common_header_set_version(ch,v) (ch)->version=v
#define rtcp_common_header_set_padbit(ch,p) (ch)->padbit=p
#define rtcp_common_header_set_rc(ch,rc) (ch)->rc=rc
#define rtcp_common_header_set_packet_type(ch,pt) (ch)->packet_type=pt
#define rtcp_common_header_set_length(ch,l)	(ch)->length=htons(l)

#define rtcp_common_header_get_version(ch) ((ch)->version)
#define rtcp_common_header_get_padbit(ch) ((ch)->padbit)
#define rtcp_common_header_get_rc(ch) ((ch)->rc)
#define rtcp_common_header_get_packet_type(ch) ((ch)->packet_type)
#define rtcp_common_header_get_length(ch)	ntohs((ch)->length)


/* SR or RR  packets */

typedef struct sender_info
{
	uint32_t ntp_timestamp_msw;
	uint32_t ntp_timestamp_lsw;
	uint32_t rtp_timestamp;
	uint32_t senders_packet_count;
	uint32_t senders_octet_count;
} sender_info_t;

static ORTP_INLINE uint64_t sender_info_get_ntp_timestamp(const sender_info_t *si) {
  return ((((uint64_t)ntohl(si->ntp_timestamp_msw)) << 32) +
          ((uint64_t) ntohl(si->ntp_timestamp_lsw)));
}
#define sender_info_get_rtp_timestamp(si)	ntohl((si)->rtp_timestamp)
#define sender_info_get_packet_count(si) \
	ntohl((si)->senders_packet_count)
#define sender_info_get_octet_count(si) \
	ntohl((si)->senders_octet_count)


typedef struct report_block
{
	uint32_t ssrc;
	uint32_t fl_cnpl;/*fraction lost + cumulative number of packet lost*/
	uint32_t ext_high_seq_num_rec; /*extended highest sequence number received */
	uint32_t interarrival_jitter;
	uint32_t lsr; /*last SR */
	uint32_t delay_snc_last_sr; /*delay since last sr*/
} report_block_t;

static ORTP_INLINE uint32_t report_block_get_ssrc(const report_block_t * rb) {
	return ntohl(rb->ssrc);
}
static ORTP_INLINE uint32_t report_block_get_high_ext_seq(const report_block_t * rb) {
	return ntohl(rb->ext_high_seq_num_rec);
}
static ORTP_INLINE uint32_t report_block_get_interarrival_jitter(const report_block_t * rb) {
	return ntohl(rb->interarrival_jitter);
}

static ORTP_INLINE uint32_t report_block_get_last_SR_time(const report_block_t * rb) {
	return ntohl(rb->lsr);
}
static ORTP_INLINE uint32_t report_block_get_last_SR_delay(const report_block_t * rb) {
	return ntohl(rb->delay_snc_last_sr);
}
static ORTP_INLINE uint32_t report_block_get_fraction_lost(const report_block_t * rb) {
	return (ntohl(rb->fl_cnpl)>>24);
}
static ORTP_INLINE int32_t report_block_get_cum_packet_lost(const report_block_t * rb){
	int cum_loss = ntohl(rb->fl_cnpl);
	if (((cum_loss>>23)&1)==0)
		return 0x00FFFFFF & cum_loss;
	else
		return 0xFF000000 | (cum_loss-0xFFFFFF-1);
}

static ORTP_INLINE void report_block_set_fraction_lost(report_block_t * rb, int fl){
	rb->fl_cnpl = htonl( (ntohl(rb->fl_cnpl) & 0xFFFFFF) | (fl&0xFF)<<24);
}

static ORTP_INLINE void report_block_set_cum_packet_lost(report_block_t * rb, int64_t cpl) {
	uint32_t clamp = (uint32_t)((1<<24) + ((cpl>=0) ? (cpl>0x7FFFFF?0x7FFFFF:cpl) : (-cpl>0x800000?-0x800000:cpl)));

	rb->fl_cnpl=htonl(
			(ntohl(rb->fl_cnpl) & 0xFF000000) |
			(cpl >= 0 ? clamp&0x7FFFFF : clamp|0x800000)
		);
}

/* SDES packets */

typedef enum {
	RTCP_SDES_END = 0,
	RTCP_SDES_CNAME = 1,
	RTCP_SDES_NAME = 2,
	RTCP_SDES_EMAIL = 3,
	RTCP_SDES_PHONE = 4,
	RTCP_SDES_LOC = 5,
	RTCP_SDES_TOOL = 6,
	RTCP_SDES_NOTE = 7,
	RTCP_SDES_PRIV = 8,
	RTCP_SDES_MAX = 9
} rtcp_sdes_type_t;

typedef struct sdes_chunk
{
	uint32_t csrc;
} sdes_chunk_t;


#define sdes_chunk_get_csrc(c)	ntohl((c)->csrc)

typedef struct sdes_item
{
	uint8_t item_type;
	uint8_t len;
	char content[1];
} sdes_item_t;

#define RTCP_SDES_MAX_STRING_SIZE 255
#define RTCP_SDES_ITEM_HEADER_SIZE 2
#define RTCP_SDES_CHUNK_DEFAULT_SIZE 1024
#define RTCP_SDES_CHUNK_HEADER_SIZE (sizeof(sdes_chunk_t))

/* RTCP bye packet */

typedef struct rtcp_bye_reason
{
	uint8_t len;
	char content[1];
} rtcp_bye_reason_t;

typedef struct rtcp_bye
{
	rtcp_common_header_t ch;
	uint32_t ssrc[1];  /* the bye may contain several ssrc/csrc */
} rtcp_bye_t;
#define RTCP_BYE_HEADER_SIZE sizeof(rtcp_bye_t)
#define RTCP_BYE_REASON_MAX_STRING_SIZE 255


/* RTCP XR packet */

#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_STD ((1 << 7) | (1 << 6))
#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_ENH (1 << 7)
#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_DIS (1 << 6)
#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_UNS 0
#define RTCP_XR_VOIP_METRICS_CONFIG_JBA_ADA ((1 << 5) | (1 << 4))
#define RTCP_XR_VOIP_METRICS_CONFIG_JBA_NON (1 << 5)
#define RTCP_XR_VOIP_METRICS_CONFIG_JBA_UNK 0

typedef enum {
	RTCP_XR_LOSS_RLE = 1,
	RTCP_XR_DUPLICATE_RLE = 2,
	RTCP_XR_PACKET_RECEIPT_TIMES = 3,
	RTCP_XR_RCVR_RTT = 4,
	RTCP_XR_DLRR = 5,
	RTCP_XR_STAT_SUMMARY = 6,
	RTCP_XR_VOIP_METRICS = 7
} rtcp_xr_block_type_t;

typedef struct rtcp_xr_header {
	rtcp_common_header_t ch;
	uint32_t ssrc;
} rtcp_xr_header_t;

typedef struct rtcp_xr_generic_block_header {
	uint8_t bt;
	uint8_t flags;
	uint16_t length;
} rtcp_xr_generic_block_header_t;

typedef struct rtcp_xr_rcvr_rtt_report_block {
	rtcp_xr_generic_block_header_t bh;
	uint32_t ntp_timestamp_msw;
	uint32_t ntp_timestamp_lsw;
} rtcp_xr_rcvr_rtt_report_block_t;

typedef struct rtcp_xr_dlrr_report_subblock {
	uint32_t ssrc;
	uint32_t lrr;
	uint32_t dlrr;
} rtcp_xr_dlrr_report_subblock_t;

typedef struct rtcp_xr_dlrr_report_block {
	rtcp_xr_generic_block_header_t bh;
	rtcp_xr_dlrr_report_subblock_t content[1];
} rtcp_xr_dlrr_report_block_t;

typedef struct rtcp_xr_stat_summary_report_block {
	rtcp_xr_generic_block_header_t bh;
	uint32_t ssrc;
	uint16_t begin_seq;
	uint16_t end_seq;
	uint32_t lost_packets;
	uint32_t dup_packets;
	uint32_t min_jitter;
	uint32_t max_jitter;
	uint32_t mean_jitter;
	uint32_t dev_jitter;
	uint8_t min_ttl_or_hl;
	uint8_t max_ttl_or_hl;
	uint8_t mean_ttl_or_hl;
	uint8_t dev_ttl_or_hl;
} rtcp_xr_stat_summary_report_block_t;

typedef struct rtcp_xr_voip_metrics_report_block {
	rtcp_xr_generic_block_header_t bh;
	uint32_t ssrc;
	uint8_t loss_rate;
	uint8_t discard_rate;
	uint8_t burst_density;
	uint8_t gap_density;
	uint16_t burst_duration;
	uint16_t gap_duration;
	uint16_t round_trip_delay;
	uint16_t end_system_delay;
	uint8_t signal_level;
	uint8_t noise_level;
	uint8_t rerl;
	uint8_t gmin;
	uint8_t r_factor;
	uint8_t ext_r_factor;
	uint8_t mos_lq;
	uint8_t mos_cq;
	uint8_t rx_config;
	uint8_t reserved2;
	uint16_t jb_nominal;
	uint16_t jb_maximum;
	uint16_t jb_abs_max;
} rtcp_xr_voip_metrics_report_block_t;

#define MIN_RTCP_XR_PACKET_SIZE (sizeof(rtcp_xr_header_t) + 4)

typedef enum {
	RTCP_RTPFB_NACK = 1,
	RTCP_RTPFB_TMMBR = 3,
	RTCP_RTPFB_TMMBN = 4
} rtcp_rtpfb_type_t;

typedef enum {
	RTCP_PSFB_PLI = 1,
	RTCP_PSFB_SLI = 2,
	RTCP_PSFB_RPSI = 3,
	RTCP_PSFB_FIR = 4,
	RTCP_PSFB_AFB = 15
} rtcp_psfb_type_t;

typedef struct rtcp_fb_header {
	uint32_t packet_sender_ssrc;
	uint32_t media_source_ssrc;
} rtcp_fb_header_t;

typedef struct rtcp_fb_generic_nack_fci {
	uint16_t pid;
	uint16_t blp;
} rtcp_fb_generic_nack_fci_t;

#define rtcp_fb_generic_nack_fci_get_pid(nack) ntohs((nack)->pid)
#define rtcp_fb_generic_nack_fci_set_pid(nack, value) ((nack)->pid) = htons(value)
#define rtcp_fb_generic_nack_fci_get_blp(nack) ntohs((nack)->blp)
#define rtcp_fb_generic_nack_fci_set_blp(nack, value) ((nack)->blp) = htons(value)

typedef struct rtcp_fb_tmmbr_fci {
	uint32_t ssrc;
	uint32_t value;
} rtcp_fb_tmmbr_fci_t;

#define rtcp_fb_tmmbr_fci_get_ssrc(tmmbr) ntohl((tmmbr)->ssrc)
#define rtcp_fb_tmmbr_fci_get_mxtbr_exp(tmmbr) \
	((uint8_t)((ntohl((tmmbr)->value) >> 26) & 0x0000003F))
#define rtcp_fb_tmmbr_fci_set_mxtbr_exp(tmmbr, mxtbr_exp) \
	((tmmbr)->value) = htonl((ntohl((tmmbr)->value) & 0x03FFFFFF) | (((mxtbr_exp) & 0x0000003F) << 26))
#define rtcp_fb_tmmbr_fci_get_mxtbr_mantissa(tmmbr) \
	((uint32_t)((ntohl((tmmbr)->value) >> 9) & 0x0001FFFF))
#define rtcp_fb_tmmbr_fci_set_mxtbr_mantissa(tmmbr, mxtbr_mantissa) \
	((tmmbr)->value) = htonl((ntohl((tmmbr)->value) & 0xFC0001FF) | (((mxtbr_mantissa) & 0x0001FFFF) << 9))
#define rtcp_fb_tmmbr_fci_get_measured_overhead(tmmbr) \
	((uint16_t)(ntohl((tmmbr)->value) & 0x000001FF))
#define rtcp_fb_tmmbr_fci_set_measured_overhead(tmmbr, measured_overhead) \
	((tmmbr)->value) = htonl((ntohl((tmmbr)->value) & 0xFFFFFE00) | ((measured_overhead) & 0x000001FF))

typedef struct rtcp_fb_fir_fci {
	uint32_t ssrc;
	uint8_t seq_nr;
	uint8_t pad1;
	uint16_t pad2;
} rtcp_fb_fir_fci_t;

#define rtcp_fb_fir_fci_get_ssrc(fci) ntohl((fci)->ssrc)
#define rtcp_fb_fir_fci_get_seq_nr(fci) (fci)->seq_nr

typedef struct rtcp_fb_sli_fci {
	uint32_t value;
} rtcp_fb_sli_fci_t;

#define rtcp_fb_sli_fci_get_first(fci) \
	((uint16_t)((ntohl((fci)->value) >> 19) & 0x00001FFF))
#define rtcp_fb_sli_fci_set_first(fci, first) \
	((fci)->value) = htonl((ntohl((fci)->value) & 0x0007FFFF) | (((first) & 0x00001FFF) << 19))
#define rtcp_fb_sli_fci_get_number(fci) \
	((uint16_t)((ntohl((fci)->value) >> 6) & 0x00001FFF))
#define rtcp_fb_sli_fci_set_number(fci, number) \
	((fci)->value) = htonl((ntohl((fci)->value) & 0xFFF8003F) | (((number) & 0x00001FFF) << 6))
#define rtcp_fb_sli_fci_get_picture_id(fci) \
	((uint8_t)(ntohl((fci)->value) & 0x0000003F))
#define rtcp_fb_sli_fci_set_picture_id(fci, picture_id) \
	((fci)->value) = htonl((ntohl((fci)->value) & 0xFFFFFFC0) | ((picture_id) & 0x0000003F))

typedef struct rtcp_fb_rpsi_fci {
	uint8_t pb;
	uint8_t payload_type;
	uint16_t bit_string[1];
} rtcp_fb_rpsi_fci_t;

#define rtcp_fb_rpsi_fci_get_payload_type(fci) (fci)->payload_type
#define rtcp_fb_rpsi_fci_get_bit_string(fci) ((uint8_t *)(fci)->bit_string)

#define MIN_RTCP_PSFB_PACKET_SIZE (sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t))
#define MIN_RTCP_RTPFB_PACKET_SIZE (sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t))



typedef struct rtcp_sr{
	rtcp_common_header_t ch;
	uint32_t ssrc;
	sender_info_t si;
	report_block_t rb[1];
} rtcp_sr_t;

typedef struct rtcp_rr{
	rtcp_common_header_t ch;
	uint32_t ssrc;
	report_block_t rb[1];
} rtcp_rr_t;

typedef struct rtcp_app{
	rtcp_common_header_t ch;
	uint32_t ssrc;
	char name[4];
} rtcp_app_t;

struct _RtpSession;
struct _RtpStream;
ORTP_PUBLIC void rtp_session_rtcp_process_send(struct _RtpSession *s);
ORTP_PUBLIC void rtp_session_rtcp_process_recv(struct _RtpSession *s);


#define RTCP_DEFAULT_REPORT_INTERVAL 5000 /* in milliseconds */


/* packet parsing api */

/*in case of coumpound packet, set read pointer of m to the beginning of the next RTCP
packet */
ORTP_PUBLIC bool_t rtcp_next_packet(mblk_t *m);
/* put the read pointer at the first RTCP packet of the compound packet (as before any previous calls ot rtcp_next_packet() */
ORTP_PUBLIC void rtcp_rewind(mblk_t *m);
/* get common header*/
ORTP_PUBLIC const rtcp_common_header_t * rtcp_get_common_header(const mblk_t *m);

/*Sender Report accessors */
/* check if this packet is a SR and if it is correct */
ORTP_PUBLIC bool_t rtcp_is_SR(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_SR_get_ssrc(const mblk_t *m);
ORTP_PUBLIC const sender_info_t * rtcp_SR_get_sender_info(const mblk_t *m);
ORTP_PUBLIC const report_block_t * rtcp_SR_get_report_block(const mblk_t *m, int idx);

/*Receiver report accessors*/
ORTP_PUBLIC bool_t rtcp_is_RR(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_RR_get_ssrc(const mblk_t *m);
ORTP_PUBLIC const report_block_t * rtcp_RR_get_report_block(const mblk_t *m,int idx);

/*SDES accessors */
ORTP_PUBLIC bool_t rtcp_is_SDES(const mblk_t *m);
typedef void (*SdesItemFoundCallback)(void *user_data, uint32_t csrc, rtcp_sdes_type_t t, const char *content, uint8_t content_len);
ORTP_PUBLIC void rtcp_sdes_parse(const mblk_t *m, SdesItemFoundCallback cb, void *user_data);

/*BYE accessors */
ORTP_PUBLIC bool_t rtcp_is_BYE(const mblk_t *m);
ORTP_PUBLIC bool_t rtcp_BYE_get_ssrc(const mblk_t *m, int idx, uint32_t *ssrc);
ORTP_PUBLIC bool_t rtcp_BYE_get_reason(const mblk_t *m, const char **reason, int *reason_len);

/*APP accessors */
ORTP_PUBLIC bool_t rtcp_is_APP(const mblk_t *m);
ORTP_PUBLIC int rtcp_APP_get_subtype(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_APP_get_ssrc(const mblk_t *m);
/* name argument is supposed to be at least 4 characters (note: no '\0' written)*/
ORTP_PUBLIC void rtcp_APP_get_name(const mblk_t *m, char *name);
/* retrieve the data. when returning, data points directly into the mblk_t */
ORTP_PUBLIC void rtcp_APP_get_data(const mblk_t *m, uint8_t **data, int *len);

/* RTCP XR accessors */
ORTP_PUBLIC bool_t rtcp_is_XR(const mblk_t *m);
ORTP_PUBLIC rtcp_xr_block_type_t rtcp_XR_get_block_type(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_get_ssrc(const mblk_t *m);
ORTP_PUBLIC uint64_t rtcp_XR_rcvr_rtt_get_ntp_timestamp(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_dlrr_get_ssrc(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_dlrr_get_lrr(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_dlrr_get_dlrr(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_stat_summary_get_flags(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_stat_summary_get_ssrc(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_stat_summary_get_begin_seq(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_stat_summary_get_end_seq(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_stat_summary_get_lost_packets(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_stat_summary_get_dup_packets(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_stat_summary_get_min_jitter(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_stat_summary_get_max_jitter(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_stat_summary_get_mean_jitter(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_stat_summary_get_dev_jitter(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_stat_summary_get_min_ttl_or_hl(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_stat_summary_get_max_ttl_or_hl(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_stat_summary_get_mean_ttl_or_hl(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_stat_summary_get_dev_ttl_or_hl(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_XR_voip_metrics_get_ssrc(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_loss_rate(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_discard_rate(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_burst_density(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_gap_density(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_voip_metrics_get_burst_duration(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_voip_metrics_get_gap_duration(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_voip_metrics_get_round_trip_delay(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_voip_metrics_get_end_system_delay(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_signal_level(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_noise_level(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_rerl(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_gmin(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_r_factor(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_ext_r_factor(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_mos_lq(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_mos_cq(const mblk_t *m);
ORTP_PUBLIC uint8_t rtcp_XR_voip_metrics_get_rx_config(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_voip_metrics_get_jb_nominal(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_voip_metrics_get_jb_maximum(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_XR_voip_metrics_get_jb_abs_max(const mblk_t *m);

/* RTCP RTPFB accessors */
ORTP_PUBLIC bool_t rtcp_is_RTPFB(const mblk_t *m);
ORTP_PUBLIC rtcp_rtpfb_type_t rtcp_RTPFB_get_type(const mblk_t *m);
ORTP_PUBLIC rtcp_fb_generic_nack_fci_t * rtcp_RTPFB_generic_nack_get_fci(const mblk_t *m);
ORTP_PUBLIC rtcp_fb_tmmbr_fci_t * rtcp_RTPFB_tmmbr_get_fci(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_RTPFB_get_packet_sender_ssrc(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_RTPFB_get_media_source_ssrc(const mblk_t *m);

/* RTCP PSFB accessors */
ORTP_PUBLIC bool_t rtcp_is_PSFB(const mblk_t *m);
ORTP_PUBLIC rtcp_psfb_type_t rtcp_PSFB_get_type(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_PSFB_get_packet_sender_ssrc(const mblk_t *m);
ORTP_PUBLIC uint32_t rtcp_PSFB_get_media_source_ssrc(const mblk_t *m);
ORTP_PUBLIC rtcp_fb_fir_fci_t * rtcp_PSFB_fir_get_fci(const mblk_t *m, unsigned int idx);
ORTP_PUBLIC rtcp_fb_sli_fci_t * rtcp_PSFB_sli_get_fci(const mblk_t *m, unsigned int idx);
ORTP_PUBLIC rtcp_fb_rpsi_fci_t * rtcp_PSFB_rpsi_get_fci(const mblk_t *m);
ORTP_PUBLIC uint16_t rtcp_PSFB_rpsi_get_fci_bit_string_len(const mblk_t *m);


typedef struct OrtpLossRateEstimator{
	int min_packet_count_interval;
	uint64_t min_time_ms_interval;
	uint64_t last_estimate_time_ms;
	int32_t last_cum_loss;
	int32_t last_ext_seq;
	float loss_rate;
	/**
	* Total number of outgoing duplicate packets on last
	* ortp_loss_rate_estimator_process_report_block iteration.
	**/
	int64_t last_dup_packet_sent_count;
	/**
	* Total number of outgoing unique packets on last
	* ortp_loss_rate_estimator_process_report_block iteration.
	**/
	int64_t last_packet_sent_count;
}OrtpLossRateEstimator;


ORTP_PUBLIC OrtpLossRateEstimator * ortp_loss_rate_estimator_new(int min_packet_count_interval, uint64_t min_time_ms_interval, struct _RtpSession *session);

ORTP_PUBLIC void ortp_loss_rate_estimator_init(OrtpLossRateEstimator *obj, int min_packet_count_interval, uint64_t min_time_ms_interval, struct _RtpSession *session);


/**
 * Process an incoming report block to compute loss rate percentage. It tries to compute
 * loss rate, depending on the previous report block. It may fails if the two
 * reports are too close or if a discontinuity occurred. You should NOT use
 * loss rate field of the report block directly (see below).
 * This estimator is useful for two reasons: first, on AVPF session, multiple
 * reports can be received in a short period and loss_rate contained in these
 * reports is unreliable. Secondly, it computes the loss rate using the
 * cumulative loss factor which allows us to take into consideration duplicates
 * packets as well.
 * @param[in] obj #OrtpLossRateEstimator object.
 * @param[in] session #_RtpSession stream in which the report block to consider belongs.
 * @param[in] rb Report block to analyze.
 * @return TRUE if a new loss rate estimation is ready, FALSE otherwise.
 */
ORTP_PUBLIC bool_t ortp_loss_rate_estimator_process_report_block(OrtpLossRateEstimator *obj,
																 const struct _RtpSession *session,
																 const report_block_t *rb);
/**
 * Get the latest loss rate in percentage estimation computed.
 *
 * @param obj #OrtpLossRateEstimator object.
 * @return The latest loss rate in percentage computed.
 */
ORTP_PUBLIC float ortp_loss_rate_estimator_get_value(OrtpLossRateEstimator *obj);

ORTP_PUBLIC void ortp_loss_rate_estimator_uninit(OrtpLossRateEstimator *obj);

ORTP_PUBLIC void ortp_loss_rate_estimator_destroy(OrtpLossRateEstimator *obj);

#ifdef __cplusplus
}
#endif

#endif
