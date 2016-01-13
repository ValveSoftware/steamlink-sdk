// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_truetype_font_host.h"

#include "base/bind.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_truetype_font.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using ppapi::host::HostMessageContext;
using ppapi::host::ReplyMessageContext;

namespace content {

PepperTrueTypeFontHost::PepperTrueTypeFontHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      font_(PepperTrueTypeFont::Create(desc)),
      weak_factory_(this) {}

PepperTrueTypeFontHost::~PepperTrueTypeFontHost() {}

int32_t PepperTrueTypeFontHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  if (!host()->permissions().HasPermission(ppapi::PERMISSION_DEV))
    return PP_ERROR_FAILED;

  PPAPI_BEGIN_MESSAGE_MAP(PepperTrueTypeFontHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_TrueTypeFont_Describe,
                                        OnHostMsgDescribe)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_TrueTypeFont_GetTableTags,
                                        OnHostMsgGetTableTags)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TrueTypeFont_GetTable,
                                      OnHostMsgGetTable)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperTrueTypeFontHost::OnHostMsgDescribe(HostMessageContext* context) {
  if (!font_->IsValid())
    return PP_ERROR_FAILED;

  ppapi::proxy::SerializedTrueTypeFontDesc desc;
  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  reply_context.params.set_result(font_->Describe(&desc));
  host()->SendReply(reply_context,
                    PpapiPluginMsg_TrueTypeFont_DescribeReply(desc));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTrueTypeFontHost::OnHostMsgGetTableTags(
    HostMessageContext* context) {
  if (!font_->IsValid())
    return PP_ERROR_FAILED;

  std::vector<uint32_t> tags;
  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  reply_context.params.set_result(font_->GetTableTags(&tags));
  host()->SendReply(reply_context,
                    PpapiPluginMsg_TrueTypeFont_GetTableTagsReply(tags));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTrueTypeFontHost::OnHostMsgGetTable(HostMessageContext* context,
                                                  uint32_t table,
                                                  int32_t offset,
                                                  int32_t max_data_length) {
  if (!font_->IsValid())
    return PP_ERROR_FAILED;
  if (offset < 0 || max_data_length < 0)
    return PP_ERROR_BADARGUMENT;

  std::string data;
  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  reply_context.params.set_result(
      font_->GetTable(table, offset, max_data_length, &data));
  host()->SendReply(reply_context,
                    PpapiPluginMsg_TrueTypeFont_GetTableReply(data));
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace content
