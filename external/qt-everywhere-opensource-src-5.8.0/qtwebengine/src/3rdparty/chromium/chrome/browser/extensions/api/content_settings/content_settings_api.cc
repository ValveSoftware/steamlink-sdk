// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api_constants.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/content_settings.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/plugin_service.h"
#include "extensions/browser/extension_prefs_scope.h"
#include "extensions/common/error_utils.h"

using content::BrowserThread;
using content::PluginService;

namespace Clear = extensions::api::content_settings::ContentSetting::Clear;
namespace Get = extensions::api::content_settings::ContentSetting::Get;
namespace Set = extensions::api::content_settings::ContentSetting::Set;
namespace pref_helpers = extensions::preference_helpers;
namespace pref_keys = extensions::preference_api_constants;

namespace {

bool RemoveContentType(base::ListValue* args,
                       ContentSettingsType* content_type) {
  std::string content_type_str;
  if (!args->GetString(0, &content_type_str))
    return false;
  // We remove the ContentSettingsType parameter since this is added by the
  // renderer, and is not part of the JSON schema.
  args->Remove(0, NULL);
  *content_type =
      extensions::content_settings_helpers::StringToContentSettingsType(
          content_type_str);
  return *content_type != CONTENT_SETTINGS_TYPE_DEFAULT;
}

}  // namespace

namespace extensions {

namespace helpers = content_settings_helpers;
namespace keys = content_settings_api_constants;

bool ContentSettingsContentSettingClearFunction::RunSync() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  std::unique_ptr<Clear::Params> params(Clear::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  bool incognito = false;
  if (params->details.scope ==
      api::content_settings::SCOPE_INCOGNITO_SESSION_ONLY) {
    scope = kExtensionPrefsScopeIncognitoSessionOnly;
    incognito = true;
  }

  if (incognito) {
    // We don't check incognito permissions here, as an extension should be
    // always allowed to clear its own settings.
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (GetProfile()->IsOffTheRecord()) {
      error_ = keys::kIncognitoContextError;
      return false;
    }
  }

  scoped_refptr<ContentSettingsStore> store =
      ContentSettingsService::Get(GetProfile())->content_settings_store();
  store->ClearContentSettingsForExtension(extension_id(), scope);

  return true;
}

bool ContentSettingsContentSettingGetFunction::RunSync() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  std::unique_ptr<Get::Params> params(Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL primary_url(params->details.primary_url);
  if (!primary_url.is_valid()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
        params->details.primary_url);
    return false;
  }

  GURL secondary_url(primary_url);
  if (params->details.secondary_url.get()) {
    secondary_url = GURL(*params->details.secondary_url);
    if (!secondary_url.is_valid()) {
      error_ = ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
        *params->details.secondary_url);
      return false;
    }
  }

  std::string resource_identifier;
  if (params->details.resource_identifier.get())
    resource_identifier = params->details.resource_identifier->id;

  bool incognito = false;
  if (params->details.incognito.get())
    incognito = *params->details.incognito;
  if (incognito && !include_incognito()) {
    error_ = pref_keys::kIncognitoErrorMessage;
    return false;
  }

  HostContentSettingsMap* map;
  content_settings::CookieSettings* cookie_settings;
  if (incognito) {
    if (!GetProfile()->HasOffTheRecordProfile()) {
      // TODO(bauerb): Allow reading incognito content settings
      // outside of an incognito session.
      error_ = keys::kIncognitoSessionOnlyError;
      return false;
    }
    map = HostContentSettingsMapFactory::GetForProfile(
        GetProfile()->GetOffTheRecordProfile());
    cookie_settings = CookieSettingsFactory::GetForProfile(
                          GetProfile()->GetOffTheRecordProfile()).get();
  } else {
    map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
    cookie_settings = CookieSettingsFactory::GetForProfile(GetProfile()).get();
  }

  ContentSetting setting;
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
    // TODO(jochen): Do we return the value for setting or for reading cookies?
    bool setting_cookie = false;
    setting = cookie_settings->GetCookieSetting(primary_url, secondary_url,
                                                setting_cookie, NULL);
  } else {
    setting = map->GetContentSetting(primary_url, secondary_url, content_type,
                                     resource_identifier);
  }

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());
  result->SetString(keys::kContentSettingKey, setting_string);

  SetResult(std::move(result));

  return true;
}

bool ContentSettingsContentSettingSetFunction::RunSync() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  std::unique_ptr<Set::Params> params(Set::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string primary_error;
  ContentSettingsPattern primary_pattern =
      helpers::ParseExtensionPattern(params->details.primary_pattern,
                                     &primary_error);
  if (!primary_pattern.IsValid()) {
    error_ = primary_error;
    return false;
  }

  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  if (params->details.secondary_pattern.get()) {
    std::string secondary_error;
    secondary_pattern =
        helpers::ParseExtensionPattern(*params->details.secondary_pattern,
                                       &secondary_error);
    if (!secondary_pattern.IsValid()) {
      error_ = secondary_error;
      return false;
    }
  }

  std::string resource_identifier;
  if (params->details.resource_identifier.get())
    resource_identifier = params->details.resource_identifier->id;

  std::string setting_str;
  EXTENSION_FUNCTION_VALIDATE(
      params->details.setting->GetAsString(&setting_str));
  ContentSetting setting;
  EXTENSION_FUNCTION_VALIDATE(
      content_settings::ContentSettingFromString(setting_str, &setting));
  EXTENSION_FUNCTION_VALIDATE(
      content_settings::ContentSettingsRegistry::GetInstance()
          ->Get(content_type)
          ->IsSettingValid(setting));

  // Some content setting types support the full set of values listed in
  // content_settings.json only for exceptions. For the default setting,
  // some values might not be supported.
  // For example, camera supports [allow, ask, block] for exceptions, but only
  // [ask, block] for the default setting.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      !HostContentSettingsMap::IsDefaultSettingAllowedForType(setting,
                                                              content_type)) {
    static const char kUnsupportedDefaultSettingError[] =
        "'%s' is not supported as the default setting of %s.";

    // TODO(msramek): Get the same human readable name as is presented
    // externally in the API, i.e. chrome.contentSettings.<name>.set().
    std::string readable_type_name;
    if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
      readable_type_name = "microphone";
    } else if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
      readable_type_name = "camera";
    } else {
      NOTREACHED() << "No human-readable type name defined for this type.";
    }

    error_ = base::StringPrintf(
        kUnsupportedDefaultSettingError,
        setting_str.c_str(),
        readable_type_name.c_str());
    return false;
  }

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  bool incognito = false;
  if (params->details.scope ==
      api::content_settings::SCOPE_INCOGNITO_SESSION_ONLY) {
    scope = kExtensionPrefsScopeIncognitoSessionOnly;
    incognito = true;
  }

  if (incognito) {
    // Regular profiles can't access incognito unless include_incognito is true.
    if (!GetProfile()->IsOffTheRecord() && !include_incognito()) {
      error_ = pref_keys::kIncognitoErrorMessage;
      return false;
    }
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (GetProfile()->IsOffTheRecord()) {
      error_ = keys::kIncognitoContextError;
      return false;
    }
  }

  if (scope == kExtensionPrefsScopeIncognitoSessionOnly &&
      !GetProfile()->HasOffTheRecordProfile()) {
    error_ = pref_keys::kIncognitoSessionOnlyErrorMessage;
    return false;
  }

  scoped_refptr<ContentSettingsStore> store =
      ContentSettingsService::Get(GetProfile())->content_settings_store();
  store->SetExtensionContentSetting(extension_id(), primary_pattern,
                                    secondary_pattern, content_type,
                                    resource_identifier, setting, scope);
  return true;
}

bool ContentSettingsContentSettingGetResourceIdentifiersFunction::RunAsync() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  if (content_type != CONTENT_SETTINGS_TYPE_PLUGINS) {
    SendResponse(true);
    return true;
  }

  PluginService::GetInstance()->GetPlugins(
      base::Bind(&ContentSettingsContentSettingGetResourceIdentifiersFunction::
                 OnGotPlugins,
                 this));
  return true;
}

void ContentSettingsContentSettingGetResourceIdentifiersFunction::OnGotPlugins(
    const std::vector<content::WebPluginInfo>& plugins) {
  PluginFinder* finder = PluginFinder::GetInstance();
  std::set<std::string> group_identifiers;
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (std::vector<content::WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end(); ++it) {
    std::unique_ptr<PluginMetadata> plugin_metadata(
        finder->GetPluginMetadata(*it));
    const std::string& group_identifier = plugin_metadata->identifier();
    if (group_identifiers.find(group_identifier) != group_identifiers.end())
      continue;

    group_identifiers.insert(group_identifier);
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString(keys::kIdKey, group_identifier);
    dict->SetString(keys::kDescriptionKey, plugin_metadata->name());
    list->Append(std::move(dict));
  }
  SetResult(std::move(list));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          &ContentSettingsContentSettingGetResourceIdentifiersFunction::
          SendResponse,
          this,
          true));
}

}  // namespace extensions
