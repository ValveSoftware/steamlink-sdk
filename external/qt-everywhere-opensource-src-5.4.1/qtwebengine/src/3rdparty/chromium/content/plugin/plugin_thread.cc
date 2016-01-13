// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/plugin_thread.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_local.h"
#include "content/child/blink_platform_impl.h"
#include "content/child/child_process.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/common/plugin_process_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/plugin/content_plugin_client.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/message_filter.h"

namespace content {

namespace {

class EnsureTerminateMessageFilter : public IPC::MessageFilter {
 public:
  EnsureTerminateMessageFilter() {}

 protected:
  virtual ~EnsureTerminateMessageFilter() {}

  // IPC::MessageFilter:
  virtual void OnChannelError() OVERRIDE {
    // How long we wait before forcibly shutting down the process.
    const base::TimeDelta kPluginProcessTerminateTimeout =
        base::TimeDelta::FromSeconds(3);
    // Ensure that we don't wait indefinitely for the plugin to shutdown.
    // as the browser does not terminate plugin processes on shutdown.
    // We achieve this by posting an exit process task on the IO thread.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&EnsureTerminateMessageFilter::Terminate, this),
        kPluginProcessTerminateTimeout);
  }

 private:
  void Terminate() {
    base::KillProcess(base::GetCurrentProcessHandle(), 0, false);
  }
};

}  // namespace

static base::LazyInstance<base::ThreadLocalPointer<PluginThread> > lazy_tls =
    LAZY_INSTANCE_INITIALIZER;

PluginThread::PluginThread()
    : preloaded_plugin_module_(NULL),
      forcefully_terminate_plugin_process_(false) {
  base::FilePath plugin_path =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kPluginPath);

  lazy_tls.Pointer()->Set(this);

  PatchNPNFunctions();

  // Preload the library to avoid loading, unloading then reloading
  preloaded_plugin_module_ = base::LoadNativeLibrary(plugin_path, NULL);

  scoped_refptr<PluginLib> plugin(PluginLib::CreatePluginLib(plugin_path));
  if (plugin.get()) {
    plugin->NP_Initialize();
    // For OOP plugins the plugin dll will be unloaded during process shutdown
    // time.
    plugin->set_defer_unload(true);
  }

  GetContentClient()->plugin()->PluginProcessStarted(
      plugin.get() ? plugin->plugin_info().name : base::string16());

  channel()->AddFilter(new EnsureTerminateMessageFilter());

  // This is needed because we call some code which uses WebKit strings.
  webkit_platform_support_.reset(new BlinkPlatformImpl);
  blink::initialize(webkit_platform_support_.get());
}

PluginThread::~PluginThread() {
}

void PluginThread::SetForcefullyTerminatePluginProcess() {
  forcefully_terminate_plugin_process_ = true;
}

void PluginThread::Shutdown() {
  ChildThread::Shutdown();

  if (preloaded_plugin_module_) {
    base::UnloadNativeLibrary(preloaded_plugin_module_);
    preloaded_plugin_module_ = NULL;
  }
  NPChannelBase::CleanupChannels();
  PluginLib::UnloadAllPlugins();

  if (forcefully_terminate_plugin_process_)
    base::KillProcess(base::GetCurrentProcessHandle(), 0, /* wait= */ false);

  lazy_tls.Pointer()->Set(NULL);
}

PluginThread* PluginThread::current() {
  return lazy_tls.Pointer()->Get();
}

bool PluginThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginThread, msg)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_CreateChannel, OnCreateChannel)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_NotifyRenderersOfPendingShutdown,
                        OnNotifyRenderersOfPendingShutdown)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PluginThread::OnCreateChannel(int renderer_id,
                                   bool incognito) {
  scoped_refptr<PluginChannel> channel(PluginChannel::GetPluginChannel(
      renderer_id, ChildProcess::current()->io_message_loop_proxy()));
  IPC::ChannelHandle channel_handle;
  if (channel.get()) {
    channel_handle.name = channel->channel_handle().name;
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD.
    channel_handle.socket =
        base::FileDescriptor(channel->TakeRendererFileDescriptor(), true);
#endif
    channel->set_incognito(incognito);
  }

  Send(new PluginProcessHostMsg_ChannelCreated(channel_handle));
}

void PluginThread::OnNotifyRenderersOfPendingShutdown() {
  PluginChannel::NotifyRenderersOfPendingShutdown();
}

}  // namespace content
