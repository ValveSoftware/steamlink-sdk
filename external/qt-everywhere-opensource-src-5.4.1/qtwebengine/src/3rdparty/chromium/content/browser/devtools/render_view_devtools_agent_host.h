// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

namespace cc {
class CompositorFrameMetadata;
}

namespace content {

class DevToolsPowerHandler;
class DevToolsTracingHandler;
class RendererOverridesHandler;
class RenderViewHost;

#if defined(OS_ANDROID)
class PowerSaveBlockerImpl;
#endif

class CONTENT_EXPORT RenderViewDevToolsAgentHost
    : public IPCDevToolsAgentHost,
      private WebContentsObserver,
      public NotificationObserver {
 public:
  static void OnCancelPendingNavigation(RenderViewHost* pending,
                                        RenderViewHost* current);

  static bool DispatchIPCMessage(RenderViewHost* source,
                                 const IPC::Message& message);

  RenderViewDevToolsAgentHost(RenderViewHost*);

  RenderViewHost* render_view_host() { return render_view_host_; }

  void SynchronousSwapCompositorFrame(
      const cc::CompositorFrameMetadata& frame_metadata);

 private:
  friend class DevToolsAgentHost;

  virtual ~RenderViewDevToolsAgentHost();

  // DevTooolsAgentHost overrides.
  virtual void DisconnectRenderViewHost() OVERRIDE;
  virtual void ConnectRenderViewHost(RenderViewHost* rvh) OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() OVERRIDE;

  // IPCDevToolsAgentHost overrides.
  virtual void DispatchOnInspectorBackend(const std::string& message) OVERRIDE;
  virtual void SendMessageToAgent(IPC::Message* msg) OVERRIDE;
  virtual void OnClientAttached() OVERRIDE;
  virtual void OnClientDetached() OVERRIDE;

  // WebContentsObserver overrides.
  virtual void AboutToNavigateRenderView(RenderViewHost* dest_rvh) OVERRIDE;
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) OVERRIDE;
  virtual void RenderViewDeleted(RenderViewHost* rvh) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void DidAttachInterstitialPage() OVERRIDE;

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  void ReattachToRenderViewHost(RenderViewHost* rvh);

  bool DispatchIPCMessage(const IPC::Message& message);

  void SetRenderViewHost(RenderViewHost* rvh);
  void ClearRenderViewHost();

  void RenderViewCrashed();
  void OnSwapCompositorFrame(const IPC::Message& message);

  void OnDispatchOnInspectorFrontend(const std::string& message);
  void OnSaveAgentRuntimeState(const std::string& state);

  void ClientDetachedFromRenderer();

  void InnerOnClientAttached();
  void InnerClientDetachedFromRenderer();

  RenderViewHost* render_view_host_;
  scoped_ptr<RendererOverridesHandler> overrides_handler_;
  scoped_ptr<DevToolsTracingHandler> tracing_handler_;
  scoped_ptr<DevToolsPowerHandler> power_handler_;
#if defined(OS_ANDROID)
  scoped_ptr<PowerSaveBlockerImpl> power_save_blocker_;
#endif
  std::string state_;
  NotificationRegistrar registrar_;
  bool reattaching_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
