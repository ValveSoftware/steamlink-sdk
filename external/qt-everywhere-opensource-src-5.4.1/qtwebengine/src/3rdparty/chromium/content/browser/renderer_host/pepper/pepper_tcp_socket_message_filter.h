// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/pepper/ssl_context_helper.h"
#include "content/common/content_export.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_socket.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/resource_message_filter.h"
#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"

namespace net {
enum AddressFamily;
class DrainableIOBuffer;
class IOBuffer;
class SingleRequestHostResolver;
class SSLClientSocket;
}

namespace ppapi {
class SocketOptionData;

namespace host {
class PpapiHost;
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHostImpl;
class ContentBrowserPepperHostFactory;
class ResourceContext;

class CONTENT_EXPORT PepperTCPSocketMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperTCPSocketMessageFilter(ContentBrowserPepperHostFactory* factory,
                               BrowserPpapiHostImpl* host,
                               PP_Instance instance,
                               ppapi::TCPSocketVersion version);

  // Used for creating already connected sockets.
  PepperTCPSocketMessageFilter(BrowserPpapiHostImpl* host,
                               PP_Instance instance,
                               ppapi::TCPSocketVersion version,
                               scoped_ptr<net::TCPSocket> socket);

  static size_t GetNumInstances();

 private:
  virtual ~PepperTCPSocketMessageFilter();

  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  int32_t OnMsgBind(const ppapi::host::HostMessageContext* context,
                    const PP_NetAddress_Private& net_addr);
  int32_t OnMsgConnect(const ppapi::host::HostMessageContext* context,
                       const std::string& host,
                       uint16_t port);
  int32_t OnMsgConnectWithNetAddress(
      const ppapi::host::HostMessageContext* context,
      const PP_NetAddress_Private& net_addr);
  int32_t OnMsgSSLHandshake(
      const ppapi::host::HostMessageContext* context,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs);
  int32_t OnMsgRead(const ppapi::host::HostMessageContext* context,
                    int32_t bytes_to_read);
  int32_t OnMsgWrite(const ppapi::host::HostMessageContext* context,
                     const std::string& data);
  int32_t OnMsgListen(const ppapi::host::HostMessageContext* context,
                      int32_t backlog);
  int32_t OnMsgAccept(const ppapi::host::HostMessageContext* context);
  int32_t OnMsgClose(const ppapi::host::HostMessageContext* context);
  int32_t OnMsgSetOption(const ppapi::host::HostMessageContext* context,
                         PP_TCPSocket_Option name,
                         const ppapi::SocketOptionData& value);

  void DoBind(const ppapi::host::ReplyMessageContext& context,
              const PP_NetAddress_Private& net_addr);
  void DoConnect(const ppapi::host::ReplyMessageContext& context,
                 const std::string& host,
                 uint16_t port,
                 ResourceContext* resource_context);
  void DoConnectWithNetAddress(const ppapi::host::ReplyMessageContext& context,
                               const PP_NetAddress_Private& net_addr);
  void DoWrite(const ppapi::host::ReplyMessageContext& context);
  void DoListen(const ppapi::host::ReplyMessageContext& context,
                int32_t backlog);

  void OnResolveCompleted(const ppapi::host::ReplyMessageContext& context,
                          int net_result);
  void StartConnect(const ppapi::host::ReplyMessageContext& context);

  void OnConnectCompleted(const ppapi::host::ReplyMessageContext& context,
                          int net_result);
  void OnSSLHandshakeCompleted(const ppapi::host::ReplyMessageContext& context,
                               int net_result);
  void OnReadCompleted(const ppapi::host::ReplyMessageContext& context,
                       int net_result);
  void OnWriteCompleted(const ppapi::host::ReplyMessageContext& context,
                        int net_result);
  void OnAcceptCompleted(const ppapi::host::ReplyMessageContext& context,
                         int net_result);

  void SendBindReply(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_result,
                     const PP_NetAddress_Private& local_addr);
  void SendBindError(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_error);
  void SendConnectReply(const ppapi::host::ReplyMessageContext& context,
                        int32_t pp_result,
                        const PP_NetAddress_Private& local_addr,
                        const PP_NetAddress_Private& remote_addr);
  void SendConnectError(const ppapi::host::ReplyMessageContext& context,
                        int32_t pp_error);
  void SendSSLHandshakeReply(const ppapi::host::ReplyMessageContext& context,
                             int32_t pp_result);
  void SendReadReply(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_result,
                     const std::string& data);
  void SendReadError(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_error);
  void SendWriteReply(const ppapi::host::ReplyMessageContext& context,
                      int32_t pp_result);
  void SendListenReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result);
  void SendAcceptReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result,
                       int pending_host_id,
                       const PP_NetAddress_Private& local_addr,
                       const PP_NetAddress_Private& remote_addr);
  void SendAcceptError(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_error);

  bool IsPrivateAPI() const {
    return version_ == ppapi::TCP_SOCKET_VERSION_PRIVATE;
  }

  // The following fields are used on both the UI and IO thread.
  const ppapi::TCPSocketVersion version_;

  // The following fields are used only on the UI thread.
  const bool external_plugin_;

  int render_process_id_;
  int render_frame_id_;

  // The following fields are used only on the IO thread.
  // Non-owning ptr.
  ppapi::host::PpapiHost* ppapi_host_;
  // Non-owning ptr.
  ContentBrowserPepperHostFactory* factory_;
  PP_Instance instance_;

  ppapi::TCPSocketState state_;
  bool end_of_file_reached_;

  // This is the address requested to bind. Please note that this is not the
  // bound address. For example, |bind_input_addr_| may have port set to 0.
  // It is used to check permission for listening.
  PP_NetAddress_Private bind_input_addr_;

  scoped_ptr<net::SingleRequestHostResolver> resolver_;

  // |address_list_| may store multiple addresses when
  // PPB_TCPSocket_Private.Connect() is used, which involves name resolution.
  // In that case, we will try each address in the list until a connection is
  // successfully established.
  net::AddressList address_list_;
  // Where we are in the above list.
  size_t address_index_;

  // Non-null unless an SSL connection is requested.
  scoped_ptr<net::TCPSocket> socket_;
  // Non-null if an SSL connection is requested.
  scoped_ptr<net::SSLClientSocket> ssl_socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;

  // TCPSocket::Write() may not always write the full buffer, but we would
  // rather have our DoWrite() do so whenever possible. To do this, we may have
  // to call the former multiple times for each of the latter. This entails
  // using a DrainableIOBuffer, which requires an underlying base IOBuffer.
  scoped_refptr<net::IOBuffer> write_buffer_base_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  scoped_refptr<SSLContextHelper> ssl_context_helper_;

  bool pending_accept_;
  scoped_ptr<net::TCPSocket> accepted_socket_;
  net::IPEndPoint accepted_address_;

  DISALLOW_COPY_AND_ASSIGN(PepperTCPSocketMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_
