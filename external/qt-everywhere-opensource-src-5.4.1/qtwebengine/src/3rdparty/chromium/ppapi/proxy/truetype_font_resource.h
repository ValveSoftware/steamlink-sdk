// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TRUETYPE_FONT_RESOURCE_H_
#define PPAPI_PROXY_TRUETYPE_FONT_RESOURCE_H_

#include <string>

#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/ppb_truetype_font_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

struct SerializedTrueTypeFontDesc;

class PPAPI_PROXY_EXPORT TrueTypeFontResource
    : public PluginResource,
      public thunk::PPB_TrueTypeFont_API {
 public:
  TrueTypeFontResource(Connection connection,
                       PP_Instance instance,
                       const PP_TrueTypeFontDesc_Dev& desc);
  virtual ~TrueTypeFontResource();

  // Resource overrides.
  virtual thunk::PPB_TrueTypeFont_API* AsPPB_TrueTypeFont_API() OVERRIDE;

  // PPB_TrueTypeFont_API implementation.
  virtual int32_t Describe(
      PP_TrueTypeFontDesc_Dev* desc,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t GetTableTags(
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t GetTable(
      uint32_t table,
      int32_t offset,
      int32_t max_data_length,
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  void OnPluginMsgDescribeComplete(
      scoped_refptr<TrackedCallback> callback,
      PP_TrueTypeFontDesc_Dev* pp_desc,
      const ResourceMessageReplyParams& params,
      const ppapi::proxy::SerializedTrueTypeFontDesc& desc);
  void OnPluginMsgGetTableTagsComplete(
      scoped_refptr<TrackedCallback> callback,
      PP_ArrayOutput array_output,
      const ResourceMessageReplyParams& params,
      const std::vector<uint32_t>& data);
  void OnPluginMsgGetTableComplete(
      scoped_refptr<TrackedCallback> callback,
      PP_ArrayOutput array_output,
      const ResourceMessageReplyParams& params,
      const std::string& data);

  DISALLOW_COPY_AND_ASSIGN(TrueTypeFontResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TRUETYPE_FONT_RESOURCE_H_
