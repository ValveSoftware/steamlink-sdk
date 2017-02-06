// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

namespace settings_private = api::settings_private;

SettingsPrivateDelegate::SettingsPrivateDelegate(Profile* profile)
    : profile_(profile) {
  prefs_util_.reset(new PrefsUtil(profile));
}

SettingsPrivateDelegate::~SettingsPrivateDelegate() {
}

std::unique_ptr<base::Value> SettingsPrivateDelegate::GetPref(
    const std::string& name) {
  std::unique_ptr<api::settings_private::PrefObject> pref =
      prefs_util_->GetPref(name);
  if (!pref)
    return base::Value::CreateNullValue();
  return pref->ToValue();
}

std::unique_ptr<base::Value> SettingsPrivateDelegate::GetAllPrefs() {
  std::unique_ptr<base::ListValue> prefs(new base::ListValue());

  const TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
  for (const auto& it : keys) {
    std::unique_ptr<base::Value> pref = GetPref(it.first);
    if (!pref->IsType(base::Value::TYPE_NULL))
      prefs->Append(std::move(pref));
  }

  return std::move(prefs);
}

PrefsUtil::SetPrefResult SettingsPrivateDelegate::SetPref(
    const std::string& pref_name, const base::Value* value) {
  return prefs_util_->SetPref(pref_name, value);
}

std::unique_ptr<base::Value> SettingsPrivateDelegate::GetDefaultZoomPercent() {
  double zoom = content::ZoomLevelToZoomFactor(
      profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref()) * 100;
  std::unique_ptr<base::Value> value(new base::FundamentalValue(zoom));
  return value;
}

PrefsUtil::SetPrefResult SettingsPrivateDelegate::SetDefaultZoomPercent(
    int percent) {
  double zoom_factor = content::ZoomFactorToZoomLevel(percent * 0.01);
  profile_->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(zoom_factor);
  return PrefsUtil::SetPrefResult::SUCCESS;
}

}  // namespace extensions
