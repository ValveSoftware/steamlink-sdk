// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_TCP_SOCKET_PRIVATE_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/tcp_socket_resource_base.h"
#include "ppapi/thunk/ppb_tcp_socket_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT TCPSocketPrivateResource
    : public thunk::PPB_TCPSocket_Private_API,
      public TCPSocketResourceBase {
 public:
  // C-tor used for new sockets.
  TCPSocketPrivateResource(Connection connection, PP_Instance instance);

  // C-tor used for already accepted sockets.
  TCPSocketPrivateResource(Connection connection,
                           PP_Instance instance,
                           int pending_resource_id,
                           const PP_NetAddress_Private& local_addr,
                           const PP_NetAddress_Private& remote_addr);

  virtual ~TCPSocketPrivateResource();

  // PluginResource overrides.
  virtual PPB_TCPSocket_Private_API* AsPPB_TCPSocket_Private_API() OVERRIDE;

  // PPB_TCPSocket_Private_API implementation.
  virtual int32_t Connect(const char* host,
                          uint16_t port,
                          scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t ConnectWithNetAddress(
      const PP_NetAddress_Private* addr,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool GetLocalAddress(PP_NetAddress_Private* local_addr) OVERRIDE;
  virtual PP_Bool GetRemoteAddress(PP_NetAddress_Private* remote_addr) OVERRIDE;
  virtual int32_t SSLHandshake(
      const char* server_name,
      uint16_t server_port,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetServerCertificate() OVERRIDE;
  virtual PP_Bool AddChainBuildingCertificate(PP_Resource certificate,
                                              PP_Bool trusted) OVERRIDE;
  virtual int32_t Read(char* buffer,
                      int32_t bytes_to_read,
                      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual int32_t SetOption(PP_TCPSocketOption_Private name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;

  // TCPSocketResourceBase implementation.
  virtual PP_Resource CreateAcceptedSocket(
      int pending_host_id,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TCPSocketPrivateResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_PRIVATE_RESOURCE_H_
