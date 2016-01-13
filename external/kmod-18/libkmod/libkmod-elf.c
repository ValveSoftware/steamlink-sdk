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

#include <elf.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "libkmod.h"
#include "libkmod-internal.h"

enum kmod_elf_class {
	KMOD_ELF_32 = (1 << 1),
	KMOD_ELF_64 = (1 << 2),
	KMOD_ELF_LSB = (1 << 3),
	KMOD_ELF_MSB = (1 << 4)
};

/* as defined in module-init-tools */
struct kmod_modversion32 {
	uint32_t crc;
	char name[64 - sizeof(uint32_t)];
};

struct kmod_modversion64 {
	uint64_t crc;
	char name[64 - sizeof(uint64_t)];
};

#ifdef WORDS_BIGENDIAN
static const enum kmod_elf_class native_endianess = KMOD_ELF_MSB;
#else
static const enum kmod_elf_class native_endianess = KMOD_ELF_LSB;
#endif

struct kmod_elf {
	const uint8_t *memory;
	uint8_t *changed;
	uint64_t size;
	enum kmod_elf_class class;
	struct kmod_elf_header {
		struct {
			uint64_t offset;
			uint16_t count;
			uint16_t entry_size;
		} section;
		struct {
			uint16_t section; /* index of the strings section */
			uint64_t size;
			uint64_t offset;
			uint32_t nameoff; /* offset in strings itself */
		} strings;
		uint16_t machine;
	} header;
};

//#define ENABLE_ELFDBG 1

#if defined(ENABLE_LOGGING) && defined(ENABLE_ELFDBG)
#define ELFDBG(elf, ...)			\
	_elf_dbg(elf, __FILE__, __LINE__, __func__, __VA_ARGS__);

static inline void _elf_dbg(const struct kmod_elf *elf, const char *fname, unsigned line, const char *func, const char *fmt, ...)
{
	va_list args;

	fprintf(stderr, "ELFDBG-%d%c: %s:%u %s() ",
		(elf->class & KMOD_ELF_32) ? 32 : 64,
		(elf->class & KMOD_ELF_MSB) ? 'M' : 'L',
		fname, line, func);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}
#else
#define ELFDBG(elf, ...)
#endif


static int elf_identify(const void *memory, uint64_t size)
{
	const uint8_t *p = memory;
	int class = 0;

	if (size <= EI_NIDENT || memcmp(p, ELFMAG, SELFMAG) != 0)
		return -ENOEXEC;

	switch (p[EI_CLASS]) {
	case ELFCLASS32:
		if (size <= sizeof(Elf32_Ehdr))
			return -EINVAL;
		class |= KMOD_ELF_32;
		break;
	case ELFCLASS64:
		if (size <= sizeof(Elf64_Ehdr))
			return -EINVAL;
		class |= KMOD_ELF_64;
		break;
	default:
		return -EINVAL;
	}

	switch (p[EI_DATA]) {
	case ELFDATA2LSB:
		class |= KMOD_ELF_LSB;
		break;
	case ELFDATA2MSB:
		class |= KMOD_ELF_MSB;
		break;
	default:
		return -EINVAL;
	}

	return class;
}

static inline uint64_t elf_get_uint(const struct kmod_elf *elf, uint64_t offset, uint16_t size)
{
	const uint8_t *p;
	uint64_t ret = 0;
	size_t i;

	assert(size <= sizeof(uint64_t));
	assert(offset + size <= elf->size);
	if (offset + size > elf->size) {
		ELFDBG(elf, "out of bounds: %"PRIu64" + %"PRIu16" = %"PRIu64"> %"PRIu64" (ELF size)\n",
		       offset, size, offset + size, elf->size);
		return (uint64_t)-1;
	}

	p = elf->memory + offset;
	if (elf->class & KMOD_ELF_MSB) {
		for (i = 0; i < size; i++)
			ret = (ret << 8) | p[i];
	} else {
		for (i = 1; i <= size; i++)
			ret = (ret << 8) | p[size - i];
	}

	ELFDBG(elf, "size=%"PRIu16" offset=%"PRIu64" value=%"PRIu64"\n",
	       size, offset, ret);

	return ret;
}

static inline int elf_set_uint(struct kmod_elf *elf, uint64_t offset, uint64_t size, uint64_t value)
{
	uint8_t *p;
	size_t i;

	ELFDBG(elf, "size=%"PRIu16" offset=%"PRIu64" value=%"PRIu64" write memory=%p\n",
	       size, offset, value, elf->changed);

	assert(size <= sizeof(uint64_t));
	assert(offset + size <= elf->size);
	if (offset + size > elf->size) {
		ELFDBG(elf, "out of bounds: %"PRIu64" + %"PRIu16" = %"PRIu64"> %"PRIu64" (ELF size)\n",
		       offset, size, offset + size, elf->size);
		return -1;
	}

	if (elf->changed == NULL) {
		elf->changed = malloc(elf->size);
		if (elf->changed == NULL)
			return -errno;
		memcpy(elf->changed, elf->memory, elf->size);
		elf->memory = elf->changed;
		ELFDBG(elf, "copied memory to allow writing.\n");
	}

	p = elf->changed + offset;
	if (elf->class & KMOD_ELF_MSB) {
		for (i = 1; i <= size; i++) {
			p[size - i] = value & 0xff;
			value = (value & 0xffffffffffffff00) >> 8;
		}
	} else {
		for (i = 0; i < size; i++) {
			p[i] = value & 0xff;
			value = (value & 0xffffffffffffff00) >> 8;
		}
	}

	return 0;
}

static inline const void *elf_get_mem(const struct kmod_elf *elf, uint64_t offset)
{
	assert(offset < elf->size);
	if (offset >= elf->size) {
		ELFDBG(elf, "out-of-bounds: %"PRIu64" >= %"PRIu64" (ELF size)\n",
		       offset, elf->size);
		return NULL;
	}
	return elf->memory + offset;
}

static inline const void *elf_get_section_header(const struct kmod_elf *elf, uint16_t idx)
{
	assert(idx != SHN_UNDEF);
	assert(idx < elf->header.section.count);
	if (idx == SHN_UNDEF || idx >= elf->header.section.count) {
		ELFDBG(elf, "invalid section number: %"PRIu16", last=%"PRIu16"\n",
		       idx, elf->header.section.count);
		return NULL;
	}
	return elf_get_mem(elf, elf->header.section.offset +
			   idx * elf->header.section.entry_size);
}

static inline int elf_get_section_info(const struct kmod_elf *elf, uint16_t idx, uint64_t *offset, uint64_t *size, uint32_t *nameoff)
{
	const uint8_t *p = elf_get_section_header(elf, idx);
	uint64_t min_size, off = p - elf->memory;

	if (p == NULL) {
		ELFDBG(elf, "no section at %"PRIu16"\n", idx);
		*offset = 0;
		*size = 0;
		*nameoff = 0;
		return -EINVAL;
	}

#define READV(field) \
	elf_get_uint(elf, off + offsetof(typeof(*hdr), field), sizeof(hdr->field))

	if (elf->class & KMOD_ELF_32) {
		const Elf32_Shdr *hdr _unused_ = (const Elf32_Shdr *)p;
		*size = READV(sh_size);
		*offset = READV(sh_offset);
		*nameoff = READV(sh_name);
	} else {
		const Elf64_Shdr *hdr _unused_ = (const Elf64_Shdr *)p;
		*size = READV(sh_size);
		*offset = READV(sh_offset);
		*nameoff = READV(sh_name);
	}
#undef READV

	min_size = *offset + *size;
	if (min_size > elf->size) {
		ELFDBG(elf, "out-of-bounds: %"PRIu64" >= %"PRIu64" (ELF size)\n",
		       min_size, elf->size);
		return -EINVAL;
	}

	ELFDBG(elf, "section=%"PRIu16" is: offset=%"PRIu64" size=%"PRIu64" nameoff=%"PRIu32"\n",
	       idx, *offset, *size, *nameoff);

	return 0;
}

static const char *elf_get_strings_section(const struct kmod_elf *elf, uint64_t *size)
{
	*size = elf->header.strings.size;
	return elf_get_mem(elf, elf->header.strings.offset);
}

struct kmod_elf *kmod_elf_new(const void *memory, off_t size)
{
	struct kmod_elf *elf;
	size_t hdr_size, shdr_size, min_size;
	int class;

	assert_cc(sizeof(uint16_t) == sizeof(Elf32_Half));
	assert_cc(sizeof(uint16_t) == sizeof(Elf64_Half));
	assert_cc(sizeof(uint32_t) == sizeof(Elf32_Word));
	assert_cc(sizeof(uint32_t) == sizeof(Elf64_Word));

	class = elf_identify(memory, size);
	if (class < 0) {
		errno = -class;
		return NULL;
	}

	elf = malloc(sizeof(struct kmod_elf));
	if (elf == NULL) {
		return NULL;
	}

	elf->memory = memory;
	elf->changed = NULL;
	elf->size = size;
	elf->class = class;

#define READV(field) \
	elf_get_uint(elf, offsetof(typeof(*hdr), field), sizeof(hdr->field))

#define LOAD_HEADER						\
	elf->header.section.offset = READV(e_shoff);		\
	elf->header.section.count = READV(e_shnum);		\
	elf->header.section.entry_size = READV(e_shentsize);	\
	elf->header.strings.section = READV(e_shstrndx);	\
	elf->header.machine = READV(e_machine)
	if (elf->class & KMOD_ELF_32) {
		const Elf32_Ehdr *hdr _unused_ = elf_get_mem(elf, 0);
		LOAD_HEADER;
		hdr_size = sizeof(Elf32_Ehdr);
		shdr_size = sizeof(Elf32_Shdr);
	} else {
		const Elf64_Ehdr *hdr _unused_ = elf_get_mem(elf, 0);
		LOAD_HEADER;
		hdr_size = sizeof(Elf64_Ehdr);
		shdr_size = sizeof(Elf64_Shdr);
	}
#undef LOAD_HEADER
#undef READV

	ELFDBG(elf, "section: offset=%"PRIu64" count=%"PRIu16" entry_size=%"PRIu16" strings index=%"PRIu16"\n",
	       elf->header.section.offset,
	       elf->header.section.count,
	       elf->header.section.entry_size,
	       elf->header.strings.section);

	if (elf->header.section.entry_size != shdr_size) {
		ELFDBG(elf, "unexpected section entry size: %"PRIu16", expected %"PRIu16"\n",
		       elf->header.section.entry_size, shdr_size);
		goto invalid;
	}
	min_size = hdr_size + shdr_size * elf->header.section.count;
	if (min_size >= elf->size) {
		ELFDBG(elf, "file is too short to hold sections\n");
		goto invalid;
	}

	if (elf_get_section_info(elf, elf->header.strings.section,
					&elf->header.strings.offset,
					&elf->header.strings.size,
					&elf->header.strings.nameoff) < 0) {
		ELFDBG(elf, "could not get strings section\n");
		goto invalid;
	} else {
		uint64_t slen;
		const char *s = elf_get_strings_section(elf, &slen);
		if (slen == 0 || s[slen - 1] != '\0') {
			ELFDBG(elf, "strings section does not ends with \\0\n");
			goto invalid;
		}
	}

	return elf;

invalid:
	free(elf);
	errno = EINVAL;
	return NULL;
}

void kmod_elf_unref(struct kmod_elf *elf)
{
	free(elf->changed);
	free(elf);
}

const void *kmod_elf_get_memory(const struct kmod_elf *elf)
{
	return elf->memory;
}

static int elf_find_section(const struct kmod_elf *elf, const char *section)
{
	uint64_t nameslen;
	const char *names = elf_get_strings_section(elf, &nameslen);
	uint16_t i;

	for (i = 1; i < elf->header.section.count; i++) {
		uint64_t off, size;
		uint32_t nameoff;
		const char *n;
		int err = elf_get_section_info(elf, i, &off, &size, &nameoff);
		if (err < 0)
			continue;
		if (nameoff >= nameslen)
			continue;
		n = names + nameoff;
		if (!streq(section, n))
			continue;

		return i;
	}

	return -ENOENT;
}

int kmod_elf_get_section(const struct kmod_elf *elf, const char *section, const void **buf, uint64_t *buf_size)
{
	uint64_t nameslen;
	const char *names = elf_get_strings_section(elf, &nameslen);
	uint16_t i;

	*buf = NULL;
	*buf_size = 0;

	for (i = 1; i < elf->header.section.count; i++) {
		uint64_t off, size;
		uint32_t nameoff;
		const char *n;
		int err = elf_get_section_info(elf, i, &off, &size, &nameoff);
		if (err < 0)
			continue;
		if (nameoff >= nameslen)
			continue;
		n = names + nameoff;
		if (!streq(section, n))
			continue;

		*buf = elf_get_mem(elf, off);
		*buf_size = size;
		return 0;
	}

	return -ENOENT;
}

/* array will be allocated with strings in a single malloc, just free *array */
int kmod_elf_get_strings(const struct kmod_elf *elf, const char *section, char ***array)
{
	size_t i, j, count;
	uint64_t size;
	const void *buf;
	const char *strings;
	char *s, **a;
	int err;

	*array = NULL;

	err = kmod_elf_get_section(elf, section, &buf, &size);
	if (err < 0)
		return err;

	strings = buf;
	if (strings == NULL || size == 0)
		return 0;

	/* skip zero padding */
	while (strings[0] == '\0' && size > 1) {
		strings++;
		size--;
	}

	if (size <= 1)
		return 0;

	for (i = 0, count = 0; i < size; ) {
		if (strings[i] != '\0') {
			i++;
			continue;
		}

		while (strings[i] == '\0' && i < size)
			i++;

		count++;
	}

	if (strings[i - 1] != '\0')
		count++;

	*array = a = malloc(size + 1 + sizeof(char *) * (count + 1));
	if (*array == NULL)
		return -errno;

	s = (char *)(a + count + 1);
	memcpy(s, strings, size);

	/* make sure the last string is NULL-terminated */
	s[size] = '\0';
	a[count] = NULL;
	a[0] = s;

	for (i = 0, j = 1; j < count && i < size; ) {
		if (s[i] != '\0') {
			i++;
			continue;
		}

		while (strings[i] == '\0' && i < size)
			i++;

		a[j] = &s[i];
		j++;
	}

	return count;
}

/* array will be allocated with strings in a single malloc, just free *array */
int kmod_elf_get_modversions(const struct kmod_elf *elf, struct kmod_modversion **array)
{
	size_t off, offcrc, slen;
	uint64_t size;
	struct kmod_modversion *a;
	const void *buf;
	char *itr;
	int i, count, err;
#define MODVERSION_SEC_SIZE (sizeof(struct kmod_modversion64))

	assert_cc(sizeof(struct kmod_modversion64) ==
					sizeof(struct kmod_modversion32));

	if (elf->class & KMOD_ELF_32)
		offcrc = sizeof(uint32_t);
	else
		offcrc = sizeof(uint64_t);

	*array = NULL;

	err = kmod_elf_get_section(elf, "__versions", &buf, &size);
	if (err < 0)
		return err;

	if (buf == NULL || size == 0)
		return 0;

	if (size % MODVERSION_SEC_SIZE != 0)
		return -EINVAL;

	count = size / MODVERSION_SEC_SIZE;

	off = (const uint8_t *)buf - elf->memory;
	slen = 0;

	for (i = 0; i < count; i++, off += MODVERSION_SEC_SIZE) {
		const char *symbol = elf_get_mem(elf, off + offcrc);

		if (symbol[0] == '.')
			symbol++;

		slen += strlen(symbol) + 1;
	}

	*array = a = malloc(sizeof(struct kmod_modversion) * count + slen);
	if (*array == NULL)
		return -errno;

	itr = (char *)(a + count);
	off = (const uint8_t *)buf - elf->memory;

	for (i = 0; i < count; i++, off += MODVERSION_SEC_SIZE) {
		uint64_t crc = elf_get_uint(elf, off, offcrc);
		const char *symbol = elf_get_mem(elf, off + offcrc);
		size_t symbollen;

		if (symbol[0] == '.')
			symbol++;

		a[i].crc = crc;
		a[i].bind = KMOD_SYMBOL_UNDEF;
		a[i].symbol = itr;
		symbollen = strlen(symbol) + 1;
		memcpy(itr, symbol, symbollen);
		itr += symbollen;
	}

	return count;
}

int kmod_elf_strip_section(struct kmod_elf *elf, const char *section)
{
	uint64_t off, size;
	const void *buf;
	int idx = elf_find_section(elf, section);
	uint64_t val;

	if (idx < 0)
		return idx;

	buf = elf_get_section_header(elf, idx);
	off = (const uint8_t *)buf - elf->memory;

	if (elf->class & KMOD_ELF_32) {
		off += offsetof(Elf32_Shdr, sh_flags);
		size = sizeof(((Elf32_Shdr *)buf)->sh_flags);
	} else {
		off += offsetof(Elf64_Shdr, sh_flags);
		size = sizeof(((Elf64_Shdr *)buf)->sh_flags);
	}

	val = elf_get_uint(elf, off, size);
	val &= ~(uint64_t)SHF_ALLOC;

	return elf_set_uint(elf, off, size, val);
}

int kmod_elf_strip_vermagic(struct kmod_elf *elf)
{
	uint64_t i, size;
	const void *buf;
	const char *strings;
	int err;

	err = kmod_elf_get_section(elf, ".modinfo", &buf, &size);
	if (err < 0)
		return err;
	strings = buf;
	if (strings == NULL || size == 0)
		return 0;

	/* skip zero padding */
	while (strings[0] == '\0' && size > 1) {
		strings++;
		size--;
	}
	if (size <= 1)
		return 0;

	for (i = 0; i < size; i++) {
		const char *s;
		size_t off, len;

		if (strings[i] == '\0')
			continue;
		if (i + 1 >= size)
			continue;

		s = strings + i;
		len = sizeof("vermagic=") - 1;
		if (i + len >= size)
			continue;
		if (strncmp(s, "vermagic=", len) != 0) {
			i += strlen(s);
			continue;
		}
		off = (const uint8_t *)s - elf->memory;

		if (elf->changed == NULL) {
			elf->changed = malloc(elf->size);
			if (elf->changed == NULL)
				return -errno;
			memcpy(elf->changed, elf->memory, elf->size);
			elf->memory = elf->changed;
			ELFDBG(elf, "copied memory to allow writing.\n");
		}

		len = strlen(s);
		ELFDBG(elf, "clear .modinfo vermagic \"%s\" (%zd bytes)\n",
		       s, len);
		memset(elf->changed + off, '\0', len);
		return 0;
	}

	ELFDBG(elf, "no vermagic found in .modinfo\n");
	return -ENOENT;
}


static int kmod_elf_get_symbols_symtab(const struct kmod_elf *elf, struct kmod_modversion **array)
{
	uint64_t i, last, size;
	const void *buf;
	const char *strings;
	char *itr;
	struct kmod_modversion *a;
	int count, err;

	*array = NULL;

	err = kmod_elf_get_section(elf, "__ksymtab_strings", &buf, &size);
	if (err < 0)
		return err;
	strings = buf;
	if (strings == NULL || size == 0)
		return 0;

	/* skip zero padding */
	while (strings[0] == '\0' && size > 1) {
		strings++;
		size--;
	}
	if (size <= 1)
		return 0;

	last = 0;
	for (i = 0, count = 0; i < size; i++) {
		if (strings[i] == '\0') {
			if (last == i) {
				last = i + 1;
				continue;
			}
			count++;
			last = i + 1;
		}
	}
	if (strings[i - 1] != '\0')
		count++;

	*array = a = malloc(size + 1 + sizeof(struct kmod_modversion) * count);
	if (*array == NULL)
		return -errno;

	itr = (char *)(a + count);
	last = 0;
	for (i = 0, count = 0; i < size; i++) {
		if (strings[i] == '\0') {
			size_t slen = i - last;
			if (last == i) {
				last = i + 1;
				continue;
			}
			a[count].crc = 0;
			a[count].bind = KMOD_SYMBOL_GLOBAL;
			a[count].symbol = itr;
			memcpy(itr, strings + last, slen);
			itr[slen] = '\0';
			itr += slen + 1;
			count++;
			last = i + 1;
		}
	}
	if (strings[i - 1] != '\0') {
		size_t slen = i - last;
		a[count].crc = 0;
		a[count].bind = KMOD_SYMBOL_GLOBAL;
		a[count].symbol = itr;
		memcpy(itr, strings + last, slen);
		itr[slen] = '\0';
		count++;
	}

	return count;
}

static inline uint8_t kmod_symbol_bind_from_elf(uint8_t elf_value)
{
	switch (elf_value) {
	case STB_LOCAL:
		return KMOD_SYMBOL_LOCAL;
	case STB_GLOBAL:
		return KMOD_SYMBOL_GLOBAL;
	case STB_WEAK:
		return KMOD_SYMBOL_WEAK;
	default:
		return KMOD_SYMBOL_NONE;
	}
}

/* array will be allocated with strings in a single malloc, just free *array */
int kmod_elf_get_symbols(const struct kmod_elf *elf, struct kmod_modversion **array)
{
	static const char crc_str[] = "__crc_";
	static const size_t crc_strlen = sizeof(crc_str) - 1;
	uint64_t strtablen, symtablen, str_off, sym_off;
	const void *strtab, *symtab;
	struct kmod_modversion *a;
	char *itr;
	size_t slen, symlen;
	int i, count, symcount, err;

	err = kmod_elf_get_section(elf, ".strtab", &strtab, &strtablen);
	if (err < 0) {
		ELFDBG(elf, "no .strtab found.\n");
		goto fallback;
	}

	err = kmod_elf_get_section(elf, ".symtab", &symtab, &symtablen);
	if (err < 0) {
		ELFDBG(elf, "no .symtab found.\n");
		goto fallback;
	}

	if (elf->class & KMOD_ELF_32)
		symlen = sizeof(Elf32_Sym);
	else
		symlen = sizeof(Elf64_Sym);

	if (symtablen % symlen != 0) {
		ELFDBG(elf, "unexpected .symtab of length %"PRIu64", not multiple of %"PRIu64" as expected.\n", symtablen, symlen);
		goto fallback;
	}

	symcount = symtablen / symlen;
	count = 0;
	slen = 0;
	str_off = (const uint8_t *)strtab - elf->memory;
	sym_off = (const uint8_t *)symtab - elf->memory + symlen;
	for (i = 1; i < symcount; i++, sym_off += symlen) {
		const char *name;
		uint32_t name_off;

#define READV(field)							\
		elf_get_uint(elf, sym_off + offsetof(typeof(*s), field),\
			     sizeof(s->field))
		if (elf->class & KMOD_ELF_32) {
			Elf32_Sym *s;
			name_off = READV(st_name);
		} else {
			Elf64_Sym *s;
			name_off = READV(st_name);
		}
#undef READV
		if (name_off >= strtablen) {
			ELFDBG(elf, ".strtab is %"PRIu64" bytes, but .symtab entry %d wants to access offset %"PRIu32".\n", strtablen, i, name_off);
			goto fallback;
		}

		name = elf_get_mem(elf, str_off + name_off);

		if (strncmp(name, crc_str, crc_strlen) != 0)
			continue;
		slen += strlen(name + crc_strlen) + 1;
		count++;
	}

	if (count == 0)
		goto fallback;

	*array = a = malloc(sizeof(struct kmod_modversion) * count + slen);
	if (*array == NULL)
		return -errno;

	itr = (char *)(a + count);
	count = 0;
	str_off = (const uint8_t *)strtab - elf->memory;
	sym_off = (const uint8_t *)symtab - elf->memory + symlen;
	for (i = 1; i < symcount; i++, sym_off += symlen) {
		const char *name;
		uint32_t name_off;
		uint64_t crc;
		uint8_t info, bind;

#define READV(field)							\
		elf_get_uint(elf, sym_off + offsetof(typeof(*s), field),\
			     sizeof(s->field))
		if (elf->class & KMOD_ELF_32) {
			Elf32_Sym *s;
			name_off = READV(st_name);
			crc = READV(st_value);
			info = READV(st_info);
		} else {
			Elf64_Sym *s;
			name_off = READV(st_name);
			crc = READV(st_value);
			info = READV(st_info);
		}
#undef READV
		name = elf_get_mem(elf, str_off + name_off);
		if (strncmp(name, crc_str, crc_strlen) != 0)
			continue;
		name += crc_strlen;

		if (elf->class & KMOD_ELF_32)
			bind = ELF32_ST_BIND(info);
		else
			bind = ELF64_ST_BIND(info);

		a[count].crc = crc;
		a[count].bind = kmod_symbol_bind_from_elf(bind);
		a[count].symbol = itr;
		slen = strlen(name);
		memcpy(itr, name, slen);
		itr[slen] = '\0';
		itr += slen + 1;
		count++;
	}
	return count;

fallback:
	ELFDBG(elf, "Falling back to __ksymtab_strings!\n");
	return kmod_elf_get_symbols_symtab(elf, array);
}

static int kmod_elf_crc_find(const struct kmod_elf *elf, const void *versions, uint64_t versionslen, const char *name, uint64_t *crc)
{
	size_t verlen, crclen, off;
	uint64_t i;

	if (elf->class & KMOD_ELF_32) {
		struct kmod_modversion32 *mv;
		verlen = sizeof(*mv);
		crclen = sizeof(mv->crc);
	} else {
		struct kmod_modversion64 *mv;
		verlen = sizeof(*mv);
		crclen = sizeof(mv->crc);
	}

	off = (const uint8_t *)versions - elf->memory;
	for (i = 0; i < versionslen; i += verlen) {
		const char *symbol = elf_get_mem(elf, off + i + crclen);
		if (!streq(name, symbol))
			continue;
		*crc = elf_get_uint(elf, off + i, crclen);
		return i / verlen;
	}

	ELFDBG(elf, "could not find crc for symbol '%s'\n", name);
	*crc = 0;
	return -1;
}

/* from module-init-tools:elfops_core.c */
#ifndef STT_REGISTER
#define STT_REGISTER    13              /* Global register reserved to app. */
#endif

/* array will be allocated with strings in a single malloc, just free *array */
int kmod_elf_get_dependency_symbols(const struct kmod_elf *elf, struct kmod_modversion **array)
{
	uint64_t versionslen, strtablen, symtablen, str_off, sym_off, ver_off;
	const void *versions, *strtab, *symtab;
	struct kmod_modversion *a;
	char *itr;
	size_t slen, verlen, symlen, crclen;
	int i, count, symcount, vercount, err;
	bool handle_register_symbols;
	uint8_t *visited_versions;
	uint64_t *symcrcs;

	err = kmod_elf_get_section(elf, "__versions", &versions, &versionslen);
	if (err < 0) {
		versions = NULL;
		versionslen = 0;
		verlen = 0;
		crclen = 0;
	} else {
		if (elf->class & KMOD_ELF_32) {
			struct kmod_modversion32 *mv;
			verlen = sizeof(*mv);
			crclen = sizeof(mv->crc);
		} else {
			struct kmod_modversion64 *mv;
			verlen = sizeof(*mv);
			crclen = sizeof(mv->crc);
		}
		if (versionslen % verlen != 0) {
			ELFDBG(elf, "unexpected __versions of length %"PRIu64", not multiple of %zd as expected.\n", versionslen, verlen);
			versions = NULL;
			versionslen = 0;
		}
	}

	err = kmod_elf_get_section(elf, ".strtab", &strtab, &strtablen);
	if (err < 0) {
		ELFDBG(elf, "no .strtab found.\n");
		return -EINVAL;
	}

	err = kmod_elf_get_section(elf, ".symtab", &symtab, &symtablen);
	if (err < 0) {
		ELFDBG(elf, "no .symtab found.\n");
		return -EINVAL;
	}

	if (elf->class & KMOD_ELF_32)
		symlen = sizeof(Elf32_Sym);
	else
		symlen = sizeof(Elf64_Sym);

	if (symtablen % symlen != 0) {
		ELFDBG(elf, "unexpected .symtab of length %"PRIu64", not multiple of %"PRIu64" as expected.\n", symtablen, symlen);
		return -EINVAL;
	}

	if (versionslen == 0) {
		vercount = 0;
		visited_versions = NULL;
	} else {
		vercount = versionslen / verlen;
		visited_versions = calloc(vercount, sizeof(uint8_t));
		if (visited_versions == NULL)
			return -ENOMEM;
	}

	handle_register_symbols = (elf->header.machine == EM_SPARC ||
				   elf->header.machine == EM_SPARCV9);

	symcount = symtablen / symlen;
	count = 0;
	slen = 0;
	str_off = (const uint8_t *)strtab - elf->memory;
	sym_off = (const uint8_t *)symtab - elf->memory + symlen;

	symcrcs = calloc(symcount, sizeof(uint64_t));
	if (symcrcs == NULL) {
		free(visited_versions);
		return -ENOMEM;
	}

	for (i = 1; i < symcount; i++, sym_off += symlen) {
		const char *name;
		uint64_t crc;
		uint32_t name_off;
		uint16_t secidx;
		uint8_t info;
		int idx;

#define READV(field)							\
		elf_get_uint(elf, sym_off + offsetof(typeof(*s), field),\
			     sizeof(s->field))
		if (elf->class & KMOD_ELF_32) {
			Elf32_Sym *s;
			name_off = READV(st_name);
			secidx = READV(st_shndx);
			info = READV(st_info);
		} else {
			Elf64_Sym *s;
			name_off = READV(st_name);
			secidx = READV(st_shndx);
			info = READV(st_info);
		}
#undef READV
		if (secidx != SHN_UNDEF)
			continue;

		if (handle_register_symbols) {
			uint8_t type;
			if (elf->class & KMOD_ELF_32)
				type = ELF32_ST_TYPE(info);
			else
				type = ELF64_ST_TYPE(info);

			/* Not really undefined: sparc gcc 3.3 creates
			 * U references when you have global asm
			 * variables, to avoid anyone else misusing
			 * them.
			 */
			if (type == STT_REGISTER)
				continue;
		}

		if (name_off >= strtablen) {
			ELFDBG(elf, ".strtab is %"PRIu64" bytes, but .symtab entry %d wants to access offset %"PRIu32".\n", strtablen, i, name_off);
			free(visited_versions);
			free(symcrcs);
			return -EINVAL;
		}

		name = elf_get_mem(elf, str_off + name_off);
		if (name[0] == '\0') {
			ELFDBG(elf, "empty symbol name at index %"PRIu64"\n", i);
			continue;
		}

		slen += strlen(name) + 1;
		count++;

		idx = kmod_elf_crc_find(elf, versions, versionslen, name, &crc);
		if (idx >= 0 && visited_versions != NULL)
			visited_versions[idx] = 1;
		symcrcs[i] = crc;
	}

	if (visited_versions != NULL) {
		/* module_layout/struct_module are not visited, but needed */
		ver_off = (const uint8_t *)versions - elf->memory;
		for (i = 0; i < vercount; i++) {
			if (visited_versions[i] == 0) {
				const char *name;
				name = elf_get_mem(elf, ver_off + i * verlen + crclen);
				slen += strlen(name) + 1;

				count++;
			}
		}
	}

	if (count == 0) {
		free(visited_versions);
		free(symcrcs);
		*array = NULL;
		return 0;
	}

	*array = a = malloc(sizeof(struct kmod_modversion) * count + slen);
	if (*array == NULL) {
		free(visited_versions);
		free(symcrcs);
		return -errno;
	}

	itr = (char *)(a + count);
	count = 0;
	str_off = (const uint8_t *)strtab - elf->memory;
	sym_off = (const uint8_t *)symtab - elf->memory + symlen;
	for (i = 1; i < symcount; i++, sym_off += symlen) {
		const char *name;
		uint64_t crc;
		uint32_t name_off;
		uint16_t secidx;
		uint8_t info, bind;

#define READV(field)							\
		elf_get_uint(elf, sym_off + offsetof(typeof(*s), field),\
			     sizeof(s->field))
		if (elf->class & KMOD_ELF_32) {
			Elf32_Sym *s;
			name_off = READV(st_name);
			secidx = READV(st_shndx);
			info = READV(st_info);
		} else {
			Elf64_Sym *s;
			name_off = READV(st_name);
			secidx = READV(st_shndx);
			info = READV(st_info);
		}
#undef READV
		if (secidx != SHN_UNDEF)
			continue;

		if (handle_register_symbols) {
			uint8_t type;
			if (elf->class & KMOD_ELF_32)
				type = ELF32_ST_TYPE(info);
			else
				type = ELF64_ST_TYPE(info);

			/* Not really undefined: sparc gcc 3.3 creates
			 * U references when you have global asm
			 * variables, to avoid anyone else misusing
			 * them.
			 */
			if (type == STT_REGISTER)
				continue;
		}

		name = elf_get_mem(elf, str_off + name_off);
		if (name[0] == '\0') {
			ELFDBG(elf, "empty symbol name at index %"PRIu64"\n", i);
			continue;
		}

		if (elf->class & KMOD_ELF_32)
			bind = ELF32_ST_BIND(info);
		else
			bind = ELF64_ST_BIND(info);
		if (bind == STB_WEAK)
			bind = KMOD_SYMBOL_WEAK;
		else
			bind = KMOD_SYMBOL_UNDEF;

		slen = strlen(name);
		crc = symcrcs[i];

		a[count].crc = crc;
		a[count].bind = bind;
		a[count].symbol = itr;
		memcpy(itr, name, slen);
		itr[slen] = '\0';
		itr += slen + 1;

		count++;
	}

	free(symcrcs);

	if (visited_versions == NULL)
		return count;

	/* add unvisited (module_layout/struct_module) */
	ver_off = (const uint8_t *)versions - elf->memory;
	for (i = 0; i < vercount; i++) {
		const char *name;
		uint64_t crc;

		if (visited_versions[i] != 0)
			continue;

		name = elf_get_mem(elf, ver_off + i * verlen + crclen);
		slen = strlen(name);
		crc = elf_get_uint(elf, ver_off + i * verlen, crclen);

		a[count].crc = crc;
		a[count].bind = KMOD_SYMBOL_UNDEF;
		a[count].symbol = itr;
		memcpy(itr, name, slen);
		itr[slen] = '\0';
		itr += slen + 1;

		count++;
	}
	free(visited_versions);
	return count;
}
