// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/launcher_page_info.h"

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

LauncherPageHandler::LauncherPageHandler() {
}

LauncherPageHandler::~LauncherPageHandler() {
}

// static
LauncherPageInfo* LauncherPageHandler::GetInfo(const Extension* extension) {
  return static_cast<LauncherPageInfo*>(
      extension->GetManifestData(manifest_keys::kLauncherPage));
}

bool LauncherPageHandler::Parse(Extension* extension, base::string16* error) {
  const extensions::Manifest* manifest = extension->manifest();
  std::unique_ptr<LauncherPageInfo> launcher_page_info(new LauncherPageInfo);
  const base::DictionaryValue* launcher_page_dict = NULL;
  if (!manifest->GetDictionary(manifest_keys::kLauncherPage,
                               &launcher_page_dict)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidLauncherPage);
    return false;
  }

  if (!manifest->HasPath(extensions::manifest_keys::kLauncherPagePage)) {
    *error = base::ASCIIToUTF16(manifest_errors::kLauncherPagePageRequired);
    return false;
  }

  std::string launcher_page_page;
  if (!manifest->GetString(extensions::manifest_keys::kLauncherPagePage,
                           &launcher_page_page)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidLauncherPagePage);
    return false;
  }

  launcher_page_info->page = launcher_page_page;

  extension->SetManifestData(manifest_keys::kLauncherPage,
                             launcher_page_info.release());
  return true;
}

bool LauncherPageHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  LauncherPageInfo* info = GetInfo(extension);
  const base::FilePath path = extension->GetResource(info->page).GetFilePath();
  if (!base::PathExists(path)) {
    *error = l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_LAUNCHER_PAGE_FAILED,
                                       base::UTF8ToUTF16(info->page));
    return false;
  }

  return true;
}

const std::vector<std::string> LauncherPageHandler::Keys() const {
  return SingleKey(manifest_keys::kLauncherPage);
}

}  // namespace extensions
