// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_RESOLVER_RESOURCE_H_
#define PPAPI_PROXY_HOST_RESOLVER_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/host_resolver_resource_base.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_host_resolver_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT HostResolverResource
    : public HostResolverResourceBase,
      public thunk::PPB_HostResolver_API {
 public:
  HostResolverResource(Connection connection, PP_Instance instance);
  virtual ~HostResolverResource();

  // PluginResource overrides.
  virtual thunk::PPB_HostResolver_API* AsPPB_HostResolver_API() OVERRIDE;

  // thunk::PPB_HostResolver_API implementation.
  virtual int32_t Resolve(const char* host,
                          uint16_t port,
                          const PP_HostResolver_Hint* hint,
                          scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Var GetCanonicalName() OVERRIDE;
  virtual uint32_t GetNetAddressCount() OVERRIDE;
  virtual PP_Resource GetNetAddress(uint32_t index) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostResolverResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HOST_RESOLVER_RESOURCE_H_
