# This file is part of PulseAudio.
#
# PulseAudio is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# PulseAudio is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.

DISTCLEANFILES = \
	$(noinst_DATA)

noinst_DATA = \
	pulseaudio.1.xml \
	esdcompat.1.xml \
	pax11publish.1.xml \
	pacat.1.xml \
	pacmd.1.xml \
	pactl.1.xml \
	pasuspender.1.xml \
	padsp.1.xml \
	pulse-daemon.conf.5.xml \
	pulse-client.conf.5.xml \
	default.pa.5.xml \
	pulse-cli-syntax.5.xml \
	start-pulseaudio-x11.1.xml

xmllint: $(noinst_DATA)
	for f in $(noinst_DATA) ; do \
			xmllint --noout --valid "$$f" || exit 1 ; \
	done

if BUILD_MANPAGES

dist_man_MANS = \
	pulseaudio.1 \
	esdcompat.1 \
	pax11publish.1 \
	pacat.1 \
	pacmd.1 \
	pactl.1 \
	pasuspender.1 \
	padsp.1 \
	pulse-daemon.conf.5 \
	pulse-client.conf.5 \
	default.pa.5 \
	pulse-cli-syntax.5 \
	start-pulseaudio-x11.1

CLEANFILES = \
	$(dist_man_MANS)

%: %.xml Makefile
	$(AM_V_GEN) perl $(srcdir)/xmltoman $< > $@ || rm -f $@

if OS_IS_WIN32
SYMLINK_PROGRAM=cd $(DESTDIR)$(man1dir) && cp
else
SYMLINK_PROGRAM=ln -sf
endif
install-data-hook:
	$(SYMLINK_PROGRAM) pacat.1 $(DESTDIR)$(man1dir)/paplay.1
	$(SYMLINK_PROGRAM) pacat.1 $(DESTDIR)$(man1dir)/parec.1
	$(SYMLINK_PROGRAM) pacat.1 $(DESTDIR)$(man1dir)/parecord.1
	$(SYMLINK_PROGRAM) pacat.1 $(DESTDIR)$(man1dir)/pamon.1

uninstall-hook:
	rm -f $(DESTDIR)$(man1dir)/paplay.1
	rm -f $(DESTDIR)$(man1dir)/parec.1
	rm -f $(DESTDIR)$(man1dir)/parecord.1
	rm -f $(DESTDIR)$(man1dir)/pamon.1
endif

EXTRA_DIST = \
	pulseaudio.1.xml.in \
	esdcompat.1.xml.in \
	pax11publish.1.xml.in \
	pacat.1.xml.in \
	pacmd.1.xml.in \
	pactl.1.xml.in \
	pasuspender.1.xml.in \
	padsp.1.xml.in \
	pulse-daemon.conf.5.xml.in \
	pulse-client.conf.5.xml.in \
	default.pa.5.xml.in \
	pulse-cli-syntax.5.xml.in \
	start-pulseaudio-x11.1.xml.in \
	xmltoman \
	xmltoman.css \
	xmltoman.xsl \
	xmltoman.dtd
