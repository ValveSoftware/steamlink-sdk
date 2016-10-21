/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright © 2010-2016  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <linux/if_arp.h>

#include "pacrunner.h"
#include "js.h"

int __pacrunner_js_getipaddr(struct pacrunner_proxy *proxy, char *host,
			     size_t hostlen)
{
	const char *interface;
	struct sockaddr_in addr;
	struct ifreq ifr;
	int sk, err;

	interface = pacrunner_proxy_get_interface(proxy);
	if (!interface)
		return -EINVAL;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -EIO;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name));

	err = ioctl(sk, SIOCGIFADDR, &ifr);

	close(sk);

	if (err < 0)
		return -EIO;

	memcpy(&addr, &ifr.ifr_addr, sizeof(addr));
	snprintf(host, hostlen, "%s", inet_ntoa(addr.sin_addr));

	return 0;
}

int __pacrunner_js_resolve(struct pacrunner_proxy *proxy, const char *node,
			   char *host, size_t hostlen)
{
	struct addrinfo *info;
	char **split_res;
	int err;

	/* Q&D test on host to know if it is a proper hostname */
	split_res = g_strsplit_set(node, ":%?!,;@\\'*|<>{}[]()+=$&~# \"", -1);
	if (split_res) {
		int length = g_strv_length(split_res);
		g_strfreev(split_res);

		if (length > 1)
			return -EINVAL;
	}


	if (getaddrinfo(node, NULL, NULL, &info) < 0)
		return -EIO;

	err = getnameinfo(info->ai_addr, info->ai_addrlen,
				host, hostlen, NULL, 0, NI_NUMERICHOST);

	freeaddrinfo(info);

	if (err < 0)
		return -EIO;

	return 0;
}

const char __pacrunner_js_routines[] =
"function dnsDomainIs(host, domain) {\n" \
"    return (host.length >= domain.length &&\n" \
"            host.substring(host.length - domain.length) == domain);\n" \
"}\n" \
"function dnsDomainLevels(host) {\n" \
"    return host.split('.').length-1;\n" \
"}\n" \
"function convert_addr(ipchars) {\n" \
"    var bytes = ipchars.split('.');\n" \
"    var result = ((bytes[0] & 0xff) << 24) |\n" \
"                 ((bytes[1] & 0xff) << 16) |\n" \
"                 ((bytes[2] & 0xff) << 8) |\n" \
"                  (bytes[3] & 0xff);\n" \
"    return result;\n" \
"}\n" \
"function isInNet(ipaddr, pattern, maskstr) {\n" \
"    var test = /^(\\d{1,4})\\.(\\d{1,4})\\.(\\d{1,4})\\.(\\d{1,4})$/.exec(ipaddr);\n" \
"    if (test == null) {\n" \
"        ipaddr = dnsResolve(ipaddr);\n" \
"        if (ipaddr == null)\n" \
"            return false;\n" \
"    } else if (test[1] > 255 || test[2] > 255 ||\n" \
"               test[3] > 255 || test[4] > 255) {\n" \
"        return false;\n" \
"    }\n" \
"    var host = convert_addr(ipaddr);\n" \
"    var pat = convert_addr(pattern);\n" \
"    var mask = convert_addr(maskstr);\n" \
"    return ((host & mask) == (pat & mask));\n" \
"}\n" \
"function isPlainHostName(host) {\n" \
"    return (host.search('\\\\.') == -1);\n" \
"}\n" \
"function isResolvable(host) {\n" \
"    var ip = dnsResolve(host);\n" \
"    return (ip != null);\n" \
"}\n" \
"function localHostOrDomainIs(host, hostdom) {\n" \
"    if (isPlainHostName(host)) {\n" \
"        return (hostdom.search('/^' + host + '/') != -1);\n" \
"    }\n" \
"    else {\n" \
"        return (host == hostdom);\n" \
"    }\n" \
"}\n" \
"function shExpMatch(url, pattern) {\n" \
"   pattern = pattern.replace(/\\./g, '\\\\.');\n" \
"   pattern = pattern.replace(/\\*/g, '.*');\n" \
"   pattern = pattern.replace(/\\?/g, '.');\n" \
"   var newRe = new RegExp('^'+pattern+'$');\n" \
"   return newRe.test(url);\n" \
"}\n" \
"var wdays = new Array('SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT');\n" \
"var monthes = new Array('JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC');\n" \
"function weekdayRange() {\n" \
"    function getDay(weekday) {\n" \
"        for (var i = 0; i < 6; i++) {\n" \
"            if (weekday == wdays[i])\n" \
"                return i;\n" \
"        }\n" \
"        return -1;\n" \
"    }\n" \
"    var date = new Date();\n" \
"    var argc = arguments.length;\n" \
"    var wday;\n" \
"    if (argc < 1)\n" \
"        return false;\n" \
"    if (arguments[argc - 1] == 'GMT') {\n" \
"        argc--;\n" \
"        wday = date.getUTCDay();\n" \
"    } else {\n" \
"        wday = date.getDay();\n" \
"    }\n" \
"    var wd1 = getDay(arguments[0]);\n" \
"    var wd2 = (argc == 2) ? getDay(arguments[1]) : wd1;\n" \
"    return (wd1 == -1 || wd2 == -1) ? false\n" \
"                                    : (wd1 <= wday && wday <= wd2);\n" \
"}\n" \
"function dateRange() {\n" \
"    function getMonth(name) {\n" \
"        for (var i = 0; i < 6; i++) {\n" \
"            if (name == monthes[i])\n" \
"                return i;\n" \
"        }\n" \
"        return -1;\n" \
"    }\n" \
"    var date = new Date();\n" \
"    var argc = arguments.length;\n" \
"    if (argc < 1) {\n" \
"        return false;\n" \
"    }\n" \
"    var isGMT = (arguments[argc - 1] == 'GMT');\n" \
"    if (isGMT) {\n" \
"        argc--;\n" \
"    }\n" \
"    if (argc == 1) {\n" \
"        var tmp = parseInt(arguments[0]);\n" \
"        if (isNaN(tmp)) {\n" \
"            return ((isGMT ? date.getUTCMonth() : date.getMonth()) == getMonth(arguments[0]));\n" \
"        } else if (tmp < 32) {\n" \
"            return ((isGMT ? date.getUTCDate() : date.getDate()) == tmp);\n" \
"        } else {\n" \
"            return ((isGMT ? date.getUTCFullYear() : date.getFullYear()) == tmp);\n" \
"        }\n" \
"    }\n" \
"    var year = date.getFullYear();\n" \
"    var date1, date2;\n" \
"    date1 = new Date(year, 0, 1, 0, 0, 0);\n" \
"    date2 = new Date(year, 11, 31, 23, 59, 59);\n" \
"    var adjustMonth = false;\n" \
"    for (var i = 0; i < (argc >> 1); i++) {\n" \
"        var tmp = parseInt(arguments[i]);\n" \
"        if (isNaN(tmp)) {\n" \
"            var mon = getMonth(arguments[i]);\n" \
"            date1.setMonth(mon);\n" \
"        } else if (tmp < 32) {\n" \
"            adjustMonth = (argc <= 2);\n" \
"            date1.setDate(tmp);\n" \
"        } else {\n" \
"            date1.setFullYear(tmp);\n" \
"        }\n" \
"    }\n" \
"    for (var i = (argc >> 1); i < argc; i++) {\n" \
"        var tmp = parseInt(arguments[i]);\n" \
"        if (isNaN(tmp)) {\n" \
"            var mon = getMonth(arguments[i]);\n" \
"            date2.setMonth(mon);\n" \
"        } else if (tmp < 32) {\n" \
"            date2.setDate(tmp);\n" \
"        } else {\n" \
"            date2.setFullYear(tmp);\n" \
"        }\n" \
"    }\n" \
"    if (adjustMonth) {\n" \
"        date1.setMonth(date.getMonth());\n" \
"        date2.setMonth(date.getMonth());\n" \
"    }\n" \
"    if (isGMT) {\n" \
"    var tmp = date;\n" \
"        tmp.setFullYear(date.getUTCFullYear());\n" \
"        tmp.setMonth(date.getUTCMonth());\n" \
"        tmp.setDate(date.getUTCDate());\n" \
"        tmp.setHours(date.getUTCHours());\n" \
"        tmp.setMinutes(date.getUTCMinutes());\n" \
"        tmp.setSeconds(date.getUTCSeconds());\n" \
"        date = tmp;\n" \
"    }\n" \
"    return ((date1 <= date) && (date <= date2));\n" \
"}\n" \
"function timeRange() {\n" \
"    var argc = arguments.length;\n" \
"    var date = new Date();\n" \
"    var isGMT= false;\n" \
"    if (argc < 1) {\n" \
"        return false;\n" \
"    }\n" \
"    if (arguments[argc - 1] == 'GMT') {\n" \
"        isGMT = true;\n" \
"        argc--;\n" \
"    }\n" \
"    var hour = isGMT ? date.getUTCHours() : date.getHours();\n" \
"    var date1, date2;\n" \
"    date1 = new Date();\n" \
"    date2 = new Date();\n" \
"    if (argc == 1) {\n" \
"        return (hour == arguments[0]);\n" \
"    } else if (argc == 2) {\n" \
"        return ((arguments[0] <= hour) && (hour <= arguments[1]));\n" \
"    } else {\n" \
"        switch (argc) {\n" \
"        case 6:\n" \
"            date1.setSeconds(arguments[2]);\n" \
"            date2.setSeconds(arguments[5]);\n" \
"        case 4:\n" \
"            var middle = argc >> 1;\n" \
"            date1.setHours(arguments[0]);\n" \
"            date1.setMinutes(arguments[1]);\n" \
"            date2.setHours(arguments[middle]);\n" \
"            date2.setMinutes(arguments[middle + 1]);\n" \
"            if (middle == 2) {\n" \
"                date2.setSeconds(59);\n" \
"            }\n" \
"            break;\n" \
"        default:\n" \
"          throw 'timeRange: bad number of arguments'\n" \
"        }\n" \
"    }\n" \
"    if (isGMT) {\n" \
"        date.setFullYear(date.getUTCFullYear());\n" \
"        date.setMonth(date.getUTCMonth());\n" \
"        date.setDate(date.getUTCDate());\n" \
"        date.setHours(date.getUTCHours());\n" \
"        date.setMinutes(date.getUTCMinutes());\n" \
"        date.setSeconds(date.getUTCSeconds());\n" \
"    }\n" \
"    return ((date1 <= date) && (date <= date2));\n" \
"}\n" \
"";
