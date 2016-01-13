// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_THREAD_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_THREAD_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/user_metrics_action.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sender.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

class GURL;

namespace base {
class MessageLoop;
class MessageLoopProxy;
class WaitableEvent;
}

namespace IPC {
class MessageFilter;
class SyncChannel;
class SyncMessageFilter;
}

namespace v8 {
class Extension;
}

namespace content {

class RenderProcessObserver;
class ResourceDispatcherDelegate;

class CONTENT_EXPORT RenderThread : public IPC::Sender {
 public:
  // Returns the one render thread for this process.  Note that this can only
  // be accessed when running on the render thread itself.
  static RenderThread* Get();

  RenderThread();
  virtual ~RenderThread();

  virtual base::MessageLoop* GetMessageLoop() = 0;
  virtual IPC::SyncChannel* GetChannel() = 0;
  virtual std::string GetLocale() = 0;
  virtual IPC::SyncMessageFilter* GetSyncMessageFilter() = 0;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() = 0;

  // Called to add or remove a listener for a particular message routing ID.
  // These methods normally get delegated to a MessageRouter.
  virtual void AddRoute(int32 routing_id, IPC::Listener* listener) = 0;
  virtual void RemoveRoute(int32 routing_id) = 0;
  virtual int GenerateRoutingID() = 0;

  // These map to IPC::ChannelProxy methods.
  virtual void AddFilter(IPC::MessageFilter* filter) = 0;
  virtual void RemoveFilter(IPC::MessageFilter* filter) = 0;

  // Add/remove observers for the process.
  virtual void AddObserver(RenderProcessObserver* observer) = 0;
  virtual void RemoveObserver(RenderProcessObserver* observer) = 0;

  // Set the ResourceDispatcher delegate object for this process.
  virtual void SetResourceDispatcherDelegate(
      ResourceDispatcherDelegate* delegate) = 0;

  // We initialize WebKit as late as possible. Call this to force
  // initialization.
  virtual void EnsureWebKitInitialized() = 0;

  // Sends over a base::UserMetricsAction to be recorded by user metrics as
  // an action. Once a new user metric is added, run
  //   tools/metrics/actions/extract_actions.py
  // to add the metric to actions.xml, then update the <owner>s and
  // <description> sections. Make sure to include the actions.xml file when you
  // upload your code for review!
  //
  // WARNING: When using base::UserMetricsAction, base::UserMetricsAction
  // and a string literal parameter must be on the same line, e.g.
  //   RenderThread::Get()->RecordAction(
  //       base::UserMetricsAction("my extremely long action name"));
  // because otherwise our processing scripts won't pick up on new actions.
  virtual void RecordAction(const base::UserMetricsAction& action) = 0;

  // Sends over a string to be recorded by user metrics as a computed action.
  // When you use this you need to also update the rules for extracting known
  // actions in chrome/tools/extract_actions.py.
  virtual void RecordComputedAction(const std::string& action) = 0;

  // Asks the host to create a block of shared memory for the renderer.
  // The shared memory allocated by the host is returned back.
  virtual scoped_ptr<base::SharedMemory> HostAllocateSharedMemoryBuffer(
      size_t buffer_size) = 0;

  // Registers the given V8 extension with WebKit.
  virtual void RegisterExtension(v8::Extension* extension) = 0;

  // Schedule a call to IdleHandler with the given initial delay.
  virtual void ScheduleIdleHandler(int64 initial_delay_ms) = 0;

  // A task we invoke periodically to assist with idle cleanup.
  virtual void IdleHandler() = 0;

  // Get/Set the delay for how often the idle handler is called.
  virtual int64 GetIdleNotificationDelayInMs() const = 0;
  virtual void SetIdleNotificationDelayInMs(
      int64 idle_notification_delay_in_ms) = 0;

  virtual void UpdateHistograms(int sequence_number) = 0;

  // Post task to all worker threads. Returns number of workers.
  virtual int PostTaskToAllWebWorkers(const base::Closure& closure) = 0;

  // Resolve the proxy servers to use for a given url. On success true is
  // returned and |proxy_list| is set to a PAC string containing a list of
  // proxy servers.
  virtual bool ResolveProxy(const GURL& url, std::string* proxy_list) = 0;

  // Gets the shutdown event for the process.
  virtual base::WaitableEvent* GetShutdownEvent() = 0;

#if defined(OS_WIN)
  // Request that the given font be loaded by the browser so it's cached by the
  // OS. Please see ChildProcessHost::PreCacheFont for details.
  virtual void PreCacheFont(const LOGFONT& log_font) = 0;

  // Release cached font.
  virtual void ReleaseCachedFonts() = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_THREAD_H_
