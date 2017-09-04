// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/output_protection_private.h"

#include <stdio.h>
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_OutputProtection_Private_0_1>() {
  return PPB_OUTPUTPROTECTION_PRIVATE_INTERFACE_0_1;
}

}  // namespace

OutputProtection_Private::OutputProtection_Private(
    const InstanceHandle& instance) {
  if (has_interface<PPB_OutputProtection_Private_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_OutputProtection_Private_0_1>()->Create(
            instance.pp_instance()));
  }
}

OutputProtection_Private::~OutputProtection_Private() {
}

int32_t OutputProtection_Private::QueryStatus(
    uint32_t* link_mask,
    uint32_t* protection_mask,
    const CompletionCallback& callback) {
  if (has_interface<PPB_OutputProtection_Private_0_1>()) {
    return get_interface<PPB_OutputProtection_Private_0_1>()->QueryStatus(
        pp_resource(), link_mask, protection_mask,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t OutputProtection_Private::EnableProtection(
    uint32_t desired_method_mask,
    const CompletionCallback& callback) {
  if (has_interface<PPB_OutputProtection_Private_0_1>()) {
    return get_interface<PPB_OutputProtection_Private_0_1>()->EnableProtection(
        pp_resource(), desired_method_mask,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
