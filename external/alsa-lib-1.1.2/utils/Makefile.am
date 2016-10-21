if INSTALL_M4
aclocaldir=$(datadir)/aclocal
aclocal_DATA=alsa.m4
endif
EXTRA_DIST=alsa.m4 buildrpm alsa.pc.in

alsapkgconfdir = @ALSA_PKGCONF_DIR@
pkgconfigdir = $(alsapkgconfdir)
pkgconfig_DATA = alsa.pc

rpm: buildrpm alsa-lib.spec
	VERSION=$(VERSION) $(srcdir)/buildrpm

AM_CPPFLAGS=-I$(top_srcdir)/include
