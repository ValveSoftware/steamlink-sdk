// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/chrome_elf_util.h"

#include <tuple>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const wchar_t kRegPathClientState[] = L"Software\\Google\\Update\\ClientState";
const wchar_t kRegPathClientStateMedium[] =
    L"Software\\Google\\Update\\ClientStateMedium";
const wchar_t kRegValueUsageStats[] = L"usagestats";
const wchar_t kUninstallArgumentsField[] = L"UninstallArguments";

const wchar_t kAppGuidCanary[] =
    L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";
const wchar_t kAppGuidGoogleChrome[] =
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kAppGuidGoogleBinaries[] =
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

const wchar_t kCanaryExePath[] =
    L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome SxS\\Application"
    L"\\chrome.exe";
const wchar_t kChromeSystemExePath[] =
    L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe";
const wchar_t kChromeUserExePath[] =
    L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome\\Application\\chrome.exe";
const wchar_t kChromiumExePath[] =
    L"C:\\Users\\user\\AppData\\Local\\Chromium\\Application\\chrome.exe";


TEST(ChromeElfUtilTest, CanaryTest) {
  EXPECT_TRUE(IsCanary(kCanaryExePath));
  EXPECT_FALSE(IsCanary(kChromeUserExePath));
  EXPECT_FALSE(IsCanary(kChromiumExePath));
}

TEST(ChromeElfUtilTest, SystemInstallTest) {
  EXPECT_TRUE(IsSystemInstall(kChromeSystemExePath));
  EXPECT_FALSE(IsSystemInstall(kChromeUserExePath));
}

// Parameterized test with paramters:
// 1: product: "canary" or "google"
// 2: install level: "user" or "system"
// 3: install mode: "single" or "multi"
class ChromeElfUtilTest :
    public testing::TestWithParam<std::tuple<const char*,
                                             const char*,
                                             const char*> > {
 protected:
  virtual void SetUp() OVERRIDE {
    override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE,
                                      L"chrome_elf_test_local");
    override_manager_.OverrideRegistry(HKEY_CURRENT_USER,
                                      L"chrome_elf_test_current");
    const char* app;
    const char* level;
    const char* mode;
    std::tie(app, level, mode) = GetParam();
    is_canary_ = (std::string(app) == "canary");
    system_level_ = (std::string(level) != "user");
    multi_install_ = (std::string(mode) != "single");
    if (is_canary_) {
      ASSERT_FALSE(system_level_);
      ASSERT_FALSE(multi_install_);
      app_guid_ = kAppGuidCanary;
      chrome_path_ = kCanaryExePath;
    } else {
      app_guid_ = kAppGuidGoogleChrome;
      chrome_path_ = (system_level_ ? kChromeSystemExePath :
                                      kChromeUserExePath);
    }
    if (multi_install_) {
      SetMultiInstallStateInRegistry(system_level_, true);
      app_guid_ = kAppGuidGoogleBinaries;
    }
  }

  base::string16 BuildKey(const wchar_t* path, const wchar_t* guid) {
    base::string16 full_key_path(path);
    full_key_path.append(1, L'\\');
    full_key_path.append(guid);
    return full_key_path;
  }

  void SetUsageStat(DWORD value, bool state_medium) {
    LONG result = base::win::RegKey(
        system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
        BuildKey(state_medium ? kRegPathClientStateMedium : kRegPathClientState,
                 app_guid_).c_str(),
        KEY_SET_VALUE).WriteValue(kRegValueUsageStats, value);
    ASSERT_EQ(ERROR_SUCCESS, result);
  }

  void SetMultiInstallStateInRegistry(bool system_install, bool multi) {
    base::win::RegKey key(
        system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
        BuildKey(kRegPathClientState, kAppGuidGoogleChrome).c_str(),
        KEY_SET_VALUE);
    LONG result;
    if (multi) {
      result = key.WriteValue(kUninstallArgumentsField,
                              L"yadda yadda --multi-install yadda yadda");
    } else {
      result = key.DeleteValue(kUninstallArgumentsField);
    }
    ASSERT_EQ(ERROR_SUCCESS, result);
  }

  const wchar_t* app_guid_;
  const wchar_t* chrome_path_;
  bool system_level_;
  bool multi_install_;
  bool is_canary_;
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_P(ChromeElfUtilTest, MultiInstallTest) {
  if (is_canary_)
    return;
  SetMultiInstallStateInRegistry(system_level_, true);
  EXPECT_TRUE(IsMultiInstall(system_level_));

  SetMultiInstallStateInRegistry(system_level_, false);
  EXPECT_FALSE(IsMultiInstall(system_level_));
}

TEST_P(ChromeElfUtilTest, UsageStatsAbsent) {
  EXPECT_FALSE(AreUsageStatsEnabled(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsZero) {
  SetUsageStat(0, false);
  EXPECT_FALSE(AreUsageStatsEnabled(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsOne) {
  SetUsageStat(1, false);
  EXPECT_TRUE(AreUsageStatsEnabled(chrome_path_));
  if (is_canary_) {
    EXPECT_FALSE(AreUsageStatsEnabled(kChromeUserExePath));
    EXPECT_FALSE(AreUsageStatsEnabled(kChromeSystemExePath));
  } else if (system_level_) {
    EXPECT_FALSE(AreUsageStatsEnabled(kCanaryExePath));
    EXPECT_FALSE(AreUsageStatsEnabled(kChromeUserExePath));
  } else {
    EXPECT_FALSE(AreUsageStatsEnabled(kCanaryExePath));
    EXPECT_FALSE(AreUsageStatsEnabled(kChromeSystemExePath));
  }
}

TEST_P(ChromeElfUtilTest, UsageStatsZeroInStateMedium) {
  if (!system_level_)
    return;
  SetUsageStat(0, true);
  EXPECT_FALSE(AreUsageStatsEnabled(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsOneInStateMedium) {
  if (!system_level_)
    return;
  SetUsageStat(1, true);
  EXPECT_TRUE(AreUsageStatsEnabled(chrome_path_));
  EXPECT_FALSE(AreUsageStatsEnabled(kCanaryExePath));
  EXPECT_FALSE(AreUsageStatsEnabled(kChromeUserExePath));
}

INSTANTIATE_TEST_CASE_P(Canary, ChromeElfUtilTest,
                        testing::Combine(testing::Values("canary"),
                                         testing::Values("user"),
                                         testing::Values("single")));
INSTANTIATE_TEST_CASE_P(GoogleChrome, ChromeElfUtilTest,
                        testing::Combine(testing::Values("google"),
                                         testing::Values("user", "system"),
                                         testing::Values("single", "multi")));

}  // namespace
