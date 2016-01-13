// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/sys_utils.h"

#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(SysUtils, AmountOfPhysicalMemory) {
  // Check that the RAM size reported by sysconf() matches the one
  // computed by base::android::SysUtils::AmountOfPhysicalMemory().
  size_t sys_ram_size =
      static_cast<size_t>(sysconf(_SC_PHYS_PAGES) * PAGE_SIZE);
  EXPECT_EQ(sys_ram_size, SysUtils::AmountOfPhysicalMemoryKB() * 1024UL);
}

}  // namespace android
}  // namespace base
