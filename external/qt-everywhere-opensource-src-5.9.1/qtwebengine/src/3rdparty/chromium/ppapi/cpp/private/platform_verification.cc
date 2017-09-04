// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/platform_verification.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_platform_verification_private.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_PlatformVerification_Private_0_2>() {
  return PPB_PLATFORMVERIFICATION_PRIVATE_INTERFACE_0_2;
}

inline bool HasInterface() {
  return has_interface<PPB_PlatformVerification_Private_0_2>();
}

inline const PPB_PlatformVerification_Private_0_2* GetInterface() {
  return get_interface<PPB_PlatformVerification_Private_0_2>();
}

}  // namespace

PlatformVerification::PlatformVerification(const InstanceHandle& instance) {
  if (HasInterface())
    PassRefFromConstructor(GetInterface()->Create(instance.pp_instance()));
}

PlatformVerification::~PlatformVerification() {}

int32_t PlatformVerification::ChallengePlatform(
    const Var& service_id,
    const Var& challenge,
    Var* signed_data,
    Var* signed_data_signature,
    Var* platform_key_certificate,
    const CompletionCallback& callback) {
  if (!HasInterface())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return GetInterface()->ChallengePlatform(
      pp_resource(), service_id.pp_var(), challenge.pp_var(),
      const_cast<PP_Var*>(&signed_data->pp_var()),
      const_cast<PP_Var*>(&signed_data_signature->pp_var()),
      const_cast<PP_Var*>(&platform_key_certificate->pp_var()),
      callback.pp_completion_callback());
}

}  // namespace pp
