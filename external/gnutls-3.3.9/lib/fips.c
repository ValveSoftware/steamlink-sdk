/*
 * Copyright (C) 2013 Red Hat
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
#include <gnutls_int.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <unistd.h>
#include <gnutls_errors.h>
#include <fips.h>
#include <gnutls/self-test.h>
#include <stdio.h>
#include <random.h>

unsigned int _gnutls_lib_mode = LIB_STATE_POWERON;
#ifdef ENABLE_FIPS140

#include <dlfcn.h>

#define FIPS_KERNEL_FILE "/proc/sys/crypto/fips_enabled"
#define FIPS_SYSTEM_FILE "/etc/system-fips"

/* Returns:
 * 0 - FIPS mode disabled
 * 1 - FIPS mode enabled and enforced
 * 2 - FIPS in testing mode
 */
unsigned _gnutls_fips_mode_enabled(void)
{
unsigned f1p = 0, f2p;
FILE* fd;
static int fips_mode = -1;
const char *p;

	if (fips_mode != -1)
		return fips_mode;

	p = getenv("GNUTLS_FORCE_FIPS_MODE");
	if (p) {
		if (p[0] == '1')
			fips_mode = 1;
		else if (p[0] == '2')
			fips_mode = 2;
		else
			fips_mode = 0;
		return fips_mode;
	}

	fd = fopen(FIPS_KERNEL_FILE, "r");
	if (fd != NULL) {
		f1p = fgetc(fd);
		fclose(fd);
		
		if (f1p == '1') f1p = 1;
		else f1p = 0;
	}

	f2p = !access(FIPS_SYSTEM_FILE, F_OK);

	if (f1p != 0 && f2p != 0) {
		_gnutls_debug_log("FIPS140-2 mode enabled\n");
		fips_mode = 1;
		return fips_mode;
	}

	if (f2p != 0) {
		/* a funny state where self tests are performed
		 * and ignored */
		_gnutls_debug_log("FIPS140-2 ZOMBIE mode enabled\n");
		fips_mode = 2;
		return fips_mode;
	}

	fips_mode = 0;
	return fips_mode;
}

#define GNUTLS_LIBRARY_NAME "libgnutls.so.28"
#define NETTLE_LIBRARY_NAME "libnettle.so.4"
#define HOGWEED_LIBRARY_NAME "libhogweed.so.2"
#define GMP_LIBRARY_NAME "libgmp.so.10"

static const char fips_key[] = "I'd rather be skiing";

#define HMAC_SUFFIX ".hmac"
#define HMAC_SIZE 32
#define HMAC_ALGO GNUTLS_MAC_SHA256

static int get_library_path(const char* lib, const char* symbol, char* path, size_t path_size)
{
Dl_info info;
int ret;
void *dl, *sym;

	dl = dlopen(lib, RTLD_LAZY);
	if (dl == NULL)
		return gnutls_assert_val(GNUTLS_E_FILE_ERROR);

	sym = dlsym(dl, symbol);
	if (sym == NULL) {
		ret = gnutls_assert_val(GNUTLS_E_FILE_ERROR);
		goto cleanup;
	}
	
	ret = dladdr(sym, &info);
	if (ret == 0) {
		ret = gnutls_assert_val(GNUTLS_E_FILE_ERROR);
		goto cleanup;
	}
	
	snprintf(path, path_size, "%s", info.dli_fname);

	ret = 0;
cleanup:
	dlclose(dl);
	return ret;
}

static void get_hmac_file(char *mac_file, size_t mac_file_size, const char* orig)
{
char* p;

	p = strrchr(orig, '/');
	if (p==NULL) {
		snprintf(mac_file, mac_file_size, ".%s"HMAC_SUFFIX, orig);
		return;
	}
	snprintf(mac_file, mac_file_size, "%.*s/.%s"HMAC_SUFFIX, (int)(p-orig), orig, p+1);
}

static void get_hmac_file2(char *mac_file, size_t mac_file_size, const char* orig)
{
char* p;

	p = strrchr(orig, '/');
	if (p==NULL) {
		snprintf(mac_file, mac_file_size, "fipscheck/%s"HMAC_SUFFIX, orig);
		return;
	}
	snprintf(mac_file, mac_file_size, "%.*s/fipscheck/%s"HMAC_SUFFIX, (int)(p-orig), orig, p+1);
}

/* Run an HMAC using the key above on the library binary data. 
 * Returns true on success and false on error.
 */
static unsigned check_binary_integrity(const char* libname, const char* symbol)
{
	int ret;
	unsigned prev;
	char mac_file[GNUTLS_PATH_MAX];
	char file[GNUTLS_PATH_MAX];
	uint8_t hmac[HMAC_SIZE];
	uint8_t new_hmac[HMAC_SIZE];
	size_t hmac_size;
	gnutls_datum_t data;

	ret = get_library_path(libname, symbol, file, sizeof(file));
	if (ret < 0) {
		_gnutls_debug_log("Could not get path for library %s\n", libname);
		return 0;
	}

	_gnutls_debug_log("Loading: %s\n", file);
	ret = gnutls_load_file(file, &data);
	if (ret < 0) {
		_gnutls_debug_log("Could not load: %s\n", file);
		return gnutls_assert_val(0);
	}

	prev = _gnutls_get_lib_state();
	_gnutls_switch_lib_state(LIB_STATE_OPERATIONAL);
	ret = gnutls_hmac_fast(HMAC_ALGO, fips_key, sizeof(fips_key)-1,
		data.data, data.size, new_hmac);
	_gnutls_switch_lib_state(prev);
	
	gnutls_free(data.data);

	if (ret < 0)
		return gnutls_assert_val(0);

	/* now open the .hmac file and compare */
	get_hmac_file(mac_file, sizeof(mac_file), file);

	ret = gnutls_load_file(mac_file, &data);
	if (ret < 0) {
		_gnutls_debug_log("Could not open %s for MAC testing: %s\n", mac_file, gnutls_strerror(ret));
		get_hmac_file2(mac_file, sizeof(mac_file), file);
		ret = gnutls_load_file(mac_file, &data);
		if (ret < 0) {
			_gnutls_debug_log("Could not open %s for MAC testing: %s\n", mac_file, gnutls_strerror(ret));
			return gnutls_assert_val(0);
		}
	}

	hmac_size = sizeof(hmac);
	ret = _gnutls_hex2bin((void*)data.data, data.size, hmac, &hmac_size);
	gnutls_free(data.data);

	if (ret < 0) {
		_gnutls_debug_log("Could not convert hex data to binary for MAC testing for %s.\n", libname);
		return gnutls_assert_val(0);
	}

	if (hmac_size != sizeof(hmac) ||
			memcmp(hmac, new_hmac, sizeof(hmac)) != 0) {
		_gnutls_debug_log("Calculated MAC for %s does not match\n", libname);
		return gnutls_assert_val(0);
	}
	_gnutls_debug_log("Successfully verified library MAC for %s\n", libname);
	
	return 1;
}

int _gnutls_fips_perform_self_checks1(void)
{
	int ret;

	_gnutls_switch_lib_state(LIB_STATE_SELFTEST);

	/* Tests the FIPS algorithms used by nettle internally.
	 * In our case we test AES-CBC since nettle's AES is used by
	 * the DRBG-AES.
	 */

	/* ciphers - one test per cipher */
	ret = gnutls_cipher_self_test(0, GNUTLS_CIPHER_AES_128_CBC);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	return 0;

error:
	_gnutls_switch_lib_state(LIB_STATE_ERROR);
	_gnutls_audit_log(NULL, "FIPS140-2 self testing part1 failed\n");

	return GNUTLS_E_SELF_TEST_ERROR;
}

int _gnutls_fips_perform_self_checks2(void)
{
	int ret;

	_gnutls_switch_lib_state(LIB_STATE_SELFTEST);

	/* Tests the FIPS algorithms */

	/* ciphers - one test per cipher */
	ret = gnutls_cipher_self_test(0, GNUTLS_CIPHER_3DES_CBC);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_cipher_self_test(0, GNUTLS_CIPHER_AES_256_GCM);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	/* MAC (includes message digest test) */
	ret = gnutls_mac_self_test(0, GNUTLS_MAC_SHA1);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_mac_self_test(0, GNUTLS_MAC_SHA224);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_mac_self_test(0, GNUTLS_MAC_SHA256);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_mac_self_test(0, GNUTLS_MAC_SHA384);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_mac_self_test(0, GNUTLS_MAC_SHA512);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	/* PK */
	ret = gnutls_pk_self_test(0, GNUTLS_PK_RSA);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_pk_self_test(0, GNUTLS_PK_DSA);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_pk_self_test(0, GNUTLS_PK_EC);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = gnutls_pk_self_test(0, GNUTLS_PK_DH);
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	if (_gnutls_rnd_ops.self_test == NULL) {
		gnutls_assert();
		goto error;
	}
        
	ret = _gnutls_rnd_ops.self_test();
	if (ret < 0) {
		gnutls_assert();
		goto error;
	}

	ret = check_binary_integrity(GNUTLS_LIBRARY_NAME, "gnutls_global_init");
	if (ret == 0) {
		gnutls_assert();
		goto error;
	}

	ret = check_binary_integrity(NETTLE_LIBRARY_NAME, "nettle_aes_set_encrypt_key");
	if (ret == 0) {
		gnutls_assert();
		goto error;
	}

	ret = check_binary_integrity(HOGWEED_LIBRARY_NAME, "nettle_mpz_sizeinbase_256_u");
	if (ret == 0) {
		gnutls_assert();
		goto error;
	}

	ret = check_binary_integrity(GMP_LIBRARY_NAME, "__gmpz_init");
	if (ret == 0) {
		gnutls_assert();
		goto error;
	}
	
	return 0;

error:
	_gnutls_switch_lib_state(LIB_STATE_ERROR);
	_gnutls_audit_log(NULL, "FIPS140-2 self testing part 2 failed\n");

	return GNUTLS_E_SELF_TEST_ERROR;
}
#endif

/**
 * gnutls_fips140_mode_enabled:
 *
 * Checks whether this library is in FIPS140 mode.
 *
 * Returns: return non-zero if true or zero if false.
 *
 * Since: 3.3.0
 **/
int gnutls_fips140_mode_enabled(void)
{
#ifdef ENABLE_FIPS140
int ret = _gnutls_fips_mode_enabled();

	if (ret == 1)
		return ret;
#endif
	return 0;
}

void _gnutls_lib_simulate_error(void)
{
	_gnutls_switch_lib_state(LIB_STATE_ERROR);
}
