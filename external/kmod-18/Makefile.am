SUBDIRS = . libkmod/docs

if BUILD_MANPAGES
SUBDIRS += man
endif

DISTCLEAN_LOCAL_HOOKS =
EXTRA_DIST =
CLEANFILES = $(BUILT_FILES)
BUILT_FILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir)/libkmod \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	${zlib_CFLAGS}

AM_CFLAGS = $(WARNINGFLAGS)
AM_LDFLAGS = $(GCLDFLAGS)

SED_PROCESS = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(SED) \
	-e 's,@VERSION\@,$(VERSION),g' \
	-e 's,@prefix\@,$(prefix),g' \
	-e 's,@exec_prefix\@,$(exec_prefix),g' \
	-e 's,@libdir\@,$(libdir),g' \
	-e 's,@includedir\@,$(includedir),g' \
	-e 's,@liblzma_CFLAGS\@,${liblzma_CFLAGS},g' \
	-e 's,@liblzma_LIBS\@,${liblzma_LIBS},g' \
	-e 's,@zlib_CFLAGS\@,${zlib_CFLAGS},g' \
	-e 's,@zlib_LIBS\@,${zlib_LIBS},g' \
	< $< > $@ || rm $@

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

LIBKMOD_CURRENT=4
LIBKMOD_REVISION=8
LIBKMOD_AGE=2

noinst_LTLIBRARIES = libkmod/libkmod-util.la
libkmod_libkmod_util_la_SOURCES = libkmod/libkmod-hash.c \
				  libkmod/libkmod-hash.h \
				  libkmod/libkmod-array.c \
				  libkmod/libkmod-array.h \
				  libkmod/libkmod-util.c \
				  libkmod/libkmod-util.h

include_HEADERS = libkmod/libkmod.h
lib_LTLIBRARIES = libkmod/libkmod.la

libkmod_libkmod_la_SOURCES =\
	libkmod/libkmod.h \
	libkmod/libkmod-internal.h \
	libkmod/macro.h \
	libkmod/missing.h \
	libkmod/libkmod.c \
	libkmod/libkmod-list.c \
	libkmod/libkmod-config.c \
	libkmod/libkmod-index.c \
	libkmod/libkmod-index.h \
	libkmod/libkmod-module.c \
	libkmod/libkmod-file.c \
	libkmod/libkmod-elf.c \
	libkmod/libkmod-signature.c

EXTRA_DIST += libkmod/libkmod.sym
EXTRA_DIST += libkmod/README libkmod/COPYING testsuite/COPYING COPYING

libkmod_libkmod_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBKMOD_CURRENT):$(LIBKMOD_REVISION):$(LIBKMOD_AGE) \
	-Wl,--version-script=$(top_srcdir)/libkmod/libkmod.sym
libkmod_libkmod_la_DEPENDENCIES = libkmod/libkmod-util.la \
				  ${top_srcdir}/libkmod/libkmod.sym
libkmod_libkmod_la_LIBADD = libkmod/libkmod-util.la \
			    ${liblzma_LIBS} ${zlib_LIBS}

noinst_LTLIBRARIES += libkmod/libkmod-internal.la
libkmod_libkmod_internal_la_SOURCES = $(libkmod_libkmod_la_SOURCES)
libkmod_libkmod_internal_la_LDFLAGS = $(AM_LDFLAGS) \
	-Wl,--version-script=$(top_srcdir)/libkmod/libkmod.sym
libkmod_libkmod_internal_la_DEPENDENCIES  = $(libkmod_libkmod_la_DEPENDENCIES)
libkmod_libkmod_internal_la_LIBADD = $(libkmod_libkmod_la_LIBADD)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libkmod/libkmod.pc
EXTRA_DIST += libkmod/libkmod.pc.in
CLEANFILES += libkmod/libkmod.pc

bashcompletiondir=@bashcompletiondir@
dist_bashcompletion_DATA = \
	shell-completion/bash/kmod

install-exec-hook:
	if test "$(libdir)" != "$(rootlibdir)"; then \
		$(MKDIR_P) $(DESTDIR)$(rootlibdir) && \
		so_img_name=$$(readlink $(DESTDIR)$(libdir)/libkmod.so) && \
		so_img_rel_target_prefix=$$(echo $(libdir) | sed 's,\(^/\|\)[^/][^/]*,..,g') && \
		ln -sf $$so_img_rel_target_prefix$(rootlibdir)/$$so_img_name $(DESTDIR)$(libdir)/libkmod.so && \
		mv $(DESTDIR)$(libdir)/libkmod.so.* $(DESTDIR)$(rootlibdir); \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(rootlibdir)/libkmod.so*

if BUILD_TOOLS
bin_PROGRAMS = tools/kmod

noinst_SCRIPTS = tools/insmod tools/rmmod tools/lsmod \
		 tools/modprobe tools/modinfo tools/depmod

tools_kmod_SOURCES = tools/kmod.c tools/kmod.h tools/lsmod.c \
		     tools/rmmod.c tools/insmod.c \
		     tools/modinfo.c tools/modprobe.c \
		     tools/depmod.c tools/log.h tools/log.c \
		     tools/static-nodes.c
tools_kmod_LDADD = libkmod/libkmod-util.la \
		   libkmod/libkmod-internal.la

${noinst_SCRIPTS}: tools/kmod
	$(AM_V_GEN) ($(RM) $@; \
		$(LN_S) $(notdir $<) $@)
endif

# ------------------------------------------------------------------------------
# PYTHON BINDINGS
# ------------------------------------------------------------------------------

CYTHON_FLAGS_VERBOSE_ =
CYTHON_FLAGS_VERBOSE_0 =
CYTHON_FLAGS_VERBOSE_1 = -v
CYTHON_FLAGS = $(CYTHON_FLAGS_VERBOSE_$(V))
AM_V_CYTHON = $(am__v_CYTHON_$(V))
am__v_CYTHON_ = $(am__v_CYTHON_$(AM_DEFAULT_VERBOSITY))
am__v_CYTHON_0 = @echo "  CYTHON " $@;

.pyx.c:
	$(AM_V_CYTHON)$(CYTHON) -o $@ $<

%.py: %.py.in Makefile
	$(SED_PROCESS)

# Remove some warnings for generated code
PYTHON_NOWARN = -Wno-redundant-decls -Wno-shadow -Wno-strict-aliasing

CPYTHON_MODULE_CFLAGS = $(AM_CFLAGS) -DCPYTHON_COMPILING_IN_PYPY=0 \
			$(PYTHON_NOWARN) $(PYTHON_CFLAGS) \
			-fvisibility=default
CPYTHON_MODULE_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version -shared

if BUILD_PYTHON
pkgpyexec_LTLIBRARIES = libkmod/python/kmod/kmod.la \
			libkmod/python/kmod/list.la \
			libkmod/python/kmod/module.la \
			libkmod/python/kmod/_util.la

libkmod_python_kmod_kmod_la_SOURCES = libkmod/python/kmod/kmod.c
libkmod_python_kmod_kmod_la_CFLAGS = $(CPYTHON_MODULE_CFLAGS)
libkmod_python_kmod_kmod_la_LDFLAGS = $(CPYTHON_MODULE_LDFLAGS)
libkmod_python_kmod_kmod_la_LIBADD = $(PYTHON_LIBS) libkmod/libkmod.la

libkmod_python_kmod_list_la_SOURCES = libkmod/python/kmod/list.c
libkmod_python_kmod_list_la_CFLAGS = $(CPYTHON_MODULE_CFLAGS)
libkmod_python_kmod_list_la_LDFLAGS = $(CPYTHON_MODULE_LDFLAGS)
libkmod_python_kmod_list_la_LIBADD = $(PYTHON_LIBS) libkmod/libkmod.la

libkmod_python_kmod_module_la_SOURCES = libkmod/python/kmod/module.c
libkmod_python_kmod_module_la_CFLAGS = $(CPYTHON_MODULE_CFLAGS)
libkmod_python_kmod_module_la_LDFLAGS = $(CPYTHON_MODULE_LDFLAGS)
libkmod_python_kmod_module_la_LIBADD = $(PYTHON_LIBS) libkmod/libkmod.la

libkmod_python_kmod__util_la_SOURCES = libkmod/python/kmod/_util.c
libkmod_python_kmod__util_la_CFLAGS = $(CPYTHON_MODULE_CFLAGS)
libkmod_python_kmod__util_la_LDFLAGS = $(CPYTHON_MODULE_LDFLAGS)
libkmod_python_kmod__util_la_LIBADD = $(PYTHON_LIBS) libkmod/libkmod.la

BUILT_FILES += $(libkmod_python_kmod_kmod_la_SOURCES) \
	       $(libkmod_python_kmod_list_la_SOURCES) \
	       $(libkmod_python_kmod_module_la_SOURCES) \
	       $(libkmod_python_kmod__util_la_SOURCES)

dist_pkgpyexec_PYTHON = libkmod/python/kmod/error.py \
			libkmod/python/kmod/__init__.py \
			libkmod/python/kmod/version.py

BUILT_FILES += libkmod/python/kmod/version.py

endif
# ------------------------------------------------------------------------------
# TESTSUITE
# ------------------------------------------------------------------------------

ROOTFS = testsuite/rootfs
ROOTFS_PRISTINE = $(top_srcdir)/testsuite/rootfs-pristine
CREATE_ROOTFS = $(AM_V_GEN) ( $(RM) -rf $(ROOTFS) && mkdir -p $(dir $(ROOTFS)) && \
				cp -r $(ROOTFS_PRISTINE) $(ROOTFS) && \
				touch testsuite/stamp-rootfs && \
				find $(ROOTFS) -type d -exec chmod +w {} \; )

rootfs:
	$(CREATE_ROOTFS)
.PHONY: rootfs

$(ROOTFS): $(ROOTFS_PRISTINE)
	$(CREATE_ROOTFS)

TESTSUITE_OVERRIDE_LIBS = testsuite/uname.la testsuite/path.la \
			  testsuite/init_module.la \
			  testsuite/delete_module.la
TESTSUITE_OVERRIDE_LIBS_LDFLAGS = avoid-version -module -shared -export-dynamic \
				  -rpath /nowhere -ldl

check-sysconfdir:
	$(AM_V_at)if test "$(sysconfdir)" != "/etc" -a "$(sysconfdir)" != "/etc/"; then \
		echo "warning: Some tests will fail without --sysconfdir=/etc" >&2; \
	fi
.PHONY: check-sysconfdir

check-am: rootfs check-sysconfdir


check_LTLIBRARIES = $(TESTSUITE_OVERRIDE_LIBS)

testsuite_uname_la_LDFLAGS = $(TESTSUITE_OVERRIDE_LIBS_LDFLAGS)
testsuite_path_la_LDFLAGS = $(TESTSUITE_OVERRIDE_LIBS_LDFLAGS)

testsuite_delete_module_la_LDFLAGS = $(TESTSUITE_OVERRIDE_LIBS_LDFLAGS)
testsuite_init_module_la_LDFLAGS = $(TESTSUITE_OVERRIDE_LIBS_LDFLAGS)
testsuite_init_module_la_SOURCES = testsuite/init_module.c \
				   testsuite/stripped-module.h
testsuite_init_module_la_LIBADD = libkmod/libkmod-internal.la

TESTSUITE_CPPFLAGS = $(AM_CPPFLAGS) \
		     -DTESTSUITE_ROOTFS=\"$(abs_top_builddir)/$(ROOTFS)/\" \
		     -DABS_TOP_BUILDDIR=\"$(abs_top_builddir)\"
TESTSUITE_LDADD = testsuite/libtestsuite.la libkmod/libkmod-internal.la \
		  libkmod/libkmod-util.la

check_LTLIBRARIES += testsuite/libtestsuite.la
testsuite_libtestsuite_la_SOURCES = testsuite/testsuite.c \
				    testsuite/testsuite.h
testsuite_libtestsuite_la_DEPENDENCIES = $(ROOTFS) \
					 $(TESTSUITE_OVERRIDE_LIBS)
testsuite_libtestsuite_la_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_libtestsuite_la_LIBADD = -lrt

TESTSUITE = testsuite/test-hash \
            testsuite/test-init testsuite/test-testsuite testsuite/test-loaded \
	    testsuite/test-modinfo testsuite/test-util testsuite/test-new-module \
	    testsuite/test-modprobe testsuite/test-blacklist \
	    testsuite/test-dependencies testsuite/test-depmod

check_PROGRAMS = $(TESTSUITE)
TESTS = $(TESTSUITE)

testsuite_test_testsuite_LDADD = testsuite/libtestsuite.la libkmod/libkmod-util.la
testsuite_test_testsuite_CPPFLAGS = $(TESTSUITE_CPPFLAGS)

testsuite_test_hash_LDADD = $(TESTSUITE_LDADD)
testsuite_test_hash_CPPFLAGS = $(TESTSUITE_CPPFLAGS)

testsuite_test_init_LDADD = $(TESTSUITE_LDADD)
testsuite_test_init_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_loaded_LDADD = $(TESTSUITE_LDADD)
testsuite_test_loaded_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_modinfo_LDADD = $(TESTSUITE_LDADD)
testsuite_test_modinfo_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_util_LDADD = $(TESTSUITE_LDADD)
testsuite_test_util_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_new_module_LDADD = $(TESTSUITE_LDADD)
testsuite_test_new_module_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_modprobe_LDADD = $(TESTSUITE_LDADD)
testsuite_test_modprobe_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_blacklist_LDADD = $(TESTSUITE_LDADD)
testsuite_test_blacklist_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_dependencies_LDADD = $(TESTSUITE_LDADD)
testsuite_test_dependencies_CPPFLAGS = $(TESTSUITE_CPPFLAGS)
testsuite_test_depmod_LDADD = $(TESTSUITE_LDADD)
testsuite_test_depmod_CPPFLAGS = $(TESTSUITE_CPPFLAGS)

testsuite-distclean:
	$(RM) -r $(ROOTFS)
	$(RM) testsuite/stamp-rootfs

DISTCLEAN_LOCAL_HOOKS += testsuite-distclean
EXTRA_DIST += testsuite/rootfs-pristine

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc --enable-python --sysconfdir=/etc \
	--with-zlib \
	--with-bashcompletiondir=$$dc_install_base/$(bashcompletiondir)

distclean-local: $(DISTCLEAN_LOCAL_HOOKS)

buildtest-TESTS:
	$(MAKE) $(AM_MAKEFLAGS) $(check_LTLIBRARIES) $(check_PROGRAMS)

# ------------------------------------------------------------------------------
# custom release helpers
# ------------------------------------------------------------------------------

git-release:
	head -1 NEWS | grep -q "kmod $(VERSION)"
	git commit -a -m "kmod $(VERSION)"
	git tag -m "kmod $(VERSION)" -s v$(VERSION)
	git gc --prune=0

kmod-$(VERSION).tar.xz:
	make distcheck

kmod-$(VERSION).tar.sign:
	xz -d -c kmod-$(VERSION).tar.xz | gpg --armor --detach-sign --output kmod-$(VERSION).tar.sign

tar: kmod-$(VERSION).tar.xz kmod-$(VERSION).tar.sign

tar-sync: kmod-$(VERSION).tar.xz kmod-$(VERSION).tar.sign
	kup put kmod-$(VERSION).tar.xz  kmod-$(VERSION).tar.sign /pub/linux/utils/kernel/kmod/
