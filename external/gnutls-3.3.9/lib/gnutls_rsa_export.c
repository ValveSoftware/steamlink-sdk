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

/* This file contains code for RSA temporary keys. These keys are
 * only used in export cipher suites.
 */

#include <gnutls_int.h>
#include <gnutls_errors.h>
#include <gnutls_datum.h>
#include "x509/x509_int.h"
#include "debug.h"

#ifdef ENABLE_RSA_EXPORT

/* The are included for binary compatibility with previous versions
 * only */

/**
 * gnutls_rsa_params_import_raw:
 * @rsa_params: Is a structure will hold the parameters
 * @m: holds the modulus
 * @e: holds the public exponent
 * @d: holds the private exponent
 * @p: holds the first prime (p)
 * @q: holds the second prime (q)
 * @u: holds the coefficient
 *
 * This function will replace the parameters in the given structure.
 * The new parameters should be stored in the appropriate
 * gnutls_datum.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an negative error code.
 **/
int
gnutls_rsa_params_import_raw(gnutls_rsa_params_t rsa_params,
			     const gnutls_datum_t * m,
			     const gnutls_datum_t * e,
			     const gnutls_datum_t * d,
			     const gnutls_datum_t * p,
			     const gnutls_datum_t * q,
			     const gnutls_datum_t * u)
{
	return gnutls_x509_privkey_import_rsa_raw(rsa_params, m, e, d, p,
						  q, u);
}

/**
 * gnutls_rsa_params_init:
 * @rsa_params: Is a structure that will hold the parameters
 *
 * This function will initialize the temporary RSA parameters structure.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an negative error code.
 **/
int gnutls_rsa_params_init(gnutls_rsa_params_t * rsa_params)
{
	int ret;

	ret = gnutls_x509_privkey_init(rsa_params);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}

/**
 * gnutls_rsa_params_deinit:
 * @rsa_params: Is a structure that holds the parameters
 *
 * This function will deinitialize the RSA parameters structure.
 **/
void gnutls_rsa_params_deinit(gnutls_rsa_params_t rsa_params)
{
	gnutls_x509_privkey_deinit(rsa_params);
}

/**
 * gnutls_rsa_params_cpy:
 * @dst: Is the destination structure, which should be initialized.
 * @src: Is the source structure
 *
 * This function will copy the RSA parameters structure from source
 * to destination.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an negative error code.
 **/
int gnutls_rsa_params_cpy(gnutls_rsa_params_t dst, gnutls_rsa_params_t src)
{
	return gnutls_x509_privkey_cpy(dst, src);
}

/**
 * gnutls_rsa_params_generate2:
 * @params: The structure where the parameters will be stored
 * @bits: is the prime's number of bits
 *
 * This function will generate new temporary RSA parameters for use in
 * RSA-EXPORT ciphersuites.  This function is normally slow.
 *
 * Note that if the parameters are to be used in export cipher suites the
 * bits value should be 512 or less.
 * Also note that the generation of new RSA parameters is only useful
 * to servers. Clients use the parameters sent by the server, thus it's
 * no use calling this in client side.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an negative error code.
 **/
int
gnutls_rsa_params_generate2(gnutls_rsa_params_t params, unsigned int bits)
{
	return gnutls_x509_privkey_generate(params, GNUTLS_PK_RSA, bits,
					    0);
}

/**
 * gnutls_rsa_params_import_pkcs1:
 * @params: A structure where the parameters will be copied to
 * @pkcs1_params: should contain a PKCS1 RSAPrivateKey structure PEM or DER encoded
 * @format: the format of params. PEM or DER.
 *
 * This function will extract the RSAPrivateKey found in a PKCS1 formatted
 * structure.
 *
 * If the structure is PEM encoded, it should have a header
 * of "BEGIN RSA PRIVATE KEY".
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an negative error code.
 **/
int
gnutls_rsa_params_import_pkcs1(gnutls_rsa_params_t params,
			       const gnutls_datum_t * pkcs1_params,
			       gnutls_x509_crt_fmt_t format)
{
	return gnutls_x509_privkey_import(params, pkcs1_params, format);
}

/**
 * gnutls_rsa_params_export_pkcs1:
 * @params: Holds the RSA parameters
 * @format: the format of output params. One of PEM or DER.
 * @params_data: will contain a PKCS1 RSAPrivateKey structure PEM or DER encoded
 * @params_data_size: holds the size of params_data (and will be replaced by the actual size of parameters)
 *
 * This function will export the given RSA parameters to a PKCS1
 * RSAPrivateKey structure. If the buffer provided is not long enough to
 * hold the output, then GNUTLS_E_SHORT_MEMORY_BUFFER will be returned.
 *
 * If the structure is PEM encoded, it will have a header
 * of "BEGIN RSA PRIVATE KEY".
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an negative error code.
 **/
int
gnutls_rsa_params_export_pkcs1(gnutls_rsa_params_t params,
			       gnutls_x509_crt_fmt_t format,
			       unsigned char *params_data,
			       size_t * params_data_size)
{
	return gnutls_x509_privkey_export(params, format,
					  params_data, params_data_size);
}

/**
 * gnutls_rsa_params_export_raw:
 * @rsa: a structure that holds the rsa parameters
 * @m: will hold the modulus
 * @e: will hold the public exponent
 * @d: will hold the private exponent
 * @p: will hold the first prime (p)
 * @q: will hold the second prime (q)
 * @u: will hold the coefficient
 * @bits: if non null will hold the prime's number of bits
 *
 * This function will export the RSA parameters found in the given
 * structure. The new parameters will be allocated using
 * gnutls_malloc() and will be stored in the appropriate datum.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an negative error code.
 **/
int
gnutls_rsa_params_export_raw(gnutls_rsa_params_t rsa,
			     gnutls_datum_t * m, gnutls_datum_t * e,
			     gnutls_datum_t * d, gnutls_datum_t * p,
			     gnutls_datum_t * q, gnutls_datum_t * u,
			     unsigned int *bits)
{
	int ret;

	ret = gnutls_x509_privkey_export_rsa_raw(rsa, m, e, d, p, q, u);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	if (bits)
		*bits = _gnutls_mpi_get_nbits(rsa->params.params[3]);

	return 0;
}

#endif				/* ENABLE_RSA_EXPORT */
