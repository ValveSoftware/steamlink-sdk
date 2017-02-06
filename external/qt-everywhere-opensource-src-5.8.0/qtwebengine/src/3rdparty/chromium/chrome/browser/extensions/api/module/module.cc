// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/module/module.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_url_handlers.h"

namespace extensions {

namespace extension {

namespace {

// A preference for storing the extension's update URL data. If not empty, the
// the ExtensionUpdater will append a ap= parameter to the URL when checking if
// a new version of the extension is available.
const char kUpdateURLData[] = "update_url_data";

}  // namespace

std::string GetUpdateURLData(const ExtensionPrefs* prefs,
                             const std::string& extension_id) {
  std::string data;
  prefs->ReadPrefAsString(extension_id, kUpdateURLData, &data);
  return data;
}

}  // namespace extension

bool ExtensionSetUpdateUrlDataFunction::RunSync() {
  std::string data;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &data));

  if (ManifestURL::UpdatesFromGallery(extension())) {
    return false;
  }

  ExtensionPrefs::Get(GetProfile())->UpdateExtensionPref(
      extension_id(), extension::kUpdateURLData, new base::StringValue(data));
  return true;
}

bool ExtensionIsAllowedIncognitoAccessFunction::RunSync() {
  SetResult(base::MakeUnique<base::FundamentalValue>(
      util::IsIncognitoEnabled(extension_id(), GetProfile())));
  return true;
}

bool ExtensionIsAllowedFileSchemeAccessFunction::RunSync() {
  SetResult(base::MakeUnique<base::FundamentalValue>(
      util::AllowFileAccess(extension_id(), GetProfile())));
  return true;
}

}  // namespace extensions
