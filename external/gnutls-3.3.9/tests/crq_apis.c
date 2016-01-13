/*
 * Copyright (C) 2008-2012 Free Software Foundation, Inc.
 *
 * Author: Simon Josefsson
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "utils.h"

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s |<%d>| %s", "crq_key_id", level, str);
}

static unsigned char key_pem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIICXAIBAAKBgQC7ZkP18sXXtozMxd/1iDuxyUtqDqGtIFBACIChT1yj0Phsz+Y8\n"
    "9+wEdhMXi2SJIlvA3VN8O+18BLuAuSi+jpvGjqClEsv1Vx6i57u3M0mf47tKrmpN\n"
    "aP/JEeIyjc49gAuNde/YAIGPKAQDoCKNYQQH+rY3fSEHSdIJYWmYkKNYqQIDAQAB\n"
    "AoGADpmARG5CQxS+AesNkGmpauepiCz1JBF/JwnyiX6vEzUh0Ypd39SZztwrDxvF\n"
    "PJjQaKVljml1zkJpIDVsqvHdyVdse8M+Qn6hw4x2p5rogdvhhIL1mdWo7jWeVJTF\n"
    "RKB7zLdMPs3ySdtcIQaF9nUAQ2KJEvldkO3m/bRJFEp54k0CQQDYy+RlTmwRD6hy\n"
    "7UtMjR0H3CSZJeQ8svMCxHLmOluG9H1UKk55ZBYfRTsXniqUkJBZ5wuV1L+pR9EK\n"
    "ca89a+1VAkEA3UmBelwEv2u9cAU1QjKjmwju1JgXbrjEohK+3B5y0ESEXPAwNQT9\n"
    "TrDM1m9AyxYTWLxX93dI5QwNFJtmbtjeBQJARSCWXhsoaDRG8QZrCSjBxfzTCqZD\n"
    "ZXtl807ymCipgJm60LiAt0JLr4LiucAsMZz6+j+quQbSakbFCACB8SLV1QJBAKZQ\n"
    "YKf+EPNtnmta/rRKKvySsi3GQZZN+Dt3q0r094XgeTsAqrqujVNfPhTMeP4qEVBX\n"
    "/iVX2cmMTSh3w3z8MaECQEp0XJWDVKOwcTW6Ajp9SowtmiZ3YDYo1LF9igb4iaLv\n"
    "sWZGfbnU3ryjvkb6YuFjgtzbZDZHWQCo8/cOtOBmPdk=\n"
    "-----END RSA PRIVATE KEY-----\n";
const gnutls_datum_t key = { key_pem, sizeof(key_pem) };

void doit(void)
{
	gnutls_x509_privkey_t pkey;
	gnutls_x509_crt_t crt;
	gnutls_x509_crq_t crq;
	const char *err = NULL;

	gnutls_datum_t out;

	size_t s = 0;

	char smallbuf[10];

	int ret;

	ret = global_init();
	if (ret < 0)
		fail("global_init\n");

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = gnutls_x509_crq_init(&crq);
	if (ret != 0)
		fail("gnutls_x509_crq_init\n");

	ret = gnutls_x509_privkey_init(&pkey);
	if (ret != 0)
		fail("gnutls_x509_privkey_init\n");

	ret = gnutls_x509_crt_init(&crt);
	if (ret != 0)
		fail("gnutls_x509_crt_init\n");

	ret = gnutls_x509_privkey_import(pkey, &key, GNUTLS_X509_FMT_PEM);
	if (ret != 0)
		fail("gnutls_x509_privkey_import\n");

	ret = gnutls_x509_crq_set_version(crq, 0);
	if (ret != 0)
		fail("gnutls_x509_crq_set_version\n");

	ret = gnutls_x509_crq_set_key(crq, pkey);
	if (ret != 0)
		fail("gnutls_x509_crq_set_key\n");

	s = 0;
	ret = gnutls_x509_crq_get_extension_info(crq, 0, NULL, &s, NULL);
	if (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
		fail("gnutls_x509_crq_get_extension_info\n");

	ret = gnutls_x509_crq_set_basic_constraints(crq, 0, 0);
	if (ret != 0)
		fail("gnutls_x509_crq_set_basic_constraints %d\n", ret);

	ret = gnutls_x509_crq_set_key_usage(crq, 0);
	if (ret != 0)
		fail("gnutls_x509_crq_set_key_usage %d\n", ret);

	ret = gnutls_x509_crq_get_challenge_password(crq, NULL, &s);
	if (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
		fail("%d: gnutls_x509_crq_get_challenge_password %d: %s\n",
		     __LINE__, ret, gnutls_strerror(ret));

	ret = gnutls_x509_crq_set_dn(crq, "cn = nikos, o = none to\\, mention", &err);
	if (ret < 0) {
		fail("gnutls_x509_crq_set_dn: %s, %s\n", gnutls_strerror(ret), err);
	}

	ret = gnutls_x509_crq_set_challenge_password(crq, "foo");
	if (ret != 0)
		fail("gnutls_x509_crq_set_challenge_password %d\n", ret);

	s = 0;
	ret = gnutls_x509_crq_get_challenge_password(crq, NULL, &s);
	if (ret != GNUTLS_E_SHORT_MEMORY_BUFFER || s != 4)
		fail("%d: gnutls_x509_crq_get_challenge_password %d: %s (passlen: %d)\n", __LINE__, ret, gnutls_strerror(ret), (int) s);

	s = 10;
	ret = gnutls_x509_crq_get_challenge_password(crq, smallbuf, &s);
	if (ret != 0 || s != 3 || strcmp(smallbuf, "foo") != 0)
		fail("%d: gnutls_x509_crq_get_challenge_password3 %d/%d/%s\n", __LINE__, ret, (int) s, smallbuf);

	s = 0;
	ret = gnutls_x509_crq_get_extension_info(crq, 0, NULL, &s, NULL);
	if (ret != 0)
		fail("gnutls_x509_crq_get_extension_info2\n");

	s = 0;
	ret = gnutls_x509_crq_get_extension_data(crq, 0, NULL, &s);
	if (ret != 0)
		fail("gnutls_x509_crq_get_extension_data\n");

	ret = gnutls_x509_crq_set_subject_alt_name(crq, GNUTLS_SAN_DNSNAME,
						   "foo", 3, 1);
	if (ret != 0)
		fail("gnutls_x509_crq_set_subject_alt_name\n");

	ret = gnutls_x509_crq_set_subject_alt_name(crq, GNUTLS_SAN_DNSNAME,
						   "bar", 3, 1);
	if (ret != 0)
		fail("gnutls_x509_crq_set_subject_alt_name\n");

	ret = gnutls_x509_crq_set_subject_alt_name(crq, GNUTLS_SAN_DNSNAME,
						   "apa", 3, 0);
	if (ret != 0)
		fail("gnutls_x509_crq_set_subject_alt_name\n");

	ret = gnutls_x509_crq_set_subject_alt_name(crq, GNUTLS_SAN_DNSNAME,
						   "foo", 3, 1);
	if (ret != 0)
		fail("gnutls_x509_crq_set_subject_alt_name\n");

	s = 0;
	ret = gnutls_x509_crq_get_key_purpose_oid(crq, 0, NULL, &s, NULL);
	if (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
		fail("gnutls_x509_crq_get_key_purpose_oid %d\n", ret);

	s = 0;
	ret =
	    gnutls_x509_crq_set_key_purpose_oid(crq,
						GNUTLS_KP_TLS_WWW_SERVER,
						0);
	if (ret != 0)
		fail("gnutls_x509_crq_set_key_purpose_oid %d\n", ret);

	s = 0;
	ret = gnutls_x509_crq_get_key_purpose_oid(crq, 0, NULL, &s, NULL);
	if (ret != GNUTLS_E_SHORT_MEMORY_BUFFER)
		fail("gnutls_x509_crq_get_key_purpose_oid %d\n", ret);

	s = 0;
	ret =
	    gnutls_x509_crq_set_key_purpose_oid(crq,
						GNUTLS_KP_TLS_WWW_CLIENT,
						1);
	if (ret != 0)
		fail("gnutls_x509_crq_set_key_purpose_oid2 %d\n", ret);

	ret = gnutls_x509_crq_print(crq, GNUTLS_CRT_PRINT_FULL, &out);
	if (ret != 0)
		fail("gnutls_x509_crq_print\n");
	if (debug)
		printf("crq: %.*s\n", out.size, out.data);
	gnutls_free(out.data);

	ret = gnutls_x509_crq_sign2(crq, pkey, GNUTLS_DIG_SHA256, 0);
	if (ret < 0)
		fail("gnutls_x509_crq_sign2: %s\n", gnutls_strerror(ret));

	ret = gnutls_x509_crt_set_crq(crt, crq);
	if (ret != 0)
		fail("gnutls_x509_crt_set_crq: %s\n", gnutls_strerror(ret));

	ret = gnutls_x509_crt_set_issuer_dn(crt, "cn = my CA, o = big\\, and one", &err);
	if (ret < 0) {
		fail("gnutls_x509_crt_set_issuer_dn: %s, %s\n", gnutls_strerror(ret), err);
	}

	ret = gnutls_x509_crt_set_version(crt, 3);
	if (ret != 0)
		fail("gnutls_x509_crt_set_version\n");

	ret = gnutls_x509_crt_set_crq_extensions(crt, crq);
	if (ret != 0)
		fail("gnutls_x509_crt_set_crq_extensions\n");

	ret = gnutls_x509_crt_print(crt, GNUTLS_CRT_PRINT_FULL, &out);
	if (ret != 0)
		fail("gnutls_x509_crt_print\n");
	if (debug)
		printf("crt: %.*s\n", out.size, out.data);
	gnutls_free(out.data);

	ret = gnutls_x509_crt_get_raw_issuer_dn(crt, &out);
	if (ret < 0 || out.size == 0)
		fail("gnutls_x509_crt_get_raw_issuer_dn: %s\n", gnutls_strerror(ret));

	if (out.size != 41 ||
	    memcmp(out.data, "\x30\x27\x31\x0e\x30\x0c\x06\x03\x55\x04\x03\x13\x05\x6d\x79\x20\x43\x41\x31\x15\x30\x13\x06\x03\x55\x04\x0a\x13\x0c\x62\x69\x67\x2c\x20\x61\x6e\x64\x20\x6f\x6e\x65", 41) != 0) {
		fail("issuer DN comparison failed\n");
	}
	gnutls_free(out.data);

	ret = gnutls_x509_crt_get_raw_dn(crt, &out);
	if (ret < 0 || out.size == 0)
		fail("gnutls_x509_crt_get_raw_dn: %s\n", gnutls_strerror(ret));

	if (out.size != 45 ||
	    memcmp(out.data, "\x30\x2b\x31\x0e\x30\x0c\x06\x03\x55\x04\x03\x13\x05\x6e\x69\x6b\x6f\x73\x31\x19\x30\x17\x06\x03\x55\x04\x0a\x13\x10\x6e\x6f\x6e\x65\x20\x74\x6f\x2c\x20\x6d\x65\x6e\x74\x69\x6f\x6e", 45) != 0) {
		fail("DN comparison failed\n");
	}
	gnutls_free(out.data);

	gnutls_x509_crq_deinit(crq);
	gnutls_x509_crt_deinit(crt);
	gnutls_x509_privkey_deinit(pkey);

	gnutls_global_deinit();
}
