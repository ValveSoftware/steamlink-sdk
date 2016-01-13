/*
 * Copyright (C) 2003-2014 Free Software Foundation, Inc.
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
#include <libtasn1.h>
#include <gnutls_datum.h>
#include <gnutls_global.h>
#include <gnutls_errors.h>
#include <gnutls_str.h>
#include <gnutls_x509.h>
#include <gnutls_num.h>
#include <x509_b64.h>
#include "x509_int.h"
#include <common.h>
#include <c-ctype.h>

static int
data2hex(const void *data, size_t data_size,
	 void *_out, size_t * sizeof_out);

struct oid_to_string {
	const char *oid;
	unsigned oid_size;
	const char *ldap_desc;
	unsigned ldap_desc_size;
	const char *asn_desc;	/* description in the pkix file if complex type */
	unsigned int etype;	/* the libtasn1 ASN1_ETYPE or INVALID
				 * if cannot be simply parsed */
};

#define ENTRY(oid, ldap, asn, etype) {oid, sizeof(oid)-1, ldap, sizeof(ldap)-1, asn, etype}

/* when there is no ldap description */
#define ENTRY_ND(oid, asn, etype) {oid, sizeof(oid)-1, NULL, 0, asn, etype}

/* This list contains all the OIDs that may be
 * contained in a rdnSequence and are printable.
 */
static const struct oid_to_string _oid2str[] = {
	/* PKIX
	 */
	ENTRY("1.3.6.1.5.5.7.9.2", "placeOfBirth", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("1.3.6.1.5.5.7.9.3", "gender", NULL, ASN1_ETYPE_PRINTABLE_STRING),
	ENTRY("1.3.6.1.5.5.7.9.4", "countryOfCitizenship", NULL,
	 ASN1_ETYPE_PRINTABLE_STRING),
	ENTRY("1.3.6.1.5.5.7.9.5", "countryOfResidence", NULL,
	 ASN1_ETYPE_PRINTABLE_STRING),

	ENTRY("2.5.4.6", "C", NULL, ASN1_ETYPE_PRINTABLE_STRING),
	ENTRY("2.5.4.9", "street", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.12", "title", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.10", "O", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.11", "OU", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.3", "CN", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.7", "L", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.8", "ST", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.13", "description", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),

	ENTRY("2.5.4.5", "serialNumber", NULL, ASN1_ETYPE_PRINTABLE_STRING),
	ENTRY("2.5.4.20", "telephoneNumber", NULL, ASN1_ETYPE_PRINTABLE_STRING),
	ENTRY("2.5.4.4", "surName", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.43", "initials", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.44", "generationQualifier", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.42", "givenName", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.65", "pseudonym", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.46", "dnQualifier", NULL, ASN1_ETYPE_PRINTABLE_STRING),
	ENTRY("2.5.4.17", "postalCode", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.41", "name", "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("2.5.4.15", "businessCategory", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),

	ENTRY("0.9.2342.19200300.100.1.25", "DC", NULL, ASN1_ETYPE_IA5_STRING),
	ENTRY("0.9.2342.19200300.100.1.1", "UID", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),
	ENTRY("1.2.840.113556.1.4.656", "userPrincipalName", "PKIX1.DirectoryString",
	 ASN1_ETYPE_INVALID),

	/* Extended validation
	 */
	ENTRY("1.3.6.1.4.1.311.60.2.1.1",
	 "jurisdictionOfIncorporationLocalityName",
	 "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("1.3.6.1.4.1.311.60.2.1.2",
	 "jurisdictionOfIncorporationStateOrProvinceName",
	 "PKIX1.DirectoryString", ASN1_ETYPE_INVALID),
	ENTRY("1.3.6.1.4.1.311.60.2.1.3",
	 "jurisdictionOfIncorporationCountryName",
	 NULL, ASN1_ETYPE_PRINTABLE_STRING),

	/* PKCS #9
	 */
	ENTRY("1.2.840.113549.1.9.1", "EMAIL", NULL, ASN1_ETYPE_IA5_STRING),
	ENTRY_ND("1.2.840.113549.1.9.7", "PKIX1.pkcs-9-challengePassword",
	 ASN1_ETYPE_INVALID),

	/* friendly name */
	ENTRY_ND("1.2.840.113549.1.9.20", NULL, ASN1_ETYPE_BMP_STRING),
	/* local key id */
	ENTRY_ND("1.2.840.113549.1.9.21", NULL, ASN1_ETYPE_OCTET_STRING),

	/* rfc3920 section 5.1.1 */
	ENTRY("1.3.6.1.5.5.7.8.5", "XmppAddr", NULL, ASN1_ETYPE_UTF8_STRING),

	{NULL, 0, NULL, 0, NULL, 0}
};

int _san_othername_to_virtual(const char *oid, size_t size)
{
	if (oid) {
		if ((unsigned) size == (sizeof(XMPP_OID)-1)
		    && memcmp(oid, XMPP_OID, sizeof(XMPP_OID)-1) == 0)
			return GNUTLS_SAN_OTHERNAME_XMPP;
	}

	return GNUTLS_SAN_OTHERNAME;
} 

static const struct oid_to_string *get_oid_entry(const char *oid)
{
	unsigned int i = 0;
	unsigned len = strlen(oid);

	do {
		if (len == _oid2str[i].oid_size &&
			strcmp(_oid2str[i].oid, oid) == 0)
			return &_oid2str[i];
		i++;
	}
	while (_oid2str[i].oid != NULL);

	return NULL;
}

const char *_gnutls_ldap_string_to_oid(const char *str, unsigned str_len)
{
	unsigned int i = 0;

	do {
		if ((_oid2str[i].ldap_desc != NULL) &&
		    (str_len == _oid2str[i].ldap_desc_size) &&
		    (strncasecmp(_oid2str[i].ldap_desc, str, str_len) ==
		     0))
			return _oid2str[i].oid;
		i++;
	}
	while (_oid2str[i].oid != NULL);

	return NULL;
}

/* Escapes a string following the rules from RFC4514.
 */
static int str_escape(const gnutls_datum_t * str, gnutls_datum_t * escaped)
{
	unsigned int j, i;
	uint8_t *buffer = NULL;
	int ret;

	if (str == NULL)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	/* the string will be at most twice the original */
	buffer = gnutls_malloc(str->size * 2 + 2);
	if (buffer == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	for (i = j = 0; i < str->size; i++) {
		if (str->data[i] == 0) {
			/* this is handled earlier */
			ret = gnutls_assert_val(GNUTLS_E_ASN1_DER_ERROR);
			goto cleanup;
		}

		if (str->data[i] == ',' || str->data[i] == '+'
		    || str->data[i] == '"' || str->data[i] == '\\'
		    || str->data[i] == '<' || str->data[i] == '>'
		    || str->data[i] == ';' || str->data[i] == 0)
			buffer[j++] = '\\';
		else if (i == 0 && str->data[i] == '#')
			buffer[j++] = '\\';
		else if (i == 0 && str->data[i] == ' ')
			buffer[j++] = '\\';
		else if (i == (str->size - 1) && str->data[i] == ' ')
			buffer[j++] = '\\';

		buffer[j++] = str->data[i];
	}

	/* null terminate the string */
	buffer[j] = 0;
	escaped->data = buffer;
	escaped->size = j;

	return 0;
      cleanup:
	gnutls_free(buffer);
	return ret;
}

/**
 * gnutls_x509_dn_oid_known:
 * @oid: holds an Object Identifier in a null terminated string
 *
 * This function will inform about known DN OIDs. This is useful since
 * functions like gnutls_x509_crt_set_dn_by_oid() use the information
 * on known OIDs to properly encode their input. Object Identifiers
 * that are not known are not encoded by these functions, and their
 * input is stored directly into the ASN.1 structure. In that case of
 * unknown OIDs, you have the responsibility of DER encoding your
 * data.
 *
 * Returns: 1 on known OIDs and 0 otherwise.
 **/
int gnutls_x509_dn_oid_known(const char *oid)
{
	unsigned int i = 0;
	unsigned len = strlen(oid);

	do {
		if (len == _oid2str[i].oid_size &&
			strcmp(_oid2str[i].oid, oid) == 0)
			return 1;
		i++;
	}
	while (_oid2str[i].oid != NULL);

	return 0;
}

/**
 * gnutls_x509_dn_oid_name:
 * @oid: holds an Object Identifier in a null terminated string
 * @flags: 0 or GNUTLS_X509_DN_OID_*
 *
 * This function will return the name of a known DN OID. If
 * %GNUTLS_X509_DN_OID_RETURN_OID is specified this function
 * will return the given OID if no descriptive name has been
 * found.
 *
 * Returns: A null terminated string or NULL otherwise.
 *
 * Since: 3.0
 **/
const char *gnutls_x509_dn_oid_name(const char *oid, unsigned int flags)
{
	unsigned int i = 0;
	unsigned len = strlen(oid);

	do {
		if ((_oid2str[i].oid_size == len) && 
			strcmp(_oid2str[i].oid, oid) == 0 && _oid2str[i].ldap_desc != NULL)
			return _oid2str[i].ldap_desc;
		i++;
	}
	while (_oid2str[i].oid != NULL);

	if (flags & GNUTLS_X509_DN_OID_RETURN_OID)
		return oid;
	else
		return NULL;
}

static int
make_printable_string(unsigned etype, const gnutls_datum_t * input,
		      gnutls_datum_t * out)
{
	int printable = 0;
	int ret;
	unsigned int i;
	size_t size;

	if (etype == ASN1_ETYPE_BMP_STRING) {
		ret = _gnutls_ucs2_to_utf8(input->data, input->size, out);
		if (ret < 0) {
			/* could not convert. Handle it as non-printable */
			printable = 0;
		} else
			printable = 1;
	} else if (etype == ASN1_ETYPE_TELETEX_STRING) {
		int ascii = 0;
		/* HACK: if the teletex string contains only ascii
		 * characters then treat it as printable.
		 */
		for (i = 0; i < input->size; i++)
			if (!c_isascii(input->data[i]))
				ascii = 1;

		if (ascii == 0) {
			out->data = gnutls_malloc(input->size + 1);
			if (out->data == NULL)
				return
				    gnutls_assert_val
				    (GNUTLS_E_MEMORY_ERROR);

			memcpy(out->data, input->data, input->size);
			out->size = input->size;

			out->data[out->size] = 0;

			printable = 1;
		}
	} else if (etype != ASN1_ETYPE_UNIVERSAL_STRING)	/* supported but not printable */
		return GNUTLS_E_INVALID_REQUEST;

	if (printable == 0) {	/* need to allocate out */
		out->size = input->size * 2 + 2;
		out->data = gnutls_malloc(out->size);
		if (out->data == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		size = out->size;
		ret = data2hex(input->data, input->size, out->data, &size);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
		out->size = size;
	}

	return 0;

      cleanup:
	_gnutls_free_datum(out);
	return ret;
}

static int
decode_complex_string(const struct oid_to_string *oentry, void *value,
		      int value_size, gnutls_datum_t * out)
{
	char str[MAX_STRING_LEN], tmpname[128];
	int len = -1, result;
	ASN1_TYPE tmpasn = ASN1_TYPE_EMPTY;
	char asn1_err[ASN1_MAX_ERROR_DESCRIPTION_SIZE] = "";
	unsigned int etype;
	gnutls_datum_t td = {NULL, 0};

	if (oentry->asn_desc == NULL) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	if ((result =
	     asn1_create_element(_gnutls_get_pkix(), oentry->asn_desc,
				 &tmpasn)) != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	if ((result =
	     asn1_der_decoding(&tmpasn, value, value_size,
			       asn1_err)) != ASN1_SUCCESS) {
		gnutls_assert();
		_gnutls_debug_log("asn1_der_decoding: %s\n", asn1_err);
		asn1_delete_structure(&tmpasn);
		return _gnutls_asn2err(result);
	}

	/* Read the type of choice.
	 */
	len = sizeof(str) - 1;
	if ((result = asn1_read_value(tmpasn, "", str, &len)) != ASN1_SUCCESS) {	/* CHOICE */
		gnutls_assert();
		asn1_delete_structure(&tmpasn);
		return _gnutls_asn2err(result);
	}

	str[len] = 0;

	/* We set the etype on the strings that may need
	 * some conversion to UTF-8. The INVALID flag indicates
	 * no conversion needed */
	if (strcmp(str, "teletexString") == 0)
		etype = ASN1_ETYPE_TELETEX_STRING;
	else if (strcmp(str, "bmpString") == 0)
		etype = ASN1_ETYPE_BMP_STRING;
	else if (strcmp(str, "universalString") == 0)
		etype = ASN1_ETYPE_UNIVERSAL_STRING;
	else
		etype = ASN1_ETYPE_INVALID;

	_gnutls_str_cpy(tmpname, sizeof(tmpname), str);

	result = _gnutls_x509_read_value(tmpasn, tmpname, &td);
	asn1_delete_structure(&tmpasn);
	if (result < 0)
		return gnutls_assert_val(result);

	if (etype != ASN1_ETYPE_INVALID) {
		result = make_printable_string(etype, &td, out);

		_gnutls_free_datum(&td);

		if (result < 0)
			return gnutls_assert_val(result);
	} else {
		out->data = td.data;
		out->size = td.size;
		/* _gnutls_x509_read_value always null terminates */
	}

	/* Refuse to deal with strings containing NULs. */
	if (strlen((void *) out->data) != (size_t) out->size) {
		_gnutls_free_datum(out);
		return gnutls_assert_val(GNUTLS_E_ASN1_DER_ERROR);
	}

	return 0;
}


/* This function will convert an attribute value, specified by the OID,
 * to a string. The result will be a null terminated string.
 *
 * res may be null. This will just return the res_size, needed to
 * hold the string.
 */
int
_gnutls_x509_dn_to_string(const char *oid, void *value,
			  int value_size, gnutls_datum_t * str)
{
	const struct oid_to_string *oentry;
	int ret;
	gnutls_datum_t tmp;
	size_t size;

	if (value == NULL || value_size <= 0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	oentry = get_oid_entry(oid);
	if (oentry == NULL) {	/* unknown OID -> hex */
 unknown_oid:
		str->size = value_size * 2 + 2;
		str->data = gnutls_malloc(str->size);
		if (str->data == NULL)
			return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

		size = str->size;
		ret = data2hex(value, value_size, str->data, &size);
		if (ret < 0) {
			gnutls_assert();
			gnutls_free(str->data);
			return ret;
		}
		str->size = size;
		return 0;
	}

	if (oentry->asn_desc != NULL) {	/* complex */
		ret =
		    decode_complex_string(oentry, value, value_size, &tmp);
		if (ret < 0) {
			/* we failed decoding -> handle it as unknown OID */
			goto unknown_oid;
		}
	} else {
		ret =
		    _gnutls_x509_decode_string(oentry->etype, value,
					       value_size, &tmp);
		if (ret < 0) {
			/* we failed decoding -> handle it as unknown OID */
			goto unknown_oid;
		}
	}

	ret = str_escape(&tmp, str);
	_gnutls_free_datum(&tmp);

	if (ret < 0)
		return gnutls_assert_val(ret);

	return 0;
}

/* Converts a data string to an LDAP rfc2253 hex string
 * something like '#01020304'
 */
static int
data2hex(const void *data, size_t data_size,
	 void *_out, size_t * sizeof_out)
{
	char *res;
	char escaped[MAX_STRING_LEN];
	unsigned int size, res_size;
	char *out = _out;

	if (2 * data_size + 1 > MAX_STRING_LEN) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	res =
	    _gnutls_bin2hex(data, data_size, escaped, sizeof(escaped),
			    NULL);
	if (!res) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	res_size = strlen(res);
	size = res_size + 1;	/* +1 for the '#' */
	if (size + 1 > *sizeof_out) {
		*sizeof_out = size + 1;
		return GNUTLS_E_SHORT_MEMORY_BUFFER;
	}
	*sizeof_out = size;	/* -1 for the null +1 for the '#' */

	if (out) {
		out[0] = '#';
		memcpy(&out[1], res, res_size);
		out[size] = 0;
	}

	return 0;
}


/* TIME functions 
 * Convertions between generalized or UTC time to time_t
 *
 */

/* This is an emulations of the struct tm.
 * Since we do not use libc's functions, we don't need to
 * depend on the libc structure.
 */
typedef struct fake_tm {
	int tm_mon;
	int tm_year;		/* FULL year - ie 1971 */
	int tm_mday;
	int tm_hour;
	int tm_min;
	int tm_sec;
} fake_tm;

/* The mktime_utc function is due to Russ Allbery (rra@stanford.edu),
 * who placed it under public domain:
 */

/* The number of days in each month. 
 */
static const int MONTHDAYS[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

    /* Whether a given year is a leap year. */
#define ISLEAP(year) \
        (((year) % 4) == 0 && (((year) % 100) != 0 || ((year) % 400) == 0))

/*
 **  Given a struct tm representing a calendar time in UTC, convert it to
 **  seconds since epoch.  Returns (time_t) -1 if the time is not
 **  convertable.  Note that this function does not canonicalize the provided
 **  struct tm, nor does it allow out of range values or years before 1970.
 */
static time_t mktime_utc(const struct fake_tm *tm)
{
	time_t result = 0;
	int i;

/* We do allow some ill-formed dates, but we don't do anything special
 * with them and our callers really shouldn't pass them to us.  Do
 * explicitly disallow the ones that would cause invalid array accesses
 * or other algorithm problems. 
 */
	if (tm->tm_mon < 0 || tm->tm_mon > 11 || tm->tm_year < 1970)
		return (time_t) - 1;

/* Convert to a time_t. 
 */
	for (i = 1970; i < tm->tm_year; i++)
		result += 365 + ISLEAP(i);
	for (i = 0; i < tm->tm_mon; i++)
		result += MONTHDAYS[i];
	if (tm->tm_mon > 1 && ISLEAP(tm->tm_year))
		result++;
	result = 24 * (result + tm->tm_mday - 1) + tm->tm_hour;
	result = 60 * result + tm->tm_min;
	result = 60 * result + tm->tm_sec;
	return result;
}


/* this one will parse dates of the form:
 * month|day|hour|minute|sec* (2 chars each)
 * and year is given. Returns a time_t date.
 */
static time_t time2gtime(const char *ttime, int year)
{
	char xx[4];
	struct fake_tm etime;

	if (strlen(ttime) < 8) {
		gnutls_assert();
		return (time_t) - 1;
	}

	etime.tm_year = year;

	/* In order to work with 32 bit
	 * time_t.
	 */
	if (sizeof(time_t) <= 4 && etime.tm_year >= 2038)
		return (time_t) 2145914603;	/* 2037-12-31 23:23:23 */

	if (etime.tm_year < 1970)
		return (time_t) 0;

	xx[2] = 0;

/* get the month
 */
	memcpy(xx, ttime, 2);	/* month */
	etime.tm_mon = atoi(xx) - 1;
	ttime += 2;

/* get the day
 */
	memcpy(xx, ttime, 2);	/* day */
	etime.tm_mday = atoi(xx);
	ttime += 2;

/* get the hour
 */
	memcpy(xx, ttime, 2);	/* hour */
	etime.tm_hour = atoi(xx);
	ttime += 2;

/* get the minutes
 */
	memcpy(xx, ttime, 2);	/* minutes */
	etime.tm_min = atoi(xx);
	ttime += 2;

	if (strlen(ttime) >= 2) {
		memcpy(xx, ttime, 2);
		etime.tm_sec = atoi(xx);
	} else
		etime.tm_sec = 0;

	return mktime_utc(&etime);
}


/* returns a time_t value that contains the given time.
 * The given time is expressed as:
 * YEAR(2)|MONTH(2)|DAY(2)|HOUR(2)|MIN(2)|SEC(2)*
 *
 * (seconds are optional)
 */
static time_t utcTime2gtime(const char *ttime)
{
	char xx[3];
	int year;

	if (strlen(ttime) < 10) {
		gnutls_assert();
		return (time_t) - 1;
	}
	xx[2] = 0;
/* get the year
 */
	memcpy(xx, ttime, 2);	/* year */
	year = atoi(xx);
	ttime += 2;

	if (year > 49)
		year += 1900;
	else
		year += 2000;

	return time2gtime(ttime, year);
}

/* returns a time_t value that contains the given time.
 * The given time is expressed as:
 * YEAR(4)|MONTH(2)|DAY(2)|HOUR(2)|MIN(2)|SEC(2)*
 */
time_t _gnutls_x509_generalTime2gtime(const char *ttime)
{
	char xx[5];
	int year;

	if (strlen(ttime) < 12) {
		gnutls_assert();
		return (time_t) - 1;
	}

	if (strchr(ttime, 'Z') == 0) {
		gnutls_assert();
		/* sorry we don't support it yet
		 */
		return (time_t) - 1;
	}
	xx[4] = 0;

/* get the year
 */
	memcpy(xx, ttime, 4);	/* year */
	year = atoi(xx);
	ttime += 4;

	return time2gtime(ttime, year);
}

static int
gtime2generalTime(time_t gtime, char *str_time, size_t str_time_size)
{
	size_t ret;
	struct tm _tm;
	
	if (gtime == (time_t)-1
#if SIZEOF_LONG == 8
		|| gtime >= 253402210800
#endif
	 ) {
        	snprintf(str_time, str_time_size, "99991231235959Z");
        	return 0;
	}

	if (!gmtime_r(&gtime, &_tm)) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	ret = strftime(str_time, str_time_size, "%Y%m%d%H%M%SZ", &_tm);
	if (!ret) {
		gnutls_assert();
		return GNUTLS_E_SHORT_MEMORY_BUFFER;
	}


	return 0;
}


/* Extracts the time in time_t from the ASN1_TYPE given. When should
 * be something like "tbsCertList.thisUpdate".
 */
#define MAX_TIME 64
time_t _gnutls_x509_get_time(ASN1_TYPE c2, const char *when, int nochoice)
{
	char ttime[MAX_TIME];
	char name[128];
	time_t c_time = (time_t) - 1;
	int len, result;

	len = sizeof(ttime) - 1;
	result = asn1_read_value(c2, when, ttime, &len);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return (time_t) (-1);
	}

	if (nochoice != 0) {
		c_time = _gnutls_x509_generalTime2gtime(ttime);
	} else {
		_gnutls_str_cpy(name, sizeof(name), when);

		/* choice */
		if (strcmp(ttime, "generalTime") == 0) {
			_gnutls_str_cat(name, sizeof(name),
					".generalTime");
			len = sizeof(ttime) - 1;
			result = asn1_read_value(c2, name, ttime, &len);
			if (result == ASN1_SUCCESS)
				c_time =
				    _gnutls_x509_generalTime2gtime(ttime);
		} else {	/* UTCTIME */
			_gnutls_str_cat(name, sizeof(name), ".utcTime");
			len = sizeof(ttime) - 1;
			result = asn1_read_value(c2, name, ttime, &len);
			if (result == ASN1_SUCCESS)
				c_time = utcTime2gtime(ttime);
		}

		/* We cannot handle dates after 2031 in 32 bit machines.
		 * a time_t of 64bits has to be used.
		 */
		if (result != ASN1_SUCCESS) {
			gnutls_assert();
			return (time_t) (-1);
		}
	}

	return c_time;
}

/* Sets the time in time_t in the ASN1_TYPE given. Where should
 * be something like "tbsCertList.thisUpdate".
 */
int
_gnutls_x509_set_time(ASN1_TYPE c2, const char *where, time_t tim,
		      int nochoice)
{
	char str_time[MAX_TIME];
	char name[128];
	int result, len;

	if (nochoice != 0) {
		result =
		    gtime2generalTime(tim, str_time, sizeof(str_time));
		if (result < 0)
			return gnutls_assert_val(result);
		len = strlen(str_time);
		result = asn1_write_value(c2, where, str_time, len);
		if (result != ASN1_SUCCESS)
			return gnutls_assert_val(_gnutls_asn2err(result));

		return 0;
	}

	_gnutls_str_cpy(name, sizeof(name), where);

	if ((result = asn1_write_value(c2, name, "generalTime", 1)) < 0) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	result = gtime2generalTime(tim, str_time, sizeof(str_time));
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	_gnutls_str_cat(name, sizeof(name), ".generalTime");

	len = strlen(str_time);
	result = asn1_write_value(c2, name, str_time, len);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	return 0;
}


gnutls_x509_subject_alt_name_t _gnutls_x509_san_find_type(char *str_type)
{
	if (strcmp(str_type, "dNSName") == 0)
		return GNUTLS_SAN_DNSNAME;
	if (strcmp(str_type, "rfc822Name") == 0)
		return GNUTLS_SAN_RFC822NAME;
	if (strcmp(str_type, "uniformResourceIdentifier") == 0)
		return GNUTLS_SAN_URI;
	if (strcmp(str_type, "iPAddress") == 0)
		return GNUTLS_SAN_IPADDRESS;
	if (strcmp(str_type, "otherName") == 0)
		return GNUTLS_SAN_OTHERNAME;
	if (strcmp(str_type, "directoryName") == 0)
		return GNUTLS_SAN_DN;
	return (gnutls_x509_subject_alt_name_t) - 1;
}

/* A generic export function. Will export the given ASN.1 encoded data
 * to PEM or DER raw data.
 */
int
_gnutls_x509_export_int_named(ASN1_TYPE asn1_data, const char *name,
			      gnutls_x509_crt_fmt_t format,
			      const char *pem_header,
			      unsigned char *output_data,
			      size_t * output_data_size)
{
	int ret;
	gnutls_datum_t out = {NULL,0};
	size_t size;

	ret = _gnutls_x509_export_int_named2(asn1_data, name,
					     format, pem_header, &out);
	if (ret < 0)
		return gnutls_assert_val(ret);

	if (format == GNUTLS_X509_FMT_PEM)
		size = out.size + 1;
	else
		size = out.size;

	if (*output_data_size < size) {
		*output_data_size = size;
		ret = gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);
		goto cleanup;
	}

	*output_data_size = (size_t) out.size;
	if (output_data) {
		memcpy(output_data, out.data, (size_t) out.size);
		if (format == GNUTLS_X509_FMT_PEM)
			output_data[out.size] = 0;
	}

	ret = 0;

      cleanup:
	gnutls_free(out.data);

	return ret;
}

/* A generic export function. Will export the given ASN.1 encoded data
 * to PEM or DER raw data.
 */
int
_gnutls_x509_export_int_named2(ASN1_TYPE asn1_data, const char *name,
			       gnutls_x509_crt_fmt_t format,
			       const char *pem_header,
			       gnutls_datum_t * out)
{
	int ret;

	if (format == GNUTLS_X509_FMT_DER) {
		ret = _gnutls_x509_der_encode(asn1_data, name, out, 0);
		if (ret < 0)
			return gnutls_assert_val(ret);
	} else {		/* PEM */
		gnutls_datum_t tmp;

		ret = _gnutls_x509_der_encode(asn1_data, name, &tmp, 0);
		if (ret < 0)
			return gnutls_assert_val(ret);

		ret =
		    _gnutls_fbase64_encode(pem_header, tmp.data, tmp.size,
					   out);
		_gnutls_free_datum(&tmp);

		if (ret < 0)
			return gnutls_assert_val(ret);
	}

	return 0;
}

/* Decodes an octet string. The etype specifies the string type.
 * The returned string is always null terminated (but null is not
 * included in size).
 */
int
_gnutls_x509_decode_string(unsigned int etype,
			   const uint8_t * der, size_t der_size,
			   gnutls_datum_t * output)
{
	int ret;
	const uint8_t *str;
	unsigned int str_size, len;
	gnutls_datum_t td;

	ret =
	    asn1_decode_simple_der(etype, der, der_size, &str, &str_size);
	if (ret != ASN1_SUCCESS) {
		gnutls_assert();
		ret = _gnutls_asn2err(ret);
		return ret;
	}

	td.size = str_size;
	td.data = gnutls_malloc(str_size + 1);
	if (td.data == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	memcpy(td.data, str, str_size);
	td.data[str_size] = 0;

	ret = make_printable_string(etype, &td, output);
	if (ret == GNUTLS_E_INVALID_REQUEST) {	/* unsupported etype */
		output->data = td.data;
		output->size = td.size;
		ret = 0;
	} else if (ret <= 0) {
		_gnutls_free_datum(&td);
	}

	/* Refuse to deal with strings containing NULs. */
	if (etype != ASN1_ETYPE_OCTET_STRING) {
		if (output->data)
			len = strlen((void *) output->data);
		else
			len = 0;

		if (len != (size_t) output->size) {
			_gnutls_free_datum(output);
			ret = gnutls_assert_val(GNUTLS_E_ASN1_DER_ERROR);
		}
	}

	return ret;
}


/* Reads a value from an ASN1 tree, and puts the output
 * in an allocated variable in the given datum.
 *
 * Note that this function always allocates one plus
 * the required data size (and places a null byte).
 */
int
_gnutls_x509_read_value(ASN1_TYPE c, const char *root,
			gnutls_datum_t * ret)
{
	int len = 0, result;
	uint8_t *tmp = NULL;
	unsigned int etype;

	result = asn1_read_value_type(c, root, NULL, &len, &etype);
	if (result == 0 && len == 0) {
		/* don't allow null strings */
		return gnutls_assert_val(GNUTLS_E_ASN1_DER_ERROR);
	}

	if (result != ASN1_MEM_ERROR) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		return result;
	}

	if (etype == ASN1_ETYPE_BIT_STRING) {
		len = (len + 7)/8;
	}

	tmp = gnutls_malloc((size_t) len + 1);
	if (tmp == NULL) {
		gnutls_assert();
		result = GNUTLS_E_MEMORY_ERROR;
		goto cleanup;
	}

	result = asn1_read_value(c, root, tmp, &len);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	if (etype == ASN1_ETYPE_BIT_STRING) {
		ret->size = (len+7) / 8;
	} else {
		ret->size = (unsigned) len;
	}

	tmp[ret->size] = 0;
	ret->data = tmp;

	return 0;

      cleanup:
	gnutls_free(tmp);
	return result;
}

/* Reads a value from an ASN1 tree, then interprets it as the provided
 * type of string and returns the output in an allocated variable.
 *
 * Note that this function always places a null character
 * at the end of a readable string value (which is not accounted into size)
 */
int
_gnutls_x509_read_string(ASN1_TYPE c, const char *root,
			 gnutls_datum_t * ret, unsigned int etype)
{
	int len = 0, result;
	size_t slen;
	uint8_t *tmp = NULL;
	unsigned rtype;

	result = asn1_read_value_type(c, root, NULL, &len, &rtype);
	if (result != ASN1_MEM_ERROR) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		return result;
	}

	if (rtype == ASN1_ETYPE_BIT_STRING)
		len /= 8;

	tmp = gnutls_malloc((size_t) len + 1);
	if (tmp == NULL) {
		gnutls_assert();
		result = GNUTLS_E_MEMORY_ERROR;
		goto cleanup;
	}

	result = asn1_read_value(c, root, tmp, &len);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	if (rtype == ASN1_ETYPE_BIT_STRING)
		len /= 8;

	/* Extract the STRING.
	 */
	slen = (size_t) len;

	result = _gnutls_x509_decode_string(etype, tmp, slen, ret);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}
	gnutls_free(tmp);

	return 0;

      cleanup:
	gnutls_free(tmp);
	return result;
}

/* The string type should be IA5String, UTF8String etc. Leave
 * null for octet string */
int _gnutls_x509_encode_string(unsigned int etype,
			       const void *input_data, size_t input_size,
			       gnutls_datum_t * output)
{
	uint8_t tl[ASN1_MAX_TL_SIZE];
	unsigned int tl_size;
	int ret;

	tl_size = sizeof(tl);
	ret =
	    asn1_encode_simple_der(etype, input_data, input_size, tl,
				   &tl_size);
	if (ret != ASN1_SUCCESS) {
		gnutls_assert();
		ret = _gnutls_asn2err(ret);
		return ret;
	}

	output->data = gnutls_malloc(tl_size + input_size);
	if (output->data == NULL)
		return gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);

	memcpy(output->data, tl, tl_size);
	memcpy(output->data + tl_size, input_data, input_size);

	output->size = tl_size + input_size;

	return 0;
}

/* DER Encodes the src ASN1_TYPE and stores it to
 * the given datum. If str is non zero then the data are encoded as
 * an OCTET STRING.
 */
int
_gnutls_x509_der_encode(ASN1_TYPE src, const char *src_name,
			gnutls_datum_t * res, int str)
{
	int size, result;
	int asize;
	uint8_t *data = NULL;
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;

	size = 0;
	result = asn1_der_coding(src, src_name, NULL, &size, NULL);
	/* this check explicitly covers the case where size == 0 && result == 0 */
	if (result != ASN1_MEM_ERROR) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	/* allocate data for the der
	 */

	if (str)
		size += 16;	/* for later to include the octet tags */
	asize = size;

	data = gnutls_malloc((size_t) size);
	if (data == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	result = asn1_der_coding(src, src_name, data, &size, NULL);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	if (str) {
		if ((result = asn1_create_element
		     (_gnutls_get_pkix(), "PKIX1.pkcs-7-Data",
		      &c2)) != ASN1_SUCCESS) {
			gnutls_assert();
			result = _gnutls_asn2err(result);
			goto cleanup;
		}

		result = asn1_write_value(c2, "", data, size);
		if (result != ASN1_SUCCESS) {
			gnutls_assert();
			result = _gnutls_asn2err(result);
			goto cleanup;
		}

		result = asn1_der_coding(c2, "", data, &asize, NULL);
		if (result != ASN1_SUCCESS) {
			gnutls_assert();
			result = _gnutls_asn2err(result);
			goto cleanup;
		}

		size = asize;

		asn1_delete_structure(&c2);
	}

	res->data = data;
	res->size = (unsigned) size;
	return 0;

      cleanup:
	gnutls_free(data);
	asn1_delete_structure(&c2);
	return result;

}

/* DER Encodes the src ASN1_TYPE and stores it to
 * dest in dest_name. Useful to encode something and store it
 * as OCTET. If str is non null then the data are encoded as
 * an OCTET STRING.
 */
int
_gnutls_x509_der_encode_and_copy(ASN1_TYPE src, const char *src_name,
				 ASN1_TYPE dest, const char *dest_name,
				 int str)
{
	int result;
	gnutls_datum_t encoded;

	result = _gnutls_x509_der_encode(src, src_name, &encoded, str);

	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Write the data.
	 */
	result =
	    asn1_write_value(dest, dest_name, encoded.data,
			     (int) encoded.size);

	_gnutls_free_datum(&encoded);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	return 0;
}

/* Writes the value of the datum in the given ASN1_TYPE. 
 */
int
_gnutls_x509_write_value(ASN1_TYPE c, const char *root,
			 const gnutls_datum_t * data)
{
	int ret;

	/* Write the data.
	 */
	ret = asn1_write_value(c, root, data->data, data->size);
	if (ret != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(ret);
	}

	return 0;
}

/* Writes the value of the datum in the given ASN1_TYPE as a string. 
 */
int
_gnutls_x509_write_string(ASN1_TYPE c, const char *root,
			  const gnutls_datum_t * data, unsigned int etype)
{
	int ret;
	gnutls_datum_t val = { NULL, 0 };

	ret =
	    _gnutls_x509_encode_string(etype, data->data, data->size,
				       &val);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* Write the data.
	 */
	ret = asn1_write_value(c, root, val.data, val.size);
	if (ret != ASN1_SUCCESS) {
		gnutls_assert();
		ret = _gnutls_asn2err(ret);
		goto cleanup;
	}

	ret = 0;

      cleanup:
	_gnutls_free_datum(&val);
	return ret;
}

void
_asnstr_append_name(char *name, size_t name_size, const char *part1,
		    const char *part2)
{
	if (part1[0] != 0) {
		_gnutls_str_cpy(name, name_size, part1);
		_gnutls_str_cat(name, name_size, part2);
	} else
		_gnutls_str_cpy(name, name_size,
				part2 + 1 /* remove initial dot */ );
}



/* Encodes and copies the private key parameters into a
 * subjectPublicKeyInfo structure.
 *
 */
int
_gnutls_x509_encode_and_copy_PKI_params(ASN1_TYPE dst,
					const char *dst_name,
					gnutls_pk_algorithm_t
					pk_algorithm,
					gnutls_pk_params_st * params)
{
	const char *pk;
	gnutls_datum_t der = { NULL, 0 };
	int result;
	char name[128];

	pk = _gnutls_x509_pk_to_oid(pk_algorithm);
	if (pk == NULL) {
		gnutls_assert();
		return GNUTLS_E_UNKNOWN_PK_ALGORITHM;
	}

	/* write the OID
	 */
	_asnstr_append_name(name, sizeof(name), dst_name,
			    ".algorithm.algorithm");

	result = asn1_write_value(dst, name, pk, 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	result =
	    _gnutls_x509_write_pubkey_params(pk_algorithm, params, &der);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	_asnstr_append_name(name, sizeof(name), dst_name,
			    ".algorithm.parameters");

	result = asn1_write_value(dst, name, der.data, der.size);
	_gnutls_free_datum(&der);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	result = _gnutls_x509_write_pubkey(pk_algorithm, params, &der);
	if (result < 0) {
		gnutls_assert();
		return result;
	}

	/* Write the DER parameters. (in bits)
	 */
	_asnstr_append_name(name, sizeof(name), dst_name,
			    ".subjectPublicKey");
	result = asn1_write_value(dst, name, der.data, der.size * 8);
	_gnutls_free_datum(&der);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	return 0;
}

/* Encodes and public key parameters into a
 * subjectPublicKeyInfo structure and stores it in der.
 */
int
_gnutls_x509_encode_PKI_params(gnutls_datum_t * der,
			       gnutls_pk_algorithm_t
			       pk_algorithm, gnutls_pk_params_st * params)
{
	int ret;
	ASN1_TYPE tmp;

	ret = asn1_create_element(_gnutls_get_pkix(),
				  "PKIX1.Certificate", &tmp);
	if (ret != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(ret);
	}

	ret = _gnutls_x509_encode_and_copy_PKI_params(tmp,
						      "tbsCertificate.subjectPublicKeyInfo",
						      pk_algorithm,
						      params);
	if (ret != ASN1_SUCCESS) {
		gnutls_assert();
		ret = _gnutls_asn2err(ret);
		goto cleanup;
	}

	ret =
	    _gnutls_x509_der_encode(tmp,
				    "tbsCertificate.subjectPublicKeyInfo",
				    der, 0);

      cleanup:
	asn1_delete_structure(&tmp);

	return ret;
}

/* Reads and returns the PK algorithm of the given certificate-like
 * ASN.1 structure. src_name should be something like "tbsCertificate.subjectPublicKeyInfo".
 */
int
_gnutls_x509_get_pk_algorithm(ASN1_TYPE src, const char *src_name,
			      unsigned int *bits)
{
	int result;
	int algo;
	char oid[64];
	int len;
	gnutls_pk_params_st params;
	char name[128];

	gnutls_pk_params_init(&params);

	_asnstr_append_name(name, sizeof(name), src_name,
			    ".algorithm.algorithm");
	len = sizeof(oid);
	result = asn1_read_value(src, name, oid, &len);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	algo = _gnutls_x509_oid2pk_algorithm(oid);
	if (algo == GNUTLS_PK_UNKNOWN) {
		_gnutls_debug_log
		    ("%s: unknown public key algorithm: %s\n", __func__,
		     oid);
	}

	if (bits == NULL) {
		return algo;
	}

	/* Now read the parameters' bits 
	 */
	result = _gnutls_get_asn_mpis(src, src_name, &params);
	if (result < 0)
		return gnutls_assert_val(result);

	bits[0] = pubkey_to_bits(algo, &params);

	gnutls_pk_params_release(&params);
	return algo;
}

/* Reads the DER signed data from the certificate and allocates space and
 * returns them into signed_data.
 */
int
_gnutls_x509_get_signed_data(ASN1_TYPE src,  const gnutls_datum *_der,
			     const char *src_name,
			     gnutls_datum_t * signed_data)
{
	int start, end, result;
	gnutls_datum_t der;
	unsigned need_free = 0;

	if (_der == NULL || _der->size == 0) {
		result = _gnutls_x509_der_encode(src, "", &der, 0);
		if (result < 0) {
			gnutls_assert();
			return result;
		}
		need_free = 1;
	} else {
		der.data = _der->data;
		der.size = _der->size;
	}

	/* Get the signed data
	 */
	result = asn1_der_decoding_startEnd(src, der.data, der.size,
					    src_name, &start, &end);
	if (result != ASN1_SUCCESS) {
		result = _gnutls_asn2err(result);
		gnutls_assert();
		goto cleanup;
	}

	result =
	    _gnutls_set_datum(signed_data, &der.data[start],
			      end - start + 1);

	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	result = 0;

      cleanup:
	if (need_free != 0)
		_gnutls_free_datum(&der);

	return result;
}

/*-
 * gnutls_x509_get_signature_algorithm:
 * @src: should contain an ASN1_TYPE structure
 * @src_name: the description of the signature field
 *
 * This function will return a value of the #gnutls_sign_algorithm_t
 * enumeration that is the signature algorithm that has been used to
 * sign this certificate.
 *
 * Returns: a #gnutls_sign_algorithm_t value, or a negative error code on
 *   error.
 -*/
int
_gnutls_x509_get_signature_algorithm(ASN1_TYPE src, const char *src_name)
{
	int result;
	gnutls_datum_t sa;

	/* Read the signature algorithm. Note that parameters are not
	 * read. They will be read from the issuer's certificate if needed.
	 */
	result = _gnutls_x509_read_value(src, src_name, &sa);

	if (result < 0) {
		gnutls_assert();
		return result;
	}

	result = _gnutls_x509_oid2sign_algorithm((char *) sa.data);

	_gnutls_free_datum(&sa);

	return result;
}


/* Reads the DER signature from the certificate and allocates space and
 * returns them into signed_data.
 */
int
_gnutls_x509_get_signature(ASN1_TYPE src, const char *src_name,
			   gnutls_datum_t * signature)
{
	int result, len;
	int bits;

	signature->data = NULL;
	signature->size = 0;

	/* Read the signature 
	 */
	len = 0;
	result = asn1_read_value(src, src_name, NULL, &len);

	if (result != ASN1_MEM_ERROR) {
		result = _gnutls_asn2err(result);
		gnutls_assert();
		goto cleanup;
	}

	bits = len;
	if (bits % 8 != 0 || bits < 8) {
		gnutls_assert();
		result = GNUTLS_E_CERTIFICATE_ERROR;
		goto cleanup;
	}

	len = bits / 8;

	signature->data = gnutls_malloc(len);
	if (signature->data == NULL) {
		gnutls_assert();
		result = GNUTLS_E_MEMORY_ERROR;
		return result;
	}

	/* read the bit string of the signature
	 */
	bits = len;
	result =
	    asn1_read_value(src, src_name, signature->data, &bits);

	if (result != ASN1_SUCCESS) {
		result = _gnutls_asn2err(result);
		gnutls_assert();
		goto cleanup;
	}

	signature->size = len;

	return 0;

      cleanup:
	return result;
}

/* ASN.1 PrintableString rules */
static int is_printable(char p)
{
	if ((p >= 'a' && p <= 'z') || (p >= 'A' && p <= 'Z') ||
	    (p >= '0' && p <= '9') || p == ' ' || p == '(' || p == ')' ||
	    p == '+' || p == ',' || p == '-' || p == '.' || p == '/' ||
	    p == ':' || p == '=' || p == '?')
		return 1;

	return 0;
}

static int write_complex_string(ASN1_TYPE asn_struct, const char *where,
				const struct oid_to_string *oentry,
				const uint8_t * data, size_t data_size)
{
	char tmp[128];
	ASN1_TYPE c2;
	int result;
	const char *string_type;
	unsigned int i;

	result =
	    asn1_create_element(_gnutls_get_pkix(), oentry->asn_desc, &c2);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	tmp[0] = 0;

	string_type = "printableString";

	/* Check if the data is ASN.1 printable, and use
	 * the UTF8 string type if not.
	 */
	for (i = 0; i < data_size; i++) {
		if (!is_printable(data[i])) {
			string_type = "utf8String";
			break;
		}
	}

	/* if the type is a CHOICE then write the
	 * type we'll use.
	 */
	result = asn1_write_value(c2, "", string_type, 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto error;
	}

	_gnutls_str_cpy(tmp, sizeof(tmp), string_type);

	result = asn1_write_value(c2, tmp, data, data_size);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto error;
	}

	result =
	    _gnutls_x509_der_encode_and_copy(c2, "", asn_struct, where, 0);
	if (result < 0) {
		gnutls_assert();
		goto error;
	}

	result = 0;

      error:
	asn1_delete_structure(&c2);
	return result;
}


/* This will encode and write the AttributeTypeAndValue field.
 * 'multi' must be (0) if writing an AttributeTypeAndValue, and 1 if Attribute.
 * In all cases only one value is written.
 */
int
_gnutls_x509_encode_and_write_attribute(const char *given_oid,
					ASN1_TYPE asn1_struct,
					const char *where,
					const void *_data,
					int data_size, int multi)
{
	const uint8_t *data = _data;
	char tmp[128];
	int result;
	const struct oid_to_string *oentry;

	oentry = get_oid_entry(given_oid);
	if (oentry == NULL) {
		gnutls_assert();
		_gnutls_debug_log("Cannot find OID: %s\n", given_oid);
		return GNUTLS_E_X509_UNSUPPORTED_OID;
	}

	/* write the data (value)
	 */

	_gnutls_str_cpy(tmp, sizeof(tmp), where);
	_gnutls_str_cat(tmp, sizeof(tmp), ".value");

	if (multi != 0) {	/* if not writing an AttributeTypeAndValue, but an Attribute */
		_gnutls_str_cat(tmp, sizeof(tmp), "s");	/* values */

		result = asn1_write_value(asn1_struct, tmp, "NEW", 1);
		if (result != ASN1_SUCCESS) {
			gnutls_assert();
			result = _gnutls_asn2err(result);
			goto error;
		}

		_gnutls_str_cat(tmp, sizeof(tmp), ".?LAST");
	}

	if (oentry->asn_desc != NULL) {	/* write a complex string API */
		result =
		    write_complex_string(asn1_struct, tmp, oentry, data,
					 data_size);
		if (result < 0)
			return gnutls_assert_val(result);
	} else {		/* write a simple string */

		gnutls_datum_t td;

		td.data = (void *) data;
		td.size = data_size;
		result =
		    _gnutls_x509_write_string(asn1_struct, tmp, &td,
					      oentry->etype);
		if (result < 0) {
			gnutls_assert();
			goto error;
		}
	}

	/* write the type
	 */
	_gnutls_str_cpy(tmp, sizeof(tmp), where);
	_gnutls_str_cat(tmp, sizeof(tmp), ".type");

	result = asn1_write_value(asn1_struct, tmp, given_oid, 1);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto error;
	}

	result = 0;

      error:
	return result;
}

/* copies a datum to a buffer. If it doesn't fit it returns
 * GNUTLS_E_SHORT_MEMORY_BUFFER. It always deinitializes the datum
 * after the copy.
 *
 * The buffer will always be null terminated.
 */
int _gnutls_strdatum_to_buf(gnutls_datum_t * d, void *buf,
			    size_t * buf_size)
{
	int ret;
	uint8_t *_buf = buf;

	if (buf == NULL || *buf_size < d->size + 1) {
		*buf_size = d->size + 1;
		ret = gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);
		goto cleanup;
	}
	memcpy(buf, d->data, d->size);
	_buf[d->size] = 0;

	*buf_size = d->size;
	ret = 0;

      cleanup:
	_gnutls_free_datum(d);

	return ret;
}

int
_gnutls_x509_get_raw_field2(ASN1_TYPE c2, gnutls_datum_t * raw,
			 const char *whom, gnutls_datum_t * dn)
{
	int result, len1;
	int start1, end1;
	result =
	    asn1_der_decoding_startEnd(c2, raw->data, raw->size,
				       whom, &start1, &end1);

	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	len1 = end1 - start1 + 1;

	dn->data = &raw->data[start1];
	dn->size = len1;
	result = 0;

      cleanup:
	return result;
}

int _gnutls_copy_string(gnutls_datum_t* str, uint8_t *out, size_t *out_size)
{
unsigned size_to_check;

	size_to_check = str->size + 1;

	if ((unsigned) size_to_check > *out_size) {
		gnutls_assert();
		(*out_size) = size_to_check;
		return GNUTLS_E_SHORT_MEMORY_BUFFER;
	}

	if (out != NULL && str->data != NULL) {
		memcpy(out, str->data, str->size);
		out[str->size] = 0;
	} else if (out != NULL) {
		out[0] = 0;
	}
	*out_size = str->size;

	return 0;
}

int _gnutls_copy_data(gnutls_datum_t* str, uint8_t *out, size_t *out_size)
{
	if ((unsigned) str->size > *out_size) {
		gnutls_assert();
		(*out_size) = str->size;
		return GNUTLS_E_SHORT_MEMORY_BUFFER;
	}

	if (out != NULL && str->data != NULL) {
		memcpy(out, str->data, str->size);
	}
	*out_size = str->size;

	return 0;
}

/* Converts an X.509 certificate to subjectPublicKeyInfo */
int x509_crt_to_raw_pubkey(gnutls_x509_crt_t crt,
			   gnutls_datum_t * rpubkey)
{
	gnutls_pubkey_t pubkey = NULL;
	int ret;

	ret = gnutls_pubkey_init(&pubkey);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = gnutls_pubkey_import_x509(pubkey, crt, 0);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret =
	    gnutls_pubkey_export2(pubkey, GNUTLS_X509_FMT_DER, rpubkey);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;

      cleanup:
	gnutls_pubkey_deinit(pubkey);
	return ret;
}

/* Converts an X.509 certificate to subjectPublicKeyInfo */
int x509_raw_crt_to_raw_pubkey(const gnutls_datum_t * cert,
			   gnutls_datum_t * rpubkey)
{
	gnutls_x509_crt_t crt = NULL;
	int ret;

	ret = gnutls_x509_crt_init(&crt);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = gnutls_x509_crt_import(crt, cert, GNUTLS_X509_FMT_DER);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = x509_crt_to_raw_pubkey(crt, rpubkey);
 cleanup:
	gnutls_x509_crt_deinit(crt);

	return ret;
}

bool
_gnutls_check_valid_key_id(gnutls_datum_t *key_id,
                           gnutls_x509_crt_t cert, time_t now)
{
	uint8_t id[MAX_KEY_ID_SIZE];
	size_t id_size;
	bool result = 0;

	if (now > gnutls_x509_crt_get_expiration_time(cert) ||
	    now < gnutls_x509_crt_get_activation_time(cert)) {
		/* don't bother, certificate is not yet activated or expired */
		gnutls_assert();
		goto out;
	}

	if (gnutls_x509_crt_get_subject_key_id(cert, id, &id_size, NULL) < 0) {
		gnutls_assert();
		goto out;
	}

	if (id_size == key_id->size && !memcmp(id, key_id->data, id_size))
		result = 1;

 out:
	return result;
}
