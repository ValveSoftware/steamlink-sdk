/*
 * libkmod - module signature display
 *
 * Copyright (C) 2013 Michal Marek, SUSE
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

#include <endian.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "libkmod-internal.h"

/* These types and tables were copied from the 3.7 kernel sources.
 * As this is just description of the signature format, it should not be
 * considered derived work (so libkmod can use the LGPL license).
 */
enum pkey_algo {
	PKEY_ALGO_DSA,
	PKEY_ALGO_RSA,
	PKEY_ALGO__LAST
};

static const char *const pkey_algo[PKEY_ALGO__LAST] = {
	[PKEY_ALGO_DSA]		= "DSA",
	[PKEY_ALGO_RSA]		= "RSA",
};

enum pkey_hash_algo {
	PKEY_HASH_MD4,
	PKEY_HASH_MD5,
	PKEY_HASH_SHA1,
	PKEY_HASH_RIPE_MD_160,
	PKEY_HASH_SHA256,
	PKEY_HASH_SHA384,
	PKEY_HASH_SHA512,
	PKEY_HASH_SHA224,
	PKEY_HASH__LAST
};

const char *const pkey_hash_algo[PKEY_HASH__LAST] = {
	[PKEY_HASH_MD4]		= "md4",
	[PKEY_HASH_MD5]		= "md5",
	[PKEY_HASH_SHA1]	= "sha1",
	[PKEY_HASH_RIPE_MD_160]	= "rmd160",
	[PKEY_HASH_SHA256]	= "sha256",
	[PKEY_HASH_SHA384]	= "sha384",
	[PKEY_HASH_SHA512]	= "sha512",
	[PKEY_HASH_SHA224]	= "sha224",
};

enum pkey_id_type {
	PKEY_ID_PGP,		/* OpenPGP generated key ID */
	PKEY_ID_X509,		/* X.509 arbitrary subjectKeyIdentifier */
	PKEY_ID_TYPE__LAST
};

const char *const pkey_id_type[PKEY_ID_TYPE__LAST] = {
	[PKEY_ID_PGP]		= "PGP",
	[PKEY_ID_X509]		= "X509",
};

/*
 * Module signature information block.
 *
 * The constituents of the signature section are, in order:
 *
 *	- Signer's name
 *	- Key identifier
 *	- Signature data
 *	- Information block
 */
struct module_signature {
	uint8_t algo;        /* Public-key crypto algorithm [enum pkey_algo] */
	uint8_t hash;        /* Digest algorithm [enum pkey_hash_algo] */
	uint8_t id_type;     /* Key identifier type [enum pkey_id_type] */
	uint8_t signer_len;  /* Length of signer's name */
	uint8_t key_id_len;  /* Length of key identifier */
	uint8_t __pad[3];
	uint32_t sig_len;    /* Length of signature data (big endian) */
};

#define SIG_MAGIC "~Module signature appended~\n"

bool kmod_module_signature_info(const struct kmod_file *file, struct kmod_signature_info *sig_info)
{
	const char *mem;
	off_t size;
	const struct module_signature *modsig;
	size_t sig_len;


	size = kmod_file_get_size(file);
	mem = kmod_file_get_contents(file);
	if (size < (off_t)strlen(SIG_MAGIC))
		return false;
	size -= strlen(SIG_MAGIC);
	if (memcmp(SIG_MAGIC, mem + size, strlen(SIG_MAGIC)) != 0)
		return false;

	if (size < (off_t)sizeof(struct module_signature))
		return false;
	size -= sizeof(struct module_signature);
	modsig = (struct module_signature *)(mem + size);
	if (modsig->algo >= PKEY_ALGO__LAST ||
			modsig->hash >= PKEY_HASH__LAST ||
			modsig->id_type >= PKEY_ID_TYPE__LAST)
		return false;
	sig_len = be32toh(get_unaligned(&modsig->sig_len));
	if (size < (off_t)(modsig->signer_len + modsig->key_id_len + sig_len))
		return false;

	size -= modsig->key_id_len + sig_len;
	sig_info->key_id = mem + size;
	sig_info->key_id_len = modsig->key_id_len;

	size -= modsig->signer_len;
	sig_info->signer = mem + size;
	sig_info->signer_len = modsig->signer_len;

	sig_info->algo = pkey_algo[modsig->algo];
	sig_info->hash_algo = pkey_hash_algo[modsig->hash];
	sig_info->id_type = pkey_id_type[modsig->id_type];

	return true;
}
