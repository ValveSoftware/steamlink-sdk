/*
 * Copyright (C) 2004-2012 Free Software Foundation, Inc.
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

/* Here lies the code of the gnutls_*_set_priority() functions.
 */

#include "gnutls_int.h"
#include "algorithms.h"
#include "gnutls_errors.h"
#include <gnutls_num.h>
#include <gnutls/x509.h>
#include <c-ctype.h>

#define MAX_ELEMENTS 64

static void
break_list(char *etag,
		 char *broken_etag[MAX_ELEMENTS], int *size);

/**
 * gnutls_cipher_set_priority:
 * @session: is a #gnutls_session_t structure.
 * @list: is a 0 terminated list of gnutls_cipher_algorithm_t elements.
 *
 * Sets the priority on the ciphers supported by gnutls.  Priority is
 * higher for elements specified before others.  After specifying the
 * ciphers you want, you must append a 0.  Note that the priority is
 * set on the client. The server does not use the algorithm's
 * priority except for disabling algorithms that were not specified.
 *
 * Returns: %GNUTLS_E_SUCCESS (0) on success, or a negative error code.
 **/
int gnutls_cipher_set_priority(gnutls_session_t session, const int *list)
{
	int num = 0, i;

	while (list[num] != 0)
		num++;
	if (num > MAX_ALGOS)
		num = MAX_ALGOS;
	session->internals.priorities.cipher.algorithms = num;

	for (i = 0; i < num; i++) {
		session->internals.priorities.cipher.priority[i] = list[i];
	}

	return 0;
}

typedef void (bulk_rmadd_func) (priority_st * priority_list, const int *);

inline static void _set_priority(priority_st * st, const int *list)
{
	int num = 0, i;

	while (list[num] != 0)
		num++;
	if (num > MAX_ALGOS)
		num = MAX_ALGOS;
	st->algorithms = num;

	for (i = 0; i < num; i++) {
		st->priority[i] = list[i];
	}

	return;
}

inline static void _add_priority(priority_st * st, const int *list)
{
	int num, i, j, init;

	init = i = st->algorithms;

	for (num = 0; list[num] != 0; ++num) {
		if (i + 1 > MAX_ALGOS) {
			return;
		}

		for (j = 0; j < init; j++) {
			if (st->priority[j] == (unsigned) list[num]) {
				break;
			}
		}

		if (j == init) {
			st->priority[i++] = list[num];
			st->algorithms++;
		}
	}

	return;
}

static void _clear_priorities(priority_st * st, const int *list)
{
	memset(st, 0, sizeof(*st));
}

/**
 * gnutls_kx_set_priority:
 * @session: is a #gnutls_session_t structure.
 * @list: is a 0 terminated list of gnutls_kx_algorithm_t elements.
 *
 * Sets the priority on the key exchange algorithms supported by
 * gnutls.  Priority is higher for elements specified before others.
 * After specifying the algorithms you want, you must append a 0.
 * Note that the priority is set on the client. The server does not
 * use the algorithm's priority except for disabling algorithms that
 * were not specified.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int gnutls_kx_set_priority(gnutls_session_t session, const int *list)
{
	_set_priority(&session->internals.priorities.kx, list);
	return 0;
}

/**
 * gnutls_mac_set_priority:
 * @session: is a #gnutls_session_t structure.
 * @list: is a 0 terminated list of gnutls_mac_algorithm_t elements.
 *
 * Sets the priority on the mac algorithms supported by gnutls.
 * Priority is higher for elements specified before others.  After
 * specifying the algorithms you want, you must append a 0.  Note
 * that the priority is set on the client. The server does not use
 * the algorithm's priority except for disabling algorithms that were
 * not specified.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int gnutls_mac_set_priority(gnutls_session_t session, const int *list)
{
	_set_priority(&session->internals.priorities.mac, list);
	return 0;
}

/**
 * gnutls_compression_set_priority:
 * @session: is a #gnutls_session_t structure.
 * @list: is a 0 terminated list of gnutls_compression_method_t elements.
 *
 * Sets the priority on the compression algorithms supported by
 * gnutls.  Priority is higher for elements specified before others.
 * After specifying the algorithms you want, you must append a 0.
 * Note that the priority is set on the client. The server does not
 * use the algorithm's priority except for disabling algorithms that
 * were not specified.
 *
 * TLS 1.0 does not define any compression algorithms except
 * NULL. Other compression algorithms are to be considered as gnutls
 * extensions.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_compression_set_priority(gnutls_session_t session, const int *list)
{
	_set_priority(&session->internals.priorities.compression, list);
	return 0;
}

/**
 * gnutls_protocol_set_priority:
 * @session: is a #gnutls_session_t structure.
 * @list: is a 0 terminated list of gnutls_protocol_t elements.
 *
 * Sets the priority on the protocol versions supported by gnutls.
 * This function actually enables or disables protocols. Newer protocol
 * versions always have highest priority.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int gnutls_protocol_set_priority(gnutls_session_t session, const int *list)
{
	if (list) {
		_set_priority(&session->internals.priorities.protocol, list);
		/* set the current version to the first in the chain.
		 * This will be overridden later.
		 */
		if (_gnutls_set_current_version(session, list[0]) < 0)
			return gnutls_assert_val(GNUTLS_E_UNSUPPORTED_VERSION_PACKET);
	}

	return 0;
}

/**
 * gnutls_certificate_type_set_priority:
 * @session: is a #gnutls_session_t structure.
 * @list: is a 0 terminated list of gnutls_certificate_type_t elements.
 *
 * Sets the priority on the certificate types supported by gnutls.
 * Priority is higher for elements specified before others.
 * After specifying the types you want, you must append a 0.
 * Note that the certificate type priority is set on the client.
 * The server does not use the cert type priority except for disabling
 * types that were not specified.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_certificate_type_set_priority(gnutls_session_t session,
				     const int *list)
{
#ifdef ENABLE_OPENPGP
	_set_priority(&session->internals.priorities.cert_type, list);
	return 0;
#else

	return GNUTLS_E_UNIMPLEMENTED_FEATURE;

#endif
}

static const int supported_ecc_normal[] = {
#ifdef ENABLE_NON_SUITEB_CURVES
	GNUTLS_ECC_CURVE_SECP192R1,
	GNUTLS_ECC_CURVE_SECP224R1,
#endif
	GNUTLS_ECC_CURVE_SECP256R1,
	GNUTLS_ECC_CURVE_SECP384R1,
	GNUTLS_ECC_CURVE_SECP521R1,
	0
};

static const int supported_ecc_secure128[] = {
	GNUTLS_ECC_CURVE_SECP256R1,
	GNUTLS_ECC_CURVE_SECP384R1,
	GNUTLS_ECC_CURVE_SECP521R1,
	0
};

static const int supported_ecc_suiteb128[] = {
	GNUTLS_ECC_CURVE_SECP256R1,
	GNUTLS_ECC_CURVE_SECP384R1,
	0
};

static const int supported_ecc_suiteb192[] = {
	GNUTLS_ECC_CURVE_SECP384R1,
	0
};

static const int supported_ecc_secure192[] = {
	GNUTLS_ECC_CURVE_SECP384R1,
	GNUTLS_ECC_CURVE_SECP521R1,
	0
};

static const int protocol_priority[] = {
	GNUTLS_TLS1_2,
	GNUTLS_TLS1_1,
	GNUTLS_TLS1_0,
	GNUTLS_SSL3,
	GNUTLS_DTLS1_2,
	GNUTLS_DTLS1_0,
	0
};

static const int dtls_protocol_priority[] = {
	GNUTLS_DTLS1_2,
	GNUTLS_DTLS1_0,
	0
};

static const int protocol_priority_suiteb[] = {
	GNUTLS_TLS1_2,
	0
};

static const int kx_priority_performance[] = {
	GNUTLS_KX_RSA,
#ifdef ENABLE_ECDHE
	GNUTLS_KX_ECDHE_ECDSA,
	GNUTLS_KX_ECDHE_RSA,
#endif
#ifdef ENABLE_DHE
	GNUTLS_KX_DHE_RSA,
	GNUTLS_KX_DHE_DSS,
#endif
	0
};

static const int kx_priority_pfs[] = {
#ifdef ENABLE_ECDHE
	GNUTLS_KX_ECDHE_ECDSA,
	GNUTLS_KX_ECDHE_RSA,
#endif
#ifdef ENABLE_DHE
	GNUTLS_KX_DHE_RSA,
	GNUTLS_KX_DHE_DSS,
#endif
	0
};

static const int kx_priority_suiteb[] = {
	GNUTLS_KX_ECDHE_ECDSA,
	0
};

static const int kx_priority_secure[] = {
	/* The ciphersuites that offer forward secrecy take
	 * precedence
	 */
#ifdef ENABLE_ECDHE
	GNUTLS_KX_ECDHE_ECDSA,
	GNUTLS_KX_ECDHE_RSA,
#endif
	GNUTLS_KX_RSA,
	/* KX-RSA is now ahead of DHE-RSA and DHE-DSS due to the compatibility
	 * issues the DHE ciphersuites have. That is, one cannot enforce a specific
	 * security level without dropping the connection. 
	 */
#ifdef ENABLE_DHE
	GNUTLS_KX_DHE_RSA,
	GNUTLS_KX_DHE_DSS,
#endif
	/* GNUTLS_KX_ANON_DH: Man-in-the-middle prone, don't add!
	 */
	0
};

static const int cipher_priority_performance_default[] = {
	GNUTLS_CIPHER_ARCFOUR_128,
	GNUTLS_CIPHER_AES_128_GCM,
	GNUTLS_CIPHER_AES_256_GCM,
	GNUTLS_CIPHER_CAMELLIA_128_GCM,
	GNUTLS_CIPHER_CAMELLIA_256_GCM,
	GNUTLS_CIPHER_AES_128_CBC,
	GNUTLS_CIPHER_AES_256_CBC,
	GNUTLS_CIPHER_CAMELLIA_128_CBC,
	GNUTLS_CIPHER_CAMELLIA_256_CBC,
	GNUTLS_CIPHER_3DES_CBC,
	0
};

/* If GCM and AES acceleration is available then prefer
 * them over anything else.
 */
static const int cipher_priority_normal_default[] = {
	GNUTLS_CIPHER_AES_128_GCM,
	GNUTLS_CIPHER_AES_256_GCM,
	GNUTLS_CIPHER_CAMELLIA_128_GCM,
	GNUTLS_CIPHER_CAMELLIA_256_GCM,
	GNUTLS_CIPHER_AES_128_CBC,
	GNUTLS_CIPHER_AES_256_CBC,
	GNUTLS_CIPHER_CAMELLIA_128_CBC,
	GNUTLS_CIPHER_CAMELLIA_256_CBC,
	GNUTLS_CIPHER_3DES_CBC,
	GNUTLS_CIPHER_ARCFOUR_128,
	0
};

static const int cipher_priority_performance_fips[] = {
	GNUTLS_CIPHER_AES_128_GCM,
	GNUTLS_CIPHER_AES_256_GCM,
	GNUTLS_CIPHER_AES_128_CBC,
	GNUTLS_CIPHER_AES_256_CBC,
	GNUTLS_CIPHER_3DES_CBC,
	0
};

static const int cipher_priority_normal_fips[] = {
	GNUTLS_CIPHER_AES_128_GCM,
	GNUTLS_CIPHER_AES_256_GCM,
	GNUTLS_CIPHER_AES_128_CBC,
	GNUTLS_CIPHER_AES_256_CBC,
	GNUTLS_CIPHER_3DES_CBC,
	0
};


static const int cipher_priority_suiteb128[] = {
	GNUTLS_CIPHER_AES_128_GCM,
	GNUTLS_CIPHER_AES_256_GCM,
	0
};

static const int cipher_priority_suiteb192[] = {
	GNUTLS_CIPHER_AES_256_GCM,
	0
};


static const int cipher_priority_secure128[] = {
	GNUTLS_CIPHER_AES_128_GCM,
	GNUTLS_CIPHER_CAMELLIA_128_GCM,
	GNUTLS_CIPHER_AES_256_GCM,
	GNUTLS_CIPHER_CAMELLIA_256_GCM,

	GNUTLS_CIPHER_AES_128_CBC,
	GNUTLS_CIPHER_CAMELLIA_128_CBC,
	GNUTLS_CIPHER_AES_256_CBC,
	GNUTLS_CIPHER_CAMELLIA_256_CBC,
	0
};


static const int cipher_priority_secure192[] = {
	GNUTLS_CIPHER_AES_256_GCM,
	GNUTLS_CIPHER_CAMELLIA_256_GCM,
	GNUTLS_CIPHER_AES_256_CBC,
	GNUTLS_CIPHER_CAMELLIA_256_CBC,
	0
};

static const int comp_priority[] = {
	/* compression should be explicitly requested to be enabled */
	GNUTLS_COMP_NULL,
	0
};

static const int sign_priority_default[] = {
	GNUTLS_SIGN_RSA_SHA256,
	GNUTLS_SIGN_DSA_SHA256,
	GNUTLS_SIGN_ECDSA_SHA256,

	GNUTLS_SIGN_RSA_SHA384,
	GNUTLS_SIGN_ECDSA_SHA384,

	GNUTLS_SIGN_RSA_SHA512,
	GNUTLS_SIGN_ECDSA_SHA512,

	GNUTLS_SIGN_RSA_SHA224,
	GNUTLS_SIGN_DSA_SHA224,
	GNUTLS_SIGN_ECDSA_SHA224,

	GNUTLS_SIGN_RSA_SHA1,
	GNUTLS_SIGN_DSA_SHA1,
	GNUTLS_SIGN_ECDSA_SHA1,
	0
};

static const int sign_priority_suiteb128[] = {
	GNUTLS_SIGN_ECDSA_SHA256,
	GNUTLS_SIGN_ECDSA_SHA384,
	0
};

static const int sign_priority_suiteb192[] = {
	GNUTLS_SIGN_ECDSA_SHA384,
	0
};

static const int sign_priority_secure128[] = {
	GNUTLS_SIGN_RSA_SHA256,
	GNUTLS_SIGN_DSA_SHA256,
	GNUTLS_SIGN_ECDSA_SHA256,
	GNUTLS_SIGN_RSA_SHA384,
	GNUTLS_SIGN_ECDSA_SHA384,
	GNUTLS_SIGN_RSA_SHA512,
	GNUTLS_SIGN_ECDSA_SHA512,
	0
};

static const int sign_priority_secure192[] = {
	GNUTLS_SIGN_RSA_SHA384,
	GNUTLS_SIGN_ECDSA_SHA384,
	GNUTLS_SIGN_RSA_SHA512,
	GNUTLS_SIGN_ECDSA_SHA512,
	0
};

static const int mac_priority_normal_default[] = {
	GNUTLS_MAC_SHA1,
	GNUTLS_MAC_SHA256,
	GNUTLS_MAC_SHA384,
	GNUTLS_MAC_AEAD,
	GNUTLS_MAC_MD5,
	0
};

static const int mac_priority_normal_fips[] = {
	GNUTLS_MAC_SHA1,
	GNUTLS_MAC_SHA256,
	GNUTLS_MAC_SHA384,
	GNUTLS_MAC_AEAD,
	0
};

static const int * cipher_priority_performance = cipher_priority_performance_default;
static const int * cipher_priority_normal = cipher_priority_normal_default;
static const int * mac_priority_normal = mac_priority_normal_default;

/* if called with replace the default priorities with the FIPS140 ones */
void _gnutls_priority_update_fips(void)
{
	cipher_priority_performance = cipher_priority_performance_fips;
	cipher_priority_normal = cipher_priority_normal_fips;
	mac_priority_normal = mac_priority_normal_fips;
}

static const int mac_priority_suiteb128[] = {
	GNUTLS_MAC_AEAD,
	0
};

static const int mac_priority_suiteb192[] = {
	GNUTLS_MAC_AEAD,
	0
};

static const int mac_priority_secure128[] = {
	GNUTLS_MAC_SHA1,
	GNUTLS_MAC_SHA256,
	GNUTLS_MAC_SHA384,
	GNUTLS_MAC_AEAD,
	0
};

static const int mac_priority_secure192[] = {
	GNUTLS_MAC_SHA256,
	GNUTLS_MAC_SHA384,
	GNUTLS_MAC_AEAD,
	0
};

static const int cert_type_priority_default[] = {
	GNUTLS_CRT_X509,
	0
};

static const int cert_type_priority_all[] = {
	GNUTLS_CRT_X509,
	GNUTLS_CRT_OPENPGP,
	0
};

typedef void (rmadd_func) (priority_st * priority_list, unsigned int alg);

static void prio_remove(priority_st * priority_list, unsigned int algo)
{
	unsigned int i;

	for (i = 0; i < priority_list->algorithms; i++) {
		if (priority_list->priority[i] == algo) {
			priority_list->algorithms--;
			if ((priority_list->algorithms - i) > 0)
				memmove(&priority_list->priority[i],
					&priority_list->priority[i + 1],
					(priority_list->algorithms -
					 i) *
					sizeof(priority_list->
					       priority[0]));
			priority_list->priority[priority_list->
						algorithms] = 0;
			break;
		}
	}

	return;
}

static void prio_add(priority_st * priority_list, unsigned int algo)
{
	unsigned int i, l = priority_list->algorithms;

	if (l >= MAX_ALGOS)
		return;		/* can't add it anyway */

	for (i = 0; i < l; ++i) {
		if (algo == priority_list->priority[i])
			return;	/* if it exists */
	}

	priority_list->priority[l] = algo;
	priority_list->algorithms++;

	return;
}


/**
 * gnutls_priority_set:
 * @session: is a #gnutls_session_t structure.
 * @priority: is a #gnutls_priority_t structure.
 *
 * Sets the priorities to use on the ciphers, key exchange methods,
 * macs and compression methods.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_priority_set(gnutls_session_t session, gnutls_priority_t priority)
{
	if (priority == NULL) {
		gnutls_assert();
		return GNUTLS_E_NO_CIPHER_SUITES;
	}

	memcpy(&session->internals.priorities, priority,
	       sizeof(struct gnutls_priority_st));

	/* set the current version to the first in the chain.
	 * This will be overridden later.
	 */
	if (session->internals.priorities.protocol.algorithms > 0) {
		if (_gnutls_set_current_version(session,
					    session->internals.priorities.
					    protocol.priority[0]) < 0) {
			return gnutls_assert_val(GNUTLS_E_UNSUPPORTED_VERSION_PACKET);
		}
	}

	if (session->internals.priorities.protocol.algorithms == 0 ||
	    session->internals.priorities.cipher.algorithms == 0 ||
	    session->internals.priorities.mac.algorithms == 0 ||
	    session->internals.priorities.kx.algorithms == 0 ||
	    session->internals.priorities.compression.algorithms == 0)
		return gnutls_assert_val(GNUTLS_E_NO_PRIORITIES_WERE_SET);

	return 0;
}


#define LEVEL_NONE "NONE"
#define LEVEL_NORMAL "NORMAL"
#define LEVEL_PFS "PFS"
#define LEVEL_PERFORMANCE "PERFORMANCE"
#define LEVEL_SECURE128 "SECURE128"
#define LEVEL_SECURE192 "SECURE192"
#define LEVEL_SECURE256 "SECURE256"
#define LEVEL_SUITEB128 "SUITEB128"
#define LEVEL_SUITEB192 "SUITEB192"
#define LEVEL_EXPORT "EXPORT"
#define LEVEL_LEGACY "LEGACY"

#define SET_PROFILE(to_set) \
	profile = GNUTLS_VFLAGS_TO_PROFILE(priority_cache->additional_verify_flags); \
	if (profile == 0 || profile > to_set) { \
		priority_cache->additional_verify_flags &= ~GNUTLS_VFLAGS_PROFILE_MASK; \
		priority_cache->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(to_set); \
	}

#define SET_LEVEL(to_set) \
		if (priority_cache->level == 0 || priority_cache->level > to_set) \
			priority_cache->level = to_set

static
int check_level(const char *level, gnutls_priority_t priority_cache,
		int add)
{
	bulk_rmadd_func *func;
	unsigned profile = 0;

	if (add)
		func = _add_priority;
	else
		func = _set_priority;

	if (strcasecmp(level, LEVEL_PERFORMANCE) == 0) {
		func(&priority_cache->cipher, cipher_priority_performance);
		func(&priority_cache->kx, kx_priority_performance);
		func(&priority_cache->mac, mac_priority_normal);
		func(&priority_cache->sign_algo, sign_priority_default);
		func(&priority_cache->supported_ecc, supported_ecc_normal);

		SET_PROFILE(GNUTLS_PROFILE_LOW); /* set certificate level */
		SET_LEVEL(GNUTLS_SEC_PARAM_WEAK); /* set DH params level */
		return 1;
	} else if (strcasecmp(level, LEVEL_NORMAL) == 0) {
		func(&priority_cache->cipher, cipher_priority_normal);
		func(&priority_cache->kx, kx_priority_secure);
		func(&priority_cache->mac, mac_priority_normal);
		func(&priority_cache->sign_algo, sign_priority_default);
		func(&priority_cache->supported_ecc, supported_ecc_normal);

		SET_PROFILE(GNUTLS_PROFILE_LOW);
		SET_LEVEL(GNUTLS_SEC_PARAM_WEAK);
		return 1;
	} else if (strcasecmp(level, LEVEL_PFS) == 0) {
		func(&priority_cache->cipher, cipher_priority_normal);
		func(&priority_cache->kx, kx_priority_pfs);
		func(&priority_cache->mac, mac_priority_secure128);
		func(&priority_cache->sign_algo, sign_priority_default);
		func(&priority_cache->supported_ecc, supported_ecc_normal);

		SET_PROFILE(GNUTLS_PROFILE_LOW);
		SET_LEVEL(GNUTLS_SEC_PARAM_WEAK);
		return 1;
	} else if (strcasecmp(level, LEVEL_SECURE256) == 0
		   || strcasecmp(level, LEVEL_SECURE192) == 0) {
		func(&priority_cache->cipher, cipher_priority_secure192);
		func(&priority_cache->kx, kx_priority_secure);
		func(&priority_cache->mac, mac_priority_secure192);
		func(&priority_cache->sign_algo, sign_priority_secure192);
		func(&priority_cache->supported_ecc,
		     supported_ecc_secure192);

		SET_PROFILE(GNUTLS_PROFILE_HIGH);
		SET_LEVEL(GNUTLS_SEC_PARAM_HIGH);
		return 1;
	} else if (strcasecmp(level, LEVEL_SECURE128) == 0
		   || strcasecmp(level, "SECURE") == 0) {
		func(&priority_cache->cipher, cipher_priority_secure128);
		func(&priority_cache->kx, kx_priority_secure);
		func(&priority_cache->mac, mac_priority_secure128);
		func(&priority_cache->sign_algo, sign_priority_secure128);
		func(&priority_cache->supported_ecc,
		     supported_ecc_secure128);

		/* The profile should have been HIGH but if we don't allow
		 * SHA-1 (80-bits) as signature algorithm we are not able
		 * to connect anywhere with this level */
		SET_PROFILE(GNUTLS_PROFILE_LOW);
		SET_LEVEL(GNUTLS_SEC_PARAM_LOW);
		return 1;
	} else if (strcasecmp(level, LEVEL_SUITEB128) == 0) {
		func(&priority_cache->protocol, protocol_priority_suiteb);
		func(&priority_cache->cipher, cipher_priority_suiteb128);
		func(&priority_cache->kx, kx_priority_suiteb);
		func(&priority_cache->mac, mac_priority_suiteb128);
		func(&priority_cache->sign_algo, sign_priority_suiteb128);
		func(&priority_cache->supported_ecc,
		     supported_ecc_suiteb128);

		SET_PROFILE(GNUTLS_PROFILE_SUITEB128);
		SET_LEVEL(GNUTLS_SEC_PARAM_HIGH);
		return 1;
	} else if (strcasecmp(level, LEVEL_SUITEB192) == 0) {
		func(&priority_cache->protocol, protocol_priority_suiteb);
		func(&priority_cache->cipher, cipher_priority_suiteb192);
		func(&priority_cache->kx, kx_priority_suiteb);
		func(&priority_cache->mac, mac_priority_suiteb192);
		func(&priority_cache->sign_algo, sign_priority_suiteb192);
		func(&priority_cache->supported_ecc,
		     supported_ecc_suiteb192);

		SET_PROFILE(GNUTLS_PROFILE_SUITEB192);
		SET_LEVEL(GNUTLS_SEC_PARAM_ULTRA);
		return 1;
	} else if (strcasecmp(level, LEVEL_LEGACY) == 0) {
		func(&priority_cache->cipher, cipher_priority_normal);
		func(&priority_cache->kx, kx_priority_secure);
		func(&priority_cache->mac, mac_priority_normal);
		func(&priority_cache->sign_algo, sign_priority_default);
		func(&priority_cache->supported_ecc, supported_ecc_normal);

		SET_LEVEL(GNUTLS_SEC_PARAM_VERY_WEAK);
		return 1;
	} else if (strcasecmp(level, LEVEL_EXPORT) == 0) {
		func(&priority_cache->cipher, cipher_priority_performance);
		func(&priority_cache->kx, kx_priority_performance);
		func(&priority_cache->mac, mac_priority_secure128);
		func(&priority_cache->sign_algo, sign_priority_default);
		func(&priority_cache->supported_ecc, supported_ecc_normal);

		SET_LEVEL(GNUTLS_SEC_PARAM_EXPORT);
		return 1;
	}
	return 0;
}

static void enable_compat(gnutls_priority_t c)
{
	ENABLE_COMPAT(c);
}
static void enable_dumbfw(gnutls_priority_t c)
{
	c->dumbfw = 1;
}
static void enable_no_extensions(gnutls_priority_t c)
{
	c->no_extensions = 1;
}
static void enable_stateless_compression(gnutls_priority_t c)
{
	c->stateless_compression = 1;
}
static void disable_wildcards(gnutls_priority_t c)
{
	c->additional_verify_flags |= GNUTLS_VERIFY_DO_NOT_ALLOW_WILDCARDS;
}
static void enable_profile_very_weak(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_VERY_WEAK);
	c->level = GNUTLS_SEC_PARAM_VERY_WEAK;
}
static void enable_profile_low(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_LOW);
	c->level = GNUTLS_SEC_PARAM_LOW;
}
static void enable_profile_legacy(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_LEGACY);
	c->level = GNUTLS_SEC_PARAM_LEGACY;
}
static void enable_profile_high(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_HIGH);
	c->level = GNUTLS_SEC_PARAM_HIGH;
}
static void enable_profile_ultra(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_ULTRA);
	c->level = GNUTLS_SEC_PARAM_ULTRA;
}
static void enable_profile_medium(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_MEDIUM);
	c->level = GNUTLS_SEC_PARAM_MEDIUM;
}
static void enable_profile_suiteb128(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_SUITEB128);
	c->level = GNUTLS_SEC_PARAM_HIGH;
}
static void enable_profile_suiteb192(gnutls_priority_t c)
{
	c->additional_verify_flags &= 0x00ffffff;
	c->additional_verify_flags |= GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_SUITEB192);
	c->level = GNUTLS_SEC_PARAM_ULTRA;
}
static void enable_safe_renegotiation(gnutls_priority_t c)
{
	c->sr = SR_SAFE;

}
static void enable_unsafe_renegotiation(gnutls_priority_t c)
{
	c->sr = SR_UNSAFE;
}
static void enable_partial_safe_renegotiation(gnutls_priority_t c)
{
	c->sr = SR_PARTIAL;
}
static void disable_safe_renegotiation(gnutls_priority_t c)
{
	c->sr = SR_DISABLED;
}
static void enable_latest_record_version(gnutls_priority_t c)
{
	c->ssl3_record_version = 0;
}
static void enable_ssl3_record_version(gnutls_priority_t c)
{
	c->ssl3_record_version = 1;
}
static void enable_verify_allow_rsa_md5(gnutls_priority_t c)
{
	c->additional_verify_flags |=
	    GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD5;
}
static void disable_crl_checks(gnutls_priority_t c)
{
	c->additional_verify_flags |=
		GNUTLS_VERIFY_DISABLE_CRL_CHECKS;
}
static void enable_server_precedence(gnutls_priority_t c)
{
	c->server_precedence = 1;
}
static void dummy_func(gnutls_priority_t c)
{
}

#include <priority_options.h>

static char *check_str(char *line, size_t line_size, const char *needle, size_t needle_size)
{
	char *p;
	unsigned n;

	while (c_isspace(*line)) {
		line++;
		line_size--;
	}

	if (line[0] == '#' || needle_size >= line_size)
		return NULL;

	if (memcmp(line, needle, needle_size) == 0) {
		p = &line[needle_size];
		while (c_isspace(*p)) {
			p++;
		}
		if (*p != '=') {
			return NULL;
		} else
			p++;

		while (c_isspace(*p)) {
			p++;
		}

		n = strlen(p);

		if (n > 1 && p[n-1] == '\n') {
			n--;
			p[n] = 0;
		}

		if (n > 1 && p[n-1] == '\r') {
			n--;
			p[n] = 0;
		}
		return p;
	}

	return NULL;
}

/* Returns the new priorities if SYSTEM is specified in
 * an allocated string, or just a copy of the provided
 * priorities, appended with any additional present in
 * the priorities string.
 *
 * The returned string must be released using free().
 */
static char *resolve_priorities(const char* priorities)
{
char *p = (char*)priorities;
char *additional = NULL;
char *ret = NULL;
char *ss, *line = NULL;
unsigned ss_len;
int l;
FILE* fp = NULL;
size_t n, n2 = 0, line_size;

	while (c_isspace(*p))
		p++;

	if (*p == '@') {
		ss = p+1;

		additional = strchr(p, ':');
		if (additional != NULL) {
			ss_len = additional - ss;
			additional++;
		} else {
			ss_len = strlen(ss);
		}

		fp = fopen(SYSTEM_PRIORITY_FILE, "r");
		if (fp == NULL) {/* fail */
			ret = NULL;
			goto finish;
		}

		do {
			l = getline(&line, &line_size, fp);
			if (l > 0) {
				p = check_str(line, line_size, ss, ss_len);
				if (p != NULL)
					break;
			}
		} while (l>0);

		if (p == NULL) {
			ret = NULL;
			goto finish;
		}

		n = strlen(p);
		if (additional)
			n2 = strlen(additional);

		ret = malloc(n+n2+1+1);
		if (ret == NULL) {
			goto finish;
		}

		memcpy(ret, p, n);
		if (additional != NULL) {
			ret[n] = ':';
			memcpy(&ret[n+1], additional, n2);
			ret[n+n2+1] = 0;
		} else {
			ret[n] = 0;
		}
	} else {
		return strdup(p);
	}

finish:
	if (ret != NULL) {
		_gnutls_debug_log("selected priority string: %s\n", ret);
	}
	free(line);
	if (fp != NULL)
		fclose(fp);

	return ret;
}

/**
 * gnutls_priority_init:
 * @priority_cache: is a #gnutls_prioritity_t structure.
 * @priorities: is a string describing priorities
 * @err_pos: In case of an error this will have the position in the string the error occurred
 *
 * Sets priorities for the ciphers, key exchange methods, macs and
 * compression methods.
 *
 * The #priorities option allows you to specify a colon
 * separated list of the cipher priorities to enable.
 * Some keywords are defined to provide quick access
 * to common preferences.
 *
 * Unless there is a special need, use the "NORMAL" keyword to
 * apply a reasonable security level, or "NORMAL:%COMPAT" for compatibility.
 *
 * "PERFORMANCE" means all the "secure" ciphersuites are enabled,
 * limited to 128 bit ciphers and sorted by terms of speed
 * performance.
 *
 * "LEGACY" the NORMAL settings for GnuTLS 3.2.x or earlier. There is
 * no verification profile set, and the allowed DH primes are considered
 * weak today.
 *
 * "NORMAL" means all "secure" ciphersuites. The 256-bit ciphers are
 * included as a fallback only.  The ciphers are sorted by security
 * margin.
 *
 * "PFS" means all "secure" ciphersuites that support perfect forward secrecy. 
 * The 256-bit ciphers are included as a fallback only.  
 * The ciphers are sorted by security margin.
 *
 * "SECURE128" means all "secure" ciphersuites of security level 128-bit
 * or more.
 *
 * "SECURE192" means all "secure" ciphersuites of security level 192-bit
 * or more.
 *
 * "SUITEB128" means all the NSA SuiteB ciphersuites with security level
 * of 128.
 *
 * "SUITEB192" means all the NSA SuiteB ciphersuites with security level
 * of 192.
 *
 * "EXPORT" means all ciphersuites are enabled, including the
 * low-security 40 bit ciphers.
 *
 * "NONE" means nothing is enabled.  This disables even protocols and
 * compression methods.
 *
 * "@KEYWORD" The system administrator imposed settings. The provided keywords
 * will be expanded from a configuration-time provided file - default is:
 * /etc/gnutls/default-priorities. Any keywords that follow it, will 
 * be appended to the expanded string. If there is no system string,
 * then the function will fail. The system file should be formatted
 * as "KEYWORD=VALUE", e.g., "SYSTEM=NORMAL:-ARCFOUR-128".
 *
 * Special keywords are "!", "-" and "+".
 * "!" or "-" appended with an algorithm will remove this algorithm.
 * "+" appended with an algorithm will add this algorithm.
 *
 * Check the GnuTLS manual section "Priority strings" for detailed
 * information.
 *
 * Examples:
 *
 * "NONE:+VERS-TLS-ALL:+MAC-ALL:+RSA:+AES-128-CBC:+SIGN-ALL:+COMP-NULL"
 *
 * "NORMAL:-ARCFOUR-128" means normal ciphers except for ARCFOUR-128.
 *
 * "SECURE128:-VERS-SSL3.0:+COMP-DEFLATE" means that only secure ciphers are
 * enabled, SSL3.0 is disabled, and libz compression enabled.
 *
 * "NONE:+VERS-TLS-ALL:+AES-128-CBC:+RSA:+SHA1:+COMP-NULL:+SIGN-RSA-SHA1", 
 *
 * "NONE:+VERS-TLS-ALL:+AES-128-CBC:+ECDHE-RSA:+SHA1:+COMP-NULL:+SIGN-RSA-SHA1:+CURVE-SECP256R1", 
 *
 * "SECURE256:+SECURE128",
 *
 * Note that "NORMAL:%COMPAT" is the most compatible mode.
 *
 * Returns: On syntax error %GNUTLS_E_INVALID_REQUEST is returned,
 * %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_priority_init(gnutls_priority_t * priority_cache,
		     const char *priorities, const char **err_pos)
{
	char *broken_list[MAX_ELEMENTS];
	int broken_list_size = 0, i = 0, j;
	char *darg = NULL;
	unsigned ikeyword_set = 0;
	int algo;
	rmadd_func *fn;
	bulk_rmadd_func *bulk_fn;

	if (err_pos)
		*err_pos = priorities;

	*priority_cache =
	    gnutls_calloc(1, sizeof(struct gnutls_priority_st));
	if (*priority_cache == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	/* for now unsafe renegotiation is default on everyone. To be removed
	 * when we make it the default.
	 */
	(*priority_cache)->sr = SR_PARTIAL;
	(*priority_cache)->ssl3_record_version = 1;

	(*priority_cache)->max_empty_records = DEFAULT_MAX_EMPTY_RECORDS;

	if (priorities == NULL)
		priorities = "NORMAL";

	darg = resolve_priorities(priorities);
	if (darg == NULL) {
		gnutls_assert();
		goto error;
	}

	break_list(darg, broken_list, &broken_list_size);
	/* This is our default set of protocol version, certificate types and
	 * compression methods.
	 */
	if (strcasecmp(broken_list[0], LEVEL_NONE) != 0) {
		_set_priority(&(*priority_cache)->protocol,
			      protocol_priority);
		_set_priority(&(*priority_cache)->compression,
			      comp_priority);
		_set_priority(&(*priority_cache)->cert_type,
			      cert_type_priority_default);
		_set_priority(&(*priority_cache)->sign_algo,
			      sign_priority_default);
		_set_priority(&(*priority_cache)->supported_ecc,
			      supported_ecc_normal);
		i = 0;
	} else {
		ikeyword_set = 1;
		i = 1;
	}

	for (; i < broken_list_size; i++) {
		if (check_level(broken_list[i], *priority_cache, ikeyword_set) != 0) {
			ikeyword_set = 1;
			continue;
		} else if (broken_list[i][0] == '!'
			   || broken_list[i][0] == '+'
			   || broken_list[i][0] == '-') {
			if (broken_list[i][0] == '+') {
				fn = prio_add;
				bulk_fn = _add_priority;
			} else {
				fn = prio_remove;
				bulk_fn = _clear_priorities;
			}

			if (broken_list[i][0] == '+'
			    && check_level(&broken_list[i][1],
					   *priority_cache, 1) != 0) {
				continue;
			} else if ((algo =
				    gnutls_mac_get_id(&broken_list[i][1]))
				   != GNUTLS_MAC_UNKNOWN)
				fn(&(*priority_cache)->mac, algo);
			else if ((algo =
				  gnutls_cipher_get_id(&broken_list[i][1]))
				 != GNUTLS_CIPHER_UNKNOWN)
				fn(&(*priority_cache)->cipher, algo);
			else if ((algo =
				  gnutls_kx_get_id(&broken_list[i][1])) !=
				 GNUTLS_KX_UNKNOWN)
				fn(&(*priority_cache)->kx, algo);
			else if (strncasecmp
				 (&broken_list[i][1], "VERS-", 5) == 0) {
				if (strncasecmp
				    (&broken_list[i][1], "VERS-TLS-ALL",
				     12) == 0) {
					bulk_fn(&(*priority_cache)->
						protocol,
						protocol_priority);
				} else
				    if (strncasecmp
					(&broken_list[i][1],
					 "VERS-DTLS-ALL", 13) == 0) {
					bulk_fn(&(*priority_cache)->
						protocol,
						dtls_protocol_priority);
				} else {
					if ((algo =
					     gnutls_protocol_get_id
					     (&broken_list[i][6])) !=
					    GNUTLS_VERSION_UNKNOWN)
						fn(&(*priority_cache)->
						   protocol, algo);
					else
						goto error;

				}
			} /* now check if the element is something like -ALGO */
			else if (strncasecmp
				 (&broken_list[i][1], "COMP-", 5) == 0) {
				if (strncasecmp
				    (&broken_list[i][1], "COMP-ALL",
				     8) == 0) {
					bulk_fn(&(*priority_cache)->
						compression,
						comp_priority);
				} else {
					if ((algo =
					     gnutls_compression_get_id
					     (&broken_list[i][6])) !=
					    GNUTLS_COMP_UNKNOWN)
						fn(&(*priority_cache)->
						   compression, algo);
					else
						goto error;
				}
			} /* now check if the element is something like -ALGO */
			else if (strncasecmp
				 (&broken_list[i][1], "CURVE-", 6) == 0) {
				if (strncasecmp
				    (&broken_list[i][1], "CURVE-ALL",
				     9) == 0) {
					bulk_fn(&(*priority_cache)->
						supported_ecc,
						supported_ecc_normal);
				} else {
					if ((algo =
					     _gnutls_ecc_curve_get_id
					     (&broken_list[i][7])) !=
					    GNUTLS_ECC_CURVE_INVALID)
						fn(&(*priority_cache)->
						   supported_ecc, algo);
					else
						goto error;
				}
			} /* now check if the element is something like -ALGO */
			else if (strncasecmp
				 (&broken_list[i][1], "CTYPE-", 6) == 0) {
				if (strncasecmp
				    (&broken_list[i][1], "CTYPE-ALL",
				     9) == 0) {
					bulk_fn(&(*priority_cache)->
						cert_type,
						cert_type_priority_all);
				} else {
					if ((algo =
					     gnutls_certificate_type_get_id
					     (&broken_list[i][7])) !=
					    GNUTLS_CRT_UNKNOWN)
						fn(&(*priority_cache)->
						   cert_type, algo);
					else
						goto error;
				}
			} /* now check if the element is something like -ALGO */
			else if (strncasecmp
				 (&broken_list[i][1], "SIGN-", 5) == 0) {
				if (strncasecmp
				    (&broken_list[i][1], "SIGN-ALL",
				     8) == 0) {
					bulk_fn(&(*priority_cache)->
						sign_algo,
						sign_priority_default);
				} else {
					if ((algo =
					     gnutls_sign_get_id
					     (&broken_list[i][6])) !=
					    GNUTLS_SIGN_UNKNOWN)
						fn(&(*priority_cache)->
						   sign_algo, algo);
					else
						goto error;
				}
			} else
			    if (strncasecmp
				(&broken_list[i][1], "MAC-ALL", 7) == 0) {
				bulk_fn(&(*priority_cache)->mac,
					mac_priority_normal);
			} else
			    if (strncasecmp
				(&broken_list[i][1], "CIPHER-ALL",
				 10) == 0) {
				bulk_fn(&(*priority_cache)->cipher,
					cipher_priority_normal);
			} else
			    if (strncasecmp
				(&broken_list[i][1], "KX-ALL", 6) == 0) {
				bulk_fn(&(*priority_cache)->kx,
					kx_priority_secure);
			} else
				goto error;
		} else if (broken_list[i][0] == '%') {
			const struct priority_options_st * o;
			/* to add a new option modify
			 * priority_options.gperf */
			o = in_word_set(&broken_list[i][1], strlen(&broken_list[i][1]));
			if (o == NULL) {
				goto error;
			}
			o->func(*priority_cache);
		} else
			goto error;
	}

	free(darg);
	return 0;

      error:
	if (err_pos != NULL && i < broken_list_size) {
		*err_pos = priorities;
		for (j = 0; j < i; j++) {
			(*err_pos) += strlen(broken_list[j]) + 1;
		}
	}
	free(darg);
	gnutls_free(*priority_cache);
	*priority_cache = NULL;

	return GNUTLS_E_INVALID_REQUEST;

}

/**
 * gnutls_priority_deinit:
 * @priority_cache: is a #gnutls_prioritity_t structure.
 *
 * Deinitializes the priority cache.
 **/
void gnutls_priority_deinit(gnutls_priority_t priority_cache)
{
	gnutls_free(priority_cache);
}


/**
 * gnutls_priority_set_direct:
 * @session: is a #gnutls_session_t structure.
 * @priorities: is a string describing priorities
 * @err_pos: In case of an error this will have the position in the string the error occured
 *
 * Sets the priorities to use on the ciphers, key exchange methods,
 * macs and compression methods.  This function avoids keeping a
 * priority cache and is used to directly set string priorities to a
 * TLS session.  For documentation check the gnutls_priority_init().
 *
 * To simply use a reasonable default, consider using gnutls_set_default_priority().
 *
 * Returns: On syntax error %GNUTLS_E_INVALID_REQUEST is returned,
 * %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_priority_set_direct(gnutls_session_t session,
			   const char *priorities, const char **err_pos)
{
	gnutls_priority_t prio;
	int ret;

	ret = gnutls_priority_init(&prio, priorities, err_pos);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret = gnutls_priority_set(session, prio);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	gnutls_priority_deinit(prio);

	return 0;
}

/* Breaks a list of "xxx", "yyy", to a character array, of
 * MAX_COMMA_SEP_ELEMENTS size; Note that the given string is modified.
  */
static void
break_list(char *list,
		 char *broken_list[MAX_ELEMENTS], int *size)
{
	char *p = list;

	*size = 0;

	do {
		broken_list[*size] = p;

		(*size)++;

		p = strchr(p, ':');
		if (p) {
			*p = 0;
			p++;	/* move to next entry and skip white
				 * space.
				 */
			while (*p == ' ')
				p++;
		}
	}
	while (p != NULL && *size < MAX_ELEMENTS);
}

/**
 * gnutls_set_default_priority:
 * @session: is a #gnutls_session_t structure.
 *
 * Sets some default priority on the ciphers, key exchange methods,
 * macs and compression methods.
 *
 * This typically sets a default priority that is considered
 * sufficiently secure to establish encrypted sessions.
 *
 * This function is kept around for backwards compatibility, but
 * because of its wide use it is still fully supported.  If you wish
 * to allow users to provide a string that specify which ciphers to
 * use (which is recommended), you should use
 * gnutls_priority_set_direct() or gnutls_priority_set() instead.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int gnutls_set_default_priority(gnutls_session_t session)
{
	return gnutls_priority_set_direct(session, NULL, NULL);
}

/**
 * gnutls_set_default_export_priority:
 * @session: is a #gnutls_session_t structure.
 *
 * Sets some default priority on the ciphers, key exchange methods, macs
 * and compression methods.  This function also includes weak algorithms.
 *
 * This is the same as calling:
 *
 * gnutls_priority_set_direct (session, "EXPORT", NULL);
 *
 * This function is kept around for backwards compatibility, but
 * because of its wide use it is still fully supported.  If you wish
 * to allow users to provide a string that specify which ciphers to
 * use (which is recommended), you should use
 * gnutls_priority_set_direct() or gnutls_priority_set() instead.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int gnutls_set_default_export_priority(gnutls_session_t session)
{
	return gnutls_priority_set_direct(session, "EXPORT", NULL);
}

/**
 * gnutls_priority_ecc_curve_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available elliptic curves in the priority
 * structure. 
 *
 * Returns: the number of curves, or an error code.
 * Since: 3.0
 **/
int
gnutls_priority_ecc_curve_list(gnutls_priority_t pcache,
			       const unsigned int **list)
{
	if (pcache->supported_ecc.algorithms == 0)
		return 0;

	*list = pcache->supported_ecc.priority;
	return pcache->supported_ecc.algorithms;
}

/**
 * gnutls_priority_kx_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available key exchange methods in the priority
 * structure. 
 *
 * Returns: the number of curves, or an error code.
 * Since: 3.2.3
 **/
int
gnutls_priority_kx_list(gnutls_priority_t pcache,
			const unsigned int **list)
{
	if (pcache->kx.algorithms == 0)
		return 0;

	*list = pcache->kx.priority;
	return pcache->kx.algorithms;
}

/**
 * gnutls_priority_cipher_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available ciphers in the priority
 * structure. 
 *
 * Returns: the number of curves, or an error code.
 * Since: 3.2.3
 **/
int
gnutls_priority_cipher_list(gnutls_priority_t pcache,
			    const unsigned int **list)
{
	if (pcache->cipher.algorithms == 0)
		return 0;

	*list = pcache->cipher.priority;
	return pcache->cipher.algorithms;
}

/**
 * gnutls_priority_mac_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available MAC algorithms in the priority
 * structure. 
 *
 * Returns: the number of curves, or an error code.
 * Since: 3.2.3
 **/
int
gnutls_priority_mac_list(gnutls_priority_t pcache,
			 const unsigned int **list)
{
	if (pcache->mac.algorithms == 0)
		return 0;

	*list = pcache->mac.priority;
	return pcache->mac.algorithms;
}

/**
 * gnutls_priority_compression_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available compression method in the priority
 * structure. 
 *
 * Returns: the number of methods, or an error code.
 * Since: 3.0
 **/
int
gnutls_priority_compression_list(gnutls_priority_t pcache,
				 const unsigned int **list)
{
	if (pcache->compression.algorithms == 0)
		return 0;

	*list = pcache->compression.priority;
	return pcache->compression.algorithms;
}

/**
 * gnutls_priority_protocol_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available TLS version numbers in the priority
 * structure. 
 *
 * Returns: the number of protocols, or an error code.
 * Since: 3.0
 **/
int
gnutls_priority_protocol_list(gnutls_priority_t pcache,
			      const unsigned int **list)
{
	if (pcache->protocol.algorithms == 0)
		return 0;

	*list = pcache->protocol.priority;
	return pcache->protocol.algorithms;
}

/**
 * gnutls_priority_sign_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available signature algorithms in the priority
 * structure. 
 *
 * Returns: the number of algorithms, or an error code.
 * Since: 3.0
 **/
int
gnutls_priority_sign_list(gnutls_priority_t pcache,
			  const unsigned int **list)
{
	if (pcache->sign_algo.algorithms == 0)
		return 0;

	*list = pcache->sign_algo.priority;
	return pcache->sign_algo.algorithms;
}

/**
 * gnutls_priority_certificate_type_list:
 * @pcache: is a #gnutls_prioritity_t structure.
 * @list: will point to an integer list
 *
 * Get a list of available certificate types in the priority
 * structure. 
 *
 * Returns: the number of certificate types, or an error code.
 * Since: 3.0
 **/
int
gnutls_priority_certificate_type_list(gnutls_priority_t pcache,
				      const unsigned int **list)
{
	if (pcache->cert_type.algorithms == 0)
		return 0;

	*list = pcache->cert_type.priority;
	return pcache->cert_type.algorithms;
}
