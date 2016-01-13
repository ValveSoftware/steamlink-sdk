// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/ppapi_dispatcher.h"

#include <map>
#include <set>

#include "build/build_config.h"
// Need to include this before most other files because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so ppapi_messages.h will generate the
// ViewMsgLog et al. functions.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "components/tracing/child_trace_message_filter.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/nacl_irt/manifest_service.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"

#if defined(IPC_MESSAGE_LOG_ENABLED)
#include "base/containers/hash_tables.h"

LogFunctionMap g_log_function_mapping;

#define IPC_MESSAGE_MACROS_LOG_ENABLED
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
  g_log_function_mapping[msg_id] = logger

#endif
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {

PpapiDispatcher::PpapiDispatcher(scoped_refptr<base::MessageLoopProxy> io_loop,
                                 base::WaitableEvent* shutdown_event,
                                 int browser_ipc_fd,
                                 int renderer_ipc_fd)
    : next_plugin_dispatcher_id_(0),
      message_loop_(io_loop),
      shutdown_event_(shutdown_event),
      renderer_ipc_fd_(renderer_ipc_fd) {
#if defined(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging::set_log_function_map(&g_log_function_mapping);
#endif

  IPC::ChannelHandle channel_handle(
      "NaCl IPC", base::FileDescriptor(browser_ipc_fd, false));

  // Delay initializing the SyncChannel until after we add filters. This
  // ensures that the filters won't miss any messages received by
  // the channel.
  channel_ =
      IPC::SyncChannel::Create(this, GetIPCMessageLoop(), GetShutdownEvent());
  channel_->AddFilter(new proxy::PluginMessageFilter(
      NULL, proxy::PluginGlobals::Get()->resource_reply_thread_registrar()));
  channel_->AddFilter(
      new tracing::ChildTraceMessageFilter(message_loop_.get()));
  channel_->Init(channel_handle, IPC::Channel::MODE_SERVER, true);
}

base::MessageLoopProxy* PpapiDispatcher::GetIPCMessageLoop() {
  return message_loop_.get();
}

base::WaitableEvent* PpapiDispatcher::GetShutdownEvent() {
  return shutdown_event_;
}

IPC::PlatformFileForTransit PpapiDispatcher::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId peer_pid,
    bool should_close_source) {
  return IPC::InvalidPlatformFileForTransit();
}

std::set<PP_Instance>* PpapiDispatcher::GetGloballySeenInstanceIDSet() {
  return &instances_;
}

uint32 PpapiDispatcher::Register(proxy::PluginDispatcher* plugin_dispatcher) {
  if (!plugin_dispatcher ||
      plugin_dispatchers_.size() >= std::numeric_limits<uint32>::max()) {
    return 0;
  }

  uint32 id = 0;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble
    // when the counter overflows.
    id = next_plugin_dispatcher_id_++;
  } while (id == 0 ||
           plugin_dispatchers_.find(id) != plugin_dispatchers_.end());
  plugin_dispatchers_[id] = plugin_dispatcher;
  return id;
}

void PpapiDispatcher::Unregister(uint32 plugin_dispatcher_id) {
  plugin_dispatchers_.erase(plugin_dispatcher_id);
}

IPC::Sender* PpapiDispatcher::GetBrowserSender() {
  return this;
}

std::string PpapiDispatcher::GetUILanguage() {
  NOTIMPLEMENTED();
  return std::string();
}

void PpapiDispatcher::PreCacheFont(const void* logfontw) {
  NOTIMPLEMENTED();
}

void PpapiDispatcher::SetActiveURL(const std::string& url) {
  NOTIMPLEMENTED();
}

PP_Resource PpapiDispatcher::CreateBrowserFont(
    proxy::Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const Preferences& prefs) {
  NOTIMPLEMENTED();
  return 0;
}

bool PpapiDispatcher::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PpapiDispatcher, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_InitializeNaClDispatcher,
                        OnMsgInitializeNaClDispatcher)
    // All other messages are simply forwarded to a PluginDispatcher.
    IPC_MESSAGE_UNHANDLED(OnPluginDispatcherMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return true;
}

void PpapiDispatcher::OnChannelError() {
  exit(1);
}

bool PpapiDispatcher::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void PpapiDispatcher::OnMsgInitializeNaClDispatcher(
    const PpapiNaClPluginArgs& args) {
  static bool command_line_and_logging_initialized = false;
  if (command_line_and_logging_initialized) {
    LOG(FATAL) << "InitializeNaClDispatcher must be called once per plugin.";
    return;
  }

  command_line_and_logging_initialized = true;
  CommandLine::Init(0, NULL);
  for (size_t i = 0; i < args.switch_names.size(); ++i) {
    DCHECK(i < args.switch_values.size());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        args.switch_names[i], args.switch_values[i]);
  }
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  proxy::PluginGlobals::Get()->set_keepalive_throttle_interval_milliseconds(
      args.keepalive_throttle_interval_milliseconds);

  // Tell the process-global GetInterface which interfaces it can return to the
  // plugin.
  proxy::InterfaceList::SetProcessGlobalPermissions(args.permissions);

  int32_t error = ::PPP_InitializeModule(
      0 /* module */,
      &proxy::PluginDispatcher::GetBrowserInterface);
  if (error)
    ::exit(error);

  proxy::PluginDispatcher* dispatcher =
      new proxy::PluginDispatcher(::PPP_GetInterface, args.permissions,
                                  args.off_the_record);
  IPC::ChannelHandle channel_handle(
      "nacl",
      base::FileDescriptor(renderer_ipc_fd_, false));
  if (!dispatcher->InitPluginWithChannel(this, base::kNullProcessId,
                                         channel_handle, false)) {
    delete dispatcher;
    return;
  }
  // From here, the dispatcher will manage its own lifetime according to the
  // lifetime of the attached channel.

  // Notify the renderer process, if necessary.
  ManifestService* manifest_service = GetManifestService();
  if (manifest_service)
    manifest_service->StartupInitializationComplete();
}

void PpapiDispatcher::OnPluginDispatcherMessageReceived(
    const IPC::Message& msg) {
  // The first parameter should be a plugin dispatcher ID.
  PickleIterator iter(msg);
  uint32 id = 0;
  if (!msg.ReadUInt32(&iter, &id)) {
    NOTREACHED();
    return;
  }
  std::map<uint32, proxy::PluginDispatcher*>::iterator dispatcher =
      plugin_dispatchers_.find(id);
  if (dispatcher != plugin_dispatchers_.end())
    dispatcher->second->OnMessageReceived(msg);
}

}  // namespace ppapi
