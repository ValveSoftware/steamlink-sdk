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

#ifndef STR_UTILS_H
#define STR_UTILS_H


#include <ortp/port.h>
#if defined(ORTP_TIMESTAMP)
#include <time.h>
#endif


#ifndef MIN
#define MIN(a,b) (((a)>(b)) ? (b) : (a))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#define return_if_fail(expr) if (!(expr)) {printf("%s:%i- assertion"#expr "failed\n",__FILE__,__LINE__); return;}
#define return_val_if_fail(expr,ret) if (!(expr)) {printf("%s:%i- assertion" #expr "failed\n",__FILE__,__LINE__); return (ret);}


typedef struct ortp_recv_addr {
	int family;
	union {
		struct in_addr ipi_addr;
		struct in6_addr ipi6_addr;
	} addr;
	unsigned short port;
} ortp_recv_addr_t;

typedef struct ortp_recv_addr_map {
	struct sockaddr_storage ss;
	ortp_recv_addr_t recv_addr;
	uint64_t ts;
} ortp_recv_addr_map_t;

typedef struct msgb
{
	struct msgb *b_prev;
	struct msgb *b_next;
	struct msgb *b_cont;
	struct datab *b_datap;
	unsigned char *b_rptr;
	unsigned char *b_wptr;
	uint32_t reserved1;
	uint32_t reserved2;
#if defined(ORTP_TIMESTAMP)
	struct timeval timestamp;
#endif
	ortp_recv_addr_t recv_addr; /*contains the destination address of incoming packets, used for ICE processing*/
	struct sockaddr_storage net_addr; /*source address of incoming packet, or dest address of outgoing packet, used only by simulator and modifiers*/
	socklen_t net_addrlen; /*source (dest) address of incoming (outgoing) packet length used by simulator and modifiers*/
	uint8_t ttl_or_hl;
} mblk_t;



typedef struct datab dblk_t;

typedef struct _queue
{
	mblk_t _q_stopper;
	int q_mcount;	/*number of packet in the q */
} queue_t;

#ifdef __cplusplus
extern "C" {
#endif

ORTP_PUBLIC void dblk_ref(dblk_t *d);
ORTP_PUBLIC void dblk_unref(dblk_t *d);
ORTP_PUBLIC unsigned char * dblk_base(dblk_t *db);
ORTP_PUBLIC unsigned char * dblk_lim(dblk_t *db);
ORTP_PUBLIC int dblk_ref_value(dblk_t *db);

ORTP_PUBLIC void qinit(queue_t *q);

ORTP_PUBLIC void putq(queue_t *q, mblk_t *m);

ORTP_PUBLIC mblk_t * getq(queue_t *q);

ORTP_PUBLIC void insq(queue_t *q,mblk_t *emp, mblk_t *mp);

ORTP_PUBLIC void remq(queue_t *q, mblk_t *mp);

ORTP_PUBLIC mblk_t * peekq(queue_t *q);

/* remove and free all messages in the q */
#define FLUSHALL 0
ORTP_PUBLIC void flushq(queue_t *q, int how);

ORTP_PUBLIC void mblk_init(mblk_t *mp);

ORTP_PUBLIC void mblk_meta_copy(const mblk_t *source, mblk_t *dest);

/* allocates a mblk_t, that points to a datab_t, that points to a buffer of size size. */
ORTP_PUBLIC mblk_t *allocb(size_t size, int unused);
#define BPRI_MED 0

/* allocates a mblk_t, that points to a datab_t, that points to buf; buf will be freed using freefn */
ORTP_PUBLIC mblk_t *esballoc(uint8_t *buf, size_t size, int pri, void (*freefn)(void*) );

/* frees a mblk_t, and if the datab ref_count is 0, frees it and the buffer too */
ORTP_PUBLIC void freeb(mblk_t *m);

/* frees recursively (follow b_cont) a mblk_t, and if the datab
ref_count is 0, frees it and the buffer too */
ORTP_PUBLIC void freemsg(mblk_t *mp);

/* duplicates a mblk_t , buffer is not duplicated*/
ORTP_PUBLIC mblk_t *dupb(mblk_t *m);

/* duplicates a complex mblk_t, buffer is not duplicated */
ORTP_PUBLIC mblk_t	*dupmsg(mblk_t* m);

/* returns the size of data of a message */
ORTP_PUBLIC size_t msgdsize(const mblk_t *mp);

/* concatenates all fragment of a complex message*/
ORTP_PUBLIC void msgpullup(mblk_t *mp,size_t len);

/* duplicates a single message, but with buffer included */
ORTP_PUBLIC mblk_t *copyb(const mblk_t *mp);

/* duplicates a complex message with buffer included */
ORTP_PUBLIC mblk_t *copymsg(const mblk_t *mp);

ORTP_PUBLIC mblk_t * appendb(mblk_t *mp, const char *data, size_t size, bool_t pad);
ORTP_PUBLIC void msgappend(mblk_t *mp, const char *data, size_t size, bool_t pad);

ORTP_PUBLIC mblk_t *concatb(mblk_t *mp, mblk_t *newm);

#define qempty(q) (&(q)->_q_stopper==(q)->_q_stopper.b_next)
#define qfirst(q) ((q)->_q_stopper.b_next!=&(q)->_q_stopper ? (q)->_q_stopper.b_next : NULL)
#define qbegin(q) ((q)->_q_stopper.b_next)
#define qlast(q) ((q)->_q_stopper.b_prev!=&(q)->_q_stopper ? (q)->_q_stopper.b_prev : NULL)
#define qend(q,mp)	((mp)==&(q)->_q_stopper)
#define qnext(q,mp) ((mp)->b_next)

typedef struct _msgb_allocator{
	queue_t q;
}msgb_allocator_t;

ORTP_PUBLIC void msgb_allocator_init(msgb_allocator_t *pa);
ORTP_PUBLIC mblk_t *msgb_allocator_alloc(msgb_allocator_t *pa, size_t size);
ORTP_PUBLIC void msgb_allocator_uninit(msgb_allocator_t *pa);

/**
 * Utility object to determine a maximum or minimum (but not both at the same
 * time), of a signal during a sliding period of time.
 */
typedef struct _ortp_extremum{
	float current_extremum;
	float last_stable;
	uint64_t extremum_time;
	int period;
}ortp_extremum;

ORTP_PUBLIC void ortp_extremum_reset(ortp_extremum *obj);
ORTP_PUBLIC void ortp_extremum_init(ortp_extremum *obj, int period);
ORTP_PUBLIC void ortp_extremum_record_min(ortp_extremum *obj, uint64_t curtime, float value);
ORTP_PUBLIC void ortp_extremum_record_max(ortp_extremum *obj, uint64_t curtime, float value);
ORTP_PUBLIC float ortp_extremum_get_current(ortp_extremum *obj);
/**
 * Unlike ortp_extremum_get_current() which can be very fluctuating, ortp_extremum_get_previous() returns the extremum found for the previous period.
**/
ORTP_PUBLIC float ortp_extremum_get_previous(ortp_extremum *obj);

ORTP_PUBLIC void ortp_recvaddr_to_sockaddr(ortp_recv_addr_t *recvaddr, struct sockaddr *addr, socklen_t *socklen);

#ifdef __cplusplus
}
#endif

#endif
