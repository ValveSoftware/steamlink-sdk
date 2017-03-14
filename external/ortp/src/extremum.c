/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014 Belledonne Communications SARL
Author: Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terortp of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "ortp/str_utils.h"

void ortp_extremum_reset(ortp_extremum *obj){
	obj->current_extremum=0;
	obj->extremum_time=(uint64_t)-1;
	obj->last_stable=0;
}

void ortp_extremum_init(ortp_extremum *obj, int period){
	ortp_extremum_reset(obj);
	obj->period=period;
}


static void extremum_check_init(ortp_extremum *obj, uint64_t curtime, float value, const char *kind){
	if (obj->extremum_time!=(uint64_t)-1){
		if (((int)(curtime-obj->extremum_time))>obj->period){
			obj->last_stable=obj->current_extremum;
			/*last extremum is too old, drop it and replace it with current value*/
			obj->current_extremum=value;
			obj->extremum_time=curtime;
		}
	}else {
		obj->current_extremum=value;
		obj->extremum_time=curtime;
	}
}

void ortp_extremum_record_min(ortp_extremum *obj, uint64_t curtime, float value){
	extremum_check_init(obj,curtime,value,"min");
	if (value<obj->current_extremum){
		obj->current_extremum=value;
		obj->extremum_time=curtime;
	}
}

void ortp_extremum_record_max(ortp_extremum *obj, uint64_t curtime, float value){
	extremum_check_init(obj,curtime,value,"max");
	if (value>obj->current_extremum){
		obj->current_extremum=value;
		obj->extremum_time=curtime;
	}
}

float ortp_extremum_get_current(ortp_extremum *obj){
	return obj->current_extremum;
}

float ortp_extremum_get_previous(ortp_extremum *obj){
	return obj->last_stable;
}

