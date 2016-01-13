// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_PLATFORM_VERIFICATION_API_H_
#define PPAPI_THUNK_PPB_PLATFORM_VERIFICATION_API_H_

#include "base/memory/ref_counted.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_PlatformVerification_API {
 public:
  virtual ~PPB_PlatformVerification_API() {}

  virtual int32_t ChallengePlatform(
      const PP_Var& service_id,
      const PP_Var& challenge,
      PP_Var* signed_data,
      PP_Var* signed_data_signature,
      PP_Var* platform_key_certificate,
      const scoped_refptr<TrackedCallback>& callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_PLATFORM_VERIFICATION_API_H_
