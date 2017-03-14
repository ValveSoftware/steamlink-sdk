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


#ifndef RTP_H
#define RTP_H

#include <ortp/port.h>
#include <ortp/str_utils.h>

#define IPMAXLEN 20
#define UDP_MAX_SIZE 1500
#define RTP_FIXED_HEADER_SIZE 12
#define RTP_DEFAULT_JITTER_TIME 80	/*miliseconds*/
#define RTP_DEFAULT_MULTICAST_TTL 5	/*hops*/
#define RTP_DEFAULT_MULTICAST_LOOPBACK 0  /*false*/
#define RTP_DEFAULT_DSCP 0x00  /*best effort*/



typedef struct rtp_header
{
#ifdef ORTP_BIGENDIAN
	uint16_t version:2;
	uint16_t padbit:1;
	uint16_t extbit:1;
	uint16_t cc:4;
	uint16_t markbit:1;
	uint16_t paytype:7;
#else
	uint16_t cc:4;
	uint16_t extbit:1;
	uint16_t padbit:1;
	uint16_t version:2;
	uint16_t paytype:7;
	uint16_t markbit:1;
#endif
	uint16_t seq_number;
	uint32_t timestamp;
	uint32_t ssrc;
	uint32_t csrc[16];
} rtp_header_t;




typedef struct rtp_stats
{
	uint64_t packet_sent;		/*number of outgoing packets */
	uint64_t packet_dup_sent;	/*number of outgoing duplicate packets */
	uint64_t sent;				/* outgoing total bytes (excluding IP header) */
	uint64_t packet_recv;		/* number of incoming packets */
	uint64_t packet_dup_recv;	/* number of incoming duplicate packets */
	uint64_t recv;				/* incoming bytes of payload and delivered in time to the application */
	uint64_t hw_recv;			/* incoming bytes of payload */
	uint64_t outoftime;			/* number of incoming packets that were received too late */
	int64_t  cum_packet_loss;	/* cumulative number of incoming packet lost */
	uint64_t bad;				/* incoming packets that did not appear to be RTP */
	uint64_t discarded;			/* incoming packets discarded because the queue exceeds its max size */
	uint64_t sent_rtcp_packets;	/* outgoing RTCP packets counter (only packets that embed a report block are considered) */
	uint64_t recv_rtcp_packets;	/* incoming RTCP packets counter (only packets that embed a report block are considered) */
} rtp_stats_t;


typedef struct jitter_stats
{
	uint32_t jitter;			/* interarrival jitter at last emitted sender report */
	uint32_t max_jitter;		/* biggest interarrival jitter (value in stream clock unit) */
	uint64_t sum_jitter;		/* sum of all interarrival jitter (value in stream clock unit) */
	uint64_t max_jitter_ts;		/* date (in ms since Epoch) of the biggest interarrival jitter */
	float jitter_buffer_size_ms;/* mean jitter buffer size in milliseconds.*/
} jitter_stats_t;

#define RTP_TIMESTAMP_IS_NEWER_THAN(ts1,ts2) \
	((uint32_t)((uint32_t)(ts1) - (uint32_t)(ts2))< (uint32_t)(1<<31))

#define RTP_TIMESTAMP_IS_STRICTLY_NEWER_THAN(ts1,ts2) \
	( ((uint32_t)((uint32_t)(ts1) - (uint32_t)(ts2))< (uint32_t)(1<<31)) && (ts1)!=(ts2) )

#define RTP_SEQ_IS_STRICTLY_GREATER_THAN(seq1,seq2)\
	(((uint16_t)((uint16_t)(seq1) - (uint16_t)(seq2))< (uint16_t)(1<<15)) && (seq1!=seq2))

#define TIME_IS_NEWER_THAN(t1,t2) RTP_TIMESTAMP_IS_NEWER_THAN(t1,t2)

#define TIME_IS_STRICTLY_NEWER_THAN(t1,t2) RTP_TIMESTAMP_IS_STRICTLY_NEWER_THAN(t1,t2)


#ifdef __cplusplus
extern "C"{
#endif

/* packet api */
/* the first argument is a mblk_t. The header is supposed to be not splitted  */
#define rtp_set_markbit(mp,value)		((rtp_header_t*)((mp)->b_rptr))->markbit=(value)
#define rtp_set_seqnumber(mp,seq)	((rtp_header_t*)((mp)->b_rptr))->seq_number=(seq)
#define rtp_set_timestamp(mp,ts)	((rtp_header_t*)((mp)->b_rptr))->timestamp=(ts)
#define rtp_set_ssrc(mp,_ssrc)		((rtp_header_t*)((mp)->b_rptr))->ssrc=(_ssrc)
ORTP_PUBLIC void rtp_add_csrc(mblk_t *mp ,uint32_t csrc);
#define rtp_set_payload_type(mp,pt)	((rtp_header_t*)((mp)->b_rptr))->paytype=(pt)

#define rtp_get_version(mp)	(((rtp_header_t*)((mp)->b_rptr))->version)
#define rtp_get_markbit(mp)	(((rtp_header_t*)((mp)->b_rptr))->markbit)
#define rtp_get_extbit(mp)	(((rtp_header_t*)((mp)->b_rptr))->extbit)
#define rtp_get_timestamp(mp)	(((rtp_header_t*)((mp)->b_rptr))->timestamp)
#define rtp_get_seqnumber(mp)	(((rtp_header_t*)((mp)->b_rptr))->seq_number)
#define rtp_get_payload_type(mp)	(((rtp_header_t*)((mp)->b_rptr))->paytype)
#define rtp_get_ssrc(mp)		(((rtp_header_t*)((mp)->b_rptr))->ssrc)
#define rtp_get_cc(mp)		(((rtp_header_t*)((mp)->b_rptr))->cc)
#define rtp_get_csrc(mp, idx)		(((rtp_header_t*)((mp)->b_rptr))->csrc[idx])

ORTP_PUBLIC int rtp_get_payload(mblk_t *packet, unsigned char **start);
ORTP_PUBLIC int rtp_get_extheader(mblk_t *packet, uint16_t *profile, uint8_t **start_ext);

#ifdef __cplusplus
}
#endif

#endif
