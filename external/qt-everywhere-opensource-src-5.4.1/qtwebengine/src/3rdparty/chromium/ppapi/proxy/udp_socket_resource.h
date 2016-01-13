// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_UDP_SOCKET_RESOURCE_H_
#define PPAPI_PROXY_UDP_SOCKET_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/udp_socket_resource_base.h"
#include "ppapi/thunk/ppb_udp_socket_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT UDPSocketResource : public UDPSocketResourceBase,
                                             public thunk::PPB_UDPSocket_API {
 public:
  UDPSocketResource(Connection connection, PP_Instance instance);
  virtual ~UDPSocketResource();

  // PluginResource implementation.
  virtual thunk::PPB_UDPSocket_API* AsPPB_UDPSocket_API() OVERRIDE;

  // thunk::PPB_UDPSocket_API implementation.
  virtual int32_t Bind(PP_Resource addr,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetBoundAddress() OVERRIDE;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           PP_Resource* addr,
                           scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         PP_Resource addr,
                         scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t SetOption(PP_UDPSocket_Option name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(UDPSocketResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_UDP_SOCKET_RESOURCE_H_
