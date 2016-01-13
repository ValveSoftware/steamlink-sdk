/*
 * Copyright (C) 2014 Free Software Foundation, Inc.
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

/* Functions on X.509 Certificate parsing
 */

#include <gnutls_int.h>
#include <gnutls_datum.h>
#include <gnutls_global.h>
#include <gnutls_errors.h>
#include <common.h>
#include <gnutls_x509.h>
#include <gnutls/x509-ext.h>
#include <x509_b64.h>
#include <x509_int.h>
#include <libtasn1.h>

/* Name constraints is limited to DNS names.
 */

static unsigned is_nc_empty(struct gnutls_name_constraints_st* nc, unsigned type)
{
	name_constraints_node_st *t;

	if (nc->permitted == NULL && nc->excluded == NULL)
		return 1;

	t = nc->permitted;
	while (t != NULL) {
		if (t->type == type)
			return 0;
		t = t->next;
	}

	t = nc->excluded;
	while (t != NULL) {
		if (t->type == type)
			return 0;
		t = t->next;
	}

	/* no constraint for that type exists */
	return 1;
}

int _gnutls_extract_name_constraints(ASN1_TYPE c2, const char *vstr,
				    name_constraints_node_st ** _nc)
{
	int ret;
	char tmpstr[128];
	unsigned indx = 0;
	gnutls_datum_t tmp = { NULL, 0 };
	unsigned int type;
	struct name_constraints_node_st *nc, *prev;

	prev = *_nc;
	if (prev != NULL) {
		while(prev->next != NULL)
			prev = prev->next;
	}

	do {
		indx++;
		snprintf(tmpstr, sizeof(tmpstr), "%s.?%u.base", vstr, indx);

		ret =
		    _gnutls_parse_general_name2(c2, tmpstr, -1, &tmp, &type, 0);

		if (ret < 0) {
			gnutls_assert();
			break;
		}

		if (type != GNUTLS_SAN_DNSNAME && type != GNUTLS_SAN_RFC822NAME &&
		    type != GNUTLS_SAN_DN && type != GNUTLS_SAN_URI &&
		    type != GNUTLS_SAN_IPADDRESS) {
			gnutls_assert();
			ret = GNUTLS_E_ILLEGAL_PARAMETER;
			goto cleanup;
		}

		nc = gnutls_malloc(sizeof(struct name_constraints_node_st));
		if (nc == NULL) {
			gnutls_assert();
			ret = GNUTLS_E_MEMORY_ERROR;
			goto cleanup;
		}

		memcpy(&nc->name, &tmp, sizeof(gnutls_datum_t));
		nc->type = type;
		nc->next = NULL;

		if (prev == NULL) {
			*_nc = prev = nc;
		} else {
			prev->next = nc;
			prev = nc;
		}

		tmp.data = NULL;
	} while (ret >= 0);

	if (ret < 0 && ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;
 cleanup:
	gnutls_free(tmp.data);
	return ret;
}

/**
 * gnutls_x509_crt_get_name_constraints:
 * @crt: should contain a #gnutls_x509_crt_t structure
 * @nc: The nameconstraints intermediate structure
 * @flags: zero or %GNUTLS_NAME_CONSTRAINTS_FLAG_APPEND
 * @critical: the extension status
 *
 * This function will return an intermediate structure containing
 * the name constraints of the provided CA certificate. That
 * structure can be used in combination with gnutls_x509_name_constraints_check()
 * to verify whether a server's name is in accordance with the constraints.
 *
 * When the @flags is set to %GNUTLS_NAME_CONSTRAINTS_FLAG_APPEND, then if 
 * the @nc structure is empty
 * this function will behave identically as if the flag was not set.
 * Otherwise if there are elements in the @nc structure then only the
 * excluded constraints will be appended to the constraints.
 *
 * Note that @nc must be initialized prior to calling this function.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE
 * if the extension is not present, otherwise a negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_x509_crt_get_name_constraints(gnutls_x509_crt_t crt,
					 gnutls_x509_name_constraints_t nc,
					 unsigned int flags,
					 unsigned int *critical)
{
	int ret;
	gnutls_datum_t der = { NULL, 0 };

	if (crt == NULL) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	ret =
	    _gnutls_x509_crt_get_extension(crt, "2.5.29.30", 0, &der,
					   critical);
	if (ret < 0)
		return gnutls_assert_val(ret);

	if (der.size == 0 || der.data == NULL)
		return gnutls_assert_val(GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

	ret = gnutls_x509_ext_import_name_constraints(&der, nc, flags);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;

 cleanup:
	_gnutls_free_datum(&der);

	return ret;

}

/**
 * gnutls_x509_name_constraints_deinit:
 * @nc: The nameconstraints structure
 *
 * This function will deinitialize a name constraints structure.
 *
 * Since: 3.3.0
 **/
void gnutls_x509_name_constraints_deinit(gnutls_x509_name_constraints_t nc)
{
	name_constraints_node_st * next, *t;

	t = nc->permitted;
	while (t != NULL) {
		next = t->next;
		gnutls_free(t->name.data);
		gnutls_free(t);
		t = next;
	}

	t = nc->excluded;
	while (t != NULL) {
		next = t->next;
		gnutls_free(t->name.data);
		gnutls_free(t);
		t = next;
	}
	gnutls_free(nc);
}

/**
 * gnutls_x509_name_constraints_init:
 * @nc: The nameconstraints structure
 *
 * This function will initialize a name constraints structure.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_x509_name_constraints_init(gnutls_x509_name_constraints_t *nc)
{
	*nc = gnutls_calloc(1, sizeof(struct gnutls_name_constraints_st));
	if (*nc == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	return 0;
}

static
int name_constraints_add(gnutls_x509_name_constraints_t nc,
		       gnutls_x509_subject_alt_name_t type,
		       const gnutls_datum_t * name,
		       unsigned permitted)
{
	struct name_constraints_node_st * tmp, *prev = NULL;
	int ret;

	if (type != GNUTLS_SAN_DNSNAME && type != GNUTLS_SAN_RFC822NAME &&
		type != GNUTLS_SAN_DN && type != GNUTLS_SAN_URI && type != GNUTLS_SAN_IPADDRESS)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	if (type == GNUTLS_SAN_DNSNAME && name->size > 0 && name->data[0] == '.') {
		_gnutls_debug_log("DNSNAME constraints cannot start with '.'. They must contain a domain name\n");
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	if (permitted != 0)
		prev = tmp = nc->permitted;
	else
		prev = tmp = nc->excluded;

	while(tmp != NULL) {
		tmp = tmp->next;
		if (tmp != NULL)
			prev = tmp;
	}

	tmp = gnutls_malloc(sizeof(struct name_constraints_node_st));
	if (tmp == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	tmp->next = NULL;
	tmp->type = type;
	ret = _gnutls_set_datum(&tmp->name, name->data, name->size);
	if (ret < 0) {
		gnutls_assert();
		gnutls_free(tmp);
		return ret;
	}

	if (prev == NULL) {
		if (permitted != 0)
			nc->permitted = tmp;
		else
			nc->excluded = tmp;
	} else
		prev->next = tmp;

	return 0;
}

/**
 * gnutls_x509_name_constraints_add_permitted:
 * @nc: The nameconstraints structure
 * @type: The type of the constraints
 * @name: The data of the constraints
 *
 * This function will add a name constraint to the list of permitted
 * constraints.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_x509_name_constraints_add_permitted(gnutls_x509_name_constraints_t nc,
					       gnutls_x509_subject_alt_name_t type,
					       const gnutls_datum_t * name)
{
	return name_constraints_add(nc, type, name, 1);
}

/**
 * gnutls_x509_name_constraints_add_excluded:
 * @nc: The nameconstraints structure
 * @type: The type of the constraints
 * @name: The data of the constraints
 *
 * This function will add a name constraint to the list of excluded
 * constraints.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_x509_name_constraints_add_excluded(gnutls_x509_name_constraints_t nc,
					      gnutls_x509_subject_alt_name_t type,
					      const gnutls_datum_t * name)
{
	return name_constraints_add(nc, type, name, 0);
}

/**
 * gnutls_x509_crt_set_name_constraints:
 * @crt: The certificate structure
 * @nc: The nameconstraints structure
 * @critical: whether this extension will be critical
 *
 * This function will set the provided name constraints to
 * the certificate extension list. This extension is always
 * marked as critical.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_x509_crt_set_name_constraints(gnutls_x509_crt_t crt, 
					 gnutls_x509_name_constraints_t nc,
					 unsigned int critical)
{
int ret;
gnutls_datum_t der;

	ret = gnutls_x509_ext_export_name_constraints(nc, &der);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret =
	    _gnutls_x509_crt_set_extension(crt, "2.5.29.30", &der, critical);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;
	crt->use_extensions = 1;

cleanup:
	_gnutls_free_datum(&der);
	return ret;
}

static
unsigned ends_with(const gnutls_datum_t * str, const gnutls_datum_t * suffix)
{
	if (suffix->size >= str->size)
		return 0;

	if (memcmp(str->data + str->size - suffix->size, suffix->data, suffix->size) == 0 &&
		str->data[str->size - suffix->size -1] == '.')
		return 1;

	return 0;
}

static
unsigned email_ends_with(const gnutls_datum_t * str, const gnutls_datum_t * suffix)
{
	if (suffix->size >= str->size)
		return 0;

	if (suffix->size > 1 && suffix->data[0] == '.') {
		/* .domain.com */
		if (memcmp(str->data + str->size - suffix->size, suffix->data, suffix->size) == 0)
			return 1;
	} else {
		if (memcmp(str->data + str->size - suffix->size, suffix->data, suffix->size) == 0 &&
			str->data[str->size - suffix->size -1] == '@')
			return 1;
	}

	return 0;
}

static unsigned dnsname_matches(const gnutls_datum_t *name, const gnutls_datum_t *suffix)
{
	_gnutls_hard_log("matching %.*s with DNS constraint %.*s\n", name->size, name->data,
		suffix->size, suffix->data);

	if (suffix->size == name->size && memcmp(suffix->data, name->data, suffix->size) == 0)
		return 1; /* match */

	return ends_with(name, suffix);
}

static unsigned email_matches(const gnutls_datum_t *name, const gnutls_datum_t *suffix)
{
	_gnutls_hard_log("matching %.*s with e-mail constraint %.*s\n", name->size, name->data,
		suffix->size, suffix->data);

	if (suffix->size == name->size && memcmp(suffix->data, name->data, suffix->size) == 0)
		return 1; /* match */

	return email_ends_with(name, suffix);
}

static
unsigned check_unsupported_constraint(gnutls_x509_name_constraints_t nc,
				       gnutls_x509_subject_alt_name_t type)
{
unsigned i;
int ret;
unsigned rtype;
gnutls_datum_t rname;

	/* check if there is a restrictions with that type, if
	 * yes, then reject the name.
	 */
	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_excluded(nc, i++, &rtype, &rname);
		if (ret >= 0) {
			if (rtype != type)
				continue;
			else
				return gnutls_assert_val(0);
		}

	} while(ret == 0);

	return 1;
}

static
unsigned check_dns_constraints(gnutls_x509_name_constraints_t nc,
				       const gnutls_datum_t * name)
{
unsigned i;
int ret;
unsigned rtype;
unsigned allowed_found = 0;
gnutls_datum_t rname;

	/* check restrictions */
	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_excluded(nc, i++, &rtype, &rname);
		if (ret >= 0) {
			if (rtype != GNUTLS_SAN_DNSNAME)
				continue;

			/* a name of value 0 means that the CA shouldn't have issued
			 * a certificate with a DNSNAME. */
			if (rname.size == 0)
				return gnutls_assert_val(0);

			if (dnsname_matches(name, &rname) != 0)
				return gnutls_assert_val(0); /* rejected */
		}
	} while(ret == 0);

	/* check allowed */
	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_permitted(nc, i++, &rtype, &rname);
		if (ret >= 0) {
			if (rtype != GNUTLS_SAN_DNSNAME)
				continue;

			if (rname.size == 0)
				continue;

			allowed_found = 1;

			if (dnsname_matches(name, &rname) != 0)
				return 1; /* accepted */
		}
	} while(ret == 0);

	if (allowed_found != 0) /* there are allowed directives but this host wasn't found */
		return gnutls_assert_val(0);

	return 1;
}

static
unsigned check_email_constraints(gnutls_x509_name_constraints_t nc,
				       const gnutls_datum_t * name)
{
unsigned i;
int ret;
unsigned rtype;
unsigned allowed_found = 0;
gnutls_datum_t rname;

	/* check restrictions */
	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_excluded(nc, i++, &rtype, &rname);
		if (ret >= 0) {
			if (rtype != GNUTLS_SAN_RFC822NAME)
				continue;

			/* a name of value 0 means that the CA shouldn't have issued
			 * a certificate with an e-mail. */
			if (rname.size == 0)
				return gnutls_assert_val(0);

			if (email_matches(name, &rname) != 0)
				return gnutls_assert_val(0); /* rejected */
		}
	} while(ret == 0);

	/* check allowed */
	i = 0;
	do {
		ret = gnutls_x509_name_constraints_get_permitted(nc, i++, &rtype, &rname);
		if (ret >= 0) {
			if (rtype != GNUTLS_SAN_RFC822NAME)
				continue;

			if (rname.size == 0)
				continue;

			allowed_found = 1;

			if (email_matches(name, &rname) != 0)
				return 1; /* accepted */
		}
	} while(ret == 0);

	if (allowed_found != 0) /* there are allowed directives but this host wasn't found */
		return gnutls_assert_val(0);

	return 1;
}

/**
 * gnutls_x509_name_constraints_check:
 * @nc: the extracted name constraints structure
 * @type: the type of the constraint to check (of type gnutls_x509_subject_alt_name_t)
 * @name: the name to be checked
 *
 * This function will check the provided name against the constraints in
 * @nc using the RFC5280 rules. Currently this function is limited to DNS
 * names and emails (of type %GNUTLS_SAN_DNSNAME and %GNUTLS_SAN_RFC822NAME).
 *
 * Returns: zero if the provided name is not acceptable, and non-zero otherwise.
 *
 * Since: 3.3.0
 **/
unsigned gnutls_x509_name_constraints_check(gnutls_x509_name_constraints_t nc,
				       gnutls_x509_subject_alt_name_t type,
				       const gnutls_datum_t * name)
{
	if (type == GNUTLS_SAN_DNSNAME)
		return check_dns_constraints(nc, name);

	if (type == GNUTLS_SAN_RFC822NAME)
		return check_email_constraints(nc, name);

	return check_unsupported_constraint(nc, type);
}

/**
 * gnutls_x509_name_constraints_check_crt:
 * @nc: the extracted name constraints structure
 * @type: the type of the constraint to check (of type gnutls_x509_subject_alt_name_t)
 * @cert: the certificate to be checked
 *
 * This function will check the provided certificate names against the constraints in
 * @nc using the RFC5280 rules. It will traverse all the certificate's names and
 * alternative names.
 *
 * Currently this function is limited to DNS
 * names and emails (of type %GNUTLS_SAN_DNSNAME and %GNUTLS_SAN_RFC822NAME).
 *
 * Returns: zero if the provided name is not acceptable, and non-zero otherwise.
 *
 * Since: 3.3.0
 **/
unsigned gnutls_x509_name_constraints_check_crt(gnutls_x509_name_constraints_t nc,
				       gnutls_x509_subject_alt_name_t type,
				       gnutls_x509_crt_t cert)
{
char name[MAX_CN];
size_t name_size;
int ret;
unsigned idx, t, san_type;
gnutls_datum_t n;
unsigned found_one;

	if (is_nc_empty(nc, type) != 0)
		return 1; /* shortcut; no constraints to check */

	if (type == GNUTLS_SAN_RFC822NAME) {
		idx = found_one = 0;
		do {
			name_size = sizeof(name);
			ret = gnutls_x509_crt_get_subject_alt_name2(cert,
				idx++, name, &name_size, &san_type, NULL);
			if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				break;
			else if (ret < 0)
				return gnutls_assert_val(0);

			if (san_type != GNUTLS_SAN_RFC822NAME)
				continue;

			found_one = 1;
			n.data = (void*)name;
			n.size = name_size;
			t = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME,
				&n);
			if (t == 0)
				return gnutls_assert_val(t);
		} while(ret >= 0);

		/* there is at least a single e-mail. That means that the EMAIL field will
		 * not be used for verifying the identity of the holder. */
		if (found_one != 0)
			return 1;

		do {
			/* ensure there is only a single EMAIL, similarly to CN handling (rfc6125) */
			name_size = sizeof(name);
			ret = gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_PKCS9_EMAIL,
							    1, 0, name, &name_size);
			if (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				return gnutls_assert_val(0);

			name_size = sizeof(name);
			ret = gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_PKCS9_EMAIL,
							    0, 0, name, &name_size);
			if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				break;
			else if (ret < 0)
				return gnutls_assert_val(0);

			found_one = 1;
			n.data = (void*)name;
			n.size = name_size;
			t = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_RFC822NAME, &n);
			if (t == 0)
				return gnutls_assert_val(t);
		} while(0);

		/* passed */
		if (found_one != 0)
			return 1;
		else /* nothing was found */
			return gnutls_assert_val(0);
	} else if (type == GNUTLS_SAN_DNSNAME) {
		idx = found_one = 0;
		do {
			name_size = sizeof(name);
			ret = gnutls_x509_crt_get_subject_alt_name2(cert,
				idx++, name, &name_size, &san_type, NULL);
			if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				break;
			else if (ret < 0)
				return gnutls_assert_val(0);

			if (san_type != GNUTLS_SAN_DNSNAME)
				continue;

			found_one = 1;
			n.data = (void*)name;
			n.size = name_size;
			t = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_DNSNAME,
				&n);
			if (t == 0)
				return gnutls_assert_val(t);
		} while(ret >= 0);

		/* there is at least a single DNS name. That means that the CN will
		 * not be used for verifying the identity of the holder. */
		if (found_one != 0)
			return 1;

		/* verify the name constraints against the CN, if the certificate is
		 * not a CA. */
		do {

			/* ensure there is only a single CN, according to rfc6125 */
			name_size = sizeof(name);
			ret = gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME,
				 			    1, 0, name, &name_size);
			if (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				return gnutls_assert_val(0);

			name_size = sizeof(name);
			ret = gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME,
							    0, 0, name, &name_size);
			if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				break;
			else if (ret < 0)
				return gnutls_assert_val(0);

			found_one = 1;
			n.data = (void*)name;
			n.size = name_size;
			t = gnutls_x509_name_constraints_check(nc, GNUTLS_SAN_DNSNAME,
				&n);
			if (t == 0)
				return gnutls_assert_val(t);
		} while(0);

		/* passed */
		if (found_one != 0)
			return 1;
		else /* nothing was found */
			return gnutls_assert_val(0);
	} else
		return check_unsupported_constraint(nc, type);
}

/**
 * gnutls_x509_name_constraints_get_permitted:
 * @nc: the extracted name constraints structure
 * @idx: the index of the constraint
 * @type: the type of the constraint (of type gnutls_x509_subject_alt_name_t)
 * @name: the name in the constraint (of the specific type)
 *
 * This function will return an intermediate structure containing
 * the name constraints of the provided CA certificate. That
 * structure can be used in combination with gnutls_x509_name_constraints_check()
 * to verify whether a server's name is in accordance with the constraints.
 *
 * The name should be treated as constant and valid for the lifetime of @nc.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE
 * if the extension is not present, otherwise a negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_x509_name_constraints_get_permitted(gnutls_x509_name_constraints_t nc,
				     unsigned idx,
				     unsigned *type, gnutls_datum_t * name)
{
	unsigned int i;
	struct name_constraints_node_st * tmp = nc->permitted;

	for (i = 0; i < idx; i++) {
		if (tmp == NULL)
			return
			    gnutls_assert_val
			    (GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

		tmp = tmp->next;
	}

	if (tmp == NULL)
		return gnutls_assert_val(GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

	*type = tmp->type;
	*name = tmp->name;

	return 0;
}

/**
 * gnutls_x509_name_constraints_get_excluded:
 * @nc: the extracted name constraints structure
 * @idx: the index of the constraint
 * @type: the type of the constraint (of type gnutls_x509_subject_alt_name_t)
 * @name: the name in the constraint (of the specific type)
 *
 * This function will return an intermediate structure containing
 * the name constraints of the provided CA certificate. That
 * structure can be used in combination with gnutls_x509_name_constraints_check()
 * to verify whether a server's name is in accordance with the constraints.
 *
 * The name should be treated as constant and valid for the lifetime of @nc.
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE
 * if the extension is not present, otherwise a negative error value.
 *
 * Since: 3.3.0
 **/
int gnutls_x509_name_constraints_get_excluded(gnutls_x509_name_constraints_t nc,
				     unsigned idx,
				     unsigned *type, gnutls_datum_t * name)
{
	unsigned int i;
	struct name_constraints_node_st * tmp = nc->excluded;

	for (i = 0; i < idx; i++) {
		if (tmp == NULL)
			return
			    gnutls_assert_val
			    (GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

		tmp = tmp->next;
	}

	if (tmp == NULL)
		return gnutls_assert_val(GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

	*type = tmp->type;
	*name = tmp->name;

	return 0;
}
