// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_
#define PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/tcp_socket_resource_base.h"
#include "ppapi/thunk/ppb_tcp_socket_api.h"

namespace ppapi {

enum TCPSocketVersion;

namespace proxy {

class PPAPI_PROXY_EXPORT TCPSocketResource : public thunk::PPB_TCPSocket_API,
                                             public TCPSocketResourceBase {
 public:
  // C-tor used for new sockets created.
  TCPSocketResource(Connection connection,
                    PP_Instance instance,
                    TCPSocketVersion version);

  virtual ~TCPSocketResource();

  // PluginResource overrides.
  virtual thunk::PPB_TCPSocket_API* AsPPB_TCPSocket_API() OVERRIDE;

  // thunk::PPB_TCPSocket_API implementation.
  virtual int32_t Bind(PP_Resource addr,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Connect(PP_Resource addr,
                          scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetLocalAddress() OVERRIDE;
  virtual PP_Resource GetRemoteAddress() OVERRIDE;
  virtual int32_t Read(char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Listen(int32_t backlog,
                         scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Accept(PP_Resource* accepted_tcp_socket,
                         scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t SetOption(PP_TCPSocket_Option name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;

  // TCPSocketResourceBase implementation.
  virtual PP_Resource CreateAcceptedSocket(
      int pending_host_id,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr) OVERRIDE;

 private:
  // C-tor used for accepted sockets.
  TCPSocketResource(Connection connection,
                    PP_Instance instance,
                    int pending_host_id,
                    const PP_NetAddress_Private& local_addr,
                    const PP_NetAddress_Private& remote_addr);

  DISALLOW_COPY_AND_ASSIGN(TCPSocketResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_
