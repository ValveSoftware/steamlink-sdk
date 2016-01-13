/*
 * Copyright (C) 2008-2014 Free Software Foundation, Inc.
 *
 * Author: Simon Josefsson, Nikos Mavrogiannopoulos
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "../utils.h"
#include "../test-chains.h"

#define URL "pkcs11:model=SoftHSM;manufacturer=SoftHSM;serial=1;token=test"
#define CONFIG "softhsm.config"

#define DEFAULT_THEN 1256803113
static time_t then = DEFAULT_THEN;

/* GnuTLS internally calls time() to find out the current time when
   verifying certificates.  To avoid a time bomb, we hard code the
   current time.  This should work fine on systems where the library
   call to time is resolved at run-time.  */
static time_t mytime(time_t * t)
{
	if (t)
		*t = then;

	return then;
}

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "|<%d>| %s", level, str);
}

static
int pin_func(void* userdata, int attempt, const char* url, const char *label,
		unsigned flags, char *pin, size_t pin_max)
{
	if (attempt == 0) {
		strcpy(pin, "1234");
		return 0;
	}
	return -1;
}

#define LIB1 "/usr/lib64/softhsm/libsofthsm.so"
#define LIB2 "/usr/lib/softhsm/libsofthsm.so"

void doit(void)
{
	int exit_val = 0;
	size_t i;
	int ret;
	FILE *fp;
	const char *lib;

	unsetenv("SOFTHSM_CONF");
	/* The overloading of time() seems to work in linux (ELF?)
	 * systems only. Disable it on windows.
	 */
#ifdef _WIN32
	exit(77);
#endif

	if (access("/usr/bin/softhsm", X_OK) < 0) {
		fprintf(stderr, "cannot find softhsm binary\n");
		exit(77);
	}

	if (access(LIB1, R_OK) == 0) {
		lib = LIB1;
	} else if (access(LIB2, R_OK) == 0) {
		lib = LIB2;
	} else {
		fprintf(stderr, "cannot find softhsm module\n");
		exit(77);
	}


	ret = global_init();
	if (ret != 0) {
		fail("%d: %s\n", ret, gnutls_strerror(ret));
		exit(1);
	}

	gnutls_pkcs11_set_pin_function(pin_func, NULL);
	gnutls_global_set_time_function(mytime);
	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	/* write softhsm.config */
	fp = fopen(CONFIG, "w");
	if (fp == NULL) {
		fprintf(stderr, "error writing softhsm.config\n");
		exit(1);
	}
	fputs("0:./softhsm.db\n", fp);
	fclose(fp);

	setenv("SOFTHSM_CONF", CONFIG, 0);

	system("softhsm --init-token --slot 0 --label test --so-pin 1234 --pin 1234");

	ret = gnutls_pkcs11_add_provider(lib, "trusted");
	if (ret < 0) {
		fprintf(stderr, "gnutls_x509_crt_init: %s\n",
			gnutls_strerror(ret));
		exit(1);
	}

	for (i = 0; chains[i].chain; i++) {
		gnutls_x509_trust_list_t tl;
		unsigned int verify_status;
		gnutls_x509_crt_t certs[MAX_CHAIN];
		gnutls_x509_crt_t ca;
		gnutls_datum_t tmp;
		size_t j;

		if (debug)
			printf("Chain '%s' (%d)...\n", chains[i].name,
			       (int) i);

		for (j = 0; chains[i].chain[j]; j++) {
			if (debug > 2)
				printf("\tAdding certificate %d...",
				       (int) j);

			ret = gnutls_x509_crt_init(&certs[j]);
			if (ret < 0) {
				fprintf(stderr,
					"gnutls_x509_crt_init[%d,%d]: %s\n",
					(int) i, (int) j,
					gnutls_strerror(ret));
				exit(1);
			}

			tmp.data = (unsigned char *) chains[i].chain[j];
			tmp.size = strlen(chains[i].chain[j]);

			ret =
			    gnutls_x509_crt_import(certs[j], &tmp,
						   GNUTLS_X509_FMT_PEM);
			if (debug > 2)
				printf("done\n");
			if (ret < 0) {
				fprintf(stderr,
					"gnutls_x509_crt_import[%s,%d]: %s\n",
					chains[i].name, (int) j,
					gnutls_strerror(ret));
				exit(1);
			}

			gnutls_x509_crt_print(certs[j],
					      GNUTLS_CRT_PRINT_ONELINE,
					      &tmp);
			if (debug)
				printf("\tCertificate %d: %.*s\n", (int) j,
				       tmp.size, tmp.data);
			gnutls_free(tmp.data);
		}

		if (debug > 2)
			printf("\tAdding CA certificate...");

		ret = gnutls_x509_crt_init(&ca);
		if (ret < 0) {
			fprintf(stderr, "gnutls_x509_crt_init: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		tmp.data = (unsigned char *) *chains[i].ca;
		tmp.size = strlen(*chains[i].ca);

		ret =
		    gnutls_x509_crt_import(ca, &tmp, GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			fprintf(stderr, "gnutls_x509_crt_import: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		if (debug > 2)
			printf("done\n");

		gnutls_x509_crt_print(ca, GNUTLS_CRT_PRINT_ONELINE, &tmp);
		if (debug)
			printf("\tCA Certificate: %.*s\n", tmp.size,
			       tmp.data);
		gnutls_free(tmp.data);

		if (debug)
			printf("\tVerifying...");

		/* initialize softhsm token */
		ret = gnutls_pkcs11_token_init(URL, "1234", "test");
		if (ret < 0) {
			fail("gnutls_pkcs11_token_init\n");
			exit(1);
		}

		/* write CA certificate to softhsm */
		ret = gnutls_pkcs11_copy_x509_crt(URL, ca, "test-ca", GNUTLS_PKCS11_OBJ_FLAG_MARK_TRUSTED|
			GNUTLS_PKCS11_OBJ_FLAG_MARK_CA|
			GNUTLS_PKCS11_OBJ_FLAG_LOGIN_SO);
		if (ret < 0) {
			fail("gnutls_pkcs11_copy_x509_crt: %s\n", gnutls_strerror(ret));
			exit(1);
		}

		gnutls_x509_trust_list_init(&tl, 0);

		ret = gnutls_x509_trust_list_add_trust_file(tl, URL, NULL, 0, 0, 0);
		if (ret < 0) {
			fail("gnutls_x509_trust_list_add_trust_file: %s\n", gnutls_strerror(ret));
			exit(1);
		}

		if (ret < 1) {
			fail("gnutls_x509_trust_list_add_trust_file returned zero!\n");
			exit(1);
		}

		if (chains[i].expected_time != 0)
			then = chains[i].expected_time;
		else
			then = DEFAULT_THEN;

		/* make sure that the two functions don't diverge */
		ret = gnutls_x509_trust_list_verify_crt(tl, certs, j, chains[i].verify_flags,
						&verify_status, NULL);
		if (ret < 0) {
			fprintf(stderr,
				"gnutls_x509_crt_list_verify[%d,%d]: %s\n",
				(int) i, (int) j, gnutls_strerror(ret));
			exit(1);
		}

		if (verify_status != chains[i].expected_verify_result) {
			gnutls_datum_t out1, out2;
			gnutls_certificate_verification_status_print
			    (verify_status, GNUTLS_CRT_X509, &out1, 0);
			gnutls_certificate_verification_status_print(chains
								     [i].
								     expected_verify_result,
								     GNUTLS_CRT_X509,
								     &out2,
								     0);
			fail("chain[%s]:\nverify_status: %d: %s\nexpected: %d: %s\n", chains[i].name, verify_status, out1.data, chains[i].expected_verify_result, out2.data);
			gnutls_free(out1.data);
			gnutls_free(out2.data);

#if 0
			j = 0;
			do {
				fprintf(stderr, "%s\n",
					chains[i].chain[j]);
			}
			while (chains[i].chain[++j] != NULL);
#endif

			if (!debug)
				exit(1);
		} else if (debug)
			printf("done\n");

		if (debug)
			printf("\tCleanup...");

		gnutls_x509_trust_list_deinit(tl, 0);
		gnutls_x509_crt_deinit(ca);
		for (j = 0; chains[i].chain[j]; j++)
			gnutls_x509_crt_deinit(certs[j]);

		if (debug)
			printf("done\n\n\n");
	}

	gnutls_global_deinit();

	if (debug)
		printf("Exit status...%d\n", exit_val);
	remove(CONFIG);

	exit(exit_val);
}
