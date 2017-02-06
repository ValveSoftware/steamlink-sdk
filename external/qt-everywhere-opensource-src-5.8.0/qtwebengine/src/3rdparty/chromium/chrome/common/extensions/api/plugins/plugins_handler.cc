// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/plugins/plugins_handler.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = manifest_keys;

namespace {

struct PluginManifestData : Extension::ManifestData {
  // Optional list of NPAPI plugins and associated properties for an extension.
  PluginInfo::PluginVector plugins;
};

}  // namespace

PluginInfo::PluginInfo(const base::FilePath& plugin_path, bool plugin_is_public)
    : path(plugin_path), is_public(plugin_is_public) {
}

PluginInfo::~PluginInfo() {
}

// static
const PluginInfo::PluginVector* PluginInfo::GetPlugins(
    const Extension* extension) {
  PluginManifestData* data = static_cast<PluginManifestData*>(
      extension->GetManifestData(keys::kPlugins));
  return data ? &data->plugins : NULL;
}

// static
bool PluginInfo::HasPlugins(const Extension* extension) {
  PluginManifestData* data = static_cast<PluginManifestData*>(
      extension->GetManifestData(keys::kPlugins));
  return data && !data->plugins.empty() ? true : false;
}

PluginsHandler::PluginsHandler() {
}

PluginsHandler::~PluginsHandler() {
}

const std::vector<std::string> PluginsHandler::Keys() const {
  return SingleKey(keys::kPlugins);
}

bool PluginsHandler::Parse(Extension* extension, base::string16* error) {
  const base::ListValue* list_value = NULL;
  if (!extension->manifest()->GetList(keys::kPlugins, &list_value)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidPlugins);
    return false;
  }

  std::unique_ptr<PluginManifestData> plugins_data(new PluginManifestData);

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    const base::DictionaryValue* plugin_value = NULL;
    if (!list_value->GetDictionary(i, &plugin_value)) {
      *error = base::ASCIIToUTF16(manifest_errors::kInvalidPlugins);
      return false;
    }
    // Get plugins[i].path.
    std::string path_str;
    if (!plugin_value->GetString(keys::kPluginsPath, &path_str)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidPluginsPath, base::SizeTToString(i));
      return false;
    }

    // Get plugins[i].content (optional).
    bool is_public = false;
    if (plugin_value->HasKey(keys::kPluginsPublic)) {
      if (!plugin_value->GetBoolean(keys::kPluginsPublic, &is_public)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidPluginsPublic, base::SizeTToString(i));
        return false;
      }
    }

    // We don't allow extensions to load NPAPI plugins on Chrome OS, but still
    // parse the entries to display consistent error messages. If the extension
    // actually requires the plugins then LoadRequirements will prevent it
    // loading.
#if defined(OS_CHROMEOS)
    continue;
#endif  // defined(OS_CHROMEOS).
    plugins_data->plugins.push_back(PluginInfo(
        extension->path().Append(base::FilePath::FromUTF8Unsafe(path_str)),
        is_public));
  }

  if (!plugins_data->plugins.empty()) {
    extension->SetManifestData(keys::kPlugins, plugins_data.release());
    PermissionsParser::AddAPIPermission(extension, APIPermission::kPlugin);
  }

  return true;
}

bool PluginsHandler::Validate(const Extension* extension,
                              std::string* error,
                              std::vector<InstallWarning>* warnings) const {
  // Validate claimed plugin paths.
  if (extensions::PluginInfo::HasPlugins(extension)) {
    const extensions::PluginInfo::PluginVector* plugins =
        extensions::PluginInfo::GetPlugins(extension);
    CHECK(plugins);
    for (std::vector<extensions::PluginInfo>::const_iterator plugin =
             plugins->begin();
         plugin != plugins->end(); ++plugin) {
      if (!base::PathExists(plugin->path)) {
        *error = l10n_util::GetStringFUTF8(
            IDS_EXTENSION_LOAD_PLUGIN_PATH_FAILED,
            plugin->path.LossyDisplayName());
      return false;
      }
    }
  }
  return true;
}

}  // namespace extensions
