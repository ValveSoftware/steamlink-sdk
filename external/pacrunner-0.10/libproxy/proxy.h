/*
 *
 *  libproxy - A library for proxy configuration
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms and conditions of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __PROXY_H
#define __PROXY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _pxProxyFactory pxProxyFactory;

pxProxyFactory *px_proxy_factory_new(void);
void px_proxy_factory_free(pxProxyFactory *factory);

char **px_proxy_factory_get_proxies(pxProxyFactory *factory, const char *url);

#ifdef __cplusplus
}
#endif

#endif /* __PROXY_H */
