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

static const mac_entry_st hash_algorithms[] = {
	{"SHA1", HASH_OID_SHA1, GNUTLS_MAC_SHA1, 20, 20, 0, 0, 1, 64},
	{"MD5", HASH_OID_MD5, GNUTLS_MAC_MD5, 16, 16, 0, 0, 0, 64},
	{"SHA256", HASH_OID_SHA256, GNUTLS_MAC_SHA256, 32, 32, 0, 0, 1,
	 64},
	{"SHA384", HASH_OID_SHA384, GNUTLS_MAC_SHA384, 48, 48, 0, 0, 1,
	 64},
	{"SHA512", HASH_OID_SHA512, GNUTLS_MAC_SHA512, 64, 64, 0, 0, 1,
	 64},
	{"SHA224", HASH_OID_SHA224, GNUTLS_MAC_SHA224, 28, 28, 0, 0, 1,
	 64},
	{"UMAC-96", NULL, GNUTLS_MAC_UMAC_96, 12, 16, 8, 0, 1, 0},
	{"UMAC-128", NULL, GNUTLS_MAC_UMAC_128, 16, 16, 8, 0, 1, 0},
	{"AEAD", NULL, GNUTLS_MAC_AEAD, 0, 0, 0, 1, 1, 0},
	{"MD2", HASH_OID_MD2, GNUTLS_MAC_MD2, 0, 0, 0, 0, 0, 0},	/* not used as MAC */
	{"RIPEMD160", HASH_OID_RMD160, GNUTLS_MAC_RMD160, 20, 20, 0, 0, 1,
	 64},
	{"MAC-NULL", NULL, GNUTLS_MAC_NULL, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
};


#define GNUTLS_HASH_LOOP(b) \
        const mac_entry_st *p; \
                for(p = hash_algorithms; p->name != NULL; p++) { b ; }

#define GNUTLS_HASH_ALG_LOOP(a) \
                        GNUTLS_HASH_LOOP( if(p->id == algorithm) { a; break; } )

const mac_entry_st *_gnutls_mac_to_entry(gnutls_mac_algorithm_t c)
{
	GNUTLS_HASH_LOOP(if (c == p->id) return p);

	return NULL;
}

int
_gnutls_mac_priority(gnutls_session_t session,
		     gnutls_mac_algorithm_t algorithm)
{				/* actually returns the priority */
	unsigned int i;
	for (i = 0; i < session->internals.priorities.mac.algorithms; i++) {
		if (session->internals.priorities.mac.priority[i] ==
		    algorithm)
			return i;
	}
	return -1;
}

/**
 * gnutls_mac_get_name:
 * @algorithm: is a MAC algorithm
 *
 * Convert a #gnutls_mac_algorithm_t value to a string.
 *
 * Returns: a string that contains the name of the specified MAC
 *   algorithm, or %NULL.
 **/
const char *gnutls_mac_get_name(gnutls_mac_algorithm_t algorithm)
{
	const char *ret = NULL;

	/* avoid prefix */
	GNUTLS_HASH_ALG_LOOP(ret = p->name);

	return ret;
}

/**
 * gnutls_digest_get_name:
 * @algorithm: is a digest algorithm
 *
 * Convert a #gnutls_digest_algorithm_t value to a string.
 *
 * Returns: a string that contains the name of the specified digest
 *   algorithm, or %NULL.
 **/
const char *gnutls_digest_get_name(gnutls_digest_algorithm_t algorithm)
{
	const char *ret = NULL;

	GNUTLS_HASH_LOOP(
		if (algorithm == (unsigned) p->id && p->oid != NULL) {
			ret = p->name;
			break;
		}
	);

	return ret;
}

/**
 * gnutls_digest_get_id:
 * @name: is a digest algorithm name
 *
 * Convert a string to a #gnutls_digest_algorithm_t value.  The names are
 * compared in a case insensitive way.
 *
 * Returns: a #gnutls_digest_algorithm_t id of the specified MAC
 *   algorithm string, or %GNUTLS_DIG_UNKNOWN on failures.
 **/
gnutls_digest_algorithm_t gnutls_digest_get_id(const char *name)
{
	gnutls_digest_algorithm_t ret = GNUTLS_DIG_UNKNOWN;

	GNUTLS_HASH_LOOP(
		if (p->oid != NULL && strcasecmp(p->name, name) == 0) {
			if (_gnutls_digest_exists((gnutls_digest_algorithm_t)p->id))
				ret = (gnutls_digest_algorithm_t)p->id;
			break;
		}
	);

	return ret;
}

/**
 * gnutls_mac_get_id:
 * @name: is a MAC algorithm name
 *
 * Convert a string to a #gnutls_mac_algorithm_t value.  The names are
 * compared in a case insensitive way.
 *
 * Returns: a #gnutls_mac_algorithm_t id of the specified MAC
 *   algorithm string, or %GNUTLS_MAC_UNKNOWN on failures.
 **/
gnutls_mac_algorithm_t gnutls_mac_get_id(const char *name)
{
	gnutls_mac_algorithm_t ret = GNUTLS_MAC_UNKNOWN;

	GNUTLS_HASH_LOOP(
		if (strcasecmp(p->name, name) == 0) {
			if (p->placeholder != 0 || _gnutls_mac_exists(p->id))
  				ret = p->id;
			break;
		}
	);

	return ret;
}

/**
 * gnutls_mac_get_key_size:
 * @algorithm: is an encryption algorithm
 *
 * Returns the size of the MAC key used in TLS. 
 *
 * Returns: length (in bytes) of the given MAC key size, or 0 if the
 *   given MAC algorithm is invalid.
 **/
size_t gnutls_mac_get_key_size(gnutls_mac_algorithm_t algorithm)
{
	size_t ret = 0;

	/* avoid prefix */
	GNUTLS_HASH_ALG_LOOP(ret = p->key_size);

	return ret;
}

/**
 * gnutls_mac_get_nonce_size:
 * @algorithm: is an encryption algorithm
 *
 * Returns the size of the nonce used by the MAC in TLS.
 *
 * Returns: length (in bytes) of the given MAC nonce size, or 0.
 *
 * Since: 3.2.0
 **/
size_t gnutls_mac_get_nonce_size(gnutls_mac_algorithm_t algorithm)
{
	size_t ret = 0;

	/* avoid prefix */
	GNUTLS_HASH_ALG_LOOP(ret = p->nonce_size);

	return ret;
}

/**
 * gnutls_mac_list:
 *
 * Get a list of hash algorithms for use as MACs.  Note that not
 * necessarily all MACs are supported in TLS cipher suites.  
 * This function is not thread safe.
 *
 * Returns: Return a (0)-terminated list of #gnutls_mac_algorithm_t
 *   integers indicating the available MACs.
 **/
const gnutls_mac_algorithm_t *gnutls_mac_list(void)
{
	static gnutls_mac_algorithm_t supported_macs[MAX_ALGOS] = { 0 };

	if (supported_macs[0] == 0) {
		int i = 0;

		GNUTLS_HASH_LOOP(
			if (p->placeholder != 0 || _gnutls_mac_exists(p->id))
				 supported_macs[i++] = p->id;
		);
		supported_macs[i++] = 0;
	}

	return supported_macs;
}

/**
 * gnutls_digest_list:
 *
 * Get a list of hash (digest) algorithms supported by GnuTLS.
 *
 * This function is not thread safe.
 *
 * Returns: Return a (0)-terminated list of #gnutls_digest_algorithm_t
 *   integers indicating the available digests.
 **/
const gnutls_digest_algorithm_t *gnutls_digest_list(void)
{
	static gnutls_digest_algorithm_t supported_digests[MAX_ALGOS] =
	    { 0 };

	if (supported_digests[0] == 0) {
		int i = 0;

		GNUTLS_HASH_LOOP(
			if (p->oid != NULL && (p->placeholder != 0 ||
				_gnutls_mac_exists(p->id))) {
				
				supported_digests[i++] = (gnutls_digest_algorithm_t)p->id;
			}
		);
		supported_digests[i++] = 0;
	}

	return supported_digests;
}

gnutls_digest_algorithm_t _gnutls_x509_oid_to_digest(const char *oid)
{
	gnutls_digest_algorithm_t ret = 0;

	GNUTLS_HASH_LOOP(
		if (p->oid && strcmp(oid, p->oid) == 0) {
			ret = (gnutls_digest_algorithm_t) p->id; 
			break;
		}
	);

	if (ret == 0)
		return GNUTLS_DIG_UNKNOWN;
	return ret;
}
