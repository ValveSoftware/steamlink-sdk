/*
 * Copyright (C) 2009-2012 Free Software Foundation, Inc.
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Written by Nikos Mavrogiannopoulos <nmav@gnutls.org>.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <time.h>
#include "benchmark.h"

static unsigned char data[64 * 1024];


static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "|<%d>| %s", level, str);
}

static void cipher_mac_bench(int algo, int mac_algo, int size)
{
	int ret;
	gnutls_cipher_hd_t ctx;
	gnutls_hmac_hd_t mac_ctx;
	void *_key, *_iv;
	gnutls_datum_t key, iv;
	int ivsize = gnutls_cipher_get_iv_size(algo);
	int keysize = gnutls_cipher_get_key_size(algo);
	int step = size * 1024;
	struct benchmark_st st;

	_key = malloc(keysize);
	if (_key == NULL)
		return;
	memset(_key, 0xf0, keysize);

	_iv = malloc(ivsize);
	if (_iv == NULL)
		return;
	memset(_iv, 0xf0, ivsize);

	iv.data = _iv;
	iv.size = ivsize;

	key.data = _key;
	key.size = keysize;

	printf("%16s-%s ", gnutls_cipher_get_name(algo),
	       gnutls_mac_get_name(mac_algo));
	fflush(stdout);

	start_benchmark(&st);

	ret = gnutls_hmac_init(&mac_ctx, mac_algo, key.data, key.size);
	if (ret < 0) {
		fprintf(stderr, "error: %s\n", gnutls_strerror(ret));
		goto leave;
	}

	ret = gnutls_cipher_init(&ctx, algo, &key, &iv);
	if (ret < 0) {
		fprintf(stderr, "error: %s\n", gnutls_strerror(ret));
		goto leave;
	}

	gnutls_hmac(mac_ctx, data, 1024);

	do {
		gnutls_hmac(mac_ctx, data, step);
		gnutls_cipher_encrypt2(ctx, data, step, data, step + 64);
		st.size += step;
	}
	while (benchmark_must_finish == 0);

	gnutls_cipher_deinit(ctx);
	gnutls_hmac_deinit(mac_ctx, NULL);

	stop_benchmark(&st, NULL, 1);

      leave:
	free(_key);
	free(_iv);
}


static void cipher_bench(int algo, int size, int aead)
{
	int ret;
	gnutls_cipher_hd_t ctx;
	void *_key, *_iv;
	gnutls_datum_t key, iv;
	int ivsize = gnutls_cipher_get_iv_size(algo);
	int keysize = gnutls_cipher_get_key_size(algo);
	int step = size * 1024;
	struct benchmark_st st;

	_key = malloc(keysize);
	if (_key == NULL)
		return;
	memset(_key, 0xf0, keysize);

	_iv = malloc(ivsize);
	if (_iv == NULL)
		return;
	memset(_iv, 0xf0, ivsize);

	iv.data = _iv;
	if (aead)
		iv.size = 12;
	else
		iv.size = ivsize;

	key.data = _key;
	key.size = keysize;

	printf("%16s ", gnutls_cipher_get_name(algo));
	fflush(stdout);

	start_benchmark(&st);

	ret = gnutls_cipher_init(&ctx, algo, &key, &iv);
	if (ret < 0) {
		fprintf(stderr, "error: %s\n", gnutls_strerror(ret));
		goto leave;
	}

	if (aead)
		gnutls_cipher_add_auth(ctx, data, 1024);

	do {
		gnutls_cipher_encrypt2(ctx, data, step, data, step + 64);
		st.size += step;
	}
	while (benchmark_must_finish == 0);

	gnutls_cipher_deinit(ctx);

	stop_benchmark(&st, NULL, 1);

      leave:
	free(_key);
	free(_iv);
}

static void mac_bench(int algo, int size)
{
	void *_key;
	int blocksize = gnutls_hmac_get_len(algo);
	int step = size * 1024;
	struct benchmark_st st;

	_key = malloc(blocksize);
	if (_key == NULL)
		return;
	memset(_key, 0xf0, blocksize);

	printf("%16s ", gnutls_mac_get_name(algo));
	fflush(stdout);

	start_benchmark(&st);

	do {
		gnutls_hmac_fast(algo, _key, blocksize, data, step, _key);
		st.size += step;
	}
	while (benchmark_must_finish == 0);

	stop_benchmark(&st, NULL, 1);

	free(_key);
}

void benchmark_cipher(int debug_level)
{
	int size = 16;
	gnutls_global_set_log_function(tls_log_func);
	gnutls_global_set_log_level(debug_level);

	gnutls_rnd(GNUTLS_RND_NONCE, data, sizeof(data));

	printf("Checking cipher-MAC combinations, payload size: %u\n", size * 1024);
	cipher_mac_bench(GNUTLS_CIPHER_SALSA20_256, GNUTLS_MAC_SHA1, size);
	cipher_mac_bench(GNUTLS_CIPHER_AES_128_CBC, GNUTLS_MAC_SHA1, size);
	cipher_mac_bench(GNUTLS_CIPHER_AES_128_CBC, GNUTLS_MAC_SHA256,
			 size);
	cipher_bench(GNUTLS_CIPHER_AES_128_GCM, size, 1);

	printf("\nChecking MAC algorithms, payload size: %u\n", size * 1024);
	mac_bench(GNUTLS_MAC_SHA1, size);
	mac_bench(GNUTLS_MAC_SHA256, size);
	mac_bench(GNUTLS_MAC_SHA512, size);

	printf("\nChecking ciphers, payload size: %u\n", size * 1024);
	cipher_bench(GNUTLS_CIPHER_3DES_CBC, size, 0);

	cipher_bench(GNUTLS_CIPHER_AES_128_CBC, size, 0);

	cipher_bench(GNUTLS_CIPHER_ARCFOUR, size, 0);

	cipher_bench(GNUTLS_CIPHER_SALSA20_256, size, 0);

	gnutls_global_deinit();
}
