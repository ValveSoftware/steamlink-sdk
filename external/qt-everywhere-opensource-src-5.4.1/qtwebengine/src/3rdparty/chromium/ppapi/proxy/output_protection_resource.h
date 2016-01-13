// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_OUTPUT_PROTECTION_RESOURCE_H_
#define PPAPI_PROXY_OUTPUT_PROTECTION_RESOURCE_H_

#include "base/compiler_specific.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/proxy/device_enumeration_resource_helper.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/thunk/ppb_output_protection_api.h"

namespace ppapi {
namespace proxy {

class OutputProtectionResource
    : public PluginResource,
      public ::ppapi::thunk::PPB_OutputProtection_API {
 public:
  OutputProtectionResource(Connection connection,
                           PP_Instance instance);

 private:
  virtual ~OutputProtectionResource();

  // PluginResource overrides.
  virtual thunk::PPB_OutputProtection_API* AsPPB_OutputProtection_API()
      OVERRIDE;

  // PPB_OutputProtection_API implementation.
  virtual int32_t QueryStatus(
      uint32_t* link_mask,
      uint32_t* protection_mask,
      const scoped_refptr<TrackedCallback>& callback) OVERRIDE;
  virtual int32_t EnableProtection(
      uint32_t desired_method_mask,
      const scoped_refptr<TrackedCallback>& callback) OVERRIDE;

  void OnPluginMsgQueryStatusReply(
      uint32_t* out_link_mask,
      uint32_t* out_protection_mask,
      const ResourceMessageReplyParams& params,
      uint32_t link_mask,
      uint32_t protection_mask);
  void OnPluginMsgEnableProtectionReply(
      const ResourceMessageReplyParams& params);

  scoped_refptr<TrackedCallback> query_status_callback_;
  scoped_refptr<TrackedCallback> enable_protection_callback_;

  DISALLOW_COPY_AND_ASSIGN(OutputProtectionResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_OUTPUT_PROTECTION_RESOURCE_H_
