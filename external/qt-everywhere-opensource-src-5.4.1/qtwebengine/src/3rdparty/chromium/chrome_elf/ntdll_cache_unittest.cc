// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome_elf/ntdll_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NTDLLCacheTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    InitCache();
  }

};

TEST_F(NTDLLCacheTest, NtDLLCacheSanityCheck) {
  HMODULE ntdll_handle = ::GetModuleHandle(L"ntdll.dll");
  // Grab a couple random entries from the cache and make sure they match the
  // addresses exported by ntdll.
  EXPECT_EQ(::GetProcAddress(ntdll_handle, "A_SHAFinal"),
            g_ntdll_lookup["A_SHAFinal"]);
  EXPECT_EQ(::GetProcAddress(ntdll_handle, "ZwTraceControl"),
            g_ntdll_lookup["ZwTraceControl"]);
}

}  // namespace
