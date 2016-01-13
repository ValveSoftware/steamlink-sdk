// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_OUTPUT_PROTECTION_API_H_
#define PPAPI_THUNK_OUTPUT_PROTECTION_API_H_

#include "ppapi/c/private/ppb_output_protection_private.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_OutputProtection_API {
 public:
  virtual ~PPB_OutputProtection_API() {}

  virtual int32_t QueryStatus(
      uint32_t* link_mask,
      uint32_t* protection_mask,
      const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t EnableProtection(
      uint32_t desired_method_mask,
      const scoped_refptr<TrackedCallback>& callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_OUTPUT_PROTECTION_API_H_
