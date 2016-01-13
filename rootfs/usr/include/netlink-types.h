/*
 * netlink-types.h	Netlink Types (Private)
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2003-2008 Thomas Graf <tgraf@suug.ch>
 */

#ifndef NETLINK_LOCAL_TYPES_H_
#define NETLINK_LOCAL_TYPES_H_

#include <netlink/list.h>
#include <linux/rtnetlink.h>
#include <netlink/route/link.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/route.h>
#include <netlink/route/link/info-api.h>


struct nl_cache_ops;
struct nl_sock;
struct nl_object;

struct nl_cache
{
	struct nl_list_head	c_items;
	int			c_nitems;
	int                     c_iarg1;
	int                     c_iarg2;
	struct nl_cache_ops *   c_ops;
};

struct nl_cache_assoc
{
	struct nl_cache *	ca_cache;
	change_func_t		ca_change;
};

struct nl_cache_mngr
{
	int			cm_protocol;
	int			cm_flags;
	int			cm_nassocs;
	struct nl_sock *	cm_handle;
	struct nl_cache_assoc *	cm_assocs;
};

struct nl_parser_param;

#define LOOSE_COMPARISON	1


struct nl_data
{
	size_t			d_size;
	void *			d_data;
};

struct nl_addr
{
	int			a_family;
	unsigned int		a_maxsize;
	unsigned int		a_len;
	int			a_prefixlen;
	int			a_refcnt;
	char			a_addr[0];
};

struct rtnl_link_map
{
	uint64_t lm_mem_start;
	uint64_t lm_mem_end;
	uint64_t lm_base_addr;
	uint16_t lm_irq;
	uint8_t  lm_dma;
	uint8_t  lm_port;
};

#define IFQDISCSIZ	32

struct rtnl_link
{
	NLHDR_COMMON

	char		l_name[IFNAMSIZ];

	uint32_t	l_family;
	uint32_t	l_arptype;
	uint32_t	l_index;
	uint32_t	l_flags;
	uint32_t	l_change;
	uint32_t 	l_mtu;
	uint32_t	l_link;
	uint32_t	l_txqlen;
	uint32_t	l_weight;
	uint32_t	l_master;
	struct nl_addr *l_addr;	
	struct nl_addr *l_bcast;
	char		l_qdisc[IFQDISCSIZ];
	struct rtnl_link_map l_map;
	uint64_t	l_stats[RTNL_LINK_STATS_MAX+1];
	uint32_t	l_flag_mask;
	uint8_t		l_operstate;
	uint8_t		l_linkmode;
	/* 2 byte hole */
	struct rtnl_link_info_ops *l_info_ops;
	void *		l_info;
};

struct rtnl_ncacheinfo
{
	uint32_t nci_confirmed;	/**< Time since neighbour validty was last confirmed */
	uint32_t nci_used;	/**< Time since neighbour entry was last ued */
	uint32_t nci_updated;	/**< Time since last update */
	uint32_t nci_refcnt;	/**< Reference counter */
};


struct rtnl_neigh
{
	NLHDR_COMMON
	uint32_t	n_family;
	uint32_t	n_ifindex;
	uint16_t	n_state;
	uint8_t		n_flags;
	uint8_t		n_type;	
	struct nl_addr *n_lladdr;
	struct nl_addr *n_dst;	
	uint32_t	n_probes;
	struct rtnl_ncacheinfo n_cacheinfo;
	uint32_t                n_state_mask;
	uint32_t                n_flag_mask;
};


struct rtnl_addr_cacheinfo
{
	/* Preferred lifetime in seconds */
	uint32_t aci_prefered;

	/* Valid lifetime in seconds */
	uint32_t aci_valid;

	/* Timestamp of creation in 1/100s seince boottime */
	uint32_t aci_cstamp;

	/* Timestamp of last update in 1/100s since boottime */
	uint32_t aci_tstamp;
};

struct rtnl_addr
{
	NLHDR_COMMON

	uint8_t		a_family;
	uint8_t		a_prefixlen;
	uint8_t		a_flags;
	uint8_t		a_scope;
	uint32_t	a_ifindex;

	struct nl_addr *a_peer;	
	struct nl_addr *a_local;
	struct nl_addr *a_bcast;
	struct nl_addr *a_anycast;
	struct nl_addr *a_multicast;

	struct rtnl_addr_cacheinfo a_cacheinfo;
	
	char a_label[IFNAMSIZ];
	uint32_t a_flag_mask;
};

struct rtnl_nexthop
{
	uint8_t			rtnh_flags;
	uint8_t			rtnh_flag_mask;
	uint8_t			rtnh_weight;
	/* 1 byte spare */
	uint32_t		rtnh_ifindex;
	struct nl_addr *	rtnh_gateway;
	uint32_t		ce_mask; /* HACK to support attr macros */
	struct nl_list_head	rtnh_list;
	uint32_t		rtnh_realms;
};

struct rtnl_route
{
	NLHDR_COMMON

	uint8_t			rt_family;
	uint8_t			rt_dst_len;
	uint8_t			rt_src_len;
	uint8_t			rt_tos;
	uint8_t			rt_protocol;
	uint8_t			rt_scope;
	uint8_t			rt_type;
	uint8_t			rt_nmetrics;
	uint32_t		rt_flags;
	struct nl_addr *	rt_dst;
	struct nl_addr *	rt_src;
	uint32_t		rt_table;
	uint32_t		rt_iif;
	uint32_t		rt_prio;
	uint32_t		rt_metrics[RTAX_MAX];
	uint32_t		rt_metrics_mask;
	uint32_t		rt_nr_nh;
	struct nl_addr *	rt_pref_src;
	struct nl_list_head	rt_nexthops;
	struct rtnl_rtcacheinfo	rt_cacheinfo;
	uint32_t		rt_flag_mask;
};

struct rtnl_rule
{
	NLHDR_COMMON

	uint64_t	r_mark;
	uint32_t	r_prio;
	uint32_t	r_realms;
	uint32_t	r_table;
	uint8_t		r_dsfield;
	uint8_t		r_type;
	uint8_t		r_family;
	uint8_t		r_src_len;
	uint8_t		r_dst_len;
	char		r_iif[IFNAMSIZ];
	struct nl_addr *r_src;
	struct nl_addr *r_dst;
	struct nl_addr *r_srcmap;
};

struct rtnl_neightbl_parms
{
	/**
	 * Interface index of the device this parameter set is assigned
	 * to or 0 for the default set.
	 */
	uint32_t		ntp_ifindex;

	/**
	 * Number of references to this parameter set.
	 */
	uint32_t		ntp_refcnt;

	/**
	 * Queue length for pending arp requests, i.e. the number of
	 * packets which are accepted from other layers while the
	 * neighbour address is still being resolved
	 */
	uint32_t		ntp_queue_len;

	/**
	 * Number of requests to send to the user level ARP daemon.
	 * Specify 0 to disable.
	 */
	uint32_t		ntp_app_probes;

	/**
	 * Maximum number of retries for unicast solicitation.
	 */
	uint32_t		ntp_ucast_probes;

	/**
	 * Maximum number of retries for multicast solicitation.
	 */
	uint32_t		ntp_mcast_probes;

	/**
	 * Base value in milliseconds to ompute reachable time, see RFC2461.
	 */
	uint64_t		ntp_base_reachable_time;

	/**
	 * Actual reachable time (read-only)
	 */
	uint64_t		ntp_reachable_time;	/* secs */

	/**
	 * The time in milliseconds between retransmitted Neighbor
	 * Solicitation messages.
	 */
	uint64_t		ntp_retrans_time;

	/**
	 * Interval in milliseconds to check for stale neighbour
	 * entries.
	 */
	uint64_t		ntp_gc_stale_time;	/* secs */

	/**
	 * Delay in milliseconds for the first time probe if
	 * the neighbour is reachable.
	 */
	uint64_t		ntp_probe_delay;	/* secs */

	/**
	 * Maximum delay in milliseconds of an answer to a neighbour
	 * solicitation message.
	 */
	uint64_t		ntp_anycast_delay;

	/**
	 * Minimum age in milliseconds before a neighbour entry
	 * may be replaced.
	 */
	uint64_t		ntp_locktime;

	/**
	 * Delay in milliseconds before answering to an ARP request
	 * for which a proxy ARP entry exists.
	 */
	uint64_t		ntp_proxy_delay;

	/**
	 * Queue length for the delayed proxy arp requests.
	 */
	uint32_t		ntp_proxy_qlen;
	
	/**
	 * Mask of available parameter attributes
	 */
	uint32_t		ntp_mask;
};

#define NTBLNAMSIZ	32

/**
 * Neighbour table
 * @ingroup neightbl
 */
struct rtnl_neightbl
{
	NLHDR_COMMON

	char			nt_name[NTBLNAMSIZ];
	uint32_t		nt_family;
	uint32_t		nt_gc_thresh1;
	uint32_t		nt_gc_thresh2;
	uint32_t		nt_gc_thresh3;
	uint64_t		nt_gc_interval;
	struct ndt_config	nt_config;
	struct rtnl_neightbl_parms nt_parms;
	struct ndt_stats	nt_stats;
};

struct rtnl_ratespec
{
	uint8_t			rs_cell_log;
	uint16_t		rs_feature;
	uint16_t		rs_addend;
	uint16_t		rs_mpu;
	uint32_t		rs_rate;
};

struct rtnl_tstats
{
	struct {
		uint64_t            bytes;
		uint64_t            packets;
	} tcs_basic;

	struct {
		uint32_t            bps;
		uint32_t            pps;
	} tcs_rate_est;

	struct {
		uint32_t            qlen;
		uint32_t            backlog;
		uint32_t            drops;
		uint32_t            requeues;
		uint32_t            overlimits;
	} tcs_queue;
};

struct rtnl_fifo
{
	uint32_t	qf_limit;
	uint32_t	qf_mask;
};

struct rtnl_prio
{
	uint32_t	qp_bands;
	uint8_t		qp_priomap[TC_PRIO_MAX+1];
	uint32_t	qp_mask;
};

struct rtnl_tbf
{
	uint32_t		qt_limit;
	uint32_t		qt_mpu;
	struct rtnl_ratespec	qt_rate;
	uint32_t		qt_rate_bucket;
	uint32_t		qt_rate_txtime;
	struct rtnl_ratespec	qt_peakrate;
	uint32_t		qt_peakrate_bucket;
	uint32_t		qt_peakrate_txtime;
	uint32_t		qt_mask;
};

struct rtnl_sfq
{
	uint32_t	qs_quantum;
	uint32_t	qs_perturb;
	uint32_t	qs_limit;
	uint32_t	qs_divisor;
	uint32_t	qs_flows;
	uint32_t	qs_mask;
};

struct rtnl_netem_corr
{
	uint32_t	nmc_delay;
	uint32_t	nmc_loss;
	uint32_t	nmc_duplicate;
};

struct rtnl_netem_reo
{
	uint32_t	nmro_probability;
	uint32_t	nmro_correlation;
};

struct rtnl_netem_crpt
{
	uint32_t	nmcr_probability;
	uint32_t	nmcr_correlation;
};

struct rtnl_netem_dist
{
	int16_t	*	dist_data;
	size_t		dist_size;
};

struct rtnl_netem
{
	uint32_t		qnm_latency;
	uint32_t		qnm_limit;
	uint32_t		qnm_loss;
	uint32_t		qnm_gap;
	uint32_t		qnm_duplicate;
	uint32_t		qnm_jitter;
	uint32_t		qnm_mask;
	struct rtnl_netem_corr	qnm_corr;
	struct rtnl_netem_reo	qnm_ro;
	struct rtnl_netem_crpt	qnm_crpt;
	struct rtnl_netem_dist  qnm_dist;
};

struct rtnl_htb_qdisc
{
	uint32_t		qh_rate2quantum;
	uint32_t		qh_defcls;
	uint32_t		qh_mask;
};

struct rtnl_htb_class
{
	uint32_t		ch_prio;
	uint32_t		ch_mtu;
	struct rtnl_ratespec	ch_rate;
	struct rtnl_ratespec	ch_ceil;
	uint32_t		ch_rbuffer;
	uint32_t		ch_cbuffer;
	uint32_t		ch_quantum;
	uint8_t			ch_overhead;
	uint8_t			ch_mpu;
	uint32_t		ch_mask;
};

struct rtnl_cbq
{
	struct tc_cbq_lssopt    cbq_lss;
	struct tc_ratespec      cbq_rate;
	struct tc_cbq_wrropt    cbq_wrr;
	struct tc_cbq_ovl       cbq_ovl;
	struct tc_cbq_fopt      cbq_fopt;
	struct tc_cbq_police    cbq_police;
};

struct rtnl_red
{
	uint32_t	qr_limit;
	uint32_t	qr_qth_min;
	uint32_t	qr_qth_max;
	uint8_t		qr_flags;
	uint8_t		qr_wlog;
	uint8_t		qr_plog;
	uint8_t		qr_scell_log;
	uint32_t	qr_mask;
};

struct flnl_request
{
	NLHDR_COMMON

	struct nl_addr *	lr_addr;
	uint32_t		lr_fwmark;
	uint8_t			lr_tos;
	uint8_t			lr_scope;
	uint8_t			lr_table;
};


struct flnl_result
{
	NLHDR_COMMON

	struct flnl_request *	fr_req;
	uint8_t			fr_table_id;
	uint8_t			fr_prefixlen;
	uint8_t			fr_nh_sel;
	uint8_t			fr_type;
	uint8_t			fr_scope;
	uint32_t		fr_error;
};

#define GENL_OP_HAS_POLICY	1
#define GENL_OP_HAS_DOIT	2
#define GENL_OP_HAS_DUMPIT	4

struct genl_family_op
{
	uint32_t		o_id;
	uint32_t		o_flags;

	struct nl_list_head	o_list;
};


#endif
