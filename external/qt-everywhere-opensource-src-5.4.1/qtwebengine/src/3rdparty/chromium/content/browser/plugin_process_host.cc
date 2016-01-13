// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_process_host.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <utility>  // for pair<>
#endif

#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/plugin_service_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/plugin_process_messages.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "content/common/plugin_carbon_interpose_constants_mac.h"
#include "ui/gfx/rect.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/common/plugin_constants_win.h"
#endif

namespace content {

#if defined(OS_WIN)
void PluginProcessHost::OnPluginWindowDestroyed(HWND window, HWND parent) {
  // The window is destroyed at this point, we just care about its parent, which
  // is the intermediate window we created.
  std::set<HWND>::iterator window_index =
      plugin_parent_windows_set_.find(parent);
  if (window_index == plugin_parent_windows_set_.end())
    return;

  plugin_parent_windows_set_.erase(window_index);
  PostMessage(parent, WM_CLOSE, 0, 0);
}

void PluginProcessHost::AddWindow(HWND window) {
  plugin_parent_windows_set_.insert(window);
}
#endif  // defined(OS_WIN)

// NOTE: changes to this class need to be reviewed by the security team.
class PluginSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  explicit PluginSandboxedProcessLauncherDelegate(ChildProcessHost* host)
#if defined(OS_POSIX)
      : ipc_fd_(host->TakeClientFileDescriptor())
#endif  // OS_POSIX
  {}

  virtual ~PluginSandboxedProcessLauncherDelegate() {}

#if defined(OS_WIN)
  virtual bool ShouldSandbox() OVERRIDE {
    return false;
  }

#elif defined(OS_POSIX)
  virtual int GetIpcFd() OVERRIDE {
    return ipc_fd_;
  }
#endif  // OS_WIN

 private:
#if defined(OS_POSIX)
  int ipc_fd_;
#endif  // OS_POSIX

  DISALLOW_COPY_AND_ASSIGN(PluginSandboxedProcessLauncherDelegate);
};

PluginProcessHost::PluginProcessHost()
#if defined(OS_MACOSX)
    : plugin_cursor_visible_(true)
#endif
{
  process_.reset(new BrowserChildProcessHostImpl(PROCESS_TYPE_PLUGIN, this));
}

PluginProcessHost::~PluginProcessHost() {
#if defined(OS_WIN)
  // We erase HWNDs from the plugin_parent_windows_set_ when we receive a
  // notification that the window is being destroyed. If we don't receive this
  // notification and the PluginProcessHost instance is being destroyed, it
  // means that the plugin process crashed. We paint a sad face in this case in
  // the renderer process. To ensure that the sad face shows up, and we don't
  // leak HWNDs, we should destroy existing plugin parent windows.
  std::set<HWND>::iterator window_index;
  for (window_index = plugin_parent_windows_set_.begin();
       window_index != plugin_parent_windows_set_.end();
       ++window_index) {
    PostMessage(*window_index, WM_CLOSE, 0, 0);
  }
#elif defined(OS_MACOSX)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // If the plugin process crashed but had fullscreen windows open at the time,
  // make sure that the menu bar is visible.
  for (size_t i = 0; i < plugin_fullscreen_windows_set_.size(); ++i) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(base::mac::ReleaseFullScreen,
                                       base::mac::kFullScreenModeHideAll));
  }
  // If the plugin hid the cursor, reset that.
  if (!plugin_cursor_visible_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(base::mac::SetCursorVisibility, true));
  }
#endif
  // Cancel all pending and sent requests.
  CancelRequests();
}

bool PluginProcessHost::Send(IPC::Message* message) {
  return process_->Send(message);
}

bool PluginProcessHost::Init(const WebPluginInfo& info) {
  info_ = info;
  process_->SetName(info_.name);

  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

  // Build command line for plugin. When we have a plugin launcher, we can't
  // allow "self" on linux and we need the real file path.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPluginLauncher);

#if defined(OS_MACOSX)
  // Run the plug-in process in a mode tolerant of heap execution without
  // explicit mprotect calls. Some plug-ins still rely on this quaint and
  // archaic "feature." See http://crbug.com/93551.
  int flags = ChildProcessHost::CHILD_ALLOW_HEAP_EXECUTION;
#elif defined(OS_LINUX)
  int flags = plugin_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                        ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  // Put the process type and plugin path first so they're easier to see
  // in process listings using native process management tools.
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kPluginProcess);
  cmd_line->AppendSwitchPath(switches::kPluginPath, info.path);

  // Propagate the following switches to the plugin command line (along with
  // any associated values) if present in the browser command line
  static const char* const kSwitchNames[] = {
    switches::kDisableBreakpad,
    switches::kDisableDirectNPAPIRequests,
    switches::kEnableStatsTable,
    switches::kFullMemoryCrashReport,
    switches::kLoggingLevel,
    switches::kLogPluginMessages,
    switches::kNoSandbox,
    switches::kPluginStartupDialog,
    switches::kTraceStartup,
    switches::kUseGL,
    switches::kForceDeviceScaleFactor,
#if defined(OS_MACOSX)
    switches::kDisableCoreAnimationPlugins,
    switches::kEnableSandboxLogging,
#endif
  };

  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));

  GpuDataManagerImpl::GetInstance()->AppendPluginCommandLine(cmd_line);

  // If specified, prepend a launcher program to the command line.
  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  if (!locale.empty()) {
    // Pass on the locale so the null plugin will use the right language in the
    // prompt to install the desired plugin.
    cmd_line->AppendSwitchASCII(switches::kLang, locale);
  }

  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);

#if defined(OS_POSIX)
  base::EnvironmentMap env;
#if defined(OS_MACOSX) && !defined(__LP64__)
  if (browser_command_line.HasSwitch(switches::kEnableCarbonInterposing)) {
    std::string interpose_list = GetContentClient()->GetCarbonInterposePath();
    if (!interpose_list.empty()) {
      // Add our interposing library for Carbon. This is stripped back out in
      // plugin_main.cc, so changes here should be reflected there.
      const char* existing_list = getenv(kDYLDInsertLibrariesKey);
      if (existing_list) {
        interpose_list.insert(0, ":");
        interpose_list.insert(0, existing_list);
      }
    }
    env[kDYLDInsertLibrariesKey] = interpose_list;
  }
#endif
#endif

  process_->Launch(
      new PluginSandboxedProcessLauncherDelegate(process_->GetHost()),
      cmd_line);

  // The plugin needs to be shutdown gracefully, i.e. NP_Shutdown needs to be
  // called on the plugin. The plugin process exits when it receives the
  // OnChannelError notification indicating that the browser plugin channel has
  // been destroyed.
  process_->SetTerminateChildOnShutdown(false);

  ResourceMessageFilter::GetContextsCallback get_contexts_callback(
      base::Bind(&PluginProcessHost::GetContexts,
      base::Unretained(this)));

  // TODO(jam): right now we're passing NULL for appcache, blob storage, and
  // file system. If NPAPI plugins actually use this, we'll have to plumb them.
  ResourceMessageFilter* resource_message_filter = new ResourceMessageFilter(
      process_->GetData().id, PROCESS_TYPE_PLUGIN, NULL, NULL, NULL, NULL,
      get_contexts_callback);
  process_->AddFilter(resource_message_filter);
  return true;
}

void PluginProcessHost::ForceShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Send(new PluginProcessMsg_NotifyRenderersOfPendingShutdown());
  process_->ForceShutdown();
}

bool PluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ChannelCreated, OnChannelCreated)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ChannelDestroyed,
                        OnChannelDestroyed)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginWindowDestroyed,
                        OnPluginWindowDestroyed)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginSelectWindow,
                        OnPluginSelectWindow)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginShowWindow,
                        OnPluginShowWindow)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginHideWindow,
                        OnPluginHideWindow)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginSetCursorVisibility,
                        OnPluginSetCursorVisibility)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PluginProcessHost::OnChannelConnected(int32 peer_pid) {
  for (size_t i = 0; i < pending_requests_.size(); ++i) {
    RequestPluginChannel(pending_requests_[i]);
  }

  pending_requests_.clear();
}

void PluginProcessHost::OnChannelError() {
  CancelRequests();
}

bool PluginProcessHost::CanShutdown() {
  return sent_requests_.empty();
}

void PluginProcessHost::OnProcessCrashed(int exit_code) {
  PluginServiceImpl::GetInstance()->RegisterPluginCrash(info_.path);
}

void PluginProcessHost::CancelRequests() {
  for (size_t i = 0; i < pending_requests_.size(); ++i)
    pending_requests_[i]->OnError();
  pending_requests_.clear();

  while (!sent_requests_.empty()) {
    Client* client = sent_requests_.front();
    if (client)
      client->OnError();
    sent_requests_.pop_front();
  }
}

void PluginProcessHost::OpenChannelToPlugin(Client* client) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowserChildProcessHostImpl::NotifyProcessInstanceCreated,
                 process_->GetData()));
  client->SetPluginInfo(info_);
  if (process_->GetHost()->IsChannelOpening()) {
    // The channel is already in the process of being opened.  Put
    // this "open channel" request into a queue of requests that will
    // be run once the channel is open.
    pending_requests_.push_back(client);
    return;
  }

  // We already have an open channel, send a request right away to plugin.
  RequestPluginChannel(client);
}

void PluginProcessHost::CancelPendingRequest(Client* client) {
  std::vector<Client*>::iterator it = pending_requests_.begin();
  while (it != pending_requests_.end()) {
    if (client == *it) {
      pending_requests_.erase(it);
      return;
    }
    ++it;
  }
  DCHECK(it != pending_requests_.end());
}

void PluginProcessHost::CancelSentRequest(Client* client) {
  std::list<Client*>::iterator it = sent_requests_.begin();
  while (it != sent_requests_.end()) {
    if (client == *it) {
      *it = NULL;
      return;
    }
    ++it;
  }
  DCHECK(it != sent_requests_.end());
}

void PluginProcessHost::RequestPluginChannel(Client* client) {
  // We can't send any sync messages from the browser because it might lead to
  // a hang.  However this async messages must be answered right away by the
  // plugin process (i.e. unblocks a Send() call like a sync message) otherwise
  // a deadlock can occur if the plugin creation request from the renderer is
  // a result of a sync message by the plugin process.
  PluginProcessMsg_CreateChannel* msg =
      new PluginProcessMsg_CreateChannel(
          client->ID(),
          client->OffTheRecord());
  msg->set_unblock(true);
  if (Send(msg)) {
    sent_requests_.push_back(client);
    client->OnSentPluginChannelRequest();
  } else {
    client->OnError();
  }
}

void PluginProcessHost::OnChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  Client* client = sent_requests_.front();

  if (client) {
    if (!resource_context_map_.count(client->ID())) {
      ResourceContextEntry entry;
      entry.ref_count = 0;
      entry.resource_context = client->GetResourceContext();
      resource_context_map_[client->ID()] = entry;
    }
    resource_context_map_[client->ID()].ref_count++;
    client->OnChannelOpened(channel_handle);
  }
  sent_requests_.pop_front();
}

void PluginProcessHost::OnChannelDestroyed(int renderer_id) {
  resource_context_map_[renderer_id].ref_count--;
  if (!resource_context_map_[renderer_id].ref_count)
    resource_context_map_.erase(renderer_id);
}

void PluginProcessHost::GetContexts(const ResourceHostMsg_Request& request,
                                    ResourceContext** resource_context,
                                    net::URLRequestContext** request_context) {
  *resource_context =
      resource_context_map_[request.origin_pid].resource_context;
  *request_context = (*resource_context)->GetRequestContext();
}

}  // namespace content
