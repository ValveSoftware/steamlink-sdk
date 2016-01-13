// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/process_type.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/host/resource_message_filter.h"

struct PP_NetAddress_Private;

namespace net {
class IOBuffer;
class IOBufferWithSize;
class UDPServerSocket;
}

namespace ppapi {

class SocketOptionData;

namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHostImpl;
struct SocketPermissionRequest;

class CONTENT_EXPORT PepperUDPSocketMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperUDPSocketMessageFilter(BrowserPpapiHostImpl* host,
                               PP_Instance instance,
                               bool private_api);

  static size_t GetNumInstances();

 protected:
  virtual ~PepperUDPSocketMessageFilter();

 private:
  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  int32_t OnMsgSetOption(const ppapi::host::HostMessageContext* context,
                         PP_UDPSocket_Option name,
                         const ppapi::SocketOptionData& value);
  int32_t OnMsgBind(const ppapi::host::HostMessageContext* context,
                    const PP_NetAddress_Private& addr);
  int32_t OnMsgRecvFrom(const ppapi::host::HostMessageContext* context,
                        int32_t num_bytes);
  int32_t OnMsgSendTo(const ppapi::host::HostMessageContext* context,
                      const std::string& data,
                      const PP_NetAddress_Private& addr);
  int32_t OnMsgClose(const ppapi::host::HostMessageContext* context);

  void DoBind(const ppapi::host::ReplyMessageContext& context,
              const PP_NetAddress_Private& addr);
  void DoSendTo(const ppapi::host::ReplyMessageContext& context,
                const std::string& data,
                const PP_NetAddress_Private& addr);
  void Close();

  void OnRecvFromCompleted(const ppapi::host::ReplyMessageContext& context,
                           int net_result);
  void OnSendToCompleted(const ppapi::host::ReplyMessageContext& context,
                         int net_result);

  void SendBindReply(const ppapi::host::ReplyMessageContext& context,
                     int32_t result,
                     const PP_NetAddress_Private& addr);
  void SendRecvFromReply(const ppapi::host::ReplyMessageContext& context,
                         int32_t result,
                         const std::string& data,
                         const PP_NetAddress_Private& addr);
  void SendSendToReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t result,
                       int32_t bytes_written);

  void SendBindError(const ppapi::host::ReplyMessageContext& context,
                     int32_t result);
  void SendRecvFromError(const ppapi::host::ReplyMessageContext& context,
                         int32_t result);
  void SendSendToError(const ppapi::host::ReplyMessageContext& context,
                       int32_t result);

  bool allow_address_reuse_;
  bool allow_broadcast_;

  scoped_ptr<net::UDPServerSocket> socket_;
  bool closed_;

  scoped_refptr<net::IOBuffer> recvfrom_buffer_;
  scoped_refptr<net::IOBufferWithSize> sendto_buffer_;

  net::IPEndPoint recvfrom_address_;

  bool external_plugin_;
  bool private_api_;

  int render_process_id_;
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(PepperUDPSocketMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_
