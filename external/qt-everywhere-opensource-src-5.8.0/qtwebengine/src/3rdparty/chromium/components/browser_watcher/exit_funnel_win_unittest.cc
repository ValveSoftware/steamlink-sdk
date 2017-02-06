// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/exit_funnel_win.h"

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/command_line.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

const wchar_t kRegistryPath[] = L"Software\\ExitFunnelWinTest";

class ExitFunnelWinTest : public testing::Test {
 public:
  typedef testing::Test Super;
  typedef std::map<base::string16, int64_t> EventMap;

  void SetUp() override {
    Super::SetUp();

    override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
  }

  base::string16 GetEventSubkey() {
    // There should be a single subkey named after this process' pid.
    base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, kRegistryPath);
    EXPECT_EQ(1u, it.SubkeyCount());

    unsigned pid = 0;
    base::StringToUint(it.Name(), &pid);
    EXPECT_EQ(base::GetCurrentProcId(), pid);

    return it.Name();
  }

  // TODO(siggi): Reuse the reading code from the metrics provider.
  EventMap ReadEventsFromSubkey(const base::string16& subkey_name) {
    base::string16 key_name(
        base::StringPrintf(L"%ls\\%ls", kRegistryPath, subkey_name.c_str()));

    base::win::RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ);
    EXPECT_TRUE(key.Valid());
    EXPECT_EQ(2u, key.GetValueCount());

    EventMap events;
    for (size_t i = 0; i < key.GetValueCount(); ++i) {
      base::string16 name;
      EXPECT_EQ(key.GetValueNameAt(i, &name), ERROR_SUCCESS);
      int64_t value = 0;
      EXPECT_EQ(key.ReadInt64(name.c_str(), &value), ERROR_SUCCESS);

      events[name] = value;
    }

    return events;
  }

  EventMap ReadEvents() {
    base::string16 name = GetEventSubkey();
    return ReadEventsFromSubkey(name);
  }

 protected:
  registry_util::RegistryOverrideManager override_manager_;
};

}  // namespace

TEST_F(ExitFunnelWinTest, RecordSingleEvent) {
  // Record a couple of events.
  ASSERT_TRUE(ExitFunnel::RecordSingleEvent(kRegistryPath, L"One"));
  ASSERT_TRUE(ExitFunnel::RecordSingleEvent(kRegistryPath, L"Two"));

  EventMap events = ReadEvents();

  ASSERT_EQ(2u, events.size());
  ASSERT_TRUE(events.find(L"One") != events.end());
  ASSERT_TRUE(events.find(L"Two") != events.end());
}

TEST_F(ExitFunnelWinTest, RecordsEventTimes) {
  ExitFunnel funnel;

  ASSERT_TRUE(funnel.Init(kRegistryPath, base::GetCurrentProcessHandle()));
  ASSERT_TRUE(funnel.RecordEvent(L"One"));

  // Sleep for half a second to space the events in time.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));

  ASSERT_TRUE(funnel.RecordEvent(L"Two"));

  EventMap events = ReadEvents();
  ASSERT_EQ(2u, events.size());

  base::TimeDelta one = base::TimeDelta::FromInternalValue(events[L"One"]);
  base::TimeDelta two = base::TimeDelta::FromInternalValue(events[L"Two"]);

  // Sleep is not accurate, it may over or under sleep. To minimize flakes,
  // this test only compares relative ordering of the events.
  ASSERT_LT(one, two);
}

}  // namespace browser_watcher
