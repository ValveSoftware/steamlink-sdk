/*
 * Copyright (C) 2002-2012 Free Software Foundation, Inc.
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

/* This file contains the code the Certificate Type TLS extension.
 * This extension is currently gnutls specific.
 */

#include "gnutls_int.h"
#include "gnutls_errors.h"
#include "gnutls_num.h"
#include <gnutls/gnutls.h>
#include <ext/signature.h>
#include <gnutls_state.h>
#include <gnutls_num.h>
#include <algorithms.h>
#include <abstract_int.h>

static int _gnutls_signature_algorithm_recv_params(gnutls_session_t
						   session,
						   const uint8_t * data,
						   size_t data_size);
static int _gnutls_signature_algorithm_send_params(gnutls_session_t
						   session,
						   gnutls_buffer_st *
						   extdata);
static void signature_algorithms_deinit_data(extension_priv_data_t priv);
static int signature_algorithms_pack(extension_priv_data_t epriv,
				     gnutls_buffer_st * ps);
static int signature_algorithms_unpack(gnutls_buffer_st * ps,
				       extension_priv_data_t * _priv);

extension_entry_st ext_mod_sig = {
	.name = "SIGNATURE ALGORITHMS",
	.type = GNUTLS_EXTENSION_SIGNATURE_ALGORITHMS,
	.parse_type = GNUTLS_EXT_TLS,

	.recv_func = _gnutls_signature_algorithm_recv_params,
	.send_func = _gnutls_signature_algorithm_send_params,
	.pack_func = signature_algorithms_pack,
	.unpack_func = signature_algorithms_unpack,
	.deinit_func = signature_algorithms_deinit_data,
};

typedef struct {
	/* TLS 1.2 signature algorithms */
	gnutls_sign_algorithm_t sign_algorithms[MAX_SIGNATURE_ALGORITHMS];
	uint16_t sign_algorithms_size;
} sig_ext_st;

/* generates a SignatureAndHashAlgorithm structure with length as prefix
 * by using the setup priorities.
 */
int
_gnutls_sign_algorithm_write_params(gnutls_session_t session,
				    uint8_t * data, size_t max_data_size)
{
	uint8_t *p = data, *len_p;
	unsigned int len, i, j;
	const sign_algorithm_st *aid;

	if (max_data_size <
	    (session->internals.priorities.sign_algo.algorithms * 2) + 2) {
		gnutls_assert();
		return GNUTLS_E_SHORT_MEMORY_BUFFER;
	}

	len = 0;
	len_p = p;

	p += 2;

	for (i = j = 0;
	     j < session->internals.priorities.sign_algo.algorithms;
	     i += 2, j++) {
		aid =
		    _gnutls_sign_to_tls_aid(session->internals.
					    priorities.sign_algo.
					    priority[j]);

		if (aid == NULL)
			continue;

		_gnutls_handshake_log
		    ("EXT[%p]: sent signature algo (%d.%d) %s\n", session,
		     aid->hash_algorithm, aid->sign_algorithm,
		     gnutls_sign_get_name(session->internals.priorities.
					  sign_algo.priority[j]));
		*p = aid->hash_algorithm;
		p++;
		*p = aid->sign_algorithm;
		p++;
		len += 2;
	}

	_gnutls_write_uint16(len, len_p);
	return len + 2;
}


/* Parses the Signature Algorithm structure and stores data into
 * session->security_parameters.extensions.
 */
int
_gnutls_sign_algorithm_parse_data(gnutls_session_t session,
				  const uint8_t * data, size_t data_size)
{
	unsigned int sig, i;
	sig_ext_st *priv;
	extension_priv_data_t epriv;

	if (data_size % 2 != 0)
		return gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);

	priv = gnutls_calloc(1, sizeof(*priv));
	if (priv == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	for (i = 0; i < data_size; i += 2) {
		sign_algorithm_st aid;

		aid.hash_algorithm = data[i];
		aid.sign_algorithm = data[i + 1];

		sig = _gnutls_tls_aid_to_sign(&aid);

		_gnutls_handshake_log
		    ("EXT[%p]: rcvd signature algo (%d.%d) %s\n", session,
		     aid.hash_algorithm, aid.sign_algorithm,
		     gnutls_sign_get_name(sig));

		if (sig != GNUTLS_SIGN_UNKNOWN) {
			priv->sign_algorithms[priv->
					      sign_algorithms_size++] =
			    sig;
			if (priv->sign_algorithms_size ==
			    MAX_SIGNATURE_ALGORITHMS)
				break;
		}
	}

	epriv.ptr = priv;
	_gnutls_ext_set_session_data(session,
				     GNUTLS_EXTENSION_SIGNATURE_ALGORITHMS,
				     epriv);

	return 0;
}

/*
 * In case of a server: if a SIGNATURE_ALGORITHMS extension type is
 * received then it stores into the session security parameters the
 * new value.
 *
 * In case of a client: If a signature_algorithms have been specified
 * then it is an error;
 */

static int
_gnutls_signature_algorithm_recv_params(gnutls_session_t session,
					const uint8_t * data,
					size_t _data_size)
{
	ssize_t data_size = _data_size;
	int ret;

	if (session->security_parameters.entity == GNUTLS_CLIENT) {
		/* nothing for now */
		gnutls_assert();
		/* Although TLS 1.2 mandates that we must not accept reply
		 * to this message, there are good reasons to just ignore it. Check
		 * http://www.ietf.org/mail-archive/web/tls/current/msg03880.html
		 */
		/* return GNUTLS_E_UNEXPECTED_PACKET; */
	} else {
		/* SERVER SIDE - we must check if the sent cert type is the right one
		 */
		if (data_size > 2) {
			uint16_t len;

			DECR_LEN(data_size, 2);
			len = _gnutls_read_uint16(data);
			DECR_LEN(data_size, len);

			ret =
			    _gnutls_sign_algorithm_parse_data(session,
							      data + 2,
							      len);
			if (ret < 0) {
				gnutls_assert();
				return ret;
			}
		}
	}

	return 0;
}

/* returns data_size or a negative number on failure
 */
static int
_gnutls_signature_algorithm_send_params(gnutls_session_t session,
					gnutls_buffer_st * extdata)
{
	int ret;
	size_t init_length = extdata->length;
	const version_entry_st *ver = get_version(session);

	if (unlikely(ver == NULL))
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

	/* this function sends the client extension data */
	if (session->security_parameters.entity == GNUTLS_CLIENT
	    && _gnutls_version_has_selectable_sighash(ver)) {
		if (session->internals.priorities.sign_algo.algorithms > 0) {
			uint8_t p[MAX_SIGN_ALGO_SIZE];

			ret =
			    _gnutls_sign_algorithm_write_params(session, p,
								sizeof(p));
			if (ret < 0)
				return gnutls_assert_val(ret);

			ret = _gnutls_buffer_append_data(extdata, p, ret);
			if (ret < 0)
				return gnutls_assert_val(ret);

			return extdata->length - init_length;
		}
	}

	/* if we are here it means we don't send the extension */
	return 0;
}

/* Returns a requested by the peer signature algorithm that
 * matches the given certificate's public key algorithm. 
 */
gnutls_sign_algorithm_t
_gnutls_session_get_sign_algo(gnutls_session_t session,
			      gnutls_pcert_st * cert)
{
	unsigned i;
	int ret;
	const version_entry_st *ver = get_version(session);
	sig_ext_st *priv;
	extension_priv_data_t epriv;
	unsigned int cert_algo;

	if (unlikely(ver == NULL))
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

	cert_algo = gnutls_pubkey_get_pk_algorithm(cert->pubkey, NULL);

	ret =
	    _gnutls_ext_get_session_data(session,
					 GNUTLS_EXTENSION_SIGNATURE_ALGORITHMS,
					 &epriv);
	priv = epriv.ptr;

	if (ret < 0 || !_gnutls_version_has_selectable_sighash(ver)
	    || priv->sign_algorithms_size == 0)
		/* none set, allow SHA-1 only */
	{
		return gnutls_pk_to_sign(cert_algo, GNUTLS_DIG_SHA1);
	}

	for (i = 0; i < priv->sign_algorithms_size; i++) {
		if (gnutls_sign_get_pk_algorithm(priv->sign_algorithms[i])
		    == cert_algo) {
			if (_gnutls_pubkey_compatible_with_sig
			    (session, cert->pubkey, ver,
			     priv->sign_algorithms[i]) < 0)
				continue;

			if (_gnutls_session_sign_algo_enabled
			    (session, priv->sign_algorithms[i]) < 0)
				continue;

			return priv->sign_algorithms[i];
		}
	}

	return GNUTLS_SIGN_UNKNOWN;
}

/* Check if the given signature algorithm is supported.
 * This means that it is enabled by the priority functions,
 * and in case of a server a matching certificate exists.
 */
int
_gnutls_session_sign_algo_enabled(gnutls_session_t session,
				  gnutls_sign_algorithm_t sig)
{
	unsigned i;
	int ret;
	const version_entry_st *ver = get_version(session);
	sig_ext_st *priv;
	extension_priv_data_t epriv;

	if (unlikely(ver == NULL))
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

	ret =
	    _gnutls_ext_get_session_data(session,
					 GNUTLS_EXTENSION_SIGNATURE_ALGORITHMS,
					 &epriv);
	if (ret < 0) {
		gnutls_assert();
		return 0;
	}
	priv = epriv.ptr;

	if (!_gnutls_version_has_selectable_sighash(ver)
	    || priv->sign_algorithms_size == 0)
		/* none set, allow all */
	{
		return 0;
	}

	for (i = 0; i < session->internals.priorities.sign_algo.algorithms;
	     i++) {
		if (session->internals.priorities.sign_algo.priority[i] ==
		    sig) {
			return 0;	/* ok */
		}
	}

	return GNUTLS_E_UNSUPPORTED_SIGNATURE_ALGORITHM;
}

static void signature_algorithms_deinit_data(extension_priv_data_t priv)
{
	gnutls_free(priv.ptr);
}

static int
signature_algorithms_pack(extension_priv_data_t epriv,
			  gnutls_buffer_st * ps)
{
	sig_ext_st *priv = epriv.ptr;
	int ret, i;

	BUFFER_APPEND_NUM(ps, priv->sign_algorithms_size);
	for (i = 0; i < priv->sign_algorithms_size; i++) {
		BUFFER_APPEND_NUM(ps, priv->sign_algorithms[i]);
	}
	return 0;
}

static int
signature_algorithms_unpack(gnutls_buffer_st * ps,
			    extension_priv_data_t * _priv)
{
	sig_ext_st *priv;
	int i, ret;
	extension_priv_data_t epriv;

	priv = gnutls_calloc(1, sizeof(*priv));
	if (priv == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	BUFFER_POP_NUM(ps, priv->sign_algorithms_size);
	for (i = 0; i < priv->sign_algorithms_size; i++) {
		BUFFER_POP_NUM(ps, priv->sign_algorithms[i]);
	}

	epriv.ptr = priv;
	*_priv = epriv;

	return 0;

      error:
	gnutls_free(priv);
	return ret;
}



/**
 * gnutls_sign_algorithm_get_requested:
 * @session: is a #gnutls_session_t structure.
 * @indx: is an index of the signature algorithm to return
 * @algo: the returned certificate type will be stored there
 *
 * Returns the signature algorithm specified by index that was
 * requested by the peer. If the specified index has no data available
 * this function returns %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE.  If
 * the negotiated TLS version does not support signature algorithms
 * then %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE will be returned even
 * for the first index.  The first index is 0.
 *
 * This function is useful in the certificate callback functions
 * to assist in selecting the correct certificate.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise
 *   an error code is returned.
 *
 * Since: 2.10.0
 **/
int
gnutls_sign_algorithm_get_requested(gnutls_session_t session,
				    size_t indx,
				    gnutls_sign_algorithm_t * algo)
{
	const version_entry_st *ver = get_version(session);
	sig_ext_st *priv;
	extension_priv_data_t epriv;
	int ret;

	if (unlikely(ver == NULL))
		return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);

	ret =
	    _gnutls_ext_get_session_data(session,
					 GNUTLS_EXTENSION_SIGNATURE_ALGORITHMS,
					 &epriv);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}
	priv = epriv.ptr;

	if (!_gnutls_version_has_selectable_sighash(ver)
	    || priv->sign_algorithms_size == 0) {
		return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
	}

	if (indx < priv->sign_algorithms_size) {
		*algo = priv->sign_algorithms[indx];
		return 0;
	} else
		return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
}

/**
 * gnutls_sign_algorithm_get:
 * @session: is a #gnutls_session_t structure.
 *
 * Returns the signature algorithm that is (or will be) used in this 
 * session by the server to sign data.
 *
 * Returns: The sign algorithm or %GNUTLS_SIGN_UNKNOWN.
 *
 * Since: 3.1.1
 **/
int gnutls_sign_algorithm_get(gnutls_session_t session)
{
	return session->security_parameters.server_sign_algo;
}

/**
 * gnutls_sign_algorithm_get_client:
 * @session: is a #gnutls_session_t structure.
 *
 * Returns the signature algorithm that is (or will be) used in this 
 * session by the client to sign data.
 *
 * Returns: The sign algorithm or %GNUTLS_SIGN_UNKNOWN.
 *
 * Since: 3.1.11
 **/
int gnutls_sign_algorithm_get_client(gnutls_session_t session)
{
	return session->security_parameters.client_sign_algo;
}
