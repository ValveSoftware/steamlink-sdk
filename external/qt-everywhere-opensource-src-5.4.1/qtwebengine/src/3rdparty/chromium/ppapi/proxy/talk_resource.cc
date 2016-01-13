// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/talk_resource.h"

#include "base/bind.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

TalkResource::TalkResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance),
      event_callback_(NULL),
      event_callback_user_data_(NULL) {
  SendCreate(BROWSER, PpapiHostMsg_Talk_Create());
}

TalkResource::~TalkResource() {
}

thunk::PPB_Talk_Private_API* TalkResource::AsPPB_Talk_Private_API() {
  return this;
}

int32_t TalkResource::RequestPermission(
    PP_TalkPermission permission,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(permission_callback_))
    return PP_ERROR_INPROGRESS;

  permission_callback_ = callback;

  Call<PpapiPluginMsg_Talk_RequestPermissionReply>(
      BROWSER,
      PpapiHostMsg_Talk_RequestPermission(permission),
      base::Bind(&TalkResource::OnRequestPermissionReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TalkResource::StartRemoting(PP_TalkEventCallback event_callback,
                                    void* user_data,
                                    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(start_callback_) ||
      event_callback_ != NULL)
    return PP_ERROR_INPROGRESS;

  start_callback_ = callback;
  event_callback_ = event_callback;
  event_callback_user_data_ = user_data;

  Call<PpapiPluginMsg_Talk_StartRemotingReply>(
      BROWSER,
      PpapiHostMsg_Talk_StartRemoting(),
      base::Bind(&TalkResource::OnStartRemotingReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TalkResource::StopRemoting(scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(stop_callback_))
    return PP_ERROR_INPROGRESS;

  if (event_callback_ == NULL)
    return PP_ERROR_FAILED;

  stop_callback_ = callback;

  Call<PpapiPluginMsg_Talk_StopRemotingReply>(
      BROWSER,
      PpapiHostMsg_Talk_StopRemoting(),
      base::Bind(&TalkResource::OnStopRemotingReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

void TalkResource::OnReplyReceived(const ResourceMessageReplyParams& params,
                                   const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(TalkResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_Talk_NotifyEvent,
        OnNotifyEvent)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  PPAPI_END_MESSAGE_MAP()
}

void TalkResource::OnNotifyEvent(const ResourceMessageReplyParams& params,
                                 PP_TalkEvent event) {
  if (event_callback_ != NULL)
    event_callback_(event_callback_user_data_, event);
}

void TalkResource::OnRequestPermissionReply(
    const ResourceMessageReplyParams& params) {
  permission_callback_->Run(params.result());
}

void TalkResource::OnStartRemotingReply(
    const ResourceMessageReplyParams& params) {
  start_callback_->Run(params.result());
}

void TalkResource::OnStopRemotingReply(
    const ResourceMessageReplyParams& params) {
  event_callback_ = NULL;
  event_callback_user_data_ = NULL;
  stop_callback_->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
