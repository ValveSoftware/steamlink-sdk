// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_PLUGIN_MAIN_H_
#define PPAPI_NACL_IRT_PLUGIN_MAIN_H_

#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

#ifdef __cplusplus
extern "C" {
#endif

// The entry point for the main thread of the PPAPI plugin process.
PPAPI_PROXY_EXPORT int PpapiPluginMain(void);

PPAPI_PROXY_EXPORT void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* new_funcs);

#ifdef __cplusplus
}
#endif

#endif  // PPAPI_NACL_IRT_PLUGIN_MAIN_H_
