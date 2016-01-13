// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include "base/strings/string_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace net {

MappedHostResolver::MappedHostResolver(scoped_ptr<HostResolver> impl)
    : impl_(impl.Pass()) {
}

MappedHostResolver::~MappedHostResolver() {
}

int MappedHostResolver::Resolve(const RequestInfo& original_info,
                                RequestPriority priority,
                                AddressList* addresses,
                                const CompletionCallback& callback,
                                RequestHandle* out_req,
                                const BoundNetLog& net_log) {
  RequestInfo info = original_info;
  int rv = ApplyRules(&info);
  if (rv != OK)
    return rv;

  return impl_->Resolve(info, priority, addresses, callback, out_req, net_log);
}

int MappedHostResolver::ResolveFromCache(const RequestInfo& original_info,
                                         AddressList* addresses,
                                         const BoundNetLog& net_log) {
  RequestInfo info = original_info;
  int rv = ApplyRules(&info);
  if (rv != OK)
    return rv;

  return impl_->ResolveFromCache(info, addresses, net_log);
}

void MappedHostResolver::CancelRequest(RequestHandle req) {
  impl_->CancelRequest(req);
}

void MappedHostResolver::SetDnsClientEnabled(bool enabled) {
  impl_->SetDnsClientEnabled(enabled);
}

HostCache* MappedHostResolver::GetHostCache() {
  return impl_->GetHostCache();
}

base::Value* MappedHostResolver::GetDnsConfigAsValue() const {
  return impl_->GetDnsConfigAsValue();
}

int MappedHostResolver::ApplyRules(RequestInfo* info) const {
  HostPortPair host_port(info->host_port_pair());
  if (rules_.RewriteHost(&host_port)) {
    if (host_port.host() == "~NOTFOUND")
      return ERR_NAME_NOT_RESOLVED;
    info->set_host_port_pair(host_port);
  }
  return OK;
}

}  // namespace net
