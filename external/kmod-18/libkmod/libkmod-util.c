/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 * Copyright (C) 2012  Lucas De Marchi <lucas.de.marchi@gmail.com>
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "libkmod.h"
#include "libkmod-internal.h"

/*
 * Read one logical line from a configuration file.
 *
 * Line endings may be escaped with backslashes, to form one logical line from
 * several physical lines.  No end of line character(s) are included in the
 * result.
 *
 * If linenum is not NULL, it is incremented by the number of physical lines
 * which have been read.
 */
char *getline_wrapped(FILE *fp, unsigned int *linenum)
{
	int size = 256;
	int i = 0, n = 0;
	_cleanup_free_ char *buf = malloc(size);

	if (buf == NULL)
		return NULL;

	for(;;) {
		int ch = getc_unlocked(fp);

		switch(ch) {
		case EOF:
			if (i == 0)
				return NULL;
			/* else fall through */

		case '\n':
			n++;

			{
				char *ret;
				if (i == size) {
					ret = realloc(buf, size + 1);
					if (!ret)
						return NULL;
				} else
					ret = buf;
				ret[i] = '\0';
				buf = NULL;
				if (linenum)
					*linenum += n;
				return ret;
			}

		case '\\':
			ch = getc_unlocked(fp);

			if (ch == '\n') {
				n++;
				continue;
			}
			/* else fall through */

		default:
			buf[i++] = ch;

			if (i == size) {
				char *tmp;
				size *= 2;
				tmp = realloc(buf, size);
				if (!tmp)
					return NULL;
				buf = tmp;
			}
		}
	}
}

inline int alias_normalize(const char *alias, char buf[PATH_MAX], size_t *len)
{
	size_t s;

	for (s = 0; s < PATH_MAX - 1; s++) {
		const char c = alias[s];
		switch (c) {
		case '-':
			buf[s] = '_';
			break;
		case ']':
			return -EINVAL;
		case '[':
			while (alias[s] != ']' && alias[s] != '\0') {
				buf[s] = alias[s];
				s++;
			}

			if (alias[s] != ']')
				return -EINVAL;

			buf[s] = alias[s];
			break;
		case '\0':
			goto finish;
		default:
			buf[s] = c;
		}
	}

finish:
	buf[s] = '\0';

	if (len)
		*len = s;

	return 0;
}

inline char *modname_normalize(const char *modname, char buf[PATH_MAX],
								size_t *len)
{
	size_t s;

	for (s = 0; s < PATH_MAX - 1; s++) {
		const char c = modname[s];
		if (c == '-')
			buf[s] = '_';
		else if (c == '\0' || c == '.')
			break;
		else
			buf[s] = c;
	}

	buf[s] = '\0';

	if (len)
		*len = s;

	return buf;
}

char *path_to_modname(const char *path, char buf[PATH_MAX], size_t *len)
{
	char *modname;

	modname = basename(path);
	if (modname == NULL || modname[0] == '\0')
		return NULL;

	return modname_normalize(modname, buf, len);
}

inline void *memdup(const void *p, size_t n)
{
	void *r = malloc(n);

	if (r == NULL)
		return NULL;

	return memcpy(r, p, n);
}

ssize_t read_str_safe(int fd, char *buf, size_t buflen)
{
	size_t todo = buflen - 1;
	size_t done = 0;

	do {
		ssize_t r = read(fd, buf + done, todo);

		if (r == 0)
			break;
		else if (r > 0) {
			todo -= r;
			done += r;
		} else {
			if (errno == EAGAIN || errno == EWOULDBLOCK ||
								errno == EINTR)
				continue;
			else
				return -errno;
		}
	} while (todo > 0);

	buf[done] = '\0';
	return done;
}

ssize_t write_str_safe(int fd, const char *buf, size_t buflen)
{
	size_t todo = buflen;
	size_t done = 0;

	do {
		ssize_t r = write(fd, buf + done, todo);

		if (r == 0)
			break;
		else if (r > 0) {
			todo -= r;
			done += r;
		} else {
			if (errno == EAGAIN || errno == EWOULDBLOCK ||
								errno == EINTR)
				continue;
			else
				return -errno;
		}
	} while (todo > 0);

	return done;
}

int read_str_long(int fd, long *value, int base)
{
	char buf[32], *end;
	long v;
	int err;

	*value = 0;
	err = read_str_safe(fd, buf, sizeof(buf));
	if (err < 0)
		return err;
	errno = 0;
	v = strtol(buf, &end, base);
	if (end == buf || !isspace(*end))
		return -EINVAL;

	*value = v;
	return 0;
}

int read_str_ulong(int fd, unsigned long *value, int base)
{
	char buf[32], *end;
	long v;
	int err;

	*value = 0;
	err = read_str_safe(fd, buf, sizeof(buf));
	if (err < 0)
		return err;
	errno = 0;
	v = strtoul(buf, &end, base);
	if (end == buf || !isspace(*end))
		return -EINVAL;
	*value = v;
	return 0;
}

char *strchr_replace(char *s, int c, char r)
{
	char *p;

	for (p = s; *p != '\0'; p++)
		if (*p == c)
			*p = r;

	return s;
}

bool path_is_absolute(const char *p)
{
	assert(p != NULL);

	return p[0] == '/';
}

char *path_make_absolute_cwd(const char *p)
{
	_cleanup_free_ char *cwd = NULL;
	size_t plen, cwdlen;
	char *r;

	if (path_is_absolute(p))
		return strdup(p);

	cwd = get_current_dir_name();
	if (!cwd)
		return NULL;

	plen = strlen(p);
	cwdlen = strlen(cwd);

	/* cwd + '/' + p + '\0' */
	r = realloc(cwd, cwdlen + 1 + plen + 1);
	if (r == NULL)
		return NULL;

	cwd = NULL;
	r[cwdlen] = '/';
	memcpy(&r[cwdlen + 1], p, plen + 1);

	return r;
}

static inline int is_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st) >= 0)
		return S_ISDIR(st.st_mode);

	return -errno;
}

int mkdir_p(const char *path, int len, mode_t mode)
{
	char *start, *end;

	start = strndupa(path, len);
	end = start + len;

	/*
	 * scan backwards, replacing '/' with '\0' while the component doesn't
	 * exist
	 */
	for (;;) {
		int r = is_dir(start);
		if (r > 0) {
			end += strlen(end);

			if (end == start + len)
				return 0;

			/* end != start, since it would be caught on the first
			 * iteration */
			*end = '/';
			break;
		} else if (r == 0)
			return -ENOTDIR;

		if (end == start)
			break;

		*end = '\0';

		/* Find the next component, backwards, discarding extra '/'*/
		while (end > start && *end != '/')
			end--;

		while (end > start && *(end - 1) == '/')
			end--;
	}

	for (; end < start + len;) {
		if (mkdir(start, mode) < 0 && errno != EEXIST)
			return -errno;

		end += strlen(end);
		*end = '/';
	}

	return 0;
}

int mkdir_parents(const char *path, mode_t mode)
{
	char *end = strrchr(path, '/');

	/* no parent directories */
	if (end == NULL)
		return 0;

	return mkdir_p(path, end - path, mode);
}

const struct kmod_ext kmod_exts[] = {
	{".ko", sizeof(".ko") - 1},
#ifdef ENABLE_ZLIB
	{".ko.gz", sizeof(".ko.gz") - 1},
#endif
#ifdef ENABLE_XZ
	{".ko.xz", sizeof(".ko.xz") - 1},
#endif
	{ }
};

bool path_ends_with_kmod_ext(const char *path, size_t len)
{
	const struct kmod_ext *eitr;

	for (eitr = kmod_exts; eitr->ext != NULL; eitr++) {
		if (len <= eitr->len)
			continue;
		if (streq(path + len - eitr->len, eitr->ext))
			return true;
	}

	return false;
}

#define USEC_PER_SEC  1000000ULL
#define NSEC_PER_USEC 1000ULL
unsigned long long ts_usec(const struct timespec *ts)
{
	return (unsigned long long) ts->tv_sec * USEC_PER_SEC +
	       (unsigned long long) ts->tv_nsec / NSEC_PER_USEC;
}

unsigned long long stat_mstamp(const struct stat *st)
{
#ifdef HAVE_STRUCT_STAT_ST_MTIM
	return ts_usec(&st->st_mtim);
#else
	return (unsigned long long) st->st_mtime;
#endif
}
