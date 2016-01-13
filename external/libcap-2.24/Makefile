#
# Makefile for libcap
#
topdir=$(shell pwd)
include Make.Rules

#
# flags
#

all install clean: %: %-here
	$(MAKE) -C libcap $@
ifneq ($(PAM_CAP),no)
	$(MAKE) -C pam_cap $@
endif
	$(MAKE) -C progs $@
	$(MAKE) -C doc $@

all-here:

install-here:

clean-here:
	$(LOCALCLEAN)

distclean: clean
	$(DISTCLEAN)

release: distclean
	cd .. && ln -s libcap libcap-$(VERSION).$(MINOR) && tar cvf libcap-$(VERSION).$(MINOR).tar libcap-$(VERSION).$(MINOR)/* && rm libcap-$(VERSION).$(MINOR)
	cd .. && gpg -sba -u E2CCF3F4 libcap-$(VERSION).$(MINOR).tar

tagrelease: distclean
	@echo "sign the tag twice: older DSA key; and newer RSA kernel.org key"
	git tag -u D41A6DF2 -s libcap-$(VERSION).$(MINOR)
	git tag -u E2CCF3F4 -s libcap-korg-$(VERSION).$(MINOR)
	make release
