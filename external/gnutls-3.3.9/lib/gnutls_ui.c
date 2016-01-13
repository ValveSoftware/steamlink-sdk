/*
 * Copyright (C) 2001-2012 Free Software Foundation, Inc.
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

/* This file contains certificate authentication functions to be exported in the
 * API which did not fit elsewhere.
 */

#include <gnutls_int.h>
#include <auth/srp.h>
#include <auth/anon.h>
#include <auth/cert.h>
#include <auth/psk.h>
#include <gnutls_errors.h>
#include <gnutls_auth.h>
#include <gnutls_state.h>
#include <gnutls_datum.h>
#include <extras/randomart.h>
#include <read-file.h>
#include <algorithms.h>

/**
 * gnutls_random_art:
 * @type: The type of the random art (for now only %GNUTLS_RANDOM_ART_OPENSSH is supported)
 * @key_type: The type of the key (RSA, DSA etc.)
 * @key_size: The size of the key in bits
 * @fpr: The fingerprint of the key
 * @fpr_size: The size of the fingerprint
 * @art: The returned random art
 *
 * This function will convert a given fingerprint to an "artistic"
 * image. The returned image is allocated using gnutls_malloc().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 *
 **/
int gnutls_random_art(gnutls_random_art_t type,
		      const char *key_type, unsigned int key_size,
		      void *fpr, size_t fpr_size, gnutls_datum_t * art)
{
	if (type != GNUTLS_RANDOM_ART_OPENSSH)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	art->data =
	    (void *) _gnutls_key_fingerprint_randomart(fpr, fpr_size,
						       key_type, key_size,
						       NULL);
	if (art->data == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	art->size = strlen((char *) art->data);

	return 0;
}

/* ANON & DHE */

#if defined(ENABLE_DHE) || defined(ENABLE_ANON)
/**
 * gnutls_dh_set_prime_bits:
 * @session: is a #gnutls_session_t structure.
 * @bits: is the number of bits
 *
 * This function sets the number of bits, for use in a Diffie-Hellman
 * key exchange.  This is used both in DH ephemeral and DH anonymous
 * cipher suites.  This will set the minimum size of the prime that
 * will be used for the handshake.
 *
 * In the client side it sets the minimum accepted number of bits.  If
 * a server sends a prime with less bits than that
 * %GNUTLS_E_DH_PRIME_UNACCEPTABLE will be returned by the handshake.
 *
 * Note that values lower than 512 bits may allow decryption of the
 * exchanged data.
 *
 * The function has no effect in server side.
 * 
 * Note that since 3.1.7 this function is deprecated. The minimum
 * number of bits is set by the priority string level.
 * Also this function must be called after gnutls_priority_set_direct()
 * or the set value may be overridden by the selected priority options.
 *
 *
 **/
void gnutls_dh_set_prime_bits(gnutls_session_t session, unsigned int bits)
{
	if (bits <= gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_VERY_WEAK)
		&& bits != 0)
		_gnutls_audit_log(session,
				  "Note that the security level of the Diffie-Hellman key exchange has been lowered to %u bits and this may allow decryption of the session data\n",
				  bits);
	session->internals.priorities.dh_prime_bits = bits;
}


/**
 * gnutls_dh_get_group:
 * @session: is a gnutls session
 * @raw_gen: will hold the generator.
 * @raw_prime: will hold the prime.
 *
 * This function will return the group parameters used in the last
 * Diffie-Hellman key exchange with the peer.  These are the prime and
 * the generator used.  This function should be used for both
 * anonymous and ephemeral Diffie-Hellman.  The output parameters must
 * be freed with gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_dh_get_group(gnutls_session_t session,
		    gnutls_datum_t * raw_gen, gnutls_datum_t * raw_prime)
{
	dh_info_st *dh;
	int ret;
	anon_auth_info_t anon_info;
	cert_auth_info_t cert_info;
	psk_auth_info_t psk_info;

	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		anon_info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
		if (anon_info == NULL)
			return GNUTLS_E_INTERNAL_ERROR;
		dh = &anon_info->dh;
		break;
	case GNUTLS_CRD_PSK:
		psk_info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
		if (psk_info == NULL)
			return GNUTLS_E_INTERNAL_ERROR;
		dh = &psk_info->dh;
		break;
	case GNUTLS_CRD_CERTIFICATE:
		cert_info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
		if (cert_info == NULL)
			return GNUTLS_E_INTERNAL_ERROR;
		dh = &cert_info->dh;
		break;
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	ret = _gnutls_set_datum(raw_prime, dh->prime.data, dh->prime.size);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret =
	    _gnutls_set_datum(raw_gen, dh->generator.data,
			      dh->generator.size);
	if (ret < 0) {
		gnutls_assert();
		_gnutls_free_datum(raw_prime);
		return ret;
	}

	return 0;
}

/**
 * gnutls_dh_get_pubkey:
 * @session: is a gnutls session
 * @raw_key: will hold the public key.
 *
 * This function will return the peer's public key used in the last
 * Diffie-Hellman key exchange.  This function should be used for both
 * anonymous and ephemeral Diffie-Hellman.  The output parameters must
 * be freed with gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_dh_get_pubkey(gnutls_session_t session, gnutls_datum_t * raw_key)
{
	dh_info_st *dh;
	anon_auth_info_t anon_info;
	cert_auth_info_t cert_info;
	cert_auth_info_t psk_info;

	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		{
			anon_info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
			if (anon_info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			dh = &anon_info->dh;
			break;
		}
	case GNUTLS_CRD_PSK:
		{
			psk_info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
			if (psk_info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			dh = &psk_info->dh;
			break;
		}
	case GNUTLS_CRD_CERTIFICATE:
		{

			cert_info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
			if (cert_info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			dh = &cert_info->dh;
			break;
		}
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	return _gnutls_set_datum(raw_key, dh->public_key.data,
				 dh->public_key.size);
}

/**
 * gnutls_dh_get_secret_bits:
 * @session: is a gnutls session
 *
 * This function will return the bits used in the last Diffie-Hellman
 * key exchange with the peer.  Should be used for both anonymous and
 * ephemeral Diffie-Hellman.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int gnutls_dh_get_secret_bits(gnutls_session_t session)
{
	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		{
			anon_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			return info->dh.secret_bits;
		}
	case GNUTLS_CRD_PSK:
		{
			psk_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			return info->dh.secret_bits;
		}
	case GNUTLS_CRD_CERTIFICATE:
		{
			cert_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			return info->dh.secret_bits;
		}
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}
}


static int mpi_buf2bits(gnutls_datum_t * mpi_buf)
{
	bigint_t mpi;
	int rc;

	rc = _gnutls_mpi_init_scan_nz(&mpi, mpi_buf->data, mpi_buf->size);
	if (rc) {
		gnutls_assert();
		return rc;
	}

	rc = _gnutls_mpi_get_nbits(mpi);
	_gnutls_mpi_release(&mpi);

	return rc;
}

/**
 * gnutls_dh_get_prime_bits:
 * @session: is a gnutls session
 *
 * This function will return the bits of the prime used in the last
 * Diffie-Hellman key exchange with the peer.  Should be used for both
 * anonymous and ephemeral Diffie-Hellman.  Note that some ciphers,
 * like RSA and DSA without DHE, do not use a Diffie-Hellman key
 * exchange, and then this function will return 0.
 *
 * Returns: The Diffie-Hellman bit strength is returned, or 0 if no
 *   Diffie-Hellman key exchange was done, or a negative error code on
 *   failure.
 **/
int gnutls_dh_get_prime_bits(gnutls_session_t session)
{
	dh_info_st *dh;

	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		{
			anon_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_PSK:
		{
			psk_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;
			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_CERTIFICATE:
		{
			cert_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	return mpi_buf2bits(&dh->prime);
}


/**
 * gnutls_dh_get_peers_public_bits:
 * @session: is a gnutls session
 *
 * Get the Diffie-Hellman public key bit size.  Can be used for both
 * anonymous and ephemeral Diffie-Hellman.
 *
 * Returns: The public key bit size used in the last Diffie-Hellman
 *   key exchange with the peer, or a negative error code in case of error.
 **/
int gnutls_dh_get_peers_public_bits(gnutls_session_t session)
{
	dh_info_st *dh;

	switch (gnutls_auth_get_type(session)) {
	case GNUTLS_CRD_ANON:
		{
			anon_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_ANON);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_PSK:
		{
			psk_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	case GNUTLS_CRD_CERTIFICATE:
		{
			cert_auth_info_t info;

			info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
			if (info == NULL)
				return GNUTLS_E_INTERNAL_ERROR;

			dh = &info->dh;
			break;
		}
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	return mpi_buf2bits(&dh->public_key);
}

/**
 * gnutls_certificate_set_dh_params:
 * @res: is a gnutls_certificate_credentials_t structure
 * @dh_params: is a structure that holds Diffie-Hellman parameters.
 *
 * This function will set the Diffie-Hellman parameters for a
 * certificate server to use. These parameters will be used in
 * Ephemeral Diffie-Hellman cipher suites.  Note that only a pointer
 * to the parameters are stored in the certificate handle, so you
 * must not deallocate the parameters before the certificate is deallocated.
 *
 **/
void
gnutls_certificate_set_dh_params(gnutls_certificate_credentials_t res,
				 gnutls_dh_params_t dh_params)
{
	res->dh_params = dh_params;
}

#endif				/* DH */

/* CERTIFICATE STUFF */

/**
 * gnutls_certificate_get_ours:
 * @session: is a gnutls session
 *
 * Gets the certificate as sent to the peer in the last handshake.
 * The certificate is in raw (DER) format.  No certificate
 * list is being returned. Only the first certificate.
 *
 * Returns: a pointer to a #gnutls_datum_t containing our
 *   certificate, or %NULL in case of an error or if no certificate
 *   was used.
 **/
const gnutls_datum_t *gnutls_certificate_get_ours(gnutls_session_t session)
{
	gnutls_certificate_credentials_t cred;

	CHECK_AUTH(GNUTLS_CRD_CERTIFICATE, NULL);

	cred = (gnutls_certificate_credentials_t)
	    _gnutls_get_cred(session, GNUTLS_CRD_CERTIFICATE);
	if (cred == NULL || cred->certs == NULL) {
		gnutls_assert();
		return NULL;
	}

	if (session->internals.selected_cert_list == NULL)
		return NULL;

	return &session->internals.selected_cert_list[0].cert;
}

/**
 * gnutls_certificate_get_peers:
 * @session: is a gnutls session
 * @list_size: is the length of the certificate list (may be %NULL)
 *
 * Get the peer's raw certificate (chain) as sent by the peer.  These
 * certificates are in raw format (DER encoded for X.509).  In case of
 * a X.509 then a certificate list may be present.  The first
 * certificate in the list is the peer's certificate, following the
 * issuer's certificate, then the issuer's issuer etc.
 *
 * In case of OpenPGP keys a single key will be returned in raw
 * format.
 *
 * Returns: a pointer to a #gnutls_datum_t containing the peer's
 *   certificates, or %NULL in case of an error or if no certificate
 *   was used.
 **/
const gnutls_datum_t *gnutls_certificate_get_peers(gnutls_session_t
						   session,
						   unsigned int *list_size)
{
	cert_auth_info_t info;

	CHECK_AUTH(GNUTLS_CRD_CERTIFICATE, NULL);

	info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
	if (info == NULL)
		return NULL;

	if (list_size)
		*list_size = info->ncerts;
	return info->raw_certificate_list;
}

#ifdef ENABLE_OPENPGP
/**
 * gnutls_certificate_get_peers_subkey_id:
 * @session: is a gnutls session
 * @id: will contain the ID
 *
 * Get the peer's subkey ID when OpenPGP certificates are
 * used. The returned @id should be treated as constant.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 *
 * Since: 3.1.3
 **/
int gnutls_certificate_get_peers_subkey_id(gnutls_session_t session,
					   gnutls_datum_t * id)
{
	cert_auth_info_t info;

	CHECK_AUTH(GNUTLS_CRD_CERTIFICATE, GNUTLS_E_INVALID_REQUEST);

	info = _gnutls_get_auth_info(session, GNUTLS_CRD_CERTIFICATE);
	if (info == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	id->data = info->subkey_id;
	id->size = GNUTLS_OPENPGP_KEYID_SIZE;

	return 0;
}
#endif

/**
 * gnutls_certificate_client_get_request_status:
 * @session: is a gnutls session
 *
 * Get whether client certificate is requested or not.
 *
 * Returns: 0 if the peer (server) did not request client
 *   authentication or 1 otherwise.
 **/
int gnutls_certificate_client_get_request_status(gnutls_session_t session)
{
	return session->key.crt_requested;
}

/**
 * gnutls_fingerprint:
 * @algo: is a digest algorithm
 * @data: is the data
 * @result: is the place where the result will be copied (may be null).
 * @result_size: should hold the size of the result. The actual size
 * of the returned result will also be copied there.
 *
 * This function will calculate a fingerprint (actually a hash), of
 * the given data.  The result is not printable data.  You should
 * convert it to hex, or to something else printable.
 *
 * This is the usual way to calculate a fingerprint of an X.509 DER
 * encoded certificate.  Note however that the fingerprint of an
 * OpenPGP certificate is not just a hash and cannot be calculated with this
 * function.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_fingerprint(gnutls_digest_algorithm_t algo,
		   const gnutls_datum_t * data, void *result,
		   size_t * result_size)
{
	int ret;
	int hash_len = _gnutls_hash_get_algo_len(hash_to_entry(algo));

	if (hash_len < 0 || (unsigned) hash_len > *result_size
	    || result == NULL) {
		*result_size = hash_len;
		return GNUTLS_E_SHORT_MEMORY_BUFFER;
	}
	*result_size = hash_len;

	if (result) {
		ret =
		    _gnutls_hash_fast(algo, data->data, data->size,
				      result);
		if (ret < 0)
			return gnutls_assert_val(ret);
	}

	return 0;
}

/**
 * gnutls_certificate_set_params_function:
 * @res: is a gnutls_certificate_credentials_t structure
 * @func: is the function to be called
 *
 * This function will set a callback in order for the server to get
 * the Diffie-Hellman or RSA parameters for certificate
 * authentication.  The callback should return %GNUTLS_E_SUCCESS (0) on success.
 **/
void
gnutls_certificate_set_params_function(gnutls_certificate_credentials_t
				       res, gnutls_params_function * func)
{
	res->params_func = func;
}

/**
 * gnutls_certificate_set_verify_flags:
 * @res: is a gnutls_certificate_credentials_t structure
 * @flags: are the flags
 *
 * This function will set the flags to be used for verification 
 * of certificates and override any defaults.  The provided flags must be an OR of the
 * #gnutls_certificate_verify_flags enumerations. 
 *
 **/
void
gnutls_certificate_set_verify_flags(gnutls_certificate_credentials_t
				    res, unsigned int flags)
{
	res->verify_flags = flags;
}

/**
 * gnutls_certificate_set_verify_limits:
 * @res: is a gnutls_certificate_credentials structure
 * @max_bits: is the number of bits of an acceptable certificate (default 8200)
 * @max_depth: is maximum depth of the verification of a certificate chain (default 5)
 *
 * This function will set some upper limits for the default
 * verification function, gnutls_certificate_verify_peers2(), to avoid
 * denial of service attacks.  You can set them to zero to disable
 * limits.
 **/
void
gnutls_certificate_set_verify_limits(gnutls_certificate_credentials_t res,
				     unsigned int max_bits,
				     unsigned int max_depth)
{
	res->verify_depth = max_depth;
	res->verify_bits = max_bits;
}

#ifdef ENABLE_PSK
/**
 * gnutls_psk_set_params_function:
 * @res: is a gnutls_psk_server_credentials_t structure
 * @func: is the function to be called
 *
 * This function will set a callback in order for the server to get
 * the Diffie-Hellman or RSA parameters for PSK authentication.  The
 * callback should return %GNUTLS_E_SUCCESS (0) on success.
 **/
void
gnutls_psk_set_params_function(gnutls_psk_server_credentials_t res,
			       gnutls_params_function * func)
{
	res->params_func = func;
}
#endif

#ifdef ENABLE_ANON
/**
 * gnutls_anon_set_params_function:
 * @res: is a gnutls_anon_server_credentials_t structure
 * @func: is the function to be called
 *
 * This function will set a callback in order for the server to get
 * the Diffie-Hellman or RSA parameters for anonymous authentication.
 * The callback should return %GNUTLS_E_SUCCESS (0) on success.
 **/
void
gnutls_anon_set_params_function(gnutls_anon_server_credentials_t res,
				gnutls_params_function * func)
{
	res->params_func = func;
}
#endif

/**
 * gnutls_load_file:
 * @filename: the name of the file to load
 * @data: Where the file will be stored
 *
 * This function will load a file into a datum. The data are
 * zero terminated but the terminating null is not included in length.
 * The returned data are allocated using gnutls_malloc().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 *
 * Since 3.1.0
 **/
int gnutls_load_file(const char *filename, gnutls_datum_t * data)
{
	size_t len;

	data->data = (void *) read_binary_file(filename, &len);
	if (data->data == NULL)
		return GNUTLS_E_FILE_ERROR;

	if (malloc != gnutls_malloc) {
		void *tmp = gnutls_malloc(len);

		memcpy(tmp, data->data, len);
		free(data->data);
		data->data = tmp;
	}

	data->size = len;

	return 0;
}

/**
 * gnutls_ocsp_status_request_is_checked:
 * @session: is a gnutls session
 * @flags: should be zero
 *
 * Check whether an OCSP status response was included in the handshake
 * and whether it was checked and valid (not too old or superseded). 
 * This is a helper function when needing to decide whether to perform an
 * OCSP validity check on the peer's certificate. Must be called after
 * gnutls_certificate_verify_peers3() is called.
 *
 * Returns: non zero it was valid, or a zero if it wasn't sent,
 * or sent and was invalid.
 **/
int
gnutls_ocsp_status_request_is_checked(gnutls_session_t session,
				      unsigned int flags)
{
	return session->internals.ocsp_check_ok;
}

#ifdef ENABLE_RSA_EXPORT

/**
 * gnutls_rsa_export_get_pubkey:
 * @session: is a gnutls session
 * @exponent: will hold the exponent.
 * @modulus: will hold the modulus.
 *
 * This function will return the peer's public key exponent and
 * modulus used in the last RSA-EXPORT authentication.  The output
 * parameters must be freed with gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_rsa_export_get_pubkey(gnutls_session_t session,
			     gnutls_datum_t * exponent,
			     gnutls_datum_t * modulus)
{
	return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
}

/**
 * gnutls_rsa_export_get_modulus_bits:
 * @session: is a gnutls session
 *
 * Get the export RSA parameter's modulus size.
 *
 * Returns: The bits used in the last RSA-EXPORT key exchange with the
 *   peer, or a negative error code in case of error.
 **/
int gnutls_rsa_export_get_modulus_bits(gnutls_session_t session)
{
	return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
}

/**
 * gnutls_certificate_set_rsa_export_params:
 * @res: is a gnutls_certificate_credentials_t structure
 * @rsa_params: is a structure that holds temporary RSA parameters.
 *
 * This function will set the temporary RSA parameters for a
 * certificate server to use.  These parameters will be used in
 * RSA-EXPORT cipher suites.
 **/
void
gnutls_certificate_set_rsa_export_params(gnutls_certificate_credentials_t
					 res,
					 gnutls_rsa_params_t rsa_params)
{
	return;
}
#endif

#define DESC_SIZE 64

/**
 * gnutls_session_get_desc:
 * @session: is a gnutls session
 *
 * This function returns a string describing the current session.
 * The string is null terminated and allocated using gnutls_malloc().
 *
 * Returns: a description of the protocols and algorithms in the current session.
 *
 * Since: 3.1.10
 **/
char *gnutls_session_get_desc(gnutls_session_t session)
{
	gnutls_kx_algorithm_t kx;
	unsigned type;
	char kx_name[32];
	char proto_name[32];
	const char *curve_name = NULL;
	unsigned dh_bits = 0;
	unsigned mac_id;
	char *desc;

	kx = session->security_parameters.kx_algorithm;

	if (kx == GNUTLS_KX_ANON_ECDH || kx == GNUTLS_KX_ECDHE_PSK ||
	    kx == GNUTLS_KX_ECDHE_RSA || kx == GNUTLS_KX_ECDHE_ECDSA) {
		curve_name =
		    gnutls_ecc_curve_get_name(gnutls_ecc_curve_get
					      (session));
	} else if (kx == GNUTLS_KX_ANON_DH || kx == GNUTLS_KX_DHE_PSK
		   || kx == GNUTLS_KX_DHE_RSA || kx == GNUTLS_KX_DHE_DSS) {
		dh_bits = gnutls_dh_get_prime_bits(session);
	}

	if (curve_name != NULL)
		snprintf(kx_name, sizeof(kx_name), "%s-%s",
			 gnutls_kx_get_name(kx), curve_name);
	else if (dh_bits != 0)
		snprintf(kx_name, sizeof(kx_name), "%s-%u",
			 gnutls_kx_get_name(kx), dh_bits);
	else
		snprintf(kx_name, sizeof(kx_name), "%s",
			 gnutls_kx_get_name(kx));

	type = gnutls_certificate_type_get(session);
	if (type == GNUTLS_CRT_X509)
		snprintf(proto_name, sizeof(proto_name), "%s",
			 gnutls_protocol_get_name(get_num_version
						  (session)));
	else
		snprintf(proto_name, sizeof(proto_name), "%s-%s",
			 gnutls_protocol_get_name(get_num_version
						  (session)),
			 gnutls_certificate_type_get_name(type));

	gnutls_protocol_get_name(get_num_version(session)),
	    desc = gnutls_malloc(DESC_SIZE);
	if (desc == NULL)
		return NULL;

	mac_id = gnutls_mac_get(session);
	if (mac_id == GNUTLS_MAC_AEAD) { /* no need to print */
		snprintf(desc, DESC_SIZE,
			 "(%s)-(%s)-(%s)",
			 proto_name,
			 kx_name,
			 gnutls_cipher_get_name(gnutls_cipher_get(session)));
	} else {
		snprintf(desc, DESC_SIZE,
			 "(%s)-(%s)-(%s)-(%s)",
			 proto_name,
			 kx_name,
			 gnutls_cipher_get_name(gnutls_cipher_get(session)),
			 gnutls_mac_get_name(mac_id));
	}

	return desc;
}

/**
 * gnutls_session_set_id:
 * @session: is a #gnutls_session_t structure.
 * @sid: the session identifier
 *
 * This function sets the session ID to be used in a client hello.
 * This is a function intended for exceptional uses. Do not use this
 * function unless you are implementing a custom protocol.
 *
 * To set session resumption parameters use gnutls_session_set_data() instead.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 **/
int
gnutls_session_set_id(gnutls_session_t session, const gnutls_datum_t * sid)
{
	if (session->security_parameters.entity == GNUTLS_SERVER ||
	    sid->size > TLS_MAX_SESSION_ID_SIZE)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	memset(&session->internals.resumed_security_parameters, 0,
	       sizeof(session->internals.resumed_security_parameters));

	session->internals.resumed_security_parameters.session_id_size =
	    sid->size;
	memcpy(session->internals.resumed_security_parameters.session_id,
	       sid->data, sid->size);

	return 0;
}
