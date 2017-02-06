// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_client_win.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "components/browser_watcher/exit_code_watcher_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace browser_watcher {

namespace {

// Command line switches used to communiate to the child test.
const char kParentHandle[] = "parent-handle";
const char kLeakHandle[] = "leak-handle";
const char kNoLeakHandle[] = "no-leak-handle";

bool IsValidParentProcessHandle(base::CommandLine& cmd_line,
                                const char* switch_name) {
  std::string str_handle =
      cmd_line.GetSwitchValueASCII(switch_name);

  size_t integer_handle = 0;
  if (!base::StringToSizeT(str_handle, &integer_handle))
    return false;

  base::ProcessHandle handle =
      reinterpret_cast<base::ProcessHandle>(integer_handle);
  // Verify that we can get the associated process id.
  base::ProcessId parent_id = base::GetProcId(handle);
  if (parent_id == 0) {
    // Unable to get the parent pid - perhaps insufficient permissions.
    return false;
  }

  // Make sure the handle grants SYNCHRONIZE by waiting on it.
  DWORD err = ::WaitForSingleObject(handle, 0);
  if (err != WAIT_OBJECT_0 && err != WAIT_TIMEOUT) {
    // Unable to wait on the handle - perhaps insufficient permissions.
    return false;
  }

  return true;
}

std::string HandleToString(HANDLE handle) {
  // A HANDLE is a void* pointer, which is the same size as a size_t,
  // so we can use reinterpret_cast<> on it.
  size_t integer_handle = reinterpret_cast<size_t>(handle);
  return base::SizeTToString(integer_handle);
}

MULTIPROCESS_TEST_MAIN(VerifyParentHandle) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  // Make sure we got a valid parent process handle from the watcher client.
  if (!IsValidParentProcessHandle(*cmd_line, kParentHandle)) {
    LOG(ERROR) << "Invalid or missing parent-handle.";
    return 1;
  }

  // If in the legacy mode, we expect this second handle will leak into the
  // child process. This mainly serves to verify that the legacy mode is
  // getting tested.
  if (cmd_line->HasSwitch(kLeakHandle) &&
      !IsValidParentProcessHandle(*cmd_line, kLeakHandle)) {
    LOG(ERROR) << "Parent process handle unexpectedly didn't leak.";
    return 1;
  }

  // If not in the legacy mode, this second handle should not leak into the
  // child process.
  if (cmd_line->HasSwitch(kNoLeakHandle) &&
      IsValidParentProcessHandle(*cmd_line, kLeakHandle)) {
    LOG(ERROR) << "Parent process handle unexpectedly leaked.";
    return 1;
  }

  return 0;
}

class WatcherClientTest : public base::MultiProcessTest {
 public:
  void SetUp() override {
    // Open an inheritable handle on our own process to test handle leakage.
    self_.Set(::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                            TRUE,  // Ineritable handle.
                            base::GetCurrentProcId()));

    ASSERT_TRUE(self_.IsValid());
  }

  enum HandlePolicy {
    LEAK_HANDLE,
    NO_LEAK_HANDLE
  };

  // Get a base command line to launch back into this test fixture.
  base::CommandLine GetBaseCommandLine(HandlePolicy handle_policy,
                                       HANDLE parent_handle) {
    base::CommandLine ret = base::GetMultiProcessTestChildBaseCommandLine();

    ret.AppendSwitchASCII(switches::kTestChildProcess, "VerifyParentHandle");
    ret.AppendSwitchASCII(kParentHandle, HandleToString(parent_handle));

    switch (handle_policy) {
      case LEAK_HANDLE:
        ret.AppendSwitchASCII(kLeakHandle, HandleToString(self_.Get()));
        break;

      case NO_LEAK_HANDLE:
        ret.AppendSwitchASCII(kNoLeakHandle, HandleToString(self_.Get()));
        break;

      default:
        ADD_FAILURE() << "Impossible handle_policy";
    }

    return ret;
  }

  WatcherClient::CommandLineGenerator GetBaseCommandLineGenerator(
      HandlePolicy handle_policy) {
    return base::Bind(&WatcherClientTest::GetBaseCommandLine,
                      base::Unretained(this), handle_policy);
  }

  void AssertSuccessfulExitCode(base::Process process) {
    ASSERT_TRUE(process.IsValid());
    int exit_code = 0;
    if (!process.WaitForExit(&exit_code))
      FAIL() << "Process::WaitForExit failed.";
    ASSERT_EQ(0, exit_code);
  }

  // Inheritable process handle used for testing.
  base::win::ScopedHandle self_;
};

}  // namespace

// TODO(siggi): More testing - test WatcherClient base implementation.

TEST_F(WatcherClientTest, LaunchWatcherSucceeds) {
  // We can only use the non-legacy launch method on Windows Vista or better.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;

  WatcherClient client(GetBaseCommandLineGenerator(NO_LEAK_HANDLE));
  ASSERT_FALSE(client.use_legacy_launch());

  client.LaunchWatcher();

  ASSERT_NO_FATAL_FAILURE(
      AssertSuccessfulExitCode(client.process().Duplicate()));
}

TEST_F(WatcherClientTest, LaunchWatcherLegacyModeSucceeds) {
  // Test the XP-compatible legacy launch mode. This is expected to leak
  // a handle to the child process.
  WatcherClient client(GetBaseCommandLineGenerator(LEAK_HANDLE));

  // Use the legacy launch mode.
  client.set_use_legacy_launch(true);

  client.LaunchWatcher();

  ASSERT_NO_FATAL_FAILURE(
      AssertSuccessfulExitCode(client.process().Duplicate()));
}

TEST_F(WatcherClientTest, LegacyModeDetectedOnXP) {
  // This test only works on Windows XP.
  if (base::win::GetVersion() > base::win::VERSION_XP)
    return;

  // Test that the client detects the need to use legacy launch mode, and that
  // it works on Windows XP.
  WatcherClient client(GetBaseCommandLineGenerator(LEAK_HANDLE));
  ASSERT_TRUE(client.use_legacy_launch());

  client.LaunchWatcher();

  ASSERT_NO_FATAL_FAILURE(
      AssertSuccessfulExitCode(client.process().Duplicate()));
}

}  // namespace browser_watcher
