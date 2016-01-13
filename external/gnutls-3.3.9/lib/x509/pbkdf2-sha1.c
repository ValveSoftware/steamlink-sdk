/* gc-pbkdf2-sha1.c --- Password-Based Key Derivation Function a'la PKCS#5
   Copyright (C) 2002-2012 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>

*/

/* Written by Simon Josefsson.  The comments in this file are taken
   from RFC 2898.  */

#include <gnutls_int.h>
#include <gnutls_datum.h>
#include <gnutls_errors.h>
#include <gnutls_hash_int.h>
#include <pbkdf2-sha1.h>

/*
 * 5.2 PBKDF2
 *
 *  PBKDF2 applies a pseudorandom function (see Appendix B.1 for an
 *  example) to derive keys. The length of the derived key is essentially
 *  unbounded. (However, the maximum effective search space for the
 *  derived key may be limited by the structure of the underlying
 *  pseudorandom function. See Appendix B.1 for further discussion.)
 *  PBKDF2 is recommended for new applications.
 *
 *  PBKDF2 (P, S, c, dkLen)
 *
 *  Options:        PRF        underlying pseudorandom function (hLen
 *                             denotes the length in octets of the
 *                             pseudorandom function output)
 *
 *  Input:          P          password, an octet string (ASCII or UTF-8)
 *                  S          salt, an octet string
 *                  c          iteration count, a positive integer
 *                  dkLen      intended length in octets of the derived
 *                             key, a positive integer, at most
 *                             (2^32 - 1) * hLen
 *
 *  Output:         DK         derived key, a dkLen-octet string
 */

int
_gnutls_pbkdf2_sha1(const char *P, size_t Plen,
		    const unsigned char *S, size_t Slen,
		    unsigned int c, unsigned char *DK, size_t dkLen)
{
	unsigned int hLen = 20;
	char U[20];
	char T[20];
	unsigned int u;
	unsigned int l;
	unsigned int r;
	unsigned int i;
	unsigned int k;
	int rc;
	char *tmp;
	size_t tmplen = Slen + 4;

	if (c == 0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	if (dkLen == 0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}
	/*
	 *
	 *  Steps:
	 *
	 *     1. If dkLen > (2^32 - 1) * hLen, output "derived key too long" and
	 *        stop.
	 */

	if (dkLen > 4294967295U) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	/*
	 *     2. Let l be the number of hLen-octet blocks in the derived key,
	 *        rounding up, and let r be the number of octets in the last
	 *        block:
	 *
	 *                  l = CEIL (dkLen / hLen) ,
	 *                  r = dkLen - (l - 1) * hLen .
	 *
	 *        Here, CEIL (x) is the "ceiling" function, i.e. the smallest
	 *        integer greater than, or equal to, x.
	 */

	l = ((dkLen - 1) / hLen) + 1;
	r = dkLen - (l - 1) * hLen;

	/*
	 *     3. For each block of the derived key apply the function F defined
	 *        below to the password P, the salt S, the iteration count c, and
	 *        the block index to compute the block:
	 *
	 *                  T_1 = F (P, S, c, 1) ,
	 *                  T_2 = F (P, S, c, 2) ,
	 *                  ...
	 *                  T_l = F (P, S, c, l) ,
	 *
	 *        where the function F is defined as the exclusive-or sum of the
	 *        first c iterates of the underlying pseudorandom function PRF
	 *        applied to the password P and the concatenation of the salt S
	 *        and the block index i:
	 *
	 *                  F (P, S, c, i) = U_1 \xor U_2 \xor ... \xor U_c
	 *
	 *        where
	 *
	 *                  U_1 = PRF (P, S || INT (i)) ,
	 *                  U_2 = PRF (P, U_1) ,
	 *                  ...
	 *                  U_c = PRF (P, U_{c-1}) .
	 *
	 *        Here, INT (i) is a four-octet encoding of the integer i, most
	 *        significant octet first.
	 *
	 *     4. Concatenate the blocks and extract the first dkLen octets to
	 *        produce a derived key DK:
	 *
	 *                  DK = T_1 || T_2 ||  ...  || T_l<0..r-1>
	 *
	 *     5. Output the derived key DK.
	 *
	 *  Note. The construction of the function F follows a "belt-and-
	 *  suspenders" approach. The iterates U_i are computed recursively to
	 *  remove a degree of parallelism from an opponent; they are exclusive-
	 *  ored together to reduce concerns about the recursion degenerating
	 *  into a small set of values.
	 *
	 */

	tmp = gnutls_malloc(tmplen);
	if (tmp == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	memcpy(tmp, S, Slen);

	for (i = 1; i <= l; i++) {
		memset(T, 0, hLen);

		for (u = 1; u <= c; u++) {
			if (u == 1) {
				tmp[Slen + 0] = (i & 0xff000000) >> 24;
				tmp[Slen + 1] = (i & 0x00ff0000) >> 16;
				tmp[Slen + 2] = (i & 0x0000ff00) >> 8;
				tmp[Slen + 3] = (i & 0x000000ff) >> 0;

				rc = _gnutls_mac_fast(GNUTLS_MAC_SHA1, P,
						      Plen, tmp, tmplen,
						      U);
			} else
				rc = _gnutls_mac_fast(GNUTLS_MAC_SHA1, P,
						      Plen, U, hLen, U);

			if (rc < 0) {
				gnutls_free(tmp);
				return rc;
			}

			for (k = 0; k < hLen; k++)
				T[k] ^= U[k];
		}

		memcpy(DK + (i - 1) * hLen, T, i == l ? r : hLen);
	}

	gnutls_free(tmp);

	return 0;
}
