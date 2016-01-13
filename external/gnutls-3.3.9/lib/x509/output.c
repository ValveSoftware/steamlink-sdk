/*
 * Copyright (C) 2007-2014 Free Software Foundation, Inc.
 *
 * Author: Simon Josefsson, Nikos Mavrogiannopoulos
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

/* Functions for printing X.509 Certificate structures
 */

#include <gnutls_int.h>
#include <common.h>
#include <gnutls_x509.h>
#include <x509_int.h>
#include <gnutls_num.h>
#include <gnutls_errors.h>
#include <extras/randomart.h>

#ifdef HAVE_INET_NTOP
# include <arpa/inet.h>
#endif

#define addf _gnutls_buffer_append_printf
#define adds _gnutls_buffer_append_str

#define NON_NULL(x) (((x)!=NULL)?((char*)(x)):"")
#define ERROR_STR (char*) "(error)"

static const
char *ip_to_string(void *_ip, int ip_size, char *string,
			  int string_size)
{

	if (ip_size != 4 && ip_size != 16) {
		gnutls_assert();
		return NULL;
	}

	if (ip_size == 4 && string_size < 16) {
		gnutls_assert();
		return NULL;
	}

	if (ip_size == 16 && string_size < 48) {
		gnutls_assert();
		return NULL;
	}

#ifdef HAVE_INET_NTOP
	if (ip_size == 4)
		return inet_ntop(AF_INET, _ip, string, string_size);
	else
		return inet_ntop(AF_INET6, _ip, string, string_size);
#else
	{
		uint8_t *ip;
		ip = _ip;
		switch (ip_size) {
		case 4:
			snprintf(string, string_size, "%u.%u.%u.%u", ip[0], ip[1],
				 ip[2], ip[3]);
			break;
		case 16:
			snprintf(string, string_size, "%x:%x:%x:%x:%x:%x:%x:%x",
				 (ip[0] << 8) | ip[1], (ip[2] << 8) | ip[3],
				 (ip[4] << 8) | ip[5], (ip[6] << 8) | ip[7],
				 (ip[8] << 8) | ip[9], (ip[10] << 8) | ip[11],
				 (ip[12] << 8) | ip[13], (ip[14] << 8) | ip[15]);
			break;
		}
		return string;
	}
#endif
}

static void
print_name(gnutls_buffer_st *str, const char *prefix, unsigned type, gnutls_datum_t *name)
{
char *sname = (char*)name->data;
char str_ip[64];
const char *p;

	if ((type == GNUTLS_SAN_DNSNAME || type == GNUTLS_SAN_OTHERNAME_XMPP
	     || type == GNUTLS_SAN_RFC822NAME
	     || type == GNUTLS_SAN_URI) && sname != NULL && strlen(sname) != name->size) {
		adds(str,
		     _("warning: generalName contains an embedded NUL, "
			      "replacing with '!'\n"));
		while (strlen(sname) < name->size)
			name->data[strlen(sname)] = '!';
	}

	switch (type) {
	case GNUTLS_SAN_DNSNAME:
		addf(str,  _("%sDNSname: %.*s\n"), prefix, name->size, NON_NULL(name->data));
		break;

	case GNUTLS_SAN_RFC822NAME:
		addf(str,  _("%sRFC822Name: %.*s\n"), prefix, name->size, NON_NULL(name->data));
		break;

	case GNUTLS_SAN_URI:
		addf(str,  _("%sURI: %.*s\n"), prefix, name->size, NON_NULL(name->data));
		break;

	case GNUTLS_SAN_IPADDRESS:
		p = ip_to_string(name->data, name->size, str_ip, sizeof(str_ip));
		if (p == NULL)
			p = ERROR_STR;
		addf(str, "%sIPAddress: %s\n", prefix, p);
		break;

	case GNUTLS_SAN_DN:
		addf(str,  _("%sdirectoryName: %.*s\n"), prefix, name->size, NON_NULL(name->data));
		break;

	case GNUTLS_SAN_OTHERNAME_XMPP:
		addf(str,  _("%sXMPP Address: %.*s\n"), prefix, name->size, NON_NULL(name->data));
		break;
	default:
		addf(str,  _("%sUnknown name: "), prefix);
		_gnutls_buffer_hexprint(str, name->data, name->size);
		break;
	}
}

static void print_proxy(gnutls_buffer_st * str, gnutls_datum_t *der)
{
	int pathlen;
	char *policyLanguage;
	char *policy;
	size_t npolicy;
	int err;

	err = gnutls_x509_ext_import_proxy(der, &pathlen, &policyLanguage,
					&policy, &npolicy);
	if (err < 0) {
		addf(str, "error: get_proxy: %s\n", gnutls_strerror(err));
		return;
	}

	if (pathlen >= 0)
		addf(str, _("\t\t\tPath Length Constraint: %d\n"),
		     pathlen);
	addf(str, _("\t\t\tPolicy Language: %s"), policyLanguage);
	if (strcmp(policyLanguage, "1.3.6.1.5.5.7.21.1") == 0)
		adds(str, " (id-ppl-inheritALL)\n");
	else if (strcmp(policyLanguage, "1.3.6.1.5.5.7.21.2") == 0)
		adds(str, " (id-ppl-independent)\n");
	else
		adds(str, "\n");
	if (npolicy) {
		adds(str, _("\t\t\tPolicy:\n\t\t\t\tASCII: "));
		_gnutls_buffer_asciiprint(str, policy, npolicy);
		adds(str, _("\n\t\t\t\tHexdump: "));
		_gnutls_buffer_hexprint(str, policy, npolicy);
		adds(str, "\n");
	}
	gnutls_free(policy);
	gnutls_free(policyLanguage);
}


static void print_nc(gnutls_buffer_st * str, const char* prefix, gnutls_datum_t *der)
{
	gnutls_x509_name_constraints_t nc;
	int ret;
	unsigned idx = 0;
	gnutls_datum_t name;
	unsigned type;
	char new_prefix[16];

	ret = gnutls_x509_name_constraints_init(&nc);
	if (ret < 0)
		return;

	ret = gnutls_x509_ext_import_name_constraints(der, nc, 0);
	if (ret < 0)
		goto cleanup;

	snprintf(new_prefix, sizeof(new_prefix), "%s\t\t\t\t", prefix);

	do {
		ret = gnutls_x509_name_constraints_get_permitted(nc, idx++, &type, &name);

		if (ret >= 0) {
			if (idx == 1)
				addf(str,  _("%s\t\t\tPermitted:\n"), prefix);

			print_name(str, new_prefix, type, &name);
		}
	} while (ret == 0);

	idx = 0;
	do {
		ret = gnutls_x509_name_constraints_get_excluded(nc, idx++, &type, &name);

		if (ret >= 0) {
			if (idx == 1)
				addf(str,  _("%s\t\t\tExcluded:\n"), prefix);

			print_name(str, new_prefix, type, &name);
		}
	} while (ret == 0);

cleanup:
	gnutls_x509_name_constraints_deinit(nc);
}

static void print_aia(gnutls_buffer_st * str, const gnutls_datum_t *der)
{
	int err;
	int seq;
	gnutls_datum_t san = { NULL, 0 }, oid = {NULL, 0};
	gnutls_x509_aia_t aia;
	unsigned int san_type;
	
	err = gnutls_x509_aia_init(&aia);
	if (err < 0)
		return;

	err = gnutls_x509_ext_import_aia(der, aia, 0);
	if (err < 0) {
		addf(str, "error: get_aia: %s\n",
		     gnutls_strerror(err));
		return;
	}

	for (seq=0;;seq++) {
		err = gnutls_x509_aia_get(aia, seq, &oid, &san_type, &san);
		if (err == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			goto cleanup;
		if (err < 0) {
			addf(str, "error: aia_get: %s\n",
			     gnutls_strerror(err));
			goto cleanup;
		}

		if (strcmp((char*)oid.data, GNUTLS_OID_AD_OCSP) == 0)
			addf(str, _("\t\t\tAccess Method: %s (%s)\n"), GNUTLS_OID_AD_OCSP, "id-ad-ocsp");
		else if (strcmp((char*)oid.data, GNUTLS_OID_AD_CAISSUERS) == 0)
			addf(str, _("\t\t\tAccess Method: %s (%s)\n"), GNUTLS_OID_AD_CAISSUERS, "id-ad-caIssuers");
		else {
			addf(str, _("\t\t\tAccess Method: %s (%s)\n"), (char*)oid.data, "UNKNOWN");
		}

		adds(str, "\t\t\tAccess Location ");
		print_name(str, "", san_type, &san);
	}

	return;
cleanup:
	gnutls_x509_aia_deinit(aia);
}

static void print_ski(gnutls_buffer_st * str, gnutls_datum_t *der)
{
	gnutls_datum_t id = {NULL, 0};
	int err;

	err = gnutls_x509_ext_import_subject_key_id(der, &id);
	if (err < 0) {
		addf(str, "error: get_subject_key_id: %s\n",
		     gnutls_strerror(err));
		return;
	}

	adds(str, "\t\t\t");
	_gnutls_buffer_hexprint(str, id.data, id.size);
	adds(str, "\n");

	gnutls_free(id.data);
}

#define TYPE_CRT 2
#define TYPE_CRQ 3

typedef union {
	gnutls_x509_crt_t crt;
	gnutls_x509_crq_t crq;
} cert_type_t;

static void
print_aki_gn_serial(gnutls_buffer_st * str, gnutls_x509_aki_t aki)
{
	gnutls_datum_t san, other_oid, serial;
	unsigned int alt_type;
	int err;

	err =
	    gnutls_x509_aki_get_cert_issuer(aki,
					    0, &alt_type, &san, &other_oid, &serial);
	if (err < 0) {
		addf(str, "error: gnutls_x509_aki_get_cert_issuer: %s\n",
		     gnutls_strerror(err));
		return;
	}

	print_name(str, "\t\t\t", alt_type, &san);

	adds(str, "\t\t\tserial: ");
	_gnutls_buffer_hexprint(str, serial.data, serial.size);
	adds(str, "\n");
}

static void print_aki(gnutls_buffer_st * str, gnutls_datum_t *der)
{
	int err;
	gnutls_x509_aki_t aki;
	gnutls_datum_t id;

	err = gnutls_x509_aki_init(&aki);
	if (err < 0) {
		addf(str, "error: gnutls_x509_aki_init: %s\n",
		     gnutls_strerror(err));
		return;
	}

	err = gnutls_x509_ext_import_authority_key_id(der, aki, 0);
	if (err < 0) {
		addf(str, "error: gnutls_x509_ext_import_authority_key_id: %s\n",
		     gnutls_strerror(err));
		goto cleanup;
	}

	err = gnutls_x509_aki_get_id(aki, &id);
	if (err == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
		/* Check if an alternative name is there */
		print_aki_gn_serial(str, aki);
		goto cleanup;
	} else if (err < 0) {
		addf(str, "error: gnutls_x509_aki_get_id: %s\n",
		     gnutls_strerror(err));
		goto cleanup;
	}
	
	adds(str, "\t\t\t");
	_gnutls_buffer_hexprint(str, id.data, id.size);
	adds(str, "\n");

 cleanup:
	gnutls_x509_aki_deinit(aki);
}

static void
print_key_usage2(gnutls_buffer_st * str, const char *prefix, unsigned int key_usage)
{
	if (key_usage & GNUTLS_KEY_DIGITAL_SIGNATURE)
		addf(str, _("%sDigital signature.\n"), prefix);
	if (key_usage & GNUTLS_KEY_NON_REPUDIATION)
		addf(str, _("%sNon repudiation.\n"), prefix);
	if (key_usage & GNUTLS_KEY_KEY_ENCIPHERMENT)
		addf(str, _("%sKey encipherment.\n"), prefix);
	if (key_usage & GNUTLS_KEY_DATA_ENCIPHERMENT)
		addf(str, _("%sData encipherment.\n"), prefix);
	if (key_usage & GNUTLS_KEY_KEY_AGREEMENT)
		addf(str, _("%sKey agreement.\n"), prefix);
	if (key_usage & GNUTLS_KEY_KEY_CERT_SIGN)
		addf(str, _("%sCertificate signing.\n"), prefix);
	if (key_usage & GNUTLS_KEY_CRL_SIGN)
		addf(str, _("%sCRL signing.\n"), prefix);
	if (key_usage & GNUTLS_KEY_ENCIPHER_ONLY)
		addf(str, _("%sKey encipher only.\n"), prefix);
	if (key_usage & GNUTLS_KEY_DECIPHER_ONLY)
		addf(str, _("%sKey decipher only.\n"), prefix);
}

static void
print_key_usage(gnutls_buffer_st * str, const char *prefix, gnutls_datum_t *der)
{
	unsigned int key_usage;
	int err;

	err = gnutls_x509_ext_import_key_usage(der, &key_usage);
	if (err < 0) {
		addf(str, "error: get_key_usage: %s\n",
		     gnutls_strerror(err));
		return;
	}

	print_key_usage2(str, prefix, key_usage);
}

static void
print_private_key_usage_period(gnutls_buffer_st * str, const char *prefix, gnutls_datum_t *der)
{
	time_t activation, expiration;
	int err;
	char s[42];
	struct tm t;
	size_t max;

	err = gnutls_x509_ext_import_private_key_usage_period(der, &activation, &expiration);
	if (err < 0) {
		addf(str, "error: get_private_key_usage_period: %s\n",
		     gnutls_strerror(err));
		return;
	}

	max = sizeof(s);

	if (gmtime_r(&activation, &t) == NULL)
		addf(str, "error: gmtime_r (%ld)\n",
		     (unsigned long) activation);
	else if (strftime(s, max, "%a %b %d %H:%M:%S UTC %Y", &t) == 0)
		addf(str, "error: strftime (%ld)\n",
		     (unsigned long) activation);
	else
		addf(str, _("\t\t\tNot Before: %s\n"), s);

	if (gmtime_r(&expiration, &t) == NULL)
		addf(str, "error: gmtime_r (%ld)\n",
		     (unsigned long) expiration);
	else if (strftime(s, max, "%a %b %d %H:%M:%S UTC %Y", &t) == 0)
		addf(str, "error: strftime (%ld)\n",
		     (unsigned long) expiration);
	else
		addf(str, _("\t\t\tNot After: %s\n"), s);

}

static void print_crldist(gnutls_buffer_st * str, gnutls_datum_t *der)
{
	int err;
	int indx;
	gnutls_x509_crl_dist_points_t dp;
	unsigned int flags, type;
	gnutls_datum_t dist;

	err = gnutls_x509_crl_dist_points_init(&dp);
	if (err < 0) {
		addf(str, "error: gnutls_x509_crl_dist_points_init: %s\n",
		     gnutls_strerror(err));
		return;
	}

	err = gnutls_x509_ext_import_crl_dist_points(der, dp, 0);
	if (err < 0) {
		addf(str, "error: gnutls_x509_ext_import_crl_dist_points: %s\n",
		     gnutls_strerror(err));
		goto cleanup;
	}

	for (indx = 0;; indx++) {
		err =
		    gnutls_x509_crl_dist_points_get(dp, indx, &type, &dist, &flags);
		if (err == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			goto cleanup;
		else if (err < 0) {
			addf(str, "error: get_crl_dist_points: %s\n",
			     gnutls_strerror(err));
			return;
		}

		print_name(str, "\t\t\t", type, &dist);
	}
 cleanup:
 	gnutls_x509_crl_dist_points_deinit(dp);
}

static void
print_key_purpose(gnutls_buffer_st * str, const char *prefix, gnutls_datum_t *der)
{
	int indx;
	gnutls_datum_t oid;
	char *p;
	int err;
	gnutls_x509_key_purposes_t purposes;
	
	err = gnutls_x509_key_purpose_init(&purposes);
	if (err < 0) {
		addf(str, "error: gnutls_x509_key_purpose_init: %s\n",
		     gnutls_strerror(err));
		return;
	}

	err = gnutls_x509_ext_import_key_purposes(der, purposes, 0);
	if (err < 0) {
		addf(str, "error: gnutls_x509_ext_import_key_purposes: %s\n",
		     gnutls_strerror(err));
		goto cleanup;
	}

	for (indx = 0;; indx++) {
		err = gnutls_x509_key_purpose_get(purposes, indx, &oid);
		if (err == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			goto cleanup;
		else if (err < 0) {
			addf(str, "error: gnutls_x509_key_purpose_get: %s\n",
			     gnutls_strerror(err));
			goto cleanup;
		}

		p = (void*)oid.data;
		if (strcmp(p, GNUTLS_KP_TLS_WWW_SERVER) == 0)
			addf(str, _("%s\t\t\tTLS WWW Server.\n"), prefix);
		else if (strcmp(p, GNUTLS_KP_TLS_WWW_CLIENT) == 0)
			addf(str, _("%s\t\t\tTLS WWW Client.\n"), prefix);
		else if (strcmp(p, GNUTLS_KP_CODE_SIGNING) == 0)
			addf(str, _("%s\t\t\tCode signing.\n"), prefix);
		else if (strcmp(p, GNUTLS_KP_EMAIL_PROTECTION) == 0)
			addf(str, _("%s\t\t\tEmail protection.\n"),
			     prefix);
		else if (strcmp(p, GNUTLS_KP_TIME_STAMPING) == 0)
			addf(str, _("%s\t\t\tTime stamping.\n"), prefix);
		else if (strcmp(p, GNUTLS_KP_OCSP_SIGNING) == 0)
			addf(str, _("%s\t\t\tOCSP signing.\n"), prefix);
		else if (strcmp(p, GNUTLS_KP_IPSEC_IKE) == 0)
			addf(str, _("%s\t\t\tIpsec IKE.\n"), prefix);
		else if (strcmp(p, GNUTLS_KP_ANY) == 0)
			addf(str, _("%s\t\t\tAny purpose.\n"), prefix);
		else
			addf(str, "%s\t\t\t%s\n", prefix, p);
	}
 cleanup:
	gnutls_x509_key_purpose_deinit(purposes);
}

static void
print_basic(gnutls_buffer_st * str, const char *prefix, gnutls_datum_t *der)
{
	int pathlen;
	unsigned ca;
	int err;

	err = gnutls_x509_ext_import_basic_constraints(der, &ca, &pathlen);
	if (err < 0) {
		addf(str, "error: get_basic_constraints: %s\n",
		     gnutls_strerror(err));
		return;
	}

	if (ca == 0)
		addf(str, _("%s\t\t\tCertificate Authority (CA): FALSE\n"),
		     prefix);
	else
		addf(str, _("%s\t\t\tCertificate Authority (CA): TRUE\n"),
		     prefix);

	if (pathlen >= 0)
		addf(str, _("%s\t\t\tPath Length Constraint: %d\n"),
		     prefix, pathlen);
}


static void
print_altname(gnutls_buffer_st * str, const char *prefix, gnutls_datum_t *der)
{
	unsigned int altname_idx;
	gnutls_subject_alt_names_t names;
	unsigned int type;
	gnutls_datum_t san;
	gnutls_datum_t othername;
	char pfx[16];
	int err;

	err = gnutls_subject_alt_names_init(&names);
	if (err < 0) {
		addf(str, "error: gnutls_subject_alt_names_init: %s\n",
		     gnutls_strerror(err));
		return;
	}

	err = gnutls_x509_ext_import_subject_alt_names(der, names, 0);
	if (err < 0) {
		addf(str, "error: gnutls_x509_ext_import_subject_alt_names: %s\n",
		     gnutls_strerror(err));
		return;
	}

	for (altname_idx = 0;; altname_idx++) {
		err = gnutls_subject_alt_names_get(names, altname_idx,
						   &type, &san, &othername);
		if (err == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			break;
		else if (err < 0) {
			addf(str,
			     "error: gnutls_subject_alt_names_get: %s\n",
			     gnutls_strerror(err));
			break;
		}


		if (type == GNUTLS_SAN_OTHERNAME) {
			unsigned vtype;
			gnutls_datum_t virt;

			err = gnutls_x509_othername_to_virtual((char*)othername.data, &san, &vtype, &virt);
			if (err >= 0) {
				snprintf(pfx, sizeof(pfx), "%s\t\t\t", prefix);
				print_name(str, pfx, vtype, &virt);
				gnutls_free(virt.data);
				continue;
			}

			addf(str,
			     _("%s\t\t\totherName OID: %.*s\n"),
			     prefix, (int)othername.size, (char*)othername.data);
			addf(str, _("%s\t\t\totherName DER: "),
				     prefix);
			_gnutls_buffer_hexprint(str, san.data, san.size);
			addf(str, _("\n%s\t\t\totherName ASCII: "),
				     prefix);
			_gnutls_buffer_asciiprint(str, (char*)san.data, san.size);
				addf(str, "\n");
		} else {

			snprintf(pfx, sizeof(pfx), "%s\t\t\t", prefix);
			print_name(str, pfx, type, &san);
		}
	}
	gnutls_subject_alt_names_deinit(names);
}

static void
guiddump(gnutls_buffer_st * str, const char *data, size_t len,
	 const char *spc)
{
	size_t j;

	if (spc)
		adds(str, spc);
	addf(str, "{");
	addf(str, "%.2X", (unsigned char) data[3]);
	addf(str, "%.2X", (unsigned char) data[2]);
	addf(str, "%.2X", (unsigned char) data[1]);
	addf(str, "%.2X", (unsigned char) data[0]);
	addf(str, "-");
	addf(str, "%.2X", (unsigned char) data[5]);
	addf(str, "%.2X", (unsigned char) data[4]);
	addf(str, "-");
	addf(str, "%.2X", (unsigned char) data[7]);
	addf(str, "%.2X", (unsigned char) data[6]);
	addf(str, "-");
	addf(str, "%.2X", (unsigned char) data[8]);
	addf(str, "%.2X", (unsigned char) data[9]);
	addf(str, "-");
	for (j = 10; j < 16; j++) {
		addf(str, "%.2X", (unsigned char) data[j]);
	}
	addf(str, "}\n");
}

static void
print_unique_ids(gnutls_buffer_st * str, const gnutls_x509_crt_t cert)
{
	int result;
	char buf[256];		/* if its longer, we won't bother to print it */
	size_t buf_size = 256;

	result =
	    gnutls_x509_crt_get_issuer_unique_id(cert, buf, &buf_size);
	if (result >= 0) {
		addf(str, ("\t\tIssuer Unique ID:\n"));
		_gnutls_buffer_hexdump(str, buf, buf_size, "\t\t\t");
		if (buf_size == 16) {	/* this could be a GUID */
			guiddump(str, buf, buf_size, "\t\t\t");
		}
	}

	buf_size = 256;
	result =
	    gnutls_x509_crt_get_subject_unique_id(cert, buf, &buf_size);
	if (result >= 0) {
		addf(str, ("\t\tSubject Unique ID:\n"));
		_gnutls_buffer_hexdump(str, buf, buf_size, "\t\t\t");
		if (buf_size == 16) {	/* this could be a GUID */
			guiddump(str, buf, buf_size, "\t\t\t");
		}
	}
}

struct ext_indexes_st {
	int san;
	int ian;
	int proxy;
	int basic;
	int keyusage;
	int keypurpose;
	int ski;
	int aki, nc;
	int crldist, pkey_usage_period;
};

static void print_extension(gnutls_buffer_st * str, const char *prefix,
			    struct ext_indexes_st *idx, const char *oid,
			    unsigned critical, gnutls_datum_t *der)
{
	int err;
	unsigned j;
	char pfx[16];

	if (strcmp(oid, "2.5.29.19") == 0) {
		if (idx->basic) {
			addf(str,
			     "warning: more than one basic constraint\n");
		}

		addf(str, _("%s\t\tBasic Constraints (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));

		print_basic(str, prefix, der);
		idx->basic++;

	} else if (strcmp(oid, "2.5.29.14") == 0) {
		if (idx->ski) {
			addf(str,
			     "warning: more than one SKI extension\n");
		}

		addf(str,
		     _("%s\t\tSubject Key Identifier (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));

		print_ski(str, der);

		idx->ski++;
	} else if (strcmp(oid, "2.5.29.32") == 0) {
		struct gnutls_x509_policy_st policy;
		gnutls_x509_policies_t policies;
		const char *name;
		int x;

		err = gnutls_x509_policies_init(&policies);
		if (err < 0) {
			addf(str,
			     "error: certificate policies: %s\n",
			     gnutls_strerror(err));
			return;
		}

		err = gnutls_x509_ext_import_policies(der, policies, 0);
		if (err < 0) {
			addf(str,
			     "error: certificate policies import: %s\n",
			     gnutls_strerror(err));
			return;
		}

		for (x = 0;; x++) {
			err = gnutls_x509_policies_get(policies, x, &policy);
			if (err ==
			    GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				break;

			if (err < 0) {
				addf(str,
				     "error: certificate policy: %s\n",
				     gnutls_strerror(err));
				break;
			}

			if (x == 0)
				addf(str,
				     "%s\t\tCertificate Policies (%s):\n",
				     prefix,
				     critical ? _("critical") :
				     _("not critical"));

			addf(str, "%s\t\t\t%s\n", prefix, policy.oid);
			for (j = 0; j < policy.qualifiers; j++) {
				if (policy.qualifier[j].type ==
				    GNUTLS_X509_QUALIFIER_URI)
					name = "URI";
				else if (policy.qualifier[j].
					 type ==
					 GNUTLS_X509_QUALIFIER_NOTICE)
					name = "Note";
				else
					name = "Unknown qualifier";
				addf(str, "%s\t\t\t\t%s: %s\n",
				     prefix, name,
				     policy.qualifier[j].data);
			}
		}
		gnutls_x509_policies_deinit(policies);
	} else if (strcmp(oid, "2.5.29.35") == 0) {

		if (idx->aki) {
			addf(str,
			     "warning: more than one AKI extension\n");
		}

		addf(str,
		     _("%s\t\tAuthority Key Identifier (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));

		print_aki(str, der);

		idx->aki++;
	} else if (strcmp(oid, "2.5.29.15") == 0) {
		if (idx->keyusage) {
			addf(str,
			     "warning: more than one key usage extension\n");
		}

		addf(str, _("%s\t\tKey Usage (%s):\n"), prefix,
			     critical ? _("critical") : _("not critical"));

		snprintf(pfx, sizeof(pfx), "%s\t\t\t", prefix);
		print_key_usage(str, pfx, der);

		idx->keyusage++;
	} else if (strcmp(oid, "2.5.29.16") == 0) {
		if (idx->pkey_usage_period) {
			addf(str,
			     "warning: more than one private key usage period extension\n");
		}

		addf(str,
		     _("%s\t\tPrivate Key Usage Period (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));

		print_private_key_usage_period(str, prefix, der);

		idx->pkey_usage_period++;
	} else if (strcmp(oid, "2.5.29.37") == 0) {
		if (idx->keypurpose) {
			addf(str,
			     "warning: more than one key purpose extension\n");
		}

		addf(str, _("%s\t\tKey Purpose (%s):\n"), prefix,
		     critical ? _("critical") : _("not critical"));

		print_key_purpose(str, prefix, der);
		idx->keypurpose++;
	} else if (strcmp(oid, "2.5.29.17") == 0) {
		if (idx->san) {
			addf(str,
			     "warning: more than one SKI extension\n");
		}

		addf(str,
		     _("%s\t\tSubject Alternative Name (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));
			print_altname(str, prefix, der);
		idx->san++;
	} else if (strcmp(oid, "2.5.29.18") == 0) {
		if (idx->ian) {
			addf(str,
			     "warning: more than one Issuer AltName extension\n");
		}

		addf(str,
		     _("%s\t\tIssuer Alternative Name (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));

		print_altname(str, prefix, der);

		idx->ian++;
	} else if (strcmp(oid, "2.5.29.31") == 0) {
		if (idx->crldist) {
			addf(str,
			     "warning: more than one CRL distribution point\n");
		}

		addf(str,
		     _("%s\t\tCRL Distribution points (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));

		print_crldist(str, der);
		idx->crldist++;
	} else if (strcmp(oid, "1.3.6.1.5.5.7.1.14") == 0) {
		if (idx->proxy) {
			addf(str,
			     "warning: more than one proxy extension\n");
		}

		addf(str,
			     _
		     ("%s\t\tProxy Certificate Information (%s):\n"),
		     prefix,
		     critical ? _("critical") : _("not critical"));

		print_proxy(str, der);

		idx->proxy++;
	} else if (strcmp(oid, "1.3.6.1.5.5.7.1.1") == 0) {
		addf(str, _("%s\t\tAuthority Information "
			    "Access (%s):\n"), prefix,
		     critical ? _("critical") : _("not critical"));

		print_aia(str, der);
	} else if (strcmp(oid, "2.5.29.30") == 0) {
		if (idx->nc) {
			addf(str,
			     "warning: more than one name constraints extension\n");
		}
		idx->nc++;

		addf(str, _("%s\t\tName Constraints (%s):\n"), prefix,
		     critical ? _("critical") : _("not critical"));

		print_nc(str, prefix, der);
	} else {
		addf(str, _("%s\t\tUnknown extension %s (%s):\n"),
		     prefix, oid,
		     critical ? _("critical") : _("not critical"));

		addf(str, _("%s\t\t\tASCII: "), prefix);
		_gnutls_buffer_asciiprint(str, (char*)der->data, der->size);

		addf(str, "\n");
		addf(str, _("%s\t\t\tHexdump: "), prefix);
		_gnutls_buffer_hexprint(str, (char*)der->data, der->size);
		adds(str, "\n");
	}
}

static void
print_extensions(gnutls_buffer_st * str, const char *prefix, int type,
		 cert_type_t cert)
{
	unsigned i;
	int err;
	gnutls_datum_t der = {NULL, 0};
	struct ext_indexes_st idx;

	memset(&idx, 0, sizeof(idx));

	for (i = 0;; i++) {
		char oid[MAX_OID_SIZE] = "";
		size_t sizeof_oid = sizeof(oid);
		unsigned int critical;

		if (type == TYPE_CRT)
			err =
			    gnutls_x509_crt_get_extension_info(cert.crt, i,
							       oid,
							       &sizeof_oid,
							       &critical);

		else if (type == TYPE_CRQ)
			err =
			    gnutls_x509_crq_get_extension_info(cert.crq, i,
							       oid,
							       &sizeof_oid,
							       &critical);
		else {
			gnutls_assert();
			return;
		}

		if (err < 0) {
			if (err == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				break;
			addf(str, "error: get_extension_info: %s\n",
			     gnutls_strerror(err));
			continue;
		}

		if (i == 0)
			addf(str, _("%s\tExtensions:\n"), prefix);

		if (type == TYPE_CRT)
			err = gnutls_x509_crt_get_extension_data2(cert.crt, i, &der);
		else
			err = gnutls_x509_crq_get_extension_data2(cert.crq, i, &der);

		if (err < 0) {
			der.data = NULL;
			der.size = 0;
		}

		print_extension(str, prefix, &idx, oid, critical, &der);
		gnutls_free(der.data);
	}
}

static void
print_pubkey(gnutls_buffer_st * str, const char *key_name,
	     gnutls_pubkey_t pubkey,
	     gnutls_certificate_print_formats_t format)
{
	int err, pk;
	const char *name;
	unsigned bits;

	err = gnutls_pubkey_get_pk_algorithm(pubkey, &bits);
	if (err < 0) {
		addf(str, "error: get_pk_algorithm: %s\n",
		     gnutls_strerror(err));
		return;
	}

	name = gnutls_pk_algorithm_get_name(err);
	if (name == NULL)
		name = _("unknown");

	pk = err;

	addf(str, _("\t%sPublic Key Algorithm: %s\n"), key_name, name);
	addf(str, _("\tAlgorithm Security Level: %s (%d bits)\n"),
	     gnutls_sec_param_get_name(gnutls_pk_bits_to_sec_param
				       (err, bits)), bits);
	switch (pk) {
	case GNUTLS_PK_RSA:
		{
			gnutls_datum_t m, e;

			err = gnutls_pubkey_get_pk_rsa_raw(pubkey, &m, &e);
			if (err < 0)
				addf(str, "error: get_pk_rsa_raw: %s\n",
				     gnutls_strerror(err));
			else {
				if (format ==
				    GNUTLS_CRT_PRINT_FULL_NUMBERS) {
					addf(str,
					     _("\t\tModulus (bits %d): "),
					     bits);
					_gnutls_buffer_hexprint(str,
								m.data,
								m.size);
					adds(str, "\n");
					addf(str,
					     _("\t\tExponent (bits %d): "),
					     e.size * 8);
					_gnutls_buffer_hexprint(str,
								e.data,
								e.size);
					adds(str, "\n");
				} else {
					addf(str,
					     _("\t\tModulus (bits %d):\n"),
					     bits);
					_gnutls_buffer_hexdump(str, m.data,
							       m.size,
							       "\t\t\t");
					addf(str,
					     _
					     ("\t\tExponent (bits %d):\n"),
					     e.size * 8);
					_gnutls_buffer_hexdump(str, e.data,
							       e.size,
							       "\t\t\t");
				}

				gnutls_free(m.data);
				gnutls_free(e.data);
			}

		}
		break;

	case GNUTLS_PK_EC:
		{
			gnutls_datum_t x, y;
			gnutls_ecc_curve_t curve;

			err =
			    gnutls_pubkey_get_pk_ecc_raw(pubkey, &curve,
							 &x, &y);
			if (err < 0)
				addf(str, "error: get_pk_ecc_raw: %s\n",
				     gnutls_strerror(err));
			else {
				addf(str, _("\t\tCurve:\t%s\n"),
				     gnutls_ecc_curve_get_name(curve));
				if (format ==
				    GNUTLS_CRT_PRINT_FULL_NUMBERS) {
					adds(str, _("\t\tX: "));
					_gnutls_buffer_hexprint(str,
								x.data,
								x.size);
					adds(str, "\n");
					adds(str, _("\t\tY: "));
					_gnutls_buffer_hexprint(str,
								y.data,
								y.size);
					adds(str, "\n");
				} else {
					adds(str, _("\t\tX:\n"));
					_gnutls_buffer_hexdump(str, x.data,
							       x.size,
							       "\t\t\t");
					adds(str, _("\t\tY:\n"));
					_gnutls_buffer_hexdump(str, y.data,
							       y.size,
							       "\t\t\t");
				}

				gnutls_free(x.data);
				gnutls_free(y.data);

			}
		}
		break;
	case GNUTLS_PK_DSA:
		{
			gnutls_datum_t p, q, g, y;

			err =
			    gnutls_pubkey_get_pk_dsa_raw(pubkey, &p, &q,
							 &g, &y);
			if (err < 0)
				addf(str, "error: get_pk_dsa_raw: %s\n",
				     gnutls_strerror(err));
			else {
				if (format ==
				    GNUTLS_CRT_PRINT_FULL_NUMBERS) {
					addf(str,
					     _
					     ("\t\tPublic key (bits %d): "),
					     bits);
					_gnutls_buffer_hexprint(str,
								y.data,
								y.size);
					adds(str, "\n");
					addf(str, _("\t\tP: "));
					_gnutls_buffer_hexprint(str,
								p.data,
								p.size);
					adds(str, "\n");
					addf(str, _("\t\tQ: "));
					_gnutls_buffer_hexprint(str,
								q.data,
								q.size);
					adds(str, "\n");
					addf(str, _("\t\tG: "));
					_gnutls_buffer_hexprint(str,
								g.data,
								g.size);
					adds(str, "\n");
				} else {
					addf(str,
					     _
					     ("\t\tPublic key (bits %d):\n"),
					     bits);
					_gnutls_buffer_hexdump(str, y.data,
							       y.size,
							       "\t\t\t");
					adds(str, _("\t\tP:\n"));
					_gnutls_buffer_hexdump(str, p.data,
							       p.size,
							       "\t\t\t");
					adds(str, _("\t\tQ:\n"));
					_gnutls_buffer_hexdump(str, q.data,
							       q.size,
							       "\t\t\t");
					adds(str, _("\t\tG:\n"));
					_gnutls_buffer_hexdump(str, g.data,
							       g.size,
							       "\t\t\t");
				}

				gnutls_free(p.data);
				gnutls_free(q.data);
				gnutls_free(g.data);
				gnutls_free(y.data);

			}
		}
		break;

	default:
		break;
	}
}

static void
print_crt_pubkey(gnutls_buffer_st * str, gnutls_x509_crt_t crt,
		 gnutls_certificate_print_formats_t format)
{
	gnutls_pubkey_t pubkey;
	int ret;

	ret = gnutls_pubkey_init(&pubkey);
	if (ret < 0)
		return;

	ret = gnutls_pubkey_import_x509(pubkey, crt, 0);
	if (ret < 0)
		goto cleanup;

	print_pubkey(str, _("Subject "), pubkey, format);

      cleanup:
	gnutls_pubkey_deinit(pubkey);
	return;
}

static void
print_cert(gnutls_buffer_st * str, gnutls_x509_crt_t cert,
	   gnutls_certificate_print_formats_t format)
{
	/* Version. */
	{
		int version = gnutls_x509_crt_get_version(cert);
		if (version < 0)
			addf(str, "error: get_version: %s\n",
			     gnutls_strerror(version));
		else
			addf(str, _("\tVersion: %d\n"), version);
	}

	/* Serial. */
	{
		char serial[128];
		size_t serial_size = sizeof(serial);
		int err;

		err =
		    gnutls_x509_crt_get_serial(cert, serial, &serial_size);
		if (err < 0)
			addf(str, "error: get_serial: %s\n",
			     gnutls_strerror(err));
		else {
			adds(str, _("\tSerial Number (hex): "));
			_gnutls_buffer_hexprint(str, serial, serial_size);
			adds(str, "\n");
		}
	}

	/* Issuer. */
	if (format != GNUTLS_CRT_PRINT_UNSIGNED_FULL) {
		char *dn;
		size_t dn_size = 0;
		int err;

		err = gnutls_x509_crt_get_issuer_dn(cert, NULL, &dn_size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER)
			addf(str, "error: get_issuer_dn: %s\n",
			     gnutls_strerror(err));
		else {
			dn = gnutls_malloc(dn_size);
			if (!dn)
				addf(str, "error: malloc (%d): %s\n",
				     (int) dn_size,
				     gnutls_strerror
				     (GNUTLS_E_MEMORY_ERROR));
			else {
				err =
				    gnutls_x509_crt_get_issuer_dn(cert, dn,
								  &dn_size);
				if (err < 0)
					addf(str,
					     "error: get_issuer_dn: %s\n",
					     gnutls_strerror(err));
				else
					addf(str, _("\tIssuer: %s\n"), dn);
				gnutls_free(dn);
			}
		}
	}

	/* Validity. */
	{
		time_t tim;

		adds(str, _("\tValidity:\n"));

		tim = gnutls_x509_crt_get_activation_time(cert);
		{
			char s[42];
			size_t max = sizeof(s);
			struct tm t;

			if (gmtime_r(&tim, &t) == NULL)
				addf(str, "error: gmtime_r (%ld)\n",
				     (unsigned long) tim);
			else if (strftime
				 (s, max, "%a %b %d %H:%M:%S UTC %Y",
				  &t) == 0)
				addf(str, "error: strftime (%ld)\n",
				     (unsigned long) tim);
			else
				addf(str, _("\t\tNot Before: %s\n"), s);
		}

		tim = gnutls_x509_crt_get_expiration_time(cert);
		{
			char s[42];
			size_t max = sizeof(s);
			struct tm t;

			if (gmtime_r(&tim, &t) == NULL)
				addf(str, "error: gmtime_r (%ld)\n",
				     (unsigned long) tim);
			else if (strftime
				 (s, max, "%a %b %d %H:%M:%S UTC %Y",
				  &t) == 0)
				addf(str, "error: strftime (%ld)\n",
				     (unsigned long) tim);
			else
				addf(str, _("\t\tNot After: %s\n"), s);
		}
	}

	/* Subject. */
	{
		char *dn;
		size_t dn_size = 0;
		int err;

		err = gnutls_x509_crt_get_dn(cert, NULL, &dn_size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER)
			addf(str, "error: get_dn: %s\n",
			     gnutls_strerror(err));
		else {
			dn = gnutls_malloc(dn_size);
			if (!dn)
				addf(str, "error: malloc (%d): %s\n",
				     (int) dn_size,
				     gnutls_strerror
				     (GNUTLS_E_MEMORY_ERROR));
			else {
				err =
				    gnutls_x509_crt_get_dn(cert, dn,
							   &dn_size);
				if (err < 0)
					addf(str, "error: get_dn: %s\n",
					     gnutls_strerror(err));
				else
					addf(str, _("\tSubject: %s\n"),
					     dn);
				gnutls_free(dn);
			}
		}
	}

	/* SubjectPublicKeyInfo. */
	print_crt_pubkey(str, cert, format);

	print_unique_ids(str, cert);

	/* Extensions. */
	if (gnutls_x509_crt_get_version(cert) >= 3) {
		cert_type_t ccert;

		ccert.crt = cert;
		print_extensions(str, "", TYPE_CRT, ccert);
	}

	/* Signature. */
	if (format != GNUTLS_CRT_PRINT_UNSIGNED_FULL) {
		int err;
		size_t size = 0;
		char *buffer = NULL;

		err = gnutls_x509_crt_get_signature_algorithm(cert);
		if (err < 0)
			addf(str, "error: get_signature_algorithm: %s\n",
			     gnutls_strerror(err));
		else {
			const char *name =
			    gnutls_sign_algorithm_get_name(err);
			if (name == NULL)
				name = _("unknown");
			addf(str, _("\tSignature Algorithm: %s\n"), name);
		}
		if (gnutls_sign_is_secure(err) == 0) {
			adds(str,
			     _("warning: signed using a broken signature "
			       "algorithm that can be forged.\n"));
		}

		err = gnutls_x509_crt_get_signature(cert, buffer, &size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER) {
			addf(str, "error: get_signature: %s\n",
			     gnutls_strerror(err));
			return;
		}

		buffer = gnutls_malloc(size);
		if (!buffer) {
			addf(str, "error: malloc: %s\n",
			     gnutls_strerror(GNUTLS_E_MEMORY_ERROR));
			return;
		}

		err = gnutls_x509_crt_get_signature(cert, buffer, &size);
		if (err < 0) {
			gnutls_free(buffer);
			addf(str, "error: get_signature2: %s\n",
			     gnutls_strerror(err));
			return;
		}

		adds(str, _("\tSignature:\n"));
		_gnutls_buffer_hexdump(str, buffer, size, "\t\t");

		gnutls_free(buffer);
	}
}

static void
print_fingerprint(gnutls_buffer_st * str, gnutls_x509_crt_t cert,
		  gnutls_digest_algorithm_t algo)
{
	int err;
	char buffer[MAX_HASH_SIZE];
	size_t size = sizeof(buffer);

	err = gnutls_x509_crt_get_fingerprint(cert, algo, buffer, &size);
	if (err < 0) {
		addf(str, "error: get_fingerprint: %s\n",
		     gnutls_strerror(err));
		return;
	}

	addf(str, _("\t%s fingerprint:\n\t\t"), gnutls_mac_get_name((gnutls_mac_algorithm_t)algo));

	_gnutls_buffer_hexprint(str, buffer, size);
	adds(str, "\n");
}

static void print_keyid(gnutls_buffer_st * str, gnutls_x509_crt_t cert)
{
	int err;
	unsigned char buffer[32];
	size_t size = sizeof(buffer);
	const char *name;
	char *p;
	unsigned int bits;

	err = gnutls_x509_crt_get_key_id(cert, 0, buffer, &size);
	if (err < 0) {
		addf(str, "error: get_key_id: %s\n", gnutls_strerror(err));
		return;
	}

	adds(str, _("\tPublic Key ID:\n\t\t"));
	_gnutls_buffer_hexprint(str, buffer, size);
	adds(str, "\n");

	err = gnutls_x509_crt_get_pk_algorithm(cert, &bits);
	if (err < 0)
		return;

	name = gnutls_pk_get_name(err);
	if (name == NULL)
		return;

	p = _gnutls_key_fingerprint_randomart(buffer, size, name, bits,
					      "\t\t");
	if (p == NULL)
		return;

	adds(str, _("\tPublic key's random art:\n"));
	adds(str, p);
	adds(str, "\n");

	gnutls_free(p);
}

static void
print_other(gnutls_buffer_st * str, gnutls_x509_crt_t cert,
	    gnutls_certificate_print_formats_t format)
{
	if (format != GNUTLS_CRT_PRINT_UNSIGNED_FULL) {
		print_fingerprint(str, cert, GNUTLS_DIG_SHA1);
		print_fingerprint(str, cert, GNUTLS_DIG_SHA256);
	}
	print_keyid(str, cert);
}

static void print_oneline(gnutls_buffer_st * str, gnutls_x509_crt_t cert)
{
	int err;

	/* Subject. */
	{
		char *dn;
		size_t dn_size = 0;

		err = gnutls_x509_crt_get_dn(cert, NULL, &dn_size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER)
			addf(str, "unknown subject (%s), ",
			     gnutls_strerror(err));
		else {
			dn = gnutls_malloc(dn_size);
			if (!dn)
				addf(str, "unknown subject (%s), ",
				     gnutls_strerror
				     (GNUTLS_E_MEMORY_ERROR));
			else {
				err =
				    gnutls_x509_crt_get_dn(cert, dn,
							   &dn_size);
				if (err < 0)
					addf(str, "unknown subject (%s), ",
					     gnutls_strerror(err));
				else
					addf(str, "subject `%s', ", dn);
				gnutls_free(dn);
			}
		}
	}

	/* Issuer. */
	{
		char *dn;
		size_t dn_size = 0;

		err = gnutls_x509_crt_get_issuer_dn(cert, NULL, &dn_size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER)
			addf(str, "unknown issuer (%s), ",
			     gnutls_strerror(err));
		else {
			dn = gnutls_malloc(dn_size);
			if (!dn)
				addf(str, "unknown issuer (%s), ",
				     gnutls_strerror
				     (GNUTLS_E_MEMORY_ERROR));
			else {
				err =
				    gnutls_x509_crt_get_issuer_dn(cert, dn,
								  &dn_size);
				if (err < 0)
					addf(str, "unknown issuer (%s), ",
					     gnutls_strerror(err));
				else
					addf(str, "issuer `%s', ", dn);
				gnutls_free(dn);
			}
		}
	}

	/* Key algorithm and size. */
	{
		unsigned int bits;
		const char *name = gnutls_pk_algorithm_get_name
		    (gnutls_x509_crt_get_pk_algorithm(cert, &bits));
		if (name == NULL)
			name = "Unknown";
		addf(str, "%s key %d bits, ", name, bits);
	}

	/* Signature Algorithm. */
	{
		err = gnutls_x509_crt_get_signature_algorithm(cert);
		if (err < 0)
			addf(str, "unknown signature algorithm (%s), ",
			     gnutls_strerror(err));
		else {
			const char *name =
			    gnutls_sign_algorithm_get_name(err);
			if (name == NULL)
				name = _("unknown");
			if (gnutls_sign_is_secure(err) == 0)
				addf(str, _("signed using %s (broken!), "),
				     name);
			else
				addf(str, _("signed using %s, "), name);
		}
	}

	/* Validity. */
	{
		time_t tim;

		tim = gnutls_x509_crt_get_activation_time(cert);
		{
			char s[42];
			size_t max = sizeof(s);
			struct tm t;

			if (gmtime_r(&tim, &t) == NULL)
				addf(str, "unknown activation (%ld), ",
				     (unsigned long) tim);
			else if (strftime
				 (s, max, "%Y-%m-%d %H:%M:%S UTC",
				  &t) == 0)
				addf(str, "failed activation (%ld), ",
				     (unsigned long) tim);
			else
				addf(str, "activated `%s', ", s);
		}

		tim = gnutls_x509_crt_get_expiration_time(cert);
		{
			char s[42];
			size_t max = sizeof(s);
			struct tm t;

			if (gmtime_r(&tim, &t) == NULL)
				addf(str, "unknown expiry (%ld), ",
				     (unsigned long) tim);
			else if (strftime
				 (s, max, "%Y-%m-%d %H:%M:%S UTC",
				  &t) == 0)
				addf(str, "failed expiry (%ld), ",
				     (unsigned long) tim);
			else
				addf(str, "expires `%s', ", s);
		}
	}

	{
		int pathlen;
		char *policyLanguage;

		err = gnutls_x509_crt_get_proxy(cert, NULL,
						&pathlen, &policyLanguage,
						NULL, NULL);
		if (err == 0) {
			addf(str, "proxy certificate (policy=");
			if (strcmp(policyLanguage, "1.3.6.1.5.5.7.21.1") ==
			    0)
				addf(str, "id-ppl-inheritALL");
			else if (strcmp
				 (policyLanguage,
				  "1.3.6.1.5.5.7.21.2") == 0)
				addf(str, "id-ppl-independent");
			else
				addf(str, "%s", policyLanguage);
			if (pathlen >= 0)
				addf(str, ", pathlen=%d), ", pathlen);
			else
				addf(str, "), ");
			gnutls_free(policyLanguage);
		}
	}

	{
		char buffer[20];
		size_t size = sizeof(buffer);

		err =
		    gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_SHA1,
						    buffer, &size);
		if (err < 0) {
			addf(str, "unknown fingerprint (%s)",
			     gnutls_strerror(err));
		} else {
			addf(str, "SHA-1 fingerprint `");
			_gnutls_buffer_hexprint(str, buffer, size);
			adds(str, "'");
		}
	}

}

/**
 * gnutls_x509_crt_print:
 * @cert: The structure to be printed
 * @format: Indicate the format to use
 * @out: Newly allocated datum with null terminated string.
 *
 * This function will pretty print a X.509 certificate, suitable for
 * display to a human.
 *
 * If the format is %GNUTLS_CRT_PRINT_FULL then all fields of the
 * certificate will be output, on multiple lines.  The
 * %GNUTLS_CRT_PRINT_ONELINE format will generate one line with some
 * selected fields, which is useful for logging purposes.
 *
 * The output @out needs to be deallocated using gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crt_print(gnutls_x509_crt_t cert,
		      gnutls_certificate_print_formats_t format,
		      gnutls_datum_t * out)
{
	gnutls_buffer_st str;
	int ret;

	if (format == GNUTLS_CRT_PRINT_COMPACT) {
		_gnutls_buffer_init(&str);

		print_oneline(&str, cert);

		_gnutls_buffer_append_data(&str, "\n", 1);
		print_keyid(&str, cert);

		_gnutls_buffer_append_data(&str, "\0", 1);

		ret = _gnutls_buffer_to_datum(&str, out);
		if (out->size > 0)
			out->size--;

		return ret;
	} else if (format == GNUTLS_CRT_PRINT_ONELINE) {
		_gnutls_buffer_init(&str);

		print_oneline(&str, cert);

		_gnutls_buffer_append_data(&str, "\0", 1);

		ret = _gnutls_buffer_to_datum(&str, out);
		if (out->size > 0)
			out->size--;

		return ret;
	} else {
		_gnutls_buffer_init(&str);

		_gnutls_buffer_append_str(&str,
					  _
					  ("X.509 Certificate Information:\n"));

		print_cert(&str, cert, format);

		_gnutls_buffer_append_str(&str, _("Other Information:\n"));

		print_other(&str, cert, format);

		_gnutls_buffer_append_data(&str, "\0", 1);

		ret = _gnutls_buffer_to_datum(&str, out);
		if (out->size > 0)
			out->size--;

		return ret;
	}
}

static void
print_crl(gnutls_buffer_st * str, gnutls_x509_crl_t crl, int notsigned)
{
	/* Version. */
	{
		int version = gnutls_x509_crl_get_version(crl);
		if (version == GNUTLS_E_ASN1_ELEMENT_NOT_FOUND)
			adds(str, _("\tVersion: 1 (default)\n"));
		else if (version < 0)
			addf(str, "error: get_version: %s\n",
			     gnutls_strerror(version));
		else
			addf(str, _("\tVersion: %d\n"), version);
	}

	/* Issuer. */
	if (!notsigned) {
		char *dn;
		size_t dn_size = 0;
		int err;

		err = gnutls_x509_crl_get_issuer_dn(crl, NULL, &dn_size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER)
			addf(str, "error: get_issuer_dn: %s\n",
			     gnutls_strerror(err));
		else {
			dn = gnutls_malloc(dn_size);
			if (!dn)
				addf(str, "error: malloc (%d): %s\n",
				     (int) dn_size,
				     gnutls_strerror
				     (GNUTLS_E_MEMORY_ERROR));
			else {
				err =
				    gnutls_x509_crl_get_issuer_dn(crl, dn,
								  &dn_size);
				if (err < 0)
					addf(str,
					     "error: get_issuer_dn: %s\n",
					     gnutls_strerror(err));
				else
					addf(str, _("\tIssuer: %s\n"), dn);
			}
			gnutls_free(dn);
		}
	}

	/* Validity. */
	{
		time_t tim;

		adds(str, _("\tUpdate dates:\n"));

		tim = gnutls_x509_crl_get_this_update(crl);
		{
			char s[42];
			size_t max = sizeof(s);
			struct tm t;

			if (gmtime_r(&tim, &t) == NULL)
				addf(str, "error: gmtime_r (%ld)\n",
				     (unsigned long) tim);
			else if (strftime
				 (s, max, "%a %b %d %H:%M:%S UTC %Y",
				  &t) == 0)
				addf(str, "error: strftime (%ld)\n",
				     (unsigned long) tim);
			else
				addf(str, _("\t\tIssued: %s\n"), s);
		}

		tim = gnutls_x509_crl_get_next_update(crl);
		{
			char s[42];
			size_t max = sizeof(s);
			struct tm t;

			if (tim == -1)
				addf(str, "\t\tNo next update time.\n");
			else if (gmtime_r(&tim, &t) == NULL)
				addf(str, "error: gmtime_r (%ld)\n",
				     (unsigned long) tim);
			else if (strftime
				 (s, max, "%a %b %d %H:%M:%S UTC %Y",
				  &t) == 0)
				addf(str, "error: strftime (%ld)\n",
				     (unsigned long) tim);
			else
				addf(str, _("\t\tNext at: %s\n"), s);
		}
	}

	/* Extensions. */
	if (gnutls_x509_crl_get_version(crl) >= 2) {
		size_t i;
		int err = 0;
		int aki_idx = 0;
		int crl_nr = 0;

		for (i = 0;; i++) {
			char oid[MAX_OID_SIZE] = "";
			size_t sizeof_oid = sizeof(oid);
			unsigned int critical;

			err = gnutls_x509_crl_get_extension_info(crl, i,
								 oid,
								 &sizeof_oid,
								 &critical);
			if (err < 0) {
				if (err ==
				    GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
					break;
				addf(str,
				     "error: get_extension_info: %s\n",
				     gnutls_strerror(err));
				continue;
			}

			if (i == 0)
				adds(str, _("\tExtensions:\n"));

			if (strcmp(oid, "2.5.29.20") == 0) {
				char nr[128];
				size_t nr_size = sizeof(nr);

				if (crl_nr) {
					addf(str,
					     "warning: more than one CRL number\n");
				}

				err =
				    gnutls_x509_crl_get_number(crl, nr,
							       &nr_size,
							       &critical);

				addf(str, _("\t\tCRL Number (%s): "),
				     critical ? _("critical") :
				     _("not critical"));

				if (err < 0)
					addf(str,
					     "error: get_number: %s\n",
					     gnutls_strerror(err));
				else {
					_gnutls_buffer_hexprint(str, nr,
								nr_size);
					addf(str, "\n");
				}

				crl_nr++;
			} else if (strcmp(oid, "2.5.29.35") == 0) {
				gnutls_datum_t der;

				if (aki_idx) {
					addf(str,
					     "warning: more than one AKI extension\n");
				}

				addf(str,
				     _
				     ("\t\tAuthority Key Identifier (%s):\n"),
				     critical ? _("critical") :
				     _("not critical"));

				err = gnutls_x509_crl_get_extension_data2(crl, i, &der);
				if (err < 0) {
					addf(str,
					     "error: get_extension_data2: %s\n",
					     gnutls_strerror(err));
					continue;
				}
				print_aki(str, &der);
				gnutls_free(der.data);

				aki_idx++;
			} else {
				gnutls_datum_t der;

				addf(str,
				     _("\t\tUnknown extension %s (%s):\n"),
				     oid,
				     critical ? _("critical") :
				     _("not critical"));

				err =
				    gnutls_x509_crl_get_extension_data2(crl,
								       i,
								       &der);
				if (err < 0) {
					addf(str,
					     "error: get_extension_data2: %s\n",
					     gnutls_strerror(err));
					continue;
				}

				adds(str, _("\t\t\tASCII: "));
				_gnutls_buffer_asciiprint(str, (char*)der.data, der.size);
				adds(str, "\n");

				adds(str, _("\t\t\tHexdump: "));
				_gnutls_buffer_hexprint(str, der.data, der.size);
				adds(str, "\n");

				gnutls_free(der.data);
			}
		}
	}


	/* Revoked certificates. */
	{
		int num = gnutls_x509_crl_get_crt_count(crl);
		gnutls_x509_crl_iter_t iter = NULL;
		int j;

		if (num)
			addf(str, _("\tRevoked certificates (%d):\n"),
			     num);
		else
			adds(str, _("\tNo revoked certificates.\n"));

		for (j = 0; j < num; j++) {
			unsigned char serial[128];
			size_t serial_size = sizeof(serial);
			int err;
			time_t tim;

			err =
			    gnutls_x509_crl_iter_crt_serial(crl, &iter, serial,
							   &serial_size,
							   &tim);
			if (err < 0) {
				addf(str, "error: iter_crt_serial: %s\n",
				     gnutls_strerror(err));
				break;
			} else {
				char s[42];
				size_t max = sizeof(s);
				struct tm t;

				adds(str, _("\t\tSerial Number (hex): "));
				_gnutls_buffer_hexprint(str, serial,
							serial_size);
				adds(str, "\n");

				if (gmtime_r(&tim, &t) == NULL)
					addf(str,
					     "error: gmtime_r (%ld)\n",
					     (unsigned long) tim);
				else if (strftime
					 (s, max,
					  "%a %b %d %H:%M:%S UTC %Y",
					  &t) == 0)
					addf(str,
					     "error: strftime (%ld)\n",
					     (unsigned long) tim);
				else
					addf(str,
					     _("\t\tRevoked at: %s\n"), s);
			}
		}
		gnutls_x509_crl_iter_deinit(iter);
	}

	/* Signature. */
	if (!notsigned) {
		int err;
		size_t size = 0;
		char *buffer = NULL;

		err = gnutls_x509_crl_get_signature_algorithm(crl);
		if (err < 0)
			addf(str, "error: get_signature_algorithm: %s\n",
			     gnutls_strerror(err));
		else {
			const char *name =
			    gnutls_sign_algorithm_get_name(err);
			if (name == NULL)
				name = _("unknown");
			addf(str, _("\tSignature Algorithm: %s\n"), name);
		}
		if (gnutls_sign_is_secure(err) == 0) {
			adds(str,
			     _("warning: signed using a broken signature "
			       "algorithm that can be forged.\n"));
		}

		err = gnutls_x509_crl_get_signature(crl, buffer, &size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER) {
			addf(str, "error: get_signature: %s\n",
			     gnutls_strerror(err));
			return;
		}

		buffer = gnutls_malloc(size);
		if (!buffer) {
			addf(str, "error: malloc: %s\n",
			     gnutls_strerror(GNUTLS_E_MEMORY_ERROR));
			return;
		}

		err = gnutls_x509_crl_get_signature(crl, buffer, &size);
		if (err < 0) {
			gnutls_free(buffer);
			addf(str, "error: get_signature2: %s\n",
			     gnutls_strerror(err));
			return;
		}

		adds(str, _("\tSignature:\n"));
		_gnutls_buffer_hexdump(str, buffer, size, "\t\t");

		gnutls_free(buffer);
	}
}

/**
 * gnutls_x509_crl_print:
 * @crl: The structure to be printed
 * @format: Indicate the format to use
 * @out: Newly allocated datum with null terminated string.
 *
 * This function will pretty print a X.509 certificate revocation
 * list, suitable for display to a human.
 *
 * The output @out needs to be deallocated using gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_crl_print(gnutls_x509_crl_t crl,
		      gnutls_certificate_print_formats_t format,
		      gnutls_datum_t * out)
{
	gnutls_buffer_st str;
	int ret;

	_gnutls_buffer_init(&str);

	_gnutls_buffer_append_str
	    (&str, _("X.509 Certificate Revocation List Information:\n"));

	print_crl(&str, crl, format == GNUTLS_CRT_PRINT_UNSIGNED_FULL);

	_gnutls_buffer_append_data(&str, "\0", 1);

	ret = _gnutls_buffer_to_datum(&str, out);
	if (out->size > 0)
		out->size--;

	return ret;
}

static void
print_crq_pubkey(gnutls_buffer_st * str, gnutls_x509_crq_t crq,
		 gnutls_certificate_print_formats_t format)
{
	gnutls_pubkey_t pubkey;
	int ret;

	ret = gnutls_pubkey_init(&pubkey);
	if (ret < 0)
		return;

	ret = gnutls_pubkey_import_x509_crq(pubkey, crq, 0);
	if (ret < 0)
		goto cleanup;

	print_pubkey(str, _("Subject "), pubkey, format);

      cleanup:
	gnutls_pubkey_deinit(pubkey);
	return;
}

static void
print_crq(gnutls_buffer_st * str, gnutls_x509_crq_t cert,
	  gnutls_certificate_print_formats_t format)
{
	/* Version. */
	{
		int version = gnutls_x509_crq_get_version(cert);
		if (version < 0)
			addf(str, "error: get_version: %s\n",
			     gnutls_strerror(version));
		else
			addf(str, _("\tVersion: %d\n"), version);
	}

	/* Subject */
	{
		char *dn;
		size_t dn_size = 0;
		int err;

		err = gnutls_x509_crq_get_dn(cert, NULL, &dn_size);
		if (err != GNUTLS_E_SHORT_MEMORY_BUFFER)
			addf(str, "error: get_dn: %s\n",
			     gnutls_strerror(err));
		else {
			dn = gnutls_malloc(dn_size);
			if (!dn)
				addf(str, "error: malloc (%d): %s\n",
				     (int) dn_size,
				     gnutls_strerror
				     (GNUTLS_E_MEMORY_ERROR));
			else {
				err =
				    gnutls_x509_crq_get_dn(cert, dn,
							   &dn_size);
				if (err < 0)
					addf(str, "error: get_dn: %s\n",
					     gnutls_strerror(err));
				else
					addf(str, _("\tSubject: %s\n"),
					     dn);
				gnutls_free(dn);
			}
		}
	}

	/* SubjectPublicKeyInfo. */
	{
		int err;
		unsigned int bits;

		err = gnutls_x509_crq_get_pk_algorithm(cert, &bits);
		if (err < 0)
			addf(str, "error: get_pk_algorithm: %s\n",
			     gnutls_strerror(err));
		else
			print_crq_pubkey(str, cert, format);
	}

	/* parse attributes */
	{
		size_t i;
		int err = 0;
		int extensions = 0;
		int challenge = 0;

		for (i = 0;; i++) {
			char oid[MAX_OID_SIZE] = "";
			size_t sizeof_oid = sizeof(oid);

			err =
			    gnutls_x509_crq_get_attribute_info(cert, i,
							       oid,
							       &sizeof_oid);
			if (err < 0) {
				if (err ==
				    GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
					break;
				addf(str,
				     "error: get_extension_info: %s\n",
				     gnutls_strerror(err));
				continue;
			}

			if (i == 0)
				adds(str, _("\tAttributes:\n"));

			if (strcmp(oid, "1.2.840.113549.1.9.14") == 0) {
				cert_type_t ccert;

				if (extensions) {
					addf(str,
					     "warning: more than one extensionsRequest\n");
				}

				ccert.crq = cert;
				print_extensions(str, "\t", TYPE_CRQ,
						 ccert);

				extensions++;
			} else if (strcmp(oid, "1.2.840.113549.1.9.7") ==
				   0) {
				char *pass;
				size_t size;

				if (challenge) {
					adds(str,
					     "warning: more than one Challenge password attribute\n");
				}

				err =
				    gnutls_x509_crq_get_challenge_password
				    (cert, NULL, &size);
				if (err < 0
				    && err !=
				    GNUTLS_E_SHORT_MEMORY_BUFFER) {
					addf(str,
					     "error: get_challenge_password: %s\n",
					     gnutls_strerror(err));
					continue;
				}

				size++;

				pass = gnutls_malloc(size);
				if (!pass) {
					addf(str, "error: malloc: %s\n",
					     gnutls_strerror
					     (GNUTLS_E_MEMORY_ERROR));
					continue;
				}

				err =
				    gnutls_x509_crq_get_challenge_password
				    (cert, pass, &size);
				if (err < 0)
					addf(str,
					     "error: get_challenge_password: %s\n",
					     gnutls_strerror(err));
				else
					addf(str,
					     _
					     ("\t\tChallenge password: %s\n"),
					     pass);

				gnutls_free(pass);

				challenge++;
			} else {
				char *buffer;
				size_t extlen = 0;

				addf(str, _("\t\tUnknown attribute %s:\n"),
				     oid);

				err =
				    gnutls_x509_crq_get_attribute_data
				    (cert, i, NULL, &extlen);
				if (err < 0) {
					addf(str,
					     "error: get_attribute_data: %s\n",
					     gnutls_strerror(err));
					continue;
				}

				buffer = gnutls_malloc(extlen);
				if (!buffer) {
					addf(str, "error: malloc: %s\n",
					     gnutls_strerror
					     (GNUTLS_E_MEMORY_ERROR));
					continue;
				}

				err =
				    gnutls_x509_crq_get_attribute_data
				    (cert, i, buffer, &extlen);
				if (err < 0) {
					gnutls_free(buffer);
					addf(str,
					     "error: get_attribute_data2: %s\n",
					     gnutls_strerror(err));
					continue;
				}

				adds(str, _("\t\t\tASCII: "));
				_gnutls_buffer_asciiprint(str, buffer,
							  extlen);
				adds(str, "\n");

				adds(str, _("\t\t\tHexdump: "));
				_gnutls_buffer_hexprint(str, buffer,
							extlen);
				adds(str, "\n");

				gnutls_free(buffer);
			}
		}
	}
}

static void print_crq_other(gnutls_buffer_st * str, gnutls_x509_crq_t crq)
{
	int err;
	size_t size = 0;
	unsigned char *buffer = NULL;

	err = gnutls_x509_crq_get_key_id(crq, 0, buffer, &size);
	if (err != GNUTLS_E_SHORT_MEMORY_BUFFER) {
		addf(str, "error: get_key_id: %s\n", gnutls_strerror(err));
		return;
	}

	buffer = gnutls_malloc(size);
	if (!buffer) {
		addf(str, "error: malloc: %s\n",
		     gnutls_strerror(GNUTLS_E_MEMORY_ERROR));
		return;
	}

	err = gnutls_x509_crq_get_key_id(crq, 0, buffer, &size);
	if (err < 0) {
		gnutls_free(buffer);
		addf(str, "error: get_key_id2: %s\n",
		     gnutls_strerror(err));
		return;
	}

	adds(str, _("\tPublic Key ID:\n\t\t"));
	_gnutls_buffer_hexprint(str, buffer, size);
	adds(str, "\n");

	gnutls_free(buffer);
}

/**
 * gnutls_x509_crq_print:
 * @crq: The structure to be printed
 * @format: Indicate the format to use
 * @out: Newly allocated datum with null terminated string.
 *
 * This function will pretty print a certificate request, suitable for
 * display to a human.
 *
 * The output @out needs to be deallocated using gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 2.8.0
 **/
int
gnutls_x509_crq_print(gnutls_x509_crq_t crq,
		      gnutls_certificate_print_formats_t format,
		      gnutls_datum_t * out)
{
	gnutls_buffer_st str;
	int ret;

	_gnutls_buffer_init(&str);

	_gnutls_buffer_append_str
	    (&str, _("PKCS #10 Certificate Request Information:\n"));

	print_crq(&str, crq, format);

	_gnutls_buffer_append_str(&str, _("Other Information:\n"));

	print_crq_other(&str, crq);

	_gnutls_buffer_append_data(&str, "\0", 1);

	ret = _gnutls_buffer_to_datum(&str, out);
	if (out->size > 0)
		out->size--;

	return ret;
}

static void
print_pubkey_other(gnutls_buffer_st * str, gnutls_pubkey_t pubkey,
		   gnutls_certificate_print_formats_t format)
{
	uint8_t buffer[MAX_HASH_SIZE];
	size_t size = sizeof(buffer);
	int ret;
	unsigned int usage;

	ret = gnutls_pubkey_get_key_usage(pubkey, &usage);
	if (ret < 0) {
		addf(str, "error: get_key_usage: %s\n",
		     gnutls_strerror(ret));
		return;
	}

	adds(str, "\n");
	adds(str, _("Public Key Usage:\n"));
	print_key_usage2(str, "\t", pubkey->key_usage);

	ret = gnutls_pubkey_get_key_id(pubkey, 0, buffer, &size);
	if (ret < 0) {
		addf(str, "error: get_key_id: %s\n", gnutls_strerror(ret));
		return;
	}

	adds(str, "\n");
	adds(str, _("Public Key ID: "));
	_gnutls_buffer_hexprint(str, buffer, size);
	adds(str, "\n");
}

/**
 * gnutls_pubkey_print:
 * @pubkey: The structure to be printed
 * @format: Indicate the format to use
 * @out: Newly allocated datum with null terminated string.
 *
 * This function will pretty print public key information, suitable for
 * display to a human.
 *
 * Only %GNUTLS_CRT_PRINT_FULL and %GNUTLS_CRT_PRINT_FULL_NUMBERS
 * are implemented.
 *
 * The output @out needs to be deallocated using gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 *
 * Since: 3.1.5
 **/
int
gnutls_pubkey_print(gnutls_pubkey_t pubkey,
		    gnutls_certificate_print_formats_t format,
		    gnutls_datum_t * out)
{
	gnutls_buffer_st str;
	int ret;

	_gnutls_buffer_init(&str);

	_gnutls_buffer_append_str(&str, _("Public Key Information:\n"));

	print_pubkey(&str, "", pubkey, format);
	print_pubkey_other(&str, pubkey, format);

	_gnutls_buffer_append_data(&str, "\0", 1);

	ret = _gnutls_buffer_to_datum(&str, out);
	if (out->size > 0)
		out->size--;

	return ret;
}

/**
 * gnutls_x509_ext_print:
 * @exts: The structures to be printed
 * @exts_size: the number of available structures
 * @format: Indicate the format to use
 * @out: Newly allocated datum with null terminated string.
 *
 * This function will pretty print X.509 certificate extensions, 
 * suitable for display to a human.
 *
 * The output @out needs to be deallocated using gnutls_free().
 *
 * Returns: On success, %GNUTLS_E_SUCCESS (0) is returned, otherwise a
 *   negative error value.
 **/
int
gnutls_x509_ext_print(gnutls_x509_ext_st *exts, unsigned int exts_size,
		      gnutls_certificate_print_formats_t format,
		      gnutls_datum_t * out)
{
	gnutls_buffer_st str;
	struct ext_indexes_st idx;
	int ret;
	unsigned i;

	memset(&idx, 0, sizeof(idx));
	_gnutls_buffer_init(&str);

	for (i=0;i<exts_size;i++)
		print_extension(&str, "", &idx, (char*)exts[i].oid, exts[i].critical, &exts[i].data);

	_gnutls_buffer_append_data(&str, "\x00", 1);

	ret = _gnutls_buffer_to_datum(&str, out);
	if (out->size > 0)
		out->size--;
	return ret;
}
