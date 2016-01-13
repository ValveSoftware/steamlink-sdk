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

#ifndef X509_H
#define X509_H

#include <gnutls/x509.h>
#include <gnutls/x509-ext.h>
#include <gnutls/abstract.h>

#include <libtasn1.h>

#define MAX_CRQ_EXTENSIONS_SIZE 8*1024
#define MAX_OID_SIZE 128
#define MAX_KEY_ID_SIZE 128

#define HASH_OID_SHA1 "1.3.14.3.2.26"
#define HASH_OID_MD5 "1.2.840.113549.2.5"
#define HASH_OID_MD2 "1.2.840.113549.2.2"
#define HASH_OID_RMD160 "1.3.36.3.2.1"
#define HASH_OID_SHA224 "2.16.840.1.101.3.4.2.4"
#define HASH_OID_SHA256 "2.16.840.1.101.3.4.2.1"
#define HASH_OID_SHA384 "2.16.840.1.101.3.4.2.2"
#define HASH_OID_SHA512 "2.16.840.1.101.3.4.2.3"

struct gnutls_x509_crl_iter {
	/* This is used to optimize reads by gnutls_x509_crl_iter_crt_serial() */
	ASN1_TYPE rcache;
	unsigned rcache_idx;
};

typedef struct gnutls_x509_crl_int {
	ASN1_TYPE crl;

	unsigned expanded;
	/* This is used to optimize reads by gnutls_x509_crl_get_crt_serial2() */
	ASN1_TYPE rcache;
	unsigned rcache_idx;
	int use_extensions;

	gnutls_datum_t der;
	gnutls_datum_t raw_issuer_dn;
} gnutls_x509_crl_int;

typedef struct gnutls_x509_crt_int {
	ASN1_TYPE cert;
	int use_extensions;
	int expanded;		/* a certificate has been expanded */

	/* These two cached values allow fast calls to
	 * get_raw_*_dn(). */
	gnutls_datum_t raw_dn;
	gnutls_datum_t raw_issuer_dn;
	gnutls_datum_t raw_spki;

	gnutls_datum_t der;
	struct pin_info_st pin;
} gnutls_x509_crt_int;

typedef struct gnutls_x509_crq_int {
	ASN1_TYPE crq;
} gnutls_x509_crq_int;

typedef struct gnutls_pkcs7_int {
	ASN1_TYPE pkcs7;
	unsigned expanded;
} gnutls_pkcs7_int;

typedef struct gnutls_x509_privkey_int {
	/* the size of params depends on the public
	 * key algorithm
	 */
	gnutls_pk_params_st params;

	gnutls_pk_algorithm_t pk_algorithm;
	unsigned expanded;

	ASN1_TYPE key;
} gnutls_x509_privkey_int;

int _gnutls_x509_crt_cpy(gnutls_x509_crt_t dest, gnutls_x509_crt_t src);

int _gnutls_x509_compare_raw_dn(const gnutls_datum_t * dn1,
				const gnutls_datum_t * dn2);

int _gnutls_x509_crl_cpy(gnutls_x509_crl_t dest, gnutls_x509_crl_t src);
int _gnutls_x509_crl_get_raw_issuer_dn(gnutls_x509_crl_t crl,
				       gnutls_datum_t * dn);

/* sign.c */
int _gnutls_x509_get_tbs(ASN1_TYPE cert, const char *tbs_name,
			 gnutls_datum_t * tbs);
int _gnutls_x509_pkix_sign(ASN1_TYPE src, const char *src_name,
			   gnutls_digest_algorithm_t,
			   gnutls_x509_crt_t issuer,
			   gnutls_privkey_t issuer_key);

/* dn.c */
#define OID_X520_COUNTRY_NAME		"2.5.4.6"
#define OID_X520_ORGANIZATION_NAME	"2.5.4.10"
#define OID_X520_ORGANIZATIONAL_UNIT_NAME "2.5.4.11"
#define OID_X520_COMMON_NAME		"2.5.4.3"
#define OID_X520_LOCALITY_NAME		"2.5.4.7"
#define OID_X520_STATE_OR_PROVINCE_NAME	"2.5.4.8"
#define OID_LDAP_DC			"0.9.2342.19200300.100.1.25"
#define OID_LDAP_UID			"0.9.2342.19200300.100.1.1"
#define OID_PKCS9_EMAIL			"1.2.840.113549.1.9.1"

int _gnutls_x509_parse_dn(ASN1_TYPE asn1_struct,
			  const char *asn1_rdn_name, char *buf,
			  size_t * sizeof_buf);

int
_gnutls_x509_get_dn(ASN1_TYPE asn1_struct,
		    const char *asn1_rdn_name, gnutls_datum_t * dn);

int
_gnutls_x509_parse_dn_oid(ASN1_TYPE asn1_struct,
			  const char *asn1_rdn_name,
			  const char *given_oid, int indx,
			  unsigned int raw_flag, gnutls_datum_t * out);

int _gnutls_x509_set_dn_oid(ASN1_TYPE asn1_struct,
			    const char *asn1_rdn_name, const char *oid,
			    int raw_flag, const char *name,
			    int sizeof_name);

int _gnutls_x509_get_dn_oid(ASN1_TYPE asn1_struct,
			    const char *asn1_rdn_name,
			    int indx, void *_oid, size_t * sizeof_oid);

int _gnutls_parse_general_name(ASN1_TYPE src, const char *src_name,
			       int seq, void *name, size_t * name_size,
			       unsigned int *ret_type, int othername_oid);

int
_gnutls_parse_general_name2(ASN1_TYPE src, const char *src_name,
			   int seq, gnutls_datum_t *dname, 
			   unsigned int *ret_type, int othername_oid);

int
_gnutls_write_new_general_name(ASN1_TYPE ext, const char *ext_name,
		       gnutls_x509_subject_alt_name_t type,
		       const void *data, unsigned int data_size);

/* dsa.c */


/* verify.c */
int gnutls_x509_crt_is_issuer(gnutls_x509_crt_t cert,
			      gnutls_x509_crt_t issuer);

int
_gnutls_x509_verify_algorithm(gnutls_digest_algorithm_t * hash,
			      const gnutls_datum_t * signature,
			      gnutls_pk_algorithm_t pk,
			      gnutls_pk_params_st * issuer_params);

int _gnutls_x509_verify_data(const mac_entry_st * me,
			     const gnutls_datum_t * data,
			     const gnutls_datum_t * signature,
			     gnutls_x509_crt_t issuer);

/* privkey.h */
void _gnutls_x509_privkey_reinit(gnutls_x509_privkey_t key);

ASN1_TYPE _gnutls_privkey_decode_pkcs1_rsa_key(const gnutls_datum_t *
					       raw_key,
					       gnutls_x509_privkey_t pkey);
int _gnutls_privkey_decode_ecc_key(ASN1_TYPE* pkey_asn, const gnutls_datum_t *
					 raw_key,
					 gnutls_x509_privkey_t pkey,
					 gnutls_ecc_curve_t curve);

int
_gnutls_x509_read_ecc_params(uint8_t * der, int dersize,
			     unsigned int *curve);

int _gnutls_asn1_encode_privkey(gnutls_pk_algorithm_t pk, ASN1_TYPE * c2,
				gnutls_pk_params_st * params);

/* extensions.c */
int _gnutls_x509_crl_get_extension_oid(gnutls_x509_crl_t crl,
				       int indx, void *oid,
				       size_t * sizeof_oid);

int _gnutls_x509_crl_set_extension(gnutls_x509_crl_t crl,
				   const char *ext_id,
				   const gnutls_datum_t * ext_data,
				   unsigned int critical);

int
_gnutls_x509_crl_get_extension(gnutls_x509_crl_t crl,
			       const char *extension_id, int indx,
			       gnutls_datum_t * data,
			       unsigned int *critical);

int
_gnutls_x509_crt_get_extension(gnutls_x509_crt_t cert,
			       const char *extension_id, int indx,
			       gnutls_datum_t * data, unsigned int *critical);

int _gnutls_x509_crt_get_extension_oid(gnutls_x509_crt_t cert,
				       int indx, void *ret,
				       size_t * ret_size);
int _gnutls_x509_crt_set_extension(gnutls_x509_crt_t cert,
				   const char *extension_id,
				   const gnutls_datum_t * ext_data,
				   unsigned int critical);

int
_gnutls_x509_ext_extract_number(uint8_t * number,
				size_t * nr_size,
				uint8_t * extnValue, int extnValueLen);
int
_gnutls_x509_ext_gen_number(const uint8_t * nuber, size_t nr_size,
			    gnutls_datum_t * der_ext);


int
_gnutls_write_general_name(ASN1_TYPE ext, const char *ext_name,
		       gnutls_x509_subject_alt_name_t type,
		       const void *data, unsigned int data_size);

int _gnutls_x509_ext_gen_subject_alt_name(gnutls_x509_subject_alt_name_t
					  type, const void *data,
					  unsigned int data_size,
					  const gnutls_datum_t * prev_der_ext,
					  gnutls_datum_t * der_ext);
int _gnutls_x509_ext_gen_auth_key_id(const void *id, size_t id_size,
				     gnutls_datum_t * der_data);

/* mpi.c */
int _gnutls_x509_crq_get_mpis(gnutls_x509_crq_t cert,
			      gnutls_pk_params_st *);

int _gnutls_x509_crt_get_mpis(gnutls_x509_crt_t cert,
			      gnutls_pk_params_st * params);

int _gnutls_x509_read_pubkey_params(gnutls_pk_algorithm_t, uint8_t * der,
				    int dersize,
				    gnutls_pk_params_st * params);

int _gnutls_x509_read_pubkey(gnutls_pk_algorithm_t, uint8_t * der,
			     int dersize, gnutls_pk_params_st * params);

int _gnutls_x509_write_ecc_params(gnutls_ecc_curve_t curve,
				  gnutls_datum_t * der);
int _gnutls_x509_write_ecc_pubkey(gnutls_pk_params_st * params,
				  gnutls_datum_t * der);

int
_gnutls_x509_write_pubkey_params(gnutls_pk_algorithm_t algo,
				 gnutls_pk_params_st * params,
				 gnutls_datum_t * der);
int _gnutls_x509_write_pubkey(gnutls_pk_algorithm_t,
			      gnutls_pk_params_st * params,
			      gnutls_datum_t * der);

int _gnutls_x509_read_uint(ASN1_TYPE node, const char *value,
			   unsigned int *ret);

int _gnutls_x509_read_der_int(uint8_t * der, int dersize, bigint_t * out);

int _gnutls_x509_read_int(ASN1_TYPE node, const char *value,
			  bigint_t * ret_mpi);
int _gnutls_x509_write_int(ASN1_TYPE node, const char *value, bigint_t mpi,
			   int lz);

int _gnutls_x509_read_key_int(ASN1_TYPE node, const char *value,
			  bigint_t * ret_mpi);
int _gnutls_x509_write_key_int(ASN1_TYPE node, const char *value, bigint_t mpi,
			   int lz);

int _gnutls_x509_write_uint32(ASN1_TYPE node, const char *value,
			      uint32_t num);

int _gnutls_x509_write_sig_params(ASN1_TYPE dst, const char *dst_name,
				  gnutls_pk_algorithm_t pk_algorithm,
				  gnutls_digest_algorithm_t);

/* pkcs12.h */
#include <gnutls/pkcs12.h>

typedef struct gnutls_pkcs12_int {
	ASN1_TYPE pkcs12;
	unsigned expanded;
} gnutls_pkcs12_int;

#define MAX_BAG_ELEMENTS 32

struct bag_element {
	gnutls_datum_t data;
	gnutls_pkcs12_bag_type_t type;
	gnutls_datum_t local_key_id;
	char *friendly_name;
};

typedef struct gnutls_pkcs12_bag_int {
	struct bag_element element[MAX_BAG_ELEMENTS];
	int bag_elements;
} gnutls_pkcs12_bag_int;

#define BAG_PKCS8_KEY "1.2.840.113549.1.12.10.1.1"
#define BAG_PKCS8_ENCRYPTED_KEY "1.2.840.113549.1.12.10.1.2"
#define BAG_CERTIFICATE "1.2.840.113549.1.12.10.1.3"
#define BAG_CRL "1.2.840.113549.1.12.10.1.4"
#define BAG_SECRET "1.2.840.113549.1.12.10.1.5"

/* PKCS #7
 */
#define DATA_OID "1.2.840.113549.1.7.1"
#define ENC_DATA_OID "1.2.840.113549.1.7.6"

/* Bag attributes
 */
#define FRIENDLY_NAME_OID "1.2.840.113549.1.9.20"
#define KEY_ID_OID "1.2.840.113549.1.9.21"

int
_gnutls_pkcs12_string_to_key(const mac_entry_st * me,
			     unsigned int id, const uint8_t * salt,
			     unsigned int salt_size, unsigned int iter,
			     const char *pw, unsigned int req_keylen,
			     uint8_t * keybuf);

int _gnutls_pkcs7_decrypt_data(const gnutls_datum_t * data,
			       const char *password, gnutls_datum_t * dec);

typedef enum schema_id {
	PBES2_GENERIC,		/* when the algorithm is unknown, temporal use when reading only */
	PBES2_3DES,		/* the stuff in PKCS #5 */
	PBES2_AES_128,
	PBES2_AES_192,
	PBES2_AES_256,
	PKCS12_3DES_SHA1,	/* the stuff in PKCS #12 */
	PKCS12_ARCFOUR_SHA1,
	PKCS12_RC2_40_SHA1
} schema_id;

int _gnutls_pkcs_flags_to_schema(unsigned int flags);
int _gnutls_pkcs7_encrypt_data(schema_id schema,
			       const gnutls_datum_t * data,
			       const char *password, gnutls_datum_t * enc);
int _pkcs12_decode_safe_contents(const gnutls_datum_t * content,
				 gnutls_pkcs12_bag_t bag);

int
_pkcs12_encode_safe_contents(gnutls_pkcs12_bag_t bag, ASN1_TYPE * content,
			     int *enc);

int _pkcs12_decode_crt_bag(gnutls_pkcs12_bag_type_t type,
			   const gnutls_datum_t * in,
			   gnutls_datum_t * out);
int _pkcs12_encode_crt_bag(gnutls_pkcs12_bag_type_t type,
			   const gnutls_datum_t * raw,
			   gnutls_datum_t * out);

/* crq */
int _gnutls_x509_crq_set_extension(gnutls_x509_crq_t crq,
				   const char *ext_id,
				   const gnutls_datum_t * ext_data,
				   unsigned int critical);

unsigned int
_gnutls_verify_crt_status(const gnutls_x509_crt_t * certificate_list,
				int clist_size,
				const gnutls_x509_crt_t * trusted_cas,
				int tcas_size,
				unsigned int flags,
				gnutls_verify_output_function func);

#ifdef ENABLE_PKCS11
unsigned int
_gnutls_pkcs11_verify_crt_status(const char* url,
				const gnutls_x509_crt_t * certificate_list,
				unsigned clist_size,
				const char *purpose,
				unsigned int flags,
				gnutls_verify_output_function func);
#endif

int
_gnutls_x509_crt_check_revocation(gnutls_x509_crt_t cert,
				  const gnutls_x509_crl_t * crl_list,
				  int crl_list_length,
				  gnutls_verify_output_function func);

typedef struct gnutls_name_constraints_st {
	struct name_constraints_node_st * permitted;
	struct name_constraints_node_st * excluded;
} gnutls_name_constraints_st;

typedef struct name_constraints_node_st {
	unsigned type;
	gnutls_datum_t name;
	struct name_constraints_node_st *next;
} name_constraints_node_st;

int _gnutls_extract_name_constraints(ASN1_TYPE c2, const char *vstr,
				    name_constraints_node_st ** _nc);

void _gnutls_x509_policies_erase(gnutls_x509_policies_t policies, unsigned int seq);

#endif
