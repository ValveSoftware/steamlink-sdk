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

#ifndef PKCS11_INT_H
#define PKCS11_INT_H

#ifdef ENABLE_PKCS11

#define CRYPTOKI_GNU
#include <p11-kit/pkcs11.h>
#include <gnutls/pkcs11.h>
#include <x509/x509_int.h>

#define PKCS11_ID_SIZE 128
#define PKCS11_LABEL_SIZE 128

#include <p11-kit/uri.h>
typedef unsigned char ck_bool_t;

struct pkcs11_session_info {
	struct ck_function_list *module;
	struct ck_token_info tinfo;
	ck_session_handle_t pks;
	unsigned int init;
};

struct token_info {
	struct ck_token_info tinfo;
	struct ck_slot_info sinfo;
	ck_slot_id_t sid;
	struct gnutls_pkcs11_provider_st *prov;
};

struct gnutls_pkcs11_obj_st {
	gnutls_datum_t raw;
	gnutls_pkcs11_obj_type_t type;
	unsigned int flags;
	struct p11_kit_uri *info;

	/* only when pubkey */
	gnutls_datum_t pubkey[MAX_PUBLIC_PARAMS_SIZE];
	gnutls_pk_algorithm_t pk_algorithm;
	unsigned int key_usage;

	struct pin_info_st pin;
};

/* This must be called on every function that uses a PKCS #11 function
 * directly */
int _gnutls_pkcs11_check_init(void);

#define PKCS11_CHECK_INIT \
	ret = _gnutls_pkcs11_check_init(); \
	if (ret < 0) \
		return gnutls_assert_val(ret)

#define PKCS11_CHECK_INIT_RET(x) \
	ret = _gnutls_pkcs11_check_init(); \
	if (ret < 0) \
		return gnutls_assert_val(x)

/* thus function is called for every token in the traverse_tokens
 * function. Once everything is traversed it is called with NULL tinfo.
 * It should return 0 if found what it was looking for.
 */
typedef int (*find_func_t) (struct pkcs11_session_info *,
			    struct token_info * tinfo, struct ck_info *,
			    void *input);

int pkcs11_rv_to_err(ck_rv_t rv);
int pkcs11_url_to_info(const char *url, struct p11_kit_uri **info);
int
pkcs11_find_slot(struct ck_function_list **module, ck_slot_id_t * slot,
		 struct p11_kit_uri *info, struct token_info *_tinfo);

int pkcs11_read_pubkey(struct ck_function_list *module,
		       ck_session_handle_t pks, ck_object_handle_t obj,
		       ck_key_type_t key_type, gnutls_datum_t * pubkey);

int pkcs11_override_cert_exts(struct pkcs11_session_info *sinfo, gnutls_datum_t *spki, gnutls_datum_t *der);

int pkcs11_get_info(struct p11_kit_uri *info,
		    gnutls_pkcs11_obj_info_t itype, void *output,
		    size_t * output_size);
int pkcs11_login(struct pkcs11_session_info *sinfo,
		 struct pin_info_st *pin_info,
		 const struct token_info *tokinfo,
		 struct p11_kit_uri *info, int so);

int pkcs11_call_token_func(struct p11_kit_uri *info, const unsigned retry);

extern gnutls_pkcs11_token_callback_t _gnutls_token_func;
extern void *_gnutls_token_data;

void pkcs11_rescan_slots(void);
int pkcs11_info_to_url(struct p11_kit_uri *info,
		       gnutls_pkcs11_url_type_t detailed, char **url);

#define SESSION_WRITE (1<<0)
#define SESSION_LOGIN (1<<1)
#define SESSION_SO (1<<2)	/* security officer session */
#define SESSION_TRUSTED (1<<3) /* session on a marked as trusted (p11-kit) module */
int pkcs11_open_session(struct pkcs11_session_info *sinfo,
			struct pin_info_st *pin_info,
			struct p11_kit_uri *info, unsigned int flags);
int _pkcs11_traverse_tokens(find_func_t find_func, void *input,
			    struct p11_kit_uri *info,
			    struct pin_info_st *pin_info,
			    unsigned int flags);
ck_object_class_t pkcs11_strtype_to_class(const char *type);

int pkcs11_token_matches_info(struct p11_kit_uri *info,
			      struct ck_token_info *tinfo,
			      struct ck_info *lib_info);

unsigned int pkcs11_obj_flags_to_int(unsigned int flags);

int
_gnutls_pkcs11_privkey_sign_hash(gnutls_pkcs11_privkey_t key,
				 const gnutls_datum_t * hash,
				 gnutls_datum_t * signature);

int
_gnutls_pkcs11_privkey_decrypt_data(gnutls_pkcs11_privkey_t key,
				    unsigned int flags,
				    const gnutls_datum_t * ciphertext,
				    gnutls_datum_t * plaintext);

int
_pkcs11_privkey_get_pubkey (gnutls_pkcs11_privkey_t pkey, gnutls_pubkey_t *pub, unsigned flags);

static inline int pk_to_mech(gnutls_pk_algorithm_t pk)
{
	if (pk == GNUTLS_PK_DSA)
		return CKM_DSA;
	else if (pk == GNUTLS_PK_EC)
		return CKM_ECDSA;
	else
		return CKM_RSA_PKCS;
}

static inline gnutls_pk_algorithm_t mech_to_pk(ck_key_type_t m)
{
	if (m == CKK_RSA)
		return GNUTLS_PK_RSA;
	else if (m == CKK_DSA)
		return GNUTLS_PK_DSA;
	else if (m == CKK_ECDSA)
		return GNUTLS_PK_EC;
	else
		return GNUTLS_PK_UNKNOWN;
}

static inline int pk_to_genmech(gnutls_pk_algorithm_t pk, ck_key_type_t *type)
{
	if (pk == GNUTLS_PK_DSA) {
		*type = CKK_DSA;
		return CKM_DSA_KEY_PAIR_GEN;
	} else if (pk == GNUTLS_PK_EC) {
		*type = CKK_ECDSA;
		return CKM_ECDSA_KEY_PAIR_GEN;
	} else {
		*type = CKK_RSA;
		return CKM_RSA_PKCS_KEY_PAIR_GEN;
	}
}

ck_rv_t
pkcs11_generate_key_pair(struct ck_function_list * module,
			 ck_session_handle_t sess,
			 struct ck_mechanism * mechanism,
			 struct ck_attribute * pub_templ,
			 unsigned long pub_templ_count,
			 struct ck_attribute * priv_templ,
			 unsigned long priv_templ_count,
			 ck_object_handle_t * pub,
			 ck_object_handle_t * priv);

ck_rv_t
pkcs11_get_slot_list(struct ck_function_list *module,
		     unsigned char token_present,
		     ck_slot_id_t * slot_list, unsigned long *count);

ck_rv_t
pkcs11_get_module_info(struct ck_function_list *module,
		       struct ck_info *info);

ck_rv_t
pkcs11_get_slot_info(struct ck_function_list *module,
		     ck_slot_id_t slot_id, struct ck_slot_info *info);

ck_rv_t
pkcs11_get_token_info(struct ck_function_list *module,
		      ck_slot_id_t slot_id, struct ck_token_info *info);

ck_rv_t
pkcs11_find_objects_init(struct ck_function_list *module,
			 ck_session_handle_t sess,
			 struct ck_attribute *templ, unsigned long count);

ck_rv_t
pkcs11_find_objects(struct ck_function_list *module,
		    ck_session_handle_t sess,
		    ck_object_handle_t * objects,
		    unsigned long max_object_count,
		    unsigned long *object_count);

ck_rv_t pkcs11_find_objects_final(struct pkcs11_session_info *);

ck_rv_t pkcs11_close_session(struct pkcs11_session_info *);

ck_rv_t
pkcs11_get_attribute_value(struct ck_function_list *module,
			   ck_session_handle_t sess,
			   ck_object_handle_t object,
			   struct ck_attribute *templ,
			   unsigned long count);

ck_rv_t
pkcs11_get_attribute_avalue(struct ck_function_list * module,
			   ck_session_handle_t sess,
			   ck_object_handle_t object,
			   ck_attribute_type_t type,
			   gnutls_datum_t *res);

ck_rv_t
pkcs11_get_mechanism_list(struct ck_function_list *module,
			  ck_slot_id_t slot_id,
			  ck_mechanism_type_t * mechanism_list,
			  unsigned long *count);

ck_rv_t
pkcs11_sign_init(struct ck_function_list *module,
		 ck_session_handle_t sess,
		 struct ck_mechanism *mechanism, ck_object_handle_t key);

ck_rv_t
pkcs11_sign(struct ck_function_list *module,
	    ck_session_handle_t sess,
	    unsigned char *data,
	    unsigned long data_len,
	    unsigned char *signature, unsigned long *signature_len);

ck_rv_t
pkcs11_decrypt_init(struct ck_function_list *module,
		    ck_session_handle_t sess,
		    struct ck_mechanism *mechanism,
		    ck_object_handle_t key);

ck_rv_t
pkcs11_decrypt(struct ck_function_list *module,
	       ck_session_handle_t sess,
	       unsigned char *encrypted_data,
	       unsigned long encrypted_data_len,
	       unsigned char *data, unsigned long *data_len);

ck_rv_t
pkcs11_create_object(struct ck_function_list *module,
		     ck_session_handle_t sess,
		     struct ck_attribute *templ,
		     unsigned long count, ck_object_handle_t * object);

ck_rv_t
pkcs11_destroy_object(struct ck_function_list *module,
		      ck_session_handle_t sess, ck_object_handle_t object);

ck_rv_t
pkcs11_init_token(struct ck_function_list *module,
		  ck_slot_id_t slot_id, unsigned char *pin,
		  unsigned long pin_len, unsigned char *label);

ck_rv_t
pkcs11_init_pin(struct ck_function_list *module,
		ck_session_handle_t sess,
		unsigned char *pin, unsigned long pin_len);

ck_rv_t
pkcs11_set_pin(struct ck_function_list *module,
	       ck_session_handle_t sess,
	       const char *old_pin,
	       unsigned long old_len,
	       const char *new_pin, unsigned long new_len);

ck_rv_t
_gnutls_pkcs11_get_random(struct ck_function_list *module,
		  ck_session_handle_t sess, void *data, size_t len);


const char *pkcs11_strerror(ck_rv_t rv);

#endif				/* ENABLE_PKCS11 */

#endif
