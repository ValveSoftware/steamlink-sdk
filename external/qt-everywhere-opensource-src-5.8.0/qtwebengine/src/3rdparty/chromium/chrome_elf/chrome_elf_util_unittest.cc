// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using namespace install_static;

namespace {

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
  EXPECT_TRUE(IsSxSChrome(kCanaryExePath));
  EXPECT_FALSE(IsSxSChrome(kChromeUserExePath));
  EXPECT_FALSE(IsSxSChrome(kChromiumExePath));
}

TEST(ChromeElfUtilTest, SystemInstallTest) {
  EXPECT_TRUE(IsSystemInstall(kChromeSystemExePath));
  EXPECT_FALSE(IsSystemInstall(kChromeUserExePath));
}

TEST(ChromeElfUtilTest, BrowserProcessTest) {
  EXPECT_EQ(ProcessType::UNINITIALIZED, g_process_type);
  InitializeProcessType();
  EXPECT_FALSE(IsNonBrowserProcess());
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
  void SetUp() override {
    override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);
    override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
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

  void SetChannelName(const base::string16& channel_name) {
    LONG result = base::win::RegKey(
      system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
      BuildKey(kRegPathClientState, app_guid_).c_str(),
      KEY_SET_VALUE).WriteValue(kRegApField, channel_name.c_str());
    ASSERT_EQ(ERROR_SUCCESS, result);
  }

  // This function tests the install_static::GetChromeChannelName function and
  // is based on the ChannelInfoTest.Channels in channel_info_unittest.cc.
  // The |add_modifier| parameter controls whether we expect modifiers in the
  // returned channel name.
  void PerformChannelNameTests(bool add_modifier) {
    // We can't test the channel name correctly for canary mode because the
    // install_static checks whether an exe is a canary executable is based on
    // the path where the exe is running from.
    if (is_canary_)
      return;
    SetChannelName(L"");
    base::string16 channel;
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }

    SetChannelName(L"-full");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }

    SetChannelName(L"1.1-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"beta-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelBeta, channel);
    }

    SetChannelName(L"1.1-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"beta-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelBeta, channel);
    }

    SetChannelName(L"1.1-bar");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"beta-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelBeta, channel);
    }

    SetChannelName(L"1n1-foobar");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"beta-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelBeta, channel);
    }

    SetChannelName(L"foo-1.1-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }
    SetChannelName(L"2.0-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }

    SetChannelName(L"2.0-dev");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"dev-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelDev, channel);
    }
    SetChannelName(L"2.0-DEV");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"dev-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelDev, channel);
    }
    SetChannelName(L"2.0-dev-eloper");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"dev-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelDev, channel);
    }
    SetChannelName(L"2.0-doom");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"dev-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelDev, channel);
    }
    SetChannelName(L"250-doom");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"dev-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelDev, channel);
    }
    SetChannelName(L"bar-2.0-dev");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }
    SetChannelName(L"1.0-dev");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }

    SetChannelName(L"x64-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"beta-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelBeta, channel);
    }
    SetChannelName(L"bar-x64-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"beta-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelBeta, channel);
    }
    SetChannelName(L"x64-Beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"beta-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelBeta, channel);
    }

    SetChannelName(L"x64-stable");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }
    SetChannelName(L"baz-x64-stable");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }
    SetChannelName(L"x64-Stable");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }

    SetChannelName(L"fuzzy");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }
    SetChannelName(L"foo");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_EQ(L"-m", channel);
    } else {
      EXPECT_EQ(install_static::kChromeChannelStable, channel);
    }
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
  EXPECT_FALSE(GetCollectStatsConsentForTesting(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsZero) {
  SetUsageStat(0, false);
  EXPECT_FALSE(GetCollectStatsConsentForTesting(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsOne) {
  SetUsageStat(1, false);
  EXPECT_TRUE(GetCollectStatsConsentForTesting(chrome_path_));
  if (is_canary_) {
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeUserExePath));
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeSystemExePath));
  } else if (system_level_) {
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kCanaryExePath));
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeUserExePath));
  } else {
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kCanaryExePath));
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeSystemExePath));
  }
}

TEST_P(ChromeElfUtilTest, UsageStatsZeroInStateMedium) {
  if (!system_level_)
    return;
  SetUsageStat(0, true);
  EXPECT_FALSE(GetCollectStatsConsentForTesting(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsOneInStateMedium) {
  if (!system_level_)
    return;
  SetUsageStat(1, true);
  EXPECT_TRUE(GetCollectStatsConsentForTesting(chrome_path_));
  EXPECT_FALSE(GetCollectStatsConsentForTesting(kCanaryExePath));
  EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeUserExePath));
}

// TODO(ananta)
// Move this to install_static_unittests.
// http://crbug.com/604923
// This test tests the install_static::GetChromeChannelName function and is
// based on the ChannelInfoTest.Channels in channel_info_unittest.cc
TEST_P(ChromeElfUtilTest, InstallStaticGetChannelNameTest) {
  PerformChannelNameTests(true);  // add_modifier
  PerformChannelNameTests(false); // !add_modifier
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
