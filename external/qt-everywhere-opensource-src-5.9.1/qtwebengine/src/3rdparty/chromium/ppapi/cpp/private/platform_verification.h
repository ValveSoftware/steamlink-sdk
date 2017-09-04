// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_PLATFORM_VERIFICATION_H_
#define PPAPI_CPP_PRIVATE_PLATFORM_VERIFICATION_H_

#include <stdint.h>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class InstanceHandle;
class Var;

class PlatformVerification : public Resource {
 public:
  explicit PlatformVerification(const InstanceHandle& instance);
  virtual ~PlatformVerification();

  int32_t ChallengePlatform(const Var& service_id,
                            const Var& challenge,
                            Var* signed_data,
                            Var* signed_data_signature,
                            Var* platform_key_certificate,
                            const CompletionCallback& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_PLATFORM_VERIFICATION_H_
