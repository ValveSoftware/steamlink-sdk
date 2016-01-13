// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_lib.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/strings/string_util.h"
#include "content/child/npapi/plugin_host.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/common/plugin_list.h"

namespace content {

const char kPluginLibrariesLoadedCounter[] = "PluginLibrariesLoaded";
const char kPluginInstancesActiveCounter[] = "PluginInstancesActive";

// A list of all the instantiated plugins.
static std::vector<scoped_refptr<PluginLib> >* g_loaded_libs;

PluginLib* PluginLib::CreatePluginLib(const base::FilePath& filename) {
  // We can only have one PluginLib object per plugin as it controls the per
  // instance function calls (i.e. NP_Initialize and NP_Shutdown).  So we keep
  // a map of PluginLib objects.
  if (!g_loaded_libs)
    g_loaded_libs = new std::vector<scoped_refptr<PluginLib> >;

  for (size_t i = 0; i < g_loaded_libs->size(); ++i) {
    if ((*g_loaded_libs)[i]->plugin_info().path == filename)
      return (*g_loaded_libs)[i].get();
  }

  WebPluginInfo info;
  if (!PluginList::Singleton()->ReadPluginInfo(filename, &info))
    return NULL;

  return new PluginLib(info);
}

void PluginLib::UnloadAllPlugins() {
  if (g_loaded_libs) {
    // PluginLib::Unload() can remove items from the list and even delete
    // the list when it removes the last item, so we must work with a copy
    // of the list so that we don't get the carpet removed under our feet.
    std::vector<scoped_refptr<PluginLib> > loaded_libs(*g_loaded_libs);
    for (size_t i = 0; i < loaded_libs.size(); ++i)
      loaded_libs[i]->Unload();

    if (g_loaded_libs && g_loaded_libs->empty()) {
      delete g_loaded_libs;
      g_loaded_libs = NULL;
    }
  }
}

void PluginLib::ShutdownAllPlugins() {
  if (g_loaded_libs) {
    for (size_t i = 0; i < g_loaded_libs->size(); ++i)
      (*g_loaded_libs)[i]->Shutdown();
  }
}

PluginLib::PluginLib(const WebPluginInfo& info)
    : web_plugin_info_(info),
      library_(NULL),
      initialized_(false),
      saved_data_(0),
      instance_count_(0),
      skip_unload_(false),
      defer_unload_(false) {
  base::StatsCounter(kPluginLibrariesLoadedCounter).Increment();
  memset(static_cast<void*>(&plugin_funcs_), 0, sizeof(plugin_funcs_));
  g_loaded_libs->push_back(make_scoped_refptr(this));

  memset(&entry_points_, 0, sizeof(entry_points_));
}

PluginLib::~PluginLib() {
  base::StatsCounter(kPluginLibrariesLoadedCounter).Decrement();
  if (saved_data_ != 0) {
    // TODO - delete the savedData object here
  }
}

NPPluginFuncs* PluginLib::functions() {
  return &plugin_funcs_;
}

NPError PluginLib::NP_Initialize() {
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "PluginLib::NP_Initialize(" << web_plugin_info_.path.value()
      << "): initialized=" << initialized_;
  if (initialized_)
    return NPERR_NO_ERROR;

  if (!Load())
    return NPERR_MODULE_LOAD_FAILED_ERROR;

  PluginHost* host = PluginHost::Singleton();
  if (host == 0)
    return NPERR_GENERIC_ERROR;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  NPError rv = entry_points_.np_initialize(host->host_functions(),
                                           &plugin_funcs_);
#else
  NPError rv = entry_points_.np_initialize(host->host_functions());
#if defined(OS_MACOSX)
  // On the Mac, we need to get entry points after calling np_initialize to
  // match the behavior of other browsers.
  if (rv == NPERR_NO_ERROR) {
    rv = entry_points_.np_getentrypoints(&plugin_funcs_);
  }
#endif  // OS_MACOSX
#endif
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "PluginLib::NP_Initialize(" << web_plugin_info_.path.value()
      << "): result=" << rv;
  initialized_ = (rv == NPERR_NO_ERROR);
  return rv;
}

void PluginLib::NP_Shutdown(void) {
  DCHECK(initialized_);
  entry_points_.np_shutdown();
}

NPError PluginLib::NP_ClearSiteData(const char* site,
                                    uint64 flags,
                                    uint64 max_age) {
  DCHECK(initialized_);
  if (plugin_funcs_.clearsitedata)
    return plugin_funcs_.clearsitedata(site, flags, max_age);
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

char** PluginLib::NP_GetSitesWithData() {
  DCHECK(initialized_);
  if (plugin_funcs_.getsiteswithdata)
    return plugin_funcs_.getsiteswithdata();
  return NULL;
}

void PluginLib::PreventLibraryUnload() {
  skip_unload_ = true;
}

PluginInstance* PluginLib::CreateInstance(const std::string& mime_type) {
  PluginInstance* new_instance = new PluginInstance(this, mime_type);
  instance_count_++;
  base::StatsCounter(kPluginInstancesActiveCounter).Increment();
  DCHECK_NE(static_cast<PluginInstance*>(NULL), new_instance);
  return new_instance;
}

void PluginLib::CloseInstance() {
  base::StatsCounter(kPluginInstancesActiveCounter).Decrement();
  instance_count_--;
  // If a plugin is running in its own process it will get unloaded on process
  // shutdown.
  if ((instance_count_ == 0) && !defer_unload_)
    Unload();
}

bool PluginLib::Load() {
  if (library_)
    return true;

  bool rv = false;
  base::NativeLibrary library = 0;
  base::NativeLibraryLoadError error;

#if defined(OS_WIN)
  // This is to work around a bug in the Real player recorder plugin which
  // intercepts LoadLibrary calls from chrome.dll and wraps NPAPI functions
  // provided by the plugin. It crashes if the media player plugin is being
  // loaded. Workaround is to load the dll dynamically by getting the
  // LoadLibrary API address from kernel32.dll which bypasses the recorder
  // plugin.
  if (web_plugin_info_.name.find(L"Windows Media Player") !=
      std::wstring::npos) {
    library = base::LoadNativeLibraryDynamically(web_plugin_info_.path);
  } else {
    library = base::LoadNativeLibrary(web_plugin_info_.path, &error);
  }
#else
  library = base::LoadNativeLibrary(web_plugin_info_.path, &error);
#endif

  if (!library) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Couldn't load plugin " << web_plugin_info_.path.value() << " "
        << error.ToString();
    return rv;
  }

#if defined(OS_MACOSX)
  // According to the WebKit source, QuickTime at least requires us to call
  // UseResFile on the plugin resources before loading.
  if (library->bundle_resource_ref != -1)
    UseResFile(library->bundle_resource_ref);
#endif

  rv = true;  // assume success now

  entry_points_.np_initialize =
      (NP_InitializeFunc)base::GetFunctionPointerFromNativeLibrary(library,
          "NP_Initialize");
  if (entry_points_.np_initialize == 0)
    rv = false;

#if defined(OS_WIN) || defined(OS_MACOSX)
  entry_points_.np_getentrypoints =
      (NP_GetEntryPointsFunc)base::GetFunctionPointerFromNativeLibrary(
          library, "NP_GetEntryPoints");
  if (entry_points_.np_getentrypoints == 0)
    rv = false;
#endif

  entry_points_.np_shutdown =
      (NP_ShutdownFunc)base::GetFunctionPointerFromNativeLibrary(library,
          "NP_Shutdown");
  if (entry_points_.np_shutdown == 0)
    rv = false;

  if (rv) {
    plugin_funcs_.size = sizeof(plugin_funcs_);
    plugin_funcs_.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
#if !defined(OS_POSIX)
    if (entry_points_.np_getentrypoints(&plugin_funcs_) != NPERR_NO_ERROR)
      rv = false;
#else
    // On Linux and Mac, we get the plugin entry points during NP_Initialize.
#endif
  }

  if (rv) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << web_plugin_info_.path.value()
        << " loaded successfully.";
    library_ = library;
  } else {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << web_plugin_info_.path.value()
        << " failed to load, unloading.";
    base::UnloadNativeLibrary(library);
  }

  return rv;
}

// This is a helper to help perform a delayed NP_Shutdown and FreeLibrary on the
// plugin dll.
void FreePluginLibraryHelper(const base::FilePath& path,
                             base::NativeLibrary library,
                             NP_ShutdownFunc shutdown_func) {
  if (shutdown_func) {
    // Don't call NP_Shutdown if the library has been reloaded since this task
    // was posted.
    bool reloaded = false;
    if (g_loaded_libs) {
      for (size_t i = 0; i < g_loaded_libs->size(); ++i) {
        if ((*g_loaded_libs)[i]->plugin_info().path == path) {
          reloaded = true;
          break;
        }
      }
    }
    if (!reloaded)
      shutdown_func();
  }

  if (library) {
    // Always call base::UnloadNativeLibrary so that the system reference
    // count is decremented.
    base::UnloadNativeLibrary(library);
  }
}

void PluginLib::Unload() {
  if (library_) {
    // In case of single process mode, a plugin can delete itself
    // by executing a script. So delay the unloading of the library
    // so that the plugin will have a chance to unwind.
/* TODO(dglazkov): Revisit when re-enabling the JSC build.
#if USE(JSC)
    // The plugin NPAPI instances may still be around. Delay the
    // NP_Shutdown and FreeLibrary calls at least till the next
    // peek message.
    defer_unload = true;
#endif
*/
    if (!defer_unload_) {
      LOG_IF(ERROR, PluginList::DebugPluginLoading())
          << "Scheduling delayed unload for plugin "
          << web_plugin_info_.path.value();
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&FreePluginLibraryHelper,
                     web_plugin_info_.path,
                     skip_unload_ ? NULL : library_,
                     entry_points_.np_shutdown));
    } else {
      Shutdown();
      if (!skip_unload_) {
        LOG_IF(ERROR, PluginList::DebugPluginLoading())
            << "Unloading plugin " << web_plugin_info_.path.value();
        base::UnloadNativeLibrary(library_);
      }
    }

    library_ = NULL;
  }

  for (size_t i = 0; i < g_loaded_libs->size(); ++i) {
    if ((*g_loaded_libs)[i].get() == this) {
      g_loaded_libs->erase(g_loaded_libs->begin() + i);
      break;
    }
  }
  if (g_loaded_libs->empty()) {
    delete g_loaded_libs;
    g_loaded_libs = NULL;
  }
}

void PluginLib::Shutdown() {
  if (initialized_) {
    NP_Shutdown();
    initialized_ = false;
  }
}

}  // namespace content
