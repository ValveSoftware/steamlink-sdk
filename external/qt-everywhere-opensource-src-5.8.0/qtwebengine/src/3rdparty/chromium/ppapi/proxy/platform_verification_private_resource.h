// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLATFORM_VERIFICATION_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_PLATFORM_VERIFICATION_PRIVATE_RESOURCE_H_

#include <stdint.h>

#include "base/macros.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_platform_verification_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT PlatformVerificationPrivateResource
    : public PluginResource,
      public thunk::PPB_PlatformVerification_API {
 public:
  PlatformVerificationPrivateResource(Connection connection,
                                      PP_Instance instance);

 private:
  struct ChallengePlatformParams {
    PP_Var* signed_data;
    PP_Var* signed_data_signature;
    PP_Var* platform_key_certificate;
    scoped_refptr<TrackedCallback> callback;
  };

  ~PlatformVerificationPrivateResource() override;

  // PluginResource overrides.
  thunk::PPB_PlatformVerification_API* AsPPB_PlatformVerification_API()
      override;

  // PPB_PlatformVerification_API implementation.
  int32_t ChallengePlatform(
      const PP_Var& service_id,
      const PP_Var& challenge,
      PP_Var* signed_data,
      PP_Var* signed_data_signature,
      PP_Var* platform_key_certificate,
      const scoped_refptr<TrackedCallback>& callback) override;
  void OnChallengePlatformReply(
      ChallengePlatformParams output_params,
      const ResourceMessageReplyParams& params,
      const std::vector<uint8_t>& raw_signed_data,
      const std::vector<uint8_t>& raw_signed_data_signature,
      const std::string& raw_platform_key_certificate);

  DISALLOW_COPY_AND_ASSIGN(PlatformVerificationPrivateResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLATFORM_VERIFICATION_PRIVATE_RESOURCE_H_
