# -*- Mode: Makefile -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


INCLUDED_AUTOCONF_MK = 1
USE_AUTOCONF	= 1

MOZILLA_CLIENT	= 

prefix		= /usr
exec_prefix	= ${prefix}
bindir		= ${exec_prefix}/bin
includedir	= ${prefix}/include/nspr
libdir		= ${exec_prefix}/lib
datarootdir	= ${prefix}/share
datadir		= ${datarootdir}

dist_prefix	= /home/saml/dev/steamlink/firmware/external/nss-3.49/nss/../dist/Linux4.15_arm_glibc_PTH_OPT.OBJ
dist_bindir	= ${dist_prefix}/bin
dist_includedir = /home/saml/dev/steamlink/firmware/external/nss-3.49/nss/../dist/Linux4.15_arm_glibc_PTH_OPT.OBJ/include
dist_libdir	= ${dist_prefix}/lib

DIST		= $(dist_prefix)

RELEASE_OBJDIR_NAME = Linux._armv7a_glibc_PTH_DBG.OBJ
OBJDIR_NAME	= .
OBJDIR		= $(OBJDIR_NAME)
# We do magic with OBJ_SUFFIX in config.mk, the following ensures we don't
# manually use it before config.mk inclusion
OBJ_SUFFIX	= $(error config/config.mk needs to be included before using OBJ_SUFFIX)
_OBJ_SUFFIX	= o
LIB_SUFFIX	= a
DLL_SUFFIX	= so
ASM_SUFFIX	= s
MOD_NAME	= nspr20

MOD_MAJOR_VERSION = 4
MOD_MINOR_VERSION = 24
MOD_PATCH_VERSION = 0

LIBNSPR		= -L$(dist_libdir) -lnspr$(MOD_MAJOR_VERSION)
LIBPLC		= -L$(dist_libdir) -lplc$(MOD_MAJOR_VERSION)

CROSS_COMPILE	= 1
MOZ_OPTIMIZE	= 
MOZ_DEBUG	= 1
MOZ_DEBUG_SYMBOLS = 1

USE_CPLUS	= 
USE_IPV6	= 
USE_N32		= 
USE_X32		= 
USE_64		= 
ENABLE_STRIP	= 

USE_PTHREADS	= 1
USE_BTHREADS	= 
PTHREADS_USER	= 
CLASSIC_NSPR	= 

AS		= $(CC)
ASFLAGS		= $(CFLAGS)
CC		= armv7a-cros-linux-gnueabi-gcc --sysroot=/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve -marm -mfpu=neon -mfloat-abi=hard
CCC		= g++
NS_USE_GCC	= 1
GCC_USE_GNU_LD	= 
MSC_VER		= 
AR		= /usr/bin/ar
AR_FLAGS	= cr $@
LD		= /usr/bin/ld
RANLIB		= armv7a-cros-linux-gnueabi-ranlib
PERL		= /usr/bin/perl
RC		= 
RCFLAGS		= 
STRIP		= /usr/bin/strip
NSINSTALL	= $(MOD_DEPTH)/config/$(OBJDIR_NAME)/nsinstall
FILTER		= 
IMPLIB		= 
CYGWIN_WRAPPER	= 
MT		= 

OS_CPPFLAGS	= 
OS_CFLAGS	= $(OS_CPPFLAGS)  -Wall -pthread -g -fno-inline $(DSO_CFLAGS)
OS_CXXFLAGS	= $(OS_CPPFLAGS)  -Wall -pthread -g -fno-inline $(DSO_CFLAGS)
OS_LIBS         = -lpthread -ldl 
OS_LDFLAGS	= -static-libgcc -static-libstdc++  -z noexecstack
OS_DLLFLAGS	= 
DLLFLAGS	= 
EXEFLAGS  = 
OPTIMIZER	= 

PROFILE_GEN_CFLAGS  = -fprofile-generate
PROFILE_GEN_LDFLAGS = -fprofile-generate
PROFILE_USE_CFLAGS  = -fprofile-use -fprofile-correction -Wcoverage-mismatch
PROFILE_USE_LDFLAGS = -fprofile-use

MKSHLIB		= $(CC) $(DSO_LDOPTS) -o $@
WRAP_LDFLAGS	= 
DSO_CFLAGS	= -fPIC
DSO_LDOPTS	= -shared -Wl,-soname -Wl,$(notdir $@)

RESOLVE_LINK_SYMBOLS = 

HOST_CC		= armv7a-cros-linux-gnueabi-gcc --sysroot=/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve -marm -mfpu=neon -mfloat-abi=hard
HOST_CFLAGS	=  -DXP_UNIX
HOST_LDFLAGS	= 

DEFINES		=  -UNDEBUG -DDEBUG_saml -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DDEBUG=1 -DHAVE_VISIBILITY_HIDDEN_ATTRIBUTE=1 -DHAVE_VISIBILITY_PRAGMA=1 -DXP_UNIX=1 -D_GNU_SOURCE=1 -DHAVE_FCNTL_FILE_LOCKING=1 -DHAVE_POINTER_LOCALTIME_R=1 -DLINUX=1 -DHAVE_DLADDR=1 -DHAVE_LCHOWN=1 -DHAVE_SETPRIORITY=1 -DHAVE_STRERROR=1 -DHAVE_SYSCALL=1 -DHAVE_SECURE_GETENV=1 -D_REENTRANT=1

MDCPUCFG_H	= _linux.cfg
PR_MD_CSRCS	= linux.c
PR_MD_ASFILES	= 
PR_MD_ARCH_DIR	= unix
CPU_ARCH	= armv7a

OS_TARGET	= Linux
OS_ARCH		= Linux
OS_RELEASE	= .
OS_TEST		= armv7a

NOSUCHFILE	= /no-such-file
AIX_LINK_OPTS	= 
MOZ_OBJFORMAT	= 
ULTRASPARC_LIBRARY = 

OBJECT_MODE	= 
ifdef OBJECT_MODE
export OBJECT_MODE
endif

VISIBILITY_FLAGS = -fvisibility=hidden
WRAP_SYSTEM_INCLUDES = 

MACOSX_DEPLOYMENT_TARGET = 
ifdef MACOSX_DEPLOYMENT_TARGET
export MACOSX_DEPLOYMENT_TARGET
endif

MACOS_SDK_DIR	= 


NEXT_ROOT	= 
ifdef NEXT_ROOT
export NEXT_ROOT
endif
