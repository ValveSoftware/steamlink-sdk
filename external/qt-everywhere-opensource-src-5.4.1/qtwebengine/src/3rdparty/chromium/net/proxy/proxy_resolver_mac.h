// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_MAC_H_
#define NET_PROXY_PROXY_RESOLVER_MAC_H_

#include "base/compiler_specific.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_resolver.h"
#include "url/gurl.h"

namespace net {

// Implementation of ProxyResolver that uses the Mac CFProxySupport to implement
// proxies.
class NET_EXPORT ProxyResolverMac : public ProxyResolver {
 public:
  ProxyResolverMac();
  virtual ~ProxyResolverMac();

  // ProxyResolver methods:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             const net::CompletionCallback& callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE;

  virtual void CancelRequest(RequestHandle request) OVERRIDE;

  virtual LoadState GetLoadState(RequestHandle request) const OVERRIDE;

  virtual void CancelSetPacScript() OVERRIDE;

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      const net::CompletionCallback& /*callback*/) OVERRIDE;

 private:
  scoped_refptr<ProxyResolverScriptData> script_data_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_MAC_H_
