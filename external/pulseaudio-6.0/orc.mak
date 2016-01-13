#
# This is a Makefile.am fragment to build Orc code. It is based
# on the orc.mak file distributed in the GStreamer common
# repository.
#
# Include this file like this:
#
#  include $(top_srcdir)/orc.mak
#
# For each Orc source file, append its name (without the extension)
# to ORC_SOURCE:
#
#  ORC_SOURCE += gstadderorc
#
# This will create gstadder-orc-gen.c and gstadder-orc-gen.h, which
# you need to add to your nodist_module_SOURCES.
#
# Note that this file appends to BUILT_SOURCES and CLEANFILES, so
# define them before including this file.
#


EXTRA_DIST += $(addsuffix .orc,$(ORC_SOURCE))

if HAVE_ORC
ORC_BUILT_SOURCE = $(addsuffix -orc-gen.c,$(ORC_SOURCE))
ORC_BUILT_HEADER = $(addsuffix -orc-gen.h,$(ORC_SOURCE))

BUILT_SOURCES += $(ORC_BUILT_SOURCE) $(ORC_BUILT_HEADER)
CLEANFILES += $(BUILT_SOURCES)


orcc_v_gen = $(orcc_v_gen_$(V))
orcc_v_gen_ = $(orcc_v_gen_$(AM_DEFAULT_VERBOSITY))
orcc_v_gen_0 = @echo "  ORCC   $@";

cp_v_gen = $(cp_v_gen_$(V))
cp_v_gen_ = $(cp_v_gen_$(AM_DEFAULT_VERBOSITY))
cp_v_gen_0 = @echo "  CP     $@";

%-orc-gen.c: %.orc
	@mkdir -p $(@D)
	$(orcc_v_gen)$(ORCC) --implementation -o $@ $<

%-orc-gen.h: %.orc
	@mkdir -p $(@D)
	$(orcc_v_gen)$(ORCC) --header -o $@ $<
endif
