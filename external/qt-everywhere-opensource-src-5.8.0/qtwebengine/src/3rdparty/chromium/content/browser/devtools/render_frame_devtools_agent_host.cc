// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_frame_devtools_agent_host.h"

#include <tuple>
#include <utility>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_frame_trace_recorder.h"
#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/dom_handler.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/input_handler.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/protocol/service_worker_handler.h"
#include "content/browser/devtools/protocol/storage_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/browser_side_navigation_policy.h"

#if defined(OS_ANDROID)
#include "content/public/browser/render_widget_host_view.h"
#include "device/power_save_blocker/power_save_blocker.h"
#endif

namespace content {

typedef std::vector<RenderFrameDevToolsAgentHost*> Instances;

namespace {
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

static RenderFrameDevToolsAgentHost* FindAgentHost(RenderFrameHost* host) {
  if (g_instances == NULL)
    return NULL;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if ((*it)->HasRenderFrameHost(host))
      return *it;
  }
  return NULL;
}

static RenderFrameDevToolsAgentHost* FindAgentHost(
    FrameTreeNode* frame_tree_node) {
  if (g_instances == NULL)
    return NULL;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if ((*it)->frame_tree_node() == frame_tree_node)
      return *it;
  }
  return NULL;
}

static RenderFrameDevToolsAgentHost* FindAgentHost(WebContents* web_contents) {
  if (!web_contents->GetMainFrame())
    return nullptr;
  return FindAgentHost(web_contents->GetMainFrame());
}

bool ShouldCreateDevToolsFor(RenderFrameHost* rfh) {
  return rfh->IsCrossProcessSubframe() || !rfh->GetParent();
}

}  // namespace

// RenderFrameDevToolsAgentHost::FrameHostHolder -------------------------------

class RenderFrameDevToolsAgentHost::FrameHostHolder {
 public:
  FrameHostHolder(
      RenderFrameDevToolsAgentHost* agent, RenderFrameHostImpl* host);
  ~FrameHostHolder();

  RenderFrameHostImpl* host() const { return host_; }

  void Attach();
  void Reattach(FrameHostHolder* old);
  void Detach();
  void DispatchProtocolMessage(int session_id,
                               int call_id,
                               const std::string& method,
                               const std::string& message);
  void InspectElement(int x, int y);
  void ProcessChunkedMessageFromAgent(const DevToolsMessageChunk& chunk);
  void Suspend();
  void Resume();

 private:
  void GrantPolicy();
  void RevokePolicy();
  void SendMessageToClient(int session_id, const std::string& message);

  RenderFrameDevToolsAgentHost* agent_;
  RenderFrameHostImpl* host_;
  bool attached_;
  bool suspended_;
  DevToolsMessageChunkProcessor chunk_processor_;
  // <session_id, message>
  std::vector<std::pair<int, std::string>> pending_messages_;
  // <call_id> -> PendingMessage
  std::map<int, PendingMessage> sent_messages_;
};

RenderFrameDevToolsAgentHost::FrameHostHolder::FrameHostHolder(
    RenderFrameDevToolsAgentHost* agent, RenderFrameHostImpl* host)
    : agent_(agent),
      host_(host),
      attached_(false),
      suspended_(false),
      chunk_processor_(base::Bind(
           &RenderFrameDevToolsAgentHost::FrameHostHolder::SendMessageToClient,
           base::Unretained(this))) {
  DCHECK(agent_);
  DCHECK(host_);
}

RenderFrameDevToolsAgentHost::FrameHostHolder::~FrameHostHolder() {
  if (attached_)
    RevokePolicy();
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Attach() {
  host_->Send(new DevToolsAgentMsg_Attach(
      host_->GetRoutingID(), agent_->GetId(), agent_->session_id()));
  GrantPolicy();
  attached_ = true;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Reattach(
    FrameHostHolder* old) {
  if (old)
    chunk_processor_.set_state_cookie(old->chunk_processor_.state_cookie());
  host_->Send(new DevToolsAgentMsg_Reattach(
      host_->GetRoutingID(), agent_->GetId(), agent_->session_id(),
      chunk_processor_.state_cookie()));
  if (old) {
    for (const auto& pair : old->sent_messages_) {
      DispatchProtocolMessage(pair.second.session_id, pair.first,
                              pair.second.method, pair.second.message);
    }
  }
  GrantPolicy();
  attached_ = true;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Detach() {
  host_->Send(new DevToolsAgentMsg_Detach(host_->GetRoutingID()));
  RevokePolicy();
  attached_ = false;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::GrantPolicy() {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
      host_->GetProcess()->GetID());
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::RevokePolicy() {
  bool process_has_agents = false;
  RenderProcessHost* process_host = host_->GetProcess();
  for (RenderFrameDevToolsAgentHost* agent : g_instances.Get()) {
    if (!agent->IsAttached())
      continue;
    if (agent->current_ && agent->current_->host() != host_ &&
        agent->current_->host()->GetProcess() == process_host) {
      process_has_agents = true;
    }
    if (agent->pending_ && agent->pending_->host() != host_ &&
        agent->pending_->host()->GetProcess() == process_host) {
      process_has_agents = true;
    }
  }

  // We are the last to disconnect from the renderer -> revoke permissions.
  if (!process_has_agents) {
    ChildProcessSecurityPolicyImpl::GetInstance()->RevokeReadRawCookies(
        process_host->GetID());
  }
}
void RenderFrameDevToolsAgentHost::FrameHostHolder::DispatchProtocolMessage(
    int session_id,
    int call_id,
    const std::string& method,
    const std::string& message) {
  host_->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
      host_->GetRoutingID(), session_id, call_id, method, message));
  sent_messages_[call_id] = { session_id, method, message };
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::InspectElement(
    int x, int y) {
  DCHECK(attached_);
  host_->Send(new DevToolsAgentMsg_InspectElement(
      host_->GetRoutingID(), x, y));
}

void
RenderFrameDevToolsAgentHost::FrameHostHolder::ProcessChunkedMessageFromAgent(
    const DevToolsMessageChunk& chunk) {
  chunk_processor_.ProcessChunkedMessageFromAgent(chunk);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::SendMessageToClient(
    int session_id,
    const std::string& message) {
  sent_messages_.erase(chunk_processor_.last_call_id());
  if (suspended_)
    pending_messages_.push_back(std::make_pair(session_id, message));
  else
    agent_->SendMessageToClient(session_id, message);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Suspend() {
  suspended_ = true;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Resume() {
  suspended_ = false;
  for (const auto& pair : pending_messages_)
    agent_->SendMessageToClient(pair.first, pair.second);
  std::vector<std::pair<int, std::string>> empty;
  pending_messages_.swap(empty);
}

// RenderFrameDevToolsAgentHost ------------------------------------------------

// static
scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(RenderFrameHost* frame_host) {
  while (frame_host && !ShouldCreateDevToolsFor(frame_host))
    frame_host = frame_host->GetParent();
  DCHECK(frame_host);
  RenderFrameDevToolsAgentHost* result = FindAgentHost(frame_host);
  if (!result) {
    result = new RenderFrameDevToolsAgentHost(
        static_cast<RenderFrameHostImpl*>(frame_host));
  }
  return result;
}

// static
scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(WebContents* web_contents) {
  // TODO(dgozman): this check should not be necessary. See
  // http://crbug.com/489664.
  if (!web_contents->GetMainFrame())
    return nullptr;
  return DevToolsAgentHost::GetOrCreateFor(web_contents->GetMainFrame());
}

// static
scoped_refptr<DevToolsAgentHost> RenderFrameDevToolsAgentHost::GetOrCreateFor(
    RenderFrameHostImpl* host) {
  RenderFrameDevToolsAgentHost* result = FindAgentHost(host);
  if (!result)
    result = new RenderFrameDevToolsAgentHost(host);
  return result;
}

// static
void RenderFrameDevToolsAgentHost::AppendAgentHostForFrameIfApplicable(
    DevToolsAgentHost::List* result,
    RenderFrameHost* host) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(host);
  if (!rfh->IsRenderFrameLive())
    return;
  if (ShouldCreateDevToolsFor(rfh))
    result->push_back(RenderFrameDevToolsAgentHost::GetOrCreateFor(rfh));
}

// static
bool DevToolsAgentHost::HasFor(WebContents* web_contents) {
  return FindAgentHost(web_contents) != NULL;
}

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(web_contents);
  return agent_host && agent_host->IsAttached();
}

// static
void RenderFrameDevToolsAgentHost::AddAllAgentHosts(
    DevToolsAgentHost::List* result) {
  base::Callback<void(RenderFrameHost*)> callback = base::Bind(
      RenderFrameDevToolsAgentHost::AppendAgentHostForFrameIfApplicable,
      base::Unretained(result));
  for (const auto& wc : WebContentsImpl::GetAllWebContents())
    wc->ForEachFrame(callback);
}

// static
void RenderFrameDevToolsAgentHost::OnCancelPendingNavigation(
    RenderFrameHost* pending,
    RenderFrameHost* current) {
  if (IsBrowserSideNavigationEnabled())
    return;

  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(pending);
  if (!agent_host)
    return;
  if (agent_host->pending_ && agent_host->pending_->host() == pending) {
    DCHECK(agent_host->current_ && agent_host->current_->host() == current);
    agent_host->DiscardPending();
    DCHECK(agent_host->CheckConsistency());
  }
}

// static
void RenderFrameDevToolsAgentHost::OnBeforeNavigation(
    RenderFrameHost* current, RenderFrameHost* pending) {
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(current);
  if (agent_host)
    agent_host->AboutToNavigateRenderFrame(current, pending);
}

// static
void RenderFrameDevToolsAgentHost::OnBeforeNavigation(
    NavigationHandle* navigation_handle) {
  FrameTreeNode* frame_tree_node =
      static_cast<NavigationHandleImpl*>(navigation_handle)->frame_tree_node();
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (agent_host)
    agent_host->AboutToNavigate(navigation_handle);
}

RenderFrameDevToolsAgentHost::RenderFrameDevToolsAgentHost(
    RenderFrameHostImpl* host)
    : browser_handler_(new devtools::browser::BrowserHandler()),
      dom_handler_(new devtools::dom::DOMHandler()),
      input_handler_(new devtools::input::InputHandler()),
      inspector_handler_(new devtools::inspector::InspectorHandler()),
      io_handler_(new devtools::io::IOHandler(GetIOContext())),
      network_handler_(new devtools::network::NetworkHandler()),
      page_handler_(nullptr),
      security_handler_(nullptr),
      service_worker_handler_(
          new devtools::service_worker::ServiceWorkerHandler()),
      storage_handler_(new devtools::storage::StorageHandler()),
      tracing_handler_(new devtools::tracing::TracingHandler(
          devtools::tracing::TracingHandler::Renderer,
          host->GetFrameTreeNodeId(),
          GetIOContext())),
      emulation_handler_(nullptr),
      frame_trace_recorder_(nullptr),
      protocol_handler_(new DevToolsProtocolHandler(this)),
      handlers_frame_host_(nullptr),
      current_frame_crashed_(false),
      pending_handle_(nullptr),
      frame_tree_node_(host->frame_tree_node()) {
  DevToolsProtocolDispatcher* dispatcher = protocol_handler_->dispatcher();
  dispatcher->SetBrowserHandler(browser_handler_.get());
  dispatcher->SetDOMHandler(dom_handler_.get());
  dispatcher->SetInputHandler(input_handler_.get());
  dispatcher->SetInspectorHandler(inspector_handler_.get());
  dispatcher->SetIOHandler(io_handler_.get());
  dispatcher->SetNetworkHandler(network_handler_.get());
  dispatcher->SetServiceWorkerHandler(service_worker_handler_.get());
  dispatcher->SetStorageHandler(storage_handler_.get());
  dispatcher->SetTracingHandler(tracing_handler_.get());

  if (!host->GetParent()) {
    security_handler_.reset(new devtools::security::SecurityHandler());
    page_handler_.reset(new devtools::page::PageHandler());
    emulation_handler_.reset(
        new devtools::emulation::EmulationHandler());
    dispatcher->SetSecurityHandler(security_handler_.get());
    dispatcher->SetPageHandler(page_handler_.get());
    dispatcher->SetEmulationHandler(emulation_handler_.get());
  }

  SetPending(host);
  CommitPending();
  WebContentsObserver::Observe(WebContents::FromRenderFrameHost(host));

  if (web_contents() && web_contents()->GetCrashedStatus() !=
      base::TERMINATION_STATUS_STILL_RUNNING) {
      current_frame_crashed_ = true;
  }

  g_instances.Get().push_back(this);
  AddRef();  // Balanced in RenderFrameHostDestroyed.
}

void RenderFrameDevToolsAgentHost::SetPending(RenderFrameHostImpl* host) {
  DCHECK(!pending_);
  current_frame_crashed_ = false;
  pending_.reset(new FrameHostHolder(this, host));
  if (IsAttached())
    pending_->Reattach(current_.get());

  // Can only be null in constructor.
  if (current_)
    current_->Suspend();
  pending_->Suspend();

  UpdateProtocolHandlers(host);
}

void RenderFrameDevToolsAgentHost::CommitPending() {
  DCHECK(pending_);
  current_frame_crashed_ = false;

  if (!ShouldCreateDevToolsFor(pending_->host())) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
    return;
  }

  current_ = std::move(pending_);
  UpdateProtocolHandlers(current_->host());
  current_->Resume();
}

void RenderFrameDevToolsAgentHost::DiscardPending() {
  DCHECK(pending_);
  DCHECK(current_);
  pending_.reset();
  UpdateProtocolHandlers(current_->host());
  current_->Resume();
}

BrowserContext* RenderFrameDevToolsAgentHost::GetBrowserContext() {
  WebContents* contents = web_contents();
  return contents ? contents->GetBrowserContext() : nullptr;
}

WebContents* RenderFrameDevToolsAgentHost::GetWebContents() {
  return web_contents();
}

void RenderFrameDevToolsAgentHost::Attach() {
  if (current_)
    current_->Attach();
  if (pending_)
    pending_->Attach();
  OnClientAttached();
}

void RenderFrameDevToolsAgentHost::Detach() {
  if (current_)
    current_->Detach();
  if (pending_)
    pending_->Detach();
  OnClientDetached();
}

bool RenderFrameDevToolsAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  int call_id = 0;
  std::string method;
  if (protocol_handler_->HandleOptionalMessage(session_id(), message, &call_id,
                                               &method))
    return true;

  if (!navigating_handles_.empty()) {
    DCHECK(IsBrowserSideNavigationEnabled());
    in_navigation_protocol_message_buffer_[call_id] =
        { session_id(), method, message };
    return true;
  }

  if (current_)
    current_->DispatchProtocolMessage(session_id(), call_id, method, message);
  if (pending_)
    pending_->DispatchProtocolMessage(session_id(), call_id, method, message);
  return true;
}

void RenderFrameDevToolsAgentHost::InspectElement(int x, int y) {
  if (current_)
    current_->InspectElement(x, y);
  if (pending_)
    pending_->InspectElement(x, y);
}

void RenderFrameDevToolsAgentHost::OnClientAttached() {
  if (!web_contents())
    return;

  frame_trace_recorder_.reset(new DevToolsFrameTraceRecorder());
  CreatePowerSaveBlocker();

  // TODO(kaznacheev): Move this call back to DevToolsManager when
  // extensions::ProcessManager no longer relies on this notification.
  DevToolsAgentHostImpl::NotifyCallbacks(this, true);
}

void RenderFrameDevToolsAgentHost::OnClientDetached() {
#if defined(OS_ANDROID)
  power_save_blocker_.reset();
#endif
  if (emulation_handler_)
    emulation_handler_->Detached();
  if (page_handler_)
    page_handler_->Detached();
  service_worker_handler_->Detached();
  tracing_handler_->Detached();
  frame_trace_recorder_.reset();
  in_navigation_protocol_message_buffer_.clear();

  // TODO(kaznacheev): Move this call back to DevToolsManager when
  // extensions::ProcessManager no longer relies on this notification.
  DevToolsAgentHostImpl::NotifyCallbacks(this, false);
}

RenderFrameDevToolsAgentHost::~RenderFrameDevToolsAgentHost() {
  Instances::iterator it = std::find(g_instances.Get().begin(),
                                     g_instances.Get().end(),
                                     this);
  if (it != g_instances.Get().end())
    g_instances.Get().erase(it);
}

void RenderFrameDevToolsAgentHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  // ReadyToCommitNavigation should only be called in PlzNavigate.
  DCHECK(IsBrowserSideNavigationEnabled());

  // If the navigation is not tracked, return;
  if (navigating_handles_.count(navigation_handle) == 0)
    return;

  RenderFrameHostImpl* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(
          navigation_handle->GetRenderFrameHost());
  if (current_->host() != render_frame_host_impl || current_frame_crashed_) {
    SetPending(render_frame_host_impl);
    pending_handle_ = navigation_handle;
    // Commit when navigating the same frame after crash, avoiding the same
    // host in current_ and pending_.
    if (current_->host() == render_frame_host_impl) {
      pending_handle_ = nullptr;
      CommitPending();
    }
  }
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!IsBrowserSideNavigationEnabled())
    return;

  // If the navigation is not tracked, return;
  if (navigating_handles_.count(navigation_handle) == 0)
    return;

  // Now that the navigation is finished, remove the handle from the list of
  // navigating handles.
  navigating_handles_.erase(navigation_handle);

  if (pending_handle_ == navigation_handle) {
    // This navigation handle did set the pending FrameHostHolder.
    DCHECK(pending_);
    if (navigation_handle->HasCommitted()) {
      DCHECK(pending_->host() == navigation_handle->GetRenderFrameHost());
      CommitPending();
    } else {
      DiscardPending();
    }
    pending_handle_ = nullptr;
  }
  DispatchBufferedProtocolMessagesIfNecessary();

  DCHECK(CheckConsistency());
  if (navigation_handle->HasCommitted())
    service_worker_handler_->UpdateHosts();
}

void RenderFrameDevToolsAgentHost::AboutToNavigateRenderFrame(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (IsBrowserSideNavigationEnabled())
    return;

  DCHECK(!pending_ || pending_->host() != old_host);
  if (!current_ || current_->host() != old_host) {
    DCHECK(CheckConsistency());
    return;
  }
  if (old_host == new_host && !current_frame_crashed_) {
    DCHECK(CheckConsistency());
    return;
  }
  DCHECK(!pending_);
  SetPending(static_cast<RenderFrameHostImpl*>(new_host));
  // Commit when navigating the same frame after crash, avoiding the same
  // host in current_ and pending_.
  if (old_host == new_host)
    CommitPending();
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::AboutToNavigate(
    NavigationHandle* navigation_handle) {
  if (!IsBrowserSideNavigationEnabled())
    return;
  DCHECK(current_);
  navigating_handles_.insert(navigation_handle);
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (IsBrowserSideNavigationEnabled())
    return;

  DCHECK(!pending_ || pending_->host() != old_host);
  if (!current_ || current_->host() != old_host) {
    DCHECK(CheckConsistency());
    return;
  }

  // AboutToNavigateRenderFrame was not called for renderer-initiated
  // navigation.
  if (!pending_)
    SetPending(static_cast<RenderFrameHostImpl*>(new_host));
  CommitPending();
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::FrameDeleted(RenderFrameHost* rfh) {
  if (pending_ && pending_->host() == rfh) {
    if (!IsBrowserSideNavigationEnabled())
      DiscardPending();
    DCHECK(CheckConsistency());
    return;
  }

  if (current_ && current_->host() == rfh)
    DestroyOnRenderFrameGone();  // |this| may be deleted at this point.
}

void RenderFrameDevToolsAgentHost::RenderFrameDeleted(RenderFrameHost* rfh) {
  if (!current_frame_crashed_)
    FrameDeleted(rfh);
  else
    DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::DestroyOnRenderFrameGone() {
  DCHECK(current_);
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
  UpdateProtocolHandlers(nullptr);
  if (IsAttached())
    OnClientDetached();
  HostClosed();
  pending_.reset();
  current_.reset();
  frame_tree_node_ = nullptr;
  pending_handle_ = nullptr;
  WebContentsObserver::Observe(nullptr);
  Release();
}

bool RenderFrameDevToolsAgentHost::CheckConsistency() {
  if (current_ && pending_ && current_->host() == pending_->host())
    return false;
  if (IsBrowserSideNavigationEnabled())
    return true;
  if (!frame_tree_node_)
    return !handlers_frame_host_;
  RenderFrameHostManager* manager = frame_tree_node_->render_manager();
  return handlers_frame_host_ == manager->current_frame_host() ||
      handlers_frame_host_ == manager->pending_frame_host();
}

void RenderFrameDevToolsAgentHost::CreatePowerSaveBlocker() {
#if defined(OS_ANDROID)
  power_save_blocker_.reset(new device::PowerSaveBlocker(
      device::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
      device::PowerSaveBlocker::kReasonOther, "DevTools",
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  if (web_contents()->GetNativeView()) {
    view_weak_factory_.reset(new base::WeakPtrFactory<ui::ViewAndroid>(
        web_contents()->GetNativeView()));
    power_save_blocker_->InitDisplaySleepBlocker(
        view_weak_factory_->GetWeakPtr());
  }
#endif
}

void RenderFrameDevToolsAgentHost::RenderProcessGone(
    base::TerminationStatus status) {
  switch(status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      inspector_handler_->TargetCrashed();
      current_frame_crashed_ = true;
      break;
    default:
      inspector_handler_->TargetDetached("Render process gone.");
      break;
  }
  DCHECK(CheckConsistency());
}

bool RenderFrameDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message) {
  if (!current_)
    return false;
  if (message.type() == ViewHostMsg_SwapCompositorFrame::ID)
    OnSwapCompositorFrame(message);
  return false;
}

bool RenderFrameDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  bool is_current = current_ && current_->host() == render_frame_host;
  bool is_pending = pending_ && pending_->host() == render_frame_host;
  if (!is_current && !is_pending)
    return false;
  if (!IsAttached())
    return false;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(RenderFrameDevToolsAgentHost, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsAgentHostMsg_RequestNewWindow,
                        OnRequestNewWindow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderFrameDevToolsAgentHost::DidAttachInterstitialPage() {
  if (page_handler_)
    page_handler_->DidAttachInterstitialPage();

  // TODO(dgozman): this may break for cross-process subframes.
  if (!pending_) {
    DCHECK(CheckConsistency());
    return;
  }
  // Pending set in AboutToNavigateRenderFrame turned out to be interstitial.
  // Connect back to the real one.
  DiscardPending();
  pending_handle_ = nullptr;
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::DidDetachInterstitialPage() {
  if (page_handler_)
    page_handler_->DidDetachInterstitialPage();
}

void RenderFrameDevToolsAgentHost::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (IsBrowserSideNavigationEnabled())
    return;
  if (pending_ && pending_->host() == render_frame_host)
    CommitPending();
  DCHECK(CheckConsistency());
  service_worker_handler_->UpdateHosts();
}

void RenderFrameDevToolsAgentHost::DidFailProvisionalLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  if (IsBrowserSideNavigationEnabled())
    return;
  if (pending_ && pending_->host() == render_frame_host)
    DiscardPending();
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::WebContentsDestroyed() {
#if defined(OS_ANDROID)
  view_weak_factory_.reset();
#endif
}

void RenderFrameDevToolsAgentHost::WasShown() {
  CreatePowerSaveBlocker();
}

void RenderFrameDevToolsAgentHost::WasHidden() {
#if defined(OS_ANDROID)
  power_save_blocker_.reset();
#endif
}

void RenderFrameDevToolsAgentHost::
    DispatchBufferedProtocolMessagesIfNecessary() {
  if (navigating_handles_.empty() &&
      in_navigation_protocol_message_buffer_.size()) {
    DCHECK(current_);
    for (const auto& pair : in_navigation_protocol_message_buffer_) {
      current_->DispatchProtocolMessage(
          pair.second.session_id, pair.first, pair.second.method,
          pair.second.message);
    }
    in_navigation_protocol_message_buffer_.clear();
  }
}

void RenderFrameDevToolsAgentHost::UpdateProtocolHandlers(
    RenderFrameHostImpl* host) {
  handlers_frame_host_ = host;
  dom_handler_->SetRenderFrameHost(host);
  if (emulation_handler_)
    emulation_handler_->SetRenderFrameHost(host);
  input_handler_->SetRenderWidgetHost(
      host ? host->GetRenderWidgetHost() : nullptr);
  inspector_handler_->SetRenderFrameHost(host);
  network_handler_->SetRenderFrameHost(host);
  if (page_handler_)
    page_handler_->SetRenderFrameHost(host);
  service_worker_handler_->SetRenderFrameHost(host);
  if (security_handler_)
    security_handler_->SetRenderFrameHost(host);
  if (storage_handler_)
    storage_handler_->SetRenderFrameHost(host);
}

void RenderFrameDevToolsAgentHost::DisconnectWebContents() {
  if (pending_)
    DiscardPending();
  UpdateProtocolHandlers(nullptr);
  disconnected_ = std::move(current_);
  disconnected_->Detach();
  frame_tree_node_ = nullptr;
  in_navigation_protocol_message_buffer_.clear();
  navigating_handles_.clear();
  pending_handle_ = nullptr;
  WebContentsObserver::Observe(nullptr);
}

void RenderFrameDevToolsAgentHost::ConnectWebContents(WebContents* wc) {
  DCHECK(!current_);
  DCHECK(!pending_);
  RenderFrameHostImpl* host =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  DCHECK(host);
  frame_tree_node_ = host->frame_tree_node();
  current_ = std::move(disconnected_);
  SetPending(host);
  CommitPending();
  WebContentsObserver::Observe(WebContents::FromRenderFrameHost(host));
}

DevToolsAgentHost::Type RenderFrameDevToolsAgentHost::GetType() {
  return IsChildFrame() ? TYPE_FRAME : TYPE_WEB_CONTENTS;
}

std::string RenderFrameDevToolsAgentHost::GetTitle() {
  if (IsChildFrame())
    return GetURL().spec();
  if (WebContents* web_contents = GetWebContents())
    return base::UTF16ToUTF8(web_contents->GetTitle());
  return "";
}

GURL RenderFrameDevToolsAgentHost::GetURL() {
  // Order is important here.
  WebContents* web_contents = GetWebContents();
  if (web_contents && !IsChildFrame())
    return web_contents->GetVisibleURL();
  if (pending_)
    return pending_->host()->GetLastCommittedURL();
  if (current_)
    return current_->host()->GetLastCommittedURL();
  return GURL();
}

bool RenderFrameDevToolsAgentHost::Activate() {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(web_contents());
  if (wc) {
    wc->Activate();
    return true;
  }
  return false;
}

bool RenderFrameDevToolsAgentHost::Close() {
  if (web_contents()) {
    web_contents()->ClosePage();
    return true;
  }
  return false;
}

void RenderFrameDevToolsAgentHost::OnSwapCompositorFrame(
    const IPC::Message& message) {
  ViewHostMsg_SwapCompositorFrame::Param param;
  if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
    return;
  if (page_handler_)
    page_handler_->OnSwapCompositorFrame(
        std::move(std::get<1>(param).metadata));
  if (input_handler_)
    input_handler_->OnSwapCompositorFrame(std::get<1>(param).metadata);
  if (frame_trace_recorder_ && tracing_handler_->did_initiate_recording()) {
    frame_trace_recorder_->OnSwapCompositorFrame(
        current_ ? current_->host() : nullptr, std::get<1>(param).metadata);
  }
}

void RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame(
    cc::CompositorFrameMetadata frame_metadata) {
  if (page_handler_)
    page_handler_->OnSynchronousSwapCompositorFrame(std::move(frame_metadata));
  if (input_handler_)
    input_handler_->OnSwapCompositorFrame(frame_metadata);
  if (frame_trace_recorder_ && tracing_handler_->did_initiate_recording()) {
    frame_trace_recorder_->OnSynchronousSwapCompositorFrame(
        current_ ? current_->host() : nullptr,
        frame_metadata);
  }
}

void RenderFrameDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    RenderFrameHost* sender,
    const DevToolsMessageChunk& message) {
  if (current_ && current_->host() == sender)
    current_->ProcessChunkedMessageFromAgent(message);
  else if (pending_ && pending_->host() == sender)
    pending_->ProcessChunkedMessageFromAgent(message);
}

void RenderFrameDevToolsAgentHost::OnRequestNewWindow(
    RenderFrameHost* sender,
    int new_routing_id) {
  RenderFrameHostImpl* frame_host = RenderFrameHostImpl::FromID(
      sender->GetProcess()->GetID(), new_routing_id);

  bool success = false;
  if (IsAttached() && sender->GetRoutingID() != new_routing_id && frame_host) {
    scoped_refptr<DevToolsAgentHost> agent =
        DevToolsAgentHost::GetOrCreateFor(frame_host);
    success = static_cast<DevToolsAgentHostImpl*>(agent.get())->
        Inspect(agent->GetBrowserContext());
  }

  sender->Send(new DevToolsAgentMsg_RequestNewWindow_ACK(
      sender->GetRoutingID(), success));
}

bool RenderFrameDevToolsAgentHost::HasRenderFrameHost(
    RenderFrameHost* host) {
  return (current_ && current_->host() == host) ||
      (pending_ && pending_->host() == host);
}

bool RenderFrameDevToolsAgentHost::IsChildFrame() {
  return current_ && current_->host()->GetParent();
}

}  // namespace content
