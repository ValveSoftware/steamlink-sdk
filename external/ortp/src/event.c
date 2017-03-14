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

#include "ortp/event.h"
#include "ortp/ortp.h"
#include "ortp/rtpsession.h"
#include "utils.h"

OrtpEvent * ortp_event_new(unsigned long type){
	OrtpEventData *ed;
	const int size=sizeof(OrtpEventType)+sizeof(OrtpEventData);
	mblk_t *m=allocb(size,0);
	memset(m->b_wptr,0,size);
	*((OrtpEventType*)m->b_wptr)=type;
	ed = ortp_event_get_data(m);
	ortp_get_cur_time(&ed->ts);
	return m;
}

OrtpEvent *ortp_event_dup(OrtpEvent *ev){
	OrtpEvent *nev = ortp_event_new(ortp_event_get_type(ev));
	OrtpEventData * ed = ortp_event_get_data(ev);
	OrtpEventData * edv = ortp_event_get_data(nev);
	memcpy(edv,ed,sizeof(OrtpEventData));
	if (ed->packet) edv->packet = copymsg(ed->packet);
	return nev;
}

OrtpEventType ortp_event_get_type(const OrtpEvent *ev){
	return ((OrtpEventType*)ev->b_rptr)[0];
}

OrtpEventData * ortp_event_get_data(OrtpEvent *ev){
	return (OrtpEventData*)(ev->b_rptr+sizeof(OrtpEventType));
}

void ortp_event_destroy(OrtpEvent *ev){
	OrtpEventData *d=ortp_event_get_data(ev);
	if (ev->b_datap->db_ref==1){
		if (d->packet) 	freemsg(d->packet);
	}
	freemsg(ev);
}

OrtpEvQueue * ortp_ev_queue_new(){
	OrtpEvQueue *q=ortp_new(OrtpEvQueue,1);
	qinit(&q->q);
	ortp_mutex_init(&q->mutex,NULL);
	return q;
}

void ortp_ev_queue_flush(OrtpEvQueue * qp){
	OrtpEvent *ev;
	while((ev=ortp_ev_queue_get(qp))!=NULL){
		ortp_event_destroy(ev);
	}
}

OrtpEvent * ortp_ev_queue_get(OrtpEvQueue *q){
	OrtpEvent *ev;
	ortp_mutex_lock(&q->mutex);
	ev=getq(&q->q);
	ortp_mutex_unlock(&q->mutex);
	return ev;
}

void ortp_ev_queue_destroy(OrtpEvQueue * qp){
	ortp_ev_queue_flush(qp);
	ortp_mutex_destroy(&qp->mutex);
	ortp_free(qp);
}

void ortp_ev_queue_put(OrtpEvQueue *q, OrtpEvent *ev){
	ortp_mutex_lock(&q->mutex);
	putq(&q->q,ev);
	ortp_mutex_unlock(&q->mutex);
}

static bool_t rtcp_is_type(const mblk_t *m, rtcp_type_t type){
	const rtcp_common_header_t *ch=rtcp_get_common_header(m);
	return (ch!=NULL && rtcp_common_header_get_packet_type(ch)==type);
}

OrtpEvDispatcher * ortp_ev_dispatcher_new(RtpSession* session) {
	OrtpEvDispatcher *d=ortp_new(OrtpEvDispatcher,1);
	d->session = session;
	d->q = ortp_ev_queue_new();
	rtp_session_register_event_queue(session, d->q);
	d->cbs = NULL;

	return d;
}

void ortp_ev_dispatcher_destroy(OrtpEvDispatcher *d) {
	OList* it;
	for (it=d->cbs;it!=NULL;it=it->next){
		ortp_free(it->data);
	}
	o_list_free(d->cbs);
	rtp_session_unregister_event_queue(d->session, d->q);
	ortp_ev_queue_destroy(d->q);
	ortp_free(d);
}

static bool_t is_rtcp_event(OrtpEventType type) {
	return (type == ORTP_EVENT_RTCP_PACKET_RECEIVED || type == ORTP_EVENT_RTCP_PACKET_EMITTED);
}

static void iterate_cbs(OrtpEvDispatcher *disp, OrtpEvent *ev) {
	OrtpEventData *d = ortp_event_get_data(ev);
	do {
		/*for each packet part, if ANY iterate through the whole callback list to see if
		anyone is interested in it*/
		OrtpEventData *d = ortp_event_get_data(ev);
		OList* it;
		OrtpEventType evt = ortp_event_get_type(ev);
		for (it = disp->cbs; it != NULL; it = it->next){
			OrtpEvDispatcherData *data = (OrtpEvDispatcherData *)it->data;
			/*
			const rtcp_common_header_t *ch = rtcp_get_common_header(d->packet);
			rtcp_type_t packet_type = 0;
			if (ch != NULL) {
				packet_type = rtcp_common_header_get_packet_type(ch);
			}
			*/
			if (evt == data->type) {
				if (!is_rtcp_event(data->type) || rtcp_is_type(d->packet, data->subtype)) {
					data->on_found(d, data->user_data);
				}
			}
		}
	} while (d->packet!=NULL && rtcp_next_packet(d->packet));
}

void ortp_ev_dispatcher_iterate(OrtpEvDispatcher *d) {
	OrtpEvent *ev = NULL;
	while ((ev = ortp_ev_queue_get(d->q)) != NULL) {
		iterate_cbs(d, ev);
		ortp_event_destroy(ev);
	}
}

void ortp_ev_dispatcher_connect(OrtpEvDispatcher *d
								, OrtpEventType type
								, rtcp_type_t subtype
								, OrtpEvDispatcherCb cb
								, void *user_data) {
	OrtpEvDispatcherData *data=ortp_new(OrtpEvDispatcherData,1);
	data->type = type;
	data->subtype = subtype;
	data->on_found = cb;
	data->user_data = user_data;
	d->cbs = o_list_append(d->cbs, data);
}

void ortp_ev_dispatcher_disconnect(OrtpEvDispatcher *d
								, OrtpEventType type
								, rtcp_type_t subtype
								, OrtpEvDispatcherCb cb) {
	OList *it = d->cbs;
	while (it) {
		OrtpEvDispatcherData *data = (OrtpEvDispatcherData*)it->data;
		if (data && data->type == type && data->subtype == subtype && data->on_found == cb) {
			OList *tofree = it;
			it = it->next;
			ortp_free(data);
			d->cbs = o_list_remove_link(d->cbs, tofree);
		} else {
			it = it->next;
		}
	}
}
