AM_CPPFLAGS=-I$(top_srcdir)/include

SUBDIRS = include alsactl utils m4 po alsa-info
if ALSAMIXER
SUBDIRS += alsamixer
endif
if HAVE_MIXER
SUBDIRS += amixer
endif
if HAVE_RAWMIDI
SUBDIRS += amidi
endif
if ALSACONF
SUBDIRS += alsaconf
endif
if HAVE_PCM
SUBDIRS += aplay iecset speaker-test
if ALSALOOP
SUBDIRS += alsaloop
endif
if BAT
SUBDIRS += bat
endif
endif
if HAVE_SEQ
SUBDIRS += seq
endif
if HAVE_UCM
SUBDIRS += alsaucm
endif
if HAVE_TOPOLOGY
SUBDIRS += topology
endif

EXTRA_DIST= config.rpath  TODO gitcompile
AUTOMAKE_OPTIONS=foreign
ACLOCAL_AMFLAGS = -I m4

rpm: dist
	$(MAKE) -C $@

dist-hook:
	-chmod -R a+r $(distdir)
	@if ! test -z "$(AMTAR)"; then \
		$(AMTAR) --create --verbose --file=- $(distdir) | bzip2 -c -9 > $(distdir).tar.bz2 ; \
	else \
		$(TAR) --create --verbose --file=- $(distdir) | bzip2 -c -9 > $(distdir).tar.bz2 ; \
	fi

install-data-hook:
	$(MKDIR_P) -m 0755 $(DESTDIR)$(ASOUND_STATE_DIR)

DISTCHECK_CONFIGURE_FLAGS = \
        --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)
