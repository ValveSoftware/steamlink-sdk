// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ZYGOTE_HANDLE_LINUX_H_
#define CONTENT_PUBLIC_BROWSER_ZYGOTE_HANDLE_LINUX_H_

#include "content/common/content_export.h"
#include "content/public/common/zygote_handle.h"

namespace content {

// Allocates and initializes a zygote process, and returns the
// ZygoteHandle used to communicate with it.
CONTENT_EXPORT ZygoteHandle CreateZygote();

// Returns a handle to a global generic zygote object. This function allows the
// browser to launch and use a single zygote process until the performance
// issues around launching multiple zygotes are resolved.
CONTENT_EXPORT ZygoteHandle* GetGenericZygote();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ZYGOTE_HANDLE_LINUX_H_
