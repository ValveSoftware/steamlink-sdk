// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/prefs_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/url_formatter/url_fixer.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
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
  // TODO(dbeam): why aren't we using kPrefName from pref_names.h?
  s_whitelist = new PrefsUtil::TypedPrefMap();
  (*s_whitelist)["alternate_error_pages.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["autofill.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["bookmark_bar.show_on_all_tabs"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.show_home_button"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Appearance settings.
  (*s_whitelist)["extensions.theme.id"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["webkit.webprefs.default_fixed_font_size"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["webkit.webprefs.default_font_size"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["webkit.webprefs.minimum_font_size"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["webkit.webprefs.fonts.fixed.Zyyy"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["webkit.webprefs.fonts.sansserif.Zyyy"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["webkit.webprefs.fonts.serif.Zyyy"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["webkit.webprefs.fonts.standard.Zyyy"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["intl.charset_default"] =
      settings_private::PrefType::PREF_TYPE_STRING;

  // Downloads settings.
  (*s_whitelist)["download.default_directory"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["download.prompt_for_download"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["gdata.disabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Printing settings.
  (*s_whitelist)["local_discovery.notifications_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  (*s_whitelist)["enable_do_not_track"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["homepage"] = settings_private::PrefType::PREF_TYPE_URL;
  (*s_whitelist)["homepage_is_newtabpage"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["intl.app_locale"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["net.network_prediction_options"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.password_manager_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["safebrowsing.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["safebrowsing.extended_reporting_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["search.suggest_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["session.restore_on_startup"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["session.startup_urls"] =
      settings_private::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)["spellcheck.dictionaries"] =
      settings_private::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)["spellcheck.use_spelling_service"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["translate.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["translate_blocked_languages"] =
      settings_private::PrefType::PREF_TYPE_LIST;

  // Site Settings prefs.
  (*s_whitelist)["profile.block_third_party_cookies"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Clear browsing data settings.
  (*s_whitelist)["browser.clear_data.browsing_history"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.download_history"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.cache"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.cookies"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.passwords"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.form_data"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.hosted_apps_data"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.media_licenses"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.clear_data.time_period"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.cookies"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.fullscreen"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.geolocation"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.images"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.javascript"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.media_stream_camera"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.media_stream_mic"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.notifications"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.default_content_setting_values.popups"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["profile.content_settings.exceptions.cookies"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.fullscreen"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.geolocation"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.images"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.javascript"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.media_stream_camera"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.media_stream_mic"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.notifications"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)["profile.content_settings.exceptions.popups"] =
      settings_private::PrefType::PREF_TYPE_DICTIONARY;

#if defined(OS_CHROMEOS)
  (*s_whitelist)["cros.accounts.allowBWSI"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.supervisedUsersEnabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.showUserNamesOnSignIn"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.allowGuest"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.users"] =
      settings_private::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)["settings.accessibility"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.autoclick"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.autoclick_delay_ms"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.caret_highlight"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.cursor_highlight"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.enable_menu"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.focus_highlight"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.high_contrast_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.large_cursor_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.screen_magnifier"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.select_to_speak"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.sticky_keys_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.switch_access"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.virtual_keyboard"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.mono_audio"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.clock.use_24hour_clock"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.language.preferred_languages"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["settings.touchpad.enable_tap_dragging"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.metrics.reportingEnabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.device.attestation_for_content_protection_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.internet.wake_on_wifi_darkconnect"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.enable_screen_lock"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

  // Input method settings.
  (*s_whitelist)["settings.language.preload_engines"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["settings.language.enabled_extension_imes"] =
      settings_private::PrefType::PREF_TYPE_STRING;

  // Device settings.
  (*s_whitelist)["settings.touchpad.enable_tap_to_click"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.touchpad.natural_scroll"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.language.xkb_remap_search_key_to"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["settings.language.xkb_remap_control_key_to"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["settings.language.xkb_remap_alt_key_to"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["settings.language.remap_caps_lock_key_to"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["settings.language.remap_diamond_key_to"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["settings.language.send_function_keys"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.language.xkb_auto_repeat_enabled_r2"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.language.xkb_auto_repeat_delay_r2"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["settings.language.xkb_auto_repeat_interval_r2"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
#else
  (*s_whitelist)["intl.accept_languages"] =
      settings_private::PrefType::PREF_TYPE_STRING;

  // System settings.
  (*s_whitelist)["background_mode.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["hardware_acceleration_mode.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
#endif

#if defined(GOOGLE_CHROME_BUILD)
  (*s_whitelist)["media_router.cloudservices.enabled"] =
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
  DCHECK(value);
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
    pref_object->policy_source =
        settings_private::PolicySource::POLICY_SOURCE_PRIMARY_USER;
    pref_object->policy_enforcement =
        settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
    pref_object->policy_source_name.reset(new std::string(
        user_manager::UserManager::Get()->GetPrimaryUser()->email()));
    return pref_object;
  }
  if (IsPrefEnterpriseManaged(name)) {
    // Enterprise managed prefs are treated the same as device policy restricted
    // prefs in the UI.
    pref_object->policy_source =
        settings_private::PolicySource::POLICY_SOURCE_DEVICE_POLICY;
    pref_object->policy_enforcement =
        settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
    return pref_object;
  }
#endif

  if (pref && pref->IsManaged()) {
    pref_object->policy_source =
        settings_private::PolicySource::POLICY_SOURCE_USER_POLICY;
    pref_object->policy_enforcement =
        settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
    return pref_object;
  }
  if (pref && pref->IsRecommended()) {
    pref_object->policy_source =
        settings_private::PolicySource::POLICY_SOURCE_USER_POLICY;
    pref_object->policy_enforcement =
        settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_RECOMMENDED;
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
    pref_object->policy_source =
        settings_private::PolicySource::POLICY_SOURCE_OWNER;
    pref_object->policy_enforcement =
        settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
    pref_object->policy_source_name.reset(new std::string(
        user_manager::UserManager::Get()->GetOwnerAccountId().GetUserEmail()));
    return pref_object;
  }
#endif

  if (pref && pref->IsExtensionControlled()) {
    std::string extension_id =
        ExtensionPrefValueMapFactory::GetForBrowserContext(profile_)
            ->GetExtensionControllingPref(pref->name());
    const Extension* extension = ExtensionRegistry::Get(profile_)->
        GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
    if (extension) {
      pref_object->policy_source =
          settings_private::PolicySource::POLICY_SOURCE_EXTENSION;
      pref_object->policy_enforcement =
          settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
      pref_object->extension_id.reset(new std::string(extension_id));
      pref_object->policy_source_name.reset(new std::string(extension->name()));
      return pref_object;
    }
  }
  if (pref && (!pref->IsUserModifiable() || IsPrefSupervisorControlled(name))) {
    // TODO(stevenjb): Investigate whether either of these should be badged.
    pref_object->read_only.reset(new bool(true));
    return pref_object;
  }

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
    if (user && user->email() != user_manager->GetPrimaryUser()->email())
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

}  // namespace extensions
