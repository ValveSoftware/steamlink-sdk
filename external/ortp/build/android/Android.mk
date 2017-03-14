##
## Android.mk -Android build script-
##
##
## Copyright (C) 2010  Belledonne Communications, Grenoble, France
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU Library General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##


LOCAL_PATH:= $(call my-dir)/../../
include $(CLEAR_VARS)

LOCAL_MODULE := libortp


LOCAL_SRC_FILES := \
	src/avprofile.c \
	src/b64.c \
	src/event.c \
	src/extremum.c \
	src/jitterctl.c \
	src/logging.c \
	src/netsim.c \
	src/ortp.c \
	src/payloadtype.c \
	src/port.c \
	src/posixtimer.c \
	src/rtcp.c \
	src/rtcp_fb.c \
	src/rtcp_xr.c \
	src/rtcpparse.c \
	src/rtpparse.c \
	src/rtpprofile.c \
	src/rtpsession.c \
	src/rtpsession_inet.c \
	src/rtpsignaltable.c  \
	src/rtptimer.c \
	src/scheduler.c \
	src/sessionset.c \
	src/str_utils.c	\
	src/telephonyevents.c \
	src/utils.c

LOCAL_CFLAGS += \
	-DORTP_INET6 \
	-UHAVE_CONFIG_H \
	-include ortp_AndroidConfig.h \
	-DHAVE_PTHREADS \
	-Werror -Wall -Wno-error=strict-aliasing -Wuninitialized


LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/build/android

LOCAL_CPPFLAGS = $(LOCAL_CLFAGS)
LOCAL_CFLAGS += -Wdeclaration-after-statement

include $(BUILD_STATIC_LIBRARY)
