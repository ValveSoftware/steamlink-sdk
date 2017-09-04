// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_OUTPUT_PROTECTION_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_OUTPUT_PROTECTION_PRIVATE_H_

#include <stdint.h>

#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class OutputProtection_Private : public Resource {
 public:
  explicit OutputProtection_Private(const InstanceHandle& instance);
  virtual ~OutputProtection_Private();

  // PPB_OutputProtection_Private implementation.
  int32_t QueryStatus(uint32_t* link_mask, uint32_t* protection_mask,
      const CompletionCallback& callback);
  int32_t EnableProtection(uint32_t desired_method_mask,
      const CompletionCallback& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_OUTPUT_PROTECTION_PRIVATE_H_
