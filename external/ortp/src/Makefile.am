
EXTRA_DIST=dll_entry.c master system winrttimer.cpp winrttimer.h

AM_CPPFLAGS=\
	$(STRICT_OPTIONS) \
	$(STRICT_OPTIONS_CXX) \
	-I$(top_srcdir)/include/ \
	-I$(top_srcdir) \
	$(BCTOOLBOX_CFLAGS)

AM_CFLAGS=\
	$(STRICT_OPTIONS) \
	$(STRICT_OPTIONS_CC) \
	$(PTHREAD_CFLAGS) \
	$(TRUESPEECH_CFLAGS)

AM_LDFLAGS=$(PTHREAD_LDFLAGS)

lib_LTLIBRARIES = libortp.la

libortp_la_SOURCES=	\
			avprofile.c  \
			b64.c \
			event.c \
			extremum.c \
			jitterctl.c jitterctl.h \
			logging.c \
			netsim.c \
			ortp.c \
			payloadtype.c \
			port.c \
			posixtimer.c \
			rtcp.c \
			rtcp_fb.c \
			rtcp_xr.c \
			rtcpparse.c \
			rtpparse.c  \
			rtpprofile.c \
			rtpsession.c \
			rtpsession_inet.c \
			rtpsession_priv.h \
			rtpsignaltable.c  \
			rtptimer.c	rtptimer.h \
			scheduler.c scheduler.h \
			sessionset.c  \
			str_utils.c 	\
			telephonyevents.c  \
			utils.c utils.h

libortp_la_LIBADD= $(PTHREAD_LIBS) $(RT_LIBS) -lm $(BCTOOLBOX_LIBS)

libortp_la_LDFLAGS= -version-info $(LIBORTP_SO_VERSION) -no-undefined

if HAVE_LD_OUTPUT_DEF
libortp_la_LDFLAGS += -Wl,--output-def,libortp-$(LIBORTP_SO_CURRENT).def
defexecdir = $(libdir)
defexec_DATA = libortp-$(LIBORTP_SO_CURRENT).def
CLEANFILES = $(defexec_DATA)

libortp-$(LIBORTP_SO_CURRENT).def: libortp.la

if BUILD_WIN32
defexec_DATA += libortp-$(LIBORTP_SO_CURRENT).lib
libortp-$(LIBORTP_SO_CURRENT).lib: libortp-$(LIBORTP_SO_CURRENT).def libortp.la
	$(DLLTOOL) --dllname libortp-$(LIBORTP_SO_CURRENT).dll --input-def libortp-$(LIBORTP_SO_CURRENT).def --output-lib $@ libortp.la
endif
endif

SUBDIRS= . tests
