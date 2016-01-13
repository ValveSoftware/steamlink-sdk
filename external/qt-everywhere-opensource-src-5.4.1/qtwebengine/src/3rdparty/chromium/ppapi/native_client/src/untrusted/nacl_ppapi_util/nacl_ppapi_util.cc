/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/untrusted/nacl_ppapi_util/nacl_ppapi_util.h"

// TODO(bsy): move weak_ref module to the shared directory
#include "native_client/src/trusted/weak_ref/weak_ref.h"
#include "ppapi/native_client/src/trusted/weak_ref/call_on_main_thread.h"


namespace nacl_ppapi {

static VoidResult kVoidResult;
VoidResult *const g_void_result = &kVoidResult;

NaClPpapiPluginInstance::NaClPpapiPluginInstance(PP_Instance instance)
    : pp::Instance(instance) {
  anchor_ = new nacl::WeakRefAnchor();
}

NaClPpapiPluginInstance::~NaClPpapiPluginInstance() {
  anchor_->Abandon();
  anchor_->Unref();
}

}  // namespace nacl_ppapi
