/*
 * Copyright (C) 2014 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
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

/* Parts copied from GnuTLS example programs. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "utils.h"

/* Test for name constraints PKIX extension.
 */

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "<%d>| %s", level, str);
}

static unsigned char cert_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEMTCCAxmgAwIBAgIBADANBgkqhkiG9w0BAQUFADCBlTELMAkGA1UEBhMCR1Ix\n"
    "RDBCBgNVBAoTO0hlbGxlbmljIEFjYWRlbWljIGFuZCBSZXNlYXJjaCBJbnN0aXR1\n"
    "dGlvbnMgQ2VydC4gQXV0aG9yaXR5MUAwPgYDVQQDEzdIZWxsZW5pYyBBY2FkZW1p\n"
    "YyBhbmQgUmVzZWFyY2ggSW5zdGl0dXRpb25zIFJvb3RDQSAyMDExMB4XDTExMTIw\n"
    "NjEzNDk1MloXDTMxMTIwMTEzNDk1MlowgZUxCzAJBgNVBAYTAkdSMUQwQgYDVQQK\n"
    "EztIZWxsZW5pYyBBY2FkZW1pYyBhbmQgUmVzZWFyY2ggSW5zdGl0dXRpb25zIENl\n"
    "cnQuIEF1dGhvcml0eTFAMD4GA1UEAxM3SGVsbGVuaWMgQWNhZGVtaWMgYW5kIFJl\n"
    "c2VhcmNoIEluc3RpdHV0aW9ucyBSb290Q0EgMjAxMTCCASIwDQYJKoZIhvcNAQEB\n"
    "BQADggEPADCCAQoCggEBAKlTAOMupvaO+mDYLZU++CwqVE7NuYRhlFhPjz2L5EPz\n"
    "dYmNUeTDN9KKiE15HrcS3UN4SoqS5tdI1Q+kOilENbgH9mgdVc04UfCMJDGFr4PJ\n"
    "fel3r+0ae50X+bOdOFAPplp5kYCvN66m0zH7tSYJnTxa71HFK9+WXesyHgLacEns\n"
    "bgzImjeN9/E2YEsmLIKe0HjzDQ9jpFEw4fkrJxIH2Oq9GGKYsFk3fb7u8yBRQlqD\n"
    "75O6aRXxYp2fmTmCobd0LovUxQt7L/DICto9eQqakxylKHJzkUOap9FNhYS5qXSP\n"
    "FEDH3N6sQWRstBmbAmNtJGSPRLIl6s5ddAxjMlyNh+UCAwEAAaOBiTCBhjAPBgNV\n"
    "HRMBAf8EBTADAQH/MAsGA1UdDwQEAwIBBjAdBgNVHQ4EFgQUppFC/RNhSiOeCKQp\n"
    "5dgTBCPuQSUwRwYDVR0eBEAwPqA8MAWCAy5ncjAFggMuZXUwBoIELmVkdTAGggQu\n"
    "b3JnMAWBAy5ncjAFgQMuZXUwBoEELmVkdTAGgQQub3JnMA0GCSqGSIb3DQEBBQUA\n"
    "A4IBAQAf73lB4XtuP7KMhjdCSk4cNx6NZrokgclPEg8hwAOXhiVtXdMiKahsog2p\n"
    "6z0GW5k6x8zDmjR/qw7IThzh+uTczQ2+vyT+bOdrwg3IBp5OjWEopmr95fZi6hg8\n"
    "TqBTnbI6nOulnJEWtk2C4AwFSKls9cz4y51JtPACpf1wA+2KIaWuE4ZJwzNzvoc7\n"
    "dIsXRSZMFpGD/md9zU1jZ/rzAxKWeAaNsWftjj++n08C9bMJL/NMh98qy5V8Acys\n"
    "Nnq/onN694/BtZqhFLKPM58N7yLcZnuEvUUXBj08yrl3NI/K6s8/MT7jiOOASSXI\n"
    "l7WdmplNsDz4SgCbZN2fOUvRJ9e4\n"
    "-----END CERTIFICATE-----\n";

const gnutls_datum_t cert = { cert_pem, sizeof(cert_pem) };

const gnutls_datum_t name1 = { (void*)"com", 3 };
const gnutls_datum_t name2 = { (void*)"example.com", sizeof("example.com")-1 };
const gnutls_datum_t name3 = { (void*)"another.example.com", sizeof("another.example.com")-1 };

const gnutls_datum_t mail1 = { (void*)"example.com", sizeof("example.com")-1 };
const gnutls_datum_t mail2 = { (void*)".example.net", sizeof(".example.net")-1 };
const gnutls_datum_t mail3 = { (void*)"nmav@redhat.com", sizeof("nmav@redhat.com")-1 };
const gnutls_datum_t mail4 = { (void*)"koko.example.net", sizeof("koko.example.net")-1 };

void doit(void)
{
	int ret;
	unsigned int crit, i;
	gnutls_x509_crt_t crt;
	gnutls_x509_name_constraints_t nc;
	unsigned type;
	gnutls_datum_t name;

	/* this must be called once in the program
	 */
	global_init();

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(6);

	/* 0: test the reading of name constraints */

	ret = gnutls_x509_name_constraints_init(&nc);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_crt_init(&crt);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_crt_import(crt, &cert, GNUTLS_X509_FMT_PEM);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_crt_get_name_constraints(crt, nc, 0, &crit);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	if (crit != 0) {
		fail("error reading criticality\n");
	}

	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_permitted(nc, i++, &type, &name);

		if (ret >= 0 && i == 2) {
			if (name.size != 3 || memcmp(name.data, ".eu", 3) != 0) {
				fail("error reading 2nd constraint\n");
			}
		}
	} while(ret == 0);

	if (i-1 != 8) {
		fail("Could not read all contraints; read %d, expected %d\n", i-1, 8);
	}

	gnutls_x509_name_constraints_deinit(nc);
	gnutls_x509_crt_deinit(crt);

	/* 1: test the generation of name constraints */

	ret = gnutls_x509_name_constraints_init(&nc);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_crt_init(&crt);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_crt_import(crt, &cert, GNUTLS_X509_FMT_PEM);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_permitted(nc, GNUTLS_SAN_DNSNAME,
		&name1);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_excluded(nc, GNUTLS_SAN_DNSNAME,
		&name2);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_excluded(nc, GNUTLS_SAN_DNSNAME,
		&name3);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_excluded(nc, GNUTLS_SAN_URI,
		&name3);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_permitted(nc, GNUTLS_SAN_RFC822NAME,
		&mail1);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_permitted(nc, GNUTLS_SAN_RFC822NAME,
		&mail2);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_permitted(nc, GNUTLS_SAN_RFC822NAME,
		&mail3);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_name_constraints_add_excluded(nc, GNUTLS_SAN_RFC822NAME,
		&mail4);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	ret = gnutls_x509_crt_set_name_constraints(crt, nc, 1);
	if (ret < 0)
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(ret));

	/* 2: test the reading of the generated constraints */

	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_permitted(nc, i++, &type, &name);

		if (ret >= 0 && i == 1) {
			if (name.size != name1.size || memcmp(name.data, name1.data, name1.size) != 0) {
				fail("%d: error reading 1st constraint\n", __LINE__);
			}
		}
	} while(ret == 0);

	if (i-1 != 4) {
		fail("Could not read all contraints; read %d, expected %d\n", i-1, 4);
	}

	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_excluded(nc, i++, &type, &name);

		if (ret >= 0 && i == 1) {
			if (name.size != name2.size || memcmp(name.data, name2.data, name2.size) != 0) {
				fail("%d: error reading 1st excluded constraint\n", __LINE__);
			}
		}
		if (ret >= 0 && i == 2) {
			if (name.size != name3.size || memcmp(name.data, name3.data, name3.size) != 0) {
				fail("%d: error reading 1st excluded constraint\n", __LINE__);
			}
		}
	} while(ret == 0);

	if (i-1 != 4) {
		fail("Could not read all excluded contraints; read %d, expected %d\n", i-1, 4);
	}

	/* 3: test the name constraints check function */

	/* This name constraints structure doesn't have any excluded GNUTLS_SAN_DN so
	 * this test should succeed */
	name.data = (unsigned char*)"ASFHAJHjhafjs";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_DN, &name);
	if (ret == 0)
		fail("Checking DN should have succeeded\n");

	/* Test e-mails */
	name.data = (unsigned char*)"nmav@redhat.com";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME, &name);
	if (ret == 0)
		fail("Checking email should have succeeded\n");

	name.data = (unsigned char*)"nmav@radhat.com";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME, &name);
	if (ret != 0)
		fail("Checking email should have failed\n");

	name.data = (unsigned char*)"nmav@example.com";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME, &name);
	if (ret == 0)
		fail("Checking email should have succeeded\n");

	name.data = (unsigned char*)"nmav@test.example.net";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME, &name);
	if (ret == 0)
		fail("Checking email should have succeeded\n");

	name.data = (unsigned char*)"nmav@example.net";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME, &name);
	if (ret != 0)
		fail("Checking email should have failed\n");

	name.data = (unsigned char*)"nmav@koko.example.net";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME, &name);
	if (ret != 0)
		fail("Checking email should have failed\n");

	/* This name constraints structure does have an excluded URI so
	 * this test should fail */
	name.data = (unsigned char*)"http://www.com";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_URI, &name);
	if (ret != 0)
		fail("Checking URI should have failed\n");

	name.data = (unsigned char*)"goodexample.com";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_DNSNAME, &name);
	if (ret == 0)
		fail("Checking %s should have succeeded\n", name.data);

	name.data = (unsigned char*)"good.com";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_DNSNAME, &name);
	if (ret == 0)
		fail("Checking %s should have succeeded\n", name.data);

	name.data = (unsigned char*)"www.example.com";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_DNSNAME, &name);
	if (ret != 0)
		fail("Checking %s should have failed\n", name.data);

	name.data = (unsigned char*)"www.example.net";
	name.size = strlen((char*)name.data);
	ret = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_DNSNAME, &name);
	if (ret != 0)
		fail("Checking %s should have failed\n", name.data);

	gnutls_x509_name_constraints_deinit(nc);
	gnutls_x509_crt_deinit(crt);

	gnutls_global_deinit();

	if (debug)
		success("success");
}
