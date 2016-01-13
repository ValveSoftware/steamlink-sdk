// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_zoom_dev.idl modified Tue Aug 20 08:13:36 2013.

#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

void ZoomChanged(PP_Instance instance, double factor) {
  VLOG(4) << "PPB_Zoom_Dev::ZoomChanged()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->ZoomChanged(instance, factor);
}

void ZoomLimitsChanged(PP_Instance instance,
                       double minimum_factor,
                       double maximum_factor) {
  VLOG(4) << "PPB_Zoom_Dev::ZoomLimitsChanged()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->ZoomLimitsChanged(instance,
                                       minimum_factor,
                                       maximum_factor);
}

const PPB_Zoom_Dev_0_2 g_ppb_zoom_dev_thunk_0_2 = {
  &ZoomChanged,
  &ZoomLimitsChanged
};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Zoom_Dev_0_2* GetPPB_Zoom_Dev_0_2_Thunk() {
  return &g_ppb_zoom_dev_thunk_0_2;
}

}  // namespace thunk
}  // namespace ppapi
