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
#include <bctoolbox/port.h>


int rtp_profile_get_payload_number_from_mime(RtpProfile *profile, const char *mime)
{
	return rtp_profile_get_payload_number_from_mime_and_flag(profile, mime, -1);
}

int rtp_profile_get_payload_number_from_mime_and_flag(RtpProfile *profile, const char *mime, int flag)
{
	PayloadType *pt;
	int i;
	for (i = 0; i < RTP_PROFILE_MAX_PAYLOADS; i++) {
		pt = rtp_profile_get_payload(profile, i);
		if (pt != NULL) {
			if (strcasecmp(pt->mime_type, mime) == 0) {
				if (flag < 0 || pt->flags & flag) {
					return i;
				}
			}
		}
	}
	return -1;
}

int rtp_profile_find_payload_number(RtpProfile*profile,const char *mime,int rate,int channels)
{
	int i;
	PayloadType *pt;
	for (i=0;i<RTP_PROFILE_MAX_PAYLOADS;i++)
	{
		pt=rtp_profile_get_payload(profile,i);
		if (pt!=NULL)
		{
			if (strcasecmp(pt->mime_type,mime)==0 &&
			    pt->clock_rate==rate &&
			    (pt->channels==channels || channels<=0 || pt->channels<=0)) {
				/*we don't look at channels if it is undefined
				ie a negative or zero value*/
				return i;
			}
		}
	}
	return -1;
}

int rtp_profile_get_payload_number_from_rtpmap(RtpProfile *profile,const char *rtpmap)
{
	int clock_rate, channels, ret;
	char* subtype = ortp_strdup( rtpmap );
	char* rate_str = NULL;
	char* chan_str = NULL;

	/* find the slash after the subtype */
	rate_str = strchr(subtype, '/');
	if (rate_str && strlen(rate_str)>1) {
		*rate_str = 0;
		rate_str++;

		/* Look for another slash */
		chan_str = strchr(rate_str, '/');
		if (chan_str && strlen(chan_str)>1) {
			*chan_str = 0;
			chan_str++;
		} else {
			chan_str = NULL;
		}
	} else {
		rate_str = NULL;
	}

	// Use default clock rate if none given
	if (rate_str) clock_rate = atoi(rate_str);
	else clock_rate = 8000;

	// Use default number of channels if none given
	if (chan_str) channels = atoi(chan_str);
	else channels = -1;

	//printf("Searching for payload %s at freq %i with %i channels\n",subtype,clock_rate,ch1annels);
	ret=rtp_profile_find_payload_number(profile,subtype,clock_rate,channels);
	ortp_free(subtype);
	return ret;
}

PayloadType * rtp_profile_find_payload(RtpProfile *prof,const char *mime,int rate,int channels)
{
	int i;
	i=rtp_profile_find_payload_number(prof,mime,rate,channels);
	if (i>=0) return rtp_profile_get_payload(prof,i);
	return NULL;
}


PayloadType * rtp_profile_get_payload_from_mime(RtpProfile *profile,const char *mime)
{
	int pt;
	pt=rtp_profile_get_payload_number_from_mime(profile,mime);
	if (pt==-1) return NULL;
	else return rtp_profile_get_payload(profile,pt);
}


PayloadType * rtp_profile_get_payload_from_rtpmap(RtpProfile *profile, const char *rtpmap)
{
	int pt = rtp_profile_get_payload_number_from_rtpmap(profile,rtpmap);
	if (pt==-1) return NULL;
	else return rtp_profile_get_payload(profile,pt);
}

int rtp_profile_move_payload(RtpProfile *prof,int oldpos,int newpos){
	if (oldpos<0 || oldpos>=RTP_PROFILE_MAX_PAYLOADS) {
		ortp_error("Bad old pos index %i",oldpos);
		return -1;
	}
	if (newpos<0 || newpos>=RTP_PROFILE_MAX_PAYLOADS) {
		ortp_error("Bad new pos index %i",newpos);
		return -1;
	}
	prof->payload[newpos]=prof->payload[oldpos];
	prof->payload[oldpos]=NULL;
	return 0;
}

RtpProfile * rtp_profile_new(const char *name)
{
	RtpProfile *prof=(RtpProfile*)ortp_new0(RtpProfile,1);
	rtp_profile_set_name(prof,name);
	return prof;
}

/**
 *	Assign payload type number index to payload type desribed in pt for the RTP profile profile.
 * @param profile a RTP profile
 * @param idx the payload type number
 * @param pt the payload type description
 *
**/
void rtp_profile_set_payload(RtpProfile *profile, int idx, PayloadType *pt){
	if (idx<0 || idx>=RTP_PROFILE_MAX_PAYLOADS) {
		ortp_error("Bad index %i",idx);
		return;
	}
	profile->payload[idx]=pt;
}

/**
 * Initialize the profile to the empty profile (all payload type are unassigned).
 *@param profile a RTP profile
 *
**/
void rtp_profile_clear_all(RtpProfile *profile){
	int i;
	for (i=0;i<RTP_PROFILE_MAX_PAYLOADS;i++){
		profile->payload[i]=0;
	}
}


/**
 * Set a name to the rtp profile. (This is not required)
 * @param profile a rtp profile object
 * @param name a string
 *
**/
void rtp_profile_set_name(RtpProfile *profile, const char *name){
	if (profile->name!=NULL) ortp_free(profile->name);
	profile->name=ortp_strdup(name);
}

/* ! payload are not cloned*/
RtpProfile * rtp_profile_clone(RtpProfile *prof)
{
	int i;
	PayloadType *pt;
	RtpProfile *newprof=rtp_profile_new(prof->name);
	for (i=0;i<RTP_PROFILE_MAX_PAYLOADS;i++){
		pt=rtp_profile_get_payload(prof,i);
		if (pt!=NULL){
			rtp_profile_set_payload(newprof,i,pt);
		}
	}
	return newprof;
}


/*clone a profile and its payloads */
RtpProfile * rtp_profile_clone_full(RtpProfile *prof)
{
	int i;
	PayloadType *pt;
	RtpProfile *newprof=rtp_profile_new(prof->name);
	for (i=0;i<RTP_PROFILE_MAX_PAYLOADS;i++){
		pt=rtp_profile_get_payload(prof,i);
		if (pt!=NULL){
			rtp_profile_set_payload(newprof,i,payload_type_clone(pt));
		}
	}
	return newprof;
}

void rtp_profile_destroy(RtpProfile *prof)
{
	int i;
	PayloadType *payload;
	if (prof->name) {
		ortp_free(prof->name);
		prof->name = NULL;
	}
	for (i=0;i<RTP_PROFILE_MAX_PAYLOADS;i++)
	{
		payload=rtp_profile_get_payload(prof,i);
		if (payload!=NULL && (payload->flags & PAYLOAD_TYPE_ALLOCATED))
			payload_type_destroy(payload);
	}
	ortp_free(prof);
}
