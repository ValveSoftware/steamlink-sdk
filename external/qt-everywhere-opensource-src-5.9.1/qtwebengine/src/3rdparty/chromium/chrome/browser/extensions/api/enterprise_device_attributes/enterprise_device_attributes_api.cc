// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api.h"

#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_device_attributes.h"
#include "components/user_manager/user_manager.h"

namespace extensions {

namespace {

// Checks for the current browser context if the user is affiliated.
bool IsPermittedToGetDeviceId(content::BrowserContext* context) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(context));
  return user->IsAffiliated();
}

// Returns the directory device id for the permitted extensions or an empty
// string.
std::string GetDirectoryDeviceId(content::BrowserContext* context) {
  return IsPermittedToGetDeviceId(context)
             ? g_browser_process->platform_part()
                   ->browser_policy_connector_chromeos()
                   ->GetDirectoryApiID()
             : std::string();
}

}  //  namespace

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() {}

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    ~EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::Run() {
  const std::string device_id = GetDirectoryDeviceId(browser_context());
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDirectoryDeviceId::Results::Create(
          device_id)));
}

}  // namespace extensions
