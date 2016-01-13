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

#include <arpa/inet.h> /* htonl */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fnmatch.h>
#include <assert.h>
#include <inttypes.h>

#include "libkmod-internal.h"
#include "libkmod-index.h"
#include "macro.h"

/* index.c: module index file shared functions for modprobe and depmod */

#define INDEX_CHILDMAX 128

/* Disk format:

   uint32_t magic = INDEX_MAGIC;
   uint32_t version = INDEX_VERSION;
   uint32_t root_offset;

   (node_offset & INDEX_NODE_MASK) specifies the file offset of nodes:

        char[] prefix; // nul terminated

        char first;
        char last;
        uint32_t children[last - first + 1];

        uint32_t value_count;
        struct {
            uint32_t priority;
            char[] value; // nul terminated
        } values[value_count];

   (node_offset & INDEX_NODE_FLAGS) indicates which fields are present.
   Empty prefixes are omitted, leaf nodes omit the three child-related fields.

   This could be optimised further by adding a sparse child format
   (indicated using a new flag).
 */

/* Format of node offsets within index file */
enum node_offset {
	INDEX_NODE_FLAGS    = 0xF0000000, /* Flags in high nibble */
	INDEX_NODE_PREFIX   = 0x80000000,
	INDEX_NODE_VALUES = 0x40000000,
	INDEX_NODE_CHILDS   = 0x20000000,

	INDEX_NODE_MASK     = 0x0FFFFFFF, /* Offset value */
};

void index_values_free(struct index_value *values)
{
	while (values) {
		struct index_value *value = values;

		values = value->next;
		free(value);
	}
}

static int add_value(struct index_value **values,
		     const char *value, unsigned len, unsigned int priority)
{
	struct index_value *v;

	/* find position to insert value */
	while (*values && (*values)->priority < priority)
		values = &(*values)->next;

	v = malloc(sizeof(struct index_value) + len + 1);
	if (!v)
		return -1;
	v->next = *values;
	v->priority = priority;
	v->len = len;
	memcpy(v->value, value, len);
	v->value[len] = '\0';
	*values = v;

	return 0;
}

static void read_error(void)
{
	fatal("Module index: unexpected error: %s\n"
			"Try re-running depmod\n", errno ? strerror(errno) : "EOF");
}

static int read_char(FILE *in)
{
	int ch;

	errno = 0;
	ch = getc_unlocked(in);
	if (ch == EOF)
		read_error();
	return ch;
}

static uint32_t read_long(FILE *in)
{
	uint32_t l;

	errno = 0;
	if (fread(&l, sizeof(uint32_t), 1, in) != sizeof(uint32_t))
		read_error();
	return ntohl(l);
}

/*
 * Buffer abstract data type
 *
 * Used internally to store the current path during tree traversal.
 * They help build wildcard key strings to pass to fnmatch(),
 * as well as building values of matching keys.
 */
struct buffer {
	char *bytes;
	unsigned size;
	unsigned used;
};

#define BUF_STEP (2048)
static bool buf_grow(struct buffer *buf, size_t newsize)
{
	void *tmp;
	size_t sz;

	if (newsize % BUF_STEP == 0)
		sz = newsize;
	else
		sz = ((newsize / BUF_STEP) + 1) * BUF_STEP;

	if (buf->size == sz)
		return true;

	tmp = realloc(buf->bytes, sz);
	if (sz > 0 && tmp == NULL)
		return false;
	buf->bytes = tmp;
	buf->size = sz;
	return true;
}

static void buf_init(struct buffer *buf)
{
	buf->bytes = NULL;
	buf->size = 0;
	buf->used = 0;
}

static void buf_release(struct buffer *buf)
{
	free(buf->bytes);
}

/* Destroy buffer and return a copy as a C string */
static char *buf_steal(struct buffer *buf)
{
	char *bytes;

	bytes = realloc(buf->bytes, buf->used + 1);
	if (!bytes) {
		free(buf->bytes);
		return NULL;
	}
	bytes[buf->used] = '\0';
	return bytes;
}

/* Return a C string owned by the buffer
   (invalidated if the buffer is changed).
 */
static const char *buf_str(struct buffer *buf)
{
	if (!buf_grow(buf, buf->used + 1))
		return NULL;
	buf->bytes[buf->used] = '\0';
	return buf->bytes;
}

static bool buf_pushchar(struct buffer *buf, char ch)
{
	if (!buf_grow(buf, buf->used + 1))
		return false;
	buf->bytes[buf->used] = ch;
	buf->used++;
	return true;
}

static unsigned buf_pushchars(struct buffer *buf, const char *str)
{
	unsigned i = 0;
	int ch;

	while ((ch = str[i])) {
		buf_pushchar(buf, ch);
		i++;
	}

	return i;
}

static unsigned buf_freadchars(struct buffer *buf, FILE *in)
{
	unsigned i = 0;
	int ch;

	while ((ch = read_char(in))) {
		if (!buf_pushchar(buf, ch))
			break;
		i++;
	}

	return i;
}

static void buf_popchar(struct buffer *buf)
{
	assert(buf->used > 0);
	buf->used--;
}

static void buf_popchars(struct buffer *buf, unsigned n)
{
	assert(buf->used >= n);
	buf->used -= n;
}

static void buf_clear(struct buffer *buf)
{
	buf->used = 0;
}

/*
 * Index file searching
 */
struct index_node_f {
	FILE *file;
	char *prefix;		/* path compression */
	struct index_value *values;
	unsigned char first;	/* range of child nodes */
	unsigned char last;
	uint32_t children[0];
};

static struct index_node_f *index_read(FILE *in, uint32_t offset)
{
	struct index_node_f *node;
	char *prefix;
	int i, child_count = 0;

	if ((offset & INDEX_NODE_MASK) == 0)
		return NULL;

	fseek(in, offset & INDEX_NODE_MASK, SEEK_SET);

	if (offset & INDEX_NODE_PREFIX) {
		struct buffer buf;
		buf_init(&buf);
		buf_freadchars(&buf, in);
		prefix = buf_steal(&buf);
	} else
		prefix = NOFAIL(strdup(""));

	if (offset & INDEX_NODE_CHILDS) {
		char first = read_char(in);
		char last = read_char(in);
		child_count = last - first + 1;

		node = NOFAIL(malloc(sizeof(struct index_node_f) +
				     sizeof(uint32_t) * child_count));

		node->first = first;
		node->last = last;

		for (i = 0; i < child_count; i++)
			node->children[i] = read_long(in);
	} else {
		node = NOFAIL(malloc(sizeof(struct index_node_f)));
		node->first = INDEX_CHILDMAX;
		node->last = 0;
	}

	node->values = NULL;
	if (offset & INDEX_NODE_VALUES) {
		int value_count;
		struct buffer buf;
		const char *value;
		unsigned int priority;

		value_count = read_long(in);

		buf_init(&buf);
		while (value_count--) {
			priority = read_long(in);
			buf_freadchars(&buf, in);
			value = buf_str(&buf);
			add_value(&node->values, value, buf.used, priority);
			buf_clear(&buf);
		}
		buf_release(&buf);
	}

	node->prefix = prefix;
	node->file = in;
	return node;
}

static void index_close(struct index_node_f *node)
{
	free(node->prefix);
	index_values_free(node->values);
	free(node);
}

struct index_file {
	FILE *file;
	uint32_t root_offset;
};

/* Failures are silent; modprobe will fall back to text files */
struct index_file *index_file_open(const char *filename)
{
	FILE *file;
	uint32_t magic, version;
	struct index_file *new;

	file = fopen(filename, "re");
	if (!file)
		return NULL;
	errno = EINVAL;

	magic = read_long(file);
	if (magic != INDEX_MAGIC) {
		fclose(file);
		return NULL;
	}

	version = read_long(file);
	if (version >> 16 != INDEX_VERSION_MAJOR) {
		fclose(file);
		return NULL;
	}

	new = NOFAIL(malloc(sizeof(struct index_file)));
	new->file = file;
	new->root_offset = read_long(new->file);

	errno = 0;
	return new;
}

void index_file_close(struct index_file *idx)
{
	fclose(idx->file);
	free(idx);
}

static struct index_node_f *index_readroot(struct index_file *in)
{
	return index_read(in->file, in->root_offset);
}

static struct index_node_f *index_readchild(const struct index_node_f *parent,
					    int ch)
{
	if (parent->first <= ch && ch <= parent->last) {
		return index_read(parent->file,
		                       parent->children[ch - parent->first]);
	}

	return NULL;
}

static void index_dump_node(struct index_node_f *node, struct buffer *buf,
								int fd)
{
	struct index_value *v;
	int ch, pushed;

	pushed = buf_pushchars(buf, node->prefix);

	for (v = node->values; v != NULL; v = v->next) {
		write_str_safe(fd, buf->bytes, buf->used);
		write_str_safe(fd, " ", 1);
		write_str_safe(fd, v->value, strlen(v->value));
		write_str_safe(fd, "\n", 1);
	}

	for (ch = node->first; ch <= node->last; ch++) {
		struct index_node_f *child = index_readchild(node, ch);

		if (!child)
			continue;

		buf_pushchar(buf, ch);
		index_dump_node(child, buf, fd);
		buf_popchar(buf);
	}

	buf_popchars(buf, pushed);
	index_close(node);
}

void index_dump(struct index_file *in, int fd, const char *prefix)
{
	struct index_node_f *root;
	struct buffer buf;

	root = index_readroot(in);
	if (root == NULL)
		return;

	buf_init(&buf);
	buf_pushchars(&buf, prefix);
	index_dump_node(root, &buf, fd);
	buf_release(&buf);
}

static char *index_search__node(struct index_node_f *node, const char *key, int i)
{
	char *value;
	struct index_node_f *child;
	int ch;
	int j;

	while(node) {
		for (j = 0; node->prefix[j]; j++) {
			ch = node->prefix[j];

			if (ch != key[i+j]) {
				index_close(node);
				return NULL;
			}
		}

		i += j;

		if (key[i] == '\0') {
			value = node->values != NULL
				? strdup(node->values[0].value)
				: NULL;

			index_close(node);
			return value;
		}

		child = index_readchild(node, key[i]);
		index_close(node);
		node = child;
		i++;
	}

	return NULL;
}

/*
 * Search the index for a key
 *
 * Returns the value of the first match
 *
 * The recursive functions free their node argument (using index_close).
 */
char *index_search(struct index_file *in, const char *key)
{
// FIXME: return value by reference instead of strdup
	struct index_node_f *root;
	char *value;

	root = index_readroot(in);
	value = index_search__node(root, key, 0);

	return value;
}



/* Level 4: add all the values from a matching node */
static void index_searchwild__allvalues(struct index_node_f *node,
					struct index_value **out)
{
	struct index_value *v;

	for (v = node->values; v != NULL; v = v->next)
		add_value(out, v->value, v->len, v->priority);

	index_close(node);
}

/*
 * Level 3: traverse a sub-keyspace which starts with a wildcard,
 * looking for matches.
 */
static void index_searchwild__all(struct index_node_f *node, int j,
				  struct buffer *buf,
				  const char *subkey,
				  struct index_value **out)
{
	int pushed = 0;
	int ch;

	while (node->prefix[j]) {
		ch = node->prefix[j];

		buf_pushchar(buf, ch);
		pushed++;
		j++;
	}

	for (ch = node->first; ch <= node->last; ch++) {
		struct index_node_f *child = index_readchild(node, ch);

		if (!child)
			continue;

		buf_pushchar(buf, ch);
		index_searchwild__all(child, 0, buf, subkey, out);
		buf_popchar(buf);
	}

	if (node->values) {
		if (fnmatch(buf_str(buf), subkey, 0) == 0)
			index_searchwild__allvalues(node, out);
		else
			index_close(node);
	} else {
		index_close(node);
	}

	buf_popchars(buf, pushed);
}

/* Level 2: descend the tree (until we hit a wildcard) */
static void index_searchwild__node(struct index_node_f *node,
				   struct buffer *buf,
				   const char *key, int i,
				   struct index_value **out)
{
	struct index_node_f *child;
	int j;
	int ch;

	while(node) {
		for (j = 0; node->prefix[j]; j++) {
			ch = node->prefix[j];

			if (ch == '*' || ch == '?' || ch == '[') {
				index_searchwild__all(node, j, buf,
						      &key[i+j], out);
				return;
			}

			if (ch != key[i+j]) {
				index_close(node);
				return;
			}
		}

		i += j;

		child = index_readchild(node, '*');
		if (child) {
			buf_pushchar(buf, '*');
			index_searchwild__all(child, 0, buf, &key[i], out);
			buf_popchar(buf);
		}

		child = index_readchild(node, '?');
		if (child) {
			buf_pushchar(buf, '?');
			index_searchwild__all(child, 0, buf, &key[i], out);
			buf_popchar(buf);
		}

		child = index_readchild(node, '[');
		if (child) {
			buf_pushchar(buf, '[');
			index_searchwild__all(child, 0, buf, &key[i], out);
			buf_popchar(buf);
		}

		if (key[i] == '\0') {
			index_searchwild__allvalues(node, out);

			return;
		}

		child = index_readchild(node, key[i]);
		index_close(node);
		node = child;
		i++;
	}
}

/*
 * Search the index for a key.  The index may contain wildcards.
 *
 * Returns a list of all the values of matching keys.
 */
struct index_value *index_searchwild(struct index_file *in, const char *key)
{
	struct index_node_f *root = index_readroot(in);
	struct buffer buf;
	struct index_value *out = NULL;

	buf_init(&buf);
	index_searchwild__node(root, &buf, key, 0, &out);
	buf_release(&buf);
	return out;
}

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static const char _idx_empty_str[] = "";

/**************************************************************************/
/*
 * Alternative implementation, using mmap to map all the file to memory when
 * starting
 */
struct index_mm {
	struct kmod_ctx *ctx;
	void *mm;
	uint32_t root_offset;
	size_t size;
};

struct index_mm_value {
	unsigned int priority;
	unsigned int len;
	const char *value;
};

struct index_mm_value_array {
	struct index_mm_value *values;
	unsigned int len;
};

struct index_mm_node {
	struct index_mm *idx;
	const char *prefix; /* mmape'd value */
	struct index_mm_value_array values;
	unsigned char first;
	unsigned char last;
	uint32_t children[];
};

static inline uint32_t read_long_mm(void **p)
{
	uint8_t *addr = *(uint8_t **)p;
	uint32_t v;

	/* addr may be unalined to uint32_t */
	v = get_unaligned((uint32_t *) addr);

	*p = addr + sizeof(uint32_t);
	return ntohl(v);
}

static inline uint8_t read_char_mm(void **p)
{
	uint8_t *addr = *(uint8_t **)p;
	uint8_t v = *addr;
	*p = addr + sizeof(uint8_t);
	return v;
}

static inline char *read_chars_mm(void **p, unsigned *rlen)
{
	char *addr = *(char **)p;
	size_t len = *rlen = strlen(addr);
	*p = addr + len + 1;
	return addr;
}

static struct index_mm_node *index_mm_read_node(struct index_mm *idx,
							uint32_t offset) {
	void *p = idx->mm;
	struct index_mm_node *node;
	const char *prefix;
	int i, child_count, value_count, children_padding;
	uint32_t children[INDEX_CHILDMAX];
	char first, last;

	if ((offset & INDEX_NODE_MASK) == 0)
		return NULL;

	p = (char *)p + (offset & INDEX_NODE_MASK);

	if (offset & INDEX_NODE_PREFIX) {
		unsigned len;
		prefix = read_chars_mm(&p, &len);
	} else
		prefix = _idx_empty_str;

	if (offset & INDEX_NODE_CHILDS) {
		first = read_char_mm(&p);
		last = read_char_mm(&p);
		child_count = last - first + 1;
		for (i = 0; i < child_count; i++)
			children[i] = read_long_mm(&p);
	} else {
		first = INDEX_CHILDMAX;
		last = 0;
		child_count = 0;
	}

	children_padding = (sizeof(struct index_mm_node) +
			    (sizeof(uint32_t) * child_count)) % sizeof(void *);

	if (offset & INDEX_NODE_VALUES)
		value_count = read_long_mm(&p);
	else
		value_count = 0;

	node = malloc(sizeof(struct index_mm_node)
		      + sizeof(uint32_t) * child_count + children_padding
		      + sizeof(struct index_mm_value) * value_count);
	if (node == NULL)
		return NULL;

	node->idx = idx;
	node->prefix = prefix;
	if (value_count == 0)
		node->values.values = NULL;
	else {
		node->values.values = (struct index_mm_value *)
			((char *)node + sizeof(struct index_mm_node) +
			 sizeof(uint32_t) * child_count + children_padding);
	}
	node->values.len = value_count;
	node->first = first;
	node->last = last;
	memcpy(node->children, children, sizeof(uint32_t) * child_count);

	for (i = 0; i < value_count; i++) {
		struct index_mm_value *v = node->values.values + i;
		v->priority = read_long_mm(&p);
		v->value = read_chars_mm(&p, &v->len);
	}

	return node;
}

static void index_mm_free_node(struct index_mm_node *node)
{
	free(node);
}

struct index_mm *index_mm_open(struct kmod_ctx *ctx, const char *filename,
						unsigned long long *stamp)
{
	int fd;
	struct stat st;
	struct index_mm *idx;
	struct {
		uint32_t magic;
		uint32_t version;
		uint32_t root_offset;
	} hdr;
	void *p;

	DBG(ctx, "file=%s\n", filename);

	idx = malloc(sizeof(*idx));
	if (idx == NULL) {
		ERR(ctx, "malloc: %m\n");
		return NULL;
	}

	if ((fd = open(filename, O_RDONLY|O_CLOEXEC)) < 0) {
		DBG(ctx, "open(%s, O_RDONLY|O_CLOEXEC): %m\n", filename);
		goto fail_open;
	}

	if (fstat(fd, &st) < 0)
		goto fail_nommap;
	if ((size_t) st.st_size < sizeof(hdr))
		goto fail_nommap;

	if ((idx->mm = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0))
							== MAP_FAILED) {
		ERR(ctx, "mmap(NULL, %"PRIu64", PROT_READ, %d, MAP_PRIVATE, 0): %m\n",
							st.st_size, fd);
		goto fail_nommap;
	}

	p = idx->mm;
	hdr.magic = read_long_mm(&p);
	hdr.version = read_long_mm(&p);
	hdr.root_offset = read_long_mm(&p);

	if (hdr.magic != INDEX_MAGIC) {
		ERR(ctx, "magic check fail: %x instead of %x\n", hdr.magic,
								INDEX_MAGIC);
		goto fail;
	}

	if (hdr.version >> 16 != INDEX_VERSION_MAJOR) {
		ERR(ctx, "major version check fail: %u instead of %u\n",
						hdr.version, INDEX_MAGIC);
		goto fail;
	}

	idx->root_offset = hdr.root_offset;
	idx->size = st.st_size;
	idx->ctx = ctx;
	close(fd);

	*stamp = stat_mstamp(&st);

	return idx;

fail:
	munmap(idx->mm, st.st_size);
fail_nommap:
	close(fd);
fail_open:
	free(idx);
	return NULL;
}

void index_mm_close(struct index_mm *idx)
{
	munmap(idx->mm, idx->size);
	free(idx);
}

static struct index_mm_node *index_mm_readroot(struct index_mm *idx)
{
	return index_mm_read_node(idx, idx->root_offset);
}

static struct index_mm_node *index_mm_readchild(const struct index_mm_node *parent,
									int ch)
{
	if (parent->first <= ch && ch <= parent->last) {
		return index_mm_read_node(parent->idx,
					parent->children[ch - parent->first]);
	}

	return NULL;
}

static void index_mm_dump_node(struct index_mm_node *node, struct buffer *buf,
								int fd)
{
	struct index_mm_value *itr, *itr_end;
	int ch, pushed;

	pushed = buf_pushchars(buf, node->prefix);

	itr = node->values.values;
	itr_end = itr + node->values.len;
	for (; itr < itr_end; itr++) {
		write_str_safe(fd, buf->bytes, buf->used);
		write_str_safe(fd, " ", 1);
		write_str_safe(fd, itr->value, itr->len);
		write_str_safe(fd, "\n", 1);
	}

	for (ch = node->first; ch <= node->last; ch++) {
		struct index_mm_node *child = index_mm_readchild(node, ch);

		if (child == NULL)
			continue;

		buf_pushchar(buf, ch);
		index_mm_dump_node(child, buf, fd);
		buf_popchar(buf);
	}

	buf_popchars(buf, pushed);
	index_mm_free_node(node);
}

void index_mm_dump(struct index_mm *idx, int fd, const char *prefix)
{
	struct index_mm_node *root;
	struct buffer buf;

	root = index_mm_readroot(idx);
	if (root == NULL)
		return;

	buf_init(&buf);
	buf_pushchars(&buf, prefix);
	index_mm_dump_node(root, &buf, fd);
	buf_release(&buf);
}

static char *index_mm_search_node(struct index_mm_node *node, const char *key,
									int i)
{
	char *value;
	struct index_mm_node *child;
	int ch;
	int j;

	while(node) {
		for (j = 0; node->prefix[j]; j++) {
			ch = node->prefix[j];

			if (ch != key[i+j]) {
				index_mm_free_node(node);
				return NULL;
			}
		}

		i += j;

		if (key[i] == '\0') {
			value = node->values.len > 0
				? strdup(node->values.values[0].value)
				: NULL;

			index_mm_free_node(node);
			return value;
		}

		child = index_mm_readchild(node, key[i]);
		index_mm_free_node(node);
		node = child;
		i++;
	}

	return NULL;
}

/*
 * Search the index for a key
 *
 * Returns the value of the first match
 *
 * The recursive functions free their node argument (using index_close).
 */
char *index_mm_search(struct index_mm *idx, const char *key)
{
// FIXME: return value by reference instead of strdup
	struct index_mm_node *root;
	char *value;

	root = index_mm_readroot(idx);
	value = index_mm_search_node(root, key, 0);

	return value;
}

/* Level 4: add all the values from a matching node */
static void index_mm_searchwild_allvalues(struct index_mm_node *node,
						struct index_value **out)
{
	struct index_mm_value *itr, *itr_end;

	itr = node->values.values;
	itr_end = itr + node->values.len;
	for (; itr < itr_end; itr++)
		add_value(out, itr->value, itr->len, itr->priority);

	index_mm_free_node(node);
}

/*
 * Level 3: traverse a sub-keyspace which starts with a wildcard,
 * looking for matches.
 */
static void index_mm_searchwild_all(struct index_mm_node *node, int j,
					  struct buffer *buf,
					  const char *subkey,
					  struct index_value **out)
{
	int pushed = 0;
	int ch;

	while (node->prefix[j]) {
		ch = node->prefix[j];

		buf_pushchar(buf, ch);
		pushed++;
		j++;
	}

	for (ch = node->first; ch <= node->last; ch++) {
		struct index_mm_node *child = index_mm_readchild(node, ch);

		if (!child)
			continue;

		buf_pushchar(buf, ch);
		index_mm_searchwild_all(child, 0, buf, subkey, out);
		buf_popchar(buf);
	}

	if (node->values.len > 0) {
		if (fnmatch(buf_str(buf), subkey, 0) == 0)
			index_mm_searchwild_allvalues(node, out);
		else
			index_mm_free_node(node);
	} else {
		index_mm_free_node(node);
	}

	buf_popchars(buf, pushed);
}

/* Level 2: descend the tree (until we hit a wildcard) */
static void index_mm_searchwild_node(struct index_mm_node *node,
					   struct buffer *buf,
					   const char *key, int i,
					   struct index_value **out)
{
	struct index_mm_node *child;
	int j;
	int ch;

	while(node) {
		for (j = 0; node->prefix[j]; j++) {
			ch = node->prefix[j];

			if (ch == '*' || ch == '?' || ch == '[') {
				index_mm_searchwild_all(node, j, buf,
						      &key[i+j], out);
				return;
			}

			if (ch != key[i+j]) {
				index_mm_free_node(node);
				return;
			}
		}

		i += j;

		child = index_mm_readchild(node, '*');
		if (child) {
			buf_pushchar(buf, '*');
			index_mm_searchwild_all(child, 0, buf, &key[i], out);
			buf_popchar(buf);
		}

		child = index_mm_readchild(node, '?');
		if (child) {
			buf_pushchar(buf, '?');
			index_mm_searchwild_all(child, 0, buf, &key[i], out);
			buf_popchar(buf);
		}

		child = index_mm_readchild(node, '[');
		if (child) {
			buf_pushchar(buf, '[');
			index_mm_searchwild_all(child, 0, buf, &key[i], out);
			buf_popchar(buf);
		}

		if (key[i] == '\0') {
			index_mm_searchwild_allvalues(node, out);

			return;
		}

		child = index_mm_readchild(node, key[i]);
		index_mm_free_node(node);
		node = child;
		i++;
	}
}

/*
 * Search the index for a key.  The index may contain wildcards.
 *
 * Returns a list of all the values of matching keys.
 */
struct index_value *index_mm_searchwild(struct index_mm *idx, const char *key)
{
	struct index_mm_node *root = index_mm_readroot(idx);
	struct buffer buf;
	struct index_value *out = NULL;

	buf_init(&buf);
	index_mm_searchwild_node(root, &buf, key, 0, &out);
	buf_release(&buf);
	return out;
}
