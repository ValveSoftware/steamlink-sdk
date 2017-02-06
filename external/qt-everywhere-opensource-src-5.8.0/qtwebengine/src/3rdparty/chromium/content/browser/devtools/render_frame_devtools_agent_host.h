// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace cc {
class CompositorFrameMetadata;
}

#if defined(OS_ANDROID)
namespace device {
class PowerSaveBlocker;
}  // namespace device
#endif

namespace content {

class BrowserContext;
class DevToolsFrameTraceRecorder;
class DevToolsProtocolHandler;
class FrameTreeNode;
class NavigationHandle;
class RenderFrameHostImpl;

namespace devtools {
namespace browser { class BrowserHandler; }
namespace dom { class DOMHandler; }
namespace emulation { class EmulationHandler; }
namespace input { class InputHandler; }
namespace inspector { class InspectorHandler; }
namespace io { class IOHandler; }
namespace network { class NetworkHandler; }
namespace page { class PageHandler; }
namespace security { class SecurityHandler; }
namespace service_worker { class ServiceWorkerHandler; }
namespace storage { class StorageHandler; }
namespace tracing { class TracingHandler; }
}

class CONTENT_EXPORT RenderFrameDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      private WebContentsObserver {
 public:
  static void AddAllAgentHosts(DevToolsAgentHost::List* result);

  static void OnCancelPendingNavigation(RenderFrameHost* pending,
                                        RenderFrameHost* current);
  static void OnBeforeNavigation(RenderFrameHost* current,
                                 RenderFrameHost* pending);
  static void OnBeforeNavigation(NavigationHandle* navigation_handle);

  void SynchronousSwapCompositorFrame(
      cc::CompositorFrameMetadata frame_metadata);

  bool HasRenderFrameHost(RenderFrameHost* host);

  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // DevTooolsAgentHost overrides.
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* web_contents) override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  Type GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;
  bool DispatchProtocolMessage(const std::string& message) override;

 private:
  friend class DevToolsAgentHost;
  explicit RenderFrameDevToolsAgentHost(RenderFrameHostImpl*);
  ~RenderFrameDevToolsAgentHost() override;

  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      RenderFrameHostImpl* host);
  static void AppendAgentHostForFrameIfApplicable(
      DevToolsAgentHost::List* result,
      RenderFrameHost* host);

  // DevToolsAgentHostImpl overrides.
  void Attach() override;
  void Detach() override;
  void InspectElement(int x, int y) override;

  // WebContentsObserver overrides.
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* rfh) override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  void DidFailProvisionalLoad(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override;
  void WebContentsDestroyed() override;
  void WasShown() override;
  void WasHidden() override;

  void AboutToNavigateRenderFrame(RenderFrameHost* old_host,
                                  RenderFrameHost* new_host);
  void AboutToNavigate(NavigationHandle* navigation_handle);

  void DispatchBufferedProtocolMessagesIfNecessary();

  void SetPending(RenderFrameHostImpl* host);
  void CommitPending();
  void DiscardPending();
  void UpdateProtocolHandlers(RenderFrameHostImpl* host);

  bool IsChildFrame();

  void OnClientAttached();
  void OnClientDetached();

  void RenderFrameCrashed();
  void OnSwapCompositorFrame(const IPC::Message& message);
  void OnDispatchOnInspectorFrontend(
      RenderFrameHost* sender,
      const DevToolsMessageChunk& message);
  void OnRequestNewWindow(RenderFrameHost* sender, int new_routing_id);
  void DestroyOnRenderFrameGone();

  bool CheckConsistency();

  void CreatePowerSaveBlocker();

  class FrameHostHolder;

  std::unique_ptr<FrameHostHolder> current_;
  std::unique_ptr<FrameHostHolder> pending_;

  // Stores per-host state between DisconnectWebContents and ConnectWebContents.
  std::unique_ptr<FrameHostHolder> disconnected_;

  std::unique_ptr<devtools::browser::BrowserHandler> browser_handler_;
  std::unique_ptr<devtools::dom::DOMHandler> dom_handler_;
  std::unique_ptr<devtools::input::InputHandler> input_handler_;
  std::unique_ptr<devtools::inspector::InspectorHandler> inspector_handler_;
  std::unique_ptr<devtools::io::IOHandler> io_handler_;
  std::unique_ptr<devtools::network::NetworkHandler> network_handler_;
  std::unique_ptr<devtools::page::PageHandler> page_handler_;
  std::unique_ptr<devtools::security::SecurityHandler> security_handler_;
  std::unique_ptr<devtools::service_worker::ServiceWorkerHandler>
      service_worker_handler_;
  std::unique_ptr<devtools::storage::StorageHandler>
      storage_handler_;
  std::unique_ptr<devtools::tracing::TracingHandler> tracing_handler_;
  std::unique_ptr<devtools::emulation::EmulationHandler> emulation_handler_;
  std::unique_ptr<DevToolsFrameTraceRecorder> frame_trace_recorder_;
#if defined(OS_ANDROID)
  std::unique_ptr<device::PowerSaveBlocker> power_save_blocker_;
  std::unique_ptr<base::WeakPtrFactory<ui::ViewAndroid>> view_weak_factory_;
#endif
  std::unique_ptr<DevToolsProtocolHandler> protocol_handler_;
  RenderFrameHostImpl* handlers_frame_host_;
  bool current_frame_crashed_;

  // PlzNavigate

  // Handle that caused the setting of pending_.
  NavigationHandle* pending_handle_;

  // List of handles currently navigating.
  std::set<NavigationHandle*> navigating_handles_;

  struct PendingMessage {
    int session_id;
    std::string method;
    std::string message;
  };
  // <call_id> -> PendingMessage
  std::map<int, PendingMessage> in_navigation_protocol_message_buffer_;

  // The FrameTreeNode associated with this agent.
  FrameTreeNode* frame_tree_node_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
