/*
 * GnuTLS PKCS#11 support
 * Copyright (C) 2010-2012 Free Software Foundation, Inc.
 * 
 * Authors: Nikos Mavrogiannopoulos, Stef Walter
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
 */

#include <gnutls_int.h>
#include <gnutls/pkcs11.h>
#include <stdio.h>
#include <string.h>
#include <gnutls_errors.h>
#include <gnutls_datum.h>
#include <pkcs11_int.h>
#include <gnutls_sig.h>
#include <gnutls_pk.h>
#include <fips.h>
#include <p11-kit/uri.h>

/* In case of a fork, it will invalidate the open session
 * in the privkey and start another */
#define PKCS11_CHECK_INIT_PRIVKEY(k) \
	ret = _gnutls_pkcs11_check_init(); \
	if (ret < 0) \
		return gnutls_assert_val(ret); \
	if (ret == 1) { \
		memset(&k->sinfo, 0, sizeof(k->sinfo)); \
		FIND_OBJECT(k); \
	}

#define FIND_OBJECT(key) \
	do { \
		int retries = 0; \
		int rret; \
		ret = find_object (&key->sinfo, &key->pin, &key->ref, key->uinfo, \
		                          SESSION_LOGIN); \
		if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) { \
			if (_gnutls_token_func) \
			  { \
			    rret = pkcs11_call_token_func (key->uinfo, retries++); \
			    if (rret == 0) continue; \
                          } \
			return gnutls_assert_val(ret); \
		} else if (ret < 0) { \
                        return gnutls_assert_val(ret); \
                } \
	} while (0);

struct gnutls_pkcs11_privkey_st {
	gnutls_pk_algorithm_t pk_algorithm;
	unsigned int flags;
	struct p11_kit_uri *uinfo;

	struct pkcs11_session_info sinfo;
	ck_object_handle_t ref;	/* the key in the session */

	struct pin_info_st pin;
};

/**
 * gnutls_pkcs11_privkey_init:
 * @key: The structure to be initialized
 *
 * This function will initialize an private key structure.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int gnutls_pkcs11_privkey_init(gnutls_pkcs11_privkey_t * key)
{
        FAIL_IF_LIB_ERROR;

	*key = gnutls_calloc(1, sizeof(struct gnutls_pkcs11_privkey_st));
	if (*key == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	(*key)->uinfo = p11_kit_uri_new();
	if ((*key)->uinfo == NULL) {
		free(*key);
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	return 0;
}

/**
 * gnutls_pkcs11_privkey_deinit:
 * @key: The structure to be initialized
 *
 * This function will deinitialize a private key structure.
 **/
void gnutls_pkcs11_privkey_deinit(gnutls_pkcs11_privkey_t key)
{
	p11_kit_uri_free(key->uinfo);
	if (key->sinfo.init != 0)
		pkcs11_close_session(&key->sinfo);
	gnutls_free(key);
}

/**
 * gnutls_pkcs11_privkey_get_pk_algorithm:
 * @key: should contain a #gnutls_pkcs11_privkey_t structure
 * @bits: if bits is non null it will hold the size of the parameters' in bits
 *
 * This function will return the public key algorithm of a private
 * key.
 *
 * Returns: a member of the #gnutls_pk_algorithm_t enumeration on
 *   success, or a negative error code on error.
 **/
int
gnutls_pkcs11_privkey_get_pk_algorithm(gnutls_pkcs11_privkey_t key,
				       unsigned int *bits)
{
	if (bits)
		*bits = 0;	/* FIXME */
	return key->pk_algorithm;
}

/**
 * gnutls_pkcs11_privkey_get_info:
 * @pkey: should contain a #gnutls_pkcs11_privkey_t structure
 * @itype: Denotes the type of information requested
 * @output: where output will be stored
 * @output_size: contains the maximum size of the output and will be overwritten with actual
 *
 * This function will return information about the PKCS 11 private key such
 * as the label, id as well as token information where the key is stored. When
 * output is text it returns null terminated string although #output_size contains
 * the size of the actual data only.
 *
 * Returns: %GNUTLS_E_SUCCESS (0) on success or a negative error code on error.
 **/
int
gnutls_pkcs11_privkey_get_info(gnutls_pkcs11_privkey_t pkey,
			       gnutls_pkcs11_obj_info_t itype,
			       void *output, size_t * output_size)
{
	return pkcs11_get_info(pkey->uinfo, itype, output, output_size);
}

static int
find_object(struct pkcs11_session_info *sinfo,
	    struct pin_info_st *pin_info,
	    ck_object_handle_t * _obj,
	    struct p11_kit_uri *info, unsigned int flags)
{
	int ret;
	ck_object_handle_t obj;
	struct ck_attribute *attrs;
	unsigned long attr_count;
	unsigned long count;
	ck_rv_t rv;

	ret =
	    pkcs11_open_session(sinfo, pin_info, info,
				flags & SESSION_LOGIN);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	attrs = p11_kit_uri_get_attributes(info, &attr_count);
	rv = pkcs11_find_objects_init(sinfo->module, sinfo->pks, attrs,
				      attr_count);
	if (rv != CKR_OK) {
		gnutls_assert();
		_gnutls_debug_log("p11: FindObjectsInit failed.\n");
		ret = pkcs11_rv_to_err(rv);
		goto fail;
	}

	if (pkcs11_find_objects(sinfo->module, sinfo->pks, &obj, 1, &count)
	    == CKR_OK && count == 1) {
		*_obj = obj;
		pkcs11_find_objects_final(sinfo);
		return 0;
	}

	ret = GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
	pkcs11_find_objects_final(sinfo);
      fail:
	pkcs11_close_session(sinfo);

	return ret;
}



/*-
 * _gnutls_pkcs11_privkey_sign_hash:
 * @key: Holds the key
 * @hash: holds the data to be signed (should be output of a hash)
 * @signature: will contain the signature allocated with gnutls_malloc()
 *
 * This function will sign the given data using a signature algorithm
 * supported by the private key. It is assumed that the given data
 * are the output of a hash function.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 -*/
int
_gnutls_pkcs11_privkey_sign_hash(gnutls_pkcs11_privkey_t key,
				 const gnutls_datum_t * hash,
				 gnutls_datum_t * signature)
{
	ck_rv_t rv;
	int ret;
	struct ck_mechanism mech;
	gnutls_datum_t tmp = { NULL, 0 };
	unsigned long siglen;
	struct pkcs11_session_info *sinfo;

	PKCS11_CHECK_INIT_PRIVKEY(key);

	sinfo = &key->sinfo;

	mech.mechanism = pk_to_mech(key->pk_algorithm);
	mech.parameter = NULL;
	mech.parameter_len = 0;

	/* Initialize signing operation; using the private key discovered
	 * earlier. */
	rv = pkcs11_sign_init(sinfo->module, sinfo->pks, &mech, key->ref);
	if (rv != CKR_OK) {
		gnutls_assert();
		ret = pkcs11_rv_to_err(rv);
		goto cleanup;
	}

	/* Work out how long the signature must be: */
	rv = pkcs11_sign(sinfo->module, sinfo->pks, hash->data, hash->size,
			 NULL, &siglen);
	if (rv != CKR_OK) {
		gnutls_assert();
		ret = pkcs11_rv_to_err(rv);
		goto cleanup;
	}

	tmp.data = gnutls_malloc(siglen);
	tmp.size = siglen;

	rv = pkcs11_sign(sinfo->module, sinfo->pks, hash->data, hash->size,
			 tmp.data, &siglen);
	if (rv != CKR_OK) {
		gnutls_assert();
		ret = pkcs11_rv_to_err(rv);
		goto cleanup;
	}


	if (key->pk_algorithm == GNUTLS_PK_EC
	    || key->pk_algorithm == GNUTLS_PK_DSA) {
		unsigned int hlen = siglen / 2;
		gnutls_datum_t r, s;

		if (siglen % 2 != 0) {
			gnutls_assert();
			ret = GNUTLS_E_PK_SIGN_FAILED;
			goto cleanup;
		}

		r.data = tmp.data;
		r.size = hlen;

		s.data = &tmp.data[hlen];
		s.size = hlen;

		ret = _gnutls_encode_ber_rs_raw(signature, &r, &s);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		gnutls_free(tmp.data);
		tmp.data = NULL;
	} else {
		signature->size = siglen;
		signature->data = tmp.data;
	}

	ret = 0;

      cleanup:
	if (sinfo != &key->sinfo)
		pkcs11_close_session(sinfo);
	if (ret < 0)
		gnutls_free(tmp.data);

	return ret;
}

/**
 * gnutls_pkcs11_privkey_status:
 * @key: Holds the key
 *
 * Checks the status of the private key token.
 *
 * Returns: this function will return non-zero if the token 
 * holding the private key is still available (inserted), and zero otherwise.
 * 
 * Since: 3.1.9
 *
 **/
int gnutls_pkcs11_privkey_status(gnutls_pkcs11_privkey_t key)
{
	ck_rv_t rv;
	int ret;
	struct ck_session_info session_info;
	
	PKCS11_CHECK_INIT_PRIVKEY(key);

	rv = (key->sinfo.module)->C_GetSessionInfo(key->sinfo.pks, &session_info);
	if (rv != CKR_OK) {
		ret = 0;
		goto cleanup;
	}
	ret = 1;

      cleanup:

	return ret;
}

/**
 * gnutls_pkcs11_privkey_import_url:
 * @pkey: The structure to store the parsed key
 * @url: a PKCS 11 url identifying the key
 * @flags: Or sequence of GNUTLS_PKCS11_OBJ_* flags
 *
 * This function will "import" a PKCS 11 URL identifying a private
 * key to the #gnutls_pkcs11_privkey_t structure. In reality since
 * in most cases keys cannot be exported, the private key structure
 * is being associated with the available operations on the token.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_pkcs11_privkey_import_url(gnutls_pkcs11_privkey_t pkey,
				 const char *url, unsigned int flags)
{
	int ret;
	struct ck_attribute *attr;
	struct ck_attribute a[4];
	ck_key_type_t key_type;

	PKCS11_CHECK_INIT;

	memset(&pkey->sinfo, 0, sizeof(pkey->sinfo));

	ret = pkcs11_url_to_info(url, &pkey->uinfo);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	pkey->flags = flags;

	attr = p11_kit_uri_get_attribute(pkey->uinfo, CKA_CLASS);
	if (!attr || attr->value_len != sizeof(ck_object_class_t) ||
	    *(ck_object_class_t *) attr->value != CKO_PRIVATE_KEY) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	attr = p11_kit_uri_get_attribute(pkey->uinfo, CKA_ID);
	if (!attr) {
		attr = p11_kit_uri_get_attribute(pkey->uinfo, CKA_LABEL);
		if (!attr) {
			gnutls_assert();
			return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;
		}
	}

	FIND_OBJECT(pkey);

	a[0].type = CKA_KEY_TYPE;
	a[0].value = &key_type;
	a[0].value_len = sizeof(key_type);

	if (pkcs11_get_attribute_value(pkey->sinfo.module, pkey->sinfo.pks, pkey->ref, a, 1)
	    == CKR_OK) {
		pkey->pk_algorithm = mech_to_pk(key_type);
		if (pkey->pk_algorithm == GNUTLS_PK_UNKNOWN) {
			_gnutls_debug_log
			    ("Cannot determine PKCS #11 key algorithm\n");
			ret = GNUTLS_E_UNKNOWN_ALGORITHM;
			goto cleanup;
		}
	}

	ret = 0;

	return ret;

      cleanup:
	pkcs11_close_session(&pkey->sinfo);

	return ret;
}

/*-
 * _gnutls_pkcs11_privkey_decrypt_data:
 * @key: Holds the key
 * @flags: should be 0 for now
 * @ciphertext: holds the data to be signed
 * @plaintext: will contain the plaintext, allocated with gnutls_malloc()
 *
 * This function will decrypt the given data using the public key algorithm
 * supported by the private key. 
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 -*/
int
_gnutls_pkcs11_privkey_decrypt_data(gnutls_pkcs11_privkey_t key,
				    unsigned int flags,
				    const gnutls_datum_t * ciphertext,
				    gnutls_datum_t * plaintext)
{
	ck_rv_t rv;
	int ret;
	struct ck_mechanism mech;
	unsigned long siglen;

	PKCS11_CHECK_INIT_PRIVKEY(key);

	if (key->pk_algorithm != GNUTLS_PK_RSA)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	mech.mechanism = CKM_RSA_PKCS;
	mech.parameter = NULL;
	mech.parameter_len = 0;

	/* Initialize signing operation; using the private key discovered
	 * earlier. */
	rv = pkcs11_decrypt_init(key->sinfo.module, key->sinfo.pks, &mech, key->ref);
	if (rv != CKR_OK) {
		gnutls_assert();
		ret = pkcs11_rv_to_err(rv);
		goto cleanup;
	}

	/* Work out how long the plaintext must be: */
	rv = pkcs11_decrypt(key->sinfo.module, key->sinfo.pks, ciphertext->data,
			    ciphertext->size, NULL, &siglen);
	if (rv != CKR_OK) {
		gnutls_assert();
		ret = pkcs11_rv_to_err(rv);
		goto cleanup;
	}

	plaintext->data = gnutls_malloc(siglen);
	plaintext->size = siglen;

	rv = pkcs11_decrypt(key->sinfo.module, key->sinfo.pks, ciphertext->data,
			    ciphertext->size, plaintext->data, &siglen);
	if (rv != CKR_OK) {
		gnutls_free(plaintext->data);
		gnutls_assert();
		ret = pkcs11_rv_to_err(rv);
		goto cleanup;
	}

	plaintext->size = siglen;

	ret = 0;

      cleanup:
	return ret;
}

/**
 * gnutls_pkcs11_privkey_export_url:
 * @key: Holds the PKCS 11 key
 * @detailed: non zero if a detailed URL is required
 * @url: will contain an allocated url
 *
 * This function will export a URL identifying the given key.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_pkcs11_privkey_export_url(gnutls_pkcs11_privkey_t key,
				 gnutls_pkcs11_url_type_t detailed,
				 char **url)
{
	int ret;

	ret = pkcs11_info_to_url(key->uinfo, detailed, url);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}


/**
 * gnutls_pkcs11_privkey_generate:
 * @url: a token URL
 * @pk: the public key algorithm
 * @bits: the security bits
 * @label: a label
 * @flags: should be zero
 *
 * This function will generate a private key in the specified
 * by the @url token. The private key will be generate within
 * the token and will not be exportable.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.0
 **/
int
gnutls_pkcs11_privkey_generate(const char *url, gnutls_pk_algorithm_t pk,
			       unsigned int bits, const char *label,
			       unsigned int flags)
{
	return gnutls_pkcs11_privkey_generate2(url, pk, bits, label, 0,
					       NULL, flags);
}

/**
 * gnutls_pkcs11_privkey_generate2:
 * @url: a token URL
 * @pk: the public key algorithm
 * @bits: the security bits
 * @label: a label
 * @fmt: the format of output params. PEM or DER.
 * @pubkey: will hold the public key (may be %NULL)
 * @flags: zero or an OR'ed sequence of %GNUTLS_PKCS11_OBJ_FLAGs
 *
 * This function will generate a private key in the specified
 * by the @url token. The private key will be generate within
 * the token and will not be exportable. This function will
 * store the DER-encoded public key in the SubjectPublicKeyInfo format 
 * in @pubkey. The @pubkey should be deinitialized using gnutls_free().
 *
 * Note that when generating an elliptic curve key, the curve
 * can be substituted in the place of the bits parameter using the
 * GNUTLS_CURVE_TO_BITS() macro.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1.5
 **/
int
gnutls_pkcs11_privkey_generate2(const char *url, gnutls_pk_algorithm_t pk,
				unsigned int bits, const char *label,
				gnutls_x509_crt_fmt_t fmt,
				gnutls_datum_t * pubkey,
				unsigned int flags)
{
	int ret;
	const ck_bool_t tval = 1;
	const ck_bool_t fval = 0;
	struct pkcs11_session_info sinfo;
	struct p11_kit_uri *info = NULL;
	ck_rv_t rv;
	struct ck_attribute a[10], p[10];
	ck_object_handle_t pub, priv;
	unsigned long _bits = bits;
	int a_val, p_val;
	struct ck_mechanism mech;
	gnutls_pubkey_t pkey = NULL;
	gnutls_pkcs11_obj_t obj = NULL;
	gnutls_datum_t der = {NULL, 0};
	ck_key_type_t key_type;
	char pubEx[3] = { 1,0,1 }; // 65537 = 0x10001

	PKCS11_CHECK_INIT;

	memset(&sinfo, 0, sizeof(sinfo));

	ret = pkcs11_url_to_info(url, &info);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret =
	    pkcs11_open_session(&sinfo, NULL, info,
				SESSION_WRITE |
				pkcs11_obj_flags_to_int(flags));
	p11_kit_uri_free(info);

	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	/* a holds the public key template
	 * and p the private key */
	a_val = p_val = 0;
	mech.parameter = NULL;
	mech.parameter_len = 0;
	mech.mechanism = pk_to_genmech(pk, &key_type);

	switch (pk) {
	case GNUTLS_PK_RSA:
		p[p_val].type = CKA_DECRYPT;
		p[p_val].value = (void *) &tval;
		p[p_val].value_len = sizeof(tval);
		p_val++;

		p[p_val].type = CKA_SIGN;
		p[p_val].value = (void *) &tval;
		p[p_val].value_len = sizeof(tval);
		p_val++;

		a[a_val].type = CKA_ENCRYPT;
		a[a_val].value = (void *) &tval;
		a[a_val].value_len = sizeof(tval);
		a_val++;

		a[a_val].type = CKA_VERIFY;
		a[a_val].value = (void *) &tval;
		a[a_val].value_len = sizeof(tval);
		a_val++;

		a[a_val].type = CKA_MODULUS_BITS;
		a[a_val].value = &_bits;
		a[a_val].value_len = sizeof(_bits);
		a_val++;

		a[a_val].type = CKA_PUBLIC_EXPONENT;
		a[a_val].value = pubEx;
		a[a_val].value_len = sizeof(pubEx);
		a_val++;

		break;
	case GNUTLS_PK_DSA:
		p[p_val].type = CKA_SIGN;
		p[p_val].value = (void *) &tval;
		p[p_val].value_len = sizeof(tval);
		p_val++;

		a[a_val].type = CKA_VERIFY;
		a[a_val].value = (void *) &tval;
		a[a_val].value_len = sizeof(tval);
		a_val++;

		a[a_val].type = CKA_MODULUS_BITS;
		a[a_val].value = &_bits;
		a[a_val].value_len = sizeof(_bits);
		a_val++;
		break;
	case GNUTLS_PK_EC:
		p[p_val].type = CKA_SIGN;
		p[p_val].value = (void *) &tval;
		p[p_val].value_len = sizeof(tval);
		p_val++;

		a[a_val].type = CKA_VERIFY;
		a[a_val].value = (void *) &tval;
		a[a_val].value_len = sizeof(tval);
		a_val++;

		if (GNUTLS_BITS_ARE_CURVE(bits)) {
			bits = GNUTLS_BITS_TO_CURVE(bits);
		} else {
			bits = _gnutls_ecc_bits_to_curve(bits);
		}

		ret = _gnutls_x509_write_ecc_params(bits, &der);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		a[a_val].type = CKA_EC_PARAMS;
		a[a_val].value = der.data;
		a[a_val].value_len = der.size;
		a_val++;
		break;
	default:
		ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
		goto cleanup;
	}

	/*
	 * on request, add the CKA_WRAP/CKA_UNWRAP key attribute
	 */
	if (flags & GNUTLS_PKCS11_OBJ_FLAG_MARK_KEY_WRAP) {
		p[p_val].type = CKA_UNWRAP;
		p[p_val].value = (void*)&tval;
		p[p_val].value_len = sizeof(tval);
		p_val++;
		a[a_val].type = CKA_WRAP;
		a[a_val].value = (void*)&tval;
		a[a_val].value_len = sizeof(tval);
		a_val++;
	}

	/* a private key is set always as private unless
	 * requested otherwise
	 */
	if (flags & GNUTLS_PKCS11_OBJ_FLAG_MARK_NOT_PRIVATE) {
		p[p_val].type = CKA_PRIVATE;
		p[p_val].value = (void *) &fval;
		p[p_val].value_len = sizeof(fval);
		p_val++;
	} else {
		p[p_val].type = CKA_PRIVATE;
		p[p_val].value = (void *) &tval;
		p[p_val].value_len = sizeof(tval);
		p_val++;
	}

	p[p_val].type = CKA_TOKEN;
	p[p_val].value = (void *) &tval;
	p[p_val].value_len = sizeof(tval);
	p_val++;

	if (label) {
		p[p_val].type = CKA_LABEL;
		p[p_val].value = (void *) label;
		p[p_val].value_len = strlen(label);
		p_val++;

		a[a_val].type = CKA_LABEL;
		a[a_val].value = (void *) label;
		a[a_val].value_len = strlen(label);
		a_val++;
	}

	if (flags & GNUTLS_PKCS11_OBJ_FLAG_MARK_SENSITIVE) {
		p[p_val].type = CKA_SENSITIVE;
		p[p_val].value = (void *) &tval;
		p[p_val].value_len = sizeof(tval);
		p_val++;
	} else {
		p[p_val].type = CKA_SENSITIVE;
		p[p_val].value = (void *) &fval;
		p[p_val].value_len = sizeof(fval);
		p_val++;
	}

	rv = pkcs11_generate_key_pair(sinfo.module, sinfo.pks, &mech, a,
				      a_val, p, p_val, &pub, &priv);
	if (rv != CKR_OK) {
		gnutls_assert();
		_gnutls_debug_log("p11: %s\n", pkcs11_strerror(rv));
		ret = pkcs11_rv_to_err(rv);
		goto cleanup;
	}

	/* extract the public key */
	if (pubkey) {
		ret = gnutls_pubkey_init(&pkey);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = gnutls_pkcs11_obj_init(&obj);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		obj->pk_algorithm = pk;
		obj->type = GNUTLS_PKCS11_OBJ_PUBKEY;
		ret =
		    pkcs11_read_pubkey(sinfo.module, sinfo.pks, pub,
				       key_type, obj->pubkey);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = gnutls_pubkey_import_pkcs11(pkey, obj, 0);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		ret = gnutls_pubkey_export2(pkey, fmt, pubkey);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}


      cleanup:
	if (obj != NULL)
		gnutls_pkcs11_obj_deinit(obj);
	if (pkey != NULL)
		gnutls_pubkey_deinit(pkey);

	if (sinfo.pks != 0)
		pkcs11_close_session(&sinfo);
	gnutls_free(der.data);

	return ret;
}

int
_pkcs11_privkey_get_pubkey (gnutls_pkcs11_privkey_t pkey, gnutls_pubkey_t *pub, unsigned flags)
{
	struct ck_mechanism mech;
	gnutls_pubkey_t pubkey = NULL;
	gnutls_pkcs11_obj_t obj = NULL;
	ck_key_type_t key_type;
	int ret;

	PKCS11_CHECK_INIT_PRIVKEY(pkey);

	if (!pkey) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	ret = gnutls_pubkey_init(&pubkey);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = gnutls_pkcs11_obj_init(&obj);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	obj->pk_algorithm = gnutls_pkcs11_privkey_get_pk_algorithm(pkey, 0);
	obj->type = GNUTLS_PKCS11_OBJ_PUBKEY;
	mech.mechanism = pk_to_genmech(obj->pk_algorithm, &key_type);
	ret = pkcs11_read_pubkey(pkey->sinfo.module, pkey->sinfo.pks, pkey->ref, mech.mechanism, obj->pubkey);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = gnutls_pubkey_import_pkcs11(pubkey, obj, 0);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	*pub = pubkey;

	pubkey = NULL;
	ret = 0;

      cleanup:
	if (obj != NULL)
		gnutls_pkcs11_obj_deinit(obj);
	if (pubkey != NULL)
		gnutls_pubkey_deinit(pubkey);

	return ret;
}

/**
 * gnutls_pkcs11_privkey_export_pubkey
 * @pkey: The private key
 * @fmt: the format of output params. PEM or DER.
 * @data: will hold the public key
 * @flags: should be zero
 *
 * This function will extract the public key (modulus and public
 * exponent) from the private key specified by the @url private key.
 * This public key will be stored in @pubkey in the format specified
 * by @fmt. @pubkey should be deinitialized using gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.3.7
 **/
int
gnutls_pkcs11_privkey_export_pubkey(gnutls_pkcs11_privkey_t pkey,
                                     gnutls_x509_crt_fmt_t fmt,
                                     gnutls_datum_t * data,
				     unsigned int flags)
{
	int ret;
	gnutls_pubkey_t pubkey = NULL;

	ret = _pkcs11_privkey_get_pubkey(pkey, &pubkey, flags);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = gnutls_pubkey_export2(pubkey, fmt, data);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;

      cleanup:
	if (pubkey != NULL)
		gnutls_pubkey_deinit(pubkey);

	return ret;
}

/**
 * gnutls_pkcs11_privkey_set_pin_function:
 * @key: The private key
 * @fn: the callback
 * @userdata: data associated with the callback
 *
 * This function will set a callback function to be used when
 * required to access the object. This function overrides the global
 * set using gnutls_pkcs11_set_pin_function().
 *
 * Since: 3.1.0
 *
 **/
void
gnutls_pkcs11_privkey_set_pin_function(gnutls_pkcs11_privkey_t key,
				       gnutls_pin_callback_t fn,
				       void *userdata)
{
	key->pin.cb = fn;
	key->pin.data = userdata;
}
