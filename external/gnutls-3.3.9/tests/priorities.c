/*
 * Copyright (C) 2012 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnutls/gnutls.h>

#include "utils.h"

#ifdef ENABLE_FIPS140
void doit(void)
{
	exit(77);
}

#else

static void
try_prio(const char *prio, unsigned expected_cs, unsigned expected_ciphers)
{
	int ret;
	gnutls_priority_t p;
	const char *err;
	const unsigned int *t;
	unsigned i, si, count = 0;

	/* this must be called once in the program
	 */
	global_init();

	ret = gnutls_priority_init(&p, prio, &err);
	if (ret < 0) {
		fprintf(stderr, "error: %s: %s\n", gnutls_strerror(ret),
			err);
		exit(1);
	}

	for (i = 0;; i++) {
		ret = gnutls_priority_get_cipher_suite_index(p, i, &si);
		if (ret == GNUTLS_E_UNKNOWN_CIPHER_SUITE)
			continue;
		else if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			break;
		else if (ret == 0) {
			count++;
			/* fprintf(stderr, "%s\n", gnutls_cipher_suite_info(si, NULL, NULL, NULL, NULL, NULL)); */
		}

	}

	ret = gnutls_priority_cipher_list(p, &t);
	if ((unsigned) ret != expected_ciphers) {
#if 0
		for (i = 0; i < ret; i++)
			fprintf(stderr, "%s\n",
				gnutls_cipher_get_name(t[i]));
#endif
		fail("expected %d ciphers, found %d\n", expected_ciphers,
		     ret);
		exit(1);
	}

	gnutls_priority_deinit(p);

	/* fprintf(stderr, "count: %d\n", count); */

	if (debug)
		success("finished: %s\n", prio);

	if (count != expected_cs) {
		fail("expected %d ciphersuites, found %d\n", expected_cs,
		     count);
		exit(1);
	}
}

void doit(void)
{
	const int normal = 66;
	const int null = 5;
	const int sec128 = 56;

	try_prio("NORMAL", normal, 10);
	try_prio("NORMAL:-MAC-ALL:+MD5:+MAC-ALL", normal, 10);
	try_prio("NORMAL:+CIPHER-ALL", normal, 10);	/* all (except null) */
	try_prio("NORMAL:-CIPHER-ALL:+NULL", null, 1);	/* null */
	try_prio("NORMAL:-CIPHER-ALL:+NULL:+CIPHER-ALL", normal + null, 11);	/* should be null + all */
	try_prio("NORMAL:-CIPHER-ALL:+NULL:+CIPHER-ALL:-CIPHER-ALL:+AES-128-CBC", 10, 1);	/* should be null + all */
	try_prio("PERFORMANCE", normal, 10);
	try_prio("SECURE256", 20, 4);
	try_prio("SECURE128", sec128, 8);
	try_prio("SECURE128:+SECURE256", sec128, 8);	/* should be the same as SECURE128 */
	try_prio("SECURE128:+SECURE256:+NORMAL", normal, 10);	/* should be the same as NORMAL */
	try_prio("SUITEB192", 1, 1);
}

#endif
