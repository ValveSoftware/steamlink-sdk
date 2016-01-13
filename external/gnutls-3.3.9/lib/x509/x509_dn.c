/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
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

/* This file contains functions to handle X.509 certificate generation.
 */

#include <gnutls_int.h>

#include <gnutls_datum.h>
#include <gnutls_global.h>
#include <gnutls_errors.h>
#include <common.h>
#include <gnutls_x509.h>
#include <x509_b64.h>
#include <c-ctype.h>

typedef int (*set_dn_func) (void *, const char *oid, unsigned int raw_flag,
			    const void *name, unsigned int name_size);

static
int dn_attr_crt_set(set_dn_func f, void *crt, const gnutls_datum_t * name,
		    const gnutls_datum_t * val)
{
	char _oid[MAX_OID_SIZE];
	gnutls_datum_t tmp;
	const char *oid;
	int ret;
	unsigned i,j;

	if (name->size == 0 || val->size == 0)
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);

	if (c_isdigit(name->data[0]) != 0) {
		if (name->size >= sizeof(_oid))
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);

		memcpy(_oid, name->data, name->size);
		_oid[name->size] = 0;

		oid = _oid;

		if (gnutls_x509_dn_oid_known(oid) == 0) {
			_gnutls_debug_log("Unknown OID: '%s'\n", oid);
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		}
	} else {
		oid =
		    _gnutls_ldap_string_to_oid((char *) name->data,
					       name->size);
	}

	if (oid == NULL) {
		_gnutls_debug_log("Unknown DN attribute: '%.*s'\n",
				  (int) name->size, name->data);
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
	}

	if (val->data[0] == '#')
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);

	tmp.size = val->size;
	tmp.data = gnutls_malloc(tmp.size+1);
	if (tmp.data == NULL) {
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
	}

	for (j=i=0;i<tmp.size;i++) {
		if (1+j!=val->size && val->data[j] == '\\' && val->data[j+1] == ',') {
			tmp.data[i] = ',';
			j+=2;
			tmp.size--;
		} else {
			tmp.data[i] = val->data[j++];
		}
	}
	tmp.data[tmp.size] = 0;

	ret = f(crt, oid, 0, tmp.data, tmp.size);
	gnutls_free(tmp.data);

	if (ret < 0)
		return gnutls_assert_val(ret);

	return 0;
}

static int read_attr_and_val(const char **ptr,
			     gnutls_datum_t * name, gnutls_datum_t * val)
{
	const unsigned char *p = (void *) *ptr;

	/* skip any space */
	while (c_isspace(*p))
		p++;

	/* Read the name */
	name->data = (void *) p;
	while (*p != '=' && *p != 0 && !c_isspace(*p))
		p++;

	name->size = p - name->data;

	/* skip any space */
	while (c_isspace(*p))
		p++;

	if (*p != '=')
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
	p++;

	while (c_isspace(*p))
		p++;

	/* Read value */
	val->data = (void *) p;
	while (*p != 0 && (*p != ',' || (*p == ',' && *(p - 1) == '\\'))
	       && *p != '\n') {
		p++;
	}
	val->size = p - (val->data);

	/* remove spaces from the end */
	while(val->size > 0 && c_isspace(val->data[val->size-1])) {
		val->size--;
	}

	if (val->size == 0 || name->size == 0)
		return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);

	*ptr = (void *) p;

	return 0;
}

static int
crt_set_dn(set_dn_func f, void *crt, const char *dn, const char **err)
{
	const char *p = dn;
	int ret;
	gnutls_datum_t name, val;

	if (crt == NULL || dn == NULL)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	/* For each element */
	while (*p != 0 && *p != '\n') {
		if (err)
			*err = p;

		ret = read_attr_and_val(&p, &name, &val);
		if (ret < 0)
			return gnutls_assert_val(ret);

		/* skip spaces and look for comma */
		while (c_isspace(*p))
			p++;

		ret = dn_attr_crt_set(f, crt, &name, &val);
		if (ret < 0)
			return gnutls_assert_val(ret);

		if (err)
			*err = p;

		if (*p != ',' && *p != 0 && *p != '\n')
			return gnutls_assert_val(GNUTLS_E_PARSING_ERROR);
		if (*p == ',')
			p++;
	}

	return 0;
}


/**
 * gnutls_x509_crt_set_dn:
 * @crt: a certificate of type #gnutls_x509_crt_t
 * @dn: a comma separated DN string (RFC4514)
 * @err: indicates the error position (if any)
 *
 * This function will set the DN on the provided certificate.
 * The input string should be plain ASCII or UTF-8 encoded.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crt_set_dn(gnutls_x509_crt_t crt, const char *dn,
		       const char **err)
{
	return crt_set_dn((set_dn_func) gnutls_x509_crt_set_dn_by_oid, crt,
			  dn, err);
}

/**
 * gnutls_x509_crt_set_issuer_dn:
 * @crt: a certificate of type #gnutls_x509_crt_t
 * @dn: a comma separated DN string (RFC4514)
 * @err: indicates the error position (if any)
 *
 * This function will set the DN on the provided certificate.
 * The input string should be plain ASCII or UTF-8 encoded.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crt_set_issuer_dn(gnutls_x509_crt_t crt, const char *dn,
			      const char **err)
{
	return crt_set_dn((set_dn_func)
			  gnutls_x509_crt_set_issuer_dn_by_oid, crt, dn,
			  err);
}

/**
 * gnutls_x509_crq_set_dn:
 * @crq: a certificate of type #gnutls_x509_crq_t
 * @dn: a comma separated DN string (RFC4514)
 * @err: indicates the error position (if any)
 *
 * This function will set the DN on the provided certificate.
 * The input string should be plain ASCII or UTF-8 encoded.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crq_set_dn(gnutls_x509_crq_t crq, const char *dn,
		       const char **err)
{
	return crt_set_dn((set_dn_func) gnutls_x509_crq_set_dn_by_oid, crq,
			  dn, err);
}
