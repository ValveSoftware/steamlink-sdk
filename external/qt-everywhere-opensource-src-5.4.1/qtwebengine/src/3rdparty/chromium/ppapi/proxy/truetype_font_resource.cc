// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/truetype_font_resource.h"

#include "base/bind.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_TrueTypeFont_API;

namespace {

}  // namespace

namespace ppapi {
namespace proxy {

TrueTypeFontResource::TrueTypeFontResource(
    Connection connection,
    PP_Instance instance,
    const PP_TrueTypeFontDesc_Dev& desc)
    : PluginResource(connection, instance) {
  SerializedTrueTypeFontDesc serialized_desc;
  serialized_desc.SetFromPPTrueTypeFontDesc(desc);
  SendCreate(RENDERER, PpapiHostMsg_TrueTypeFont_Create(serialized_desc));
}

TrueTypeFontResource::~TrueTypeFontResource() {
}

PPB_TrueTypeFont_API* TrueTypeFontResource::AsPPB_TrueTypeFont_API() {
  return this;
}

int32_t TrueTypeFontResource::Describe(
    PP_TrueTypeFontDesc_Dev* desc,
    scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_TrueTypeFont_DescribeReply>(RENDERER,
      PpapiHostMsg_TrueTypeFont_Describe(),
      base::Bind(&TrueTypeFontResource::OnPluginMsgDescribeComplete, this,
                 callback, desc));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TrueTypeFontResource::GetTableTags(
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_TrueTypeFont_GetTableTagsReply>(RENDERER,
      PpapiHostMsg_TrueTypeFont_GetTableTags(),
      base::Bind(&TrueTypeFontResource::OnPluginMsgGetTableTagsComplete, this,
                 callback, output));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TrueTypeFontResource::GetTable(
    uint32_t table,
    int32_t offset,
    int32_t max_data_length,
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_TrueTypeFont_GetTableReply>(RENDERER,
      PpapiHostMsg_TrueTypeFont_GetTable(table, offset, max_data_length),
      base::Bind(&TrueTypeFontResource::OnPluginMsgGetTableComplete, this,
                 callback, output));
  return PP_OK_COMPLETIONPENDING;
}

void TrueTypeFontResource::OnPluginMsgDescribeComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_TrueTypeFontDesc_Dev* pp_desc,
    const ResourceMessageReplyParams& params,
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc) {
  int32_t result = params.result();
  if (result == PP_OK)
    desc.CopyToPPTrueTypeFontDesc(pp_desc);

  callback->Run(result);
}

void TrueTypeFontResource::OnPluginMsgGetTableTagsComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_ArrayOutput array_output,
    const ResourceMessageReplyParams& params,
    const std::vector<uint32_t>& tag_array) {
  // The result code should contain the data size if it's positive.
  int32_t result = params.result();
  DCHECK((result < 0 && tag_array.size() == 0) ||
         result == static_cast<int32_t>(tag_array.size()));

  ArrayWriter output;
  output.set_pp_array_output(array_output);
  if (output.is_valid())
    output.StoreArray(&tag_array[0], std::max(0, result));
  else
    result = PP_ERROR_FAILED;

  callback->Run(result);
}

void TrueTypeFontResource::OnPluginMsgGetTableComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_ArrayOutput array_output,
    const ResourceMessageReplyParams& params,
    const std::string& data) {
  // The result code should contain the data size if it's positive.
  int32_t result = params.result();
  DCHECK((result < 0 && data.size() == 0) ||
         result == static_cast<int32_t>(data.size()));

  ArrayWriter output;
  output.set_pp_array_output(array_output);
  if (output.is_valid())
    output.StoreArray(data.data(), std::max(0, result));
  else
    result = PP_ERROR_FAILED;

  callback->Run(result);
}

}  // namespace proxy
}  // namespace ppapi
