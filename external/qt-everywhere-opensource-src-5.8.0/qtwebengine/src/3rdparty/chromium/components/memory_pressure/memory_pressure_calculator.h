// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_CALCULATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_CALCULATOR_H_

#include "base/macros.h"
#include "components/memory_pressure/memory_pressure_listener.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

// Interface for a utility class that calculates memory pressure. Only used on
// platforms without native memory pressure signals.
class MemoryPressureCalculator {
 public:
  using MemoryPressureLevel = MemoryPressureListener::MemoryPressureLevel;

  MemoryPressureCalculator() {}
  virtual ~MemoryPressureCalculator() {}
  virtual MemoryPressureLevel CalculateCurrentPressureLevel() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryPressureCalculator);
};

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_CALCULATOR_H_
