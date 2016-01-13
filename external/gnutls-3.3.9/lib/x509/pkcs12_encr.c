/* minip12.c - A mini pkcs-12 implementation (modified for gnutls)
 *
 * Copyright (C) 2002-2012 Free Software Foundation, Inc.
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

#include <gnutls_mpi.h>
#include <gnutls_errors.h>
#include <x509_int.h>
#include <c-ctype.h>
#include <algorithms.h>

/* Returns 0 if the password is ok, or a negative error
 * code instead.
 */
static int _pkcs12_check_pass(const char *pass, size_t plen)
{
	unsigned int i;

	for (i = 0; i < plen; i++) {
		if (c_isascii(pass[i]))
			continue;
		return GNUTLS_E_INVALID_PASSWORD;
	}

	return 0;
}

#define MAX_PASS_LEN 128

/* ID should be:
 * 3 for MAC
 * 2 for IV
 * 1 for encryption key
 *
 * Note that this function produces different key for the
 * NULL password, and for the password with zero length.
 */
int
_gnutls_pkcs12_string_to_key(const mac_entry_st * me,
			     unsigned int id, const uint8_t * salt,
			     unsigned int salt_size, unsigned int iter,
			     const char *pw, unsigned int req_keylen,
			     uint8_t * keybuf)
{
	int rc;
	unsigned int i, j;
	digest_hd_st md;
	bigint_t num_b1 = NULL, num_ij = NULL;
	bigint_t mpi512 = NULL;
	unsigned int pwlen;
	uint8_t hash[MAX_HASH_SIZE], buf_b[64], buf_i[MAX_PASS_LEN * 2 + 64], *p;
	uint8_t d[64];
	size_t cur_keylen;
	size_t n, m, p_size, i_size;
	unsigned mac_len;
	const uint8_t buf_512[] =	/* 2^64 */
	{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00
	};

	cur_keylen = 0;

	if (pw == NULL)
		pwlen = 0;
	else
		pwlen = strlen(pw);

	if (pwlen > MAX_PASS_LEN) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	if ((rc = _pkcs12_check_pass(pw, pwlen)) < 0) {
		gnutls_assert();
		return rc;
	}

	rc = _gnutls_mpi_init_scan(&mpi512, buf_512, sizeof(buf_512));
	if (rc < 0) {
		gnutls_assert();
		return rc;
	}

	/* Store salt and password in BUF_I */
	p_size = ((pwlen / 64) * 64) + 64;

	if (p_size > sizeof(buf_i) - 64)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	p = buf_i;
	for (i = 0; i < 64; i++)
		*p++ = salt[i % salt_size];
	if (pw) {
		for (i = j = 0; i < p_size; i += 2) {
			*p++ = 0;
			*p++ = pw[j];
			if (++j > pwlen)	/* Note, that we include the trailing (0) */
				j = 0;
		}
	} else
		memset(p, 0, p_size);

	i_size = 64 + p_size;
	mac_len = _gnutls_mac_get_algo_len(me);

	for (;;) {
		rc = _gnutls_hash_init(&md, me);
		if (rc < 0) {
			gnutls_assert();
			goto cleanup;
		}
		memset(d, id & 0xff, 64);
		_gnutls_hash(&md, d, 64);
		_gnutls_hash(&md, buf_i, pw ? i_size : 64);
		_gnutls_hash_deinit(&md, hash);
		for (i = 1; i < iter; i++) {
			rc = _gnutls_hash_fast(me->id, hash, mac_len,
					       hash);
			if (rc < 0) {
				gnutls_assert();
				goto cleanup;
			}
		}
		for (i = 0; i < mac_len && cur_keylen < req_keylen; i++)
			keybuf[cur_keylen++] = hash[i];
		if (cur_keylen == req_keylen) {
			rc = 0;	/* ready */
			goto cleanup;
		}

		/* need more bytes. */
		for (i = 0; i < 64; i++)
			buf_b[i] = hash[i % mac_len];
		n = 64;
		rc = _gnutls_mpi_init_scan(&num_b1, buf_b, n);
		if (rc < 0) {
			gnutls_assert();
			goto cleanup;
		}

		rc = _gnutls_mpi_add_ui(num_b1, num_b1, 1);
		if (rc < 0) {
			gnutls_assert();
			goto cleanup;
		}
		
		for (i = 0; i < 128; i += 64) {
			n = 64;
			rc = _gnutls_mpi_init_scan(&num_ij, buf_i + i, n);
			if (rc < 0) {
				gnutls_assert();
				goto cleanup;
			}

			rc = _gnutls_mpi_addm(num_ij, num_ij, num_b1, mpi512);
			if (rc < 0) {
				gnutls_assert();
				goto cleanup;
			}

			n = 64;
#ifndef PKCS12_BROKEN_KEYGEN
			m = (_gnutls_mpi_get_nbits(num_ij) + 7) / 8;
#else
			m = n;
#endif
			memset(buf_i + i, 0, n - m);
			rc = _gnutls_mpi_print(num_ij, buf_i + i + n - m,
					       &n);
			if (rc < 0) {
				gnutls_assert();
				goto cleanup;
			}
			_gnutls_mpi_release(&num_ij);
		}
	}
      cleanup:
	_gnutls_mpi_release(&num_ij);
	_gnutls_mpi_release(&num_b1);
	_gnutls_mpi_release(&mpi512);

	return rc;
}
