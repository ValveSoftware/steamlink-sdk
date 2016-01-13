// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_udp_socket_message_filter.h"

#include <cstring>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "content/public/common/socket_permission_request.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/udp/udp_server_socket.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/error_conversion.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/udp_socket_resource_base.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/socket_option_data.h"

using ppapi::NetAddressPrivateImpl;
using ppapi::host::NetErrorToPepperError;

namespace {

size_t g_num_instances = 0;

}  // namespace

namespace content {

PepperUDPSocketMessageFilter::PepperUDPSocketMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    bool private_api)
    : allow_address_reuse_(false),
      allow_broadcast_(false),
      closed_(false),
      external_plugin_(host->external_plugin()),
      private_api_(private_api),
      render_process_id_(0),
      render_frame_id_(0) {
  ++g_num_instances;
  DCHECK(host);

  if (!host->GetRenderFrameIDsForInstance(
          instance, &render_process_id_, &render_frame_id_)) {
    NOTREACHED();
  }
}

PepperUDPSocketMessageFilter::~PepperUDPSocketMessageFilter() {
  Close();
  --g_num_instances;
}

// static
size_t PepperUDPSocketMessageFilter::GetNumInstances() {
  return g_num_instances;
}

scoped_refptr<base::TaskRunner>
PepperUDPSocketMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case PpapiHostMsg_UDPSocket_SetOption::ID:
    case PpapiHostMsg_UDPSocket_RecvFrom::ID:
    case PpapiHostMsg_UDPSocket_Close::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
    case PpapiHostMsg_UDPSocket_Bind::ID:
    case PpapiHostMsg_UDPSocket_SendTo::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }
  return NULL;
}

int32_t PepperUDPSocketMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperUDPSocketMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_SetOption,
                                      OnMsgSetOption)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_Bind, OnMsgBind)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_RecvFrom,
                                      OnMsgRecvFrom)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_SendTo,
                                      OnMsgSendTo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_UDPSocket_Close,
                                        OnMsgClose)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperUDPSocketMessageFilter::OnMsgSetOption(
    const ppapi::host::HostMessageContext* context,
    PP_UDPSocket_Option name,
    const ppapi::SocketOptionData& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (closed_)
    return PP_ERROR_FAILED;

  switch (name) {
    case PP_UDPSOCKET_OPTION_ADDRESS_REUSE:
    case PP_UDPSOCKET_OPTION_BROADCAST: {
      if (socket_.get()) {
        // They only take effect before the socket is bound.
        return PP_ERROR_FAILED;
      }

      bool boolean_value = false;
      if (!value.GetBool(&boolean_value))
        return PP_ERROR_BADARGUMENT;

      if (name == PP_UDPSOCKET_OPTION_ADDRESS_REUSE)
        allow_address_reuse_ = boolean_value;
      else
        allow_broadcast_ = boolean_value;
      return PP_OK;
    }
    case PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE:
    case PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      if (!socket_.get()) {
        // They only take effect after the socket is bound.
        return PP_ERROR_FAILED;
      }
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) || integer_value <= 0)
        return PP_ERROR_BADARGUMENT;

      int net_result = net::ERR_UNEXPECTED;
      if (name == PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE) {
        if (integer_value >
            ppapi::proxy::UDPSocketResourceBase::kMaxSendBufferSize) {
          return PP_ERROR_BADARGUMENT;
        }
        net_result = socket_->SetSendBufferSize(integer_value);
      } else {
        if (integer_value >
            ppapi::proxy::UDPSocketResourceBase::kMaxReceiveBufferSize) {
          return PP_ERROR_BADARGUMENT;
        }
        net_result = socket_->SetReceiveBufferSize(integer_value);
      }
      // TODO(wtc): Add error mapping code.
      return (net_result == net::OK) ? PP_OK : PP_ERROR_FAILED;
    }
    default: {
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
    }
  }
}

int32_t PepperUDPSocketMessageFilter::OnMsgBind(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_BIND, addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             private_api_,
                                             &request,
                                             render_process_id_,
                                             render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&PepperUDPSocketMessageFilter::DoBind,
                                     this,
                                     context->MakeReplyMessageContext(),
                                     addr));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketMessageFilter::OnMsgRecvFrom(
    const ppapi::host::HostMessageContext* context,
    int32_t num_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(context);
  DCHECK(socket_.get());

  if (closed_ || !socket_.get())
    return PP_ERROR_FAILED;

  if (recvfrom_buffer_.get())
    return PP_ERROR_INPROGRESS;

  if (num_bytes <= 0 ||
      num_bytes > ppapi::proxy::UDPSocketResourceBase::kMaxReadSize) {
    // |num_bytes| value is checked on the plugin side.
    NOTREACHED();
    return PP_ERROR_BADARGUMENT;
  }

  recvfrom_buffer_ = new net::IOBuffer(num_bytes);

  // Use base::Unretained(this), so that the lifespan of this object doesn't
  // have to last until the callback is called.
  // It is safe to do so because |socket_| is owned by this object. If this
  // object gets destroyed (and so does |socket_|), the callback won't be
  // called.
  int net_result = socket_->RecvFrom(
      recvfrom_buffer_.get(),
      num_bytes,
      &recvfrom_address_,
      base::Bind(&PepperUDPSocketMessageFilter::OnRecvFromCompleted,
                 base::Unretained(this),
                 context->MakeReplyMessageContext()));
  if (net_result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(context->MakeReplyMessageContext(), net_result);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketMessageFilter::OnMsgSendTo(
    const ppapi::host::HostMessageContext* context,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_SEND_TO, addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             private_api_,
                                             &request,
                                             render_process_id_,
                                             render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&PepperUDPSocketMessageFilter::DoSendTo,
                                     this,
                                     context->MakeReplyMessageContext(),
                                     data,
                                     addr));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketMessageFilter::OnMsgClose(
    const ppapi::host::HostMessageContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Close();
  return PP_OK;
}

void PepperUDPSocketMessageFilter::DoBind(
    const ppapi::host::ReplyMessageContext& context,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (closed_ || socket_.get()) {
    SendBindError(context, PP_ERROR_FAILED);
    return;
  }

  scoped_ptr<net::UDPServerSocket> socket(
      new net::UDPServerSocket(NULL, net::NetLog::Source()));

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendBindError(context, PP_ERROR_ADDRESS_INVALID);
    return;
  }

  if (allow_address_reuse_)
    socket->AllowAddressReuse();
  if (allow_broadcast_)
    socket->AllowBroadcast();

  int32_t pp_result =
      NetErrorToPepperError(socket->Listen(net::IPEndPoint(address, port)));
  if (pp_result != PP_OK) {
    SendBindError(context, pp_result);
    return;
  }

  net::IPEndPoint bound_address;
  pp_result = NetErrorToPepperError(socket->GetLocalAddress(&bound_address));
  if (pp_result != PP_OK) {
    SendBindError(context, pp_result);
    return;
  }

  PP_NetAddress_Private net_address = NetAddressPrivateImpl::kInvalidNetAddress;
  if (!NetAddressPrivateImpl::IPEndPointToNetAddress(
          bound_address.address(), bound_address.port(), &net_address)) {
    SendBindError(context, PP_ERROR_ADDRESS_INVALID);
    return;
  }

  allow_address_reuse_ = false;
  allow_broadcast_ = false;
  socket_.swap(socket);
  SendBindReply(context, PP_OK, net_address);
}

void PepperUDPSocketMessageFilter::DoSendTo(
    const ppapi::host::ReplyMessageContext& context,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(socket_.get());

  if (closed_ || !socket_.get()) {
    SendSendToError(context, PP_ERROR_FAILED);
    return;
  }

  if (sendto_buffer_.get()) {
    SendSendToError(context, PP_ERROR_INPROGRESS);
    return;
  }

  size_t num_bytes = data.size();
  if (num_bytes == 0 ||
      num_bytes > static_cast<size_t>(
                      ppapi::proxy::UDPSocketResourceBase::kMaxWriteSize)) {
    // Size of |data| is checked on the plugin side.
    NOTREACHED();
    SendSendToError(context, PP_ERROR_BADARGUMENT);
    return;
  }

  sendto_buffer_ = new net::IOBufferWithSize(num_bytes);
  memcpy(sendto_buffer_->data(), data.data(), num_bytes);

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendSendToError(context, PP_ERROR_ADDRESS_INVALID);
    return;
  }

  // Please see OnMsgRecvFrom() for the reason why we use base::Unretained(this)
  // when calling |socket_| methods.
  int net_result = socket_->SendTo(
      sendto_buffer_.get(),
      sendto_buffer_->size(),
      net::IPEndPoint(address, port),
      base::Bind(&PepperUDPSocketMessageFilter::OnSendToCompleted,
                 base::Unretained(this),
                 context));
  if (net_result != net::ERR_IO_PENDING)
    OnSendToCompleted(context, net_result);
}

void PepperUDPSocketMessageFilter::Close() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (socket_.get() && !closed_)
    socket_->Close();
  closed_ = true;
}

void PepperUDPSocketMessageFilter::OnRecvFromCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(recvfrom_buffer_.get());

  int32_t pp_result = NetErrorToPepperError(net_result);

  // Convert IPEndPoint we get back from RecvFrom to a PP_NetAddress_Private
  // to send back.
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (pp_result >= 0 &&
      !NetAddressPrivateImpl::IPEndPointToNetAddress(
          recvfrom_address_.address(), recvfrom_address_.port(), &addr)) {
    pp_result = PP_ERROR_ADDRESS_INVALID;
  }

  if (pp_result >= 0) {
    SendRecvFromReply(
        context, PP_OK, std::string(recvfrom_buffer_->data(), pp_result), addr);
  } else {
    SendRecvFromError(context, pp_result);
  }

  recvfrom_buffer_ = NULL;
}

void PepperUDPSocketMessageFilter::OnSendToCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sendto_buffer_.get());

  int32_t pp_result = NetErrorToPepperError(net_result);
  if (pp_result < 0)
    SendSendToError(context, pp_result);
  else
    SendSendToReply(context, PP_OK, pp_result);
  sendto_buffer_ = NULL;
}

void PepperUDPSocketMessageFilter::SendBindReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    const PP_NetAddress_Private& addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context, PpapiPluginMsg_UDPSocket_BindReply(addr));
}

void PepperUDPSocketMessageFilter::SendRecvFromReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context, PpapiPluginMsg_UDPSocket_RecvFromReply(data, addr));
}

void PepperUDPSocketMessageFilter::SendSendToReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    int32_t bytes_written) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context, PpapiPluginMsg_UDPSocket_SendToReply(bytes_written));
}

void PepperUDPSocketMessageFilter::SendBindError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  SendBindReply(context, result, NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketMessageFilter::SendRecvFromError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  SendRecvFromReply(context,
                    result,
                    std::string(),
                    NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketMessageFilter::SendSendToError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  SendSendToReply(context, result, 0);
}

}  // namespace content
