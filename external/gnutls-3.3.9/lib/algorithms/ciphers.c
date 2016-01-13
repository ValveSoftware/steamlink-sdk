/*
 * Copyright (C) 2011-2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <gnutls_int.h>
#include <algorithms.h>
#include <gnutls_errors.h>
#include <x509/common.h>


/* Note that all algorithms are in CBC or STREAM modes. 
 * Do not add any algorithms in other modes (avoid modified algorithms).
 * View first: "The order of encryption and authentication for
 * protecting communications" by Hugo Krawczyk - CRYPTO 2001
 *
 * Make sure to update MAX_CIPHER_BLOCK_SIZE and MAX_CIPHER_KEY_SIZE as well.
 */
static const cipher_entry_st algorithms[] = {
	{"AES-256-CBC", GNUTLS_CIPHER_AES_256_CBC, 16, 32, CIPHER_BLOCK,
	 0, 16, 16, 0},
	{"AES-192-CBC", GNUTLS_CIPHER_AES_192_CBC, 16, 24, CIPHER_BLOCK,
	 0, 16, 16, 0},
	{"AES-128-CBC", GNUTLS_CIPHER_AES_128_CBC, 16, 16, CIPHER_BLOCK,
	 0, 16, 16, 0},
	{"AES-128-GCM", GNUTLS_CIPHER_AES_128_GCM, 16, 16, CIPHER_STREAM,
	 4, 8, 12, 1},
	{"AES-256-GCM", GNUTLS_CIPHER_AES_256_GCM, 16, 32, CIPHER_STREAM,
	 4, 8, 12, 1},
	{"ARCFOUR-128", GNUTLS_CIPHER_ARCFOUR_128, 1, 16, CIPHER_STREAM, 
	 0, 0, 0, 0},
	{"ESTREAM-SALSA20-256", GNUTLS_CIPHER_ESTREAM_SALSA20_256, 64, 32,
	 CIPHER_STREAM, 0, 0, 8, 0},
	{"SALSA20-256", GNUTLS_CIPHER_SALSA20_256, 64, 32, CIPHER_STREAM,
	 0, 0, 8, 0},
	{"CAMELLIA-256-CBC", GNUTLS_CIPHER_CAMELLIA_256_CBC, 16, 32,
	 CIPHER_BLOCK, 0, 16, 16, 0},
	{"CAMELLIA-192-CBC", GNUTLS_CIPHER_CAMELLIA_192_CBC, 16, 24,
	 CIPHER_BLOCK, 0, 16, 16, 0},
	{"CAMELLIA-128-CBC", GNUTLS_CIPHER_CAMELLIA_128_CBC, 16, 16,
	 CIPHER_BLOCK, 0, 16, 16, 0},
	{"CAMELLIA-128-GCM", GNUTLS_CIPHER_CAMELLIA_128_GCM, 16, 16,
	 CIPHER_STREAM, 4, 8, 12, 1},
	{"CAMELLIA-256-GCM", GNUTLS_CIPHER_CAMELLIA_256_GCM, 16, 32,
	 CIPHER_STREAM, 4, 8, 12, 1},
	{"3DES-CBC", GNUTLS_CIPHER_3DES_CBC, 8, 24, CIPHER_BLOCK, 0, 8, 8, 0},
	{"DES-CBC", GNUTLS_CIPHER_DES_CBC, 8, 8, CIPHER_BLOCK, 0, 8, 8, 0},
	{"ARCFOUR-40", GNUTLS_CIPHER_ARCFOUR_40, 1, 5, CIPHER_STREAM, 0, 0, 0, 0},
	{"RC2-40", GNUTLS_CIPHER_RC2_40_CBC, 8, 5, CIPHER_BLOCK, 0, 8, 8, 0},

#ifdef ENABLE_OPENPGP
	{"IDEA-PGP-CFB", GNUTLS_CIPHER_IDEA_PGP_CFB, 8, 16, CIPHER_BLOCK,
	 0, 8, 8, 0},
	{"3DES-PGP-CFB", GNUTLS_CIPHER_3DES_PGP_CFB, 8, 24, CIPHER_BLOCK,
	 0, 8, 8, 0},
	{"CAST5-PGP-CFB", GNUTLS_CIPHER_CAST5_PGP_CFB, 8, 16, CIPHER_BLOCK,
	 0, 8, 8, 0},
	{"BLOWFISH-PGP-CFB", GNUTLS_CIPHER_BLOWFISH_PGP_CFB, 8,
	 16 /*actually unlimited */ , CIPHER_BLOCK, 0, 8, 8, 0},
	{"SAFER-SK128-PGP-CFB", GNUTLS_CIPHER_SAFER_SK128_PGP_CFB, 8, 16,
	 CIPHER_BLOCK, 0, 8, 8, 0},
	{"AES-128-PGP-CFB", GNUTLS_CIPHER_AES128_PGP_CFB, 16, 16,
	 CIPHER_BLOCK, 0, 16, 16, 0},
	{"AES-192-PGP-CFB", GNUTLS_CIPHER_AES192_PGP_CFB, 16, 24,
	 CIPHER_BLOCK, 0, 16, 16, 0},
	{"AES-256-PGP-CFB", GNUTLS_CIPHER_AES256_PGP_CFB, 16, 32,
	 CIPHER_BLOCK, 0, 16, 16, 0},
	{"TWOFISH-PGP-CFB", GNUTLS_CIPHER_TWOFISH_PGP_CFB, 16, 16,
	 CIPHER_BLOCK, 0, 16, 16, 0},
#endif

#ifndef ENABLE_FIPS140
	/* All the other ciphers are disabled on the back-end library.
	 * This needs to be disabled here as it is merely a placeholder
	 * rather than an actual cipher.
	 */
	{"NULL", GNUTLS_CIPHER_NULL, 1, 0, CIPHER_STREAM, 0, 0, 0},
#endif

	{0, 0, 0, 0, 0, 0, 0}
};

#define GNUTLS_CIPHER_LOOP(b) \
        const cipher_entry_st *p; \
                for(p = algorithms; p->name != NULL; p++) { b ; }

#define GNUTLS_ALG_LOOP(a) \
                        GNUTLS_CIPHER_LOOP( if(p->id == algorithm) { a; break; } )

/* CIPHER functions */

const cipher_entry_st *cipher_to_entry(gnutls_cipher_algorithm_t c)
{
	GNUTLS_CIPHER_LOOP(if (c == p->id) return p);

	return NULL;
}

/**
 * gnutls_cipher_get_block_size:
 * @algorithm: is an encryption algorithm
 *
 * Returns: the block size of the encryption algorithm.
 *
 * Since: 2.10.0
 **/
int gnutls_cipher_get_block_size(gnutls_cipher_algorithm_t algorithm)
{
	size_t ret = 0;
	GNUTLS_ALG_LOOP(ret = p->blocksize);
	return ret;

}

/**
 * gnutls_cipher_get_tag_size:
 * @algorithm: is an encryption algorithm
 *
 * Returns: the tag size of the authenticated encryption algorithm.
 *
 * Since: 3.2.2
 **/
int gnutls_cipher_get_tag_size(gnutls_cipher_algorithm_t algorithm)
{
	return _gnutls_cipher_get_tag_size(cipher_to_entry(algorithm));
}

/**
 * gnutls_cipher_get_iv_size:
 * @algorithm: is an encryption algorithm
 *
 * Get block size for encryption algorithm.
 *
 * Returns: block size for encryption algorithm.
 *
 * Since: 3.2.0
 **/
int gnutls_cipher_get_iv_size(gnutls_cipher_algorithm_t algorithm)
{
	size_t ret = 0;
	GNUTLS_ALG_LOOP(ret = p->cipher_iv);
	return ret;
}


 /* returns the priority */
int
_gnutls_cipher_priority(gnutls_session_t session,
			gnutls_cipher_algorithm_t algorithm)
{
	unsigned int i;
	for (i = 0; i < session->internals.priorities.cipher.algorithms;
	     i++) {
		if (session->internals.priorities.cipher.priority[i] ==
		    algorithm)
			return i;
	}
	return -1;
}

/**
 * gnutls_cipher_get_key_size:
 * @algorithm: is an encryption algorithm
 *
 * Get key size for cipher.
 *
 * Returns: length (in bytes) of the given cipher's key size, or 0 if
 *   the given cipher is invalid.
 **/
size_t gnutls_cipher_get_key_size(gnutls_cipher_algorithm_t algorithm)
{				/* In bytes */
	size_t ret = 0;
	GNUTLS_ALG_LOOP(ret = p->keysize);
	return ret;

}

/**
 * gnutls_cipher_get_name:
 * @algorithm: is an encryption algorithm
 *
 * Convert a #gnutls_cipher_algorithm_t type to a string.
 *
 * Returns: a pointer to a string that contains the name of the
 *   specified cipher, or %NULL.
 **/
const char *gnutls_cipher_get_name(gnutls_cipher_algorithm_t algorithm)
{
	const char *ret = NULL;

	/* avoid prefix */
	GNUTLS_ALG_LOOP(ret = p->name);

	return ret;
}

/**
 * gnutls_cipher_get_id:
 * @name: is a cipher algorithm name
 *
 * The names are compared in a case insensitive way.
 *
 * Returns: return a #gnutls_cipher_algorithm_t value corresponding to
 *   the specified cipher, or %GNUTLS_CIPHER_UNKNOWN on error.
 **/
gnutls_cipher_algorithm_t gnutls_cipher_get_id(const char *name)
{
	gnutls_cipher_algorithm_t ret = GNUTLS_CIPHER_UNKNOWN;

	GNUTLS_CIPHER_LOOP(
		if (strcasecmp(p->name, name) == 0) {
			if (p->id == GNUTLS_CIPHER_NULL || _gnutls_cipher_exists(p->id))
				ret = p->id; 
			break;
		}
	);

	return ret;
}

/**
 * gnutls_cipher_list:
 *
 * Get a list of supported cipher algorithms.  Note that not
 * necessarily all ciphers are supported as TLS cipher suites.  For
 * example, DES is not supported as a cipher suite, but is supported
 * for other purposes (e.g., PKCS#8 or similar).
 *
 * This function is not thread safe.
 *
 * Returns: a (0)-terminated list of #gnutls_cipher_algorithm_t
 *   integers indicating the available ciphers.
 *
 **/
const gnutls_cipher_algorithm_t *gnutls_cipher_list(void)
{
	static gnutls_cipher_algorithm_t supported_ciphers[MAX_ALGOS] =
	    { 0 };

	if (supported_ciphers[0] == 0) {
		int i = 0;

		GNUTLS_CIPHER_LOOP(
			if (p->id == GNUTLS_CIPHER_NULL || _gnutls_cipher_exists(p->id))
				supported_ciphers[i++] = p->id;
		);
		supported_ciphers[i++] = 0;
	}

	return supported_ciphers;
}
