// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_H_
#define COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_H_

#if defined(OS_LINUX)
#include "components/memory_pressure/direct_memory_pressure_calculator_linux.h"
#elsif defined(OS_WIN)
#include "components/memory_pressure/direct_memory_pressure_calculator_win.h"
#endif  // OS_LINUX

#endif  // COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_H_
