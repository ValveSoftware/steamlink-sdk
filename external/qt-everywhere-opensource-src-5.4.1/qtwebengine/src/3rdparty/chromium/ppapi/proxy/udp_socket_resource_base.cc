// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/udp_socket_resource_base.h"

#include <algorithm>
#include <cstring>

#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/error_conversion.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace proxy {

const int32_t UDPSocketResourceBase::kMaxReadSize = 1024 * 1024;
const int32_t UDPSocketResourceBase::kMaxWriteSize = 1024 * 1024;
const int32_t UDPSocketResourceBase::kMaxSendBufferSize =
    1024 * UDPSocketResourceBase::kMaxWriteSize;
const int32_t UDPSocketResourceBase::kMaxReceiveBufferSize =
    1024 * UDPSocketResourceBase::kMaxReadSize;


UDPSocketResourceBase::UDPSocketResourceBase(Connection connection,
                                             PP_Instance instance,
                                             bool private_api)
    : PluginResource(connection, instance),
      private_api_(private_api),
      bound_(false),
      closed_(false),
      read_buffer_(NULL),
      bytes_to_read_(-1) {
  recvfrom_addr_.size = 0;
  memset(recvfrom_addr_.data, 0,
         arraysize(recvfrom_addr_.data) * sizeof(*recvfrom_addr_.data));
  bound_addr_.size = 0;
  memset(bound_addr_.data, 0,
         arraysize(bound_addr_.data) * sizeof(*bound_addr_.data));

  if (private_api)
    SendCreate(BROWSER, PpapiHostMsg_UDPSocket_CreatePrivate());
  else
    SendCreate(BROWSER, PpapiHostMsg_UDPSocket_Create());
}

UDPSocketResourceBase::~UDPSocketResourceBase() {
}

int32_t UDPSocketResourceBase::SetOptionImpl(
    PP_UDPSocket_Option name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  if (closed_)
    return PP_ERROR_FAILED;

  SocketOptionData option_data;
  switch (name) {
    case PP_UDPSOCKET_OPTION_ADDRESS_REUSE:
    case PP_UDPSOCKET_OPTION_BROADCAST: {
      if (bound_)
        return PP_ERROR_FAILED;
      if (value.type != PP_VARTYPE_BOOL)
        return PP_ERROR_BADARGUMENT;
      option_data.SetBool(PP_ToBool(value.value.as_bool));
      break;
    }
    case PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE:
    case PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      if (!bound_)
        return PP_ERROR_FAILED;
      if (value.type != PP_VARTYPE_INT32)
        return PP_ERROR_BADARGUMENT;
      option_data.SetInt32(value.value.as_int);
      break;
    }
    default: {
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
    }
  }

  Call<PpapiPluginMsg_UDPSocket_SetOptionReply>(
      BROWSER,
      PpapiHostMsg_UDPSocket_SetOption(name, option_data),
      base::Bind(&UDPSocketResourceBase::OnPluginMsgSetOptionReply,
                 base::Unretained(this),
                 callback),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t UDPSocketResourceBase::BindImpl(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (bound_ || closed_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(bind_callback_))
    return PP_ERROR_INPROGRESS;

  bind_callback_ = callback;

  // Send the request, the browser will call us back via BindReply.
  Call<PpapiPluginMsg_UDPSocket_BindReply>(
      BROWSER,
      PpapiHostMsg_UDPSocket_Bind(*addr),
      base::Bind(&UDPSocketResourceBase::OnPluginMsgBindReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool UDPSocketResourceBase::GetBoundAddressImpl(
    PP_NetAddress_Private* addr) {
  if (!addr || !bound_ || closed_)
    return PP_FALSE;

  *addr = bound_addr_;
  return PP_TRUE;
}

int32_t UDPSocketResourceBase::RecvFromImpl(
    char* buffer,
    int32_t num_bytes,
    PP_Resource* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || num_bytes <= 0)
    return PP_ERROR_BADARGUMENT;
  if (!bound_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(recvfrom_callback_))
    return PP_ERROR_INPROGRESS;

  read_buffer_ = buffer;
  bytes_to_read_ = std::min(num_bytes, kMaxReadSize);
  recvfrom_callback_ = callback;

  // Send the request, the browser will call us back via RecvFromReply.
  Call<PpapiPluginMsg_UDPSocket_RecvFromReply>(
      BROWSER,
      PpapiHostMsg_UDPSocket_RecvFrom(bytes_to_read_),
      base::Bind(&UDPSocketResourceBase::OnPluginMsgRecvFromReply,
                 base::Unretained(this), addr),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool UDPSocketResourceBase::GetRecvFromAddressImpl(
    PP_NetAddress_Private* addr) {
  if (!addr)
    return PP_FALSE;
  *addr = recvfrom_addr_;
  return PP_TRUE;
}

int32_t UDPSocketResourceBase::SendToImpl(
    const char* buffer,
    int32_t num_bytes,
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || num_bytes <= 0 || !addr)
    return PP_ERROR_BADARGUMENT;
  if (!bound_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(sendto_callback_))
    return PP_ERROR_INPROGRESS;

  if (num_bytes > kMaxWriteSize)
    num_bytes = kMaxWriteSize;

  sendto_callback_ = callback;

  // Send the request, the browser will call us back via SendToReply.
  Call<PpapiPluginMsg_UDPSocket_SendToReply>(
      BROWSER,
      PpapiHostMsg_UDPSocket_SendTo(std::string(buffer, num_bytes), *addr),
      base::Bind(&UDPSocketResourceBase::OnPluginMsgSendToReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

void UDPSocketResourceBase::CloseImpl() {
  if(closed_)
    return;

  bound_ = false;
  closed_ = true;

  Post(BROWSER, PpapiHostMsg_UDPSocket_Close());

  PostAbortIfNecessary(&bind_callback_);
  PostAbortIfNecessary(&recvfrom_callback_);
  PostAbortIfNecessary(&sendto_callback_);

  read_buffer_ = NULL;
  bytes_to_read_ = -1;
}

void UDPSocketResourceBase::PostAbortIfNecessary(
    scoped_refptr<TrackedCallback>* callback) {
  if (TrackedCallback::IsPending(*callback))
    (*callback)->PostAbort();
}

void UDPSocketResourceBase::OnPluginMsgSetOptionReply(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  if (TrackedCallback::IsPending(callback))
    RunCallback(callback, params.result());
}

void UDPSocketResourceBase::OnPluginMsgBindReply(
    const ResourceMessageReplyParams& params,
    const PP_NetAddress_Private& bound_addr) {
  // It is possible that |bind_callback_| is pending while |closed_| is true:
  // CloseImpl() has been called, but a BindReply came earlier than the task to
  // abort |bind_callback_|. We don't want to update |bound_| or |bound_addr_|
  // in that case.
  if (!TrackedCallback::IsPending(bind_callback_) || closed_)
    return;

  if (params.result() == PP_OK)
    bound_ = true;
  bound_addr_ = bound_addr;
  RunCallback(bind_callback_, params.result());
}

void UDPSocketResourceBase::OnPluginMsgRecvFromReply(
    PP_Resource* output_addr,
    const ResourceMessageReplyParams& params,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  // It is possible that |recvfrom_callback_| is pending while |read_buffer_| is
  // NULL: CloseImpl() has been called, but a RecvFromReply came earlier than
  // the task to abort |recvfrom_callback_|. We shouldn't access the buffer in
  // that case. The user may have released it.
  if (!TrackedCallback::IsPending(recvfrom_callback_) || !read_buffer_)
    return;

  int32_t result = params.result();
  if (result == PP_OK && output_addr) {
    thunk::EnterResourceCreationNoLock enter(pp_instance());
    if (enter.succeeded()) {
      *output_addr = enter.functions()->CreateNetAddressFromNetAddressPrivate(
          pp_instance(), addr);
    } else {
      result = PP_ERROR_FAILED;
    }
  }

  if (result == PP_OK) {
    CHECK_LE(static_cast<int32_t>(data.size()), bytes_to_read_);
    if (!data.empty())
      memcpy(read_buffer_, data.c_str(), data.size());
  }

  read_buffer_ = NULL;
  bytes_to_read_ = -1;
  recvfrom_addr_ = addr;

  if (result == PP_OK)
    RunCallback(recvfrom_callback_, static_cast<int32_t>(data.size()));
  else
    RunCallback(recvfrom_callback_, result);
}

void UDPSocketResourceBase::OnPluginMsgSendToReply(
    const ResourceMessageReplyParams& params,
    int32_t bytes_written) {
  if (!TrackedCallback::IsPending(sendto_callback_))
    return;

  if (params.result() == PP_OK)
    RunCallback(sendto_callback_, bytes_written);
  else
    RunCallback(sendto_callback_, params.result());
}

void UDPSocketResourceBase::RunCallback(scoped_refptr<TrackedCallback> callback,
                                        int32_t pp_result) {
  callback->Run(ConvertNetworkAPIErrorForCompatibility(pp_result,
                                                       private_api_));
}

}  // namespace proxy
}  // namespace ppapi
