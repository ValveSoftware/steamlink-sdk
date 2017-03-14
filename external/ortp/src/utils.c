/***************************************************************************
 *            utils.c
 *
 *  Wed Feb 23 14:15:36 2005
 *  Copyright  2005  Simon Morlat
 *  Email simon.morlat@linphone.org
 ****************************************************************************/
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

#include "ortp/port.h"
#include "utils.h"

OList *o_list_new(void *data){
	OList *new_elem=(OList*)ortp_new0(OList,1);
	new_elem->data=data;
	return new_elem;
}

OList * o_list_prepend(OList *elem, void * data){
	OList *new_elem=o_list_new(data);
	if (elem){
		elem->prev = new_elem;
		new_elem->next = elem;
	}
	return new_elem;
}

OList * o_list_append(OList *elem, void * data){
	OList *new_elem=o_list_new(data);
	OList *it=elem;
	if (elem==NULL) return new_elem;
	while (it->next!=NULL) it=o_list_next(it);
	it->next=new_elem;
	new_elem->prev=it;
	return elem;
}

OList * o_list_free(OList *list){
	OList *elem = list;
	OList *tmp;
	if (!list) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		ortp_free(tmp);
	}
	ortp_free(elem);
	return NULL;
}

OList * o_list_free_with_data(OList *list, void (*freefunc)(void*)){
	OList *elem = list;
	OList *tmp;
	if (!list) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		freefunc(tmp->data);
		ortp_free(tmp);
	}
	freefunc(elem->data);
	ortp_free(elem);
	return NULL;
}

OList *o_list_remove_link(OList *list, OList *elem){
	OList *ret;
	if (elem==list){
		ret=elem->next;
		elem->prev=NULL;
		elem->next=NULL;
		if (ret!=NULL) ret->prev=NULL;
		ortp_free(elem);
		return ret;
	}
	elem->prev->next=elem->next;
	if (elem->next!=NULL) elem->next->prev=elem->prev;
	elem->next=NULL;
	elem->prev=NULL;
	ortp_free(elem);
	return list;
}

OList * o_list_remove(OList *list, void *data){
	OList *it;
	for(it=list;it!=NULL;it=it->next){
		if (it->data==data){
			return o_list_remove_link(list,it);
		}
	}
	return list;
}


uint64_t ortp_timeval_to_ntp(const struct timeval *tv){
	uint64_t msw;
	uint64_t lsw;
	msw=tv->tv_sec + 0x83AA7E80; /* 0x83AA7E80 is the number of seconds from 1900 to 1970 */
	lsw=(uint32_t)((double)tv->tv_usec*(double)(((uint64_t)1)<<32)*1.0e-6);
	return msw<<32 | lsw;
}
