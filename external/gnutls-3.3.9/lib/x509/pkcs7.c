/*
 * Copyright (C) 2003-2012 Free Software Foundation, Inc.
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

/* Functions that relate on PKCS7 certificate lists parsing.
 */

#include <gnutls_int.h>
#include <libtasn1.h>

#include <gnutls_datum.h>
#include <gnutls_global.h>
#include <gnutls_errors.h>
#include <common.h>
#include <x509_b64.h>

#define SIGNED_DATA_OID "1.2.840.113549.1.7.2"

/* Decodes the PKCS #7 signed data, and returns an ASN1_TYPE, 
 * which holds them. If raw is non null then the raw decoded
 * data are copied (they are locally allocated) there.
 */
static int
_decode_pkcs7_signed_data(ASN1_TYPE pkcs7, ASN1_TYPE * sdata,
			  gnutls_datum_t * raw)
{
	char oid[MAX_OID_SIZE];
	ASN1_TYPE c2;
	uint8_t *tmp = NULL;
	int tmp_size, len, result;

	len = sizeof(oid) - 1;
	result = asn1_read_value(pkcs7, "contentType", oid, &len);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	if (strcmp(oid, SIGNED_DATA_OID) != 0) {
		gnutls_assert();
		_gnutls_debug_log("Unknown PKCS7 Content OID '%s'\n", oid);
		return GNUTLS_E_UNKNOWN_PKCS_CONTENT_TYPE;
	}

	if ((result = asn1_create_element
	     (_gnutls_get_pkix(), "PKIX1.pkcs-7-SignedData",
	      &c2)) != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	/* the Signed-data has been created, so
	 * decode them.
	 */
	tmp_size = 0;
	result = asn1_read_value(pkcs7, "content", NULL, &tmp_size);
	if (result != ASN1_MEM_ERROR) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	tmp = gnutls_malloc(tmp_size);
	if (tmp == NULL) {
		gnutls_assert();
		result = GNUTLS_E_MEMORY_ERROR;
		goto cleanup;
	}

	result = asn1_read_value(pkcs7, "content", tmp, &tmp_size);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* tmp, tmp_size hold the data and the size of the CertificateSet structure
	 * actually the ANY stuff.
	 */

	/* Step 1. In case of a signed structure extract certificate set.
	 */

	result = asn1_der_decoding(&c2, tmp, tmp_size, NULL);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	if (raw == NULL) {
		gnutls_free(tmp);
	} else {
		raw->data = tmp;
		raw->size = tmp_size;
	}

	*sdata = c2;

	return 0;

      cleanup:
	if (c2)
		asn1_delete_structure(&c2);
	gnutls_free(tmp);
	return result;
}

static int pkcs7_reinit(gnutls_pkcs7_t pkcs7)
{
	int result;

	asn1_delete_structure(&pkcs7->pkcs7);

	result = asn1_create_element(_gnutls_get_pkix(),
				     "PKIX1.pkcs-7-ContentInfo",
				     &pkcs7->pkcs7);
	if (result != ASN1_SUCCESS) {
		result = _gnutls_asn2err(result);
		gnutls_assert();
		return result;
	}

	return 0;
}

/**
 * gnutls_pkcs7_init:
 * @pkcs7: The structure to be initialized
 *
 * This function will initialize a PKCS7 structure. PKCS7 structures
 * usually contain lists of X.509 Certificates and X.509 Certificate
 * revocation lists.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs7_init(gnutls_pkcs7_t * pkcs7)
{
	*pkcs7 = gnutls_calloc(1, sizeof(gnutls_pkcs7_int));

	if (*pkcs7) {
		int result = pkcs7_reinit(*pkcs7);
		if (result < 0) {
			gnutls_assert();
			gnutls_free(*pkcs7);
			return result;
		}
		return 0;	/* success */
	}
	return GNUTLS_E_MEMORY_ERROR;
}

/**
 * gnutls_pkcs7_deinit:
 * @pkcs7: The structure to be initialized
 *
 * This function will deinitialize a PKCS7 structure.
 **/
void gnutls_pkcs7_deinit(gnutls_pkcs7_t pkcs7)
{
	if (!pkcs7)
		return;

	if (pkcs7->pkcs7)
		asn1_delete_structure(&pkcs7->pkcs7);

	gnutls_free(pkcs7);
}

/**
 * gnutls_pkcs7_import:
 * @pkcs7: The structure to store the parsed PKCS7.
 * @data: The DER or PEM encoded PKCS7.
 * @format: One of DER or PEM
 *
 * This function will convert the given DER or PEM encoded PKCS7 to
 * the native #gnutls_pkcs7_t format.  The output will be stored in
 * @pkcs7.
 *
 * If the PKCS7 is PEM encoded it should have a header of "PKCS7".
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_pkcs7_import(gnutls_pkcs7_t pkcs7, const gnutls_datum_t * data,
		    gnutls_x509_crt_fmt_t format)
{
	int result = 0, need_free = 0;
	gnutls_datum_t _data;

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	_data.data = data->data;
	_data.size = data->size;

	/* If the PKCS7 is in PEM format then decode it
	 */
	if (format == GNUTLS_X509_FMT_PEM) {
		result =
		    _gnutls_fbase64_decode(PEM_PKCS7, data->data,
					   data->size, &_data);

		if (result <= 0) {
			gnutls_assert();
			return result;
		}

		need_free = 1;
	}

	if (pkcs7->expanded) {
		result = pkcs7_reinit(pkcs7);
		if (result < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}
	pkcs7->expanded = 1;

	result =
	    asn1_der_decoding(&pkcs7->pkcs7, _data.data, _data.size, NULL);
	if (result != ASN1_SUCCESS) {
		result = _gnutls_asn2err(result);
		gnutls_assert();
		goto cleanup;
	}

	if (need_free)
		_gnutls_free_datum(&_data);

	return 0;

      cleanup:
	if (need_free)
		_gnutls_free_datum(&_data);
	return result;
}

/**
 * gnutls_pkcs7_get_crt_raw:
 * @pkcs7: should contain a gnutls_pkcs7_t structure
 * @indx: contains the index of the certificate to extract
 * @certificate: the contents of the certificate will be copied
 *   there (may be null)
 * @certificate_size: should hold the size of the certificate
 *
 * This function will return a certificate of the PKCS7 or RFC2630
 * certificate set.
 *
 * After the last certificate has been read
 * %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE will be returned.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.  If the provided buffer is not long enough,
 *   then @certificate_size is updated and
 *   %GNUTLS_E_SHORT_MEMORY_BUFFER is returned.
 **/
int
gnutls_pkcs7_get_crt_raw(gnutls_pkcs7_t pkcs7,
			 int indx, void *certificate,
			 size_t * certificate_size)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result, len;
	char root2[ASN1_MAX_NAME_SIZE];
	char oid[MAX_OID_SIZE];
	gnutls_datum_t tmp = { NULL, 0 };

	if (certificate_size == NULL || pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, &tmp);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Step 2. Parse the CertificateSet 
	 */

	snprintf(root2, sizeof(root2), "certificates.?%u", indx + 1);

	len = sizeof(oid) - 1;

	result = asn1_read_value(c2, root2, oid, &len);

	if (result == ASN1_VALUE_NOT_FOUND) {
		result = GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
		goto cleanup;
	}

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* if 'Certificate' is the choice found: 
	 */
	if (strcmp(oid, "certificate") == 0) {
		int start, end;

		result = asn1_der_decoding_startEnd(c2, tmp.data, tmp.size,
						    root2, &start, &end);

		if (result != ASN1_SUCCESS) {
			gnutls_assert();
			result = _gnutls_asn2err(result);
			goto cleanup;
		}

		end = end - start + 1;

		if ((unsigned) end > *certificate_size) {
			*certificate_size = end;
			result = GNUTLS_E_SHORT_MEMORY_BUFFER;
			goto cleanup;
		}

		if (certificate)
			memcpy(certificate, &tmp.data[start], end);

		*certificate_size = end;

		result = 0;

	} else {
		result = GNUTLS_E_UNSUPPORTED_CERTIFICATE_TYPE;
	}

      cleanup:
	_gnutls_free_datum(&tmp);
	if (c2)
		asn1_delete_structure(&c2);
	return result;
}

/**
 * gnutls_pkcs7_get_crt_count:
 * @pkcs7: should contain a #gnutls_pkcs7_t structure
 *
 * This function will return the number of certifcates in the PKCS7
 * or RFC2630 certificate set.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs7_get_crt_count(gnutls_pkcs7_t pkcs7)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result, count;

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, NULL);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Step 2. Count the CertificateSet */

	result = asn1_number_of_elements(c2, "certificates", &count);

	asn1_delete_structure(&c2);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return 0;	/* no certificates */
	}

	return count;

}

/**
 * gnutls_pkcs7_export:
 * @pkcs7: Holds the pkcs7 structure
 * @format: the format of output params. One of PEM or DER.
 * @output_data: will contain a structure PEM or DER encoded
 * @output_data_size: holds the size of output_data (and will be
 *   replaced by the actual size of parameters)
 *
 * This function will export the pkcs7 structure to DER or PEM format.
 *
 * If the buffer provided is not long enough to hold the output, then
 * *@output_data_size is updated and %GNUTLS_E_SHORT_MEMORY_BUFFER
 * will be returned.
 *
 * If the structure is PEM encoded, it will have a header
 * of "BEGIN PKCS7".
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
  **/
int
gnutls_pkcs7_export(gnutls_pkcs7_t pkcs7,
		    gnutls_x509_crt_fmt_t format, void *output_data,
		    size_t * output_data_size)
{
	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	return _gnutls_x509_export_int(pkcs7->pkcs7, format, PEM_PKCS7,
				       output_data, output_data_size);
}

/**
 * gnutls_pkcs7_export2:
 * @pkcs7: Holds the pkcs7 structure
 * @format: the format of output params. One of PEM or DER.
 * @out: will contain a structure PEM or DER encoded
 *
 * This function will export the pkcs7 structure to DER or PEM format.
 *
 * The output buffer is allocated using gnutls_malloc().
 *
 * If the structure is PEM encoded, it will have a header
 * of "BEGIN PKCS7".
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1.3
 **/
int
gnutls_pkcs7_export2(gnutls_pkcs7_t pkcs7,
		     gnutls_x509_crt_fmt_t format, gnutls_datum_t * out)
{
	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	return _gnutls_x509_export_int2(pkcs7->pkcs7, format, PEM_PKCS7,
					out);
}

/* Creates an empty signed data structure in the pkcs7
 * structure and returns a handle to the signed data.
 */
static int create_empty_signed_data(ASN1_TYPE pkcs7, ASN1_TYPE * sdata)
{
	uint8_t one = 1;
	int result;

	*sdata = ASN1_TYPE_EMPTY;

	if ((result = asn1_create_element
	     (_gnutls_get_pkix(), "PKIX1.pkcs-7-SignedData",
	      sdata)) != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* Use version 1
	 */
	result = asn1_write_value(*sdata, "version", &one, 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* Use no digest algorithms
	 */

	/* id-data */
	result =
	    asn1_write_value(*sdata, "encapContentInfo.eContentType",
			     "1.2.840.113549.1.7.5", 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	result =
	    asn1_write_value(*sdata, "encapContentInfo.eContent", NULL, 0);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* Add no certificates.
	 */

	/* Add no crls.
	 */

	/* Add no signerInfos.
	 */

	/* Write the content type of the signed data
	 */
	result =
	    asn1_write_value(pkcs7, "contentType", SIGNED_DATA_OID, 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	return 0;

      cleanup:
	asn1_delete_structure(sdata);
	return result;

}

/**
 * gnutls_pkcs7_set_crt_raw:
 * @pkcs7: should contain a #gnutls_pkcs7_t structure
 * @crt: the DER encoded certificate to be added
 *
 * This function will add a certificate to the PKCS7 or RFC2630
 * certificate set.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_pkcs7_set_crt_raw(gnutls_pkcs7_t pkcs7, const gnutls_datum_t * crt)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result;

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, NULL);
	if (result < 0 && result != GNUTLS_E_ASN1_VALUE_NOT_FOUND) {
		gnutls_assert();
		return result;
	}

	/* If the signed data are uninitialized
	 * then create them.
	 */
	if (result == GNUTLS_E_ASN1_VALUE_NOT_FOUND) {
		/* The pkcs7 structure is new, so create the
		 * signedData.
		 */
		result = create_empty_signed_data(pkcs7->pkcs7, &c2);
		if (result < 0) {
			gnutls_assert();
			return result;
		}
	}

	/* Step 2. Append the new certificate.
	 */

	result = asn1_write_value(c2, "certificates", "NEW", 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	result =
	    asn1_write_value(c2, "certificates.?LAST", "certificate", 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	result =
	    asn1_write_value(c2, "certificates.?LAST.certificate",
			     crt->data, crt->size);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* Step 3. Replace the old content with the new
	 */
	result =
	    _gnutls_x509_der_encode_and_copy(c2, "", pkcs7->pkcs7,
					     "content", 0);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	asn1_delete_structure(&c2);

	return 0;

      cleanup:
	if (c2)
		asn1_delete_structure(&c2);
	return result;
}

/**
 * gnutls_pkcs7_set_crt:
 * @pkcs7: should contain a #gnutls_pkcs7_t structure
 * @crt: the certificate to be copied.
 *
 * This function will add a parsed certificate to the PKCS7 or
 * RFC2630 certificate set.  This is a wrapper function over
 * gnutls_pkcs7_set_crt_raw() .
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs7_set_crt(gnutls_pkcs7_t pkcs7, gnutls_x509_crt_t crt)
{
	int ret;
	gnutls_datum_t data;

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	ret = _gnutls_x509_der_encode(crt->cert, "", &data, 0);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret = gnutls_pkcs7_set_crt_raw(pkcs7, &data);

	_gnutls_free_datum(&data);

	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}


/**
 * gnutls_pkcs7_delete_crt:
 * @pkcs7: should contain a gnutls_pkcs7_t structure
 * @indx: the index of the certificate to delete
 *
 * This function will delete a certificate from a PKCS7 or RFC2630
 * certificate set.  Index starts from 0. Returns 0 on success.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs7_delete_crt(gnutls_pkcs7_t pkcs7, int indx)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result;
	char root2[ASN1_MAX_NAME_SIZE];

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. Decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, NULL);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Step 2. Delete the certificate.
	 */

	snprintf(root2, sizeof(root2), "certificates.?%u", indx + 1);

	result = asn1_write_value(c2, root2, NULL, 0);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* Step 3. Replace the old content with the new
	 */
	result =
	    _gnutls_x509_der_encode_and_copy(c2, "", pkcs7->pkcs7,
					     "content", 0);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	asn1_delete_structure(&c2);

	return 0;

      cleanup:
	if (c2)
		asn1_delete_structure(&c2);
	return result;
}

/* Read and write CRLs
 */

/**
 * gnutls_pkcs7_get_crl_raw:
 * @pkcs7: should contain a #gnutls_pkcs7_t structure
 * @indx: contains the index of the crl to extract
 * @crl: the contents of the crl will be copied there (may be null)
 * @crl_size: should hold the size of the crl
 *
 * This function will return a crl of the PKCS7 or RFC2630 crl set.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.  If the provided buffer is not long enough,
 *   then @crl_size is updated and %GNUTLS_E_SHORT_MEMORY_BUFFER is
 *   returned.  After the last crl has been read
 *   %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE will be returned.
 **/
int
gnutls_pkcs7_get_crl_raw(gnutls_pkcs7_t pkcs7,
			 int indx, void *crl, size_t * crl_size)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result;
	char root2[ASN1_MAX_NAME_SIZE];
	gnutls_datum_t tmp = { NULL, 0 };
	int start, end;

	if (pkcs7 == NULL || crl_size == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, &tmp);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Step 2. Parse the CertificateSet 
	 */

	snprintf(root2, sizeof(root2), "crls.?%u", indx + 1);

	/* Get the raw CRL 
	 */
	result = asn1_der_decoding_startEnd(c2, tmp.data, tmp.size,
					    root2, &start, &end);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	end = end - start + 1;

	if ((unsigned) end > *crl_size) {
		*crl_size = end;
		result = GNUTLS_E_SHORT_MEMORY_BUFFER;
		goto cleanup;
	}

	if (crl)
		memcpy(crl, &tmp.data[start], end);

	*crl_size = end;

	result = 0;

      cleanup:
	_gnutls_free_datum(&tmp);
	if (c2)
		asn1_delete_structure(&c2);
	return result;
}

/**
 * gnutls_pkcs7_get_crl_count:
 * @pkcs7: should contain a gnutls_pkcs7_t structure
 *
 * This function will return the number of certifcates in the PKCS7
 * or RFC2630 crl set.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs7_get_crl_count(gnutls_pkcs7_t pkcs7)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result, count;

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, NULL);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Step 2. Count the CertificateSet */

	result = asn1_number_of_elements(c2, "crls", &count);

	asn1_delete_structure(&c2);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return 0;	/* no crls */
	}

	return count;

}

/**
 * gnutls_pkcs7_set_crl_raw:
 * @pkcs7: should contain a #gnutls_pkcs7_t structure
 * @crl: the DER encoded crl to be added
 *
 * This function will add a crl to the PKCS7 or RFC2630 crl set.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_pkcs7_set_crl_raw(gnutls_pkcs7_t pkcs7, const gnutls_datum_t * crl)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result;

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, NULL);
	if (result < 0 && result != GNUTLS_E_ASN1_VALUE_NOT_FOUND) {
		gnutls_assert();
		return result;
	}

	/* If the signed data are uninitialized
	 * then create them.
	 */
	if (result == GNUTLS_E_ASN1_VALUE_NOT_FOUND) {
		/* The pkcs7 structure is new, so create the
		 * signedData.
		 */
		result = create_empty_signed_data(pkcs7->pkcs7, &c2);
		if (result < 0) {
			gnutls_assert();
			return result;
		}
	}

	/* Step 2. Append the new crl.
	 */

	result = asn1_write_value(c2, "crls", "NEW", 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	result = asn1_write_value(c2, "crls.?LAST", crl->data, crl->size);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* Step 3. Replace the old content with the new
	 */
	result =
	    _gnutls_x509_der_encode_and_copy(c2, "", pkcs7->pkcs7,
					     "content", 0);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	asn1_delete_structure(&c2);

	return 0;

      cleanup:
	if (c2)
		asn1_delete_structure(&c2);
	return result;
}

/**
 * gnutls_pkcs7_set_crl:
 * @pkcs7: should contain a #gnutls_pkcs7_t structure
 * @crl: the DER encoded crl to be added
 *
 * This function will add a parsed CRL to the PKCS7 or RFC2630 crl
 * set.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs7_set_crl(gnutls_pkcs7_t pkcs7, gnutls_x509_crl_t crl)
{
	int ret;
	gnutls_datum_t data;

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	ret = _gnutls_x509_der_encode(crl->crl, "", &data, 0);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret = gnutls_pkcs7_set_crl_raw(pkcs7, &data);

	_gnutls_free_datum(&data);

	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}

/**
 * gnutls_pkcs7_delete_crl:
 * @pkcs7: should contain a #gnutls_pkcs7_t structure
 * @indx: the index of the crl to delete
 *
 * This function will delete a crl from a PKCS7 or RFC2630 crl set.
 * Index starts from 0. Returns 0 on success.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs7_delete_crl(gnutls_pkcs7_t pkcs7, int indx)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result;
	char root2[ASN1_MAX_NAME_SIZE];

	if (pkcs7 == NULL)
		return GNUTLS_E_INVALID_REQUEST;

	/* Step 1. Decode the signed data.
	 */
	result = _decode_pkcs7_signed_data(pkcs7->pkcs7, &c2, NULL);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Step 2. Delete the crl.
	 */

	snprintf(root2, sizeof(root2), "crls.?%u", indx + 1);

	result = asn1_write_value(c2, root2, NULL, 0);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	/* Step 3. Replace the old content with the new
	 */
	result =
	    _gnutls_x509_der_encode_and_copy(c2, "", pkcs7->pkcs7,
					     "content", 0);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	asn1_delete_structure(&c2);

	return 0;

      cleanup:
	if (c2)
		asn1_delete_structure(&c2);
	return result;
}
