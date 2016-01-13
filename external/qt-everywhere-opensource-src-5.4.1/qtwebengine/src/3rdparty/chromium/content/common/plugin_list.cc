// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/plugin_list.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "content/common/plugin_constants_win.h"
#endif

namespace content {

namespace {

base::LazyInstance<PluginList> g_singleton = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PluginList* PluginList::Singleton() {
  return g_singleton.Pointer();
}

// static
bool PluginList::DebugPluginLoading() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDebugPluginLoading);
}

void PluginList::DisablePluginsDiscovery() {
  plugins_discovery_disabled_ = true;
}

void PluginList::RefreshPlugins() {
  base::AutoLock lock(lock_);
  loading_state_ = LOADING_STATE_NEEDS_REFRESH;
}

void PluginList::AddExtraPluginPath(const base::FilePath& plugin_path) {
  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  base::AutoLock lock(lock_);
  extra_plugin_paths_.push_back(plugin_path);
#endif
}

void PluginList::RemoveExtraPluginPath(const base::FilePath& plugin_path) {
  base::AutoLock lock(lock_);
  RemoveExtraPluginPathLocked(plugin_path);
}

void PluginList::AddExtraPluginDir(const base::FilePath& plugin_dir) {
  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  base::AutoLock lock(lock_);
  extra_plugin_dirs_.push_back(plugin_dir);
#endif
}

void PluginList::RegisterInternalPlugin(const WebPluginInfo& info,
                                        bool add_at_beginning) {
  base::AutoLock lock(lock_);

  internal_plugins_.push_back(info);
  if (add_at_beginning) {
    // Newer registrations go earlier in the list so they can override the MIME
    // types of older registrations.
    extra_plugin_paths_.insert(extra_plugin_paths_.begin(), info.path);
  } else {
    extra_plugin_paths_.push_back(info.path);
  }
}

void PluginList::UnregisterInternalPlugin(const base::FilePath& path) {
  base::AutoLock lock(lock_);
  bool found = false;
  for (size_t i = 0; i < internal_plugins_.size(); i++) {
    if (internal_plugins_[i].path == path) {
      internal_plugins_.erase(internal_plugins_.begin() + i);
      found = true;
      break;
    }
  }
  DCHECK(found);
  RemoveExtraPluginPathLocked(path);
}

void PluginList::GetInternalPlugins(
    std::vector<WebPluginInfo>* internal_plugins) {
  base::AutoLock lock(lock_);

  for (std::vector<WebPluginInfo>::iterator it = internal_plugins_.begin();
       it != internal_plugins_.end();
       ++it) {
    internal_plugins->push_back(*it);
  }
}

bool PluginList::ReadPluginInfo(const base::FilePath& filename,
                                WebPluginInfo* info) {
  {
    base::AutoLock lock(lock_);
    for (size_t i = 0; i < internal_plugins_.size(); ++i) {
      if (filename == internal_plugins_[i].path) {
        *info = internal_plugins_[i];
        return true;
      }
    }
  }

  return PluginList::ReadWebPluginInfo(filename, info);
}

// static
bool PluginList::ParseMimeTypes(
    const std::string& mime_types_str,
    const std::string& file_extensions_str,
    const base::string16& mime_type_descriptions_str,
    std::vector<WebPluginMimeType>* parsed_mime_types) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<base::string16> descriptions;
  base::SplitString(mime_types_str, '|', &mime_types);
  base::SplitString(file_extensions_str, '|', &file_extensions);
  base::SplitString(mime_type_descriptions_str, '|', &descriptions);

  parsed_mime_types->clear();

  if (mime_types.empty())
    return false;

  for (size_t i = 0; i < mime_types.size(); ++i) {
    WebPluginMimeType mime_type;
    mime_type.mime_type = StringToLowerASCII(mime_types[i]);
    if (file_extensions.size() > i)
      base::SplitString(file_extensions[i], ',', &mime_type.file_extensions);

    if (descriptions.size() > i) {
      mime_type.description = descriptions[i];

      // On Windows, the description likely has a list of file extensions
      // embedded in it (e.g. "SurfWriter file (*.swr)"). Remove an extension
      // list from the description if it is present.
      size_t ext = mime_type.description.find(base::ASCIIToUTF16("(*"));
      if (ext != base::string16::npos) {
        if (ext > 1 && mime_type.description[ext - 1] == ' ')
          ext--;

        mime_type.description.erase(ext);
      }
    }

    parsed_mime_types->push_back(mime_type);
  }

  return true;
}

PluginList::PluginList()
    : loading_state_(LOADING_STATE_NEEDS_REFRESH),
      plugins_discovery_disabled_(false) {
}

bool PluginList::PrepareForPluginLoading() {
  base::AutoLock lock(lock_);
  if (loading_state_ == LOADING_STATE_UP_TO_DATE)
    return false;

  loading_state_ = LOADING_STATE_REFRESHING;
  return true;
}

void PluginList::LoadPlugins(bool include_npapi) {
  if (!PrepareForPluginLoading())
    return;

  std::vector<WebPluginInfo> new_plugins;
  base::Closure will_load_callback;
  {
    base::AutoLock lock(lock_);
    will_load_callback = will_load_plugins_callback_;
  }
  if (!will_load_callback.is_null())
    will_load_callback.Run();

  std::vector<base::FilePath> plugin_paths;
  GetPluginPathsToLoad(&plugin_paths, include_npapi);

  for (std::vector<base::FilePath>::const_iterator it = plugin_paths.begin();
       it != plugin_paths.end();
       ++it) {
    WebPluginInfo plugin_info;
    LoadPluginIntoPluginList(*it, &new_plugins, &plugin_info);
  }

  SetPlugins(new_plugins);
}

bool PluginList::LoadPluginIntoPluginList(
    const base::FilePath& path,
    std::vector<WebPluginInfo>* plugins,
    WebPluginInfo* plugin_info) {
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Loading plugin " << path.value();
  if (!ReadPluginInfo(path, plugin_info))
    return false;

  if (!ShouldLoadPluginUsingPluginList(*plugin_info, plugins))
    return false;

#if defined(OS_WIN) && !defined(NDEBUG)
  if (path.BaseName().value() != L"npspy.dll")  // Make an exception for NPSPY
#endif
  {
    for (size_t i = 0; i < plugin_info->mime_types.size(); ++i) {
      // TODO: don't load global handlers for now.
      // WebKit hands to the Plugin before it tries
      // to handle mimeTypes on its own.
      const std::string &mime_type = plugin_info->mime_types[i].mime_type;
      if (mime_type == "*")
        return false;
    }
  }
  plugins->push_back(*plugin_info);
  return true;
}

void PluginList::GetPluginPathsToLoad(std::vector<base::FilePath>* plugin_paths,
                                      bool include_npapi) {
  // Don't want to hold the lock while loading new plugins, so we don't block
  // other methods if they're called on other threads.
  std::vector<base::FilePath> extra_plugin_paths;
  std::vector<base::FilePath> extra_plugin_dirs;
  {
    base::AutoLock lock(lock_);
    extra_plugin_paths = extra_plugin_paths_;
    extra_plugin_dirs = extra_plugin_dirs_;
  }

  for (size_t i = 0; i < extra_plugin_paths.size(); ++i) {
    const base::FilePath& path = extra_plugin_paths[i];
    if (std::find(plugin_paths->begin(), plugin_paths->end(), path) !=
        plugin_paths->end()) {
      continue;
    }
    plugin_paths->push_back(path);
  }

  if (include_npapi) {
    // A bit confusingly, this function is used to load Pepper plugins as well.
    // Those are all internal plugins so we have to use extra_plugin_paths.
    for (size_t i = 0; i < extra_plugin_dirs.size(); ++i)
      GetPluginsInDir(extra_plugin_dirs[i], plugin_paths);

    std::vector<base::FilePath> directories_to_scan;
    GetPluginDirectories(&directories_to_scan);
    for (size_t i = 0; i < directories_to_scan.size(); ++i)
      GetPluginsInDir(directories_to_scan[i], plugin_paths);

#if defined(OS_WIN)
    GetPluginPathsFromRegistry(plugin_paths);
#endif
  }
}

void PluginList::SetPlugins(const std::vector<WebPluginInfo>& plugins) {
  base::AutoLock lock(lock_);

  // If we haven't been invalidated in the mean time, mark the plug-in list as
  // up-to-date.
  if (loading_state_ != LOADING_STATE_NEEDS_REFRESH)
    loading_state_ = LOADING_STATE_UP_TO_DATE;

  plugins_list_ = plugins;
}

void PluginList::set_will_load_plugins_callback(const base::Closure& callback) {
  base::AutoLock lock(lock_);
  will_load_plugins_callback_ = callback;
}

void PluginList::GetPlugins(std::vector<WebPluginInfo>* plugins,
                            bool include_npapi) {
  LoadPlugins(include_npapi);
  base::AutoLock lock(lock_);
  plugins->insert(plugins->end(), plugins_list_.begin(), plugins_list_.end());
}

bool PluginList::GetPluginsNoRefresh(std::vector<WebPluginInfo>* plugins) {
  base::AutoLock lock(lock_);
  plugins->insert(plugins->end(), plugins_list_.begin(), plugins_list_.end());

  return loading_state_ == LOADING_STATE_UP_TO_DATE;
}

void PluginList::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    bool* use_stale,
    bool include_npapi,
    std::vector<WebPluginInfo>* info,
    std::vector<std::string>* actual_mime_types) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));
  DCHECK(info);

  if (!use_stale)
    LoadPlugins(include_npapi);
  base::AutoLock lock(lock_);
  if (use_stale)
    *use_stale = (loading_state_ != LOADING_STATE_UP_TO_DATE);
  info->clear();
  if (actual_mime_types)
    actual_mime_types->clear();

  std::set<base::FilePath> visited_plugins;

  // Add in plugins by mime type.
  for (size_t i = 0; i < plugins_list_.size(); ++i) {
    if (SupportsType(plugins_list_[i], mime_type, allow_wildcard)) {
      base::FilePath path = plugins_list_[i].path;
      if (visited_plugins.insert(path).second) {
        info->push_back(plugins_list_[i]);
        if (actual_mime_types)
          actual_mime_types->push_back(mime_type);
      }
    }
  }

  // Add in plugins by url.
  // We do not permit URL-sniff based plug-in MIME type overrides aside from
  // the case where the "type" was initially missing.
  // We collected stats to determine this approach isn't a major compat issue,
  // and we defend against content confusion attacks in various cases, such
  // as when the user doesn't have the Flash plug-in enabled.
  std::string path = url.path();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot != std::string::npos && mime_type.empty()) {
    std::string extension = StringToLowerASCII(std::string(path, last_dot+1));
    std::string actual_mime_type;
    for (size_t i = 0; i < plugins_list_.size(); ++i) {
      if (SupportsExtension(plugins_list_[i], extension, &actual_mime_type)) {
        base::FilePath path = plugins_list_[i].path;
        if (visited_plugins.insert(path).second) {
          info->push_back(plugins_list_[i]);
          if (actual_mime_types)
            actual_mime_types->push_back(actual_mime_type);
        }
      }
    }
  }
}

bool PluginList::SupportsType(const WebPluginInfo& plugin,
                              const std::string& mime_type,
                              bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_info = plugin.mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (!allow_wildcard && mime_info.mime_type == "*")
        continue;
      return true;
    }
  }
  return false;
}

bool PluginList::SupportsExtension(const WebPluginInfo& plugin,
                                   const std::string& extension,
                                   std::string* actual_mime_type) {
  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_type = plugin.mime_types[i];
    for (size_t j = 0; j < mime_type.file_extensions.size(); ++j) {
      if (mime_type.file_extensions[j] == extension) {
        if (actual_mime_type)
          *actual_mime_type = mime_type.mime_type;
        return true;
      }
    }
  }
  return false;
}

void PluginList::RemoveExtraPluginPathLocked(
    const base::FilePath& plugin_path) {
  lock_.AssertAcquired();
  std::vector<base::FilePath>::iterator it =
      std::find(extra_plugin_paths_.begin(), extra_plugin_paths_.end(),
                plugin_path);
  if (it != extra_plugin_paths_.end())
    extra_plugin_paths_.erase(it);
}

PluginList::~PluginList() {
}


}  // namespace content
