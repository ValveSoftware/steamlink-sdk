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
 * \file rtpprofile.h
 * \brief Using and creating standart and custom RTP profiles
 *
**/

#ifndef RTPPROFILE_H
#define RTPPROFILE_H
#include <ortp/port.h>

#ifdef __cplusplus
extern "C"{
#endif

#define RTP_PROFILE_MAX_PAYLOADS 128

/**
 * The RTP profile is a table RTP_PROFILE_MAX_PAYLOADS entries to make the matching
 * between RTP payload type number and the PayloadType that defines the type of
 * media.
**/
struct _RtpProfile
{
	char *name;
	PayloadType *payload[RTP_PROFILE_MAX_PAYLOADS];
};


typedef struct _RtpProfile RtpProfile;

ORTP_VAR_PUBLIC RtpProfile av_profile;

#define rtp_profile_get_name(profile) 	(const char*)((profile)->name)

ORTP_PUBLIC void rtp_profile_set_payload(RtpProfile *prof, int idx, PayloadType *pt);

/**
 *	Set payload type number \a index unassigned in the profile.
 *
 *@param profile an RTP profile
 *@param index	the payload type number
**/
#define rtp_profile_clear_payload(profile,index) \
	rtp_profile_set_payload(profile,index,NULL)

/* I prefer have this function inlined because it is very often called in the code */
/**
 *
 *	Gets the payload description of the payload type \a index in the profile.
 *
 *@param prof an RTP profile (a #_RtpProfile object)
 *@param idx	the payload type number
 *@return the payload description (a PayloadType object)
**/
static ORTP_INLINE PayloadType * rtp_profile_get_payload(const RtpProfile *prof, int idx){
	if (idx<0 || idx>=RTP_PROFILE_MAX_PAYLOADS) {
		return NULL;
	}
	return prof->payload[idx];
}
ORTP_PUBLIC void rtp_profile_clear_all(RtpProfile *prof);
ORTP_PUBLIC void rtp_profile_set_name(RtpProfile *prof, const char *name);
ORTP_PUBLIC PayloadType * rtp_profile_get_payload_from_mime(RtpProfile *profile,const char *mime);
ORTP_PUBLIC PayloadType * rtp_profile_get_payload_from_rtpmap(RtpProfile *profile, const char *rtpmap);
ORTP_PUBLIC int rtp_profile_get_payload_number_from_mime(RtpProfile *profile, const char *mime);
ORTP_PUBLIC int rtp_profile_get_payload_number_from_mime_and_flag(RtpProfile *profile, const char *mime, int flag);
ORTP_PUBLIC int rtp_profile_get_payload_number_from_rtpmap(RtpProfile *profile, const char *rtpmap);
ORTP_PUBLIC int rtp_profile_find_payload_number(RtpProfile *prof,const char *mime,int rate, int channels);
ORTP_PUBLIC PayloadType * rtp_profile_find_payload(RtpProfile *prof,const char *mime,int rate, int channels);
ORTP_PUBLIC int rtp_profile_move_payload(RtpProfile *prof,int oldpos,int newpos);

ORTP_PUBLIC RtpProfile * rtp_profile_new(const char *name);
/* clone a profile, payload are not cloned */
ORTP_PUBLIC RtpProfile * rtp_profile_clone(RtpProfile *prof);


/*clone a profile and its payloads (ie payload type are newly allocated, not reusing payload types of the reference profile) */
ORTP_PUBLIC RtpProfile * rtp_profile_clone_full(RtpProfile *prof);
/* frees the profile and all its PayloadTypes*/
ORTP_PUBLIC void rtp_profile_destroy(RtpProfile *prof);

#ifdef __cplusplus
}
#endif

#endif
