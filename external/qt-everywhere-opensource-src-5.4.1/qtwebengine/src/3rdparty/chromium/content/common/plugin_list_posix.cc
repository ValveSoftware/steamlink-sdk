// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/plugin_list.h"

namespace content {

bool PluginList::ReadWebPluginInfo(const base::FilePath& filename,
                                   WebPluginInfo* info) {
  return false;
}

void PluginList::GetPluginDirectories(
    std::vector<base::FilePath>* plugin_dirs) {
}

void PluginList::GetPluginsInDir(const base::FilePath& dir_path,
                                 std::vector<base::FilePath>* plugins) {
}

bool PluginList::ShouldLoadPluginUsingPluginList(
    const WebPluginInfo& info,
    std::vector<WebPluginInfo>* plugins) {
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Considering " << info.path.value() << " (" << info.name << ")";

  if (info.type == WebPluginInfo::PLUGIN_TYPE_NPAPI) {
    NOTREACHED() << "NPAPI plugins are not supported";
    return false;
  }

  VLOG_IF(1, PluginList::DebugPluginLoading()) << "Using " << info.path.value();
  return true;
}

}  // namespace content
