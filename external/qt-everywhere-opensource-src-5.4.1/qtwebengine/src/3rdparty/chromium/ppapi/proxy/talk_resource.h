// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TALK_RESOURCE_H_
#define PPAPI_PROXY_TALK_RESOURCE_H_

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_talk_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT TalkResource
    : public PluginResource,
      public NON_EXPORTED_BASE(thunk::PPB_Talk_Private_API) {
 public:
  TalkResource(Connection connection, PP_Instance instance);
  virtual ~TalkResource();

  // Resource overrides.
  thunk::PPB_Talk_Private_API* AsPPB_Talk_Private_API();

 private:
  // PPB_Talk_API implementation.
  virtual int32_t RequestPermission(
      PP_TalkPermission permission,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t StartRemoting(
      PP_TalkEventCallback event_callback,
      void* user_data,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t StopRemoting(
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

  // PluginResource override.
  virtual void OnReplyReceived(const ResourceMessageReplyParams& params,
                               const IPC::Message& msg) OVERRIDE;

  void OnNotifyEvent(const ResourceMessageReplyParams& params,
                     PP_TalkEvent event);
  void OnRequestPermissionReply(const ResourceMessageReplyParams& params);
  void OnStartRemotingReply(const ResourceMessageReplyParams& params);
  void OnStopRemotingReply(const ResourceMessageReplyParams& params);

  scoped_refptr<TrackedCallback> permission_callback_;
  scoped_refptr<TrackedCallback> start_callback_;
  scoped_refptr<TrackedCallback> stop_callback_;
  PP_TalkEventCallback event_callback_;
  void* event_callback_user_data_;

  DISALLOW_COPY_AND_ASSIGN(TalkResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TALK_RESOURCE_H_
