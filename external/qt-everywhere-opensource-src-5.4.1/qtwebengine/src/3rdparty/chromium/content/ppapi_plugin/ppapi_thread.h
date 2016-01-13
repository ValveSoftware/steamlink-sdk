// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "content/child/child_thread.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/trusted/ppp_broker.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {
class CommandLine;
class FilePath;
}

namespace IPC {
struct ChannelHandle;
}

namespace content {

class PpapiWebKitPlatformSupportImpl;

class PpapiThread : public ChildThread,
                    public ppapi::proxy::PluginDispatcher::PluginDelegate,
                    public ppapi::proxy::PluginProxyDelegate {
 public:
  PpapiThread(const base::CommandLine& command_line, bool is_broker);
  virtual ~PpapiThread();
  virtual void Shutdown() OVERRIDE;

 private:
  // Make sure the enum list in tools/histogram/histograms.xml is updated with
  // any change in this list.
  enum LoadResult {
    LOAD_SUCCESS,
    LOAD_FAILED,
    ENTRY_POINT_MISSING,
    INIT_FAILED,
    FILE_MISSING,
    // NOTE: Add new values only immediately above this line.
    LOAD_RESULT_MAX  // Boundary value for UMA_HISTOGRAM_ENUMERATION.
  };

  // ChildThread overrides.
  virtual bool Send(IPC::Message* msg) OVERRIDE;
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // PluginDispatcher::PluginDelegate implementation.
  virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() OVERRIDE;
  virtual base::MessageLoopProxy* GetIPCMessageLoop() OVERRIDE;
  virtual base::WaitableEvent* GetShutdownEvent() OVERRIDE;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId peer_pid,
      bool should_close_source) OVERRIDE;
  virtual uint32 Register(
      ppapi::proxy::PluginDispatcher* plugin_dispatcher) OVERRIDE;
  virtual void Unregister(uint32 plugin_dispatcher_id) OVERRIDE;

  // PluginProxyDelegate.
  // SendToBrowser() is intended to be safe to use on another thread so
  // long as the main PpapiThread outlives it.
  virtual IPC::Sender* GetBrowserSender() OVERRIDE;
  virtual std::string GetUILanguage() OVERRIDE;
  virtual void PreCacheFont(const void* logfontw) OVERRIDE;
  virtual void SetActiveURL(const std::string& url) OVERRIDE;
  virtual PP_Resource CreateBrowserFont(
      ppapi::proxy::Connection connection,
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description& desc,
      const ppapi::Preferences& prefs) OVERRIDE;

  // Message handlers.
  void OnLoadPlugin(const base::FilePath& path,
                    const ppapi::PpapiPermissions& permissions);
  void OnCreateChannel(base::ProcessId renderer_pid,
                       int renderer_child_id,
                       bool incognito);
  void OnSetNetworkState(bool online);
  void OnCrash();
  void OnHang();

  // Sets up the channel to the given renderer. On success, returns true and
  // fills the given ChannelHandle with the information from the new channel.
  bool SetupRendererChannel(base::ProcessId renderer_pid,
                            int renderer_child_id,
                            bool incognito,
                            IPC::ChannelHandle* handle);

  // Sets up the name of the plugin for logging using the given path.
  void SavePluginName(const base::FilePath& path);

  void ReportLoadResult(const base::FilePath& path, LoadResult result);

  // Reports |error| to UMA when plugin load fails.
  void ReportLoadErrorCode(const base::FilePath& path,
                           const base::NativeLibraryLoadError& error);

  // True if running in a broker process rather than a normal plugin process.
  bool is_broker_;

  base::ScopedNativeLibrary library_;

  ppapi::PpapiPermissions permissions_;

  // Global state tracking for the proxy.
  ppapi::proxy::PluginGlobals plugin_globals_;

  // Storage for plugin entry points.
  PepperPluginInfo::EntryPoints plugin_entry_points_;

  // Callback to call when a new instance connects to the broker.
  // Used only when is_broker_.
  PP_ConnectInstance_Func connect_instance_func_;

  // Local concept of the module ID. Some functions take this. It's necessary
  // for the in-process PPAPI to handle this properly, but for proxied it's
  // unnecessary. The proxy talking to multiple renderers means that each
  // renderer has a different idea of what the module ID is for this plugin.
  // To force people to "do the right thing" we generate a random module ID
  // and pass it around as necessary.
  PP_Module local_pp_module_;

  // See Dispatcher::Delegate::GetGloballySeenInstanceIDSet.
  std::set<PP_Instance> globally_seen_instance_ids_;

  // The PluginDispatcher instances contained in the map are not owned by it.
  std::map<uint32, ppapi::proxy::PluginDispatcher*> plugin_dispatchers_;
  uint32 next_plugin_dispatcher_id_;

  // The WebKitPlatformSupport implementation.
  scoped_ptr<PpapiWebKitPlatformSupportImpl> webkit_platform_support_;

#if defined(OS_WIN)
  // Caches the handle to the peer process if this is a broker.
  base::win::ScopedHandle peer_handle_;
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(PpapiThread);
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
