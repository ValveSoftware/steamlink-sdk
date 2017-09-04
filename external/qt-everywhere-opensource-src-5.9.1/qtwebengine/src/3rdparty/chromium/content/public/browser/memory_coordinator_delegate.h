// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEMORY_COORDINATOR_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_MEMORY_COORDINATOR_DELEGATE_H_

#include "content/common/content_export.h"

namespace content {

// A delegate class which is used by the memory coordinator.
class CONTENT_EXPORT MemoryCoordinatorDelegate {
 public:
  virtual ~MemoryCoordinatorDelegate() {}

  // Returns true when a given backgrounded renderer process can be suspended.
  virtual bool CanSuspendBackgroundedRenderer(int render_process_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEMORY_COORDINATOR_DELEGATE_H_
