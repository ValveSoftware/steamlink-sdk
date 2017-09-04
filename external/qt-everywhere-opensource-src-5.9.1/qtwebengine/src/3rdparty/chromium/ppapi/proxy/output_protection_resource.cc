// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/output_protection_resource.h"

#include "base/logging.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_output_protection_api.h"

namespace ppapi {
namespace proxy {

OutputProtectionResource::OutputProtectionResource(
    Connection connection,
    PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_OutputProtection_Create());
}

OutputProtectionResource::~OutputProtectionResource() {
  if (TrackedCallback::IsPending(query_status_callback_))
    query_status_callback_->PostAbort();
  if (TrackedCallback::IsPending(enable_protection_callback_))
    enable_protection_callback_->PostAbort();
}

thunk::PPB_OutputProtection_API*
    OutputProtectionResource::AsPPB_OutputProtection_API() {
  return this;
}

int32_t OutputProtectionResource::QueryStatus(
    uint32_t* link_mask,
    uint32_t* protection_mask,
    const scoped_refptr<TrackedCallback>& callback) {
  if (!link_mask || !protection_mask)
    return PP_ERROR_BADARGUMENT;
  if (TrackedCallback::IsPending(query_status_callback_))
    return PP_ERROR_INPROGRESS;

  query_status_callback_ = callback;

  Call<PpapiPluginMsg_OutputProtection_QueryStatusReply>(
      BROWSER,
      PpapiHostMsg_OutputProtection_QueryStatus(),
      base::Bind(&OutputProtectionResource::OnPluginMsgQueryStatusReply,
                 base::Unretained(this),
                 link_mask,
                 protection_mask));
  return PP_OK_COMPLETIONPENDING;
}

void OutputProtectionResource::OnPluginMsgQueryStatusReply(
    uint32_t* out_link_mask,
    uint32_t* out_protection_mask,
    const ResourceMessageReplyParams& params,
    uint32_t link_mask,
    uint32_t protection_mask) {
  // The callback may have been aborted.
  if (!TrackedCallback::IsPending(query_status_callback_))
    return;

  int32_t result = params.result();

  if (result == PP_OK) {
    DCHECK(out_link_mask);
    DCHECK(out_protection_mask);
    *out_link_mask = link_mask;
    *out_protection_mask = protection_mask;
  }
  query_status_callback_->Run(result);
}

int32_t OutputProtectionResource::EnableProtection(
    uint32_t desired_method_mask,
    const scoped_refptr<TrackedCallback>& callback) {
  if (TrackedCallback::IsPending(enable_protection_callback_))
    return PP_ERROR_INPROGRESS;

  enable_protection_callback_ = callback;

  Call<PpapiPluginMsg_OutputProtection_EnableProtectionReply>(
      BROWSER,
      PpapiHostMsg_OutputProtection_EnableProtection(desired_method_mask),
      base::Bind(&OutputProtectionResource::OnPluginMsgEnableProtectionReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

void OutputProtectionResource::OnPluginMsgEnableProtectionReply(
    const ResourceMessageReplyParams& params) {
  // The callback may have been aborted.
  if (TrackedCallback::IsPending(enable_protection_callback_))
    enable_protection_callback_->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
