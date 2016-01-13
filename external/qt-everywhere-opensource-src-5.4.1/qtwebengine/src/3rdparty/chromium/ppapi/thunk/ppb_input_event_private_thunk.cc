// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_input_event_private.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_input_event_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool TraceInputLatency(PP_Resource event, PP_Bool has_damage) {
  EnterResource<PPB_InputEvent_API> enter(event, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->TraceInputLatency(has_damage);
}

void StartTrackingLatency(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->StartTrackingLatency(instance);
}

const PPB_InputEvent_Private_0_1 g_ppb_input_event_private_thunk_0_1 = {
  &TraceInputLatency,
  &StartTrackingLatency
};

}  // namespace

const PPB_InputEvent_Private_0_1* GetPPB_InputEvent_Private_0_1_Thunk() {
  return &g_ppb_input_event_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
