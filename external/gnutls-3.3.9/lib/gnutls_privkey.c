/*
 * GnuTLS PKCS#11 support
 * Copyright (C) 2010-2014 Free Software Foundation, Inc.
 * Copyright (C) 2012-2014 Nikos Mavrogiannopoulos
 * 
 * Author: Nikos Mavrogiannopoulos
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
#include <gnutls/abstract.h>
#include <gnutls_pk.h>
#include <x509_int.h>
#include <openpgp/openpgp_int.h>
#include <openpgp/gnutls_openpgp.h>
#include <gnutls_sig.h>
#include <algorithms.h>
#include <fips.h>
#include <abstract_int.h>

/**
 * gnutls_privkey_get_type:
 * @key: should contain a #gnutls_privkey_t structure
 *
 * This function will return the type of the private key. This is
 * actually the type of the subsystem used to set this private key.
 *
 * Returns: a member of the #gnutls_privkey_type_t enumeration on
 *   success, or a negative error code on error.
 *
 * Since: 2.12.0
 **/
gnutls_privkey_type_t gnutls_privkey_get_type(gnutls_privkey_t key)
{
	return key->type;
}

/**
 * gnutls_privkey_get_pk_algorithm:
 * @key: should contain a #gnutls_privkey_t structure
 * @bits: If set will return the number of bits of the parameters (may be NULL)
 *
 * This function will return the public key algorithm of a private
 * key and if possible will return a number of bits that indicates
 * the security parameter of the key.
 *
 * Returns: a member of the #gnutls_pk_algorithm_t enumeration on
 *   success, or a negative error code on error.
 *
 * Since: 2.12.0
 **/
int gnutls_privkey_get_pk_algorithm(gnutls_privkey_t key, unsigned int *bits)
{
	switch (key->type) {
#ifdef ENABLE_OPENPGP
	case GNUTLS_PRIVKEY_OPENPGP:
		return gnutls_openpgp_privkey_get_pk_algorithm(key->key.openpgp,
							       bits);
#endif
#ifdef ENABLE_PKCS11
	case GNUTLS_PRIVKEY_PKCS11:
		return gnutls_pkcs11_privkey_get_pk_algorithm(key->key.pkcs11,
							      bits);
#endif
	case GNUTLS_PRIVKEY_X509:
		if (bits)
			*bits =
			    _gnutls_mpi_get_nbits(key->key.x509->
						  params.params[0]);
		return gnutls_x509_privkey_get_pk_algorithm(key->key.x509);
	case GNUTLS_PRIVKEY_EXT:
		if (bits)
			*bits = 0;
		return key->pk_algorithm;
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

}

static int
privkey_to_pubkey(gnutls_pk_algorithm_t pk,
		  const gnutls_pk_params_st * priv, gnutls_pk_params_st * pub)
{
	int ret;

	pub->algo = priv->algo;
	pub->flags = priv->flags;

	switch (pk) {
	case GNUTLS_PK_RSA:
		pub->params[0] = _gnutls_mpi_copy(priv->params[0]);
		pub->params[1] = _gnutls_mpi_copy(priv->params[1]);

		pub->params_nr = RSA_PUBLIC_PARAMS;

		if (pub->params[0] == NULL || pub->params[1] == NULL) {
			gnutls_assert();
			ret = GNUTLS_E_MEMORY_ERROR;
			goto cleanup;
		}

		break;
	case GNUTLS_PK_DSA:
		pub->params[0] = _gnutls_mpi_copy(priv->params[0]);
		pub->params[1] = _gnutls_mpi_copy(priv->params[1]);
		pub->params[2] = _gnutls_mpi_copy(priv->params[2]);
		pub->params[3] = _gnutls_mpi_copy(priv->params[3]);

		pub->params_nr = DSA_PUBLIC_PARAMS;

		if (pub->params[0] == NULL || pub->params[1] == NULL ||
		    pub->params[2] == NULL || pub->params[3] == NULL) {
			gnutls_assert();
			ret = GNUTLS_E_MEMORY_ERROR;
			goto cleanup;
		}

		break;
	case GNUTLS_PK_EC:
		pub->params[ECC_X] = _gnutls_mpi_copy(priv->params[ECC_X]);
		pub->params[ECC_Y] = _gnutls_mpi_copy(priv->params[ECC_Y]);

		pub->params_nr = ECC_PUBLIC_PARAMS;

		if (pub->params[ECC_X] == NULL || pub->params[ECC_Y] == NULL) {
			gnutls_assert();
			ret = GNUTLS_E_MEMORY_ERROR;
			goto cleanup;
		}

		break;
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	return 0;
 cleanup:
	gnutls_pk_params_release(pub);
	return ret;
}

/* Returns the public key of the private key (if possible)
 */
int
_gnutls_privkey_get_mpis(gnutls_privkey_t key, gnutls_pk_params_st * params)
{
	int ret;

	switch (key->type) {
#ifdef ENABLE_OPENPGP
	case GNUTLS_PRIVKEY_OPENPGP:
		{
			uint32_t kid[2];
			uint8_t keyid[GNUTLS_OPENPGP_KEYID_SIZE];

			ret =
			    gnutls_openpgp_privkey_get_preferred_key_id
			    (key->key.openpgp, keyid);
			if (ret == 0) {
				KEYID_IMPORT(kid, keyid);
				ret =
				    _gnutls_openpgp_privkey_get_mpis
				    (key->key.openpgp, kid, params);
			} else
				ret =
				    _gnutls_openpgp_privkey_get_mpis
				    (key->key.openpgp, NULL, params);

			if (ret < 0) {
				gnutls_assert();
				return ret;
			}
		}

		break;
#endif
	case GNUTLS_PRIVKEY_X509:
		ret = _gnutls_pk_params_copy(params, &key->key.x509->params);
		break;
#ifdef ENABLE_PKCS11
	case GNUTLS_PRIVKEY_PKCS11: {
		gnutls_pubkey_t pubkey;

		ret = _pkcs11_privkey_get_pubkey(key->key.pkcs11, &pubkey, 0);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ret = _gnutls_pubkey_get_mpis(pubkey, params);
		gnutls_pubkey_deinit(pubkey);

		break;
		}
#endif
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	return ret;
}

int
_gnutls_privkey_get_public_mpis(gnutls_privkey_t key,
				gnutls_pk_params_st * params)
{
	int ret;
	gnutls_pk_params_st tmp1;

	gnutls_pk_params_init(&tmp1);

	ret = _gnutls_privkey_get_mpis(key, &tmp1);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = privkey_to_pubkey(key->pk_algorithm, &tmp1, params);

	gnutls_pk_params_release(&tmp1);

	if (ret < 0)
		gnutls_assert();

	return ret;
}

/**
 * gnutls_privkey_init:
 * @key: The structure to be initialized
 *
 * This function will initialize an private key structure.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 2.12.0
 **/
int gnutls_privkey_init(gnutls_privkey_t * key)
{
	FAIL_IF_LIB_ERROR;

	*key = gnutls_calloc(1, sizeof(struct gnutls_privkey_st));
	if (*key == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	return 0;
}

/**
 * gnutls_privkey_deinit:
 * @key: The structure to be deinitialized
 *
 * This function will deinitialize a private key structure.
 *
 * Since: 2.12.0
 **/
void gnutls_privkey_deinit(gnutls_privkey_t key)
{
	if (key == NULL)
		return;

	if (key->flags & GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE
	    || key->flags & GNUTLS_PRIVKEY_IMPORT_COPY)
		switch (key->type) {
#ifdef ENABLE_OPENPGP
		case GNUTLS_PRIVKEY_OPENPGP:
			gnutls_openpgp_privkey_deinit(key->key.openpgp);
			break;
#endif
#ifdef ENABLE_PKCS11
		case GNUTLS_PRIVKEY_PKCS11:
			gnutls_pkcs11_privkey_deinit(key->key.pkcs11);
			break;
#endif
		case GNUTLS_PRIVKEY_X509:
			gnutls_x509_privkey_deinit(key->key.x509);
			break;
		case GNUTLS_PRIVKEY_EXT:
			if (key->key.ext.deinit_func != NULL)
				key->key.ext.deinit_func(key,
							 key->key.ext.userdata);
			break;
		default:
			break;
		}
	gnutls_free(key);
}

/* will fail if the private key contains an actual key.
 */
static int check_if_clean(gnutls_privkey_t key)
{
	if (key->type != 0)
		return GNUTLS_E_INVALID_REQUEST;

	return 0;
}

#ifdef ENABLE_PKCS11

/**
 * gnutls_privkey_import_pkcs11:
 * @pkey: The private key
 * @key: The private key to be imported
 * @flags: Flags for the import
 *
 * This function will import the given private key to the abstract
 * #gnutls_privkey_t structure.
 *
 * The #gnutls_pkcs11_privkey_t object must not be deallocated
 * during the lifetime of this structure.
 *
 * @flags might be zero or one of %GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE
 * and %GNUTLS_PRIVKEY_IMPORT_COPY.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 2.12.0
 **/
int
gnutls_privkey_import_pkcs11(gnutls_privkey_t pkey,
			     gnutls_pkcs11_privkey_t key, unsigned int flags)
{
	int ret;

	ret = check_if_clean(pkey);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	if (flags & GNUTLS_PRIVKEY_IMPORT_COPY)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	pkey->key.pkcs11 = key;
	pkey->type = GNUTLS_PRIVKEY_PKCS11;
	pkey->pk_algorithm = gnutls_pkcs11_privkey_get_pk_algorithm(key, NULL);
	pkey->flags = flags;

	if (pkey->pin.data)
		gnutls_pkcs11_privkey_set_pin_function(key, pkey->pin.cb,
						       pkey->pin.data);

	return 0;
}

/**
 * gnutls_privkey_import_pkcs11_url:
 * @key: A key of type #gnutls_pubkey_t
 * @url: A PKCS 11 url
 *
 * This function will import a PKCS 11 private key to a #gnutls_private_key_t
 * structure.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1.0
 **/
int gnutls_privkey_import_pkcs11_url(gnutls_privkey_t key, const char *url)
{
	gnutls_pkcs11_privkey_t pkey;
	int ret;

	ret = gnutls_pkcs11_privkey_init(&pkey);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	if (key->pin.cb)
		gnutls_pkcs11_privkey_set_pin_function(pkey, key->pin.cb,
						       key->pin.data);

	ret = gnutls_pkcs11_privkey_import_url(pkey, url, 0);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret =
	    gnutls_privkey_import_pkcs11(key, pkey,
					 GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	return 0;

 cleanup:
	gnutls_pkcs11_privkey_deinit(pkey);

	return ret;
}

#endif				/* ENABLE_PKCS11 */

/**
 * gnutls_privkey_import_ext:
 * @pkey: The private key
 * @pk: The public key algorithm
 * @userdata: private data to be provided to the callbacks
 * @sign_func: callback for signature operations
 * @decrypt_func: callback for decryption operations
 * @flags: Flags for the import
 *
 * This function will associate the given callbacks with the
 * #gnutls_privkey_t structure. At least one of the two callbacks
 * must be non-null.
 *
 * See also gnutls_privkey_import_ext2().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.0
 **/
int
gnutls_privkey_import_ext(gnutls_privkey_t pkey,
			  gnutls_pk_algorithm_t pk,
			  void *userdata,
			  gnutls_privkey_sign_func sign_func,
			  gnutls_privkey_decrypt_func decrypt_func,
			  unsigned int flags)
{
	return gnutls_privkey_import_ext2(pkey, pk, userdata, sign_func,
					  decrypt_func, NULL, flags);
}

/**
 * gnutls_privkey_import_ext2:
 * @pkey: The private key
 * @pk: The public key algorithm
 * @userdata: private data to be provided to the callbacks
 * @sign_func: callback for signature operations
 * @decrypt_func: callback for decryption operations
 * @deinit_func: a deinitialization function
 * @flags: Flags for the import
 *
 * This function will associate the given callbacks with the
 * #gnutls_privkey_t structure. At least one of the two callbacks
 * must be non-null. If a deinitialization function is provided
 * then flags is assumed to contain %GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE.
 *
 * Note that the signing function is supposed to "raw" sign data, i.e.,
 * without any hashing or preprocessing. In case of RSA the DigestInfo
 * will be provided, and the signing function is expected to do the PKCS #1
 * 1.5 padding and the exponentiation.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1
 **/
int
gnutls_privkey_import_ext2(gnutls_privkey_t pkey,
			   gnutls_pk_algorithm_t pk,
			   void *userdata,
			   gnutls_privkey_sign_func sign_func,
			   gnutls_privkey_decrypt_func decrypt_func,
			   gnutls_privkey_deinit_func deinit_func,
			   unsigned int flags)
{
	int ret;

	ret = check_if_clean(pkey);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	if (sign_func == NULL && decrypt_func == NULL)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	pkey->key.ext.sign_func = sign_func;
	pkey->key.ext.decrypt_func = decrypt_func;
	pkey->key.ext.deinit_func = deinit_func;
	pkey->key.ext.userdata = userdata;
	pkey->type = GNUTLS_PRIVKEY_EXT;
	pkey->pk_algorithm = pk;
	pkey->flags = flags;

	/* Ensure gnutls_privkey_deinit() calls the deinit_func */
	if (deinit_func)
		pkey->flags |= GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE;

	return 0;
}

/**
 * gnutls_privkey_import_x509:
 * @pkey: The private key
 * @key: The private key to be imported
 * @flags: Flags for the import
 *
 * This function will import the given private key to the abstract
 * #gnutls_privkey_t structure.
 *
 * The #gnutls_x509_privkey_t object must not be deallocated
 * during the lifetime of this structure.
 *
 * @flags might be zero or one of %GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE
 * and %GNUTLS_PRIVKEY_IMPORT_COPY.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 2.12.0
 **/
int
gnutls_privkey_import_x509(gnutls_privkey_t pkey,
			   gnutls_x509_privkey_t key, unsigned int flags)
{
	int ret;

	ret = check_if_clean(pkey);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	if (flags & GNUTLS_PRIVKEY_IMPORT_COPY) {
		ret = gnutls_x509_privkey_init(&pkey->key.x509);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ret = gnutls_x509_privkey_cpy(pkey->key.x509, key);
		if (ret < 0) {
			gnutls_x509_privkey_deinit(pkey->key.x509);
			return gnutls_assert_val(ret);
		}
	} else
		pkey->key.x509 = key;

	pkey->type = GNUTLS_PRIVKEY_X509;
	pkey->pk_algorithm = gnutls_x509_privkey_get_pk_algorithm(key);
	pkey->flags = flags;

	return 0;
}

/**
 * gnutls_privkey_generate:
 * @pkey: The private key
 * @algo: is one of the algorithms in #gnutls_pk_algorithm_t.
 * @bits: the size of the modulus
 * @flags: unused for now.  Must be 0.
 *
 * This function will generate a random private key. Note that this
 * function must be called on an empty private key.
 *
 * Note that when generating an elliptic curve key, the curve
 * can be substituted in the place of the bits parameter using the
 * GNUTLS_CURVE_TO_BITS() macro.
 *
 * Do not set the number of bits directly, use gnutls_sec_param_to_pk_bits().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.3.0
 **/
int
gnutls_privkey_generate(gnutls_privkey_t pkey,
			gnutls_pk_algorithm_t algo, unsigned int bits,
			unsigned int flags)
{
	int ret;

	ret = gnutls_x509_privkey_init(&pkey->key.x509);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = gnutls_x509_privkey_generate(pkey->key.x509, algo, bits, flags);
	if (ret < 0) {
		gnutls_x509_privkey_deinit(pkey->key.x509);
		pkey->key.x509 = NULL;
		return gnutls_assert_val(ret);
	}

	pkey->type = GNUTLS_PRIVKEY_X509;
	pkey->pk_algorithm = algo;
	pkey->flags = flags | GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE;

	return 0;
}

#ifdef ENABLE_OPENPGP
/**
 * gnutls_privkey_import_openpgp:
 * @pkey: The private key
 * @key: The private key to be imported
 * @flags: Flags for the import
 *
 * This function will import the given private key to the abstract
 * #gnutls_privkey_t structure.
 *
 * The #gnutls_openpgp_privkey_t object must not be deallocated
 * during the lifetime of this structure. The subkey set as
 * preferred will be used, or the master key otherwise.
 *
 * @flags might be zero or one of %GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE
 * and %GNUTLS_PRIVKEY_IMPORT_COPY.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 2.12.0
 **/
int
gnutls_privkey_import_openpgp(gnutls_privkey_t pkey,
			      gnutls_openpgp_privkey_t key, unsigned int flags)
{
	int ret, idx;
	uint8_t keyid[GNUTLS_OPENPGP_KEYID_SIZE];

	ret = check_if_clean(pkey);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	if (flags & GNUTLS_PRIVKEY_IMPORT_COPY) {
		ret = gnutls_openpgp_privkey_init(&pkey->key.openpgp);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ret = _gnutls_openpgp_privkey_cpy(pkey->key.openpgp, key);
		if (ret < 0) {
			gnutls_openpgp_privkey_deinit(pkey->key.openpgp);
			return gnutls_assert_val(ret);
		}
	} else
		pkey->key.openpgp = key;

	pkey->type = GNUTLS_PRIVKEY_OPENPGP;

	ret = gnutls_openpgp_privkey_get_preferred_key_id(key, keyid);
	if (ret == GNUTLS_E_OPENPGP_PREFERRED_KEY_ERROR) {
		pkey->pk_algorithm =
		    gnutls_openpgp_privkey_get_pk_algorithm(key, NULL);
	} else {
		if (ret < 0)
			return gnutls_assert_val(ret);

		idx = gnutls_openpgp_privkey_get_subkey_idx(key, keyid);

		pkey->pk_algorithm =
		    gnutls_openpgp_privkey_get_subkey_pk_algorithm(key,
								   idx, NULL);
	}

	pkey->flags = flags;

	return 0;
}

/**
 * gnutls_privkey_import_openpgp_raw:
 * @pkey: The private key
 * @data: The private key data to be imported
 * @format: The format of the private key
 * @keyid: The key id to use (optional)
 * @password: A password (optional)
 *
 * This function will import the given private key to the abstract
 * #gnutls_privkey_t structure. 
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1.0
 **/
int gnutls_privkey_import_openpgp_raw(gnutls_privkey_t pkey,
				      const gnutls_datum_t * data,
				      gnutls_openpgp_crt_fmt_t format,
				      const gnutls_openpgp_keyid_t keyid,
				      const char *password)
{
	gnutls_openpgp_privkey_t xpriv;
	int ret;

	ret = gnutls_openpgp_privkey_init(&xpriv);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = gnutls_openpgp_privkey_import(xpriv, data, format, password, 0);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (keyid) {
		ret = gnutls_openpgp_privkey_set_preferred_key_id(xpriv, keyid);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}

	ret =
	    gnutls_privkey_import_openpgp(pkey, xpriv,
					  GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;

 cleanup:
	gnutls_openpgp_privkey_deinit(xpriv);

	return ret;
}
#endif

/**
 * gnutls_privkey_sign_data:
 * @signer: Holds the key
 * @hash: should be a digest algorithm
 * @flags: Zero or one of %gnutls_privkey_flags_t
 * @data: holds the data to be signed
 * @signature: will contain the signature allocate with gnutls_malloc()
 *
 * This function will sign the given data using a signature algorithm
 * supported by the private key. Signature algorithms are always used
 * together with a hash functions.  Different hash functions may be
 * used for the RSA algorithm, but only the SHA family for the DSA keys.
 *
 * You may use gnutls_pubkey_get_preferred_hash_algorithm() to determine
 * the hash algorithm.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 * negative error value.
 *
 * Since: 2.12.0
 **/
int
gnutls_privkey_sign_data(gnutls_privkey_t signer,
			 gnutls_digest_algorithm_t hash,
			 unsigned int flags,
			 const gnutls_datum_t * data,
			 gnutls_datum_t * signature)
{
	int ret;
	gnutls_datum_t digest;
	const mac_entry_st *me = hash_to_entry(hash);

	if (flags & GNUTLS_PRIVKEY_SIGN_FLAG_TLS1_RSA)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	ret = pk_hash_data(signer->pk_algorithm, me, NULL, data, &digest);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	ret = pk_prepare_hash(signer->pk_algorithm, me, &digest);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = gnutls_privkey_sign_raw_data(signer, flags, &digest, signature);
	_gnutls_free_datum(&digest);

	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;

 cleanup:
	_gnutls_free_datum(&digest);
	return ret;
}

/**
 * gnutls_privkey_sign_hash:
 * @signer: Holds the signer's key
 * @hash_algo: The hash algorithm used
 * @flags: Zero or one of %gnutls_privkey_flags_t
 * @hash_data: holds the data to be signed
 * @signature: will contain newly allocated signature
 *
 * This function will sign the given hashed data using a signature algorithm
 * supported by the private key. Signature algorithms are always used
 * together with a hash functions.  Different hash functions may be
 * used for the RSA algorithm, but only SHA-XXX for the DSA keys.
 *
 * You may use gnutls_pubkey_get_preferred_hash_algorithm() to determine
 * the hash algorithm.
 *
 * Note that if %GNUTLS_PRIVKEY_SIGN_FLAG_TLS1_RSA flag is specified this function
 * will ignore @hash_algo and perform a raw PKCS1 signature.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 2.12.0
 **/
int
gnutls_privkey_sign_hash(gnutls_privkey_t signer,
			 gnutls_digest_algorithm_t hash_algo,
			 unsigned int flags,
			 const gnutls_datum_t * hash_data,
			 gnutls_datum_t * signature)
{
	int ret;
	gnutls_datum_t digest;

	if (flags & GNUTLS_PRIVKEY_SIGN_FLAG_TLS1_RSA)
		return gnutls_privkey_sign_raw_data(signer, flags,
						    hash_data, signature);

	digest.data = gnutls_malloc(hash_data->size);
	if (digest.data == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}
	digest.size = hash_data->size;
	memcpy(digest.data, hash_data->data, digest.size);

	ret =
	    pk_prepare_hash(signer->pk_algorithm, hash_to_entry(hash_algo),
			    &digest);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = gnutls_privkey_sign_raw_data(signer, flags, &digest, signature);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;

 cleanup:
	_gnutls_free_datum(&digest);
	return ret;
}

/**
 * gnutls_privkey_sign_raw_data:
 * @key: Holds the key
 * @flags: should be zero
 * @data: holds the data to be signed
 * @signature: will contain the signature allocate with gnutls_malloc()
 *
 * This function will sign the given data using a signature algorithm
 * supported by the private key. Note that this is a low-level function
 * and does not apply any preprocessing or hash on the signed data. 
 * For example on an RSA key the input @data should be of the DigestInfo
 * PKCS #1 1.5 format. Use it only if you know what are you doing.
 *
 * Note this function is equivalent to using the %GNUTLS_PRIVKEY_SIGN_FLAG_TLS1_RSA
 * flag with gnutls_privkey_sign_hash().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 * negative error value.
 *
 * Since: 3.1.10
 **/
int
gnutls_privkey_sign_raw_data(gnutls_privkey_t key,
			     unsigned flags,
			     const gnutls_datum_t * data,
			     gnutls_datum_t * signature)
{
	switch (key->type) {
#ifdef ENABLE_OPENPGP
	case GNUTLS_PRIVKEY_OPENPGP:
		return gnutls_openpgp_privkey_sign_hash(key->key.openpgp,
							data, signature);
#endif
#ifdef ENABLE_PKCS11
	case GNUTLS_PRIVKEY_PKCS11:
		return _gnutls_pkcs11_privkey_sign_hash(key->key.pkcs11,
							data, signature);
#endif
	case GNUTLS_PRIVKEY_X509:
		return _gnutls_pk_sign(key->key.x509->pk_algorithm,
				       signature, data, &key->key.x509->params);
	case GNUTLS_PRIVKEY_EXT:
		if (key->key.ext.sign_func == NULL)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
		return key->key.ext.sign_func(key, key->key.ext.userdata,
					      data, signature);
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}
}

/**
 * gnutls_privkey_decrypt_data:
 * @key: Holds the key
 * @flags: zero for now
 * @ciphertext: holds the data to be decrypted
 * @plaintext: will contain the decrypted data, allocated with gnutls_malloc()
 *
 * This function will decrypt the given data using the algorithm
 * supported by the private key.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 * negative error value.
 *
 * Since: 2.12.0
 **/
int
gnutls_privkey_decrypt_data(gnutls_privkey_t key,
			    unsigned int flags,
			    const gnutls_datum_t * ciphertext,
			    gnutls_datum_t * plaintext)
{
	switch (key->type) {
#ifdef ENABLE_OPENPGP
	case GNUTLS_PRIVKEY_OPENPGP:
		return _gnutls_openpgp_privkey_decrypt_data(key->key.openpgp,
							    flags, ciphertext,
							    plaintext);
#endif
	case GNUTLS_PRIVKEY_X509:
		return _gnutls_pk_decrypt(key->pk_algorithm, plaintext,
					  ciphertext, &key->key.x509->params);
#ifdef ENABLE_PKCS11
	case GNUTLS_PRIVKEY_PKCS11:
		return _gnutls_pkcs11_privkey_decrypt_data(key->key.pkcs11,
							   flags,
							   ciphertext,
							   plaintext);
#endif
	case GNUTLS_PRIVKEY_EXT:
		if (key->key.ext.decrypt_func == NULL)
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

		return key->key.ext.decrypt_func(key,
						 key->key.ext.userdata,
						 ciphertext, plaintext);
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}
}

/**
 * gnutls_privkey_import_x509_raw:
 * @pkey: The private key
 * @data: The private key data to be imported
 * @format: The format of the private key
 * @password: A password (optional)
 * @flags: an ORed sequence of gnutls_pkcs_encrypt_flags_t
 *
 * This function will import the given private key to the abstract
 * #gnutls_privkey_t structure. 
 *
 * The supported formats are basic unencrypted key, PKCS8, PKCS12, 
 * and the openssl format.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1.0
 **/
int gnutls_privkey_import_x509_raw(gnutls_privkey_t pkey,
				   const gnutls_datum_t * data,
				   gnutls_x509_crt_fmt_t format,
				   const char *password, unsigned int flags)
{
	gnutls_x509_privkey_t xpriv;
	int ret;

	ret = gnutls_x509_privkey_init(&xpriv);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = gnutls_x509_privkey_import2(xpriv, data, format, password, flags);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret =
	    gnutls_privkey_import_x509(pkey, xpriv,
				       GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	return 0;

 cleanup:
	gnutls_x509_privkey_deinit(xpriv);

	return ret;
}

/**
 * gnutls_privkey_import_url:
 * @key: A key of type #gnutls_privkey_t
 * @url: A PKCS 11 url
 * @flags: should be zero
 *
 * This function will import a PKCS11 or TPM URL as a
 * private key. The supported URL types can be checked
 * using gnutls_url_is_supported().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1.0
 **/
int
gnutls_privkey_import_url(gnutls_privkey_t key, const char *url,
			  unsigned int flags)
{
	if (strncmp(url, "pkcs11:", 7) == 0)
#ifdef ENABLE_PKCS11
		return gnutls_privkey_import_pkcs11_url(key, url);
#else
		return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
#endif

	if (strncmp(url, "tpmkey:", 7) == 0)
#ifdef HAVE_TROUSERS
		return gnutls_privkey_import_tpm_url(key, url, NULL, NULL, 0);
#else
		return gnutls_assert_val(GNUTLS_E_UNIMPLEMENTED_FEATURE);
#endif

	return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
}

/**
 * gnutls_privkey_set_pin_function:
 * @key: A key of type #gnutls_privkey_t
 * @fn: the callback
 * @userdata: data associated with the callback
 *
 * This function will set a callback function to be used when
 * required to access the object. This function overrides any other
 * global PIN functions.
 *
 * Note that this function must be called right after initialization
 * to have effect.
 *
 * Since: 3.1.0
 *
 **/
void gnutls_privkey_set_pin_function(gnutls_privkey_t key,
				     gnutls_pin_callback_t fn, void *userdata)
{
	key->pin.cb = fn;
	key->pin.data = userdata;
}

/**
 * gnutls_privkey_status:
 * @key: Holds the key
 *
 * Checks the status of the private key token. This function
 * is an actual wrapper over gnutls_pkcs11_privkey_status(), and
 * if the private key is a PKCS #11 token it will check whether
 * it is inserted or not.
 *
 * Returns: this function will return non-zero if the token 
 * holding the private key is still available (inserted), and zero otherwise.
 * 
 * Since: 3.1.10
 *
 **/
int gnutls_privkey_status(gnutls_privkey_t key)
{
	switch (key->type) {
#ifdef ENABLE_PKCS11
	case GNUTLS_PRIVKEY_PKCS11:
		return gnutls_pkcs11_privkey_status(key->key.pkcs11);
#endif
	default:
		return 1;
	}
}

/**
 * gnutls_privkey_verify_params:
 * @key: should contain a #gnutls_privkey_t structure
 *
 * This function will verify the private key parameters.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_privkey_verify_params(gnutls_privkey_t key)
{
	gnutls_pk_params_st params;
	int ret;

	gnutls_pk_params_init(&params);

	ret = _gnutls_privkey_get_mpis(key, &params);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = _gnutls_pk_verify_priv_params(key->pk_algorithm, &params);

	gnutls_pk_params_release(&params);

	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}
