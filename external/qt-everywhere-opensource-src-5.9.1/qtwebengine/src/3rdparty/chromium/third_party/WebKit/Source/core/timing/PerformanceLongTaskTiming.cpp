// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceLongTaskTiming.h"

#include "core/frame/DOMWindow.h"

namespace blink {

PerformanceLongTaskTiming::PerformanceLongTaskTiming(double startTime,
                                                     double endTime,
                                                     String name,
                                                     DOMWindow* culpritWindow)
    : PerformanceEntry(name, "longtask", startTime, endTime),
      m_culpritWindow(*culpritWindow) {}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming() {}

DOMWindow* PerformanceLongTaskTiming::culpritWindow() const {
  return m_culpritWindow.get();
}

DEFINE_TRACE(PerformanceLongTaskTiming) {
  visitor->trace(m_culpritWindow);
  PerformanceEntry::trace(visitor);
}

}  // namespace blink
