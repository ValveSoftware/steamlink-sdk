// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

TEST(SandboxNtUtil, IsSameProcessPseudoHandle) {
  InitGlobalNt();

  HANDLE current_process_pseudo = GetCurrentProcess();
  EXPECT_TRUE(IsSameProcess(current_process_pseudo));
}

TEST(SandboxNtUtil, IsSameProcessNonPseudoHandle) {
  InitGlobalNt();

  base::win::ScopedHandle current_process(
      OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId()));
  ASSERT_TRUE(current_process.IsValid());
  EXPECT_TRUE(IsSameProcess(current_process.Get()));
}

TEST(SandboxNtUtil, IsSameProcessDifferentProcess) {
  InitGlobalNt();

  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {};
  wchar_t notepad[] = L"notepad";
  ASSERT_TRUE(CreateProcessW(nullptr, notepad, nullptr, nullptr, FALSE, 0,
                             nullptr, nullptr, &si, &pi));
  base::win::ScopedProcessInformation process_info(pi);

  EXPECT_FALSE(IsSameProcess(process_info.process_handle()));
  EXPECT_TRUE(TerminateProcess(process_info.process_handle(), 0));
}

}  // namespace
}  // namespace sandbox
