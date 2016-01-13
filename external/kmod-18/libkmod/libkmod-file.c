/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "libkmod.h"
#include "libkmod-internal.h"

#ifdef ENABLE_XZ
#include <lzma.h>
#endif
#ifdef ENABLE_ZLIB
#include <zlib.h>
#endif

struct kmod_file;
struct file_ops {
	int (*load)(struct kmod_file *file);
	void (*unload)(struct kmod_file *file);
};

struct kmod_file {
#ifdef ENABLE_XZ
	bool xz_used;
#endif
#ifdef ENABLE_ZLIB
	gzFile gzf;
#endif
	int fd;
	bool direct;
	off_t size;
	void *memory;
	const struct file_ops *ops;
	const struct kmod_ctx *ctx;
	struct kmod_elf *elf;
};

#ifdef ENABLE_XZ
static void xz_uncompress_belch(struct kmod_file *file, lzma_ret ret)
{
	switch (ret) {
	case LZMA_MEM_ERROR:
		ERR(file->ctx, "xz: %s\n", strerror(ENOMEM));
		break;
	case LZMA_FORMAT_ERROR:
		ERR(file->ctx, "xz: File format not recognized\n");
		break;
	case LZMA_OPTIONS_ERROR:
		ERR(file->ctx, "xz: Unsupported compression options\n");
		break;
	case LZMA_DATA_ERROR:
		ERR(file->ctx, "xz: File is corrupt\n");
		break;
	case LZMA_BUF_ERROR:
		ERR(file->ctx, "xz: Unexpected end of input\n");
		break;
	default:
		ERR(file->ctx, "xz: Internal error (bug)\n");
		break;
	}
}

static int xz_uncompress(lzma_stream *strm, struct kmod_file *file)
{
	uint8_t in_buf[BUFSIZ], out_buf[BUFSIZ];
	lzma_action action = LZMA_RUN;
	lzma_ret ret;
	void *p = NULL;
	size_t total = 0;

	strm->avail_in  = 0;
	strm->next_out  = out_buf;
	strm->avail_out = sizeof(out_buf);

	while (true) {
		if (strm->avail_in == 0) {
			ssize_t rdret = read(file->fd, in_buf, sizeof(in_buf));
			if (rdret < 0) {
				ret = -errno;
				goto out;
			}
			strm->next_in  = in_buf;
			strm->avail_in = rdret;
			if (rdret == 0)
				action = LZMA_FINISH;
		}
		ret = lzma_code(strm, action);
		if (strm->avail_out == 0 || ret != LZMA_OK) {
			size_t write_size = BUFSIZ - strm->avail_out;
			char *tmp = realloc(p, total + write_size);
			if (tmp == NULL) {
				ret = -errno;
				goto out;
			}
			memcpy(tmp + total, out_buf, write_size);
			total += write_size;
			p = tmp;
			strm->next_out = out_buf;
			strm->avail_out = BUFSIZ;
		}
		if (ret == LZMA_STREAM_END)
			break;
		if (ret != LZMA_OK) {
			xz_uncompress_belch(file, ret);
			ret = -EINVAL;
			goto out;
		}
	}
	file->xz_used = true;
	file->memory = p;
	file->size = total;
	return 0;
 out:
	free(p);
	return ret;
}

static int load_xz(struct kmod_file *file)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret lzret;
	int ret;

	lzret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
	if (lzret == LZMA_MEM_ERROR) {
		ERR(file->ctx, "xz: %s\n", strerror(ENOMEM));
		return -ENOMEM;
	} else if (lzret != LZMA_OK) {
		ERR(file->ctx, "xz: Internal error (bug)\n");
		return -EINVAL;
	}
	ret = xz_uncompress(&strm, file);
	lzma_end(&strm);
	return ret;
}

static void unload_xz(struct kmod_file *file)
{
	if (!file->xz_used)
		return;
	free(file->memory);
}

static const char magic_xz[] = {0xfd, '7', 'z', 'X', 'Z', 0};
#endif

#ifdef ENABLE_ZLIB
#define READ_STEP (4 * 1024 * 1024)
static int load_zlib(struct kmod_file *file)
{
	int err = 0;
	off_t did = 0, total = 0;
	_cleanup_free_ unsigned char *p = NULL;

	errno = 0;
	file->gzf = gzdopen(file->fd, "rb");
	if (file->gzf == NULL)
		return -errno;
	file->fd = -1; /* now owned by gzf due gzdopen() */

	for (;;) {
		int r;

		if (did == total) {
			void *tmp = realloc(p, total + READ_STEP);
			if (tmp == NULL) {
				err = -errno;
				goto error;
			}
			total += READ_STEP;
			p = tmp;
		}

		r = gzread(file->gzf, p + did, total - did);
		if (r == 0)
			break;
		else if (r < 0) {
			int gzerr;
			const char *gz_errmsg = gzerror(file->gzf, &gzerr);

			ERR(file->ctx, "gzip: %s\n", gz_errmsg);

			/* gzip might not set errno here */
			err = gzerr == Z_ERRNO ? -errno : -EINVAL;
			goto error;
		}
		did += r;
	}

	file->memory = p;
	file->size = did;
	p = NULL;
	return 0;

error:
	gzclose(file->gzf);
	return err;
}

static void unload_zlib(struct kmod_file *file)
{
	if (file->gzf == NULL)
		return;
	free(file->memory);
	gzclose(file->gzf); /* closes file->fd */
}

static const char magic_zlib[] = {0x1f, 0x8b};
#endif

static const struct comp_type {
	size_t magic_size;
	const char *magic_bytes;
	const struct file_ops ops;
} comp_types[] = {
#ifdef ENABLE_XZ
	{sizeof(magic_xz), magic_xz, {load_xz, unload_xz}},
#endif
#ifdef ENABLE_ZLIB
	{sizeof(magic_zlib), magic_zlib, {load_zlib, unload_zlib}},
#endif
	{0, NULL, {NULL, NULL}}
};

static int load_reg(struct kmod_file *file)
{
	struct stat st;

	if (fstat(file->fd, &st) < 0)
		return -errno;

	file->size = st.st_size;
	file->memory = mmap(NULL, file->size, PROT_READ, MAP_PRIVATE,
			    file->fd, 0);
	if (file->memory == MAP_FAILED)
		return -errno;
	file->direct = true;
	return 0;
}

static void unload_reg(struct kmod_file *file)
{
	munmap(file->memory, file->size);
}

static const struct file_ops reg_ops = {
	load_reg, unload_reg
};

struct kmod_elf *kmod_file_get_elf(struct kmod_file *file)
{
	if (file->elf)
		return file->elf;

	file->elf = kmod_elf_new(file->memory, file->size);
	return file->elf;
}

struct kmod_file *kmod_file_open(const struct kmod_ctx *ctx,
						const char *filename)
{
	struct kmod_file *file = calloc(1, sizeof(struct kmod_file));
	const struct comp_type *itr;
	size_t magic_size_max = 0;
	int err;

	if (file == NULL)
		return NULL;

	file->fd = open(filename, O_RDONLY|O_CLOEXEC);
	if (file->fd < 0) {
		err = -errno;
		goto error;
	}

	for (itr = comp_types; itr->ops.load != NULL; itr++) {
		if (magic_size_max < itr->magic_size)
			magic_size_max = itr->magic_size;
	}

	file->direct = false;
	if (magic_size_max > 0) {
		char *buf = alloca(magic_size_max + 1);
		ssize_t sz;

		if (buf == NULL) {
			err = -errno;
			goto error;
		}
		sz = read_str_safe(file->fd, buf, magic_size_max + 1);
		lseek(file->fd, 0, SEEK_SET);
		if (sz != (ssize_t)magic_size_max) {
			if (sz < 0)
				err = sz;
			else
				err = -EINVAL;
			goto error;
		}

		for (itr = comp_types; itr->ops.load != NULL; itr++) {
			if (memcmp(buf, itr->magic_bytes, itr->magic_size) == 0)
				break;
		}
		if (itr->ops.load != NULL)
			file->ops = &itr->ops;
	}

	if (file->ops == NULL)
		file->ops = &reg_ops;

	err = file->ops->load(file);
	file->ctx = ctx;
error:
	if (err < 0) {
		if (file->fd >= 0)
			close(file->fd);
		free(file);
		errno = -err;
		return NULL;
	}

	return file;
}

void *kmod_file_get_contents(const struct kmod_file *file)
{
	return file->memory;
}

off_t kmod_file_get_size(const struct kmod_file *file)
{
	return file->size;
}

bool kmod_file_get_direct(const struct kmod_file *file)
{
	return file->direct;
}

int kmod_file_get_fd(const struct kmod_file *file)
{
	return file->fd;
}

void kmod_file_unref(struct kmod_file *file)
{
	if (file->elf)
		kmod_elf_unref(file->elf);

	file->ops->unload(file);
	if (file->fd >= 0)
		close(file->fd);
	free(file);
}
