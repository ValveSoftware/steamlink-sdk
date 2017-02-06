// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/chrome_direct_setting.h"

#include <utility>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/preference/chrome_direct_setting_api.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace extensions {
namespace chromedirectsetting {

DirectSettingFunctionBase::DirectSettingFunctionBase() {}

DirectSettingFunctionBase::~DirectSettingFunctionBase() {}

PrefService* DirectSettingFunctionBase::GetPrefService() {
  return GetProfile()->GetPrefs();
}

GetDirectSettingFunction::GetDirectSettingFunction() {}

bool GetDirectSettingFunction::RunSync() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  EXTENSION_FUNCTION_VALIDATE(ChromeDirectSettingAPI::Get(GetProfile())
                                  ->IsPreferenceOnWhitelist(pref_key));

  const PrefService::Preference* preference =
      GetPrefService()->FindPreference(pref_key.c_str());
  EXTENSION_FUNCTION_VALIDATE(preference);
  const base::Value* value = preference->GetValue();

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->Set(preference_api_constants::kValue, value->DeepCopy());
  SetResult(std::move(result));

  return true;
}

GetDirectSettingFunction::~GetDirectSettingFunction() {}

SetDirectSettingFunction::SetDirectSettingFunction() {}

bool SetDirectSettingFunction::RunSync() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  EXTENSION_FUNCTION_VALIDATE(ChromeDirectSettingAPI::Get(GetProfile())
                                  ->IsPreferenceOnWhitelist(pref_key));

  base::DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  base::Value* value = NULL;
  EXTENSION_FUNCTION_VALIDATE(
      details->Get(preference_api_constants::kValue, &value));

  PrefService* pref_service = GetPrefService();
  const PrefService::Preference* preference =
      pref_service->FindPreference(pref_key.c_str());
  EXTENSION_FUNCTION_VALIDATE(preference);

  EXTENSION_FUNCTION_VALIDATE(value->GetType() == preference->GetType());

  pref_service->Set(pref_key.c_str(), *value);

  return true;
}

SetDirectSettingFunction::~SetDirectSettingFunction() {}

ClearDirectSettingFunction::ClearDirectSettingFunction() {}

bool ClearDirectSettingFunction::RunSync() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  EXTENSION_FUNCTION_VALIDATE(ChromeDirectSettingAPI::Get(GetProfile())
                                  ->IsPreferenceOnWhitelist(pref_key));
  GetPrefService()->ClearPref(pref_key.c_str());

  return true;
}

ClearDirectSettingFunction::~ClearDirectSettingFunction() {}

}  // namespace chromedirectsetting
}  // namespace extensions

