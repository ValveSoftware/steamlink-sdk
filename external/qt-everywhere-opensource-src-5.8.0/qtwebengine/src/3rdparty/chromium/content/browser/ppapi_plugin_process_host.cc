// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_process_host.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "net/base/network_change_notifier.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_POSIX)
#include "content/public/browser/zygote_handle_linux.h"
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
#include "content/browser/renderer_host/dwrite_font_proxy_message_filter_win.h"
#include "content/common/sandbox_win.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/display/win/dpi.h"
#endif

namespace content {

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
ZygoteHandle g_ppapi_zygote;
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

// NOTE: changes to this class need to be reviewed by the security team.
class PpapiPluginSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  PpapiPluginSandboxedProcessLauncherDelegate(bool is_broker,
                                              const PepperPluginInfo& info,
                                              ChildProcessHost* host)
#if defined(OS_WIN)
      : info_(info), is_broker_(is_broker) {
#elif defined(OS_MACOSX) || defined(OS_ANDROID)
      : ipc_fd_(host->TakeClientFileDescriptor()) {
#elif defined(OS_POSIX)
      : ipc_fd_(host->TakeClientFileDescriptor()), is_broker_(is_broker) {
#else
  {
#endif
  }

  ~PpapiPluginSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool ShouldSandbox() override {
    return !is_broker_;
  }

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (is_broker_)
      return true;

    // The Pepper process is as locked-down as a renderer except that it can
    // create the server side of Chrome pipes.
    sandbox::ResultCode result;
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                             sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                             L"\\\\.\\pipe\\chrome.*");
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    content::ContentBrowserClient* browser_client =
        GetContentClient()->browser();

#if !defined(NACL_WIN64)
    if (IsWin32kRendererLockdownEnabled()) {
      for (const auto& mime_type : info_.mime_types) {
        if (browser_client->IsWin32kLockdownEnabledForMimeType(
                mime_type.mime_type)) {
          result = AddWin32kLockdownPolicy(policy, true);
          if (result != sandbox::SBOX_ALL_OK)
            return false;
          break;
        }
      }
    }
#endif
    const base::string16& sid =
        browser_client->GetAppContainerSidForSandboxType(GetSandboxType());
    if (!sid.empty())
      AddAppContainerPolicy(policy, sid.c_str());

    return true;
  }

#elif defined(OS_POSIX)
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  ZygoteHandle* GetZygote() override {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringType plugin_launcher = browser_command_line
        .GetSwitchValueNative(switches::kPpapiPluginLauncher);
    if (is_broker_ || !plugin_launcher.empty())
      return nullptr;
    return GetGenericZygote();
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)

  base::ScopedFD TakeIpcFd() override { return std::move(ipc_fd_); }
#endif  // OS_WIN

  SandboxType GetSandboxType() override {
    return SANDBOX_TYPE_PPAPI;
  }

 private:
#if defined(OS_WIN)
  const PepperPluginInfo& info_;
#endif // OS_WIN
#if defined(OS_POSIX)
  base::ScopedFD ipc_fd_;
#endif  // OS_POSIX
#if (defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)) || \
    defined(OS_WIN)
  bool is_broker_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PpapiPluginSandboxedProcessLauncherDelegate);
};

class PpapiPluginProcessHost::PluginNetworkObserver
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  explicit PluginNetworkObserver(PpapiPluginProcessHost* process_host)
      : process_host_(process_host) {
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }

  ~PluginNetworkObserver() override {
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }

  // IPAddressObserver implementation.
  void OnIPAddressChanged() override {
    // TODO(brettw) bug 90246: This doesn't seem correct. The online/offline
    // notification seems like it should be sufficient, but I don't see that
    // when I unplug and replug my network cable. Sending this notification when
    // "something" changes seems to make Flash reasonably happy, but seems
    // wrong. We should really be able to provide the real online state in
    // OnConnectionTypeChanged().
    process_host_->Send(new PpapiMsg_SetNetworkState(true));
  }

  // ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    process_host_->Send(new PpapiMsg_SetNetworkState(
        type != net::NetworkChangeNotifier::CONNECTION_NONE));
  }

 private:
  PpapiPluginProcessHost* const process_host_;
};

PpapiPluginProcessHost::~PpapiPluginProcessHost() {
  DVLOG(1) << "PpapiPluginProcessHost" << (is_broker_ ? "[broker]" : "")
           << "~PpapiPluginProcessHost()";
  CancelRequests();
}

// static
PpapiPluginProcessHost* PpapiPluginProcessHost::CreatePluginHost(
    const PepperPluginInfo& info,
    const base::FilePath& profile_data_directory) {
  PpapiPluginProcessHost* plugin_host = new PpapiPluginProcessHost(
      info, profile_data_directory);
  DCHECK(plugin_host);
  if (plugin_host->Init(info))
    return plugin_host;

  NOTREACHED();  // Init is not expected to fail.
  return NULL;
}

// static
PpapiPluginProcessHost* PpapiPluginProcessHost::CreateBrokerHost(
    const PepperPluginInfo& info) {
  PpapiPluginProcessHost* plugin_host =
      new PpapiPluginProcessHost();
  if (plugin_host->Init(info))
    return plugin_host;

  NOTREACHED();  // Init is not expected to fail.
  return NULL;
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
// static
void PpapiPluginProcessHost::EarlyZygoteLaunch() {
  DCHECK(!g_ppapi_zygote);
  g_ppapi_zygote = CreateZygote();
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

// static
void PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
    int plugin_process_id,
    int32_t pp_instance,
    const PepperRendererInstanceData& instance_data) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() &&
        iter->process_->GetData().id == plugin_process_id) {
      // Found the plugin.
      iter->host_impl_->AddInstance(pp_instance, instance_data);
      return;
    }
  }
  // We'll see this passed with a 0 process ID for the browser tag stuff that
  // is currently in the process of being removed.
  //
  // TODO(brettw) When old browser tag impl is removed
  // (PepperPluginDelegateImpl::CreateBrowserPluginModule passes a 0 plugin
  // process ID) this should be converted to a NOTREACHED().
  DCHECK(plugin_process_id == 0)
      << "Renderer sent a bad plugin process host ID";
}

// static
void PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(
    int plugin_process_id,
    int32_t pp_instance) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() &&
        iter->process_->GetData().id == plugin_process_id) {
      // Found the plugin.
      iter->host_impl_->DeleteInstance(pp_instance);
      return;
    }
  }
  // Note: It's possible that the plugin process has already been deleted by
  // the time this message is received. For example, it could have crashed.
  // That's OK, we can just ignore this message.
}

// static
void PpapiPluginProcessHost::OnPluginInstanceThrottleStateChange(
    int plugin_process_id,
    int32_t pp_instance,
    bool is_throttled) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() &&
        iter->process_->GetData().id == plugin_process_id) {
      // Found the plugin.
      iter->host_impl_->OnThrottleStateChanged(pp_instance, is_throttled);
      return;
    }
  }
  // Note: It's possible that the plugin process has already been deleted by
  // the time this message is received. For example, it could have crashed.
  // That's OK, we can just ignore this message.
}

// static
void PpapiPluginProcessHost::FindByName(
    const base::string16& name,
    std::vector<PpapiPluginProcessHost*>* hosts) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() && iter->process_->GetData().name == name)
      hosts->push_back(*iter);
  }
}

bool PpapiPluginProcessHost::Send(IPC::Message* message) {
  return process_->Send(message);
}

void PpapiPluginProcessHost::OpenChannelToPlugin(Client* client) {
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

PpapiPluginProcessHost::PpapiPluginProcessHost(
    const PepperPluginInfo& info,
    const base::FilePath& profile_data_directory)
    : profile_data_directory_(profile_data_directory),
      is_broker_(false),
      mojo_child_token_(mojo::edk::GenerateRandomToken()) {
  uint32_t base_permissions = info.permissions;

  // We don't have to do any whitelisting for APIs in this process host, so
  // don't bother passing a browser context or document url here.
  if (GetContentClient()->browser()->IsPluginAllowedToUseDevChannelAPIs(
          NULL, GURL()))
    base_permissions |= ppapi::PERMISSION_DEV_CHANNEL;
  permissions_ = ppapi::PpapiPermissions::GetForCommandLine(base_permissions);

  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_PPAPI_PLUGIN, this, mojo_child_token_));

  host_impl_.reset(new BrowserPpapiHostImpl(this, permissions_, info.name,
                                            info.path, profile_data_directory,
                                            false /* in_process */,
                                            false /* external_plugin */));

  filter_ = new PepperMessageFilter();
  process_->AddFilter(filter_.get());
  process_->GetHost()->AddFilter(host_impl_->message_filter().get());
#if defined(OS_WIN)
  process_->AddFilter(new DWriteFontProxyMessageFilter());
#endif

  GetContentClient()->browser()->DidCreatePpapiPlugin(host_impl_.get());

  // Only request network status updates if the plugin has dev permissions.
  if (permissions_.HasPermission(ppapi::PERMISSION_DEV))
    network_observer_.reset(new PluginNetworkObserver(this));
}

PpapiPluginProcessHost::PpapiPluginProcessHost()
    : is_broker_(true),
      mojo_child_token_(mojo::edk::GenerateRandomToken()) {
  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_PPAPI_BROKER, this, mojo_child_token_));

  ppapi::PpapiPermissions permissions;  // No permissions.
  // The plugin name, path and profile data directory shouldn't be needed for
  // the broker.
  host_impl_.reset(new BrowserPpapiHostImpl(this, permissions,
                                            std::string(), base::FilePath(),
                                            base::FilePath(),
                                            false /* in_process */,
                                            false /* external_plugin */));
}

bool PpapiPluginProcessHost::Init(const PepperPluginInfo& info) {
  plugin_path_ = info.path;
  if (info.name.empty()) {
    process_->SetName(plugin_path_.BaseName().LossyDisplayName());
  } else {
    process_->SetName(base::UTF8ToUTF16(info.name));
  }

  std::string mojo_channel_token =
      process_->GetHost()->CreateChannelMojo(mojo_child_token_);
  if (mojo_channel_token.empty()) {
    VLOG(1) << "Could not create pepper host channel.";
    return false;
  }

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPpapiPluginLauncher);

#if defined(OS_LINUX)
  int flags = plugin_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                        ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif
  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty()) {
    VLOG(1) << "Pepper plugin exe path is empty.";
    return false;
  }

  base::CommandLine* cmd_line = new base::CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              is_broker_ ? switches::kPpapiBrokerProcess
                                         : switches::kPpapiPluginProcess);
  cmd_line->AppendSwitchASCII(switches::kMojoChannelToken, mojo_channel_token);

#if defined(OS_WIN)
  cmd_line->AppendArg(is_broker_ ? switches::kPrefetchArgumentPpapiBroker
                                 : switches::kPrefetchArgumentPpapi);
#endif  // defined(OS_WIN)

  // These switches are forwarded to both plugin and broker pocesses.
  static const char* const kCommonForwardSwitches[] = {
    switches::kVModule
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kCommonForwardSwitches,
                             arraysize(kCommonForwardSwitches));

  if (!is_broker_) {
    static const char* const kPluginForwardSwitches[] = {
      switches::kDisableSeccompFilterSandbox,
#if defined(OS_MACOSX)
      switches::kEnableSandboxLogging,
#endif
      switches::kNoSandbox,
      switches::kPpapiStartupDialog,
    };
    cmd_line->CopySwitchesFrom(browser_command_line, kPluginForwardSwitches,
                               arraysize(kPluginForwardSwitches));

    // Copy any flash args over and introduce field trials if necessary.
    // TODO(vtl): Stop passing flash args in the command line, or windows is
    // going to explode.
    std::string existing_args =
        browser_command_line.GetSwitchValueASCII(switches::kPpapiFlashArgs);
    cmd_line->AppendSwitchASCII(switches::kPpapiFlashArgs, existing_args);
  }

  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  if (!locale.empty()) {
    // Pass on the locale so the plugin will know what language we're using.
    cmd_line->AppendSwitchASCII(switches::kLang, locale);
  }

#if defined(OS_WIN)
  cmd_line->AppendSwitchASCII(
      switches::kDeviceScaleFactor,
      base::DoubleToString(display::win::GetDPIScale()));
#endif

  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  // On posix, never use the zygote for the broker. Also, only use the zygote if
  // we are not using a plugin launcher - having a plugin launcher means we need
  // to use another process instead of just forking the zygote.
  process_->Launch(
      new PpapiPluginSandboxedProcessLauncherDelegate(is_broker_,
                                                      info,
                                                      process_->GetHost()),
      cmd_line,
      true);
  return true;
}

void PpapiPluginProcessHost::RequestPluginChannel(Client* client) {
  base::ProcessHandle process_handle;
  int renderer_child_id;
  client->GetPpapiChannelInfo(&process_handle, &renderer_child_id);

  base::ProcessId process_id = base::kNullProcessId;
  if (process_handle != base::kNullProcessHandle) {
    // This channel is not used by the browser itself.
    process_id = base::GetProcId(process_handle);
    CHECK_NE(base::kNullProcessId, process_id);
  }

  // We can't send any sync messages from the browser because it might lead to
  // a hang. See the similar code in PluginProcessHost for more description.
  PpapiMsg_CreateChannel* msg = new PpapiMsg_CreateChannel(
      process_id, renderer_child_id, client->OffTheRecord());
  msg->set_unblock(true);
  if (Send(msg)) {
    sent_requests_.push(client);
  } else {
    client->OnPpapiChannelOpened(IPC::ChannelHandle(), base::kNullProcessId, 0);
  }
}

void PpapiPluginProcessHost::OnProcessLaunched() {
  VLOG(2) << "ppapi plugin process launched.";
  host_impl_->set_plugin_process(process_->GetProcess().Duplicate());
}

void PpapiPluginProcessHost::OnProcessCrashed(int exit_code) {
  VLOG(1) << "ppapi plugin process crashed.";
  PluginServiceImpl::GetInstance()->RegisterPluginCrash(plugin_path_);
}

bool PpapiPluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiPluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ChannelCreated,
                        OnRendererPluginChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

// Called when the browser <--> plugin channel has been established.
void PpapiPluginProcessHost::OnChannelConnected(int32_t peer_pid) {
  // This will actually load the plugin. Errors will actually not be reported
  // back at this point. Instead, the plugin will fail to establish the
  // connections when we request them on behalf of the renderer(s).
  Send(new PpapiMsg_LoadPlugin(plugin_path_, permissions_));

  // Process all pending channel requests from the renderers.
  for (size_t i = 0; i < pending_requests_.size(); i++)
    RequestPluginChannel(pending_requests_[i]);
  pending_requests_.clear();
}

// Called when the browser <--> plugin channel has an error. This normally
// means the plugin has crashed.
void PpapiPluginProcessHost::OnChannelError() {
  VLOG(1) << "PpapiPluginProcessHost" << (is_broker_ ? "[broker]" : "")
          << "::OnChannelError()";
  // We don't need to notify the renderers that were communicating with the
  // plugin since they have their own channels which will go into the error
  // state at the same time. Instead, we just need to notify any renderers
  // that have requested a connection but have not yet received one.
  CancelRequests();
}

void PpapiPluginProcessHost::CancelRequests() {
  DVLOG(1) << "PpapiPluginProcessHost" << (is_broker_ ? "[broker]" : "")
           << "CancelRequests()";
  for (size_t i = 0; i < pending_requests_.size(); i++) {
    pending_requests_[i]->OnPpapiChannelOpened(IPC::ChannelHandle(),
                                               base::kNullProcessId, 0);
  }
  pending_requests_.clear();

  while (!sent_requests_.empty()) {
    sent_requests_.front()->OnPpapiChannelOpened(IPC::ChannelHandle(),
                                                 base::kNullProcessId, 0);
    sent_requests_.pop();
  }
}

// Called when a new plugin <--> renderer channel has been created.
void PpapiPluginProcessHost::OnRendererPluginChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  if (sent_requests_.empty())
    return;

  // All requests should be processed FIFO, so the next item in the
  // sent_requests_ queue should be the one that the plugin just created.
  Client* client = sent_requests_.front();
  sent_requests_.pop();

  const ChildProcessData& data = process_->GetData();
  client->OnPpapiChannelOpened(channel_handle, base::GetProcId(data.handle),
                               data.id);
}

}  // namespace content
