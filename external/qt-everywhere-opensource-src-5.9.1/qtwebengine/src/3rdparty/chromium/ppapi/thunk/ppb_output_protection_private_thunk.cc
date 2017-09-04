// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_output_protection_private.idl modified Wed Jan 27 17:10:16
// 2016.

#include <stdint.h>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_output_protection_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_OutputProtection_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateOutputProtectionPrivate(instance);
}

PP_Bool IsOutputProtection(PP_Resource resource) {
  VLOG(4) << "PPB_OutputProtection_Private::IsOutputProtection()";
  EnterResource<PPB_OutputProtection_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t QueryStatus(PP_Resource resource,
                    uint32_t* link_mask,
                    uint32_t* protection_mask,
                    struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_OutputProtection_Private::QueryStatus()";
  EnterResource<PPB_OutputProtection_API> enter(resource, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->QueryStatus(link_mask, protection_mask,
                                                     enter.callback()));
}

int32_t EnableProtection(PP_Resource resource,
                         uint32_t desired_protection_mask,
                         struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_OutputProtection_Private::EnableProtection()";
  EnterResource<PPB_OutputProtection_API> enter(resource, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->EnableProtection(
      desired_protection_mask, enter.callback()));
}

const PPB_OutputProtection_Private_0_1
    g_ppb_outputprotection_private_thunk_0_1 = {
        &Create, &IsOutputProtection, &QueryStatus, &EnableProtection};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_OutputProtection_Private_0_1*
GetPPB_OutputProtection_Private_0_1_Thunk() {
  return &g_ppb_outputprotection_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
