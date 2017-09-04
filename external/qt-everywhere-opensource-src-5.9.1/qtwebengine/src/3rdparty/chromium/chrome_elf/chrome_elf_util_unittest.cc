// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <windows.h>
#include <versionhelpers.h>  // windows.h must be before.

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/chrome_elf_security.h"
#include "chrome_elf/nt_registry/nt_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool SetSecurityFinchFlag(bool creation) {
  bool success = true;
  base::win::RegKey security_key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);

  if (creation) {
    if (ERROR_SUCCESS !=
        security_key.CreateKey(elf_sec::kRegSecurityFinchPath, KEY_QUERY_VALUE))
      success = false;
  } else {
    if (ERROR_SUCCESS != security_key.DeleteKey(elf_sec::kRegSecurityFinchPath))
      success = false;
  }

  security_key.Close();
  return success;
}

bool IsSecuritySet() {
  typedef decltype(GetProcessMitigationPolicy)* GetProcessMitigationPolicyFunc;

  // Check the settings from EarlyBrowserSecurity().
  if (::IsWindows8OrGreater()) {
    GetProcessMitigationPolicyFunc get_process_mitigation_policy =
        reinterpret_cast<GetProcessMitigationPolicyFunc>(::GetProcAddress(
            ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
    if (!get_process_mitigation_policy)
      return false;

    // Check that extension points are disabled.
    // (Legacy hooking.)
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
    if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                       ProcessExtensionPointDisablePolicy,
                                       &policy, sizeof(policy)))
      return false;

    return policy.DisableExtensionPoints;
  }

  return true;
}

void RegRedirect(nt::ROOT_KEY key,
                 registry_util::RegistryOverrideManager* rom) {
  ASSERT_NE(key, nt::AUTO);
  base::string16 temp;

  if (key == nt::HKCU) {
    rom->OverrideRegistry(HKEY_CURRENT_USER, &temp);
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, temp));
  } else {
    rom->OverrideRegistry(HKEY_LOCAL_MACHINE, &temp);
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKLM, temp));
  }
}

void CancelRegRedirect(nt::ROOT_KEY key) {
  ASSERT_NE(key, nt::AUTO);
  if (key == nt::HKCU)
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, base::string16()));
  else
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKLM, base::string16()));
}

TEST(ChromeElfUtilTest, BrowserProcessSecurityTest) {
  if (!::IsWindows8OrGreater())
    return;

  // Set up registry override for this test.
  registry_util::RegistryOverrideManager override_manager;
  RegRedirect(nt::HKCU, &override_manager);

  // First, ensure that the emergency-off finch signal works.
  EXPECT_TRUE(SetSecurityFinchFlag(true));
  elf_security::EarlyBrowserSecurity();
  EXPECT_FALSE(IsSecuritySet());
  EXPECT_TRUE(SetSecurityFinchFlag(false));

  // Second, test that the process mitigation is set when no finch signal.
  elf_security::EarlyBrowserSecurity();
  EXPECT_TRUE(IsSecuritySet());

  CancelRegRedirect(nt::HKCU);
}

}  // namespace
