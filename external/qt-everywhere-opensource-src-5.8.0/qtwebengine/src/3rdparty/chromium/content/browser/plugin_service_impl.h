// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_

#if !defined(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ipc/ipc_channel_handle.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

#if defined(OS_POSIX) && !defined(OS_OPENBSD) && !defined(OS_ANDROID)
#include "base/files/file_path_watcher.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class BrowserContext;
class PluginDirWatcherDelegate;
class PluginServiceFilter;
class ResourceContext;
struct PepperPluginInfo;

class CONTENT_EXPORT PluginServiceImpl
    : NON_EXPORTED_BASE(public PluginService) {
 public:
  // Returns the PluginServiceImpl singleton.
  static PluginServiceImpl* GetInstance();

  // PluginService implementation:
  void Init() override;
  bool GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types) override;
  bool GetPluginInfo(int render_process_id,
                     int render_frame_id,
                     ResourceContext* context,
                     const GURL& url,
                     const GURL& page_url,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     WebPluginInfo* info,
                     std::string* actual_mime_type) override;
  bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                           WebPluginInfo* info) override;
  base::string16 GetPluginDisplayNameByPath(
      const base::FilePath& path) override;
  void GetPlugins(const GetPluginsCallback& callback) override;
  PepperPluginInfo* GetRegisteredPpapiPluginInfo(
      const base::FilePath& plugin_path) override;
  void SetFilter(PluginServiceFilter* filter) override;
  PluginServiceFilter* GetFilter() override;
  bool IsPluginUnstable(const base::FilePath& plugin_path) override;
  void RefreshPlugins() override;
  void RegisterInternalPlugin(const WebPluginInfo& info,
                              bool add_at_beginning) override;
  void UnregisterInternalPlugin(const base::FilePath& path) override;
  void GetInternalPlugins(std::vector<WebPluginInfo>* plugins) override;
  bool PpapiDevChannelSupported(BrowserContext* browser_context,
                                const GURL& document_url) override;

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL. Must be called on the IO thread.
  PpapiPluginProcessHost* FindOrStartPpapiPluginProcess(
      int render_process_id,
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory);
  PpapiPluginProcessHost* FindOrStartPpapiBrokerProcess(
      int render_process_id, const base::FilePath& plugin_path);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.  This must be called on the IO thread
  // or else a deadlock can occur.
  void OpenChannelToPpapiPlugin(int render_process_id,
                                const base::FilePath& plugin_path,
                                const base::FilePath& profile_data_directory,
                                PpapiPluginProcessHost::PluginClient* client);
  void OpenChannelToPpapiBroker(int render_process_id,
                                const base::FilePath& path,
                                PpapiPluginProcessHost::BrokerClient* client);

  // Used to monitor plugin stability.
  void RegisterPluginCrash(const base::FilePath& plugin_path);

 private:
  friend struct base::DefaultSingletonTraits<PluginServiceImpl>;

  // Creates the PluginServiceImpl object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginServiceImpl();
  ~PluginServiceImpl() override;

#if defined(OS_WIN)
  void OnKeyChanged(base::win::RegKey* key);
#endif

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PpapiPluginProcessHost* FindPpapiPluginProcess(
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory);
  PpapiPluginProcessHost* FindPpapiBrokerProcess(
      const base::FilePath& broker_path);

  void RegisterPepperPlugins();

  // Run on the blocking pool to load the plugins synchronously.
  void GetPluginsInternal(base::SingleThreadTaskRunner* target_task_runner,
                          const GetPluginsCallback& callback);

  std::vector<PepperPluginInfo> ppapi_plugins_;

  // Weak pointer; outlives us.
  PluginServiceFilter* filter_;

  // Used to sequentialize loading plugins from disk.
  base::SequencedWorkerPool::SequenceToken plugin_list_token_;

  // Used to detect if a given plugin is crashing over and over.
  std::map<base::FilePath, std::vector<base::Time> > crash_times_;

  DISALLOW_COPY_AND_ASSIGN(PluginServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
