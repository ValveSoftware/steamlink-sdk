// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_PLUGIN_LIB_H_
#define CONTENT_CHILD_NPAPI_PLUGIN_LIB_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "content/child/npapi/webplugin.h"
#include "content/common/content_export.h"
#include "content/public/common/webplugininfo.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace base {
class FilePath;
}

namespace content {

class PluginInstance;

// This struct holds entry points into a plugin.  The entry points are
// slightly different between Win/Mac and Unixes.  Note that the interface for
// querying plugins is synchronous and it is preferable to use a higher-level
// asynchronous information to query information.
struct PluginEntryPoints {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  NP_GetEntryPointsFunc np_getentrypoints;
#endif
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
};

// A PluginLib is a single NPAPI Plugin Library, and is the lifecycle
// manager for new PluginInstances.
class CONTENT_EXPORT PluginLib : public base::RefCounted<PluginLib> {
 public:
  static PluginLib* CreatePluginLib(const base::FilePath& filename);

  // Unloads all the loaded plugin libraries and cleans up the plugin map.
  static void UnloadAllPlugins();

  // Shuts down all loaded plugin instances.
  static void ShutdownAllPlugins();

  // Get the Plugin's function pointer table.
  NPPluginFuncs* functions();

  // Creates a new instance of this plugin.
  PluginInstance* CreateInstance(const std::string& mime_type);

  // Called by the instance when the instance is tearing down.
  void CloseInstance();

  // Gets information about this plugin and the mime types that it
  // supports.
  const WebPluginInfo& plugin_info() { return web_plugin_info_; }

  //
  // NPAPI functions
  //

  // NPAPI method to initialize a Plugin.
  // Initialize can be safely called multiple times
  NPError NP_Initialize();

  // NPAPI method to shutdown a Plugin.
  void NP_Shutdown(void);

  // NPAPI method to clear locally stored data (LSO's or "Flash cookies").
  NPError NP_ClearSiteData(const char* site, uint64 flags, uint64 max_age);

  // NPAPI method to get a NULL-terminated list of all sites under which data
  // is stored.
  char** NP_GetSitesWithData();

  int instance_count() const { return instance_count_; }

  // Prevents the library code from being unload when Unload() is called (since
  // some plugins crash if unloaded).
  void PreventLibraryUnload();

  // Indicates whether plugin unload can be deferred.
  void set_defer_unload(bool defer_unload) {
    defer_unload_ = defer_unload;
  }

  // protected for testability.
 protected:
  friend class base::RefCounted<PluginLib>;

  // Creates a new PluginLib.
  explicit PluginLib(const WebPluginInfo& info);

  virtual ~PluginLib();

  // Attempts to load the plugin from the library.
  // Returns true if it is a legitimate plugin, false otherwise
  bool Load();

  // Unloads the plugin library.
  void Unload();

  // Shutdown the plugin library.
  void Shutdown();

 private:
  WebPluginInfo web_plugin_info_;  // Supported mime types, description
  base::NativeLibrary library_;  // The opened library reference.
  NPPluginFuncs plugin_funcs_;  // The struct of plugin side functions.
  bool initialized_;  // Is the plugin initialized?
  NPSavedData *saved_data_;  // Persisted plugin info for NPAPI.
  int instance_count_;  // Count of plugins in use.
  bool skip_unload_;  // True if library_ should not be unloaded.

  // Function pointers to entry points into the plugin.
  PluginEntryPoints entry_points_;

  // Set to true if unloading of the plugin dll is to be deferred.
  bool defer_unload_;

  DISALLOW_COPY_AND_ASSIGN(PluginLib);
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_PLUGIN_LIB_H_
