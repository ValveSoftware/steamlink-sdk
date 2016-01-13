# Copyright (C) 2006-2012 Free Software Foundation, Inc.
#
# Author: Simon Josefsson
#
# This file is part of GnuTLS.
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this file; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

WFLAGS ?= --enable-gcc-warnings
ADDFLAGS ?=
CFGFLAGS ?= --enable-gtk-doc --enable-gtk-doc-pdf --enable-gtk-doc-html $(ADDFLAGS) $(WFLAGS)
PACKAGE ?= gnutls

.PHONY: config

INDENT_SOURCES = `find . -name \*.[ch] -o -name gnutls.h.in | grep -v -e ^./build-aux/ -e ^./lib/minitasn1/ -e ^./lib/build-aux/ -e ^./gl/ -e ^./src/libopts/ -e -args.[ch] -e asn1_tab.c -e ^./tests/suite/`

ifeq ($(.DEFAULT_GOAL),abort-due-to-no-makefile)
.DEFAULT_GOAL := bootstrap
endif

PODIR := po
PO_DOMAIN := libgnutls

local-checks-to-skip = sc_GPL_version sc_bindtextdomain			\
	sc_immutable_NEWS sc_program_name sc_prohibit_atoi_atof		\
	sc_prohibit_empty_lines_at_EOF sc_prohibit_hash_without_use	\
	sc_prohibit_have_config_h sc_prohibit_magic_number_exit		\
	sc_prohibit_strcmp sc_require_config_h				\
	sc_require_config_h_first sc_texinfo_acronym sc_trailing_blank	\
	sc_unmarked_diagnostics sc_useless_cpp_parens			\
	sc_two_space_separator_in_usage

VC_LIST_ALWAYS_EXCLUDE_REGEX = ^maint.mk|gtk-doc.make|m4/pkg|doc/fdl-1.3.texi|po/.*.po.in|src/crywrap/|(devel/perlasm/|lib/accelerated/x86/|build-aux/|gl/|src/libopts/|tests/suite/ecore/|doc/protocol/).*$$
update-copyright-env = UPDATE_COPYRIGHT_USE_INTERVALS=1

# Explicit syntax-check exceptions.
exclude_file_name_regexp--sc_error_message_period = ^src/crywrap/crywrap.c$$
exclude_file_name_regexp--sc_error_message_uppercase = ^doc/examples/ex-cxx.cpp|guile/src/core.c|src/certtool.c|src/ocsptool.c|src/crywrap/crywrap.c|tests/pkcs12_encode.c$$
exclude_file_name_regexp--sc_file_system = ^doc/doxygen/Doxyfile
exclude_file_name_regexp--sc_prohibit_cvs_keyword = ^lib/nettle/.*$$
exclude_file_name_regexp--sc_prohibit_undesirable_word_seq = ^tests/nist-pkits/gnutls-nist-tests.html$$
exclude_file_name_regexp--sc_space_tab = ^doc/.*.(pdf|png)|tests/nist-pkits/|tests/suite/x509paths/.*$$

autoreconf:
	for f in $(PODIR)/*.po.in; do \
		cp $$f `echo $$f | sed 's/.in//'`; \
	done
	mv build-aux/config.rpath build-aux/config.rpath-
	autopoint
	rm -f m4/codeset.m4 m4/gettext.m4 m4/glibc21.m4 m4/glibc2.m4 m4/iconv.m4 m4/intdiv0.m4 m4/intldir.m4 m4/intl.m4 m4/intlmacosx.m4 m4/intmax.m4 m4/inttypes_h.m4 m4/inttypes-pri.m4 m4/lcmessage.m4 m4/lib-ld.m4 m4/lib-link.m4 m4/lib-prefix.m4 m4/lock.m4 m4/longlong.m4 m4/nls.m4 m4/po.m4 m4/printf-posix.m4 m4/progtest.m4 m4/size_max.m4 m4/stdint_h.m4 m4/uintmax_t.m4 m4/wchar_t.m4 m4/wint_t.m4 m4/visibility.m4 m4/xsize.m4
	touch ChangeLog
	test -f ./configure || AUTOPOINT=true autoreconf --install
	mv build-aux/config.rpath- build-aux/config.rpath

update-po: refresh-po
	for f in `ls $(PODIR)/*.po | grep -v quot.po`; do \
		cp $$f $$f.in; \
	done
	git add $(PODIR)/*.po.in
	git commit -m "Sync with TP." $(PODIR)/LINGUAS $(PODIR)/*.po.in

config:
	./configure $(CFGFLAGS)

.submodule.stamp:
	git submodule init
	git submodule update
	touch $@

bootstrap: autoreconf .submodule.stamp

# The only non-lgpl modules used are: gettime progname timespec. Those
# are not used (and must not be used) in the library)
glimport:
	../gnulib/gnulib-tool --dir=. --local-dir=gl/override --lib=libgnu --source-base=gl --m4-base=gl/m4 --doc-base=doc --tests-base=gl/tests --aux-dir=build-aux --add-import --lgpl=2
	../gnulib/gnulib-tool --dir=. --local-dir=src/gl/override --lib=libgnu_gpl --source-base=src/gl --m4-base=src/gl/m4 --doc-base=doc --tests-base=tests --aux-dir=build-aux --add-import

# Code Coverage

pre-coverage:
	./configure --disable-cxx
	ln -s . gl/tests/glthread/glthread
	ln -sf /usr/local/share/gaa/gaa.skel src/gaa.skel

web-coverage:
	rm -fv `find $(htmldir)/coverage -type f | grep -v CVS`
	cp -rv doc/coverage/* $(htmldir)/coverage/

upload-web-coverage:
	cd $(htmldir) && \
		cvs commit -m "Update." coverage

# Clang

clang:
	make clean
	scan-build ./configure
	rm -rf scan.tmp
	scan-build -o scan.tmp make

clang-copy:
	rm -fv `find $(htmldir)/clang -type f | grep -v CVS`
	mkdir -p $(htmldir)/clang/
	cp -rv scan.tmp/*/* $(htmldir)/clang/

clang-upload:
	cd $(htmldir) && \
		cvs add clang || true && \
		cvs add clang/*.css clang/*.js clang/*.html || true && \
		cvs commit -m "Update." clang

# Release

ChangeLog:
	git log --pretty --numstat --summary --since="2012 November 07" -- | git2cl > ChangeLog
	cat .clcopying >> ChangeLog

tag = $(PACKAGE)_`echo $(VERSION) | sed 's/\./_/g'`
htmldir = ../www-$(PACKAGE)

release: syntax-check prepare upload web upload-web

prepare:
	! git tag -l $(tag) | grep $(PACKAGE) > /dev/null
	rm -f ChangeLog
	$(MAKE) ChangeLog distcheck
	$(MAKE) -C doc/manpages/ manpages-update
	git commit -m Generated. ChangeLog
	git tag -u b565716f! -m $(VERSION) $(tag)

upload-tarballs:
	git push
	git push --tags
	build-aux/gnupload --to alpha.gnu.org:$(PACKAGE) $(distdir).tar.xz
	build-aux/gnupload --to alpha.gnu.org:$(PACKAGE) $(distdir).tar.lz
	cp $(distdir).tar.xz $(distdir).tar.xz.sig ../releases/$(PACKAGE)/
	cp $(distdir).tar.lz $(distdir).tar.lz.sig ../releases/$(PACKAGE)/


web:
	echo generating documentation for $(PACKAGE)
	make -C doc gnutls.html
	cd doc && cp gnutls.html *.png ../$(htmldir)/manual/
	cd doc && makeinfo --html --split=node -o ../$(htmldir)/manual/html_node/ --css-include=./texinfo.css gnutls.texi
	cd doc && cp *.png ../$(htmldir)/manual/html_node/
	sed 's/\@VERSION\@/$(VERSION)/g' -i $(htmldir)/manual/html_node/*.html $(htmldir)/manual/gnutls.html
	-cd doc && make gnutls.epub && cp gnutls.epub ../$(htmldir)/manual/
	cd doc/latex && make gnutls.pdf && cp gnutls.pdf ../../$(htmldir)/manual/
	#cd doc/doxygen && doxygen && cd ../.. && cp -v doc/doxygen/html/* $(htmldir)/devel/doxygen/ && cd doc/doxygen/latex && make refman.pdf && cd ../../../ && cp doc/doxygen/latex/refman.pdf $(htmldir)/devel/doxygen/$(PACKAGE).pdf
	-cp -v doc/reference/html/*.html doc/reference/html/*.png doc/reference/html/*.devhelp doc/reference/html/*.css $(htmldir)/reference/
	#cp -v doc/cyclo/cyclo-$(PACKAGE).html $(htmldir)/cyclo/

upload-web:
	cd $(htmldir) && \
		cvs commit -m "Update." manual/ reference/ \
			doxygen/ devel/ cyclo/

ASM_SOURCES_XXX := \
	lib/accelerated/x86/XXX/cpuid-x86_64.s \
	lib/accelerated/x86/XXX/cpuid-x86.s \
	lib/accelerated/x86/XXX/ghash-x86_64.s \
	lib/accelerated/x86/XXX/aesni-x86_64.s \
	lib/accelerated/x86/XXX/aesni-x86.s \
	lib/accelerated/x86/XXX/e_padlock-x86_64.s \
	lib/accelerated/x86/XXX/e_padlock-x86.s \
	lib/accelerated/x86/XXX/sha1-ssse3-x86.s \
	lib/accelerated/x86/XXX/sha1-ssse3-x86_64.s \
	lib/accelerated/x86/XXX/sha256-ssse3-x86.s \
	lib/accelerated/x86/XXX/sha512-ssse3-x86.s \
	lib/accelerated/x86/XXX/sha512-ssse3-x86_64.s \
	lib/accelerated/x86/XXX/aes-ssse3-x86.s \
	lib/accelerated/x86/XXX/aes-ssse3-x86_64.s

ASM_SOURCES_ELF := $(subst XXX,elf,$(ASM_SOURCES_XXX))
ASM_SOURCES_COFF := $(subst XXX,coff,$(ASM_SOURCES_XXX))
ASM_SOURCES_MACOSX := $(subst XXX,macosx,$(ASM_SOURCES_XXX))

asm-sources: $(ASM_SOURCES_ELF) $(ASM_SOURCES_COFF) $(ASM_SOURCES_MACOSX) lib/accelerated/x86/files.mk

asm-sources-clean:
	rm -f $(ASM_SOURCES_ELF) $(ASM_SOURCES_COFF) $(ASM_SOURCES_MACOSX) lib/accelerated/x86/files.mk

X86_FILES=XXX/aesni-x86.s XXX/cpuid-x86.s XXX/sha1-ssse3-x86.s \
	XXX/sha256-ssse3-x86.s XXX/sha512-ssse3-x86.s XXX/aes-ssse3-x86.s

X86_64_FILES=XXX/aesni-x86_64.s XXX/cpuid-x86_64.s XXX/ghash-x86_64.s \
	XXX/sha1-ssse3-x86_64.s XXX/sha512-ssse3-x86_64.s XXX/aes-ssse3-x86_64.s

X86_PADLOCK_FILES=XXX/e_padlock-x86.s
X86_64_PADLOCK_FILES=XXX/e_padlock-x86_64.s

X86_FILES_ELF := $(subst XXX,elf,$(X86_FILES))
X86_FILES_COFF := $(subst XXX,coff,$(X86_FILES))
X86_FILES_MACOSX := $(subst XXX,macosx,$(X86_FILES))
X86_64_FILES_ELF := $(subst XXX,elf,$(X86_64_FILES))
X86_64_FILES_COFF := $(subst XXX,coff,$(X86_64_FILES))
X86_64_FILES_MACOSX := $(subst XXX,macosx,$(X86_64_FILES))

X86_PADLOCK_FILES_ELF := $(subst XXX,elf,$(X86_PADLOCK_FILES))
X86_PADLOCK_FILES_COFF := $(subst XXX,coff,$(X86_PADLOCK_FILES))
X86_PADLOCK_FILES_MACOSX := $(subst XXX,macosx,$(X86_PADLOCK_FILES))
X86_64_PADLOCK_FILES_ELF := $(subst XXX,elf,$(X86_64_PADLOCK_FILES))
X86_64_PADLOCK_FILES_COFF := $(subst XXX,coff,$(X86_64_PADLOCK_FILES))
X86_64_PADLOCK_FILES_MACOSX := $(subst XXX,macosx,$(X86_64_PADLOCK_FILES))

lib/accelerated/x86/files.mk: $(ASM_SOURCES_ELF)
	echo X86_FILES_ELF=$(X86_FILES_ELF) > $@.tmp
	echo X86_FILES_COFF=$(X86_FILES_COFF) >> $@.tmp
	echo X86_FILES_MACOSX=$(X86_FILES_MACOSX) >> $@.tmp
	echo X86_64_FILES_ELF=$(X86_64_FILES_ELF) >> $@.tmp
	echo X86_64_FILES_COFF=$(X86_64_FILES_COFF) >> $@.tmp
	echo X86_64_FILES_MACOSX=$(X86_64_FILES_MACOSX) >> $@.tmp
	echo X86_PADLOCK_FILES_ELF=$(X86_PADLOCK_FILES_ELF) >> $@.tmp
	echo X86_PADLOCK_FILES_COFF=$(X86_PADLOCK_FILES_COFF) >> $@.tmp
	echo X86_PADLOCK_FILES_MACOSX=$(X86_PADLOCK_FILES_MACOSX) >> $@.tmp
	echo X86_64_PADLOCK_FILES_ELF=$(X86_64_PADLOCK_FILES_ELF) >> $@.tmp
	echo X86_64_PADLOCK_FILES_COFF=$(X86_64_PADLOCK_FILES_COFF) >> $@.tmp
	echo X86_64_PADLOCK_FILES_MACOSX=$(X86_64_PADLOCK_FILES_MACOSX) >> $@.tmp
	mv $@.tmp $@

# Appro's code
lib/accelerated/x86/elf/%.s: devel/perlasm/%.pl .submodule.stamp 
	cat $<.license > $@
	perl $< elf >> $@
	echo "" >> $@
	echo ".section .note.GNU-stack,\"\",%progbits" >> $@
	sed -i 's/OPENSSL_ia32cap_P/_gnutls_x86_cpuid_s/g' $@

lib/accelerated/x86/coff/%-x86.s: devel/perlasm/%-x86.pl .submodule.stamp 
	cat $<.license > $@
	perl $< coff >> $@
	echo "" >> $@
	sed -i 's/OPENSSL_ia32cap_P/_gnutls_x86_cpuid_s/g' $@

lib/accelerated/x86/coff/%-x86_64.s: devel/perlasm/%-x86_64.pl .submodule.stamp 
	cat $<.license > $@
	perl $< mingw64 >> $@
	echo "" >> $@
	sed -i 's/OPENSSL_ia32cap_P/_gnutls_x86_cpuid_s/g' $@

lib/accelerated/x86/macosx/%.s: devel/perlasm/%.pl .submodule.stamp 
	cat $<.license > $@
	perl $< macosx >> $@
	echo "" >> $@
	sed -i 's/OPENSSL_ia32cap_P/_gnutls_x86_cpuid_s/g' $@
