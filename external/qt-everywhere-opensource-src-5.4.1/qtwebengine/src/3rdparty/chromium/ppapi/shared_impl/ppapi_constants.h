// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_CONSTANTS_H_
#define PPAPI_SHARED_IMPL_PPAPI_CONSTANTS_H_

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// Default interval to space out IPC messages sent indicating that a plugin is
// active and should be kept alive. The value must be smaller than any threshold
// used to kill inactive plugins by the embedder host.
const unsigned kKeepaliveThrottleIntervalDefaultMilliseconds = 5000;

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_CONSTANTS_H_
