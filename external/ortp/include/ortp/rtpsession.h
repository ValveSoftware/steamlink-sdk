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

/**
 * \file rtpsession.h
 * \brief The RtpSession api
 *
 * The RtpSession objects represent a RTP session: once it is configured with
 * local and remote network addresses and a payload type is given, it let you send
 * and recv a media stream.
**/


#ifndef RTPSESSION_H
#define RTPSESSION_H


#include <bctoolbox/list.h>

#include <ortp/port.h>
#include <ortp/rtp.h>
#include <ortp/payloadtype.h>
#include <ortp/rtpprofile.h>
#include <ortp/sessionset.h>
#include <ortp/rtcp.h>
#include <ortp/str_utils.h>
#include <ortp/rtpsignaltable.h>
#include <ortp/event.h>


#define ORTP_AVPF_FEATURE_NONE 0
#define ORTP_AVPF_FEATURE_TMMBR (1 << 0)
#define ORTP_AVPF_FEATURE_GENERIC_NACK (1 << 1)


typedef enum {
	RTP_SESSION_RECVONLY,
	RTP_SESSION_SENDONLY,
	RTP_SESSION_SENDRECV
} RtpSessionMode;


/*! Jitter buffer parameters
*/
typedef struct _JBParameters{
	int min_size; /**< in milliseconds*/
	int nom_size; /**< idem */
	int max_size; /**< idem */
	bool_t adaptive;
	bool_t pad[3];
	int max_packets; /**< max number of packets allowed to be queued in the jitter buffer */
} JBParameters;

typedef struct _JitterControl
{
	unsigned int count;
	int jitt_comp;   /* the user jitt_comp in miliseconds*/
	int jitt_comp_ts; /* the jitt_comp converted in rtp time (same unit as timestamp) */
	int adapt_jitt_comp_ts;
	int64_t slide;
	int64_t prev_slide;
	int64_t olddiff;
	float jitter;
	float inter_jitter;	/* interarrival jitter as defined in the RFC */
	int corrective_step;
	int corrective_slide;
	uint64_t cum_jitter_buffer_size; /*in timestamp units*/
	unsigned int cum_jitter_buffer_count; /*used for computation of jitter buffer size*/
	int clock_rate;
	bool_t adaptive;
	bool_t enabled;
} JitterControl;

typedef struct _WaitPoint
{
	ortp_mutex_t lock;
	ortp_cond_t  cond;
	uint32_t time;
	bool_t wakeup;
} WaitPoint;

typedef struct _RtpTransportModifier
{
	void *data;
	struct _RtpSession *session;//<back pointer to the owning session, set by oRTP
	struct _RtpTransport *transport;//<back point to the owning transport, set by oRTP
	int  (*t_process_on_send)(struct _RtpTransportModifier *t, mblk_t *msg);
	int  (*t_process_on_receive)(struct _RtpTransportModifier *t, mblk_t *msg);
	void  (*t_process_on_schedule)(struct _RtpTransportModifier *t); /*invoked each time rtp_session_recvm is called even is no message are available*/
	/**
	 * Mandatory callback responsible of freeing the #_RtpTransportModifier AND the pointer.
	 * @param[in] transport #_RtpTransportModifier object to free.
	 */
	void  (*t_destroy)(struct _RtpTransportModifier *transport);
} RtpTransportModifier;

typedef struct _RtpTransport
{
	void *data;
	struct _RtpSession *session;//<back pointer to the owning session, set by oRTP
	ortp_socket_t (*t_getsocket)(struct _RtpTransport *t);
	int  (*t_sendto)(struct _RtpTransport *t, mblk_t *msg , int flags, const struct sockaddr *to, socklen_t tolen);
	int  (*t_recvfrom)(struct _RtpTransport *t, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen);
	void  (*t_close)(struct _RtpTransport *transport);
	/**
	 * Mandatory callback responsible of freeing the #_RtpTransport object AND the pointer.
	 * @param[in] transport #_RtpTransport object to free.
	 */
	void  (*t_destroy)(struct _RtpTransport *transport);
}  RtpTransport;

typedef enum _OrtpNetworkSimulatorMode{
	OrtpNetworkSimulatorInvalid=-1,
	OrtpNetworkSimulatorInbound,/**<simulation is applied when receiving packets*/
	OrtpNetworkSimulatorOutbound, /**<simulation is applied to sent packets*/
	OrtpNetworkSimulatorOutboundControlled /**<simulation is applied to sent packets according to sent timestamp
				set in the timestamp field of mblk_t, which is definied only with -DORTP_TIMESTAMP */
}OrtpNetworkSimulatorMode;

/**
 * Structure describing the network simulator parameters
**/
typedef struct _OrtpNetworkSimulatorParams{
	int enabled; /**<Whether simulation is enabled or off.*/
	float max_bandwidth; /**<IP bandwidth, in bit/s*/
	int max_buffer_size; /**<Max number of bit buffered before being discarded*/
	float loss_rate; /**<Percentage of lost packets*/
	uint32_t latency; /**<Packet transmission delay, in ms*/
	float consecutive_loss_probability;/**< a probablity of having a subsequent loss after a loss occured, in a 0-1 range. Useful to simulate burst of lost packets*/
	float jitter_burst_density; /**<density of gap/bursts events. A value of 1 means one gap/burst per second approximately*/
	float jitter_strength; /**<percentage of max_bandwidth artifically consumed during bursts events*/
	bool_t RTP_only; /**True for only RTP packet loss, False for both RTP and RTCP */
	OrtpNetworkSimulatorMode mode; /**<whether simulation is applied to inboud or outbound stream.*/
}OrtpNetworkSimulatorParams;

typedef struct _OrtpNetworkSimulatorCtx{
	OrtpNetworkSimulatorParams params;
	int bit_budget;
	int qsize;
	queue_t q;/*queue used for simulating bandwidth limit*/
	queue_t latency_q;
	queue_t send_q; /*used only for OrtpNetworkSimulatorOutbound direction*/
	struct timeval last_check;
	uint64_t last_jitter_event;
	int consecutive_drops;
	int drops_to_ignore;
	int drop_by_congestion;
	int drop_by_loss;
	int total_count; /*total number of packets gone through the simulator*/
	ortp_mutex_t mutex;
	ortp_thread_t thread;
	bool_t in_jitter_event;
	bool_t thread_started;
}OrtpNetworkSimulatorCtx;

typedef struct OrtpRtcpSendAlgorithm {
	uint64_t tn; /* Time of the next scheduled RTCP RR transmission in milliseconds. */
	uint64_t tp; /* Time of the last scheduled RTCP RR transmission in milliseconds. */
	uint64_t t_rr_last; /* Time of the last regular RTCP packet sent in milliseconds. */
	uint32_t T_rr; /* Interval for the scheduling of the next regular RTCP packet. */
	uint32_t T_max_fb_delay; /* Interval within which a feeback message is considered to be useful to the sender. */
	uint32_t T_rr_interval; /* Minimal interval to be used between regular RTCP packets. */
	uint32_t T_rr_current_interval;
	uint32_t Tmin; /* Minimal interval between RTCP packets. */
	float avg_rtcp_size;
	mblk_t *fb_packets;
	bool_t initialized; /* Whether the RTCP send algorithm is fully initialized. */
	bool_t initial;
	bool_t allow_early;
	bool_t tmmbr_scheduled;
	bool_t tmmbn_scheduled;
} OrtpRtcpSendAlgorithm;

typedef struct OrtpRtcpFbConfiguration {
	bool_t generic_nack_enabled;
	bool_t tmmbr_enabled;
} OrtpRtcpFbConfiguration;

#define ORTP_RTCP_XR_UNAVAILABLE_PARAMETER 127

typedef enum {
	OrtpRtcpXrNoPlc,
	OrtpRtcpXrSilencePlc,
	OrtpRtcpXrEnhancedPlc
} OrtpRtcpXrPlcStatus;

typedef OrtpRtcpXrPlcStatus (*OrtpRtcpXrPlcCallback)(void *userdata);
typedef int (*OrtpRtcpXrSignalLevelCallback)(void *userdata);
typedef int (*OrtpRtcpXrNoiseLevelCallback)(void *userdata);
typedef float (*OrtpRtcpXrAverageQualityIndicatorCallback)(void *userdata);

typedef struct OrtpRtcpXrMediaCallbacks {
	OrtpRtcpXrPlcCallback plc;
	OrtpRtcpXrSignalLevelCallback signal_level;
	OrtpRtcpXrNoiseLevelCallback noise_level;
	OrtpRtcpXrAverageQualityIndicatorCallback average_qi;
	OrtpRtcpXrAverageQualityIndicatorCallback average_lq_qi;
	void *userdata;
} OrtpRtcpXrMediaCallbacks;

typedef enum {
	OrtpRtcpXrRcvrRttNone,
	OrtpRtcpXrRcvrRttAll,
	OrtpRtcpXrRcvrRttSender
} OrtpRtcpXrRcvrRttMode;

typedef enum {
	OrtpRtcpXrStatSummaryLoss = (1 << 7),
	OrtpRtcpXrStatSummaryDup = (1 << 6),
	OrtpRtcpXrStatSummaryJitt = (1 << 5),
	OrtpRtcpXrStatSummaryTTL = (1 << 3),
	OrtpRtcpXrStatSummaryHL = (1 << 4)
} OrtpRtcpXrStatSummaryFlag;

typedef struct OrtpRtcpXrConfiguration {
	bool_t enabled;
	bool_t stat_summary_enabled;
	bool_t voip_metrics_enabled;
	OrtpRtcpXrRcvrRttMode rcvr_rtt_mode;
	int rcvr_rtt_max_size;
	OrtpRtcpXrStatSummaryFlag stat_summary_flags;
} OrtpRtcpXrConfiguration;

typedef struct OrtpRtcpXrStats {
	uint32_t last_rcvr_rtt_ts;	/* NTP timestamp (middle 32 bits) of last received XR rcvr-rtt */
	struct timeval last_rcvr_rtt_time;	/* Time at which last XR rcvr-rtt was received  */
	uint16_t rcv_seq_at_last_stat_summary;	/* Received sequence number at last XR stat-summary sent */
	uint32_t rcv_since_last_stat_summary;	/* The number of packets received since last XR stat-summary was sent */
	uint32_t dup_since_last_stat_summary;	/* The number of duplicate packets received since last XR stat-summary was sent */
	uint32_t min_jitter_since_last_stat_summary;	/* The minimum value of jitter since last XR stat-summary was sent */
	uint32_t max_jitter_since_last_stat_summary;	/* The maximum value of jitter since last XR stat-summary was sent */
	double olds_jitter_since_last_stat_summary;
	double oldm_jitter_since_last_stat_summary;
	double news_jitter_since_last_stat_summary;
	double newm_jitter_since_last_stat_summary;
	int64_t last_jitter_diff_since_last_stat_summary;
	double olds_ttl_or_hl_since_last_stat_summary;
	double oldm_ttl_or_hl_since_last_stat_summary;
	double news_ttl_or_hl_since_last_stat_summary;
	double newm_ttl_or_hl_since_last_stat_summary;
	uint8_t min_ttl_or_hl_since_last_stat_summary;	/* The minimum value of TTL/HL since last XR stat-summary was sent */
	uint8_t max_ttl_or_hl_since_last_stat_summary;	/* The maximum value of TTL/HL since last XR stat-summary was sent */
	uint32_t first_rcv_seq;
	uint32_t last_rcv_seq;
	uint32_t rcv_count;
	uint32_t discarded_count;
} OrtpRtcpXrStats;

typedef struct OrtpRtcpTmmbrInfo {
	mblk_t *sent;
	mblk_t *received;
} OrtpRtcpTmmbrInfo;

typedef struct _OrtpAddress{
	struct sockaddr_storage addr;
	socklen_t len;
}OrtpAddress;

typedef struct _OrtpStream {
	ortp_socket_t socket;
	int sockfamily;
	int loc_port;
	socklen_t rem_addrlen;
	struct sockaddr_storage rem_addr;
	socklen_t loc_addrlen;
	struct sockaddr_storage loc_addr;
	struct _RtpTransport *tr;
	struct timeval send_bw_start; /* used for bandwidth estimation */
	struct timeval recv_bw_start; /* used for bandwidth estimation */
	unsigned int sent_bytes; /* used for bandwidth estimation */
	unsigned int recv_bytes; /* used for bandwidth estimation */
	float upload_bw;
	float download_bw;
	OList *aux_destinations; /*list of OrtpAddress */
	msgb_allocator_t allocator;
} OrtpStream;

typedef struct _RtpStream
{
	OrtpStream gs;
	int max_rq_size;
	int time_jump;
	uint32_t ts_jump;
	queue_t rq;
	queue_t tev_rq;
	void *QoSHandle;
	unsigned long QoSFlowID;
	JitterControl jittctl;
	uint32_t snd_time_offset;/*the scheduler time when the application send its first timestamp*/
	uint32_t snd_ts_offset;	/* the first application timestamp sent by the application */
	uint32_t snd_rand_offset;	/* a random number added to the user offset to make the stream timestamp*/
	uint32_t snd_last_ts;	/* the last stream timestamp sended */
	uint32_t rcv_time_offset; /*the scheduler time when the application ask for its first timestamp*/
	uint32_t rcv_ts_offset;  /* the first stream timestamp */
	uint32_t rcv_query_ts_offset;	/* the first user timestamp asked by the application */
	uint32_t rcv_last_ts;	/* the last stream timestamp got by the application */
	uint16_t rcv_last_seq;	/* the last stream sequence number got by the application*/
	uint16_t pad;
	uint32_t rcv_last_app_ts; /* the last application timestamp asked by the application */
	uint32_t rcv_last_ret_ts; /* the timestamp of the last sample returned (only for continuous audio)*/
	uint32_t hwrcv_extseq; /* last received on socket extended sequence number */
	uint32_t hwrcv_seq_at_last_SR;
	uint32_t hwrcv_since_last_SR;
	uint32_t last_rcv_SR_ts;     /* NTP timestamp (middle 32 bits) of last received SR */
	struct timeval last_rcv_SR_time;   /* time at which last SR was received  */
	uint16_t snd_seq; /* send sequence number */
	uint32_t last_rtcp_packet_count; /*the sender's octet count in the last sent RTCP SR*/
	uint32_t sent_payload_bytes; /*used for RTCP sender reports*/
	int recv_errno;
	int send_errno;
	int snd_socket_size;
	int rcv_socket_size;
	int ssrc_changed_thres;
	jitter_stats_t jitter_stats;
}RtpStream;

typedef struct _RtcpStream
{
	OrtpStream gs;
	OrtpRtcpSendAlgorithm send_algo;
	OrtpRtcpXrConfiguration xr_conf;
	OrtpRtcpXrMediaCallbacks xr_media_callbacks;
	OrtpRtcpTmmbrInfo tmmbr_info;
	bool_t enabled; /*tells whether we can send RTCP packets */
	bool_t rtcp_xr_dlrr_to_send;
	uint8_t rtcp_fb_fir_seq_nr;	/* The FIR command sequence number */
	uint32_t last_rtcp_fb_pli_snt;
} RtcpStream;

typedef struct _RtpSession RtpSession;


/**
 * An object representing a bi-directional RTP session.
 * It holds sockets, jitter buffer, various counters (timestamp, sequence numbers...)
 * Applications SHOULD NOT try to read things within the RtpSession object but use
 * instead its public API (the rtp_session_* methods) where RtpSession is used as a
 * pointer.
 * rtp_session_new() allocates and initialize a RtpSession.
**/
struct _RtpSession
{
	RtpSession *next;	/* next RtpSession, when the session are enqueued by the scheduler */
	int mask_pos;	/* the position in the scheduler mask of RtpSession : do not move this field: it is part of the ABI since the session_set macros use it*/
	struct {
		RtpProfile *profile;
		int pt;
		unsigned int ssrc;
		WaitPoint wp;
	} snd,rcv;
	unsigned int inc_ssrc_candidate;
	int inc_same_ssrc_count;
	int hw_recv_pt; /* recv payload type before jitter buffer */
	int recv_buf_size;
	int target_upload_bandwidth; /* Target upload bandwidth at nework layer (with IP and UDP headers) in bits/s */
	RtpSignalTable on_ssrc_changed;
	RtpSignalTable on_payload_type_changed;
	RtpSignalTable on_telephone_event_packet;
	RtpSignalTable on_telephone_event;
	RtpSignalTable on_timestamp_jump;
	RtpSignalTable on_network_error;
	RtpSignalTable on_rtcp_bye;
	struct _OList *signal_tables;
	struct _OList *eventqs;
	RtpStream rtp;
	RtcpStream rtcp;
	OrtpRtcpXrStats rtcp_xr_stats;
	RtpSessionMode mode;
	struct _RtpScheduler *sched;
	uint32_t flags;
	int dscp;
	int multicast_ttl;
	int multicast_loopback;
	float duplication_ratio; /* Number of times a packet should be duplicated */
	float duplication_left ; /* Remainder of the duplication ratio */
	void * user_data;
	/* FIXME: Should be a table for all session participants. */
	struct timeval last_recv_time; /* Time of receiving the RTP/RTCP packet. */
	mblk_t *pending;
	/* telephony events extension */
	int tev_send_pt; /*telephone event to be used for sending*/
	mblk_t *current_tev;		/* the pending telephony events */
	mblk_t *minimal_sdes;
	mblk_t *full_sdes;
	queue_t contributing_sources;
	int lost_packets_test_vector;
	unsigned int interarrival_jitter_test_vector;
	unsigned int delay_test_vector;
	float rtt;/*last round trip delay calculated*/
	int cum_loss;
	OrtpNetworkSimulatorCtx *net_sim_ctx;
	RtpSession *spliced_session; /*a RtpSession that will retransmit everything received on this session*/
	rtp_stats_t stats;
	bool_t symmetric_rtp;
	bool_t permissive; /*use the permissive algorithm*/
	bool_t use_connect; /* use connect() on the socket */
	bool_t ssrc_set;

	bool_t reuseaddr; /*setsockopt SO_REUSEADDR */
	bool_t rtcp_mux;
	unsigned char avpf_features; /**< A bitmask of ORTP_AVPF_FEATURE_* macros. */
	bool_t use_pktinfo;

	bool_t is_spliced;

	bctbx_list_t *recv_addr_map;
};




#ifdef __cplusplus
extern "C"
{
#endif

ORTP_PUBLIC const char *ortp_network_simulator_mode_to_string(OrtpNetworkSimulatorMode mode);
ORTP_PUBLIC OrtpNetworkSimulatorMode ortp_network_simulator_mode_from_string(const char *str);


/* public API */
ORTP_PUBLIC RtpSession *rtp_session_new(int mode);
ORTP_PUBLIC void rtp_session_set_scheduling_mode(RtpSession *session, int yesno);
ORTP_PUBLIC void rtp_session_set_blocking_mode(RtpSession *session, int yesno);
ORTP_PUBLIC void rtp_session_set_profile(RtpSession *session, RtpProfile *profile);
ORTP_PUBLIC void rtp_session_set_send_profile(RtpSession *session,RtpProfile *profile);
ORTP_PUBLIC void rtp_session_set_recv_profile(RtpSession *session,RtpProfile *profile);
ORTP_PUBLIC RtpProfile *rtp_session_get_profile(RtpSession *session);
ORTP_PUBLIC RtpProfile *rtp_session_get_send_profile(RtpSession *session);
ORTP_PUBLIC RtpProfile *rtp_session_get_recv_profile(RtpSession *session);
ORTP_PUBLIC int rtp_session_signal_connect(RtpSession *session,const char *signal_name, RtpCallback cb, void *user_data);
ORTP_PUBLIC int rtp_session_signal_disconnect_by_callback(RtpSession *session,const char *signal_name, RtpCallback cb);
ORTP_PUBLIC void rtp_session_set_ssrc(RtpSession *session, uint32_t ssrc);
ORTP_PUBLIC uint32_t rtp_session_get_send_ssrc(RtpSession* session);
ORTP_PUBLIC uint32_t rtp_session_get_recv_ssrc(RtpSession *session);
ORTP_PUBLIC void rtp_session_set_seq_number(RtpSession *session, uint16_t seq);
ORTP_PUBLIC uint16_t rtp_session_get_seq_number(RtpSession *session);
ORTP_PUBLIC uint32_t rtp_session_get_rcv_ext_seq_number(RtpSession *session);
ORTP_PUBLIC int rtp_session_get_cum_loss(RtpSession *session);
ORTP_PUBLIC void rtp_session_set_duplication_ratio(RtpSession *session, float ratio);

ORTP_PUBLIC void rtp_session_enable_jitter_buffer(RtpSession *session , bool_t enabled);
ORTP_PUBLIC bool_t rtp_session_jitter_buffer_enabled(const RtpSession *session);
ORTP_PUBLIC void rtp_session_set_jitter_buffer_params(RtpSession *session, const JBParameters *par);
ORTP_PUBLIC void rtp_session_get_jitter_buffer_params(RtpSession *session, JBParameters *par);

/*deprecated jitter control functions*/
ORTP_PUBLIC void rtp_session_set_jitter_compensation(RtpSession *session, int milisec);
ORTP_PUBLIC void rtp_session_enable_adaptive_jitter_compensation(RtpSession *session, bool_t val);
ORTP_PUBLIC bool_t rtp_session_adaptive_jitter_compensation_enabled(RtpSession *session);

ORTP_PUBLIC void rtp_session_set_time_jump_limit(RtpSession *session, int miliseconds);
ORTP_PUBLIC int rtp_session_join_multicast_group(RtpSession *session, const char *ip);
ORTP_PUBLIC int rtp_session_set_local_addr(RtpSession *session,const char *addr, int rtp_port, int rtcp_port);
ORTP_PUBLIC int rtp_session_get_local_port(const RtpSession *session);
ORTP_PUBLIC int rtp_session_get_local_rtcp_port(const RtpSession *session);

ORTP_PUBLIC int
rtp_session_set_remote_addr_full (RtpSession * session, const char * rtp_addr, int rtp_port, const char * rtcp_addr, int rtcp_port);
/*same as previous function, old name:*/
ORTP_PUBLIC int rtp_session_set_remote_addr_and_port (RtpSession * session, const char * addr, int rtp_port, int rtcp_port);
ORTP_PUBLIC int rtp_session_set_remote_addr(RtpSession *session,const char *addr, int port);
ORTP_PUBLIC int rtp_session_add_aux_remote_addr_full(RtpSession * session, const char * rtp_addr, int rtp_port, const char * rtcp_addr, int rtcp_port);
ORTP_PUBLIC void rtp_session_clear_aux_remote_addr(RtpSession * session);
/* alternatively to the set_remote_addr() and set_local_addr(), an application can give
a valid socket (potentially connect()ed )to be used by the RtpSession */
ORTP_PUBLIC void rtp_session_set_sockets(RtpSession *session, int rtpfd, int rtcpfd);

ORTP_PUBLIC void rtp_session_get_transports(const RtpSession *session, RtpTransport **rtptr, RtpTransport **rtcptr);
/*those methods are provided for people who wants to send non-RTP messages using the RTP/RTCP sockets */
ORTP_PUBLIC ortp_socket_t rtp_session_get_rtp_socket(const RtpSession *session);
ORTP_PUBLIC ortp_socket_t rtp_session_get_rtcp_socket(const RtpSession *session);
ORTP_PUBLIC void rtp_session_refresh_sockets(RtpSession *session);


/* QOS / DSCP */
ORTP_PUBLIC int rtp_session_set_dscp(RtpSession *session, int dscp);
ORTP_PUBLIC int rtp_session_get_dscp(const RtpSession *session);


/* Packet info */
ORTP_PUBLIC int rtp_session_set_pktinfo(RtpSession *session, int activate);

/* Multicast methods */
ORTP_PUBLIC int rtp_session_set_multicast_ttl(RtpSession *session, int ttl);
ORTP_PUBLIC int rtp_session_get_multicast_ttl(RtpSession *session);

ORTP_PUBLIC int rtp_session_set_multicast_loopback(RtpSession *session, int yesno);
ORTP_PUBLIC int rtp_session_get_multicast_loopback(RtpSession *session);



ORTP_PUBLIC int rtp_session_set_send_payload_type(RtpSession *session, int paytype);
ORTP_PUBLIC int rtp_session_get_send_payload_type(const RtpSession *session);

ORTP_PUBLIC int rtp_session_get_recv_payload_type(const RtpSession *session);
ORTP_PUBLIC int rtp_session_set_recv_payload_type(RtpSession *session, int pt);

ORTP_PUBLIC int rtp_session_set_send_telephone_event_payload_type(RtpSession *session, int paytype);

ORTP_PUBLIC int rtp_session_set_payload_type(RtpSession *session, int pt);

ORTP_PUBLIC void rtp_session_set_symmetric_rtp (RtpSession * session, bool_t yesno);

ORTP_PUBLIC bool_t rtp_session_get_symmetric_rtp (const RtpSession * session);

ORTP_PUBLIC void rtp_session_enable_rtcp_mux(RtpSession *session, bool_t yesno);

ORTP_PUBLIC bool_t rtp_session_rtcp_mux_enabled(RtpSession *session);

ORTP_PUBLIC void rtp_session_set_connected_mode(RtpSession *session, bool_t yesno);

ORTP_PUBLIC void rtp_session_enable_rtcp(RtpSession *session, bool_t yesno);
/*
 * rtcp status
 * @return TRUE if rtcp is enabled for this session
 */
ORTP_PUBLIC bool_t rtp_session_rtcp_enabled(const RtpSession *session);

ORTP_PUBLIC void rtp_session_set_rtcp_report_interval(RtpSession *session, int value_ms);

ORTP_PUBLIC void rtp_session_set_target_upload_bandwidth(RtpSession *session, int target_bandwidth);

ORTP_PUBLIC void rtp_session_configure_rtcp_xr(RtpSession *session, const OrtpRtcpXrConfiguration *config);
ORTP_PUBLIC void rtp_session_set_rtcp_xr_media_callbacks(RtpSession *session, const OrtpRtcpXrMediaCallbacks *cbs);

ORTP_PUBLIC void rtp_session_set_ssrc_changed_threshold(RtpSession *session, int numpackets);

/*low level recv and send functions */
ORTP_PUBLIC mblk_t * rtp_session_recvm_with_ts (RtpSession * session, uint32_t user_ts);
ORTP_PUBLIC mblk_t * rtp_session_create_packet(RtpSession *session, size_t header_size, const uint8_t *payload, size_t payload_size);
ORTP_PUBLIC mblk_t * rtp_session_create_packet_raw(const uint8_t *packet, size_t packet_size);
ORTP_PUBLIC mblk_t * rtp_session_create_packet_with_data(RtpSession *session, uint8_t *payload, size_t payload_size, void (*freefn)(void*));
ORTP_PUBLIC mblk_t * rtp_session_create_packet_in_place(RtpSession *session,uint8_t *buffer, size_t size, void (*freefn)(void*) );
ORTP_PUBLIC int rtp_session_sendm_with_ts (RtpSession * session, mblk_t *mp, uint32_t userts);
ORTP_PUBLIC int rtp_session_sendto(RtpSession *session, bool_t is_rtp, mblk_t *m, int flags, const struct sockaddr *destaddr, socklen_t destlen);
ORTP_PUBLIC int rtp_session_recvfrom(RtpSession *session, bool_t is_rtp, mblk_t *m, int flags, struct sockaddr *from, socklen_t *fromlen);
/* high level recv and send functions */
ORTP_PUBLIC int rtp_session_recv_with_ts(RtpSession *session, uint8_t *buffer, int len, uint32_t ts, int *have_more);
ORTP_PUBLIC int rtp_session_send_with_ts(RtpSession *session, const uint8_t *buffer, int len, uint32_t userts);

/* event API*/
ORTP_PUBLIC void rtp_session_register_event_queue(RtpSession *session, OrtpEvQueue *q);
ORTP_PUBLIC void rtp_session_unregister_event_queue(RtpSession *session, OrtpEvQueue *q);


/* IP bandwidth usage estimation functions, returning bits/s*/
ORTP_PUBLIC float rtp_session_compute_send_bandwidth(RtpSession *session);
ORTP_PUBLIC float rtp_session_compute_recv_bandwidth(RtpSession *session);
ORTP_PUBLIC float rtp_session_get_send_bandwidth(RtpSession *session);
ORTP_PUBLIC float rtp_session_get_recv_bandwidth(RtpSession *session);
ORTP_PUBLIC float rtp_session_get_rtp_send_bandwidth(RtpSession *session);
ORTP_PUBLIC float rtp_session_get_rtp_recv_bandwidth(RtpSession *session);
ORTP_PUBLIC float rtp_session_get_rtcp_send_bandwidth(RtpSession *session);
ORTP_PUBLIC float rtp_session_get_rtcp_recv_bandwidth(RtpSession *session);

ORTP_PUBLIC void rtp_session_send_rtcp_APP(RtpSession *session, uint8_t subtype, const char *name, const uint8_t *data, int datalen);
/**
 *	Send the rtcp datagram \a packet to the destination set by rtp_session_set_remote_addr()
 *  The packet (\a packet) is freed once it is sent.
 *
 * @param session a rtp session.
 * @param m a rtcp packet presented as a mblk_t.
 * @return the number of bytes sent over the network.
 **/

ORTP_PUBLIC	int rtp_session_rtcp_sendm_raw(RtpSession * session, mblk_t * m);


ORTP_PUBLIC uint32_t rtp_session_get_current_send_ts(RtpSession *session);
ORTP_PUBLIC uint32_t rtp_session_get_current_recv_ts(RtpSession *session);
ORTP_PUBLIC void rtp_session_flush_sockets(RtpSession *session);
ORTP_PUBLIC void rtp_session_release_sockets(RtpSession *session);
ORTP_PUBLIC void rtp_session_resync(RtpSession *session);
ORTP_PUBLIC void rtp_session_reset(RtpSession *session);
ORTP_PUBLIC void rtp_session_destroy(RtpSession *session);

ORTP_PUBLIC const rtp_stats_t * rtp_session_get_stats(const RtpSession *session);
ORTP_PUBLIC const jitter_stats_t * rtp_session_get_jitter_stats( const RtpSession *session );
ORTP_PUBLIC void rtp_session_reset_stats(RtpSession *session);

ORTP_PUBLIC void rtp_session_set_data(RtpSession *session, void *data);
ORTP_PUBLIC void *rtp_session_get_data(const RtpSession *session);

ORTP_PUBLIC void rtp_session_set_recv_buf_size(RtpSession *session, int bufsize);
ORTP_PUBLIC void rtp_session_set_rtp_socket_send_buffer_size(RtpSession * session, unsigned int size);
ORTP_PUBLIC void rtp_session_set_rtp_socket_recv_buffer_size(RtpSession * session, unsigned int size);

/* in use with the scheduler to convert a timestamp in scheduler time unit (ms) */
ORTP_PUBLIC uint32_t rtp_session_ts_to_time(RtpSession *session,uint32_t timestamp);
ORTP_PUBLIC uint32_t rtp_session_time_to_ts(RtpSession *session, int millisecs);
/* this function aims at simulating senders with "imprecise" clocks, resulting in
rtp packets sent with timestamp uncorrelated with the system clock .
This is only availlable to sessions working with the oRTP scheduler */
ORTP_PUBLIC void rtp_session_make_time_distorsion(RtpSession *session, int milisec);

/*RTCP functions */
ORTP_PUBLIC void rtp_session_set_source_description(RtpSession *session, const char *cname,
	const char *name, const char *email, const char *phone,
    const char *loc, const char *tool, const char *note);
ORTP_PUBLIC void rtp_session_add_contributing_source(RtpSession *session, uint32_t csrc,
    const char *cname, const char *name, const char *email, const char *phone,
    const char *loc, const char *tool, const char *note);
/* DEPRECATED: Use rtp_session_remove_contributing_source instead of rtp_session_remove_contributing_sources */
#define rtp_session_remove_contributing_sources rtp_session_remove_contributing_source
ORTP_PUBLIC void rtp_session_remove_contributing_source(RtpSession *session, uint32_t csrc);
ORTP_PUBLIC mblk_t* rtp_session_create_rtcp_sdes_packet(RtpSession *session, bool_t full);

ORTP_PUBLIC void rtp_session_get_last_recv_time(RtpSession *session, struct timeval *tv);
ORTP_PUBLIC int rtp_session_bye(RtpSession *session, const char *reason);

ORTP_PUBLIC int rtp_session_get_last_send_error_code(RtpSession *session);
ORTP_PUBLIC void rtp_session_clear_send_error_code(RtpSession *session);
ORTP_PUBLIC int rtp_session_get_last_recv_error_code(RtpSession *session);
ORTP_PUBLIC void rtp_session_clear_recv_error_code(RtpSession *session);


ORTP_PUBLIC float rtp_session_get_round_trip_propagation(RtpSession *session);


ORTP_PUBLIC void rtp_session_enable_network_simulation(RtpSession *session, const OrtpNetworkSimulatorParams *params);

ORTP_PUBLIC void rtp_session_rtcp_set_lost_packet_value( RtpSession *session, const int value );
ORTP_PUBLIC void rtp_session_rtcp_set_jitter_value(RtpSession *session, const unsigned int value );
ORTP_PUBLIC void rtp_session_rtcp_set_delay_value(RtpSession *session, const unsigned int value );
ORTP_PUBLIC mblk_t * rtp_session_pick_with_cseq (RtpSession * session, const uint16_t sequence_number);


ORTP_PUBLIC void rtp_session_send_rtcp_xr_rcvr_rtt(RtpSession *session);
ORTP_PUBLIC void rtp_session_send_rtcp_xr_dlrr(RtpSession *session);
ORTP_PUBLIC void rtp_session_send_rtcp_xr_stat_summary(RtpSession *session);
ORTP_PUBLIC void rtp_session_send_rtcp_xr_voip_metrics(RtpSession *session);


ORTP_PUBLIC bool_t rtp_session_avpf_enabled(RtpSession *session);
ORTP_PUBLIC bool_t rtp_session_avpf_payload_type_feature_enabled(RtpSession *session, unsigned char feature);
ORTP_PUBLIC bool_t rtp_session_avpf_feature_enabled(RtpSession *session, unsigned char feature);
ORTP_PUBLIC void rtp_session_enable_avpf_feature(RtpSession *session, unsigned char feature, bool_t enable);
ORTP_PUBLIC uint16_t rtp_session_get_avpf_rr_interval(RtpSession *session);
ORTP_PUBLIC bool_t rtp_session_rtcp_psfb_scheduled(RtpSession *session, rtcp_psfb_type_t type);
ORTP_PUBLIC bool_t rtp_session_rtcp_rtpfb_scheduled(RtpSession *session, rtcp_rtpfb_type_t type);
ORTP_PUBLIC void rtp_session_send_rtcp_fb_generic_nack(RtpSession *session, uint16_t pid, uint16_t blp);
ORTP_PUBLIC void rtp_session_send_rtcp_fb_pli(RtpSession *session);
ORTP_PUBLIC void rtp_session_send_rtcp_fb_fir(RtpSession *session);
ORTP_PUBLIC void rtp_session_send_rtcp_fb_sli(RtpSession *session, uint16_t first, uint16_t number, uint8_t picture_id);
ORTP_PUBLIC void rtp_session_send_rtcp_fb_rpsi(RtpSession *session, uint8_t *bit_string, uint16_t bit_string_len);
ORTP_PUBLIC void rtp_session_send_rtcp_fb_tmmbr(RtpSession *session, uint64_t mxtbr);
ORTP_PUBLIC void rtp_session_send_rtcp_fb_tmmbn(RtpSession *session, uint32_t ssrc);


/*private */
ORTP_PUBLIC void rtp_session_init(RtpSession *session, int mode);
#define rtp_session_set_flag(session,flag) (session)->flags|=(flag)
#define rtp_session_unset_flag(session,flag) (session)->flags&=~(flag)
ORTP_PUBLIC void rtp_session_uninit(RtpSession *session);
ORTP_PUBLIC void rtp_session_dispatch_event(RtpSession *session, OrtpEvent *ev);

ORTP_PUBLIC void rtp_session_set_reuseaddr(RtpSession *session, bool_t yes);

ORTP_PUBLIC int meta_rtp_transport_modifier_inject_packet_to_send(RtpTransport *t, RtpTransportModifier *tpm, mblk_t *msg, int flags);
ORTP_PUBLIC int meta_rtp_transport_modifier_inject_packet_to_send_to(RtpTransport *t, RtpTransportModifier *tpm, mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen);
ORTP_PUBLIC int meta_rtp_transport_modifier_inject_packet_to_recv(RtpTransport *t, RtpTransportModifier *tpm, mblk_t *msg, int flags);

/**
 * get endpoint if any
 * @param[in] transport RtpTransport object.
 * @return #_RtpTransport
 *
 * */
ORTP_PUBLIC RtpTransport* meta_rtp_transport_get_endpoint(const RtpTransport *transport);
/**
 * set endpoint
 * @param[in] transport RtpTransport object.
 * @param[in] endpoint RtpEndpoint.
 *
 * */
ORTP_PUBLIC void meta_rtp_transport_set_endpoint(RtpTransport *transport,RtpTransport *endpoint);

ORTP_PUBLIC void meta_rtp_transport_destroy(RtpTransport *tp);
ORTP_PUBLIC void meta_rtp_transport_append_modifier(RtpTransport *tp,RtpTransportModifier *tpm);



ORTP_PUBLIC int rtp_session_splice(RtpSession *session, RtpSession *to_session);
ORTP_PUBLIC int rtp_session_unsplice(RtpSession *session, RtpSession *to_session);

ORTP_PUBLIC bool_t ortp_stream_is_ipv6(OrtpStream *os);

#ifdef __cplusplus
}
#endif

#endif
