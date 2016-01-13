#pragma once

#include <unistd.h>
#include <sys/syscall.h>

#ifdef HAVE_LINUX_MODULE_H
#include <linux/module.h>
#endif

#ifndef MODULE_INIT_IGNORE_MODVERSIONS
# define MODULE_INIT_IGNORE_MODVERSIONS 1
#endif

#ifndef MODULE_INIT_IGNORE_VERMAGIC
# define MODULE_INIT_IGNORE_VERMAGIC 2
#endif

#ifndef __NR_finit_module
# define __NR_finit_module -1
#endif

#ifndef HAVE_FINIT_MODULE
#include <errno.h>

static inline int finit_module(int fd, const char *uargs, int flags)
{
	if (__NR_finit_module == -1) {
		errno = ENOSYS;
		return -1;
	}

	return syscall(__NR_finit_module, fd, uargs, flags);
}
#endif

#if !HAVE_DECL_STRNDUPA
#define strndupa(s, n)							\
	({								\
		const char *__old = (s);				\
		size_t __len = strnlen(__old, (n));			\
		char *__new = alloca(__len + 1);			\
		__new[__len] = '\0';					\
		memcpy(__new, __old, __len);				\
	 })
#endif
