/*
 *
 *  libproxy - A library for proxy configuration
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>

#include "libproxy/proxy.h"

int main(int argc, char *argv[])
{
	pxProxyFactory *pf;
	int i;

	if (argc < 2) {
		printf("Usage: %s <url> [url*]\n", argv[0]);
		return 1;
	}

	pf = px_proxy_factory_new();
	if (!pf) {
		fprintf(stderr, "Failed to create proxy factory\n");
		return 1;
	}

	for (i = 1; i < argc; i++) {
		char **result;
		int n;

		result = px_proxy_factory_get_proxies(pf, argv[i]);
		if (!result) {
			fprintf(stderr, "Failed to get proxy for %s\n",
								argv[i]);
			continue;
		}

		for (n = 0; result[n]; n++) {
			printf("%s%s", result[n], result[n + 1] ? " " : "\n");
			free(result[n]);
		}

		free(result);
	}

	px_proxy_factory_free(pf);

	return 0;
}
