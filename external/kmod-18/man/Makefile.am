MAN5 = depmod.d.5 modprobe.d.5 modules.dep.5
MAN8 = depmod.8 insmod.8 lsmod.8 rmmod.8 modprobe.8 modinfo.8
MAN_STUB = modules.dep.bin.5

AM_V_XSLT = $(AM_V_XSLT_$(V))
AM_V_XSLT_ = $(AM_V_XSLT_$(AM_DEFAULT_VERBOSITY))
AM_V_XSLT_0 = @echo "  XSLT    " $@;

if BUILD_TOOLS
dist_man_MANS = $(MAN5) $(MAN8) $(MAN_STUB)
modules.dep.bin.5: modules.dep.5
endif

EXTRA_DIST = $(MAN5:%.5=%.xml) $(MAN8:%.8=%.xml)

XSLTPROC_FLAGS = \
	--nonet \
	--stringparam man.output.quietly 1 \
	--param funcsynopsis.style "'ansi'"

CLEANFILES = $(dist_man_MANS)

%.5 %.8: %.xml
	$(AM_V_XSLT)$(XSLTPROC) $(XSLTPROC_FLAGS) \
		http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
