/*
 * Copyright (C) 2003-2012 Free Software Foundation, Inc.
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
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations including
 * the two.
 * 
 * You must obey the GNU General Public License in all respects for all
 * of the code used other than OpenSSL. If you modify file(s) with this
 * exception, you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do
 * so, delete this exception statement from your version. If you delete
 * this exception statement from all source files in the program, then
 * also delete it here.
 */

#include <config.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/openpgp.h>
#include <gnutls/pkcs12.h>
#include <gnutls/pkcs11.h>
#include <gnutls/abstract.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <common.h>
#include "certtool-common.h"
#include "certtool-args.h"
#include "certtool-cfg.h"
#include <minmax.h>

/* Gnulib portability files. */
#include <read-file.h>

unsigned char *lbuffer = NULL;
int lbuffer_size = 0;

void fix_lbuffer(unsigned size)
{
	if (lbuffer_size == 0 || lbuffer == NULL) {
		if (size == 0)
			lbuffer_size = 64*1024;
		else
			lbuffer_size = MAX(64*1024,size);
		lbuffer = malloc(lbuffer_size);
		if (lbuffer == NULL) {
			fprintf(stderr, "memory error");
			exit(1);
		}
	}
}

FILE *safe_open_rw(const char *file, int privkey_op)
{
	mode_t omask = 0;
	FILE *fh;

	if (privkey_op != 0) {
		omask = umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	}

	fh = fopen(file, "wb");

	if (privkey_op != 0) {
		umask(omask);
	}

	return fh;
}

gnutls_datum_t *load_secret_key(int mand, common_info_st * info)
{
	static char raw_key[64];
	size_t raw_key_size = sizeof(raw_key);
	static gnutls_datum_t key;
	gnutls_datum_t hex_key;
	int ret;

	if (info->verbose)
		fprintf(stderr, "Loading secret key...\n");

	if (info->secret_key == NULL) {
		if (mand) {
			fprintf(stderr, "missing --secret-key\n");
			exit(1);
		} else
			return NULL;
	}

	hex_key.data = (void *) info->secret_key;
	hex_key.size = strlen(info->secret_key);

	ret = gnutls_hex_decode(&hex_key, raw_key, &raw_key_size);
	if (ret < 0) {
		fprintf(stderr, "hex_decode: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	key.data = (void *) raw_key;
	key.size = raw_key_size;

	return &key;
}

const char *get_password(common_info_st * cinfo, unsigned int *flags,
			 int confirm)
{
	const char *p;

	if (cinfo->null_password) {
		if (flags)
			*flags |= GNUTLS_PKCS_NULL_PASSWORD;
		return NULL;
	} else if (cinfo->password) {
		p = cinfo->password;
	} else {
		if (confirm)
			p = get_confirmed_pass(true);
		else
			p = get_pass();
	}

	if ((p == NULL || p[0] == 0) && flags && !cinfo->empty_password)
		*flags |= GNUTLS_PKCS_PLAIN;

	return p;
}

static gnutls_privkey_t _load_privkey(gnutls_datum_t * dat,
				      common_info_st * info)
{
	int ret;
	gnutls_privkey_t key;
	unsigned int flags = 0;
	const char *pass;

	ret = gnutls_privkey_init(&key);
	if (ret < 0) {
		fprintf(stderr, "privkey_init: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	ret =
	    gnutls_privkey_import_x509_raw(key, dat, info->incert_format,
					   NULL, 0);
	if (ret == GNUTLS_E_DECRYPTION_FAILED) {
		pass = get_password(info, &flags, 0);
		ret =
		    gnutls_privkey_import_x509_raw(key, dat,
						   info->incert_format,
						   pass, flags);
	}

	if (ret == GNUTLS_E_BASE64_UNEXPECTED_HEADER_ERROR) {
		fprintf(stderr,
			"import error: could not find a valid PEM header; "
			"check if your key is PKCS #12 encoded\n");
		exit(1);
	}

	if (ret < 0) {
		fprintf(stderr, "importing --load-privkey: %s: %s\n",
			info->privkey, gnutls_strerror(ret));
		exit(1);
	}

	return key;
}

static gnutls_privkey_t _load_url_privkey(const char *url)
{
	int ret;
	gnutls_privkey_t key;

	ret = gnutls_privkey_init(&key);
	if (ret < 0) {
		fprintf(stderr, "privkey_init: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	ret = gnutls_privkey_import_url(key, url, 0);
	if (ret < 0) {
		fprintf(stderr, "importing key: %s: %s\n",
			url, gnutls_strerror(ret));
		exit(1);
	}

	return key;
}

static gnutls_pubkey_t _load_url_pubkey(const char *url)
{
	int ret;
	gnutls_pubkey_t pubkey;
	unsigned int obj_flags = 0;

	ret = gnutls_pubkey_init(&pubkey);
	if (ret < 0) {
		fprintf(stderr, "Error in %s:%d: %s\n", __func__, __LINE__,
			gnutls_strerror(ret));
		exit(1);
	}

	ret = gnutls_pubkey_import_url(pubkey, url, obj_flags);
	if (ret < 0) {
		fprintf(stderr, "Error in %s:%d: %s: %s\n", __func__,
			__LINE__, gnutls_strerror(ret), url);
		exit(1);
	}

	return pubkey;
}

/* Load the private key.
 * @mand should be non zero if it is required to read a private key.
 */
gnutls_privkey_t load_private_key(int mand, common_info_st * info)
{
	gnutls_privkey_t key;
	gnutls_datum_t dat;
	size_t size;

	if (!info->privkey && !mand)
		return NULL;

	if (info->privkey == NULL) {
		fprintf(stderr, "missing --load-privkey\n");
		exit(1);
	}

	if (gnutls_url_is_supported(info->privkey) != 0)
		return _load_url_privkey(info->privkey);

	dat.data = (void *) read_binary_file(info->privkey, &size);
	dat.size = size;

	if (!dat.data) {
		fprintf(stderr, "reading --load-privkey: %s\n",
			info->privkey);
		exit(1);
	}

	key = _load_privkey(&dat, info);

	free(dat.data);

	return key;
}

/* Load the private key.
 * @mand should be non zero if it is required to read a private key.
 */
gnutls_x509_privkey_t
load_x509_private_key(int mand, common_info_st * info)
{
	gnutls_x509_privkey_t key;
	int ret;
	gnutls_datum_t dat;
	size_t size;
	unsigned int flags = 0;
	const char *pass;

	if (!info->privkey && !mand)
		return NULL;

	if (info->privkey == NULL) {
		fprintf(stderr, "missing --load-privkey\n");
		exit(1);
	}

	ret = gnutls_x509_privkey_init(&key);
	if (ret < 0) {
		fprintf(stderr, "privkey_init: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	dat.data = (void *) read_binary_file(info->privkey, &size);
	dat.size = size;

	if (!dat.data) {
		fprintf(stderr, "reading --load-privkey: %s\n",
			info->privkey);
		exit(1);
	}

	if (info->pkcs8) {
		pass = get_password(info, &flags, 0);
		ret =
		    gnutls_x509_privkey_import_pkcs8(key, &dat,
						     info->incert_format,
						     pass, flags);
	} else {
		ret =
		    gnutls_x509_privkey_import2(key, &dat,
						info->incert_format, NULL,
						0);
		if (ret == GNUTLS_E_DECRYPTION_FAILED) {
			pass = get_password(info, &flags, 0);
			ret =
			    gnutls_x509_privkey_import2(key, &dat,
							info->
							incert_format,
							pass, flags);
		}
	}

	free(dat.data);

	if (ret == GNUTLS_E_BASE64_UNEXPECTED_HEADER_ERROR) {
		fprintf(stderr,
			"import error: could not find a valid PEM header; "
			"check if your key is PEM encoded\n");
		exit(1);
	}

	if (ret < 0) {
		fprintf(stderr, "importing --load-privkey: %s: %s\n",
			info->privkey, gnutls_strerror(ret));
		exit(1);
	}

	return key;
}


/* Loads the certificate
 * If mand is non zero then a certificate is mandatory. Otherwise
 * null will be returned if the certificate loading fails.
 */
gnutls_x509_crt_t load_cert(int mand, common_info_st * info)
{
	gnutls_x509_crt_t *crt;
	size_t size;

	crt = load_cert_list(mand, &size, info);

	return crt ? crt[0] : NULL;
}

#define MAX_CERTS 256

/* Loads a certificate list
 */
gnutls_x509_crt_t *load_cert_list(int mand, size_t * crt_size,
				  common_info_st * info)
{
	FILE *fd;
	static gnutls_x509_crt_t crt[MAX_CERTS];
	char *ptr;
	int ret, i;
	gnutls_datum_t dat;
	size_t size;
	int ptr_size;

	fix_lbuffer(0);

	*crt_size = 0;
	if (info->verbose)
		fprintf(stderr, "Loading certificate list...\n");

	if (info->cert == NULL) {
		if (mand) {
			fprintf(stderr, "missing --load-certificate\n");
			exit(1);
		} else
			return NULL;
	}

	fd = fopen(info->cert, "r");
	if (fd == NULL) {
		fprintf(stderr, "Could not open %s\n", info->cert);
		exit(1);
	}

	size = fread(lbuffer, 1, lbuffer_size - 1, fd);
	lbuffer[size] = 0;

	fclose(fd);

	ptr = (void *) lbuffer;
	ptr_size = size;

	for (i = 0; i < MAX_CERTS; i++) {
		ret = gnutls_x509_crt_init(&crt[i]);
		if (ret < 0) {
			fprintf(stderr, "crt_init: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		dat.data = (void *) ptr;
		dat.size = ptr_size;

		ret =
		    gnutls_x509_crt_import(crt[i], &dat,
					   info->incert_format);
		if (ret < 0 && *crt_size > 0)
			break;
		if (ret < 0) {
			fprintf(stderr, "crt_import: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		ptr = strstr(ptr, "---END");
		if (ptr == NULL)
			break;
		ptr++;

		ptr_size = size;
		ptr_size -=
		    (unsigned int) ((unsigned char *) ptr -
				    (unsigned char *) lbuffer);

		if (ptr_size < 0)
			break;

		(*crt_size)++;
	}
	if (info->verbose)
		fprintf(stderr, "Loaded %d certificates.\n",
			(int) *crt_size);

	return crt;
}

/* Load the Certificate Request.
 */
gnutls_x509_crq_t load_request(common_info_st * info)
{
	gnutls_x509_crq_t crq;
	int ret;
	gnutls_datum_t dat;
	size_t size;

	if (!info->request)
		return NULL;

	ret = gnutls_x509_crq_init(&crq);
	if (ret < 0) {
		fprintf(stderr, "crq_init: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	dat.data = (void *) read_binary_file(info->request, &size);
	dat.size = size;

	if (!dat.data) {
		fprintf(stderr, "reading --load-request: %s\n",
			info->request);
		exit(1);
	}

	ret = gnutls_x509_crq_import(crq, &dat, info->incert_format);
	if (ret == GNUTLS_E_BASE64_UNEXPECTED_HEADER_ERROR) {
		fprintf(stderr,
			"import error: could not find a valid PEM header\n");
		exit(1);
	}

	free(dat.data);
	if (ret < 0) {
		fprintf(stderr, "importing --load-request: %s: %s\n",
			info->request, gnutls_strerror(ret));
		exit(1);
	}
	return crq;
}

/* Load the CA's private key.
 */
gnutls_privkey_t load_ca_private_key(common_info_st * info)
{
	gnutls_privkey_t key;
	gnutls_datum_t dat;
	size_t size;

	if (info->ca_privkey == NULL) {
		fprintf(stderr, "missing --load-ca-privkey\n");
		exit(1);
	}

	if (gnutls_url_is_supported(info->ca_privkey) != 0)
		return _load_url_privkey(info->ca_privkey);

	dat.data = (void *) read_binary_file(info->ca_privkey, &size);
	dat.size = size;

	if (!dat.data) {
		fprintf(stderr, "reading --load-ca-privkey: %s\n",
			info->ca_privkey);
		exit(1);
	}

	key = _load_privkey(&dat, info);

	free(dat.data);

	return key;
}

/* Loads the CA's certificate
 */
gnutls_x509_crt_t load_ca_cert(unsigned mand, common_info_st * info)
{
	gnutls_x509_crt_t crt;
	int ret;
	gnutls_datum_t dat;
	size_t size;

	if (mand == 0 && info->ca == NULL) {
		return NULL;
	}

	if (info->ca == NULL) {
		fprintf(stderr, "missing --load-ca-certificate\n");
		exit(1);
	}

	ret = gnutls_x509_crt_init(&crt);
	if (ret < 0) {
		fprintf(stderr, "crt_init: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	dat.data = (void *) read_binary_file(info->ca, &size);
	dat.size = size;

	if (!dat.data) {
		fprintf(stderr, "error reading --load-ca-certificate: %s\n",
			info->ca);
		exit(1);
	}

	ret = gnutls_x509_crt_import(crt, &dat, info->incert_format);
	free(dat.data);
	if (ret < 0) {
		fprintf(stderr, "importing --load-ca-certificate: %s: %s\n",
			info->ca, gnutls_strerror(ret));
		exit(1);
	}

	return crt;
}

/* Load a public key.
 * @mand should be non zero if it is required to read a public key.
 */
gnutls_pubkey_t load_pubkey(int mand, common_info_st * info)
{
	gnutls_pubkey_t key;
	int ret;
	gnutls_datum_t dat;
	size_t size;

	if (!info->pubkey && !mand)
		return NULL;

	if (info->pubkey == NULL) {
		fprintf(stderr, "missing --load-pubkey\n");
		exit(1);
	}

	if (gnutls_url_is_supported(info->pubkey) != 0)
		return _load_url_pubkey(info->pubkey);

	ret = gnutls_pubkey_init(&key);
	if (ret < 0) {
		fprintf(stderr, "privkey_init: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	dat.data = (void *) read_binary_file(info->pubkey, &size);
	dat.size = size;

	if (!dat.data) {
		fprintf(stderr, "reading --load-pubkey: %s\n", info->pubkey);
		exit(1);
	}

	ret = gnutls_pubkey_import(key, &dat, info->incert_format);

	free(dat.data);

	if (ret == GNUTLS_E_BASE64_UNEXPECTED_HEADER_ERROR) {
		fprintf(stderr,
			"import error: could not find a valid PEM header; "
			"check if your key has the PUBLIC KEY header\n");
		exit(1);
	}

	if (ret < 0) {
		fprintf(stderr, "importing --load-pubkey: %s: %s\n",
			info->pubkey, gnutls_strerror(ret));
		exit(1);
	}

	return key;
}

gnutls_pubkey_t load_public_key_or_import(int mand,
					  gnutls_privkey_t privkey,
					  common_info_st * info)
{
	gnutls_pubkey_t pubkey;
	int ret;

	ret = gnutls_pubkey_init(&pubkey);
	if (ret < 0) {
		fprintf(stderr, "gnutls_pubkey_init: %s\n",
			gnutls_strerror(ret));
		exit(1);
	}

	if (!privkey || (ret = gnutls_pubkey_import_privkey(pubkey, privkey, 0, 0)) < 0) {	/* could not get (e.g. on PKCS #11 */
		gnutls_pubkey_deinit(pubkey);
		return load_pubkey(mand, info);
	}

	return pubkey;
}

int
get_bits(gnutls_pk_algorithm_t key_type, int info_bits,
	 const char *info_sec_param, int warn)
{
	int bits;

	if (info_bits != 0) {
		static int warned = 0;

		if (warned == 0 && warn != 0 && GNUTLS_BITS_ARE_CURVE(info_bits)==0) {
			warned = 1;
			fprintf(stderr,
				"** Note: Please use the --sec-param instead of --bits\n");
		}
		bits = info_bits;
	} else {
		if (info_sec_param == 0) {
			/* For ECDSA keys use 256 bits or better, as they are widely supported */
			if (key_type == GNUTLS_PK_EC)
				info_sec_param = "HIGH";
			else
				info_sec_param = "MEDIUM";
		}
		bits =
		    gnutls_sec_param_to_pk_bits(key_type,
						str_to_sec_param
						(info_sec_param));
	}

	return bits;
}

gnutls_sec_param_t str_to_sec_param(const char *str)
{
	if (strcasecmp(str, "low") == 0) {
		return GNUTLS_SEC_PARAM_LOW;
	} else if (strcasecmp(str, "legacy") == 0) {
		return GNUTLS_SEC_PARAM_LEGACY;
	} else if (strcasecmp(str, "normal") == 0 || strcasecmp(str, "medium") == 0) {
		return GNUTLS_SEC_PARAM_MEDIUM;
	} else if (strcasecmp(str, "high") == 0) {
		return GNUTLS_SEC_PARAM_HIGH;
	} else if (strcasecmp(str, "ultra") == 0) {
		return GNUTLS_SEC_PARAM_ULTRA;
	} else {
		fprintf(stderr, "Unknown security parameter string: %s\n",
			str);
		exit(1);
	}

}

#define SPACE "\t"
static void
print_hex_datum(FILE * outfile, gnutls_datum_t * dat, int cprint)
{
	unsigned int j;

	if (cprint != 0) {
		fprintf(outfile, "\n" SPACE "\"");
		for (j = 0; j < dat->size; j++) {
			fprintf(outfile, "\\x%.2x",
				(unsigned char) dat->data[j]);
			if ((j + 1) % 15 == 0)
				fprintf(outfile, "\"\n" SPACE "\"");
		}
		fprintf(outfile, "\";\n\n");

		return;
	}

	fprintf(outfile, "\n" SPACE);
	for (j = 0; j < dat->size; j++) {
		fprintf(outfile, "%.2x:", (unsigned char) dat->data[j]);
		if ((j + 1) % 15 == 0)
			fprintf(outfile, "\n" SPACE);
	}
	fprintf(outfile, "\n\n");
}

static void print_head(FILE * out, const char *txt, unsigned int size,
		       int cprint)
{
	unsigned i;
	char *p, *ntxt;

	if (cprint != 0) {
		if (size > 0)
			asprintf(&ntxt, "const unsigned char %s[%u] =",
				 txt, size);
		else
			asprintf(&ntxt, "const unsigned char %s[] =\n",
				 txt);

		p = strstr(ntxt, "char");
		p += 5;

		for (i = 0; i < strlen(txt); i++)
			if (p[i] == ' ')
				p[i] = '_';

		fprintf(out, "%s", ntxt);
		free(ntxt);

		return;
	}
	fprintf(out, "%s:", txt);
}

void
print_dsa_pkey(FILE * outfile, gnutls_datum_t * x, gnutls_datum_t * y,
	       gnutls_datum_t * p, gnutls_datum_t * q, gnutls_datum_t * g,
	       int cprint)
{
	if (x) {
		print_head(outfile, "private key", x->size, cprint);
		print_hex_datum(outfile, x, cprint);
	}
	print_head(outfile, "public key", y->size, cprint);
	print_hex_datum(outfile, y, cprint);
	print_head(outfile, "p", p->size, cprint);
	print_hex_datum(outfile, p, cprint);
	print_head(outfile, "q", q->size, cprint);
	print_hex_datum(outfile, q, cprint);
	print_head(outfile, "g", g->size, cprint);
	print_hex_datum(outfile, g, cprint);
}

gnutls_ecc_curve_t str_to_curve(const char *str)
{
unsigned num = 0;
const gnutls_ecc_curve_t *list, *p;

	list = gnutls_ecc_curve_list();

	p = list;
	while(*p != 0) {
		if (strcasecmp(str, gnutls_ecc_curve_get_name(*p)) == 0)
			return *p;
		p++;
		num++;
	}

	fprintf(stderr, "Unsupported curve: %s\nAvailable curves:\n", str);
	if (num == 0)
		printf("none\n");
	p = list;
	while(*p != 0) {
		fprintf(stderr, "\t- %s\n",
		       gnutls_ecc_curve_get_name(*p));
		p++;
	}
	exit(1);
}

void
print_ecc_pkey(FILE * outfile, gnutls_ecc_curve_t curve,
	       gnutls_datum_t * k, gnutls_datum_t * x, gnutls_datum_t * y,
	       int cprint)
{
	if (cprint != 0)
		fprintf(outfile, "/* curve: %s */\n",
			gnutls_ecc_curve_get_name(curve));
	else
		fprintf(outfile, "curve:\t%s\n",
			gnutls_ecc_curve_get_name(curve));

	if (k) {
		print_head(outfile, "private key", k->size, cprint);
		print_hex_datum(outfile, k, cprint);
	}
	print_head(outfile, "x", x->size, cprint);
	print_hex_datum(outfile, x, cprint);
	print_head(outfile, "y", y->size, cprint);
	print_hex_datum(outfile, y, cprint);
}


void
print_rsa_pkey(FILE * outfile, gnutls_datum_t * m, gnutls_datum_t * e,
	       gnutls_datum_t * d, gnutls_datum_t * p, gnutls_datum_t * q,
	       gnutls_datum_t * u, gnutls_datum_t * exp1,
	       gnutls_datum_t * exp2, int cprint)
{
	print_head(outfile, "modulus", m->size, cprint);
	print_hex_datum(outfile, m, cprint);
	print_head(outfile, "public exponent", e->size, cprint);
	print_hex_datum(outfile, e, cprint);
	if (d) {
		print_head(outfile, "private exponent", d->size, cprint);
		print_hex_datum(outfile, d, cprint);
		print_head(outfile, "prime1", p->size, cprint);
		print_hex_datum(outfile, p, cprint);
		print_head(outfile, "prime2", q->size, cprint);
		print_hex_datum(outfile, q, cprint);
		print_head(outfile, "coefficient", u->size, cprint);
		print_hex_datum(outfile, u, cprint);
		if (exp1 && exp2) {
			print_head(outfile, "exp1", exp1->size, cprint);
			print_hex_datum(outfile, exp1, cprint);
			print_head(outfile, "exp2", exp2->size, cprint);
			print_hex_datum(outfile, exp2, cprint);
		}
	}
}

void _pubkey_info(FILE * outfile,
		  gnutls_certificate_print_formats_t format,
		  gnutls_pubkey_t pubkey)
{
	gnutls_datum_t data;
	int ret;
	size_t size;

	fix_lbuffer(0);

	ret = gnutls_pubkey_print(pubkey, format, &data);
	if (ret < 0) {
		fprintf(stderr, "pubkey_print error: %s\n",
			gnutls_strerror(ret));
		exit(1);
	}

	fprintf(outfile, "%s\n", data.data);
	gnutls_free(data.data);

	size = lbuffer_size;
	ret =
	    gnutls_pubkey_export(pubkey, GNUTLS_X509_FMT_PEM, lbuffer,
				 &size);
	if (ret < 0) {
		fprintf(stderr, "export error: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	fprintf(outfile, "\n%s\n", lbuffer);
}

static void
print_dh_info(FILE * outfile, gnutls_datum_t * p, gnutls_datum_t * g,
	      unsigned int q_bits, int cprint)
{
	if (q_bits > 0) {
		if (cprint != 0)
			fprintf(outfile,
				"\n /* recommended key length: %d bytes */\n\n",
				(7 + q_bits) / 8);
		else
			fprintf(outfile,
				"\nRecommended key length: %d bits\n\n",
				q_bits);
	}

	print_head(outfile, "generator", g->size, cprint);
	print_hex_datum(outfile, g, cprint);

	print_head(outfile, "prime", p->size, cprint);
	print_hex_datum(outfile, p, cprint);


}

void dh_info(FILE * infile, FILE * outfile, common_info_st * ci)
{
	gnutls_datum_t params;
	size_t size;
	int ret;
	gnutls_dh_params_t dh_params;
	gnutls_datum_t p, g;
	unsigned int q_bits = 0;

	fix_lbuffer(0);

	if (gnutls_dh_params_init(&dh_params) < 0) {
		fprintf(stderr, "Error in dh parameter initialization\n");
		exit(1);
	}

	params.data = (void *) fread_file(infile, &size);
	params.size = size;

	ret =
	    gnutls_dh_params_import_pkcs3(dh_params, &params,
					  ci->incert_format);
	if (ret < 0) {
		fprintf(stderr, "Error parsing dh params: %s\n",
			gnutls_strerror(ret));
		exit(1);
	}

	ret = gnutls_dh_params_export_raw(dh_params, &p, &g, &q_bits);
	if (ret < 0) {
		fprintf(stderr, "Error exporting parameters: %s\n",
			gnutls_strerror(ret));
		exit(1);
	}

	if (ci->outcert_format == GNUTLS_X509_FMT_PEM)
		print_dh_info(outfile, &p, &g, q_bits, ci->cprint);

	if (!ci->cprint) {	/* generate a PKCS#3 structure */
		size_t len = lbuffer_size;

		ret =
		    gnutls_dh_params_export_pkcs3(dh_params,
						  ci->outcert_format,
						  lbuffer, &len);

		if (ret == 0) {
			if (ci->outcert_format == GNUTLS_X509_FMT_PEM) {
				fprintf(outfile, "\n%s", lbuffer);
			} else {
				fwrite(lbuffer, 1, len, outfile);
			}
		} else {
			fprintf(stderr, "Error: %s\n",
				gnutls_strerror(ret));
		}
	}

	gnutls_dh_params_deinit(dh_params);
}

/* If how is zero then the included parameters are used.
 */
int generate_prime(FILE * outfile, int how, common_info_st * info)
{
	int ret;
	gnutls_dh_params_t dh_params;
	gnutls_datum_t p, g;
	int bits = get_bits(GNUTLS_PK_DH, info->bits, info->sec_param, 1);
	unsigned int q_bits = 0;

	fix_lbuffer(0);

	gnutls_dh_params_init(&dh_params);

	if (how != 0) {
		fprintf(stderr, "Generating DH parameters (%d bits)...\n",
			bits);
		fprintf(stderr, "(might take long time)\n");
	} else
		fprintf(stderr, "Retrieving DH parameters...\n");

	if (how != 0) {
		ret = gnutls_dh_params_generate2(dh_params, bits);
		if (ret < 0) {
			fprintf(stderr,
				"Error generating parameters: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		ret =
		    gnutls_dh_params_export_raw(dh_params, &p, &g,
						&q_bits);
		if (ret < 0) {
			fprintf(stderr, "Error exporting parameters: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}
	} else {
#ifdef ENABLE_SRP
		if (bits <= 1024) {
			p = gnutls_srp_1024_group_prime;
			g = gnutls_srp_1024_group_generator;
			bits = 1024;
		} else if (bits <= 1536) {
			p = gnutls_srp_1536_group_prime;
			g = gnutls_srp_1536_group_generator;
			bits = 1536;
		} else if (bits <= 2048) {
			p = gnutls_srp_2048_group_prime;
			g = gnutls_srp_2048_group_generator;
			bits = 2048;
		} else if (bits <= 3072) {
			p = gnutls_srp_3072_group_prime;
			g = gnutls_srp_3072_group_generator;
			bits = 3072;
		} else {
			p = gnutls_srp_4096_group_prime;
			g = gnutls_srp_4096_group_generator;
			bits = 4096;
		}

		ret = gnutls_dh_params_import_raw(dh_params, &p, &g);
		if (ret < 0) {
			fprintf(stderr, "Error exporting parameters: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}
#else
		fprintf(stderr,
			"Parameters unavailable as SRP is disabled.\n");
		exit(1);
#endif
	}

	print_dh_info(outfile, &p, &g, q_bits, info->cprint);

	if (!info->cprint) {	/* generate a PKCS#3 structure */
		size_t len = lbuffer_size;

		ret =
		    gnutls_dh_params_export_pkcs3(dh_params,
						  GNUTLS_X509_FMT_PEM,
						  lbuffer, &len);

		if (ret == 0) {
			fprintf(outfile, "\n%s", lbuffer);
		} else {
			fprintf(stderr, "Error: %s\n",
				gnutls_strerror(ret));
		}

	}

	gnutls_dh_params_deinit(dh_params);

	return 0;
}
