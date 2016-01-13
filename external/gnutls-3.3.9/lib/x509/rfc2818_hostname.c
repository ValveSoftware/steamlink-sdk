/*
 * Copyright (C) 2003-2012 Free Software Foundation, Inc.
 * Copyright (C) 2002 Andrew McDonald
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
#include <gnutls_str.h>
#include <x509_int.h>
#include <common.h>
#include <gnutls_errors.h>
#include <system.h>

/**
 * gnutls_x509_crt_check_hostname:
 * @cert: should contain an gnutls_x509_crt_t structure
 * @hostname: A null terminated string that contains a DNS name
 *
 * This function will check if the given certificate's subject matches
 * the given hostname.  This is a basic implementation of the matching
 * described in RFC6125, and takes into account wildcards,
 * and the DNSName/IPAddress subject alternative name PKIX extension.
 *
 * For details see also gnutls_x509_crt_check_hostname2().
 *
 * Returns: non-zero for a successful match, and zero on failure.
 **/
int
gnutls_x509_crt_check_hostname(gnutls_x509_crt_t cert,
			       const char *hostname)
{
	return gnutls_x509_crt_check_hostname2(cert, hostname, 0);
}

static int
check_ip(gnutls_x509_crt_t cert, const void *ip, unsigned ip_size, unsigned flags)
{
	char temp[16];
	size_t temp_size;
	unsigned i;
	int ret = 0;

	/* try matching against:
	 *  1) a IPaddress alternative name (subjectAltName) extension
	 *     in the certificate
	 */

	/* Check through all included subjectAltName extensions, comparing
	 * against all those of type IPAddress.
	 */
	for (i = 0; !(ret < 0); i++) {
		temp_size = sizeof(temp);
		ret = gnutls_x509_crt_get_subject_alt_name(cert, i,
							   temp,
							   &temp_size,
							   NULL);

		if (ret == GNUTLS_SAN_IPADDRESS) {
			if (temp_size == ip_size && memcmp(temp, ip, ip_size) == 0)
				return 1;
		} else if (ret == GNUTLS_E_SHORT_MEMORY_BUFFER) {
			ret = 0;
		}
	}

	/* not found a matching IP
	 */
	return 0;
}

/**
 * gnutls_x509_crt_check_hostname:
 * @cert: should contain an gnutls_x509_crt_t structure
 * @hostname: A null terminated string that contains a DNS name
 * @flags: gnutls_certificate_verify_flags
 *
 * This function will check if the given certificate's subject matches
 * the given hostname.  This is a basic implementation of the matching
 * described in RFC6125, and takes into account wildcards,
 * and the DNSName/IPAddress subject alternative name PKIX extension.
 *
 * IPv4 addresses are accepted by this function in the dotted-decimal
 * format (e.g, ddd.ddd.ddd.ddd), and IPv6 addresses in the hexadecimal
 * x:x:x:x:x:x:x:x format. For them the IPAddress subject alternative
 * name extension is consulted, as well as the DNSNames in case of a non-match.
 * The latter fallback exists due to misconfiguration of many servers
 * which place an IPAddress inside the DNSName extension.
 *
 * The comparison of dns names may have false-negatives as it is done byte 
 * by byte in non-ascii names.
 *
 * When the flag %GNUTLS_VERIFY_DO_NOT_ALLOW_WILDCARDS is specified no
 * wildcards are considered. Otherwise they are only considered if the
 * domain name consists of three components or more, and the wildcard
 * starts at the leftmost position.
 *
 * Returns: non-zero for a successful match, and zero on failure.
 **/
int
gnutls_x509_crt_check_hostname2(gnutls_x509_crt_t cert,
			        const char *hostname, unsigned int flags)
{
	char dnsname[MAX_CN];
	size_t dnsnamesize;
	int found_dnsname = 0;
	int ret = 0;
	int i = 0;
	struct in_addr ipv4;
	char *p = NULL;

	/* check whether @hostname is an ip address */
	if ((p=strchr(hostname, ':')) != NULL || inet_aton(hostname, &ipv4) != 0) {

		if (p != NULL) {
			struct in6_addr ipv6;

			ret = inet_pton(AF_INET6, hostname, &ipv6);
			if (ret == 0) {
				gnutls_assert();
				goto hostname_fallback;
			}
			ret = check_ip(cert, &ipv6, 16, flags);
		} else {
			ret = check_ip(cert, &ipv4, 4, flags);
		}

		if (ret != 0)
			return ret;

		/* There are several misconfigured servers, that place their IP
		 * in the DNS field of subjectAlternativeName. Don't break these
		 * configurations and verify the IP as it would have been a DNS name. */
	}

 hostname_fallback:
	/* try matching against:
	 *  1) a DNS name as an alternative name (subjectAltName) extension
	 *     in the certificate
	 *  2) the common name (CN) in the certificate
	 *
	 *  either of these may be of the form: *.domain.tld
	 *
	 *  only try (2) if there is no subjectAltName extension of
	 *  type dNSName, and there is a single CN.
	 */

	/* Check through all included subjectAltName extensions, comparing
	 * against all those of type dNSName.
	 */
	for (i = 0; !(ret < 0); i++) {

		dnsnamesize = sizeof(dnsname);
		ret = gnutls_x509_crt_get_subject_alt_name(cert, i,
							   dnsname,
							   &dnsnamesize,
							   NULL);

		if (ret == GNUTLS_SAN_DNSNAME) {
			found_dnsname = 1;
			if (_gnutls_hostname_compare
			    (dnsname, dnsnamesize, hostname, flags)) {
				return 1;
			}
		}
	}

	if (!found_dnsname) {
		/* did not get the necessary extension, use CN instead
		 */

		/* enforce the RFC6125 (§1.8) requirement that only
		 * a single CN must be present */
		dnsnamesize = sizeof(dnsname);
		ret = gnutls_x509_crt_get_dn_by_oid
			(cert, OID_X520_COMMON_NAME, 1, 0, dnsname,
			 &dnsnamesize);
		if (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			return 0;

		dnsnamesize = sizeof(dnsname);
		ret = gnutls_x509_crt_get_dn_by_oid
			(cert, OID_X520_COMMON_NAME, 0, 0, dnsname,
			 &dnsnamesize);
		if (ret < 0)
			return 0;

		if (_gnutls_hostname_compare
		    (dnsname, dnsnamesize, hostname, flags)) {
			return 1;
		}
	}

	/* not found a matching name
	 */
	return 0;
}
