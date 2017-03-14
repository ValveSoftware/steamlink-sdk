SUBDIRS=win_receiver win_sender

if ENABLE_TESTS

noinst_PROGRAMS=rtpsend rtprecv mrtpsend mrtprecv test_timer rtpmemtest tevrtpsend tevrtprecv tevmrtprecv rtpsend_stupid fmtp_parse

rtpsend_SOURCES=rtpsend.c

rtprecv_SOURCES=rtprecv.c

mrtpsend_SOURCES=mrtpsend.c

mrtprecv_SOURCES=mrtprecv.c

rtpmemtest_SOURCES=rtpmemtest.c

test_timer_SOURCES=test_timer.c

tevrtpsend_SOURCES=tevrtpsend.c

tevrtprecv_SOURCES=tevrtprecv.c

tevmrtprecv_SOURCES=tevmrtprecv.c

rtpsend_stupid_SOURCES=rtpsend_stupid.c

fmtp_parse_SOURCES=fmtpparse.c

endif

AM_CPPFLAGS=-I$(top_srcdir)/include/
AM_CFLAGS=-D_ORTP_SOURCE $(PTHREAD_CFLAGS) 
AM_LDFLAGS=$(PTHREAD_LDFLAGS)
LDADD=$(top_builddir)/src/libortp.la $(BCTOOLBOX_LIBS)
