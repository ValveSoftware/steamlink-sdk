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


extern mod_auth_st rsa_auth_struct;
extern mod_auth_st dhe_rsa_auth_struct;
extern mod_auth_st ecdhe_rsa_auth_struct;
extern mod_auth_st ecdhe_psk_auth_struct;
extern mod_auth_st ecdhe_ecdsa_auth_struct;
extern mod_auth_st dhe_dss_auth_struct;
extern mod_auth_st anon_auth_struct;
extern mod_auth_st anon_ecdh_auth_struct;
extern mod_auth_st srp_auth_struct;
extern mod_auth_st psk_auth_struct;
extern mod_auth_st dhe_psk_auth_struct;
extern mod_auth_st rsa_psk_auth_struct;
extern mod_auth_st srp_rsa_auth_struct;
extern mod_auth_st srp_dss_auth_struct;


/* Cred type mappings to KX algorithms 
 * FIXME: The mappings are not 1-1. Some KX such as SRP_RSA require
 * more than one credentials type.
 */
typedef struct {
	gnutls_kx_algorithm_t algorithm;
	gnutls_credentials_type_t client_type;
	gnutls_credentials_type_t server_type;	/* The type of credentials a server
						 * needs to set */
} gnutls_cred_map;

static const gnutls_cred_map cred_mappings[] = {
	{GNUTLS_KX_ANON_DH, GNUTLS_CRD_ANON, GNUTLS_CRD_ANON},
	{GNUTLS_KX_ANON_ECDH, GNUTLS_CRD_ANON, GNUTLS_CRD_ANON},
	{GNUTLS_KX_RSA, GNUTLS_CRD_CERTIFICATE, GNUTLS_CRD_CERTIFICATE},
	{GNUTLS_KX_ECDHE_RSA, GNUTLS_CRD_CERTIFICATE,
	 GNUTLS_CRD_CERTIFICATE},
	{GNUTLS_KX_ECDHE_ECDSA, GNUTLS_CRD_CERTIFICATE,
	 GNUTLS_CRD_CERTIFICATE},
	{GNUTLS_KX_DHE_DSS, GNUTLS_CRD_CERTIFICATE,
	 GNUTLS_CRD_CERTIFICATE},
	{GNUTLS_KX_DHE_RSA, GNUTLS_CRD_CERTIFICATE,
	 GNUTLS_CRD_CERTIFICATE},
	{GNUTLS_KX_PSK, GNUTLS_CRD_PSK, GNUTLS_CRD_PSK},
	{GNUTLS_KX_DHE_PSK, GNUTLS_CRD_PSK, GNUTLS_CRD_PSK},
	{GNUTLS_KX_RSA_PSK, GNUTLS_CRD_PSK, GNUTLS_CRD_CERTIFICATE},
	{GNUTLS_KX_ECDHE_PSK, GNUTLS_CRD_PSK, GNUTLS_CRD_PSK},
	{GNUTLS_KX_SRP, GNUTLS_CRD_SRP, GNUTLS_CRD_SRP},
	{GNUTLS_KX_SRP_RSA, GNUTLS_CRD_SRP, GNUTLS_CRD_CERTIFICATE},
	{GNUTLS_KX_SRP_DSS, GNUTLS_CRD_SRP, GNUTLS_CRD_CERTIFICATE},
	{0, 0, 0}
};

#define GNUTLS_KX_MAP_LOOP(b) \
        const gnutls_cred_map *p; \
                for(p = cred_mappings; p->algorithm != 0; p++) { b ; }

#define GNUTLS_KX_MAP_ALG_LOOP_SERVER(a) \
                        GNUTLS_KX_MAP_LOOP( if(p->server_type == type) { a; break; })

struct gnutls_kx_algo_entry {
	const char *name;
	gnutls_kx_algorithm_t algorithm;
	mod_auth_st *auth_struct;
	int needs_dh_params;
	gnutls_pk_algorithm_t cert_pk; /* the type of certificate required */
};
typedef struct gnutls_kx_algo_entry gnutls_kx_algo_entry;

static const gnutls_kx_algo_entry _gnutls_kx_algorithms[] = {
#if defined(ENABLE_ANON) && defined(ENABLE_DHE)
	{"ANON-DH", GNUTLS_KX_ANON_DH, &anon_auth_struct, 1, GNUTLS_PK_UNKNOWN},
#endif
#if defined(ENABLE_ANON) && defined(ENABLE_ECDHE)
	{"ANON-ECDH", GNUTLS_KX_ANON_ECDH, &anon_ecdh_auth_struct, 0, GNUTLS_PK_UNKNOWN},
#endif
	{"RSA", GNUTLS_KX_RSA, &rsa_auth_struct, 0, GNUTLS_PK_RSA},
#ifdef ENABLE_DHE
	{"DHE-RSA", GNUTLS_KX_DHE_RSA, &dhe_rsa_auth_struct, 1, GNUTLS_PK_RSA},
	{"DHE-DSS", GNUTLS_KX_DHE_DSS, &dhe_dss_auth_struct, 1, GNUTLS_PK_DSA},
#endif
#ifdef ENABLE_ECDHE
	{"ECDHE-RSA", GNUTLS_KX_ECDHE_RSA, &ecdhe_rsa_auth_struct, 0, GNUTLS_PK_RSA},
	{"ECDHE-ECDSA", GNUTLS_KX_ECDHE_ECDSA, &ecdhe_ecdsa_auth_struct,
	 0, GNUTLS_PK_EC},
#endif
#ifdef ENABLE_SRP
	{"SRP-DSS", GNUTLS_KX_SRP_DSS, &srp_dss_auth_struct, 0, GNUTLS_PK_DSA},
	{"SRP-RSA", GNUTLS_KX_SRP_RSA, &srp_rsa_auth_struct, 0, GNUTLS_PK_RSA},
	{"SRP", GNUTLS_KX_SRP, &srp_auth_struct, 0, GNUTLS_PK_UNKNOWN},
#endif
#ifdef ENABLE_PSK
	{"PSK", GNUTLS_KX_PSK, &psk_auth_struct, 0, GNUTLS_PK_UNKNOWN},
	{"RSA-PSK", GNUTLS_KX_RSA_PSK, &rsa_psk_auth_struct, 0, GNUTLS_PK_RSA},
#ifdef ENABLE_DHE
	{"DHE-PSK", GNUTLS_KX_DHE_PSK, &dhe_psk_auth_struct,
	 1 /* needs DHE params */, GNUTLS_PK_UNKNOWN},
#endif
#ifdef ENABLE_ECDHE
	{"ECDHE-PSK", GNUTLS_KX_ECDHE_PSK, &ecdhe_psk_auth_struct, 0, GNUTLS_PK_UNKNOWN},
#endif
#endif
	{0, 0, 0, 0, 0}
};

#define GNUTLS_KX_LOOP(b) \
        const gnutls_kx_algo_entry *p; \
                for(p = _gnutls_kx_algorithms; p->name != NULL; p++) { b ; }

#define GNUTLS_KX_ALG_LOOP(a) \
                        GNUTLS_KX_LOOP( if(p->algorithm == algorithm) { a; break; } )


/* Key EXCHANGE functions */
mod_auth_st *_gnutls_kx_auth_struct(gnutls_kx_algorithm_t algorithm)
{
	mod_auth_st *ret = NULL;
	GNUTLS_KX_ALG_LOOP(ret = p->auth_struct);
	return ret;

}

int
_gnutls_kx_priority(gnutls_session_t session,
		    gnutls_kx_algorithm_t algorithm)
{
	unsigned int i;
	for (i = 0; i < session->internals.priorities.kx.algorithms; i++) {
		if (session->internals.priorities.kx.priority[i] ==
		    algorithm)
			return i;
	}
	return -1;
}

/**
 * gnutls_kx_get_name:
 * @algorithm: is a key exchange algorithm
 *
 * Convert a #gnutls_kx_algorithm_t value to a string.
 *
 * Returns: a pointer to a string that contains the name of the
 *   specified key exchange algorithm, or %NULL.
 **/
const char *gnutls_kx_get_name(gnutls_kx_algorithm_t algorithm)
{
	const char *ret = NULL;

	/* avoid prefix */
	GNUTLS_KX_ALG_LOOP(ret = p->name);

	return ret;
}

/**
 * gnutls_kx_get_id:
 * @name: is a KX name
 *
 * Convert a string to a #gnutls_kx_algorithm_t value.  The names are
 * compared in a case insensitive way.
 *
 * Returns: an id of the specified KX algorithm, or %GNUTLS_KX_UNKNOWN
 *   on error.
 **/
gnutls_kx_algorithm_t gnutls_kx_get_id(const char *name)
{
	gnutls_kx_algorithm_t ret = GNUTLS_KX_UNKNOWN;

	GNUTLS_KX_LOOP(
		if (strcasecmp(p->name, name) == 0) {
			ret = p->algorithm;
			break;
		}
	);

	return ret;
}

/**
 * gnutls_kx_list:
 *
 * Get a list of supported key exchange algorithms.
 *
 * This function is not thread safe.
 *
 * Returns: a (0)-terminated list of #gnutls_kx_algorithm_t integers
 * indicating the available key exchange algorithms.
 **/
const gnutls_kx_algorithm_t *gnutls_kx_list(void)
{
	static gnutls_kx_algorithm_t supported_kxs[MAX_ALGOS] = { 0 };

	if (supported_kxs[0] == 0) {
		int i = 0;

		GNUTLS_KX_LOOP(supported_kxs[i++] = p->algorithm);
		supported_kxs[i++] = 0;
	}

	return supported_kxs;
}

int _gnutls_kx_is_ok(gnutls_kx_algorithm_t algorithm)
{
	ssize_t ret = -1;
	GNUTLS_KX_ALG_LOOP(ret = p->algorithm);
	if (ret >= 0)
		ret = 0;
	else
		ret = 1;
	return ret;
}

int _gnutls_kx_needs_dh_params(gnutls_kx_algorithm_t algorithm)
{
	ssize_t ret = 0;
	GNUTLS_KX_ALG_LOOP(ret = p->needs_dh_params);
	return ret;
}

int _gnutls_kx_cert_pk_params(gnutls_kx_algorithm_t algorithm)
{
	ssize_t ret = 0;
	GNUTLS_KX_ALG_LOOP(ret = p->cert_pk);
	return ret;
}

/* Type to KX mappings */
gnutls_kx_algorithm_t
_gnutls_map_kx_get_kx(gnutls_credentials_type_t type, int server)
{
	gnutls_kx_algorithm_t ret = -1;

	if (server) {
		GNUTLS_KX_MAP_ALG_LOOP_SERVER(ret = p->algorithm);
	} else {
		GNUTLS_KX_MAP_ALG_LOOP_SERVER(ret = p->algorithm);
	}
	return ret;
}

/* Returns the credentials type required for this
 * Key exchange method.
 */
gnutls_credentials_type_t
_gnutls_map_kx_get_cred(gnutls_kx_algorithm_t algorithm, int server)
{
	gnutls_credentials_type_t ret = -1;
	if (server) {
		GNUTLS_KX_MAP_LOOP(if (p->algorithm == algorithm) ret =
				   p->server_type);
	} else {
		GNUTLS_KX_MAP_LOOP(if (p->algorithm == algorithm) ret =
				   p->client_type);
	}

	return ret;
}
