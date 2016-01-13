// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/npapi/plugin_channel_host.h"

#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "content/child/child_process.h"
#include "content/child/npapi/npobject_base.h"
#include "content/child/plugin_messages.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#include "third_party/WebKit/public/web/WebBindings.h"

// TODO(shess): Debugging for http://crbug.com/97285
//
// The hypothesis at #55 requires that RemoveRoute() be called between
// sending ViewHostMsg_OpenChannelToPlugin to the browser, and calling
// GetPluginChannelHost() on the result.  This code detects that case
// and stores the backtrace of the RemoveRoute() in a breakpad key.
// The specific RemoveRoute() is not tracked (there could be multiple,
// and which is the one can't be known until the open completes), but
// the backtrace from any such nested call should be sufficient to
// drive a repro.
#if defined(OS_MACOSX)
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/strings/sys_string_conversions.h"

namespace {

// Breakpad key for the RemoveRoute() backtrace.
const char* kRemoveRouteTraceKey = "remove_route_bt";

// Breakpad key for the OnChannelError() backtrace.
const char* kChannelErrorTraceKey = "channel_error_bt";

// GetRemoveTrackingFlag() exposes this so that
// WebPluginDelegateProxy::Initialize() can do scoped set/reset.  When
// true, RemoveRoute() knows WBDP::Initialize() is on the stack, and
// records the backtrace.
bool remove_tracking = false;

}  // namespace
#endif

namespace content {

#if defined(OS_MACOSX)
// static
bool* PluginChannelHost::GetRemoveTrackingFlag() {
  return &remove_tracking;
}
#endif

// static
PluginChannelHost* PluginChannelHost::GetPluginChannelHost(
    const IPC::ChannelHandle& channel_handle,
    base::MessageLoopProxy* ipc_message_loop) {
  PluginChannelHost* result =
      static_cast<PluginChannelHost*>(NPChannelBase::GetChannel(
          channel_handle,
          IPC::Channel::MODE_CLIENT,
          ClassFactory,
          ipc_message_loop,
          true,
          ChildProcess::current()->GetShutDownEvent()));
  return result;
}

PluginChannelHost::PluginChannelHost() : expecting_shutdown_(false) {
}

PluginChannelHost::~PluginChannelHost() {
}

bool PluginChannelHost::Init(base::MessageLoopProxy* ipc_message_loop,
                             bool create_pipe_now,
                             base::WaitableEvent* shutdown_event) {
  return NPChannelBase::Init(ipc_message_loop, create_pipe_now, shutdown_event);
}

int PluginChannelHost::GenerateRouteID() {
  int route_id = MSG_ROUTING_NONE;
  Send(new PluginMsg_GenerateRouteID(&route_id));

  return route_id;
}

void PluginChannelHost::AddRoute(int route_id,
                                 IPC::Listener* listener,
                                 NPObjectBase* npobject) {
  NPChannelBase::AddRoute(route_id, listener, npobject);

  if (!npobject)
    proxies_[route_id] = listener;
}

void PluginChannelHost::RemoveRoute(int route_id) {
#if defined(OS_MACOSX)
  if (remove_tracking) {
    base::debug::StackTrace trace;
    size_t count = 0;
    const void* const* addresses = trace.Addresses(&count);
    base::debug::SetCrashKeyFromAddresses(
        kRemoveRouteTraceKey, addresses, count);
  }
#endif

  proxies_.erase(route_id);
  NPChannelBase::RemoveRoute(route_id);
}

bool PluginChannelHost::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginChannelHost, message)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetException, OnSetException)
    IPC_MESSAGE_HANDLER(PluginHostMsg_PluginShuttingDown, OnPluginShuttingDown)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PluginChannelHost::OnSetException(const std::string& message) {
  blink::WebBindings::setException(NULL, message.c_str());
}

void PluginChannelHost::OnPluginShuttingDown() {
  expecting_shutdown_ = true;
}

bool PluginChannelHost::Send(IPC::Message* msg) {
  if (msg->is_sync()) {
    base::TimeTicks start_time(base::TimeTicks::Now());
    bool result = NPChannelBase::Send(msg);
    UMA_HISTOGRAM_TIMES("Plugin.SyncMessageTime",
                        base::TimeTicks::Now() - start_time);
    return result;
  }
  return NPChannelBase::Send(msg);
}

void PluginChannelHost::OnChannelError() {
#if defined(OS_MACOSX)
  if (remove_tracking) {
    base::debug::StackTrace trace;
    size_t count = 0;
    const void* const* addresses = trace.Addresses(&count);
    base::debug::SetCrashKeyFromAddresses(
        kChannelErrorTraceKey, addresses, count);
  }
#endif

  NPChannelBase::OnChannelError();

  for (ProxyMap::iterator iter = proxies_.begin();
       iter != proxies_.end(); iter++) {
    iter->second->OnChannelError();
  }

  proxies_.clear();
}

}  // namespace content
