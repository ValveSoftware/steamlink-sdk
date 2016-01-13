// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_PROFILER_POWER_PROFILER_OBSERVER_H_
#define CONTENT_BROWSER_POWER_PROFILER_POWER_PROFILER_OBSERVER_H_

#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

struct PowerEvent;
typedef std::vector<PowerEvent> PowerEventVector;

// A class used to monitor power usage data.
class CONTENT_EXPORT PowerProfilerObserver {
 public:
  PowerProfilerObserver() {}
  virtual ~PowerProfilerObserver() {}

  // This method will be called on the UI thread.
  virtual void OnPowerEvent(const PowerEventVector&) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerProfilerObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_PROFILER_POWER_PROFILER_OBSERVER_H_
