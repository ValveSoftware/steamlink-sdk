/*
 * Copyright (C) 2010-2012 Free Software Foundation, Inc.
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

#include <config.h>
#include <system.h>
#include <gnutls_int.h>
#include <gnutls_errors.h>

#include <sys/socket.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <c-ctype.h>

#ifdef _WIN32
# include <windows.h>
# include <wincrypt.h>
# if defined(__MINGW32__) && !defined(__MINGW64__) && __MINGW32_MAJOR_VERSION <= 3 && __MINGW32_MINOR_VERSION <= 20
typedef PCCRL_CONTEXT WINAPI(*Type_CertEnumCRLsInStore) (HCERTSTORE
							 hCertStore,
							 PCCRL_CONTEXT
							 pPrevCrlContext);
static Type_CertEnumCRLsInStore Loaded_CertEnumCRLsInStore;
static HMODULE Crypt32_dll;
# else
#  define Loaded_CertEnumCRLsInStore CertEnumCRLsInStore
# endif

#else /* _WIN32 */
# include <sys/select.h>

# ifdef HAVE_PTHREAD_LOCKS
#  include <pthread.h>
# endif

# if defined(HAVE_GETPWUID_R)
#  include <pwd.h>
# endif
#endif

/* System specific function wrappers.
 */

#ifdef _WIN32
/* Do not use the gnulib functions for sending and receiving data.
 * Using them makes gnutls only working with gnulib applications.
 */
#undef send
#undef recv
#undef select

int system_errno(gnutls_transport_ptr p)
{
	int tmperr = WSAGetLastError();
	int ret = 0;
	switch (tmperr) {
	case WSAEWOULDBLOCK:
		ret = EAGAIN;
		break;
	case NO_ERROR:
		ret = 0;
		break;
	case WSAEINTR:
		ret = EINTR;
		break;
	case WSAEMSGSIZE:
		ret = EMSGSIZE;
		break;
	default:
		ret = EIO;
		break;
	}
	WSASetLastError(tmperr);

	return ret;
}

ssize_t
system_write(gnutls_transport_ptr ptr, const void *data, size_t data_size)
{
	return send(GNUTLS_POINTER_TO_INT(ptr), data, data_size, 0);
}
#else				/* POSIX */
int system_errno(gnutls_transport_ptr_t ptr)
{
#if defined(_AIX) || defined(AIX)
	if (errno == 0)
		errno = EAGAIN;
#endif

	return errno;
}

ssize_t
system_writev(gnutls_transport_ptr_t ptr, const giovec_t * iovec,
	      int iovec_cnt)
{
	return writev(GNUTLS_POINTER_TO_INT(ptr), (struct iovec *) iovec,
		      iovec_cnt);

}
#endif

ssize_t
system_read(gnutls_transport_ptr_t ptr, void *data, size_t data_size)
{
	return recv(GNUTLS_POINTER_TO_INT(ptr), data, data_size, 0);
}

/* Wait for data to be received within a timeout period in milliseconds.
 * To catch a termination it will also try to receive 0 bytes from the
 * socket if select reports to proceed.
 *
 * Returns -1 on error, 0 on timeout, positive value if data are available for reading.
 */
int system_recv_timeout(gnutls_transport_ptr_t ptr, unsigned int ms)
{
	fd_set rfds;
	struct timeval tv;
	int ret;
	int fd = GNUTLS_POINTER_TO_INT(ptr);

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	tv.tv_sec = 0;
	tv.tv_usec = ms * 1000;

	while (tv.tv_usec >= 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec++;
	}

	ret = select(fd + 1, &rfds, NULL, NULL, &tv);
	if (ret <= 0)
		return ret;

	return ret;
}

/* Thread stuff */

#ifdef HAVE_WIN32_LOCKS
static int gnutls_system_mutex_init(void **priv)
{
	CRITICAL_SECTION *lock = malloc(sizeof(CRITICAL_SECTION));

	if (lock == NULL)
		return GNUTLS_E_MEMORY_ERROR;

	InitializeCriticalSection(lock);

	*priv = lock;

	return 0;
}

static int gnutls_system_mutex_deinit(void **priv)
{
	DeleteCriticalSection((CRITICAL_SECTION *) * priv);
	free(*priv);

	return 0;
}

static int gnutls_system_mutex_lock(void **priv)
{
	EnterCriticalSection((CRITICAL_SECTION *) * priv);
	return 0;
}

static int gnutls_system_mutex_unlock(void **priv)
{
	LeaveCriticalSection((CRITICAL_SECTION *) * priv);
	return 0;
}

#endif				/* WIN32_LOCKS */

#ifdef HAVE_PTHREAD_LOCKS

static int gnutls_system_mutex_init(void **priv)
{
	pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
	int ret;

	if (lock == NULL)
		return GNUTLS_E_MEMORY_ERROR;

	ret = pthread_mutex_init(lock, NULL);
	if (ret) {
		free(lock);
		gnutls_assert();
		return GNUTLS_E_LOCKING_ERROR;
	}

	*priv = lock;

	return 0;
}

static int gnutls_system_mutex_deinit(void **priv)
{
	pthread_mutex_destroy((pthread_mutex_t *) * priv);
	free(*priv);
	return 0;
}

static int gnutls_system_mutex_lock(void **priv)
{
	if (pthread_mutex_lock((pthread_mutex_t *) * priv)) {
		gnutls_assert();
		return GNUTLS_E_LOCKING_ERROR;
	}

	return 0;
}

static int gnutls_system_mutex_unlock(void **priv)
{
	if (pthread_mutex_unlock((pthread_mutex_t *) * priv)) {
		gnutls_assert();
		return GNUTLS_E_LOCKING_ERROR;
	}

	return 0;
}

#endif				/* PTHREAD_LOCKS */

#ifdef HAVE_NO_LOCKS

static int gnutls_system_mutex_init(void **priv)
{
	return 0;
}

static int gnutls_system_mutex_deinit(void **priv)
{
	return 0;
}

static int gnutls_system_mutex_lock(void **priv)
{
	return 0;
}

static int gnutls_system_mutex_unlock(void **priv)
{
	return 0;
}

#endif				/* NO_LOCKS */

gnutls_time_func gnutls_time = time;
mutex_init_func gnutls_mutex_init = gnutls_system_mutex_init;
mutex_deinit_func gnutls_mutex_deinit = gnutls_system_mutex_deinit;
mutex_lock_func gnutls_mutex_lock = gnutls_system_mutex_lock;
mutex_unlock_func gnutls_mutex_unlock = gnutls_system_mutex_unlock;

int gnutls_system_global_init()
{
#ifdef _WIN32
#if defined(__MINGW32__) && !defined(__MINGW64__) && __MINGW32_MAJOR_VERSION <= 3 && __MINGW32_MINOR_VERSION <= 20
	HMODULE crypto;
	crypto = LoadLibraryA("Crypt32.dll");

	if (crypto == NULL)
		return GNUTLS_E_CRYPTO_INIT_FAILED;

	Loaded_CertEnumCRLsInStore =
	    (Type_CertEnumCRLsInStore) GetProcAddress(crypto,
						      "CertEnumCRLsInStore");
	if (Loaded_CertEnumCRLsInStore == NULL) {
		FreeLibrary(crypto);
		return GNUTLS_E_CRYPTO_INIT_FAILED;
	}

	Crypt32_dll = crypto;
#endif
#endif
	return 0;
}

void gnutls_system_global_deinit()
{
#ifdef _WIN32
#if defined(__MINGW32__) && !defined(__MINGW64__) && __MINGW32_MAJOR_VERSION <= 3 && __MINGW32_MINOR_VERSION <= 20
	FreeLibrary(Crypt32_dll);
#endif
#endif
}


#define CONFIG_PATH ".gnutls"

/* Returns a path to store user-specific configuration
 * data.
 */
int _gnutls_find_config_path(char *path, size_t max_size)
{
	const char *home_dir = getenv("HOME");

	if (home_dir != NULL && home_dir[0] != 0) {
		snprintf(path, max_size, "%s/" CONFIG_PATH, home_dir);
		return 0;
	}

#ifdef _WIN32
	if (home_dir == NULL || home_dir[0] == '\0') {
		const char *home_drive = getenv("HOMEDRIVE");
		const char *home_path = getenv("HOMEPATH");

		if (home_drive != NULL && home_path != NULL) {
			snprintf(path, max_size, "%s%s/" CONFIG_PATH, home_drive, home_path);
		} else {
			path[0] = 0;
		}
	}
#elif defined(HAVE_GETPWUID_R)
	if (home_dir == NULL || home_dir[0] == '\0') {
		struct passwd *pwd;
		struct passwd _pwd;
		int ret;
		char tmp[512];

		ret = getpwuid_r(getuid(), &_pwd, tmp, sizeof(tmp), &pwd);
		if (ret == 0 && pwd != NULL) {
			snprintf(path, max_size, "%s/" CONFIG_PATH, pwd->pw_dir);
		} else {
			path[0] = 0;
		}
	}
#else
	if (home_dir == NULL || home_dir[0] == '\0') {
			path[0] = 0;
	}
#endif

	return 0;
}

#if defined(DEFAULT_TRUST_STORE_FILE) || (defined(DEFAULT_TRUST_STORE_PKCS11) && defined(ENABLE_PKCS11))
static
int
add_system_trust(gnutls_x509_trust_list_t list,
		 unsigned int tl_flags, unsigned int tl_vflags)
{
	int ret, r = 0;
	const char *crl_file =
#ifdef DEFAULT_CRL_FILE
	    DEFAULT_CRL_FILE;
#else
	    NULL;
#endif

#if defined(ENABLE_PKCS11) && defined(DEFAULT_TRUST_STORE_PKCS11)
	ret =
	    gnutls_x509_trust_list_add_trust_file(list,
						  DEFAULT_TRUST_STORE_PKCS11,
						  crl_file,
						  GNUTLS_X509_FMT_DER,
						  tl_flags, tl_vflags);
	if (ret > 0)
		r += ret;
#endif

#ifdef DEFAULT_TRUST_STORE_FILE
	ret =
	    gnutls_x509_trust_list_add_trust_file(list,
						  DEFAULT_TRUST_STORE_FILE,
						  crl_file,
						  GNUTLS_X509_FMT_PEM,
						  tl_flags, tl_vflags);
	if (ret > 0)
		r += ret;
#endif

#ifdef DEFAULT_BLACKLIST_FILE
	ret = gnutls_x509_trust_list_remove_trust_file(list, DEFAULT_BLACKLIST_FILE, GNUTLS_X509_FMT_PEM);
	if (ret < 0) {
		_gnutls_debug_log("Could not load blacklist file '%s'\n", DEFAULT_BLACKLIST_FILE);
	}
#endif

	return r;
}
#elif defined(_WIN32)
static
int add_system_trust(gnutls_x509_trust_list_t list, unsigned int tl_flags,
		     unsigned int tl_vflags)
{
	char path[GNUTLS_PATH_MAX];
	unsigned int i;
	int r = 0;

	for (i = 0; i < 2; i++) {
		HCERTSTORE store;
		const CERT_CONTEXT *cert;
		const CRL_CONTEXT *crl;
		gnutls_datum_t data;

		if (i == 0)
			store = CertOpenSystemStore(0, "ROOT");
		else
			store = CertOpenSystemStore(0, "CA");

		if (store == NULL)
			return GNUTLS_E_FILE_ERROR;

		cert = CertEnumCertificatesInStore(store, NULL);
		crl = Loaded_CertEnumCRLsInStore(store, NULL);

		while (cert != NULL) {
			if (cert->dwCertEncodingType == X509_ASN_ENCODING) {
				data.data = cert->pbCertEncoded;
				data.size = cert->cbCertEncoded;
				if (gnutls_x509_trust_list_add_trust_mem
				    (list, &data, NULL,
				     GNUTLS_X509_FMT_DER, tl_flags,
				     tl_vflags) > 0)
					r++;
			}
			cert = CertEnumCertificatesInStore(store, cert);
		}

		while (crl != NULL) {
			if (crl->dwCertEncodingType == X509_ASN_ENCODING) {
				data.data = crl->pbCrlEncoded;
				data.size = crl->cbCrlEncoded;
				gnutls_x509_trust_list_add_trust_mem(list,
								     NULL,
								     &data,
								     GNUTLS_X509_FMT_DER,
								     tl_flags,
								     tl_vflags);
			}
			crl = Loaded_CertEnumCRLsInStore(store, crl);
		}
		CertCloseStore(store, 0);
	}

#ifdef DEFAULT_BLACKLIST_FILE
	ret = gnutls_x509_trust_list_remove_trust_file(list, DEFAULT_BLACKLIST_FILE, GNUTLS_X509_FMT_PEM);
	if (ret < 0) {
		_gnutls_debug_log("Could not load blacklist file '%s'\n", DEFAULT_BLACKLIST_FILE);
	}
#endif

	return r;
}
#elif defined(ANDROID) || defined(__ANDROID__) || defined(DEFAULT_TRUST_STORE_DIR)

# include <dirent.h>
# include <unistd.h>

# if defined(ANDROID) || defined(__ANDROID__)
#  define DEFAULT_TRUST_STORE_DIR "/system/etc/security/cacerts/"

static int load_revoked_certs(gnutls_x509_trust_list_t list, unsigned type)
{
	DIR *dirp;
	struct dirent *d;
	int ret;
	int r = 0;
	char path[GNUTLS_PATH_MAX];

	dirp = opendir("/data/misc/keychain/cacerts-removed/");
	if (dirp != NULL) {
		do {
			d = readdir(dirp);
			if (d != NULL && d->d_type == DT_REG) {
				snprintf(path, sizeof(path),
					 "/data/misc/keychain/cacerts-removed/%s",
					 d->d_name);

				ret =
				    gnutls_x509_trust_list_remove_trust_file
				    (list, path, type);
				if (ret >= 0)
					r += ret;
			}
		}
		while (d != NULL);
		closedir(dirp);
	}

	return r;
}
# endif


/* This works on android 4.x 
 */
static
int add_system_trust(gnutls_x509_trust_list_t list, unsigned int tl_flags,
		     unsigned int tl_vflags)
{
	int r = 0, ret;

	ret = gnutls_x509_trust_list_add_trust_dir(list, DEFAULT_TRUST_STORE_DIR,
		NULL, GNUTLS_X509_FMT_PEM, tl_flags, tl_vflags);
	if (ret >= 0)
		r += ret;

# if defined(ANDROID) || defined(__ANDROID__)
	ret = load_revoked_certs(list, GNUTLS_X509_FMT_DER);
	if (ret >= 0)
		r -= ret;

	ret = gnutls_x509_trust_list_add_trust_dir(list, "/data/misc/keychain/cacerts-added/",
		NULL, GNUTLS_X509_FMT_DER, tl_flags, tl_vflags);
	if (ret >= 0)
		r += ret;
# endif

	return r;
}
#else

#define add_system_trust(x,y,z) GNUTLS_E_UNIMPLEMENTED_FEATURE

#endif

/**
 * gnutls_x509_trust_list_add_system_trust:
 * @list: The structure of the list
 * @tl_flags: GNUTLS_TL_*
 * @tl_vflags: gnutls_certificate_verify_flags if flags specifies GNUTLS_TL_VERIFY_CRL
 *
 * This function adds the system's default trusted certificate
 * authorities to the trusted list. Note that on unsupported systems
 * this function returns %GNUTLS_E_UNIMPLEMENTED_FEATURE.
 *
 * This function implies the flag %GNUTLS_TL_NO_DUPLICATES.
 *
 * Returns: The number of added elements or a negative error code on error.
 *
 * Since: 3.1
 **/
int
gnutls_x509_trust_list_add_system_trust(gnutls_x509_trust_list_t list,
					unsigned int tl_flags,
					unsigned int tl_vflags)
{
	return add_system_trust(list, tl_flags|GNUTLS_TL_NO_DUPLICATES, tl_vflags);
}

#if defined(_WIN32)
#include <winnls.h>

/* Can convert only english */
int _gnutls_ucs2_to_utf8(const void *data, size_t size,
			 gnutls_datum_t * output)
{
	int ret;
	unsigned i;
	int len = 0, src_len;
	char *dst = NULL;
	char *src = NULL;

	src_len = size / 2;

	src = gnutls_malloc(size);
	if (src == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	/* convert to LE */
	for (i = 0; i < size; i += 2) {
		src[i] = ((char *) data)[1 + i];
		src[1 + i] = ((char *) data)[i];
	}

	ret =
	    WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
				(void *) src, src_len, NULL, 0, NULL,
				NULL);
	if (ret == 0) {
		ret = gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		goto fail;
	}

	len = ret + 1;
	dst = gnutls_malloc(len);
	if (dst == NULL) {
		ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
		goto fail;
	}

	ret =
	    WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
				(void *) src, src_len, dst, len, NULL,
				NULL);
	if (ret == 0) {
		ret = gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		goto fail;
	}

	dst[len - 1] = 0;
	output->data = dst;
	output->size = ret;
	ret = 0;
	goto cleanup;

      fail:
	gnutls_free(dst);

      cleanup:
	gnutls_free(src);
	return ret;
}

#elif defined(HAVE_ICONV) || defined(HAVE_LIBICONV)

#include <iconv.h>

int _gnutls_ucs2_to_utf8(const void *data, size_t size,
			 gnutls_datum_t * output)
{
	iconv_t conv;
	int ret;
	size_t orig, dstlen = size * 2;
	char *src = (void *) data;
	char *dst = NULL, *pdst;

	if (size == 0)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	conv = iconv_open("UTF-8", "UTF-16BE");
	if (conv == (iconv_t) - 1)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	/* Note that dstlen has enough size for every possible input characters.
	 * (remember the in UTF-16 the characters in data are at most size/2, 
	 *  and we allocate 4 bytes per character).
	 */
	pdst = dst = gnutls_malloc(dstlen + 1);
	if (dst == NULL) {
		ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
		goto fail;
	}

	orig = dstlen;
	ret = iconv(conv, &src, &size, &pdst, &dstlen);
	if (ret == -1) {
		ret = gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		goto fail;
	}

	output->data = (void *) dst;
	output->size = orig - dstlen;
	output->data[output->size] = 0;

	ret = 0;
	goto cleanup;

      fail:
	gnutls_free(dst);

      cleanup:
	iconv_close(conv);

	return ret;
}

#else

/* Can convert only english (ASCII) */
int _gnutls_ucs2_to_utf8(const void *data, size_t size,
			 gnutls_datum_t * output)
{
	unsigned int i, j;
	char *dst;
	const char *src = data;

	if (size == 0 || size % 2 != 0)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	dst = gnutls_malloc(size + 1);
	if (dst == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	for (i = j = 0; i < size; i += 2, j++) {
		if (src[i] != 0 || !c_isascii(src[i + 1]))
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		dst[j] = src[i + 1];
	}

	output->data = (void *) dst;
	output->size = j;
	output->data[output->size] = 0;

	return 0;
}
#endif
