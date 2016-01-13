/*
 * Copyright (C) 2012-2013  ProFUSION embedded systems
 * Copyright (C) 2012-2013  Lucas De Marchi <lucas.de.marchi@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef HAVE_FINIT_MODULE
#define HAVE_FINIT_MODULE 1
#endif

#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

/* kmod_elf_get_section() is not exported, we need the private header */
#include <libkmod-internal.h>

/* FIXME: hack, change name so we don't clash */
#undef ERR
#include "testsuite.h"
#include "stripped-module.h"

struct mod {
	struct mod *next;
	int ret;
	int errcode;
	char name[];
};

static struct mod *modules;
static bool need_init = true;
static struct kmod_ctx *ctx;

static void parse_retcodes(struct mod *_modules, const char *s)
{
	const char *p;

	if (s == NULL)
		return;

	for (p = s;;) {
		struct mod *mod;
		const char *modname;
		char *end;
		size_t modnamelen;
		int ret, errcode;
		long l;

		modname = p;
		if (modname == NULL || modname[0] == '\0')
			break;

		modnamelen = strcspn(s, ":");
		if (modname[modnamelen] != ':')
			break;

		p = modname + modnamelen + 1;
		if (p == NULL)
			break;

		l = strtol(p, &end, 0);
		if (end == p || *end != ':')
			break;
		ret = (int) l;
		p = end + 1;

		l = strtol(p, &end, 0);
		if (*end == ':')
			p = end + 1;
		else if (*end != '\0')
			break;

		errcode = (int) l;

		mod = malloc(sizeof(*mod) + modnamelen + 1);
		if (mod == NULL)
			break;

		memcpy(mod->name, modname, modnamelen);
		mod->name[modnamelen] = '\0';
		mod->ret = ret;
		mod->errcode = errcode;
		mod->next = _modules;
		_modules = mod;
	}
}

static int write_one_line_file(const char *fn, const char *line, int len)
{
        FILE *f;
        int r;

        assert(fn);
        assert(line);

        f = fopen(fn, "we");
        if (!f)
                return -errno;

        errno = 0;
        if (fputs(line, f) < 0) {
                r = -errno;
                goto finish;
        }

        fflush(f);

        if (ferror(f)) {
                if (errno != 0)
                        r = -errno;
                else
                        r = -EIO;
        } else
                r = 0;

finish:
        fclose(f);
        return r;
}

static int create_sysfs_files(const char *modname)
{
	char buf[PATH_MAX];
	const char *sysfsmod = "/sys/module/";
	int len = strlen(sysfsmod);

	memcpy(buf, sysfsmod, len);
	strcpy(buf + len, modname);
	len += strlen(modname);

	assert(mkdir_p(buf, len, 0755) >= 0);

	strcpy(buf + len, "/initstate");
	return write_one_line_file(buf, "live\n", strlen("live\n"));
}

static struct mod *find_module(struct mod *_modules, const char *modname)
{
	struct mod *mod;

	for (mod = _modules; mod != NULL; mod = mod->next) {
		if (strcmp(mod->name, modname) == 0)
			return mod;
	}

	return NULL;
}

static void init_retcodes(void)
{
	const char *s;

	if (!need_init)
		return;

	need_init = false;
	s = getenv(S_TC_INIT_MODULE_RETCODES);
	if (s == NULL) {
		fprintf(stderr, "TRAP init_module(): missing export %s?\n",
						S_TC_INIT_MODULE_RETCODES);
	}

	ctx = kmod_new(NULL, NULL);

	parse_retcodes(modules, s);
}

static inline bool module_is_inkernel(const char *modname)
{
	struct kmod_module *mod;
	int state;
	bool ret;

	if (kmod_module_new_from_name(ctx, modname, &mod) < 0)
		return false;

	state = kmod_module_get_initstate(mod);

	if (state == KMOD_MODULE_LIVE ||
			state == KMOD_MODULE_BUILTIN)
		ret = true;
	else
		ret = false;

	kmod_module_unref(mod);

	return ret;
}

static uint8_t elf_identify(void *mem)
{
	uint8_t *p = mem;
	return p[EI_CLASS];
}

TS_EXPORT long init_module(void *mem, unsigned long len, const char *args);

/*
 * Default behavior is to try to mimic init_module behavior inside the kernel.
 * If it is a simple test that you know the error code, set the return code
 * in TESTSUITE_INIT_MODULE_RETCODES env var instead.
 *
 * The exception is when the module name is not find in the memory passed.
 * This is because we want to be able to pass dummy modules (and not real
 * ones) and it still work.
 */
long init_module(void *mem, unsigned long len, const char *args)
{
	const char *modname;
	struct kmod_elf *elf;
	struct mod *mod;
	const void *buf;
	uint64_t bufsize;
	int err;
	uint8_t class;
	off_t offset;

	init_retcodes();

	elf = kmod_elf_new(mem, len);
	if (elf == NULL)
		return 0;

	err = kmod_elf_get_section(elf, ".gnu.linkonce.this_module", &buf,
								&bufsize);
	kmod_elf_unref(elf);

	/* We couldn't parse the ELF file. Just exit as if it was successful */
	if (err < 0)
		return 0;

	/* We need to open both 32 and 64 bits module - hack! */
	class = elf_identify(mem);
	if (class == ELFCLASS64)
		offset = MODULE_NAME_OFFSET_64;
	else
		offset = MODULE_NAME_OFFSET_32;

	modname = (char *)buf + offset;
	mod = find_module(modules, modname);
	if (mod != NULL) {
		errno = mod->errcode;
		err = mod->ret;
	} else if (module_is_inkernel(modname)) {
		err = -1;
		errno = EEXIST;
	} else
		err = 0;

	if (err == 0)
		create_sysfs_files(modname);

	return err;
}

static int check_kernel_version(int major, int minor)
{
	struct utsname u;
	const char *p;
	int maj = 0, min = 0;

	if (uname(&u) < 0)
		return false;
	for (p = u.release; *p >= '0' && *p <= '9'; p++)
		maj = maj * 10 + *p - '0';
	if (*p == '.')
		for (p++; *p >= '0' && *p <= '9'; p++)
			min = min * 10 + *p - '0';
	if (maj > major || (maj == major && min >= minor))
		return true;
	return false;
}


TS_EXPORT int finit_module(const int fd, const char *args, const int flags);

int finit_module(const int fd, const char *args, const int flags)
{
	int err;
	void *mem;
	unsigned long len;
	struct stat st;

	if (!check_kernel_version(3, 8)) {
		errno = ENOSYS;
		return -1;
	}
	if (fstat(fd, &st) < 0)
		return -1;

	len = st.st_size;
	mem = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mem == MAP_FAILED)
		return -1;

	err = init_module(mem, len, args);
	munmap(mem, len);

	return err;
}

TS_EXPORT long int syscall(long int __sysno, ...)
{
	va_list ap;
	long ret;

	if (__sysno == -1) {
		errno = ENOSYS;
		return -1;
	}

	if (__sysno == __NR_finit_module) {
		const char *args;
		int flags;
		int fd;

		va_start(ap, __sysno);

		fd = va_arg(ap, int);
		args = va_arg(ap, const char *);
		flags = va_arg(ap, int);

		ret = finit_module(fd, args, flags);

		va_end(ap);
		return ret;
	}

	/*
	 * FIXME: no way to call the libc function - let's hope there are no
	 * other users.
	 */
	abort();
}

/* the test is going away anyway, but lets keep valgrind happy */
void free_resources(void) __attribute__((destructor));
void free_resources(void)
{
	while (modules) {
		struct mod *mod = modules->next;
		free(modules);
		modules = mod;
	}

	if (ctx)
		kmod_unref(ctx);
}
