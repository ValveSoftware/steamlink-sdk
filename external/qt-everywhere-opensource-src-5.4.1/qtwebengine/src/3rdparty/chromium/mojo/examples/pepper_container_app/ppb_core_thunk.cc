// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "mojo/examples/pepper_container_app/thunk.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace mojo {
namespace examples {

namespace {

void AddRefResource(PP_Resource resource) {
  ppapi::ProxyAutoLock lock;
  ppapi::PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  ppapi::ProxyAutoLock lock;
  ppapi::PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(resource);
}

PP_Time GetTime() {
  NOTIMPLEMENTED();
  return 0;
}

PP_TimeTicks GetTimeTicks() {
  NOTIMPLEMENTED();
  return 0;
}

void CallOnMainThread(int32_t delay_in_milliseconds,
                      PP_CompletionCallback callback,
                      int32_t result) {
  NOTIMPLEMENTED();
}

PP_Bool IsMainThread() {
  NOTIMPLEMENTED();
  return PP_TRUE;
}

}  // namespace

const PPB_Core_1_0 g_ppb_core_thunk_1_0 = {
  &AddRefResource,
  &ReleaseResource,
  &GetTime,
  &GetTimeTicks,
  &CallOnMainThread,
  &IsMainThread
};

const PPB_Core_1_0* GetPPB_Core_1_0_Thunk() {
  return &g_ppb_core_thunk_1_0;
}

}  // namespace examples
}  // namespace mojo
