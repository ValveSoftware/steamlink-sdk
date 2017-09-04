// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/prefs_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/drive/drive_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/url_formatter/url_fixer.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
bool IsPrivilegedCrosSetting(const std::string& pref_name) {
  if (!chromeos::CrosSettings::IsCrosSettings(pref_name))
    return false;
  // kSystemTimezone should be changeable by all users.
  if (pref_name == chromeos::kSystemTimezone)
    return false;
  // All other Cros settings are considered privileged and are either policy
  // controlled or owner controlled.
  return true;
}
#endif

}  // namespace

namespace extensions {

namespace settings_private = api::settings_private;

PrefsUtil::PrefsUtil(Profile* profile) : profile_(profile) {}

PrefsUtil::~PrefsUtil() {}

#if defined(OS_CHROMEOS)
using CrosSettings = chromeos::CrosSettings;
#endif

const PrefsUtil::TypedPrefMap& PrefsUtil::GetWhitelistedKeys() {
  static PrefsUtil::TypedPrefMap* s_whitelist = nullptr;
  if (s_whitelist)
    return *s_whitelist;
  s_whitelist = new PrefsUtil::TypedPrefMap();

  // Miscellaneous
  (*s_whitelist)[::prefs::kAlternateErrorPagesEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[autofill::prefs::kAutofillEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[bookmarks::prefs::kShowBookmarkBar] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  (*s_whitelist)[::prefs::kUseCustomChromeFrame] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_whitelist)[::prefs::kShowHomeButton] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Appearance settings.
  (*s_whitelist)[::prefs::kCurrentThemeID] =
      settings_private::PrefType::PREF_TYPE_STRING;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  (*s_whitelist)[::prefs::kUsesSystemTheme] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_whitelist)[::prefs::kHomePage] =
      settings_private::PrefType::PREF_TYPE_URL;
  (*s_whitelist)[::prefs::kHomePageIsNewTabPage] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kWebKitDefaultFixedFontSize] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kWebKitDefaultFontSize] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kWebKitMinimumFontSize] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kWebKitFixedFontFamily] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kWebKitSansSerifFontFamily] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kWebKitSerifFontFamily] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kWebKitStandardFontFamily] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kDefaultCharset] =
      settings_private::PrefType::PREF_TYPE_STRING;

  // On startup.
  (*s_whitelist)[::prefs::kRestoreOnStartup] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kURLsToRestoreOnStartup] =
      settings_private::PrefType::PREF_TYPE_LIST;

  // Downloads settings.
  (*s_whitelist)[::prefs::kDownloadDefaultDirectory] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kPromptForDownload] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[drive::prefs::kDisableDrive] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Printing settings.
  (*s_whitelist)[::prefs::kLocalDiscoveryNotificationsEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Miscellaneous. TODO(stevenjb): categorize.
  (*s_whitelist)[::prefs::kEnableDoNotTrack] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kApplicationLocale] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kNetworkPredictionOptions] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[password_manager::prefs::kPasswordManagerSavingEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[password_manager::prefs::kCredentialsEnableAutosignin] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kSafeBrowsingEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kSafeBrowsingExtendedReportingEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kSearchSuggestEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[spellcheck::prefs::kSpellCheckDictionaries] =
      settings_private::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)[spellcheck::prefs::kSpellCheckUseSpellingService] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kEnableTranslate] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[translate::TranslatePrefs::kPrefTranslateBlockedLanguages] =
      settings_private::PrefType::PREF_TYPE_LIST;

  // Site Settings prefs.
  (*s_whitelist)[::prefs::kBlockThirdPartyCookies] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kPluginsAlwaysOpenPdfExternally] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Clear browsing data settings.
  (*s_whitelist)[browsing_data::prefs::kDeleteBrowsingHistory] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteDownloadHistory] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteCache] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteCookies] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeletePasswords] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteFormData] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteHostedAppsData] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteMediaLicenses] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteTimePeriod] =
      settings_private::PrefType::PREF_TYPE_NUMBER;

#if defined(OS_CHROMEOS)
  // Accounts / Users / People.
  (*s_whitelist)[chromeos::kAccountsPrefAllowGuest] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefSupervisedUsersEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefShowUserNamesOnSignIn] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefAllowNewUser] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefUsers] =
      settings_private::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)[::prefs::kEnableAutoScreenLock] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Accessibility.
  (*s_whitelist)[::prefs::kAccessibilitySpokenFeedbackEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityAutoclickEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityAutoclickDelayMs] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityCaretHighlightEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityCursorHighlightEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kShouldAlwaysShowAccessibilityMenu] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityFocusHighlightEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityHighContrastEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityLargeCursorEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityScreenMagnifierEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilitySelectToSpeakEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityStickyKeysEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilitySwitchAccessEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityVirtualKeyboardEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kAccessibilityMonoAudioEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Misc.
  (*s_whitelist)[::prefs::kUse24HourClock] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kLanguagePreferredLanguages] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kTapDraggingEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kStatsReportingPref] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAttestationForContentProtectionEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Bluetooth & Internet settings.
  (*s_whitelist)[chromeos::kAllowBluetooth] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[proxy_config::prefs::kUseSharedProxies] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kWakeOnWifiDarkConnect] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Timezone settings.
  (*s_whitelist)[chromeos::kSystemTimezone] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kResolveTimezoneByGeolocation] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Ash settings.
  (*s_whitelist)[::prefs::kEnableStylusTools] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kLaunchPaletteOnEjectEvent] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Input method settings.
  (*s_whitelist)[::prefs::kLanguagePreloadEngines] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kLanguageEnabledExtensionImes] =
      settings_private::PrefType::PREF_TYPE_STRING;

  // Device settings.
  (*s_whitelist)[::prefs::kTapToClickEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kNaturalScroll] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kTouchpadSensitivity] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kPrimaryMouseButtonRight] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kMouseSensitivity] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapSearchKeyTo] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapControlKeyTo] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapAltKeyTo] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapCapsLockKeyTo] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapBackspaceKeyTo] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapEscapeKeyTo] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapDiamondKeyTo] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageSendFunctionKeys] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kLanguageXkbAutoRepeatEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kLanguageXkbAutoRepeatDelay] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageXkbAutoRepeatInterval] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
#else
  (*s_whitelist)[::prefs::kAcceptLanguages] =
      settings_private::PrefType::PREF_TYPE_STRING;

  // System settings.
  (*s_whitelist)[::prefs::kBackgroundModeEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kHardwareAccelerationModeEnabled] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Import data
  (*s_whitelist)[::prefs::kImportAutofillFormData] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportBookmarks] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportHistory] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportSavedPasswords] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportSearchEngine] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Proxy settings.
  (*s_whitelist)[proxy_config::prefs::kProxy] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;

#if defined(GOOGLE_CHROME_BUILD)
  (*s_whitelist)[::prefs::kMediaRouterEnableCloudServices] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
#endif  // defined(GOOGLE_CHROME_BUILD)

  return *s_whitelist;
}

settings_private::PrefType PrefsUtil::GetType(const std::string& name,
                                              base::Value::Type type) {
  switch (type) {
    case base::Value::Type::TYPE_BOOLEAN:
      return settings_private::PrefType::PREF_TYPE_BOOLEAN;
    case base::Value::Type::TYPE_INTEGER:
    case base::Value::Type::TYPE_DOUBLE:
      return settings_private::PrefType::PREF_TYPE_NUMBER;
    case base::Value::Type::TYPE_STRING:
      return IsPrefTypeURL(name) ? settings_private::PrefType::PREF_TYPE_URL
                                 : settings_private::PrefType::PREF_TYPE_STRING;
    case base::Value::Type::TYPE_LIST:
      return settings_private::PrefType::PREF_TYPE_LIST;
    case base::Value::Type::TYPE_DICTIONARY:
      return settings_private::PrefType::PREF_TYPE_DICTIONARY;
    default:
      return settings_private::PrefType::PREF_TYPE_NONE;
  }
}

std::unique_ptr<settings_private::PrefObject> PrefsUtil::GetCrosSettingsPref(
    const std::string& name) {
  std::unique_ptr<settings_private::PrefObject> pref_object(
      new settings_private::PrefObject());

#if defined(OS_CHROMEOS)
  const base::Value* value = CrosSettings::Get()->GetPref(name);
  DCHECK(value) << "Pref not found: " << name;
  pref_object->key = name;
  pref_object->type = GetType(name, value->GetType());
  pref_object->value.reset(value->DeepCopy());
#endif

  return pref_object;
}

std::unique_ptr<settings_private::PrefObject> PrefsUtil::GetPref(
    const std::string& name) {
  const PrefService::Preference* pref = nullptr;
  std::unique_ptr<settings_private::PrefObject> pref_object;
  if (IsCrosSetting(name)) {
    pref_object = GetCrosSettingsPref(name);
  } else {
    PrefService* pref_service = FindServiceForPref(name);
    pref = pref_service->FindPreference(name);
    if (!pref)
      return nullptr;
    pref_object.reset(new settings_private::PrefObject());
    pref_object->key = pref->name();
    pref_object->type = GetType(name, pref->GetType());
    pref_object->value.reset(pref->GetValue()->DeepCopy());
  }

#if defined(OS_CHROMEOS)
  if (IsPrefPrimaryUserControlled(name)) {
    pref_object->controlled_by =
        settings_private::ControlledBy::CONTROLLED_BY_PRIMARY_USER;
    pref_object->enforcement =
        settings_private::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->controlled_by_name.reset(
        new std::string(user_manager::UserManager::Get()
                            ->GetPrimaryUser()
                            ->GetAccountId()
                            .GetUserEmail()));
    return pref_object;
  }

  if (IsPrefEnterpriseManaged(name)) {
    // Enterprise managed prefs are treated the same as device policy restricted
    // prefs in the UI.
    pref_object->controlled_by =
        settings_private::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    pref_object->enforcement =
        settings_private::Enforcement::ENFORCEMENT_ENFORCED;
    return pref_object;
  }
#endif

  if (pref && pref->IsManaged()) {
    pref_object->controlled_by =
        settings_private::ControlledBy::CONTROLLED_BY_USER_POLICY;
    pref_object->enforcement =
        settings_private::Enforcement::ENFORCEMENT_ENFORCED;
    return pref_object;
  }

  if (pref && pref->IsRecommended()) {
    pref_object->controlled_by =
        settings_private::ControlledBy::CONTROLLED_BY_USER_POLICY;
    pref_object->enforcement =
        settings_private::Enforcement::ENFORCEMENT_RECOMMENDED;
    pref_object->recommended_value.reset(
        pref->GetRecommendedValue()->DeepCopy());
    return pref_object;
  }

#if defined(OS_CHROMEOS)
  if (IsPrefOwnerControlled(name)) {
    // Check for owner controlled after managed checks because if there is a
    // device policy there is no "owner". (In the unlikely case that both
    // situations apply, either badge is potentially relevant, so the order
    // is somewhat arbitrary).
    pref_object->controlled_by =
        settings_private::ControlledBy::CONTROLLED_BY_OWNER;
    pref_object->enforcement =
        settings_private::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->controlled_by_name.reset(new std::string(
        user_manager::UserManager::Get()->GetOwnerAccountId().GetUserEmail()));
    return pref_object;
  }
#endif

  const Extension* extension = GetExtensionControllingPref(*pref_object);
  if (extension) {
    pref_object->controlled_by =
        settings_private::ControlledBy::CONTROLLED_BY_EXTENSION;
    pref_object->enforcement =
        settings_private::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->extension_id.reset(new std::string(extension->id()));
    pref_object->controlled_by_name.reset(new std::string(extension->name()));
    bool can_be_disabled = !ExtensionSystem::Get(profile_)->management_policy()
        ->MustRemainEnabled(extension, nullptr);
    pref_object->extension_can_be_disabled.reset(new bool(can_be_disabled));
    return pref_object;
  }

  // TODO(dbeam): surface !IsUserModifiable or IsPrefSupervisorControlled?

  return pref_object;
}

PrefsUtil::SetPrefResult PrefsUtil::SetPref(const std::string& pref_name,
                                            const base::Value* value) {
  if (IsCrosSetting(pref_name))
    return SetCrosSettingsPref(pref_name, value);

  PrefService* pref_service = FindServiceForPref(pref_name);

  if (!IsPrefUserModifiable(pref_name))
    return PREF_NOT_MODIFIABLE;

  const PrefService::Preference* pref = pref_service->FindPreference(pref_name);
  if (!pref)
    return PREF_NOT_FOUND;

  DCHECK_EQ(pref->GetType(), value->GetType());

  switch (pref->GetType()) {
    case base::Value::TYPE_BOOLEAN:
    case base::Value::TYPE_DOUBLE:
    case base::Value::TYPE_LIST:
    case base::Value::TYPE_DICTIONARY:
      pref_service->Set(pref_name, *value);
      break;
    case base::Value::TYPE_INTEGER: {
      // In JS all numbers are doubles.
      double double_value;
      if (!value->GetAsDouble(&double_value))
        return PREF_TYPE_MISMATCH;

      pref_service->SetInteger(pref_name, static_cast<int>(double_value));
      break;
    }
    case base::Value::TYPE_STRING: {
      std::string string_value;
      if (!value->GetAsString(&string_value))
        return PREF_TYPE_MISMATCH;

      if (IsPrefTypeURL(pref_name)) {
        GURL fixed = url_formatter::FixupURL(string_value, std::string());
        if (fixed.is_valid())
          string_value = fixed.spec();
        else
          string_value = std::string();
      }

      pref_service->SetString(pref_name, string_value);
      break;
    }
    default:
      return PREF_TYPE_UNSUPPORTED;
  }

  // TODO(orenb): Process setting metrics here and in the CrOS setting method
  // too (like "ProcessUserMetric" in CoreOptionsHandler).
  return SUCCESS;
}

PrefsUtil::SetPrefResult PrefsUtil::SetCrosSettingsPref(
    const std::string& pref_name, const base::Value* value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Check if setting requires owner.
  if (service && service->HandlesSetting(pref_name)) {
    if (service->Set(pref_name, *value))
      return SUCCESS;
    return PREF_NOT_MODIFIABLE;
  }

  CrosSettings::Get()->Set(pref_name, *value);
  return SUCCESS;
#else
  return PREF_NOT_FOUND;
#endif
}

bool PrefsUtil::AppendToListCrosSetting(const std::string& pref_name,
                                        const base::Value& value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Returns false if not the owner, for settings requiring owner.
  if (service && service->HandlesSetting(pref_name)) {
    return service->AppendToList(pref_name, value);
  }

  CrosSettings::Get()->AppendToList(pref_name, &value);
  return true;
#else
  return false;
#endif
}

bool PrefsUtil::RemoveFromListCrosSetting(const std::string& pref_name,
                                          const base::Value& value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Returns false if not the owner, for settings requiring owner.
  if (service && service->HandlesSetting(pref_name)) {
    return service->RemoveFromList(pref_name, value);
  }

  CrosSettings::Get()->RemoveFromList(pref_name, &value);
  return true;
#else
  return false;
#endif
}

bool PrefsUtil::IsPrefTypeURL(const std::string& pref_name) {
  settings_private::PrefType pref_type =
      settings_private::PrefType::PREF_TYPE_NONE;

  const TypedPrefMap keys = GetWhitelistedKeys();
  const auto& iter = keys.find(pref_name);
  if (iter != keys.end())
    pref_type = iter->second;

  return pref_type == settings_private::PrefType::PREF_TYPE_URL;
}

#if defined(OS_CHROMEOS)
bool PrefsUtil::IsPrefEnterpriseManaged(const std::string& pref_name) {
  if (IsPrivilegedCrosSetting(pref_name)) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    if (connector->IsEnterpriseManaged())
      return true;
  }
  return false;
}

bool PrefsUtil::IsPrefOwnerControlled(const std::string& pref_name) {
  if (IsPrivilegedCrosSetting(pref_name)) {
    if (!chromeos::ProfileHelper::IsOwnerProfile(profile_))
      return true;
  }
  return false;
}

bool PrefsUtil::IsPrefPrimaryUserControlled(const std::string& pref_name) {
  if (pref_name == prefs::kWakeOnWifiDarkConnect) {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user &&
        user->GetAccountId() != user_manager->GetPrimaryUser()->GetAccountId())
      return true;
  }
  return false;
}
#endif

bool PrefsUtil::IsPrefSupervisorControlled(const std::string& pref_name) {
  if (pref_name != prefs::kBrowserGuestModeEnabled &&
      pref_name != prefs::kBrowserAddPersonEnabled) {
    return false;
  }
  return profile_->IsSupervised();
}

bool PrefsUtil::IsPrefUserModifiable(const std::string& pref_name) {
  const PrefService::Preference* profile_pref =
      profile_->GetPrefs()->FindPreference(pref_name);
  if (profile_pref)
    return profile_pref->IsUserModifiable();

  const PrefService::Preference* local_state_pref =
      g_browser_process->local_state()->FindPreference(pref_name);
  if (local_state_pref)
    return local_state_pref->IsUserModifiable();

  return false;
}

PrefService* PrefsUtil::FindServiceForPref(const std::string& pref_name) {
  PrefService* user_prefs = profile_->GetPrefs();

  // Proxy is a peculiar case: on ChromeOS, settings exist in both user
  // prefs and local state, but chrome://settings should affect only user prefs.
  // Elsewhere the proxy settings are stored in local state.
  // See http://crbug.com/157147

  if (pref_name == proxy_config::prefs::kProxy) {
#if defined(OS_CHROMEOS)
    return user_prefs;
#else
    return g_browser_process->local_state();
#endif
  }

  // Find which PrefService contains the given pref. Pref names should not
  // be duplicated across services, however if they are, prefer the user's
  // prefs.
  if (user_prefs->FindPreference(pref_name))
    return user_prefs;

  if (g_browser_process->local_state()->FindPreference(pref_name))
    return g_browser_process->local_state();

  return user_prefs;
}

bool PrefsUtil::IsCrosSetting(const std::string& pref_name) {
#if defined(OS_CHROMEOS)
  return CrosSettings::Get()->IsCrosSettings(pref_name);
#else
  return false;
#endif
}

const Extension* PrefsUtil::GetExtensionControllingPref(
    const settings_private::PrefObject& pref_object) {
  // Look for specific prefs that might be extension controlled. This generally
  // corresponds with some indiciator that should be shown in the settings UI.
  if (pref_object.key == ::prefs::kHomePage)
    return GetExtensionOverridingHomepage(profile_);
  if (pref_object.key == ::prefs::kURLsToRestoreOnStartup)
    return GetExtensionOverridingStartupPages(profile_);
  if (pref_object.key == ::prefs::kDefaultSearchProviderEnabled)
    return GetExtensionOverridingSearchEngine(profile_);
  if (pref_object.key == proxy_config::prefs::kProxy)
    return GetExtensionOverridingProxy(profile_);
  return nullptr;
}

}  // namespace extensions
