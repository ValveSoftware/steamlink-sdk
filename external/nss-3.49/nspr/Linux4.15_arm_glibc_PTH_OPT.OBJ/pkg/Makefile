# 
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#! gmake

MOD_DEPTH	= ..
topsrcdir	= ../..
srcdir		= ../../pkg
VPATH		= ../../pkg

include $(MOD_DEPTH)/config/autoconf.mk

DIRS = 
ifeq ($(OS_TARGET),Linux)
DIRS = linux
endif
ifeq ($(OS_TARGET),SunOS)
DIRS = solaris
endif

publish::
	+$(LOOP_OVER_DIRS)

include $(topsrcdir)/config/rules.mk
