/*
 * Copyright (C) 2004-2012 Free Software Foundation, Inc.
 *
 * Author: Simon Josefsson
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "utils.h"

int debug = 0;
int error_count = 0;
int break_on_error = 0;

const char *pkcs3 =
    "-----BEGIN DH PARAMETERS-----\n"
    "MIGGAoGAtkxw2jlsVCsrfLqxrN+IrF/3W8vVFvDzYbLmxi2GQv9s/PQGWP1d9i22\n"
    "P2DprfcJknWt7KhCI1SaYseOQIIIAYP78CfyIpGScW/vS8khrw0rlQiyeCvQgF3O\n"
    "GeGOEywcw+oQT4SmFOD7H0smJe2CNyjYpexBXQ/A0mbTF9QKm1cCAQU=\n"
    "-----END DH PARAMETERS-----\n";

void fail(const char *format, ...)
{
	char str[1024];
	va_list arg_ptr;

	va_start(arg_ptr, format);
	vsnprintf(str, sizeof(str), format, arg_ptr);
	va_end(arg_ptr);
	fputs(str, stderr);
	error_count++;
	exit(1);
}

void success(const char *format, ...)
{
	char str[1024];
	va_list arg_ptr;

	va_start(arg_ptr, format);
	vsnprintf(str, sizeof(str), format, arg_ptr);
	va_end(arg_ptr);
	fputs(str, stderr);
}

void escapeprint(const char *str, size_t len)
{
	size_t i;

	printf(" (length %d bytes):\n\t", (int) len);
	for (i = 0; i < len; i++) {
		if (((str[i] & 0xFF) >= 'A' && (str[i] & 0xFF) <= 'Z') ||
		    ((str[i] & 0xFF) >= 'a' && (str[i] & 0xFF) <= 'z') ||
		    ((str[i] & 0xFF) >= '0' && (str[i] & 0xFF) <= '9')
		    || (str[i] & 0xFF) == ' ' || (str[i] & 0xFF) == '.')
			printf("%c", (str[i] & 0xFF));
		else
			printf("\\x%02X", (str[i] & 0xFF));
		if ((i + 1) % 16 == 0 && (i + 1) < len)
			printf("'\n\t'");
	}
	printf("\n");
}

void hexprint(const void *_str, size_t len)
{
	size_t i;
	const char *str = _str;

	printf("\t;; ");
	for (i = 0; i < len; i++) {
		printf("%02x ", (str[i] & 0xFF));
		if ((i + 1) % 8 == 0)
			printf(" ");
		if ((i + 1) % 16 == 0 && i + 1 < len)
			printf("\n\t;; ");
	}
	printf("\n");
}

void binprint(const void *_str, size_t len)
{
	size_t i;
	const char *str = _str;

	printf("\t;; ");
	for (i = 0; i < len; i++) {
		printf("%d%d%d%d%d%d%d%d ",
		       (str[i] & 0xFF) & 0x80 ? 1 : 0,
		       (str[i] & 0xFF) & 0x40 ? 1 : 0,
		       (str[i] & 0xFF) & 0x20 ? 1 : 0,
		       (str[i] & 0xFF) & 0x10 ? 1 : 0,
		       (str[i] & 0xFF) & 0x08 ? 1 : 0,
		       (str[i] & 0xFF) & 0x04 ? 1 : 0,
		       (str[i] & 0xFF) & 0x02 ? 1 : 0,
		       (str[i] & 0xFF) & 0x01 ? 1 : 0);
		if ((i + 1) % 3 == 0)
			printf(" ");
		if ((i + 1) % 6 == 0 && i + 1 < len)
			printf("\n\t;; ");
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	do
		if (strcmp(argv[argc - 1], "-v") == 0 ||
		    strcmp(argv[argc - 1], "--verbose") == 0)
			debug = 1;
		else if (strcmp(argv[argc - 1], "-b") == 0 ||
			 strcmp(argv[argc - 1], "--break-on-error") == 0)
			break_on_error = 1;
		else if (strcmp(argv[argc - 1], "-h") == 0 ||
			 strcmp(argv[argc - 1], "-?") == 0 ||
			 strcmp(argv[argc - 1], "--help") == 0) {
			printf
			    ("Usage: %s [-vbh?] [--verbose] [--break-on-error] [--help]\n",
			     argv[0]);
			return 1;
		}
	while (argc-- > 1);

	doit();

	if (debug || error_count > 0)
		printf("Self test `%s' finished with %d errors\n", argv[0],
		       error_count);

	return error_count ? 1 : 0;
}
