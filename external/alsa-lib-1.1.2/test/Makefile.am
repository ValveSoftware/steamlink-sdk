SUBDIRS=. lsb

check_PROGRAMS=control pcm pcm_min latency seq \
	       playmidi1 timer rawmidi midiloop \
	       oldapi queue_timer namehint client_event_filter \
	       chmap audio_time user-ctl-element-set pcm-multi-thread

control_LDADD=../src/libasound.la
pcm_LDADD=../src/libasound.la
pcm_LDFLAGS= -lm
pcm_min_LDADD=../src/libasound.la
latency_LDADD=../src/libasound.la
latency_LDFLAGS= -lm
seq_LDADD=../src/libasound.la
playmidi1_LDADD=../src/libasound.la
timer_LDADD=../src/libasound.la
rawmidi_LDADD=../src/libasound.la
midiloop_LDADD=../src/libasound.la
oldapi_LDADD=../src/libasound.la
queue_timer_LDADD=../src/libasound.la
namehint_LDADD=../src/libasound.la
client_event_filter_LDADD=../src/libasound.la
code_CFLAGS=-Wall -pipe -g -O2
chmap_LDADD=../src/libasound.la
audio_time_LDADD=../src/libasound.la
pcm_multi_thread_LDADD=../src/libasound.la
pcm_multi_thread_LDFLAGS=-lpthread
user_ctl_element_set_LDADD=../src/libasound.la
user_ctl_element_set_CFLAGS=-Wall -g

AM_CPPFLAGS=-I$(top_srcdir)/include
AM_CFLAGS=-Wall -pipe -g

EXTRA_DIST=seq-decoder.c seq-sender.c midifile.h midifile.c midifile.3
