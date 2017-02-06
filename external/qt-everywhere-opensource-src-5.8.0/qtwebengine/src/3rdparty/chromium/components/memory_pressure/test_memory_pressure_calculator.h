// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_TEST_MEMORY_PRESSURE_CALCULATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_TEST_MEMORY_PRESSURE_CALCULATOR_H_

#include "base/macros.h"
#include "components/memory_pressure/memory_pressure_calculator.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

// A mock memory pressure calculator for unittesting.
class TestMemoryPressureCalculator : public MemoryPressureCalculator {
 public:
  // Defaults to no pressure.
  TestMemoryPressureCalculator();
  explicit TestMemoryPressureCalculator(MemoryPressureLevel level);
  ~TestMemoryPressureCalculator() override {}

  // MemoryPressureCalculator implementation.
  MemoryPressureLevel CalculateCurrentPressureLevel() override;

  // Sets the mock calculator to return the given pressure level.
  void SetLevel(MemoryPressureLevel level);

  // Sets the mock calculator to return no pressure.
  void SetNone();

  // Sets the mock calculator to return moderate pressure.
  void SetModerate();

  // Sets the mock calculator to return critical pressure.
  void SetCritical();

  // Resets the call counter to 'CalculateCurrentPressureLevel'.
  void ResetCalls();

  // Returns the number of calls to 'CalculateCurrentPressureLevel'.
  int calls() const { return calls_; }

 private:
  MemoryPressureLevel level_;
  int calls_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryPressureCalculator);
};

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_TEST_MEMORY_PRESSURE_CALCULATOR_H_
