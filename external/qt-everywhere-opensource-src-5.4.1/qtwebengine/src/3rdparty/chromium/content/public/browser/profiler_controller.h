// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROFILER_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_PROFILER_CONTROLLER_H_

#include <set>
#include <string>

#include "base/tracked_objects.h"
#include "content/common/content_export.h"


namespace base {
class DictionaryValue;
}

namespace content {

class ProfilerSubscriber;

// ProfilerController is used on the browser process to collect profiler data.
// Only the browser UI thread is allowed to interact with the ProfilerController
// object.
class CONTENT_EXPORT ProfilerController {
 public:
  // Returns the ProfilerController object for the current process, or NULL if
  // none.
  static ProfilerController* GetInstance();

  virtual ~ProfilerController() {}

  // Register the subscriber so that it will be called when for example
  // OnProfilerDataCollected is returning profiler data from a child process.
  // This is called on UI thread.
  virtual void Register(ProfilerSubscriber* subscriber) = 0;

  // Unregister the subscriber so that it will not be called when for example
  // OnProfilerDataCollected is returning profiler data from a child process.
  // Safe to call even if caller is not the current subscriber.
  virtual void Unregister(const ProfilerSubscriber* subscriber) = 0;

  // Contact all processes and get their profiler data.
  virtual void GetProfilerData(int sequence_number) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROFILER_CONTROLLER_H_
