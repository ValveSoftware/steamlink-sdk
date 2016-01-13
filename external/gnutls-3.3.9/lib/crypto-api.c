/*
 * Copyright (C) 2000-2012 Free Software Foundation, Inc.
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
#include <gnutls_errors.h>
#include <gnutls_cipher_int.h>
#include <gnutls_datum.h>
#include <gnutls/crypto.h>
#include <algorithms.h>
#include <random.h>
#include <crypto.h>
#include <fips.h>

typedef struct api_cipher_hd_st {
	cipher_hd_st ctx_enc;
	cipher_hd_st ctx_dec;
} api_cipher_hd_st;

/**
 * gnutls_cipher_init:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @cipher: the encryption algorithm to use
 * @key: The key to be used for encryption
 * @iv: The IV to use (if not applicable set NULL)
 *
 * This function will initialize an context that can be used for
 * encryption/decryption of data. This will effectively use the
 * current crypto backend in use by gnutls or the cryptographic
 * accelerator in use.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int
gnutls_cipher_init(gnutls_cipher_hd_t * handle,
		   gnutls_cipher_algorithm_t cipher,
		   const gnutls_datum_t * key, const gnutls_datum_t * iv)
{
	api_cipher_hd_st *h;
	int ret;
	const cipher_entry_st* e;

	e = cipher_to_entry(cipher);
	if (e == NULL)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	*handle = gnutls_calloc(1, sizeof(api_cipher_hd_st));
	if (*handle == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	h = *handle;
	ret =
	    _gnutls_cipher_init(&h->ctx_enc, e, key,
				iv, 1);

	if (ret >= 0 && _gnutls_cipher_is_block(e) != 0)
		ret =
		    _gnutls_cipher_init(&h->ctx_dec, e, key, iv, 0);

	return ret;
}

/**
 * gnutls_cipher_tag:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @tag: will hold the tag
 * @tag_size: The length of the tag to return
 *
 * This function operates on authenticated encryption with
 * associated data (AEAD) ciphers and will return the
 * output tag.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 3.0
 **/
int
gnutls_cipher_tag(gnutls_cipher_hd_t handle, void *tag, size_t tag_size)
{
	api_cipher_hd_st *h = handle;

	if (_gnutls_cipher_is_aead(&h->ctx_enc) == 0)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	_gnutls_cipher_tag(&h->ctx_enc, tag, tag_size);

	return 0;
}

/**
 * gnutls_cipher_add_auth:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @text: the data to be authenticated
 * @text_size: The length of the data
 *
 * This function operates on authenticated encryption with
 * associated data (AEAD) ciphers and authenticate the
 * input data. This function can only be called once
 * and before any encryption operations.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 3.0
 **/
int
gnutls_cipher_add_auth(gnutls_cipher_hd_t handle, const void *text,
		       size_t text_size)
{
	api_cipher_hd_st *h = handle;

	if (_gnutls_cipher_is_aead(&h->ctx_enc) == 0)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	_gnutls_cipher_auth(&h->ctx_enc, text, text_size);

	return 0;
}

/**
 * gnutls_cipher_set_iv:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @iv: the IV to set
 * @ivlen: The length of the IV
 *
 * This function will set the IV to be used for the next
 * encryption block.
 *
 * Since: 3.0
 **/
void
gnutls_cipher_set_iv(gnutls_cipher_hd_t handle, void *iv, size_t ivlen)
{
	api_cipher_hd_st *h = handle;

	_gnutls_cipher_setiv(&h->ctx_enc, iv, ivlen);

	if (_gnutls_cipher_is_block(h->ctx_enc.e) != 0)
		_gnutls_cipher_setiv(&h->ctx_dec, iv, ivlen);
}

/**
 * gnutls_cipher_encrypt:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @text: the data to encrypt
 * @textlen: The length of data to encrypt
 *
 * This function will encrypt the given data using the algorithm
 * specified by the context.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int
gnutls_cipher_encrypt(gnutls_cipher_hd_t handle, void *text,
		      size_t textlen)
{
	api_cipher_hd_st *h = handle;

	return _gnutls_cipher_encrypt(&h->ctx_enc, text, textlen);
}

/**
 * gnutls_cipher_decrypt:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @ciphertext: the data to encrypt
 * @ciphertextlen: The length of data to encrypt
 *
 * This function will decrypt the given data using the algorithm
 * specified by the context.
 *
 * Note that in AEAD ciphers, this will not check the tag. You will
 * need to compare the tag sent with the value returned from gnutls_cipher_tag().
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int
gnutls_cipher_decrypt(gnutls_cipher_hd_t handle, void *ciphertext,
		      size_t ciphertextlen)
{
	api_cipher_hd_st *h = handle;

	if (_gnutls_cipher_is_block(h->ctx_enc.e) == 0)
		return _gnutls_cipher_decrypt(&h->ctx_enc, ciphertext,
					      ciphertextlen);
	else
		return _gnutls_cipher_decrypt(&h->ctx_dec, ciphertext,
					      ciphertextlen);
}

/**
 * gnutls_cipher_encrypt2:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @text: the data to encrypt
 * @textlen: The length of data to encrypt
 * @ciphertext: the encrypted data
 * @ciphertextlen: The available length for encrypted data
 *
 * This function will encrypt the given data using the algorithm
 * specified by the context.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.12.0
 **/
int
gnutls_cipher_encrypt2(gnutls_cipher_hd_t handle, const void *text,
		       size_t textlen, void *ciphertext,
		       size_t ciphertextlen)
{
	api_cipher_hd_st *h = handle;

	return _gnutls_cipher_encrypt2(&h->ctx_enc, text, textlen,
				       ciphertext, ciphertextlen);
}

/**
 * gnutls_cipher_decrypt2:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @ciphertext: the data to encrypt
 * @ciphertextlen: The length of data to encrypt
 * @text: the decrypted data
 * @textlen: The available length for decrypted data
 *
 * This function will decrypt the given data using the algorithm
 * specified by the context.
 *
 * Note that in AEAD ciphers, this will not check the tag. You will
 * need to compare the tag sent with the value returned from gnutls_cipher_tag().
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.12.0
 **/
int
gnutls_cipher_decrypt2(gnutls_cipher_hd_t handle, const void *ciphertext,
		       size_t ciphertextlen, void *text, size_t textlen)
{
	api_cipher_hd_st *h = handle;

	if (_gnutls_cipher_is_block(h->ctx_enc.e) == 0)
		return _gnutls_cipher_decrypt2(&h->ctx_enc, ciphertext,
					       ciphertextlen, text,
					       textlen);
	else
		return _gnutls_cipher_decrypt2(&h->ctx_dec, ciphertext,
					       ciphertextlen, text,
					       textlen);
}

/**
 * gnutls_cipher_deinit:
 * @handle: is a #gnutls_cipher_hd_t structure.
 *
 * This function will deinitialize all resources occupied by the given
 * encryption context.
 *
 * Since: 2.10.0
 **/
void gnutls_cipher_deinit(gnutls_cipher_hd_t handle)
{
	api_cipher_hd_st *h = handle;

	_gnutls_cipher_deinit(&h->ctx_enc);
	if (_gnutls_cipher_is_block(h->ctx_enc.e) != 0)
		_gnutls_cipher_deinit(&h->ctx_dec);
	gnutls_free(handle);
}


/* HMAC */

/**
 * gnutls_hmac_init:
 * @dig: is a #gnutls_hmac_hd_t structure.
 * @algorithm: the HMAC algorithm to use
 * @key: The key to be used for encryption
 * @keylen: The length of the key
 *
 * This function will initialize an context that can be used to
 * produce a Message Authentication Code (MAC) of data.  This will
 * effectively use the current crypto backend in use by gnutls or the
 * cryptographic accelerator in use.
 *
 * Note that despite the name of this function, it can be used
 * for other MAC algorithms than HMAC.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int
gnutls_hmac_init(gnutls_hmac_hd_t * dig,
		 gnutls_mac_algorithm_t algorithm,
		 const void *key, size_t keylen)
{
#ifdef ENABLE_FIPS140
	/* MD5 is only allowed internally for TLS */
	if (_gnutls_fips_mode_enabled() != 0 &&
		_gnutls_get_lib_state() != LIB_STATE_SELFTEST) {

		if (algorithm == GNUTLS_MAC_MD5)
			return gnutls_assert_val(GNUTLS_E_UNWANTED_ALGORITHM);
	}
#endif

	*dig = gnutls_malloc(sizeof(mac_hd_st));
	if (*dig == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	return _gnutls_mac_init(((mac_hd_st *) * dig),
				mac_to_entry(algorithm), key, keylen);
}

/**
 * gnutls_hmac_set_nonce:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @nonce: the data to set as nonce
 * @nonce_len: The length of data
 *
 * This function will set the nonce in the MAC algorithm.
 *
 * Since: 3.2.0
 **/
void
gnutls_hmac_set_nonce(gnutls_hmac_hd_t handle, const void *nonce,
		      size_t nonce_len)
{
	_gnutls_mac_set_nonce((mac_hd_st *) handle, nonce, nonce_len);
}

/**
 * gnutls_hmac:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @text: the data to hash
 * @textlen: The length of data to hash
 *
 * This function will hash the given data using the algorithm
 * specified by the context.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int gnutls_hmac(gnutls_hmac_hd_t handle, const void *text, size_t textlen)
{
	return _gnutls_mac((mac_hd_st *) handle, text, textlen);
}

/**
 * gnutls_hmac_output:
 * @handle: is a #gnutls_hmac_hd_t structure.
 * @digest: is the output value of the MAC
 *
 * This function will output the current MAC value
 * and reset the state of the MAC.
 *
 * Since: 2.10.0
 **/
void gnutls_hmac_output(gnutls_hmac_hd_t handle, void *digest)
{
	_gnutls_mac_output((mac_hd_st *) handle, digest);
}

/**
 * gnutls_hmac_deinit:
 * @handle: is a #gnutls_hmac_hd_t structure.
 * @digest: is the output value of the MAC
 *
 * This function will deinitialize all resources occupied by
 * the given hmac context.
 *
 * Since: 2.10.0
 **/
void gnutls_hmac_deinit(gnutls_hmac_hd_t handle, void *digest)
{
	_gnutls_mac_deinit((mac_hd_st *) handle, digest);
	gnutls_free(handle);
}

/**
 * gnutls_hmac_get_len:
 * @algorithm: the hmac algorithm to use
 *
 * This function will return the length of the output data
 * of the given hmac algorithm.
 *
 * Returns: The length or zero on error.
 *
 * Since: 2.10.0
 **/
int gnutls_hmac_get_len(gnutls_mac_algorithm_t algorithm)
{
	return _gnutls_mac_get_algo_len(mac_to_entry(algorithm));
}

/**
 * gnutls_hmac_fast:
 * @algorithm: the hash algorithm to use
 * @key: the key to use
 * @keylen: The length of the key
 * @text: the data to hash
 * @textlen: The length of data to hash
 * @digest: is the output value of the hash
 *
 * This convenience function will hash the given data and return output
 * on a single call.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int
gnutls_hmac_fast(gnutls_mac_algorithm_t algorithm,
		 const void *key, size_t keylen,
		 const void *text, size_t textlen, void *digest)
{
	return _gnutls_mac_fast(algorithm, key, keylen, text, textlen,
				digest);
}

/* HASH */

/**
 * gnutls_hash_init:
 * @dig: is a #gnutls_hash_hd_t structure.
 * @algorithm: the hash algorithm to use
 *
 * This function will initialize an context that can be used to
 * produce a Message Digest of data.  This will effectively use the
 * current crypto backend in use by gnutls or the cryptographic
 * accelerator in use.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int
gnutls_hash_init(gnutls_hash_hd_t * dig,
		 gnutls_digest_algorithm_t algorithm)
{
#ifdef ENABLE_FIPS140
	/* MD5 is only allowed internally for TLS */
	if (_gnutls_fips_mode_enabled() != 0 &&
		_gnutls_get_lib_state() != LIB_STATE_SELFTEST) {

		if (algorithm == GNUTLS_DIG_MD5)
			return gnutls_assert_val(GNUTLS_E_UNWANTED_ALGORITHM);
	}
#endif

	*dig = gnutls_malloc(sizeof(digest_hd_st));
	if (*dig == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	return _gnutls_hash_init(((digest_hd_st *) * dig),
				 hash_to_entry(algorithm));
}

/**
 * gnutls_hash:
 * @handle: is a #gnutls_cipher_hd_t structure.
 * @text: the data to hash
 * @textlen: The length of data to hash
 *
 * This function will hash the given data using the algorithm
 * specified by the context.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int gnutls_hash(gnutls_hash_hd_t handle, const void *text, size_t textlen)
{
	return _gnutls_hash((digest_hd_st *) handle, text, textlen);
}

/**
 * gnutls_hash_output:
 * @handle: is a #gnutls_hash_hd_t structure.
 * @digest: is the output value of the hash
 *
 * This function will output the current hash value
 * and reset the state of the hash.
 *
 * Since: 2.10.0
 **/
void gnutls_hash_output(gnutls_hash_hd_t handle, void *digest)
{
	_gnutls_hash_output((digest_hd_st *) handle, digest);
}

/**
 * gnutls_hash_deinit:
 * @handle: is a #gnutls_hash_hd_t structure.
 * @digest: is the output value of the hash
 *
 * This function will deinitialize all resources occupied by
 * the given hash context.
 *
 * Since: 2.10.0
 **/
void gnutls_hash_deinit(gnutls_hash_hd_t handle, void *digest)
{
	_gnutls_hash_deinit((digest_hd_st *) handle, digest);
	gnutls_free(handle);
}

/**
 * gnutls_hash_get_len:
 * @algorithm: the hash algorithm to use
 *
 * This function will return the length of the output data
 * of the given hash algorithm.
 *
 * Returns: The length or zero on error.
 *
 * Since: 2.10.0
 **/
int gnutls_hash_get_len(gnutls_digest_algorithm_t algorithm)
{
	return _gnutls_hash_get_algo_len(hash_to_entry(algorithm));
}

/**
 * gnutls_hash_fast:
 * @algorithm: the hash algorithm to use
 * @text: the data to hash
 * @textlen: The length of data to hash
 * @digest: is the output value of the hash
 *
 * This convenience function will hash the given data and return output
 * on a single call.
 *
 * Returns: Zero or a negative error code on error.
 *
 * Since: 2.10.0
 **/
int
gnutls_hash_fast(gnutls_digest_algorithm_t algorithm,
		 const void *text, size_t textlen, void *digest)
{
	return _gnutls_hash_fast(algorithm, text, textlen, digest);
}

/**
 * gnutls_key_generate:
 * @key: is a pointer to a #gnutls_datum_t which will contain a newly
 * created key.
 * @key_size: The number of bytes of the key.
 *
 * Generates a random key of @key_size bytes.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, or an
 * error code.
 *
 * Since: 3.0
 **/
int gnutls_key_generate(gnutls_datum_t * key, unsigned int key_size)
{
	int ret;

#ifdef ENABLE_FIPS140
	/* The FIPS140 approved RNGs are not allowed to be used
	 * to extract key sizes longer than their original seed.
	 */
	if (_gnutls_fips_mode_enabled() != 0 &&
	    key_size > FIPS140_RND_KEY_SIZE)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
#endif

	key->size = key_size;
	key->data = gnutls_malloc(key->size);
	if (!key->data) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	ret = _gnutls_rnd(GNUTLS_RND_RANDOM, key->data, key->size);
	if (ret < 0) {
		gnutls_assert();
		_gnutls_free_datum(key);
		return ret;
	}

	return 0;
}
