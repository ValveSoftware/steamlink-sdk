// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorReading.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"

namespace blink {

SensorReading::SensorReading(const device::SensorReading& data)
    : m_data(data) {}

SensorReading::~SensorReading() = default;

DOMHighResTimeStamp SensorReading::timeStamp(ScriptState* scriptState) const {
  LocalDOMWindow* window = scriptState->domWindow();
  if (!window)
    return 0.0;

  Performance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  return performance->monotonicTimeToDOMHighResTimeStamp(data().timestamp);
}

}  // namespace blink
