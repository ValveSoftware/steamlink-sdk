// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CONTENT_CHILD_HELPERS_H_
#define CONTENT_CHILD_CONTENT_CHILD_HELPERS_H_

#include "base/basictypes.h"

namespace content {

// Returns an estimate of the memory usage of the renderer process. Different
// platforms implement this function differently, and count in different
// allocations. Results are not comparable across platforms. The estimate is
// computed inside the sandbox and thus its not always accurate.
size_t GetMemoryUsageKB();

}  // content

#endif  // CONTENT_CHILD_CONTENT_CHILD_HELPERS_H_
