// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_UTILITY_UTILITY_THREAD_H_
#define CONTENT_PUBLIC_UTILITY_UTILITY_THREAD_H_

#include "content/public/child/child_thread.h"

namespace content {

class CONTENT_EXPORT UtilityThread : virtual public ChildThread {
 public:
  // Returns the one utility thread for this process.  Note that this can only
  // be accessed when running on the utility thread itself.
  static UtilityThread* Get();

  UtilityThread();
  ~UtilityThread() override;

  // Releases the process if we are not (or no longer) in batch mode.
  virtual void ReleaseProcessIfNeeded() = 0;

  // Initializes blink if it hasn't already been initialized.
  virtual void EnsureBlinkInitialized() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_UTILITY_UTILITY_THREAD_H_
