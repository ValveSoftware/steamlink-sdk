// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autotest_private.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#endif

namespace extensions {
namespace {

base::ListValue* GetHostPermissions(const Extension* ext, bool effective_perm) {
  const PermissionsData* permissions_data = ext->permissions_data();
  const URLPatternSet& pattern_set =
      effective_perm ? permissions_data->GetEffectiveHostPermissions()
                     : permissions_data->active_permissions().explicit_hosts();

  base::ListValue* permissions = new base::ListValue;
  for (URLPatternSet::const_iterator perm = pattern_set.begin();
       perm != pattern_set.end();
       ++perm) {
    permissions->AppendString(perm->GetAsString());
  }

  return permissions;
}

base::ListValue* GetAPIPermissions(const Extension* ext) {
  base::ListValue* permissions = new base::ListValue;
  std::set<std::string> perm_list =
      ext->permissions_data()->active_permissions().GetAPIsAsStrings();
  for (std::set<std::string>::const_iterator perm = perm_list.begin();
       perm != perm_list.end(); ++perm) {
    permissions->AppendString(perm->c_str());
  }
  return permissions;
}

bool IsTestMode(Profile* profile) {
  return AutotestPrivateAPI::GetFactoryInstance()->Get(profile)->test_mode();
}

}  // namespace

bool AutotestPrivateLogoutFunction::RunSync() {
  DVLOG(1) << "AutotestPrivateLogoutFunction";
  if (!IsTestMode(GetProfile()))
    chrome::AttemptUserExit();
  return true;
}

bool AutotestPrivateRestartFunction::RunSync() {
  DVLOG(1) << "AutotestPrivateRestartFunction";
  if (!IsTestMode(GetProfile()))
    chrome::AttemptRestart();
  return true;
}

bool AutotestPrivateShutdownFunction::RunSync() {
  std::unique_ptr<api::autotest_private::Shutdown::Params> params(
      api::autotest_private::Shutdown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateShutdownFunction " << params->force;

  if (!IsTestMode(GetProfile()))
    chrome::AttemptExit();
  return true;
}

bool AutotestPrivateLoginStatusFunction::RunSync() {
  DVLOG(1) << "AutotestPrivateLoginStatusFunction";

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
#if defined(OS_CHROMEOS)
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const bool is_screen_locked =
      !!chromeos::ScreenLocker::default_screen_locker();

  if (user_manager) {
    result->SetBoolean("isLoggedIn", user_manager->IsUserLoggedIn());
    result->SetBoolean("isOwner", user_manager->IsCurrentUserOwner());
    result->SetBoolean("isScreenLocked", is_screen_locked);
    if (user_manager->IsUserLoggedIn()) {
      result->SetBoolean("isRegularUser",
                         user_manager->IsLoggedInAsUserWithGaiaAccount());
      result->SetBoolean("isGuest", user_manager->IsLoggedInAsGuest());
      result->SetBoolean("isKiosk", user_manager->IsLoggedInAsKioskApp());

      const user_manager::User* user = user_manager->GetLoggedInUser();
      result->SetString("email", user->email());
      result->SetString("displayEmail", user->display_email());

      std::string user_image;
      switch (user->image_index()) {
        case user_manager::User::USER_IMAGE_EXTERNAL:
          user_image = "file";
          break;

        case user_manager::User::USER_IMAGE_PROFILE:
          user_image = "profile";
          break;

        default:
          user_image = base::IntToString(user->image_index());
          break;
      }
      result->SetString("userImage", user_image);
    }
  }
#endif

  SetResult(std::move(result));
  return true;
}

bool AutotestPrivateLockScreenFunction::RunSync() {
  DVLOG(1) << "AutotestPrivateLockScreenFunction";
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      RequestLockScreen();
#endif
  return true;
}

bool AutotestPrivateGetExtensionsInfoFunction::RunSync() {
  DVLOG(1) << "AutotestPrivateGetExtensionsInfoFunction";

  ExtensionService* service =
      ExtensionSystem::Get(GetProfile())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(GetProfile());
  const ExtensionSet& extensions = registry->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry->disabled_extensions();
  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(GetProfile());

  base::ListValue* extensions_values = new base::ListValue;
  ExtensionList all;
  all.insert(all.end(), extensions.begin(), extensions.end());
  all.insert(all.end(), disabled_extensions.begin(), disabled_extensions.end());
  for (ExtensionList::const_iterator it = all.begin();
       it != all.end(); ++it) {
    const Extension* extension = it->get();
    std::string id = extension->id();
    std::unique_ptr<base::DictionaryValue> extension_value(
        new base::DictionaryValue);
    extension_value->SetString("id", id);
    extension_value->SetString("version", extension->VersionString());
    extension_value->SetString("name", extension->name());
    extension_value->SetString("publicKey", extension->public_key());
    extension_value->SetString("description", extension->description());
    extension_value->SetString(
        "backgroundUrl", BackgroundInfo::GetBackgroundURL(extension).spec());
    extension_value->SetString(
        "optionsUrl", OptionsPageInfo::GetOptionsPage(extension).spec());

    extension_value->Set("hostPermissions",
                         GetHostPermissions(extension, false));
    extension_value->Set("effectiveHostPermissions",
                         GetHostPermissions(extension, true));
    extension_value->Set("apiPermissions", GetAPIPermissions(extension));

    Manifest::Location location = extension->location();
    extension_value->SetBoolean("isComponent",
                                location == Manifest::COMPONENT);
    extension_value->SetBoolean("isInternal",
                                location == Manifest::INTERNAL);
    extension_value->SetBoolean("isUserInstalled",
        location == Manifest::INTERNAL ||
        Manifest::IsUnpackedLocation(location));
    extension_value->SetBoolean("isEnabled", service->IsExtensionEnabled(id));
    extension_value->SetBoolean("allowedInIncognito",
        util::IsIncognitoEnabled(id, GetProfile()));
    extension_value->SetBoolean(
        "hasPageAction",
        extension_action_manager->GetPageAction(*extension) != NULL);

    extensions_values->Append(std::move(extension_value));
  }

  std::unique_ptr<base::DictionaryValue> return_value(
      new base::DictionaryValue);
  return_value->Set("extensions", extensions_values);
  SetResult(std::move(return_value));
  return true;
}

static int AccessArray(const volatile int arr[], const volatile int *index) {
  return arr[*index];
}

bool AutotestPrivateSimulateAsanMemoryBugFunction::RunSync() {
  DVLOG(1) << "AutotestPrivateSimulateAsanMemoryBugFunction";
  if (!IsTestMode(GetProfile())) {
    // This array is volatile not to let compiler optimize us out.
    volatile int testarray[3] = {0, 0, 0};

    // Cause Address Sanitizer to abort this process.
    volatile int index = 5;
    AccessArray(testarray, &index);
  }
  return true;
}

bool AutotestPrivateSetTouchpadSensitivityFunction::RunSync() {
  std::unique_ptr<api::autotest_private::SetTouchpadSensitivity::Params> params(
      api::autotest_private::SetTouchpadSensitivity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetTouchpadSensitivityFunction " << params->value;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetTouchpadSensitivity(
      params->value);
#endif
  return true;
}

bool AutotestPrivateSetTapToClickFunction::RunSync() {
  std::unique_ptr<api::autotest_private::SetTapToClick::Params> params(
      api::autotest_private::SetTapToClick::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetTapToClickFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetTapToClick(params->enabled);
#endif
  return true;
}

bool AutotestPrivateSetThreeFingerClickFunction::RunSync() {
  std::unique_ptr<api::autotest_private::SetThreeFingerClick::Params> params(
      api::autotest_private::SetThreeFingerClick::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetThreeFingerClickFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetThreeFingerClick(
      params->enabled);
#endif
  return true;
}

bool AutotestPrivateSetTapDraggingFunction::RunSync() {
  std::unique_ptr<api::autotest_private::SetTapDragging::Params> params(
      api::autotest_private::SetTapDragging::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetTapDraggingFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetTapDragging(params->enabled);
#endif
  return true;
}

bool AutotestPrivateSetNaturalScrollFunction::RunSync() {
  std::unique_ptr<api::autotest_private::SetNaturalScroll::Params> params(
      api::autotest_private::SetNaturalScroll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetNaturalScrollFunction " << params->enabled;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetNaturalScroll(
      params->enabled);
#endif
  return true;
}

bool AutotestPrivateSetMouseSensitivityFunction::RunSync() {
  std::unique_ptr<api::autotest_private::SetMouseSensitivity::Params> params(
      api::autotest_private::SetMouseSensitivity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetMouseSensitivityFunction " << params->value;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetMouseSensitivity(
      params->value);
#endif
  return true;
}

bool AutotestPrivateSetPrimaryButtonRightFunction::RunSync() {
  std::unique_ptr<api::autotest_private::SetPrimaryButtonRight::Params> params(
      api::autotest_private::SetPrimaryButtonRight::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateSetPrimaryButtonRightFunction " << params->right;

#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->SetPrimaryButtonRight(
      params->right);
#endif
  return true;
}

// static
std::string AutotestPrivateGetVisibleNotificationsFunction::ConvertToString(
    message_center::NotificationType type) {
#if defined(OS_CHROMEOS)
  switch (type) {
    case message_center::NOTIFICATION_TYPE_SIMPLE:
      return "simple";
    case message_center::NOTIFICATION_TYPE_BASE_FORMAT:
      return "base_format";
    case message_center::NOTIFICATION_TYPE_IMAGE:
      return "image";
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
      return "multiple";
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      return "progress";
    case message_center::NOTIFICATION_TYPE_CUSTOM:
      return "custom";
  }
#endif
  return "unknown";
}

bool AutotestPrivateGetVisibleNotificationsFunction::RunSync() {
  DVLOG(1) << "AutotestPrivateGetVisibleNotificationsFunction";
  std::unique_ptr<base::ListValue> values(new base::ListValue);
#if defined(OS_CHROMEOS)
  for (auto* notification :
       message_center::MessageCenter::Get()->GetVisibleNotifications()) {
    base::DictionaryValue* result(new base::DictionaryValue);
    result->SetString("id", notification->id());
    result->SetString("type", ConvertToString(notification->type()));
    result->SetString("title", notification->title());
    result->SetString("message", notification->message());
    result->SetInteger("priority", notification->priority());
    result->SetInteger("progress", notification->progress());
    values->Append(result);
  }

#endif
  SetResult(std::move(values));
  return true;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<AutotestPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>*
AutotestPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AutotestPrivateAPI();
}

AutotestPrivateAPI::AutotestPrivateAPI() : test_mode_(false) {
}

AutotestPrivateAPI::~AutotestPrivateAPI() {
}

}  // namespace extensions
