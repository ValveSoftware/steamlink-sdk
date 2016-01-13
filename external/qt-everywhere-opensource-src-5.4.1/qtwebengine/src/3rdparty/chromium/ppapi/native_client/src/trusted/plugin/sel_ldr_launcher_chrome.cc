// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/module.h"
#include "ppapi/native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

bool SelLdrLauncherChrome::Start(const char* url) {
  NACL_NOTREACHED();
  return false;
}

void SelLdrLauncherChrome::Start(
    PP_Instance instance,
    bool main_service_runtime,
    const char* url,
    bool uses_irt,
    bool uses_ppapi,
    bool uses_nonsfi_mode,
    bool enable_ppapi_dev,
    bool enable_dyncode_syscalls,
    bool enable_exception_handling,
    bool enable_crash_throttling,
    const PPP_ManifestService* manifest_service_interface,
    void* manifest_service_user_data,
    pp::CompletionCallback callback) {
  if (!GetNaClInterface()) {
    pp::Module::Get()->core()->CallOnMainThread(0, callback, PP_ERROR_FAILED);
    return;
  }
  GetNaClInterface()->LaunchSelLdr(
      instance,
      PP_FromBool(main_service_runtime),
      url,
      PP_FromBool(uses_irt),
      PP_FromBool(uses_ppapi),
      PP_FromBool(uses_nonsfi_mode),
      PP_FromBool(enable_ppapi_dev),
      PP_FromBool(enable_dyncode_syscalls),
      PP_FromBool(enable_exception_handling),
      PP_FromBool(enable_crash_throttling),
      manifest_service_interface,
      manifest_service_user_data,
      &channel_,
      callback.pp_completion_callback());
}

}  // namespace plugin
