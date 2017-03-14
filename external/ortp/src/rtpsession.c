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
#include "ortp/telephonyevents.h"
#include "ortp/rtcp.h"
#include "jitterctl.h"
#include "scheduler.h"
#include "utils.h"
#include "rtpsession_priv.h"

#if (_WIN32_WINNT >= 0x0600)
#include <delayimp.h>
#undef ExternC /* avoid redefinition... */
#ifdef ORTP_WINDOWS_DESKTOP
#include <QOS2.h>
#include <VersionHelpers.h>
#endif
#endif

/**
 * #_RtpTransport object which can handle multiples security protocols. You can for instance use this object
 * to use both sRTP and tunnel transporter. mblk_t messages received and sent from the endpoint
 * will pass through the list of modifiers given. First modifier in list will be first to modify the message
 * in send mode and last in receive mode.
 * @param[in] endpoint #_RtpTransport object in charge of sending/receiving packets. If NULL, it will use standards sendto and recvfrom functions.
 * @param[in] modifiers_count number of #_RtpTransport object given in the variadic list. Must be 0 if none are given.
 * @returns #_RtpTransport object that will be generated or NULL.
**/
RtpTransport* meta_rtp_transport_new(RtpTransport *endpoint, unsigned modifiers_count, ...);
RtpTransport* meta_rtcp_transport_new(RtpTransport *endpoint, unsigned modifiers_count, ...);
void meta_rtp_transport_link(RtpTransport *rtp, RtpTransport *rtcp);

/* this function initialize all session parameter's that depend on the payload type */
static void payload_type_changed(RtpSession *session, PayloadType *pt){
	jitter_control_set_payload(&session->rtp.jittctl,pt);
	rtp_session_set_time_jump_limit(session,session->rtp.time_jump);
	if (pt->type==PAYLOAD_VIDEO){
		session->permissive=TRUE;
		ortp_message("Using permissive algorithm");
	}
	else session->permissive=FALSE;
}

void wait_point_init(WaitPoint *wp){
	ortp_mutex_init(&wp->lock,NULL);
	ortp_cond_init(&wp->cond,NULL);
	wp->time=0;
	wp->wakeup=FALSE;
}
void wait_point_uninit(WaitPoint *wp){
	ortp_cond_destroy(&wp->cond);
	ortp_mutex_destroy(&wp->lock);
}

#define wait_point_lock(wp) ortp_mutex_lock(&(wp)->lock)
#define wait_point_unlock(wp) ortp_mutex_unlock(&(wp)->lock)

void wait_point_wakeup_at(WaitPoint *wp, uint32_t t, bool_t dosleep){
	wp->time=t;
	wp->wakeup=TRUE;
	if (dosleep) ortp_cond_wait(&wp->cond,&wp->lock);
}


bool_t wait_point_check(WaitPoint *wp, uint32_t t){
	bool_t ok=FALSE;

	if (wp->wakeup){
		if (TIME_IS_NEWER_THAN(t,wp->time)){
			wp->wakeup=FALSE;
			ok=TRUE;

		}
	}
	return ok;
}
#define wait_point_wakeup(wp) ortp_cond_signal(&(wp)->cond);

extern void rtp_parse(RtpSession *session, mblk_t *mp, uint32_t local_str_ts,
		struct sockaddr *addr, socklen_t addrlen);


static uint32_t uint32_t_random(void){
	return ortp_random();
}


/* put an rtp packet in queue. It is called by rtp_parse()
   A return value of -1 means the packet was a duplicate, 0 means the packet was ok */
int rtp_putq(queue_t *q, mblk_t *mp)
{
	mblk_t *tmp;
	rtp_header_t *rtp=(rtp_header_t*)mp->b_rptr,*tmprtp;
	/* insert message block by increasing time stamp order : the last (at the bottom)
		message of the queue is the newest*/
	ortp_debug("rtp_putq(): Enqueuing packet with ts=%i and seq=%i",rtp->timestamp,rtp->seq_number);

	if (qempty(q)) {
		putq(q,mp);
		return 0;
	}
	tmp=qlast(q);
	/* we look at the queue from bottom to top, because enqueued packets have a better chance
	to be enqueued at the bottom, since there are surely newer */
	while (!qend(q,tmp))
	{
		tmprtp=(rtp_header_t*)tmp->b_rptr;
		ortp_debug("rtp_putq(): Seeing packet with seq=%i",tmprtp->seq_number);

		if (rtp->seq_number == tmprtp->seq_number)
		{
			/* this is a duplicated packet. Don't queue it */
			ortp_debug("rtp_putq: duplicated message.");
			freemsg(mp);
			return -1;
		}else if (RTP_SEQ_IS_STRICTLY_GREATER_THAN(rtp->seq_number,tmprtp->seq_number)){

			insq(q,tmp->b_next,mp);
			return 0;
		}
		tmp=tmp->b_prev;
	}
	/* this packet is the oldest, it has to be
	placed on top of the queue */
	insq(q,qfirst(q),mp);
	return 0;
}



mblk_t *rtp_getq(queue_t *q,uint32_t timestamp, int *rejected)
{
	mblk_t *tmp,*ret=NULL,*old=NULL;
	rtp_header_t *tmprtp;
	uint32_t ts_found=0;

	*rejected=0;
	ortp_debug("rtp_getq(): Timestamp %i wanted.",timestamp);

	if (qempty(q))
	{
		/*ortp_debug("rtp_getq: q is empty.");*/
		return NULL;
	}
	/* return the packet with ts just equal or older than the asked timestamp */
	/* packets with older timestamps are discarded */
	while ((tmp=qfirst(q))!=NULL)
	{
		tmprtp=(rtp_header_t*)tmp->b_rptr;
		ortp_debug("rtp_getq: Seeing packet with ts=%i",tmprtp->timestamp);
		if ( RTP_TIMESTAMP_IS_NEWER_THAN(timestamp,tmprtp->timestamp) )
		{
			if (ret!=NULL && tmprtp->timestamp==ts_found) {
				/* we've found two packets with same timestamp. return the first one */
				break;
			}
			if (old!=NULL) {
				ortp_debug("rtp_getq: discarding too old packet with ts=%i",ts_found);
				(*rejected)++;
				freemsg(old);
			}
			ret=getq(q); /* dequeue the packet, since it has an interesting timestamp*/
			ts_found=tmprtp->timestamp;
			ortp_debug("rtp_getq: Found packet with ts=%i",tmprtp->timestamp);
			old=ret;
		}
		else
		{
			break;
		}
	}
	return ret;
}

mblk_t *rtp_getq_permissive(queue_t *q,uint32_t timestamp, int *rejected)
{
	mblk_t *tmp,*ret=NULL;
	rtp_header_t *tmprtp;

	*rejected=0;
	ortp_debug("rtp_getq_permissive(): Timestamp %i wanted.",timestamp);

	if (qempty(q))
	{
		/*ortp_debug("rtp_getq: q is empty.");*/
		return NULL;
	}
	/* return the packet with the older timestamp (provided that it is older than
	the asked timestamp) */
	tmp=qfirst(q);
	tmprtp=(rtp_header_t*)tmp->b_rptr;
	ortp_debug("rtp_getq_permissive: Seeing packet with ts=%i",tmprtp->timestamp);
	if ( RTP_TIMESTAMP_IS_NEWER_THAN(timestamp,tmprtp->timestamp) )
	{
		ret=getq(q); /* dequeue the packet, since it has an interesting timestamp*/
		ortp_debug("rtp_getq_permissive: Found packet with ts=%i",tmprtp->timestamp);
	}
	return ret;
}


void
rtp_session_init (RtpSession * session, int mode)
{
	JBParameters jbp;
	if (session == NULL)
	{
		ortp_debug("rtp_session_init: Invalid paramter (session=NULL)");
		return;
	}
	memset (session, 0, sizeof (RtpSession));
	session->mode = (RtpSessionMode) mode;
	if ((mode == RTP_SESSION_RECVONLY) || (mode == RTP_SESSION_SENDRECV))
	{
		rtp_session_set_flag (session, RTP_SESSION_RECV_SYNC);
		rtp_session_set_flag (session, RTP_SESSION_RECV_NOT_STARTED);

	}
	if ((mode == RTP_SESSION_SENDONLY) || (mode == RTP_SESSION_SENDRECV))
	{
		rtp_session_set_flag (session, RTP_SESSION_SEND_NOT_STARTED);
		session->snd.ssrc=uint32_t_random();
		/* set default source description */
		rtp_session_set_source_description(session,"unknown@unknown",NULL,NULL,
				NULL,NULL,"oRTP-" ORTP_VERSION,NULL);
	}
	rtp_session_set_profile (session, &av_profile); /*the default profile to work with */
	session->rtp.gs.socket=-1;
	session->rtcp.gs.socket=-1;
#ifndef _WIN32
	session->rtp.snd_socket_size=0;	/*use OS default value unless on windows where they are definitely too short*/
	session->rtp.rcv_socket_size=0;
#else
	session->rtp.snd_socket_size=session->rtp.rcv_socket_size=65536;
#endif
	session->rtp.ssrc_changed_thres=50;
	session->dscp=RTP_DEFAULT_DSCP;
	session->multicast_ttl=RTP_DEFAULT_MULTICAST_TTL;
	session->multicast_loopback=RTP_DEFAULT_MULTICAST_LOOPBACK;
	qinit(&session->rtp.rq);
	qinit(&session->rtp.tev_rq);
	qinit(&session->contributing_sources);
	session->eventqs=NULL;

	/* Initialize RTCP send algorithm */
	session->target_upload_bandwidth = 80000; /* 80kbits/s to have 4kbits/s dedicated to RTCP if rtp_session_set_target_upload_bandwidth() is not called. */
	session->rtcp.send_algo.initial = TRUE;
	session->rtcp.send_algo.allow_early = TRUE;

	/* init signal tables */
	rtp_signal_table_init (&session->on_ssrc_changed, session,"ssrc_changed");
	rtp_signal_table_init (&session->on_payload_type_changed, session,"payload_type_changed");
	rtp_signal_table_init (&session->on_telephone_event, session,"telephone-event");
	rtp_signal_table_init (&session->on_telephone_event_packet, session,"telephone-event_packet");
	rtp_signal_table_init (&session->on_timestamp_jump,session,"timestamp_jump");
	rtp_signal_table_init (&session->on_network_error,session,"network_error");
	rtp_signal_table_init (&session->on_rtcp_bye,session,"rtcp_bye");
	wait_point_init(&session->snd.wp);
	wait_point_init(&session->rcv.wp);
	/*defaults send payload type to 0 (pcmu)*/
	rtp_session_set_send_payload_type(session,0);
	/*sets supposed recv payload type to undefined */
	rtp_session_set_recv_payload_type(session,-1);
	/* configure jitter buffer with working default parameters */
	jbp.min_size=RTP_DEFAULT_JITTER_TIME;
	jbp.nom_size=RTP_DEFAULT_JITTER_TIME;
	jbp.max_size=-1;
	jbp.max_packets= 100;/* maximum number of packet allowed to be queued */
	jbp.adaptive=TRUE;
	rtp_session_enable_jitter_buffer(session,TRUE);
	rtp_session_set_jitter_buffer_params(session,&jbp);
	rtp_session_set_time_jump_limit(session,5000);
	rtp_session_enable_rtcp(session,TRUE);
	rtp_session_set_rtcp_report_interval(session,RTCP_DEFAULT_REPORT_INTERVAL);
	session->recv_buf_size = UDP_MAX_SIZE;
	session->symmetric_rtp = FALSE;
	session->permissive=FALSE;
	session->reuseaddr=TRUE;
	msgb_allocator_init(&session->rtp.gs.allocator);
	msgb_allocator_init(&session->rtcp.gs.allocator);
	/*set default rtptransport*/
	{
		RtpTransport *rtp_tr = meta_rtp_transport_new(NULL, 0);
		RtpTransport *rtcp_tr = meta_rtcp_transport_new(NULL, 0);
		meta_rtp_transport_link(rtp_tr, rtcp_tr);
		rtp_session_set_transports(session, rtp_tr, rtcp_tr);
	}
	session->tev_send_pt = -1; /*check in rtp profile when needed*/
}


/**
 * Creates a new rtp session.
 * If the session is able to send data (RTP_SESSION_SENDONLY or
 * RTP_SESSION_SENDRECV), then a random SSRC number is choosed for
 * the outgoing stream.
 * @param mode One of the RtpSessionMode flags.
 *
 * @return the newly created rtp session.
**/
RtpSession *
rtp_session_new (int mode)
{
	RtpSession *session;
	session = (RtpSession *) ortp_malloc (sizeof (RtpSession));
	if (session == NULL)
	{
		ortp_error("rtp_session_new: Memory allocation failed");
		return NULL;
	}
	rtp_session_init (session, mode);
	return session;
}

/**
 * Sets the scheduling mode of the rtp session. If \a yesno is TRUE, the rtp session is in
 *	the scheduled mode, that means that you can use session_set_select() to block until it's time
 *	to receive or send on this session according to the timestamp passed to the respective functions.
 *  You can also use blocking mode (see rtp_session_set_blocking_mode() ), to simply block within
 *	the receive and send functions.
 *	If \a yesno is FALSE, the ortp scheduler will not manage those sessions, meaning that blocking mode
 *  and the use of session_set_select() for this session are disabled.
 *@param session a rtp session.
 *@param yesno 	a boolean to indicate the scheduling mode.
 *
 *
**/
void
rtp_session_set_scheduling_mode (RtpSession * session, int yesno)
{
	if (yesno)
	{
		RtpScheduler *sched;
		sched = ortp_get_scheduler ();
		if (sched != NULL)
		{
			rtp_session_set_flag (session, RTP_SESSION_SCHEDULED);
			session->sched = sched;
			rtp_scheduler_add_session (sched, session);
		}
		else
			ortp_warning
				("rtp_session_set_scheduling_mode: Cannot use scheduled mode because the "
				 "scheduler is not started. Use ortp_scheduler_init() before.");
	}
	else
		rtp_session_unset_flag (session, RTP_SESSION_SCHEDULED);
}


/**
 *	This function implicitely enables the scheduling mode if yesno is TRUE.
 *	rtp_session_set_blocking_mode() defines the behaviour of the rtp_session_recv_with_ts() and
 *	rtp_session_send_with_ts() functions. If \a yesno is TRUE, rtp_session_recv_with_ts()
 *	will block until it is time for the packet to be received, according to the timestamp
 *	passed to the function. After this time, the function returns.
 *	For rtp_session_send_with_ts(), it will block until it is time for the packet to be sent.
 *	If \a yesno is FALSE, then the two functions will return immediately.
 *
 *  @param session a rtp session
 *  @param yesno a boolean
**/
void
rtp_session_set_blocking_mode (RtpSession * session, int yesno)
{
	if (yesno){
		rtp_session_set_scheduling_mode(session,TRUE);
		rtp_session_set_flag (session, RTP_SESSION_BLOCKING_MODE);
	}else
		rtp_session_unset_flag (session, RTP_SESSION_BLOCKING_MODE);
}

/**
 *	Set the RTP profile to be used for the session. By default, all session are created by
 *	rtp_session_new() are initialized with the AV profile, as defined in RFC 3551. The application
 *	can set any other profile instead using that function.
 *
 * @param session a rtp session
 * @param profile a rtp profile
**/

void
rtp_session_set_profile (RtpSession * session, RtpProfile * profile)
{
	session->snd.profile = profile;
	session->rcv.profile = profile;
	rtp_session_telephone_events_supported(session);
}

/**
 *	By default oRTP automatically sends RTCP SR or RR packets. If
 *	yesno is set to FALSE, the RTCP sending of packet is disabled.
 *	This functionnality might be needed for some equipments that do not
 *	support RTCP, leading to a traffic of ICMP errors on the network.
 *	It can also be used to save bandwidth despite the RTCP bandwidth is
 *	actually and usually very very low.
**/
void rtp_session_enable_rtcp(RtpSession *session, bool_t yesno){
	session->rtcp.enabled=yesno;
}

bool_t rtp_session_rtcp_enabled(const RtpSession *session) {
	return  session->rtcp.enabled;
}

/**
 * Sets the default interval in milliseconds for RTCP reports emitted by the session
 *
**/
void rtp_session_set_rtcp_report_interval(RtpSession *session, int value_ms) {
	if (value_ms <= 0) session->rtcp.send_algo.T_rr_interval = 0;
	else session->rtcp.send_algo.T_rr_interval = (uint32_t)value_ms;
}

void rtp_session_set_target_upload_bandwidth(RtpSession *session, int target_bandwidth) {
	session->target_upload_bandwidth = target_bandwidth;
}

/**
 *	Set the RTP profile to be used for the sending by this session. By default, all session are created by
 *	rtp_session_new() are initialized with the AV profile, as defined in RFC 3551. The application
 *	can set any other profile instead using that function.
 * @param session a rtp session
 * @param profile a rtp profile
 *
**/

void
rtp_session_set_send_profile (RtpSession * session, RtpProfile * profile)
{
	session->snd.profile = profile;
	rtp_session_send_telephone_events_supported(session);
}



/**
 *	Set the RTP profile to be used for the receiveing by this session. By default, all session are created by
 *	rtp_session_new() are initialized with the AV profile, as defined in RFC 3551. The application
 *	can set any other profile instead using that function.
 *
 * @param session a rtp session
 * @param profile a rtp profile
**/

void
rtp_session_set_recv_profile (RtpSession * session, RtpProfile * profile)
{
	session->rcv.profile = profile;
	rtp_session_recv_telephone_events_supported(session);
}

/**
 *@param session a rtp session
 *
 *	DEPRECATED! Returns current send profile.
 *	Use rtp_session_get_send_profile() or rtp_session_get_recv_profile()
 *
**/
RtpProfile *rtp_session_get_profile(RtpSession *session){
	return session->snd.profile;
}


/**
 *@param session a rtp session
 *
 *	Returns current send profile.
 *
**/
RtpProfile *rtp_session_get_send_profile(RtpSession *session){
	return session->snd.profile;
}

/**
 *@param session a rtp session
 *
 *	Returns current receive profile.
 *
**/
RtpProfile *rtp_session_get_recv_profile(RtpSession *session){
	return session->rcv.profile;
}

/**
 *	The default value is UDP_MAX_SIZE bytes, a value which is working for mostly everyone.
 *	However if your application can make assumption on the sizes of received packet,
 *	it can be interesting to set it to a lower value in order to save memory.
 *
 * @param session a rtp session
 * @param bufsize max size in bytes for receiving packets
**/
void rtp_session_set_recv_buf_size(RtpSession *session, int bufsize){
	session->recv_buf_size=bufsize;
}

/**
 *	Set kernel send maximum buffer size for the rtp socket.
 *	A value of zero defaults to the operating system default.
**/
void rtp_session_set_rtp_socket_send_buffer_size(RtpSession * session, unsigned int size){
	session->rtp.snd_socket_size=size;
	_rtp_session_apply_socket_sizes(session);
}

/**
 *	Set kernel recv maximum buffer size for the rtp socket.
 *	A value of zero defaults to the operating system default.
**/
void rtp_session_set_rtp_socket_recv_buffer_size(RtpSession * session, unsigned int size){
	session->rtp.rcv_socket_size=size;
	_rtp_session_apply_socket_sizes(session);
}

/**
 *	This function provides the way for an application to be informed of various events that
 *	may occur during a rtp session. \a signal_name is a string identifying the event, and \a cb is
 *	a user supplied function in charge of processing it. The application can register
 *	several callbacks for the same signal, in the limit of \a RTP_CALLBACK_TABLE_MAX_ENTRIES.
 *	Here are name and meaning of supported signals types:
 *
 *	"ssrc_changed" : the SSRC of the incoming stream has changed.
 *
 *	"payload_type_changed" : the payload type of the incoming stream has changed.
 *
 *	"telephone-event_packet" : a telephone-event rtp packet (RFC2833) is received.
 *
 *	"telephone-event" : a telephone event has occured. This is a high-level shortcut for "telephone-event_packet".
 *
 *	"network_error" : a network error happened on a socket. Arguments of the callback functions are
 *						a const char * explaining the error, an int errno error code and the user_data as usual.
 *
 *	"timestamp_jump" : we have received a packet with timestamp in far future compared to last timestamp received.
 *						The farness of far future is set by rtp_sesssion_set_time_jump_limit()
 *  "rtcp_bye": we have received a RTCP bye packet. Arguments of the callback
 *              functions are a const char * containing the leaving reason and
 *              the user_data.
 *
 *	Returns: 0 on success, -EOPNOTSUPP if the signal does not exists, -1 if no more callbacks
 *	can be assigned to the signal type.
 *
 * @param session 	a rtp session
 * @param signal_name	the name of a signal
 * @param cb		a RtpCallback
 * @param user_data	a pointer to any data to be passed when invoking the callback.
 *
**/
int
rtp_session_signal_connect (RtpSession * session, const char *signal_name,
				RtpCallback cb, void *user_data)
{
	OList *elem;
	for (elem=session->signal_tables;elem!=NULL;elem=o_list_next(elem)){
		RtpSignalTable *s=(RtpSignalTable*) elem->data;
		if (strcmp(signal_name,s->signal_name)==0){
			return rtp_signal_table_add(s,cb,user_data);
		}
	}
	ortp_warning ("rtp_session_signal_connect: inexistent signal %s",signal_name);
	return -1;
}


/**
 *	Removes callback function \a cb to the list of callbacks for signal \a signal.
 *
 * @param session a rtp session
 * @param signal_name	a signal name
 * @param cb	a callback function.
 * @return: 0 on success, a negative value if the callback was not found.
**/
int
rtp_session_signal_disconnect_by_callback (RtpSession * session, const char *signal_name,
					   RtpCallback cb)
{
	OList *elem;
	for (elem=session->signal_tables;elem!=NULL;elem=o_list_next(elem)){
		RtpSignalTable *s=(RtpSignalTable*) elem->data;
		if (strcmp(signal_name,s->signal_name)==0){
			return rtp_signal_table_remove_by_callback(s,cb);
		}
	}
	ortp_warning ("rtp_session_signal_connect: inexistant signal %s",signal_name);
	return -1;
}


/**
 * Set the initial sequence number for outgoing stream..
 * @param session		a rtp session freshly created.
 * @param seq			a 16 bit unsigned number.
 *
**/
void rtp_session_set_seq_number(RtpSession *session, uint16_t seq){
	session->rtp.snd_seq=seq;
}

void rtp_session_set_duplication_ratio(RtpSession *session, float ratio){
	session->duplication_ratio=ratio;
}


/**
 * Get the current sequence number for outgoing stream.
**/
uint16_t rtp_session_get_seq_number(RtpSession *session){
	return session->rtp.snd_seq;
}

/**
 * Returns the highest extended sequence number received.
**/
uint32_t rtp_session_get_rcv_ext_seq_number(RtpSession *session){
	return session->rtp.hwrcv_extseq;
}

/**
 * Returns the latest cumulative loss value computed
 **/
int rtp_session_get_cum_loss(RtpSession *session){
	return session->cum_loss;
}

/**
 *	Sets the SSRC for the outgoing stream.
 *  If not done, a random ssrc is used.
 *
 * @param session a rtp session.
 * @param ssrc an unsigned 32bit integer representing the synchronisation source identifier (SSRC).
**/
void
rtp_session_set_ssrc (RtpSession * session, uint32_t ssrc)
{
	session->snd.ssrc = ssrc;
}

/**
 *	Get the SSRC for the outgoing stream.
 *
 * @param session a rtp session.
**/
uint32_t
rtp_session_get_send_ssrc (RtpSession* session)
{
	return session->snd.ssrc;
}

/**
 * Get the SSRC for the incoming stream.
 *
 * If no packets have been received yet, 0 is returned.
**/
uint32_t rtp_session_get_recv_ssrc(RtpSession *session){
	return session->rcv.ssrc;
}


void rtp_session_update_payload_type(RtpSession *session, int paytype){
	/* check if we support this payload type */
	PayloadType *pt=rtp_profile_get_payload(session->rcv.profile,paytype);
	if (pt!=0){
		session->hw_recv_pt=paytype;
		ortp_message ("payload type changed to %i(%s) !",
				 paytype,pt->mime_type);
		payload_type_changed(session,pt);
	}else{
		ortp_warning("Receiving packet with unknown payload type %i.",paytype);
	}
}
/**
 *	Sets the payload type of the rtp session. It decides of the payload types written in the
 *	of the rtp header for the outgoing stream, if the session is SENDRECV or SENDONLY.
 *	For payload type in incoming packets, the application can be informed by registering
 *	for the "payload_type_changed" signal, so that it can make the necessary changes
 *	on the downstream decoder that deals with the payload of the packets.
 *
 * @param session a rtp session
 * @param paytype the payload type number
 * @return 0 on success, -1 if the payload is not defined.
**/

int
rtp_session_set_send_payload_type (RtpSession * session, int paytype)
{
	session->snd.pt=paytype;
	return 0;
}

/**
 *@param session a rtp session
 *
 *@return the payload type currently used in outgoing rtp packets
**/
int rtp_session_get_send_payload_type(const RtpSession *session){
	return session->snd.pt;
}

/**
 * Assign the payload type number for sending telephone-event.
 * It is required that a "telephone-event" PayloadType is assigned in the RtpProfile set for the RtpSession.
 * This function is in most of cases useless, unless there is an ambiguity where several PayloadType for "telephone-event" are present in the RtpProfile.
 * This might happen during SIP offeranswer scenarios. This function allows to remove any ambiguity by letting the application choose the one to be used.
 * @param session the RtpSession
 * @param paytype the payload type number
 * @returns 0, -1 on error.
**/
int rtp_session_set_send_telephone_event_payload_type(RtpSession *session, int paytype){
	session->tev_send_pt = paytype;
	return 0;
}

/**
 *
 *	Sets the expected payload type for incoming packets.
 *	If the actual payload type in incoming packets is different that this expected payload type, thus
 *	the "payload_type_changed" signal is emitted.
 *
 *@param session a rtp session
 *@param paytype the payload type number
 *@return 0 on success, -1 if the payload is not defined.
**/

int
rtp_session_set_recv_payload_type (RtpSession * session, int paytype)
{
	PayloadType *pt;
	session->rcv.pt=paytype;
	session->hw_recv_pt=paytype;
	pt=rtp_profile_get_payload(session->rcv.profile,paytype);
	if (pt!=NULL){
		payload_type_changed(session,pt);
	}
	return 0;
}

/**
 *@param session a rtp session
 *
 * @return the payload type currently used in incoming rtp packets
**/
int rtp_session_get_recv_payload_type(const RtpSession *session){
	return session->rcv.pt;
}

/**
 *	Sets the expected payload type for incoming packets and payload type to be used for outgoing packets.
 *	If the actual payload type in incoming packets is different that this expected payload type, thus
 *	the "payload_type_changed" signal is emitted.
 *
 * @param session a rtp session
 * @param pt the payload type number
 * @return 0 on success, -1 if the payload is not defined.
**/
int rtp_session_set_payload_type(RtpSession *session, int pt){
	if (rtp_session_set_send_payload_type(session,pt)<0) return -1;
	if (rtp_session_set_recv_payload_type(session,pt)<0) return -1;
	return 0;
}


static void rtp_header_init_from_session(rtp_header_t *rtp, RtpSession *session){
	rtp->version = 2;
	rtp->padbit = 0;
	rtp->extbit = 0;
	rtp->markbit= 0;
	rtp->cc = 0;
	rtp->paytype = session->snd.pt;
	rtp->ssrc = session->snd.ssrc;
	rtp->timestamp = 0;	/* set later, when packet is sended */
	/* set a seq number */
	rtp->seq_number=session->rtp.snd_seq;
}

/**
 *	Allocates a new rtp packet. In the header, ssrc and payload_type according to the session's
 *	context. Timestamp is not set, it will be set when the packet is going to be
 *	sent with rtp_session_sendm_with_ts(). Sequence number is initalized to previous sequence number sent + 1
 *	If payload_size is zero, thus an empty packet (just a RTP header) is returned.
 *
 *@param session a rtp session.
 *@param header_size the rtp header size. For standart size (without extensions), it is RTP_FIXED_HEADER_SIZE
 *@param payload data to be copied into the rtp packet.
 *@param payload_size size of data carried by the rtp packet.
 *@return a rtp packet in a mblk_t (message block) structure.
**/
mblk_t * rtp_session_create_packet(RtpSession *session,size_t header_size, const uint8_t *payload, size_t payload_size)
{
	mblk_t *mp;
	size_t msglen=header_size+payload_size;
	rtp_header_t *rtp;

	mp=allocb(msglen,BPRI_MED);
	rtp=(rtp_header_t*)mp->b_rptr;
	rtp_header_init_from_session(rtp,session);
	/*copy the payload, if any */
	mp->b_wptr+=header_size;
	if (payload_size){
		memcpy(mp->b_wptr,payload,payload_size);
		mp->b_wptr+=payload_size;
	}
	return mp;
}


/**
 * Create a packet already including headers
 */
mblk_t * rtp_session_create_packet_raw(const uint8_t *packet, size_t packet_size) {
	mblk_t *mp;

	mp=allocb(packet_size,BPRI_MED);
	if (packet_size){
		memcpy(mp->b_wptr,packet,packet_size);
		mp->b_wptr+=packet_size;
	}
	return mp;
}

/**
 *	Creates a new rtp packet using the given payload buffer (no copy). The header will be allocated separetely.
 *  In the header, ssrc and payload_type according to the session's
 *	context. Timestamp and seq number are not set, there will be set when the packet is going to be
 *	sent with rtp_session_sendm_with_ts().
 *	oRTP will send this packet using libc's sendmsg() (if this function is availlable!) so that there will be no
 *	packet concatenation involving copies to be done in user-space.
 *  \a freefn can be NULL, in that case payload will be kept untouched.
 *
 * @param session a rtp session.
 * @param payload the data to be sent with this packet
 * @param payload_size size of data
 * @param freefn a function that will be called when the payload buffer is no more needed.
 * @return: a rtp packet in a mblk_t (message block) structure.
**/

mblk_t * rtp_session_create_packet_with_data(RtpSession *session, uint8_t *payload, size_t payload_size, void (*freefn)(void*))
{
	mblk_t *mp,*mpayload;
	int header_size=RTP_FIXED_HEADER_SIZE; /* revisit when support for csrc is done */
	rtp_header_t *rtp;

	mp=allocb(header_size,BPRI_MED);
	rtp=(rtp_header_t*)mp->b_rptr;
	rtp_header_init_from_session(rtp,session);
	mp->b_wptr+=header_size;
	/* create a mblk_t around the user supplied payload buffer */
	mpayload=esballoc(payload,payload_size,BPRI_MED,freefn);
	mpayload->b_wptr+=payload_size;
	/* link it with the header */
	mp->b_cont=mpayload;
	return mp;
}


/**
 * Creates a new rtp packet using the buffer given in arguments (no copy).
 * In the header, ssrc and payload_type according to the session's
 *context. Timestamp and seq number are not set, there will be set when the packet is going to be
 *	sent with rtp_session_sendm_with_ts().
 * \a freefn can be NULL, in that case payload will be kept untouched.
 *
 * @param session a rtp session.
 * @param buffer a buffer that contains first just enough place to write a RTP header, then the data to send.
 * @param size the size of the buffer
 * @param freefn a function that will be called once the buffer is no more needed (the data has been sent).
 * @return a rtp packet in a mblk_t (message block) structure.
**/
mblk_t * rtp_session_create_packet_in_place(RtpSession *session,uint8_t *buffer, size_t size, void (*freefn)(void*) )
{
	mblk_t *mp;
	rtp_header_t *rtp;

	mp=esballoc(buffer,size,BPRI_MED,freefn);

	rtp=(rtp_header_t*)mp->b_rptr;
	rtp_header_init_from_session(rtp,session);
	return mp;
}


ORTP_PUBLIC int __rtp_session_sendm_with_ts (RtpSession * session, mblk_t *mp, uint32_t packet_ts, uint32_t send_ts)
{
	rtp_header_t *rtp;
	uint32_t packet_time;
	int error = 0;
	size_t packsize;
	RtpScheduler *sched=session->sched;
	RtpStream *stream=&session->rtp;

	if (session->flags & RTP_SESSION_SEND_NOT_STARTED)
	{
		session->rtp.snd_ts_offset = send_ts;
		/* Set initial last_rcv_time to first send time. */
		if ((session->flags & RTP_SESSION_RECV_NOT_STARTED)
		|| session->mode == RTP_SESSION_SENDONLY) {
			ortp_gettimeofday(&session->last_recv_time, NULL);
		}
		if (session->flags & RTP_SESSION_SCHEDULED) {
			session->rtp.snd_time_offset = sched->time_;
		}
		rtp_session_unset_flag (session,RTP_SESSION_SEND_NOT_STARTED);
	}
	/* if we are in blocking mode, then suspend the process until the scheduler it's time to send  the
	 * next packet */
	/* if the timestamp of the packet queued is older than current time, then you we must
	 * not block */
	if (session->flags & RTP_SESSION_SCHEDULED)
	{
		wait_point_lock(&session->snd.wp);
		packet_time =
			rtp_session_ts_to_time (session,
					 send_ts -
					 session->rtp.snd_ts_offset) +
					session->rtp.snd_time_offset;
		/*ortp_message("rtp_session_send_with_ts: packet_time=%i time=%i",packet_time,sched->time_);*/
		if (TIME_IS_STRICTLY_NEWER_THAN (packet_time, sched->time_))
		{
			wait_point_wakeup_at(&session->snd.wp,packet_time,(session->flags & RTP_SESSION_BLOCKING_MODE)!=0);
			session_set_clr(&sched->w_sessions,session);	/* the session has written */
		}
		else session_set_set(&sched->w_sessions,session);	/*to indicate select to return immediately */
		wait_point_unlock(&session->snd.wp);
	}

	if(mp==NULL) {/*for people who just want to be blocked but
		 do not want to send anything.*/
		session->rtp.snd_last_ts = packet_ts;
		return 0;
	}

	rtp=(rtp_header_t*)mp->b_rptr;

	packsize = msgdsize(mp) ;
	session->duplication_left += session->duplication_ratio;
	if (rtp->version == 0) {
		/* We are probably trying to send a STUN packet so don't change its content. */
	} else {
		rtp->timestamp=packet_ts;
		if (rtp_profile_is_telephone_event(session->snd.profile, rtp->paytype)){
			rtp->seq_number = session->rtp.snd_seq;
			session->rtp.snd_seq++;
		}
		else
		{
			session->rtp.snd_seq=rtp->seq_number+1;
		}
		session->rtp.snd_last_ts = packet_ts;

		stream->sent_payload_bytes+=(uint32_t)(packsize-RTP_FIXED_HEADER_SIZE);

		ortp_global_stats.sent += (1+(int)session->duplication_left) * packsize;
		session->stats.sent += (1+(int)session->duplication_left) * packsize;

		ortp_global_stats.packet_sent++;
		session->stats.packet_sent++;

		session->stats.packet_dup_sent+=(int)session->duplication_left;
		ortp_global_stats.packet_sent+=(int)session->duplication_left;;
	}

	while (session->duplication_left>=1.f) {
		error = rtp_session_rtp_send (session, copymsg(mp));
		session->duplication_left -= 1.f;
	}
	error = rtp_session_rtp_send (session, mp);

	/*send RTCP packet if needed */
	rtp_session_run_rtcp_send_scheduler(session);
	/* receives rtcp packet if session is send-only*/
	/*otherwise it is done in rtp_session_recvm_with_ts */
	if (session->mode==RTP_SESSION_SENDONLY) rtp_session_rtcp_recv(session);
	return error;
}

/**
 *	Send the rtp datagram \a packet to the destination set by rtp_session_set_remote_addr()
 *	with timestamp \a timestamp. For audio data, the timestamp is the number
 *	of the first sample resulting of the data transmitted. See rfc1889 for details.
 *  The packet (\a packet) is freed once it is sent.
 *
 *@param session a rtp session.
 *@param packet a rtp packet presented as a mblk_t.
 *@param timestamp the timestamp of the data to be sent.
 * @return the number of bytes sent over the network.
**/

int rtp_session_sendm_with_ts(RtpSession *session, mblk_t *packet, uint32_t timestamp){
	return __rtp_session_sendm_with_ts(session,packet,timestamp,timestamp);
}




/**
 *	Send a rtp datagram to the destination set by rtp_session_set_remote_addr() containing
 *	the data from \a buffer with timestamp \a userts. This is a high level function that uses
 *	rtp_session_create_packet() and rtp_session_sendm_with_ts() to send the data.
 *
 *@param session a rtp session.
 *@param buffer a buffer containing the data to be sent in a rtp packet.
 *@param len the length of the data buffer, in bytes.
 *@param userts	the timestamp of the data to be sent. Refer to the rfc to know what it is.
 *@return the number of bytes sent over the network.
**/
int
rtp_session_send_with_ts (RtpSession * session, const uint8_t * buffer, int len,
			  uint32_t userts)
{
	mblk_t *m;
	int err;
#ifdef USE_SENDMSG
	m=rtp_session_create_packet_with_data(session,(uint8_t*)buffer,len,NULL);
#else
	m = rtp_session_create_packet(session,RTP_FIXED_HEADER_SIZE,(uint8_t*)buffer,len);
#endif
	err=rtp_session_sendm_with_ts(session,m,userts);
	return err;
}



static void payload_type_changed_notify(RtpSession *session, int paytype){
	PayloadType *pt = rtp_profile_get_payload(session->rcv.profile,paytype);
	if (pt) {
		session->rcv.pt = paytype;
		rtp_signal_table_emit (&session->on_payload_type_changed);
	}
}
/**
 *	Try to get an rtp packet presented as a mblk_t structure from the rtp session at a given sequence number.
 *	This function is very usefull for codec with Forward error correction capabilities
 *
 *	This function returns the entire packet (with header).
 *
 *	 *
 * @param session a rtp session.
 * @param sequence_number a sequence number.
 *
 * @return a rtp packet presented as a mblk_t, or NULL if not found.
 **/

mblk_t *
rtp_session_pick_with_cseq (RtpSession * session, const uint16_t sequence_number) {
	queue_t* q= &session->rtp.rq;
	mblk_t* mb;
	for (mb=qbegin(q); !qend(q,mb); mb=qnext(q,mb)){
		if (rtp_get_seqnumber(mb)==sequence_number) {
			return mb;
		}
	}
	return NULL;
}

static void check_for_seq_number_gap(RtpSession *session, rtp_header_t *rtp) {
	uint16_t pid;
	uint16_t i;
	/*don't check anything before first packet delivered*/
	if (session->flags & RTP_SESSION_FIRST_PACKET_DELIVERED && RTP_SEQ_IS_STRICTLY_GREATER_THAN(rtp->seq_number, session->rtp.rcv_last_seq + 1)) {
		uint16_t first_missed_seq = session->rtp.rcv_last_seq + 1;
		uint16_t diff = rtp->seq_number - first_missed_seq;
		pid = first_missed_seq;
		for (i = 0; i <= (diff / 16); i++) {
			uint16_t seq;
			uint16_t blp = 0;
			for (seq = pid + 1; (seq < rtp->seq_number) && ((seq - pid) < 16); seq++) {
				blp |= (1 << (seq - pid - 1));
			}
			rtp_session_send_rtcp_fb_generic_nack(session, pid, blp);
			pid = seq;
		}
	}
}

/**
 *	Try to get a rtp packet presented as a mblk_t structure from the rtp session.
 *	The \a user_ts parameter is relative to the first timestamp of the incoming stream. In other
 *	words, the application does not have to know the first timestamp of the stream, it can
 *	simply call for the first time this function with \a user_ts=0, and then incrementing it
 *	as it want. The RtpSession takes care of synchronisation between the stream timestamp
 *	and the user timestamp given here.
 *
 *	This function returns the entire packet (with header).
 *
 *	The behaviour of this function has changed since version 0.15.0. Previously the payload data could be
 *	accessed using  mblk_t::b_cont::b_rptr field of the returned mblk_t.
 *	This is no more the case.
 *	The convenient way of accessing the payload data is to use rtp_get_payload() :
 *	@code
 *	unsigned char *payload;
 *	int payload_size;
 *	payload_size=rtp_get_payload(mp,&payload);
 *	@endcode
 *	OR simply skip the header this way, the data is then comprised between mp->b_rptr and mp->b_wptr:
 *	@code
 *	rtp_get_payload(mp,&mp->b_rptr);
 *	@endcode
 *
 *
 * @param session a rtp session.
 * @param user_ts a timestamp.
 *
 * @return a rtp packet presented as a mblk_t.
**/

mblk_t *
rtp_session_recvm_with_ts (RtpSession * session, uint32_t user_ts)
{
	mblk_t *mp = NULL;
	rtp_header_t *rtp;
	uint32_t ts;
	uint32_t packet_time;
	RtpScheduler *sched=session->sched;
	int rejected=0;
	bool_t read_socket=TRUE;

	/* if we are scheduled, remember the scheduler time at which the application has
	 * asked for its first timestamp */

	if (session->flags & RTP_SESSION_RECV_NOT_STARTED)
	{
		session->rtp.rcv_query_ts_offset = user_ts;
		/* Set initial last_rcv_time to first recv time. */
		if ((session->flags & RTP_SESSION_SEND_NOT_STARTED)
		|| session->mode == RTP_SESSION_RECVONLY) {
			ortp_gettimeofday(&session->last_recv_time, NULL);
		}
		if (session->flags & RTP_SESSION_SCHEDULED) {
			session->rtp.rcv_time_offset = sched->time_;
			//ortp_message("setting snd_time_offset=%i",session->rtp.snd_time_offset);
		}
		rtp_session_unset_flag (session,RTP_SESSION_RECV_NOT_STARTED);
	}else{
		/*prevent reading from the sockets when two
		consecutives calls for a same timestamp*/
		if (user_ts==session->rtp.rcv_last_app_ts)
			read_socket=FALSE;
	}
	session->rtp.rcv_last_app_ts = user_ts;
	if (read_socket){
		rtp_session_rtp_recv (session, user_ts);
		rtp_session_rtcp_recv(session);
	}
	/* check for telephone event first */
	mp=getq(&session->rtp.tev_rq);
	if (mp!=NULL){
		size_t msgsize=msgdsize(mp);
		ortp_global_stats.recv += msgsize;
		session->stats.recv += msgsize;
		rtp_signal_table_emit2(&session->on_telephone_event_packet,mp);
		rtp_session_check_telephone_events(session,mp);
		freemsg(mp);
		mp=NULL;
	}

	/* then now try to return a media packet, if possible */
	/* first condition: if the session is starting, don't return anything
	 * until the queue size reaches jitt_comp */

	if (session->flags & RTP_SESSION_RECV_SYNC)
	{
		queue_t *q = &session->rtp.rq;
		if (qempty(q))
		{
			ortp_debug ("Queue is empty.");
			goto end;
		}
		rtp = (rtp_header_t *) qfirst(q)->b_rptr;
		session->rtp.rcv_ts_offset = rtp->timestamp;
		session->rtp.rcv_last_ret_ts = user_ts;	/* just to have an init value */
		session->rcv.ssrc = rtp->ssrc;
		/* delete the recv synchronisation flag */
		rtp_session_unset_flag (session, RTP_SESSION_RECV_SYNC);
	}

	/*calculate the stream timestamp from the user timestamp */
	ts = jitter_control_get_compensated_timestamp(&session->rtp.jittctl,user_ts);
	if (session->rtp.jittctl.enabled==TRUE){
		if (session->permissive)
			mp = rtp_getq_permissive(&session->rtp.rq, ts,&rejected);
		else{
			mp = rtp_getq(&session->rtp.rq, ts,&rejected);
		}
	}else mp=getq(&session->rtp.rq);/*no jitter buffer at all*/

	session->stats.outoftime+=rejected;
	ortp_global_stats.outoftime+=rejected;
	session->rtcp_xr_stats.discarded_count += rejected;

	goto end;

	end:
	if (mp != NULL)
	{
		size_t msgsize = msgdsize(mp);	/* evaluate how much bytes (including header) is received by app */
		uint32_t packet_ts;
		ortp_global_stats.recv += msgsize;
		session->stats.recv += msgsize;
		rtp = (rtp_header_t *) mp->b_rptr;
		packet_ts=rtp->timestamp;
		ortp_debug("Returning mp with ts=%i", packet_ts);
		/* check for payload type changes */
		if (session->rcv.pt != rtp->paytype)
		{
			payload_type_changed_notify(session, rtp->paytype);
		}
		check_for_seq_number_gap(session, rtp);
		/* update the packet's timestamp so that it corrected by the
		adaptive jitter buffer mechanism */
		if (session->rtp.jittctl.adaptive){
			uint32_t changed_ts;
			/* only update correction offset between packets of different
			timestamps*/
			if (packet_ts!=session->rtp.rcv_last_ts)
				jitter_control_update_corrective_slide(&session->rtp.jittctl);
			changed_ts=packet_ts+session->rtp.jittctl.corrective_slide;
			rtp->timestamp=changed_ts;
			/*ortp_debug("Returned packet has timestamp %u, with clock slide compensated it is %u",packet_ts,rtp->timestamp);*/
		}
		session->rtp.rcv_last_ts = packet_ts;
		session->rtp.rcv_last_seq = rtp->seq_number;
		if (!(session->flags & RTP_SESSION_FIRST_PACKET_DELIVERED)){
			rtp_session_set_flag(session,RTP_SESSION_FIRST_PACKET_DELIVERED);
		}
	}
	else
	{
		ortp_debug ("No mp for timestamp queried");
	}
	rtp_session_rtcp_process_recv(session);

	if (session->flags & RTP_SESSION_SCHEDULED)
	{
		/* if we are in blocking mode, then suspend the calling process until timestamp
		 * wanted expires */
		/* but we must not block the process if the timestamp wanted by the application is older
		 * than current time */
		wait_point_lock(&session->rcv.wp);
		packet_time =
			rtp_session_ts_to_time (session,
					 user_ts -
					 session->rtp.rcv_query_ts_offset) +
			session->rtp.rcv_time_offset;
		ortp_debug ("rtp_session_recvm_with_ts: packet_time=%i, time=%i",packet_time, sched->time_);

		if (TIME_IS_STRICTLY_NEWER_THAN (packet_time, sched->time_))
		{
			wait_point_wakeup_at(&session->rcv.wp,packet_time, (session->flags & RTP_SESSION_BLOCKING_MODE)!=0);
			session_set_clr(&sched->r_sessions,session);
		}
		else session_set_set(&sched->r_sessions,session);	/*to unblock _select() immediately */
		wait_point_unlock(&session->rcv.wp);
	}
	return mp;
}


/**
 *	NOTE: use of this function is discouraged when sending payloads other than
 *	pcm/pcmu/pcma/adpcm types.
 *	rtp_session_recvm_with_ts() does better job.
 *
 *	Tries to read the bytes of the incoming rtp stream related to timestamp ts. In case
 *	where the user supplied buffer \a buffer is not large enough to get all the data
 *	related to timestamp ts, then *( have_more) is set to 1 to indicate that the application
 *	should recall the function with the same timestamp to get more data.
 *
 *  When the rtp session is scheduled (see rtp_session_set_scheduling_mode() ), and the
 *	blocking mode is on (see rtp_session_set_blocking_mode() ), then the calling thread
 *	is suspended until the timestamp given as argument expires, whatever a received packet
 *	fits the query or not.
 *
 *	Important note: it is clear that the application cannot know the timestamp of the first
 *	packet of the incoming stream, because it can be random. The \a ts timestamp given to the
 *	function is used relatively to first timestamp of the stream. In simple words, 0 is a good
 *	value to start calling this function.
 *
 *	This function internally calls rtp_session_recvm_with_ts() to get a rtp packet. The content
 *	of this packet is then copied into the user supplied buffer in an intelligent manner:
 *	the function takes care of the size of the supplied buffer and the timestamp given in
 *	argument. Using this function it is possible to read continous audio data (e.g. pcma,pcmu...)
 *	with for example a standart buffer of size of 160 with timestamp incrementing by 160 while the incoming
 *	stream has a different packet size.
 *
 *Returns: if a packet was availlable with the corresponding timestamp supplied in argument
 *	then the number of bytes written in the user supplied buffer is returned. If no packets
 *	are availlable, either because the sender has not started to send the stream, or either
 *	because silence packet are not transmitted, or either because the packet was lost during
 *	network transport, then the function returns zero.
 *@param session a rtp session.
 *@param buffer a user supplied buffer to write the data.
 *@param len the length in bytes of the user supplied buffer.
 *@param ts the timestamp wanted.
 *@param have_more the address of an integer to indicate if more data is availlable for the given timestamp.
 *
**/
int rtp_session_recv_with_ts (RtpSession * session, uint8_t * buffer,
				   int len, uint32_t ts, int * have_more){
	mblk_t *mp=NULL;
	int plen,blen=0;
	*have_more=0;
	while(1){
		if (session->pending){
			mp=session->pending;
			session->pending=NULL;
		}else {
			mp=rtp_session_recvm_with_ts(session,ts);
			if (mp!=NULL) rtp_get_payload(mp,&mp->b_rptr);
		}
		if (mp){
			plen=(int)(mp->b_wptr-mp->b_rptr);
			if (plen<=len){
				memcpy(buffer,mp->b_rptr,plen);
				buffer+=plen;
				blen+=plen;
				len-=plen;
				freemsg(mp);
				mp=NULL;
			}else{
				memcpy(buffer,mp->b_rptr,len);
				mp->b_rptr+=len;
				buffer+=len;
				blen+=len;
				len=0;
				session->pending=mp;
				*have_more=1;
				break;
			}
		}else break;
	}
	return blen;
}
/**
 *	When the rtp session is scheduled and has started to send packets, this function
 *	computes the timestamp that matches to the present time. Using this function can be
 *	usefull when sending discontinuous streams. Some time can be elapsed between the end
 *	of a stream burst and the begin of a new stream burst, and the application may be not
 *	not aware of this elapsed time. In order to get a valid (current) timestamp to pass to
 *	#rtp_session_send_with_ts() or #rtp_session_sendm_with_ts(), the application may
 *	use rtp_session_get_current_send_ts().
 *
 * @param session a rtp session.
 * @return the current send timestamp for the rtp session.
**/
uint32_t rtp_session_get_current_send_ts(RtpSession *session)
{
	uint32_t userts;
	uint32_t session_time;
	RtpScheduler *sched=session->sched;
	PayloadType *payload;
	payload=rtp_profile_get_payload(session->snd.profile,session->snd.pt);
	return_val_if_fail(payload!=NULL, 0);
	if ( (session->flags & RTP_SESSION_SCHEDULED)==0 ){
		ortp_warning("can't guess current timestamp because session is not scheduled.");
		return 0;
	}
	session_time=sched->time_-session->rtp.snd_time_offset;
	userts=  (uint32_t)( ( (double)(session_time) * (double) payload->clock_rate )/ 1000.0)
				+ session->rtp.snd_ts_offset;
	return userts;
}

/**
 * Same thing as rtp_session_get_current_send_ts() except that it's for an incoming stream.
 * Works only on scheduled mode.
 *
 * @param session a rtp session.
 * @return the theoritical that would have to be receive now.
 *
**/
uint32_t rtp_session_get_current_recv_ts(RtpSession *session){
	uint32_t userts;
	uint32_t session_time;
	RtpScheduler *sched=ortp_get_scheduler();
	PayloadType *payload;
	payload=rtp_profile_get_payload(session->rcv.profile,session->rcv.pt);
	return_val_if_fail(payload!=NULL, 0);
	if ( (session->flags & RTP_SESSION_SCHEDULED)==0 ){
		ortp_warning("can't guess current timestamp because session is not scheduled.");
		return 0;
	}
	session_time=sched->time_-session->rtp.rcv_time_offset;
	userts=  (uint32_t)( ( (double)(session_time) * (double) payload->clock_rate )/ 1000.0)
				+ session->rtp.rcv_ts_offset;
	return userts;
}

/**
 * oRTP has the possibility to inform the application through a callback registered
 * with rtp_session_signal_connect about crazy incoming RTP stream that jumps from
 * a timestamp N to N+some_crazy_value. This lets the opportunity for the application
 * to reset the session in order to resynchronize, or any other action like stopping the call
 * and reporting an error.
 * @param session the rtp session
 * @param milisecs a time interval in miliseconds
 *
**/
void rtp_session_set_time_jump_limit(RtpSession *session, int milisecs){
	uint32_t ts;
	session->rtp.time_jump=milisecs;
	ts=rtp_session_time_to_ts(session,milisecs);
	if (ts==0) session->rtp.ts_jump=1<<31;	/* do not detect ts jump */
	else session->rtp.ts_jump=ts;
}

void _rtp_session_release_sockets(RtpSession *session, bool_t release_transports){

	if (release_transports){
		if (session->rtp.gs.tr) {
			if (session->rtp.gs.tr->t_close)
				session->rtp.gs.tr->t_close(session->rtp.gs.tr);
			session->rtp.gs.tr->t_destroy(session->rtp.gs.tr);

		}
		session->rtp.gs.tr = 0;

		if (session->rtcp.gs.tr)  {
			if (session->rtcp.gs.tr->t_close)
				session->rtcp.gs.tr->t_close(session->rtcp.gs.tr);
			session->rtcp.gs.tr->t_destroy(session->rtcp.gs.tr);
		}
		session->rtcp.gs.tr = 0;
	}

	if (session->rtp.gs.socket!=(ortp_socket_t)-1) close_socket (session->rtp.gs.socket);
	if (session->rtcp.gs.socket!=(ortp_socket_t)-1) close_socket (session->rtcp.gs.socket);
	session->rtp.gs.socket=-1;
	session->rtcp.gs.socket=-1;

	/* don't discard remote addresses, then can be preserved for next use.
	session->rtp.gs.rem_addrlen=0;
	session->rtcp.gs.rem_addrlen=0;
	*/
}

/**
 * Closes the rtp and rtcp sockets, and associated RtpTransport.
**/
void rtp_session_release_sockets(RtpSession *session){
	_rtp_session_release_sockets(session, TRUE);
}

ortp_socket_t rtp_session_get_rtp_socket(const RtpSession *session){
	return rtp_session_using_transport(session, rtp) ? (session->rtp.gs.tr->t_getsocket)(session->rtp.gs.tr) : session->rtp.gs.socket;
}

ortp_socket_t rtp_session_get_rtcp_socket(const RtpSession *session){
	return rtp_session_using_transport(session, rtcp) ? (session->rtcp.gs.tr->t_getsocket)(session->rtcp.gs.tr) : session->rtcp.gs.socket;
}

/**
 * Register an event queue.
 * An application can use an event queue to get informed about various RTP events.
**/
void rtp_session_register_event_queue(RtpSession *session, OrtpEvQueue *q){
	session->eventqs=o_list_append(session->eventqs,q);
}

void rtp_session_unregister_event_queue(RtpSession *session, OrtpEvQueue *q){
	session->eventqs=o_list_remove(session->eventqs,q);
}

void rtp_session_dispatch_event(RtpSession *session, OrtpEvent *ev){
	OList *it;
	int i;
	for(i=0,it=session->eventqs;it!=NULL;it=it->next,++i){
		ortp_ev_queue_put((OrtpEvQueue*)it->data,ortp_event_dup(ev));
	}
	ortp_event_destroy(ev);
}

void ortp_stream_clear_aux_addresses(OrtpStream *os){
	OList *elem;
	for (elem=os->aux_destinations;elem!=NULL;elem=elem->next){
		OrtpAddress *addr=(OrtpAddress*)elem->data;
		ortp_free(addr);
	}
	os->aux_destinations=o_list_free(os->aux_destinations);
}

static void ortp_stream_uninit(OrtpStream *os){
	msgb_allocator_uninit(&os->allocator);
	ortp_stream_clear_aux_addresses(os);
}

void rtp_session_uninit (RtpSession * session)
{
	RtpTransport *rtp_meta_transport = NULL;
	RtpTransport *rtcp_meta_transport = NULL;
	/* first of all remove the session from the scheduler */
	if (session->flags & RTP_SESSION_SCHEDULED)
	{
		rtp_scheduler_remove_session (session->sched,session);
	}
	/*flush all queues */
	flushq(&session->rtp.rq, FLUSHALL);
	flushq(&session->rtp.tev_rq, FLUSHALL);

	if (session->eventqs!=NULL) o_list_free(session->eventqs);
	/* close sockets */
	rtp_session_release_sockets(session);

	wait_point_uninit(&session->snd.wp);
	wait_point_uninit(&session->rcv.wp);
	if (session->current_tev!=NULL) freemsg(session->current_tev);
	ortp_stream_uninit(&session->rtp.gs);
	ortp_stream_uninit(&session->rtcp.gs);
	if (session->full_sdes != NULL)
		freemsg(session->full_sdes);
	if (session->minimal_sdes != NULL)
		freemsg(session->minimal_sdes);
	bctbx_list_free_with_data(session->recv_addr_map, (bctbx_list_free_func)bctbx_free);

	session->signal_tables = o_list_free(session->signal_tables);

	if (session->net_sim_ctx)
		ortp_network_simulator_destroy(session->net_sim_ctx);

	rtp_session_get_transports(session,&rtp_meta_transport,&rtcp_meta_transport);
	if (rtp_meta_transport)
		meta_rtp_transport_destroy(rtp_meta_transport);
	if (rtcp_meta_transport)
		meta_rtp_transport_destroy(rtcp_meta_transport);

#if (_WIN32_WINNT >= 0x0600) && defined(ORTP_WINDOWS_DESKTOP)
	if (session->rtp.QoSFlowID != 0)
	{
		ortp_message("check OS support for qwave.lib");
		if (IsWindowsVistaOrGreater()) {
			BOOL QoSResult;
			QoSResult = QOSRemoveSocketFromFlow(session->rtp.QoSHandle, 0, session->rtp.QoSFlowID, 0);
			if (QoSResult != TRUE){
				ortp_error("QOSRemoveSocketFromFlow failed to end a flow with error %d", GetLastError());
			}
			session->rtp.QoSFlowID = 0;
		}
	}

	if (session->rtp.QoSHandle != NULL)
	{
		QOSCloseHandle(session->rtp.QoSHandle);
		session->rtp.QoSHandle=NULL;
	}
#endif
}

/**
 * Sets the number of packets containing a new SSRC that will trigger the
 * "ssrc_changed" callback.
**/
void rtp_session_set_ssrc_changed_threshold(RtpSession *session, int numpackets){
	session->rtp.ssrc_changed_thres=numpackets;
}

/**
 * Resynchronize to the incoming RTP streams.
 * This can be useful to handle discontinuous timestamps.
 * For example, call this function from the timestamp_jump signal handler.
 * @param session the rtp session
**/
void rtp_session_resync(RtpSession *session){
	int ptindex=rtp_session_get_recv_payload_type(session);
	PayloadType *pt=rtp_profile_get_payload(session->rcv.profile,ptindex);

	flushq (&session->rtp.rq, FLUSHALL);
	rtp_session_set_flag(session, RTP_SESSION_RECV_SYNC);
	rtp_session_unset_flag(session,RTP_SESSION_FIRST_PACKET_DELIVERED);
	jitter_control_init(&session->rtp.jittctl,-1,pt);

	/* Since multiple streams might share the same session (fixed RTCP port for example),
	RTCP values might be erroneous (number of packets received is computed
	over all streams, ...). There should be only one stream per RTP session*/
	session->rtp.hwrcv_extseq = 0;
	session->rtp.hwrcv_since_last_SR = 0;
	session->rtp.hwrcv_seq_at_last_SR = 0;
	rtp_session_unset_flag(session, RTP_SESSION_RECV_SEQ_INIT);
}

/**
 * Reset the session: local and remote addresses are kept. It resets timestamp, sequence
 * number, and calls rtp_session_resync().
 *
 * @param session a rtp session.
**/
void rtp_session_reset (RtpSession * session)
{
	rtp_session_set_flag (session, RTP_SESSION_RECV_NOT_STARTED);
	rtp_session_set_flag (session, RTP_SESSION_SEND_NOT_STARTED);
	//session->ssrc=0;
	session->rtp.snd_time_offset = 0;
	session->rtp.snd_ts_offset = 0;
	session->rtp.snd_rand_offset = 0;
	session->rtp.snd_last_ts = 0;
	session->rtp.rcv_time_offset = 0;
	session->rtp.rcv_ts_offset = 0;
	session->rtp.rcv_query_ts_offset = 0;
	session->rtp.rcv_last_ts = 0;
	session->rtp.rcv_last_seq = 0;
	session->rtp.rcv_last_app_ts = 0;
	session->rtp.hwrcv_extseq = 0;
	session->rtp.hwrcv_since_last_SR=0;
	session->rtp.snd_seq = 0;
	session->rtp.sent_payload_bytes=0;
	rtp_session_clear_send_error_code(session);
	rtp_session_clear_recv_error_code(session);
	rtp_stats_reset(&session->stats);
	rtp_session_resync(session);
	session->ssrc_set=FALSE;
}

/**
 * Retrieve the session's statistics.
**/
const rtp_stats_t * rtp_session_get_stats(const RtpSession *session){
	return &session->stats;
}

/**
 * Retrieves the session's jitter specific statistics.
**/
const jitter_stats_t * rtp_session_get_jitter_stats( const RtpSession *session ) {
	return &session->rtp.jitter_stats;
}

/**
 * @brief For <b>test purpose only</b>, sets a constant lost packet value within <b>all</b> RTCP output packets.@n
 *
 * The SR or RR RTCP packet contain a lost packet field. After this procedure is called, the lost packet field will be set to a constant value in all output SR or RR packets. This parameter will overridden the actual number of lost packets in the input RTP stream that the RTCP stack had previously processed.
 * @param s : the rtp session.
 * @param value : the lost packets test vector value.
**/
void rtp_session_rtcp_set_lost_packet_value( struct _RtpSession *s, const int value ) {
	s->lost_packets_test_vector = value;
	s->flags|=RTCP_OVERRIDE_LOST_PACKETS;
}

/**
 * @brief For <b>test purpose only</b>, sets a constant interarrival_jitter value within <b>all</b> RTCP output packets.@n
 *
 * The SR or RR RTCP packet contain an interarrival jitter field. After this procedure is called, the interarrival jitter field will be set to a constant value in all output SR or RR packets. This parameter will overridden the actual interarrival jitter value that was processed by the RTCP stack.
 * @param s : the rtp session.
 * @param value : the interarrival jitter test vector value.
**/
void rtp_session_rtcp_set_jitter_value( struct _RtpSession *s, const unsigned int value ) {
	s->interarrival_jitter_test_vector = value;
	s->flags|=RTCP_OVERRIDE_JITTER;
}

/**
 * @brief For <b>test purpose only</b>, simulates a constant RTT (Round Trip Time) value by setting the LSR field within <b>all</b> returned RTCP output packets.@n
 *
 * The RTT processing involves two RTCP packets exchanged between two different devices.@n
 * In a <b>normal</b> operation the device 1 issues a SR packets at time T0, hence this packet has a timestamp field set to T0.
 * The LSR and DLSR fiels of that packet are not considered here. This packet is received by the Device 2 at T1.
 * In response, the Device 2 issues another SR or RR packets at T2 with the following fields;
 * - a timestamp set to T2.
 * - a LSR (Last SR packet timestamp) field set to T0 ( this value has been extracted from the first packet).
 * - a DLSR (Delay since Last SR packet) field set to (T2 - T1).
 * .
 * This packet is received by The Device 1 at T3. So the Device 1 is now able to process the RTT using the formula :
 * RTT = T3 - LSR - DLSR = (T1 - T0) - (T3 - T2).@n
 * This way of processing is described in par. 6.4 of the RFC3550 standard.
 *
 * In the <b>test</b> mode that is enabled by this procedure, the RTCP stack is considered as beeing part of the device 2. For setting the RTT to a constant RTT0 value, the Device 2 artificially sets the LSR field of the second packet to (T1 - RTT0), instead of T0 in normal mode. The two other fields (timestamp and DLSR) are set as in the normal mode. So the Device 1 will process :
 * RTT = T3 - LSR - DLSR = RTT0 + (T3 - T2) that is near to RTT0 is T3 - T2 is small enough.
 * @note It is impossible to actually make the mesured RTT strictly equal to RTT0, as the packet trip time (T3 - T2) is unknown when this packet is issued by the Device 2.
 * @param s : the rtp session.
 * @param value : The desired RTT test vector value (RTT0).
**/
void rtp_session_rtcp_set_delay_value( struct _RtpSession *s, const unsigned int value ) {
	s->delay_test_vector= value;
	s->flags|=RTCP_OVERRIDE_DELAY;
}

void rtp_session_reset_stats(RtpSession *session){
	memset(&session->stats,0,sizeof(rtp_stats_t));
}

/**
 * Stores some application specific data into the session, so that it is easy to retrieve it from the signal callbacks using rtp_session_get_data().
 * @param session a rtp session
 * @param data an opaque pointer to be stored in the session
**/

void rtp_session_set_data(RtpSession *session, void *data){
	session->user_data=data;
}

/**
 * @param session a rtp session
 * @return the void pointer previously set using rtp_session_set_data()
**/
void *rtp_session_get_data(const RtpSession *session){
	return session->user_data;
}

/**
 * Enable or disable the "rtp symmetric" hack which consists of the following:
 * after the first packet is received, the source address of the packet
 * is set to be the destination address for all next packets.
 * This is useful to pass-through firewalls.
 * @param session a rtp session
 * @param yesno a boolean to enable or disable the feature
 *
**/
void
rtp_session_set_symmetric_rtp (RtpSession * session, bool_t yesno)
{
	session->symmetric_rtp =yesno;
}

void rtp_session_enable_rtcp_mux(RtpSession *session, bool_t yesno){
	session->rtcp_mux = yesno;
}

bool_t rtp_session_rtcp_mux_enabled(RtpSession *session){
	return session->rtcp_mux;
}

/**
 *	If yesno is TRUE, thus a connect() syscall is done on the socket to
 *	the destination address set by rtp_session_set_remote_addr(), or
 *	if the session does symmetric rtp (see rtp_session_set_symmetric_rtp())
 *	a the connect() is done to the source address of the first packet received.
 *	Connecting a socket has effect of rejecting all incoming packets that
 *	don't come from the address specified in connect().
 *	It also makes ICMP errors (such as connection refused) available to the
 *	application.
 *	@param session a rtp session
 *	@param yesno a boolean to enable or disable the feature
 *
**/
void rtp_session_set_connected_mode(RtpSession *session, bool_t yesno){
	session->use_connect=yesno;
}

static float compute_bw(struct timeval *orig, unsigned int bytes, const struct timeval *current){
	float bw;
	float time;

	time=(float)((double)(current->tv_sec - orig->tv_sec) +
		((double)(current->tv_usec - orig->tv_usec)*1e-6));
	bw=((float)bytes)*8/(time+0.0001f);
	/*+0.0001 avoids a division by zero without changing the results significatively*/
	*orig=*current;
	return bw;
}

static void compute_recv_bandwidth(OrtpStream *os, const struct timeval *current) {
	os->download_bw = compute_bw(&os->recv_bw_start, os->recv_bytes, current);
	os->recv_bytes = 0;
}

static void compute_send_bandwidth(OrtpStream *os, const struct timeval *current) {
	os->upload_bw = compute_bw(&os->send_bw_start, os->sent_bytes, current);
	os->sent_bytes = 0;
}

float rtp_session_compute_recv_bandwidth(RtpSession *session) {
	struct timeval current;
	ortp_gettimeofday(&current,NULL);

	compute_recv_bandwidth(&session->rtp.gs, &current);
	compute_recv_bandwidth(&session->rtcp.gs, &current);
	return session->rtp.gs.download_bw + session->rtcp.gs.download_bw;
}

float rtp_session_compute_send_bandwidth(RtpSession *session) {
	struct timeval current;
	ortp_gettimeofday(&current,NULL);

	compute_send_bandwidth(&session->rtp.gs, &current);
	compute_send_bandwidth(&session->rtcp.gs, &current);
	return session->rtp.gs.upload_bw + session->rtcp.gs.upload_bw;
}

/**
 * Get last computed recv bandwidth.
 * Computation must have been done with rtp_session_compute_recv_bandwidth()
**/
float rtp_session_get_recv_bandwidth(RtpSession *session){
	return session->rtp.gs.download_bw + session->rtcp.gs.download_bw;
}

/**
 * Get last computed send bandwidth.
 * Computation must have been done with rtp_session_compute_send_bandwidth()
**/
float rtp_session_get_send_bandwidth(RtpSession *session){
	return session->rtp.gs.upload_bw + session->rtcp.gs.upload_bw;
}

float rtp_session_get_rtp_recv_bandwidth(RtpSession *session) {
	return session->rtp.gs.download_bw;
}

float rtp_session_get_rtp_send_bandwidth(RtpSession *session) {
	return session->rtp.gs.upload_bw;
}

float rtp_session_get_rtcp_recv_bandwidth(RtpSession *session) {
	return session->rtcp.gs.download_bw;
}

float rtp_session_get_rtcp_send_bandwidth(RtpSession *session) {
	return session->rtcp.gs.upload_bw;
}

int rtp_session_get_last_send_error_code(RtpSession *session){
	return session->rtp.send_errno;
}

void rtp_session_clear_send_error_code(RtpSession *session){
	session->rtp.send_errno=0;
}

int rtp_session_get_last_recv_error_code(RtpSession *session){
	return session->rtp.recv_errno;
}

void rtp_session_clear_recv_error_code(RtpSession *session){
	session->rtp.send_errno=0;
}

/**
 * Returns the last known round trip propagation delay.
 *
 * This value is known after successful RTCP SR or RR exchanged between a sender and a receiver.
 * oRTP automatically takes care of sending SR or RR packets.
 * You might want to call this function when you receive an RTCP event (see rtp_session_register_event_queue() ).
 * This value might not be known: at the beginning when no RTCP packets have been exchanged yet, or simply because the
 * rtcp channel is broken due to firewall problematics, or because the remote implementation does not support RTCP.
 *
 * @return the round trip propagation time in seconds if known, -1 if unknown.
**/
float rtp_session_get_round_trip_propagation(RtpSession *session){
	return session->rtt;
}

/**
 * Destroys a rtp session.
 * All memory allocated for the RtpSession is freed.
 *
 * @param session a rtp session.
**/
void rtp_session_destroy (RtpSession * session)
{
	rtp_session_uninit (session);
	ortp_free (session);
}

void rtp_session_make_time_distorsion(RtpSession *session, int milisec)
{
	session->rtp.snd_time_offset+=milisec;
}


/* packet api */

void rtp_add_csrc(mblk_t *mp, uint32_t csrc)
{
	rtp_header_t *hdr=(rtp_header_t*)mp->b_rptr;
	hdr->csrc[hdr->cc]=csrc;
	hdr->cc++;
}

/**
 * Get a pointer to the beginning of the payload data of the RTP packet.
 * @param packet a RTP packet represented as a mblk_t
 * @param start a pointer to the beginning of the payload data, pointing inside the packet.
 * @return the length of the payload data.
**/
int rtp_get_payload(mblk_t *packet, unsigned char **start){
	unsigned char *tmp;
	int header_len=RTP_FIXED_HEADER_SIZE+(rtp_get_cc(packet)*4);
	tmp=packet->b_rptr+header_len;
	if (tmp>packet->b_wptr){
		if (packet->b_cont!=NULL){
			tmp=packet->b_cont->b_rptr+(header_len- (packet->b_wptr-packet->b_rptr));
			if (tmp<=packet->b_cont->b_wptr){
				*start=tmp;
				return (int)(packet->b_cont->b_wptr-tmp);
			}
		}
		ortp_warning("Invalid RTP packet");
		return -1;
	}
	if (rtp_get_extbit(packet)){
		int extsize=rtp_get_extheader(packet,NULL,NULL);
		if (extsize>=0){
			tmp+=4+extsize;
		}
	}
	*start=tmp;
	return (int)(packet->b_wptr-tmp);
}

/**
 * Obtain the extension header if any.
 * @param packet the RTP packet.
 * @param profile the profile field of the extension header
 * @param start_ext pointer that will be set to the beginning of the payload of the extension header.
 * @return the size of the extension in bytes (the payload size, it can be 0), -1 if parsing of the extension header failed or if no extension is present.
**/
int rtp_get_extheader(mblk_t *packet, uint16_t *profile, uint8_t **start_ext){
	int size=0;
	uint8_t *ext_header;
	if (rtp_get_extbit(packet)){
		ext_header=packet->b_rptr+RTP_FIXED_HEADER_SIZE+(rtp_get_cc(packet)*4);
		if (ext_header+4 <= packet->b_wptr){
			uint32_t h=ntohl(*(uint32_t*)ext_header);
			size=(int)(h & 0xFFFF);
			if (profile) *profile=(h>>16);
			size=(size*4); /*the size is given in the packet as multiple of 32 bit words, excluding the 4 byte header*/
			if ((ext_header+4+size)> packet->b_wptr){
				ortp_warning("Inconsistent size for rtp extension header");
				return -1;
			}
			if (start_ext) *start_ext=ext_header+4;
			return size;
		}else{
			ortp_warning("Insufficient size for rtp extension header.");
			return -1;
		}
	}
	return -1;
}


/**
 *  Gets last time a valid RTP or RTCP packet was received.
 * @param session RtpSession to get last receive time from.
 * @param tv Pointer to struct timeval to fill.
 *
**/
void
rtp_session_get_last_recv_time(RtpSession *session, struct timeval *tv)
{
#ifdef PERF
	ortp_error("rtp_session_get_last_recv_time() feature disabled.");
#else
	*tv = session->last_recv_time;
#endif
}



uint32_t rtp_session_time_to_ts(RtpSession *session, int millisecs){
	PayloadType *payload;
	payload =
		rtp_profile_get_payload (session->snd.profile,
					 session->snd.pt);
	if (payload == NULL)
	{
		ortp_warning
			("rtp_session_time_to_ts: use of unsupported payload type %d.", session->snd.pt);
		return 0;
	}
	/* the return value is in milisecond */
	return (uint32_t) (payload->clock_rate*(double) (millisecs/1000.0f));
}

/* function used by the scheduler only:*/
uint32_t rtp_session_ts_to_time (RtpSession * session, uint32_t timestamp)
{
	PayloadType *payload;
	payload =
		rtp_profile_get_payload (session->snd.profile,
					 session->snd.pt);
	if (payload == NULL)
	{
		ortp_warning
			("rtp_session_ts_to_t: use of unsupported payload type %d.", session->snd.pt);
		return 0;
	}
	/* the return value is in milisecond */
	return (uint32_t) (1000.0 *
			  ((double) timestamp /
			   (double) payload->clock_rate));
}


/* time is the number of miliseconds elapsed since the start of the scheduler */
void rtp_session_process (RtpSession * session, uint32_t time, RtpScheduler *sched)
{
	wait_point_lock(&session->snd.wp);
	if (wait_point_check(&session->snd.wp,time)){
		session_set_set(&sched->w_sessions,session);
		wait_point_wakeup(&session->snd.wp);
	}
	wait_point_unlock(&session->snd.wp);

	wait_point_lock(&session->rcv.wp);
	if (wait_point_check(&session->rcv.wp,time)){
		session_set_set(&sched->r_sessions,session);
		wait_point_wakeup(&session->rcv.wp);
	}
	wait_point_unlock(&session->rcv.wp);
}

void rtp_session_set_reuseaddr(RtpSession *session, bool_t yes) {
	session->reuseaddr=yes;
}


typedef struct _MetaRtpTransportImpl{
	RtpTransport *other_meta_rtp; /*pointer to the "other" meta RtpTransport, that is the RTCP transport if we are RTP, and the RTP transport
		if we are RTCP. This is used only for RTCP-mux*/
	OList *modifiers;
	RtpTransport *endpoint;
	bool_t is_rtp;
	bool_t has_set_session;
} MetaRtpTransportImpl;

ortp_socket_t meta_rtp_transport_getsocket(RtpTransport *t) {
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;

	if (m->endpoint!=NULL){
		return m->endpoint->t_getsocket(m->endpoint);
	}
	return (m->is_rtp ? t->session->rtp.gs.socket : t->session->rtcp.gs.socket);
}

void meta_rtp_set_session(RtpSession *s,MetaRtpTransportImpl *m){
	OList *elem;
	/*if session has not been set yet, do nothing*/
	if (s==NULL){
		return;
	}

	if (m->endpoint!=NULL){
		m->endpoint->session=s;
	}
	for (elem=m->modifiers;elem!=NULL;elem=o_list_next(elem)){
		RtpTransportModifier *rtm=(RtpTransportModifier*)elem->data;
		rtm->session=s;
	}
	m->has_set_session=TRUE;
}

static int _meta_rtp_transport_send_through_endpoint(RtpTransport *t, mblk_t *msg , int flags, const struct sockaddr *to, socklen_t tolen){
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;
	int ret;

	if (m->endpoint!=NULL){
		ret=m->endpoint->t_sendto(m->endpoint, msg, flags, to, tolen);
	}else{
		ret=rtp_session_sendto(t->session, m->is_rtp,msg,flags,to,tolen);
	}
	return ret;
}

int meta_rtp_transport_sendto(RtpTransport *t, mblk_t *msg , int flags, const struct sockaddr *to, socklen_t tolen) {
	size_t prev_ret;
	int ret = 0;
	OList *elem;
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;

	if (!m->has_set_session){
		meta_rtp_set_session(t->session,m);
	}
	prev_ret=msgdsize(msg);
	for (elem=m->modifiers;elem!=NULL;elem=o_list_next(elem)){
		RtpTransportModifier *rtm=(RtpTransportModifier*)elem->data;
		ret = rtm->t_process_on_send(rtm,msg);

		if (ret<=0){
			// something went wrong in the modifier (failed to encrypt for instance)
			return ret;
		}
		msg->b_wptr+=(ret-prev_ret);
		prev_ret=ret;
	}
	if (!m->is_rtp && t->session->rtcp_mux){ /*if this meta transport is handling RTCP and using rtcp-mux, then we should expedite the packet
		through the rtp endpoint.*/
		if (m->other_meta_rtp){
			ret = _meta_rtp_transport_send_through_endpoint(m->other_meta_rtp, msg, flags, to, tolen);
		}else{
			ortp_error("meta_rtp_transport_sendto(): rtcp-mux enabled but no RTP meta transport is specified !");
		}
	} else ret = _meta_rtp_transport_send_through_endpoint(t, msg, flags, to, tolen);
	return ret;
}

/**
 * allow a modifier to inject a packet which will be treated by successive modifiers
 */
int meta_rtp_transport_modifier_inject_packet_to_send(RtpTransport *t, RtpTransportModifier *tpm, mblk_t *msg, int flags) {
	struct sockaddr *to;
	socklen_t tolen;
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;

	if (!m->has_set_session){
		meta_rtp_set_session(t->session,m);
	}

	/* get back socket from transport session */
	if (m->is_rtp) {
		to=(struct sockaddr*)&t->session->rtp.gs.rem_addr;
		tolen=t->session->rtp.gs.rem_addrlen;
	} else {
		to = (struct sockaddr*)&t->session->rtcp.gs.rem_addr;
		tolen = t->session->rtcp.gs.rem_addrlen;
	}
	return meta_rtp_transport_modifier_inject_packet_to_send_to(t, tpm, msg, flags, to, tolen);
}

/**
 * allow a modifier to inject a packet which will be treated by successive modifiers
 */
int meta_rtp_transport_modifier_inject_packet_to_send_to(RtpTransport *t, RtpTransportModifier *tpm, mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen) {
	size_t prev_ret;
	int ret;
	bool_t foundMyself = tpm ? FALSE : TRUE; /*if no modifier, start from the beginning*/
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;
	OList *elem = m->modifiers;

	if (!m->has_set_session) {
		meta_rtp_set_session(t->session, m);
	}

	prev_ret = msgdsize(msg);
	for (;elem != NULL; elem = o_list_next(elem)) {
		/* run modifiers only after packet injection, the modifier given in parameter is not applied */
		RtpTransportModifier *rtm = (RtpTransportModifier*)elem->data;
		if (foundMyself == TRUE) {
			ret = rtm->t_process_on_send(rtm, msg);

			if (ret <= 0) {
				// something went wrong in the modifier (failed to encrypt for instance)
				return ret;
			}
			msg->b_wptr += (ret - prev_ret);
			prev_ret = ret;
		}

		/* check if we must inject the packet */
		if (rtm == tpm) {
			foundMyself = TRUE;
		}
	}

	if (m->endpoint != NULL) {
		ret = m->endpoint->t_sendto(m->endpoint, msg, flags, to, tolen);
	} else {
		ret = rtp_session_sendto(t->session, m->is_rtp, msg, flags, to, tolen);
	}
	update_sent_bytes(&t->session->rtp.gs, ret);
	return ret;
}

static int _meta_rtp_transport_recv_through_modifiers(RtpTransport *t, RtpTransportModifier *tpm, mblk_t *msg, int flags){
	int ret = 0;
	size_t prev_ret;
	bool_t foundMyself = tpm ? FALSE : TRUE; /*if no modifier, start from the beginning*/
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;
	OList *elem = m->modifiers;
	OList *last_elem = NULL;

	for (;elem != NULL; elem = o_list_next(elem)) {
		last_elem = elem;
	}

	prev_ret = msgdsize(msg);
	ret = (int)prev_ret;
	for (;last_elem != NULL; last_elem = o_list_prev(last_elem)) {
		/* run modifiers only after packet injection, the modifier given in parameter is not applied */
		RtpTransportModifier *rtm = (RtpTransportModifier*)last_elem->data;
		if (foundMyself == TRUE) {
			ret = rtm->t_process_on_receive(rtm, msg);
			if (ret < 0) {
				// something went wrong in the modifier (failed to decrypt for instance)
				break;
			}
			msg->b_wptr += ((size_t)ret - prev_ret);
			prev_ret = (size_t)ret;
		}

		/* check if we must inject the packet */
		if (rtm == tpm) {
			foundMyself = TRUE;
		}
	}
	return ret;
}

/**
 * allow a modifier to inject a packet which will be treated by successive modifiers
 */
int meta_rtp_transport_modifier_inject_packet_to_recv(RtpTransport *t, RtpTransportModifier *tpm, mblk_t *msg, int flags) {
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;
	int ret = _meta_rtp_transport_recv_through_modifiers(t, tpm, msg, flags);
	rtp_session_process_incoming(t->session, msg, m->is_rtp, msg->reserved1, FALSE);
	return ret;
}


int meta_rtp_transport_recvfrom(RtpTransport *t, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) {
	int ret;
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;
	OList *elem;
	bool_t received_via_rtcp_mux = FALSE;

	if (!m->has_set_session){
		meta_rtp_set_session(t->session,m);
	}

	/*invoke on schedule on every modifier first, regardless of if a packet is actually received.*/
	for (elem=m->modifiers;elem!=NULL;elem=o_list_next(elem)){
		RtpTransportModifier *rtm=(RtpTransportModifier*)elem->data;

		if (rtm->t_process_on_schedule) rtm->t_process_on_schedule(rtm);
	}

	if (m->endpoint!=NULL){
		ret=m->endpoint->t_recvfrom(m->endpoint,msg,flags,from,fromlen);
		if (ret > 0) {
			/*store recv addr for use by modifiers*/
			if (from && fromlen) {
				memcpy(&msg->net_addr,from,*fromlen);
				msg->net_addrlen = *fromlen;
			}
		}
	}else{
		ret=rtp_session_recvfrom(t->session,m->is_rtp,msg,flags,from,fromlen);
	}

	if (ret <= 0){
		return ret;
	}
	msg->b_wptr+=ret;

	/*in case of rtcp-mux, we are allowed to reconsider whether it is an RTP or RTCP packet*/
	if (t->session->rtcp_mux && m->is_rtp){
		if (ret >= RTP_FIXED_HEADER_SIZE && rtp_get_version(msg) == 2){
			int pt = rtp_get_payload_type(msg);
			if (pt >= 64 && pt <= 95){
				/*this is assumed to be an RTCP packet*/
				received_via_rtcp_mux = TRUE;
			}
		}
	}

	if (received_via_rtcp_mux){
		if (m->other_meta_rtp){
			_meta_rtp_transport_recv_through_modifiers(m->other_meta_rtp, NULL, msg, flags);
			rtp_session_process_incoming(t->session, dupmsg(msg),FALSE, msg->reserved1, received_via_rtcp_mux);
			ret = 0; /*since we directly inject in the RtpSession this RTCP packet, we shall return 0 and pass a duplicate of the message,
			because rtp_session_rtp_recv() is going to free it.*/
		}else{
			ortp_error("RTCP packet received via rtcp-mux but RTCP transport is not set !");
		}
	}else ret = _meta_rtp_transport_recv_through_modifiers(t, NULL, msg, flags);

	// subtract last written value since it will be rewritten by rtp_session_rtp_recv
	msg->b_wptr-= ret;

	return ret;
}

void  meta_rtp_transport_close(RtpTransport *t) {
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)t->data;
	if (m->endpoint!=NULL){
		m->endpoint->t_close(m->endpoint);
	}
}

static RtpTransport* _meta_rtp_transport_new(bool_t is_rtp, RtpTransport *endpoint, unsigned modifiers_count, va_list arguments) {
	MetaRtpTransportImpl *m;
	RtpTransport *t = ortp_new0(RtpTransport,1);
	m=t->data=ortp_new0(MetaRtpTransportImpl,1);

	t->t_getsocket=meta_rtp_transport_getsocket;
	t->t_sendto=meta_rtp_transport_sendto;
	t->t_recvfrom=meta_rtp_transport_recvfrom;
	t->t_close=meta_rtp_transport_close;
	t->t_destroy=meta_rtp_transport_destroy;

	m->is_rtp = is_rtp;
	m->endpoint=endpoint;
	while (modifiers_count != 0){
		m->modifiers=o_list_append(m->modifiers, va_arg(arguments,RtpTransportModifier*));
		modifiers_count--;
	}

	return t;
}

RtpTransport* meta_rtp_transport_new(RtpTransport *endpoint, unsigned modifiers_count, ...){
	RtpTransport *tr;
	va_list args;
	va_start (args, modifiers_count);
	tr = _meta_rtp_transport_new(TRUE, endpoint, modifiers_count, args);
	va_end(args);
	return tr;
}

RtpTransport* meta_rtcp_transport_new(RtpTransport *endpoint, unsigned modifiers_count, ...){
	RtpTransport *tr;
	va_list args;
	va_start (args, modifiers_count);
	tr = _meta_rtp_transport_new(FALSE, endpoint, modifiers_count, args);
	va_end(args);
	return tr;
}

/*this links both meta rtp transport, which is necessary for rtcp-mux to work*/
void meta_rtp_transport_link(RtpTransport *rtp, RtpTransport *rtcp){
	MetaRtpTransportImpl *mrtp = (MetaRtpTransportImpl*)rtp->data;
	MetaRtpTransportImpl *mrtcp = (MetaRtpTransportImpl*)rtcp->data;
	mrtp->other_meta_rtp = rtcp;
	mrtcp->other_meta_rtp = rtp;
}

RtpTransport* meta_rtp_transport_get_endpoint(const RtpTransport *transport) {
	return transport->data ? ((MetaRtpTransportImpl *)transport->data)->endpoint:NULL;
}

void meta_rtp_transport_set_endpoint(RtpTransport *transport,RtpTransport *endpoint) {
	if (transport->data) {
		((MetaRtpTransportImpl *)transport->data)->endpoint=endpoint;
	} else
		ortp_error("Cannot set endpoint [%p] on rtp transport [%p]",transport,endpoint);
}

void meta_rtp_transport_destroy(RtpTransport *tp) {
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)tp->data;
	OList *elem;

	if (m->endpoint!=NULL){
		m->endpoint->t_destroy(m->endpoint);
	}

	for (elem=m->modifiers;elem!=NULL;elem=o_list_next(elem)){
		RtpTransportModifier *rtm=(RtpTransportModifier*)elem->data;
		rtm->transport = NULL;
		rtm->t_destroy(rtm);
	}
	o_list_free(m->modifiers);

	ortp_free(m);
	ortp_free(tp);
}

void meta_rtp_transport_append_modifier(RtpTransport *tp,RtpTransportModifier *tpm) {
	MetaRtpTransportImpl *m = (MetaRtpTransportImpl*)tp->data;
	tpm->transport = tp;
	m->modifiers=o_list_append(m->modifiers, tpm);
	if(m->has_set_session) {
		tpm->session = tp->session;
	}
}
bool_t rtp_session_get_symmetric_rtp(const RtpSession *session) {
	return session->symmetric_rtp;
}

int rtp_session_splice(RtpSession *session, RtpSession *to_session){
	if (session->spliced_session){
		ortp_error("rtp_session_splice(): session %p already splicing into session %p", session, session->spliced_session);
		return -1;
	}
	session->spliced_session = to_session;
	to_session->is_spliced = TRUE;
	ortp_message("rtp_session_splice(): session %p splicing to %p", session, to_session);
	return 0;
}

int rtp_session_unsplice(RtpSession *session, RtpSession *to_session){
	if (session->spliced_session != to_session){
		ortp_error("rtp_session_unsplice() session %p is not spliced to session %p", session, to_session);
		return -1;
	}
	session->spliced_session = NULL;
	to_session->is_spliced = FALSE;
	ortp_message("rtp_session_unsplice(): session %p no longer splicing to %p", session, to_session);
	return 0;
}

/*
 * send packet through the peered session, the mblk_t is not freed.
**/
void rtp_session_do_splice(RtpSession *session, mblk_t *packet, bool_t is_rtp){
	RtpSession *peer = session->spliced_session;
	if (peer){
		OrtpStream *os = is_rtp ? &peer->rtp.gs : &peer->rtcp.gs;
		_ortp_sendto(os->socket, packet, 0, (struct sockaddr*)&os->rem_addr, os->rem_addrlen);
	}
}

bool_t ortp_stream_is_ipv6(OrtpStream *os) {
	if (os->sockfamily == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&os->rem_addr;
		return !IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr);
	}
	return FALSE;
}
