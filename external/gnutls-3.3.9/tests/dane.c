/*
 * Copyright (C) 2014 Red Hat
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gnutls/gnutls.h>
#include <gnutls/dane.h>

#include "utils.h"

char * const data_entries[] = {
	(char*)"\x00\x00\x01\x19\x40\x0b\xe5\xb7\xa3\x1f\xb7\x33\x91\x77\x00\x78\x9d\x2f\x0a\x24\x71\xc0\xc9\xd5\x06\xc0\xe5\x04\xc0\x6c\x16\xd7\xcb\x17\xc0",
	(char*)"\x03\x00\x01\x03\x32\xaa\x2d\x58\xb3\xe0\x54\x4b\x65\x65\x64\x38\x93\x70\x68\xba\x44\xce\x2f\x14\x46\x9c\x4f\x50\xc9\xcc\x69\x33\xc8\x08\xd3",
	(char*)"\x03\x01\x01\x46\x25\x73\x19\x5c\x86\xe8\x61\xab\xab\x8e\xcc\xfb\xc7\xf0\x48\x69\x58\xef\xdf\xf9\x44\x9a\xc1\x07\x29\xb3\xa0\xf9\x06\xf3\x88",
	NULL
};

int data_entries_size[] = {
	35,
	35,
	35,
	0
};

static void dane_raw_check(void)
{
	dane_state_t s;
	dane_query_t r;
	int ret;
	unsigned int entries, i;
	char **r_data;
	int *r_data_len;
	int secure;
	int bogus;

	ret = dane_state_init(&s, DANE_F_IGNORE_LOCAL_RESOLVER);
	if (ret < 0) {
		fail("dane_state_init: %s\n", dane_strerror(ret));
	}

	ret = dane_raw_tlsa(s, &r, data_entries, data_entries_size, 1, 0);
	if (ret < 0) {
		fail("%d: %s\n", __LINE__, dane_strerror(ret));
	}

	ret = dane_query_to_raw_tlsa(r, &entries, &r_data, &r_data_len, &secure, &bogus);
	if (ret < 0) {
		fail("%d: %s\n", __LINE__, dane_strerror(ret));
	}

	if (entries != 3)
		fail("%d: %s\n", __LINE__, dane_strerror(ret));

	if (secure != 1)
		fail("%d: %s\n", __LINE__, dane_strerror(ret));

	if (bogus != 0)
		fail("%d: %s\n", __LINE__, dane_strerror(ret));

	for (i=0;i<entries;i++) {
		if (r_data_len[i] != data_entries_size[i])
			fail("%d: %s\n", __LINE__, dane_strerror(ret));

		if (memcmp(r_data[i], data_entries[i], r_data_len[i]) != 0)
			fail("%d: %s\n", __LINE__, dane_strerror(ret));
	}

	gnutls_free(r_data);
	gnutls_free(r_data_len);

	dane_query_deinit(r);
	dane_state_deinit(s);
}

void doit(void)
{
	int ret;

	ret = global_init();
	if (ret < 0) {
		fail("global_init\n");
		exit(1);
	}

	dane_raw_check();

	/* we're done */

	gnutls_global_deinit();
}
