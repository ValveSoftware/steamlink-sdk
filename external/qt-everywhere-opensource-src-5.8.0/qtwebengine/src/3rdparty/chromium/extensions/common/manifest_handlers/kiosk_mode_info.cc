// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

#include <memory>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;

using api::extensions_manifest_types::KioskSecondaryAppsType;

KioskModeInfo::KioskModeInfo(KioskStatus kiosk_status,
                             const std::vector<std::string>& secondary_app_ids,
                             const std::string& required_platform_version)
    : kiosk_status(kiosk_status),
      secondary_app_ids(secondary_app_ids),
      required_platform_version(required_platform_version) {}

KioskModeInfo::~KioskModeInfo() {
}

// static
KioskModeInfo* KioskModeInfo::Get(const Extension* extension) {
  return static_cast<KioskModeInfo*>(
      extension->GetManifestData(keys::kKioskMode));
}

// static
bool KioskModeInfo::IsKioskEnabled(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info ? info->kiosk_status != NONE : false;
}

// static
bool KioskModeInfo::IsKioskOnly(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info ? info->kiosk_status == ONLY : false;
}

// static
bool KioskModeInfo::HasSecondaryApps(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info && !info->secondary_app_ids.empty();
}

// static
bool KioskModeInfo::IsValidPlatformVersion(const std::string& version_string) {
  const base::Version version(version_string);
  return version.IsValid() && version.components().size() <= 3u;
}

KioskModeHandler::KioskModeHandler() {
  supported_keys_.push_back(keys::kKiosk);
  supported_keys_.push_back(keys::kKioskEnabled);
  supported_keys_.push_back(keys::kKioskOnly);
  supported_keys_.push_back(keys::kKioskSecondaryApps);
}

KioskModeHandler::~KioskModeHandler() {
}

bool KioskModeHandler::Parse(Extension* extension, base::string16* error) {
  const Manifest* manifest = extension->manifest();
  DCHECK(manifest->HasKey(keys::kKioskEnabled) ||
         manifest->HasKey(keys::kKioskOnly));

  bool kiosk_enabled = false;
  if (manifest->HasKey(keys::kKioskEnabled) &&
      !manifest->GetBoolean(keys::kKioskEnabled, &kiosk_enabled)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskEnabled);
    return false;
  }

  bool kiosk_only = false;
  if (manifest->HasKey(keys::kKioskOnly) &&
      !manifest->GetBoolean(keys::kKioskOnly, &kiosk_only)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskOnly);
    return false;
  }

  if (kiosk_only && !kiosk_enabled) {
    *error = base::ASCIIToUTF16(
        manifest_errors::kInvalidKioskOnlyButNotEnabled);
    return false;
  }

  // All other use cases should be already filtered out by manifest feature
  // checks.
  DCHECK(extension->is_platform_app());

  KioskModeInfo::KioskStatus kiosk_status = KioskModeInfo::NONE;
  if (kiosk_enabled)
    kiosk_status = kiosk_only ? KioskModeInfo::ONLY : KioskModeInfo::ENABLED;

  // Kiosk secondary apps key is optional.
  std::vector<std::string> ids;
  if (manifest->HasKey(keys::kKioskSecondaryApps)) {
    const base::Value* secondary_apps = nullptr;
    const base::ListValue* list = nullptr;
    if (!manifest->Get(keys::kKioskSecondaryApps, &secondary_apps) ||
        !secondary_apps->GetAsList(&list)) {
      *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskSecondaryApps);
      return false;
    }

    for (const auto& value : *list) {
      std::unique_ptr<KioskSecondaryAppsType> app =
          KioskSecondaryAppsType::FromValue(*value, error);
      if (!app) {
        *error = base::ASCIIToUTF16(
            manifest_errors::kInvalidKioskSecondaryAppsBadAppId);
        return false;
      }
      ids.push_back(app->id);
    }
  }

  // Optional kiosk.required_platform_version key.
  std::string required_platform_version;
  if (manifest->HasPath(keys::kKioskRequiredPlatformVersion) &&
      (!manifest->GetString(keys::kKioskRequiredPlatformVersion,
                            &required_platform_version) ||
       !KioskModeInfo::IsValidPlatformVersion(required_platform_version))) {
    *error = base::ASCIIToUTF16(
        manifest_errors::kInvalidKioskRequiredPlatformVersion);
    return false;
  }

  extension->SetManifestData(
      keys::kKioskMode,
      new KioskModeInfo(kiosk_status, ids, required_platform_version));

  return true;
}

const std::vector<std::string> KioskModeHandler::Keys() const {
  return supported_keys_;
}

}  // namespace extensions
