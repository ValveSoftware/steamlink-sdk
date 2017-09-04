// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/test_memory_pressure_calculator.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

TestMemoryPressureCalculator::TestMemoryPressureCalculator()
    : level_(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE), calls_(0) {}

TestMemoryPressureCalculator::TestMemoryPressureCalculator(
    MemoryPressureLevel level)
    : level_(level), calls_(0) {}

TestMemoryPressureCalculator::MemoryPressureLevel
TestMemoryPressureCalculator::CalculateCurrentPressureLevel() {
  ++calls_;
  return level_;
}

void TestMemoryPressureCalculator::SetLevel(MemoryPressureLevel level) {
  level_ = level;
}

void TestMemoryPressureCalculator::SetNone() {
  level_ = MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

void TestMemoryPressureCalculator::SetModerate() {
  level_ = MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
}

void TestMemoryPressureCalculator::SetCritical() {
  level_ = MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
}

void TestMemoryPressureCalculator::ResetCalls() {
  calls_ = 0;
}

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure
