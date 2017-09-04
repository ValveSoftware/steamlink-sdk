// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_PLUGINS_PLUGINS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_PLUGINS_PLUGINS_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// An NPAPI plugin included in the extension.
struct PluginInfo {
  typedef std::vector<PluginInfo> PluginVector;

  PluginInfo(const base::FilePath& plugin_path, bool plugin_is_public);
  ~PluginInfo();

  base::FilePath path;  // Path to the plugin.
  bool is_public;  // False if only this extension can load this plugin.

  // Return the plugins for a given |extensions|, or NULL if none exist.
  static const PluginVector* GetPlugins(const Extension* extension);

  // Return true if the given |extension| has plugins, and false otherwise.
  static bool HasPlugins(const Extension* extension);
};

// Parses the "plugins" manifest key.
class PluginsHandler : public ManifestHandler {
 public:
  PluginsHandler();
  ~PluginsHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  const std::vector<std::string> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(PluginsHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_PLUGINS_PLUGINS_HANDLER_H_
