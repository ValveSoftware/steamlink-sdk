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

#ifndef GNUTLS_CIPHER_INT
#define GNUTLS_CIPHER_INT

#include <gnutls/crypto.h>
#include <crypto-backend.h>

extern int crypto_cipher_prio;
extern gnutls_crypto_cipher_st _gnutls_cipher_ops;

typedef int (*cipher_encrypt_func) (void *hd, const void *plaintext,
				    size_t, void *ciphertext, size_t);
typedef int (*cipher_decrypt_func) (void *hd, const void *ciphertext,
				    size_t, void *plaintext, size_t);
typedef void (*cipher_deinit_func) (void *hd);

typedef int (*cipher_auth_func) (void *hd, const void *data, size_t);
typedef int (*cipher_setiv_func) (void *hd, const void *iv, size_t);

typedef void (*cipher_tag_func) (void *hd, void *tag, size_t);

typedef struct {
	void *handle;
	const cipher_entry_st *e;
	cipher_encrypt_func encrypt;
	cipher_decrypt_func decrypt;
	cipher_auth_func auth;
	cipher_tag_func tag;
	cipher_setiv_func setiv;
	cipher_deinit_func deinit;
} cipher_hd_st;

int _gnutls_cipher_init(cipher_hd_st *, const cipher_entry_st * e,
			const gnutls_datum_t * key,
			const gnutls_datum_t * iv, int enc);

inline static void _gnutls_cipher_setiv(const cipher_hd_st * handle,
					const void *iv, size_t ivlen)
{
	handle->setiv(handle->handle, iv, ivlen);
}

inline static int
_gnutls_cipher_encrypt2(const cipher_hd_st * handle, const void *text,
			size_t textlen, void *ciphertext,
			size_t ciphertextlen)
{
	if (likely(handle != NULL && handle->handle != NULL)) {
		return handle->encrypt(handle->handle, text, textlen,
				       ciphertext, ciphertextlen);
	}

	return 0;
}

inline static int
_gnutls_cipher_decrypt2(const cipher_hd_st * handle,
			const void *ciphertext, size_t ciphertextlen,
			void *text, size_t textlen)
{
	if (likely(handle != NULL && handle->handle != NULL)) {
		return handle->decrypt(handle->handle, ciphertext,
				       ciphertextlen, text, textlen);
	}

	return 0;
}

inline static void _gnutls_cipher_deinit(cipher_hd_st * handle)
{
	if (likely(handle != NULL && handle->handle != NULL)) {
		handle->deinit(handle->handle);
		handle->handle = NULL;
	}
}

int _gnutls_cipher_exists(gnutls_cipher_algorithm_t cipher);

#define _gnutls_cipher_is_aead(h) _gnutls_cipher_algo_is_aead((h)->e)

/* returns the tag in AUTHENC ciphers */
inline static void _gnutls_cipher_tag(const cipher_hd_st * handle,
				      void *tag, size_t tag_size)
{
	if (likely(handle != NULL && handle->handle != NULL)) {
		handle->tag(handle->handle, tag, tag_size);
	}
}

/* Add auth data for AUTHENC ciphers
 */
inline static int _gnutls_cipher_auth(const cipher_hd_st * handle,
				      const void *text, size_t textlen)
{
	if (likely(handle != NULL && handle->handle != NULL)) {
		return handle->auth(handle->handle, text, textlen);
	}
	return GNUTLS_E_INTERNAL_ERROR;
}

#define _gnutls_cipher_encrypt(x,y,z) _gnutls_cipher_encrypt2(x,y,z,y,z)
#define _gnutls_cipher_decrypt(x,y,z) _gnutls_cipher_decrypt2(x,y,z,y,z)

/* auth_cipher API. Allows combining a cipher with a MAC.
 */

typedef struct {
	cipher_hd_st cipher;
	union {
		digest_hd_st dig;
		mac_hd_st mac;
	} mac;
	unsigned int is_mac:1;
	unsigned int ssl_hmac:1;
	unsigned int non_null:1;
	size_t tag_size;
} auth_cipher_hd_st;

int _gnutls_auth_cipher_init(auth_cipher_hd_st * handle,
			     const cipher_entry_st * e,
			     const gnutls_datum_t * cipher_key,
			     const gnutls_datum_t * iv,
			     const mac_entry_st * me,
			     const gnutls_datum_t * mac_key, int ssl_hmac,
			     int enc);

int _gnutls_auth_cipher_add_auth(auth_cipher_hd_st * handle,
				 const void *text, int textlen);

int _gnutls_auth_cipher_encrypt2_tag(auth_cipher_hd_st * handle,
				     const uint8_t * text, int textlen,
				     void *ciphertext, int ciphertextlen,
				     int pad_size);
int _gnutls_auth_cipher_decrypt2(auth_cipher_hd_st * handle,
				 const void *ciphertext, int ciphertextlen,
				 void *text, int textlen);
int _gnutls_auth_cipher_tag(auth_cipher_hd_st * handle, void *tag,
			    int tag_size);

inline static void _gnutls_auth_cipher_setiv(const auth_cipher_hd_st *
					     handle, const void *iv,
					     size_t ivlen)
{
	_gnutls_cipher_setiv(&handle->cipher, iv, ivlen);
}

inline static
int _gnutls_auth_cipher_set_mac_nonce(auth_cipher_hd_st * handle,
				      const void *nonce, int nonce_len)
{
	if (handle->is_mac && !handle->ssl_hmac)
		return _gnutls_mac_set_nonce(&handle->mac.mac, nonce,
					     nonce_len);
	else
		return 0;
}

inline static size_t _gnutls_auth_cipher_tag_len(auth_cipher_hd_st *
						 handle)
{
	return handle->tag_size;
}

#define _gnutls_auth_cipher_is_aead(h) _gnutls_cipher_is_aead(&(h)->cipher)

void _gnutls_auth_cipher_deinit(auth_cipher_hd_st * handle);


#endif				/* GNUTLS_CIPHER_INT */
