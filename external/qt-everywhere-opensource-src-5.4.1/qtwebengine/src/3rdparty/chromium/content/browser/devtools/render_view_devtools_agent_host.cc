// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_view_devtools_agent_host.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/devtools_power_handler.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/devtools_tracing_handler.h"
#include "content/browser/devtools/renderer_overrides_handler.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"

#if defined(OS_ANDROID)
#include "content/browser/power_save_blocker_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#endif

namespace content {

typedef std::vector<RenderViewDevToolsAgentHost*> Instances;

namespace {
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

//Returns RenderViewDevToolsAgentHost attached to any of RenderViewHost
//instances associated with |web_contents|
static RenderViewDevToolsAgentHost* FindAgentHost(WebContents* web_contents) {
  if (g_instances == NULL)
    return NULL;
  RenderViewHostDelegate* delegate =
      static_cast<WebContentsImpl*>(web_contents);
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    RenderViewHost* rvh = (*it)->render_view_host();
    if (rvh && rvh->GetDelegate() == delegate)
      return *it;
  }
  return NULL;
}

static RenderViewDevToolsAgentHost* FindAgentHost(RenderViewHost* rvh) {
  if (g_instances == NULL)
    return NULL;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if (rvh == (*it)->render_view_host())
      return *it;
  }
  return NULL;
}

}  // namespace

scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(WebContents* web_contents) {
  RenderViewDevToolsAgentHost* result = FindAgentHost(web_contents);
  if (!result)
    result = new RenderViewDevToolsAgentHost(web_contents->GetRenderViewHost());
  return result;
}

// static
scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(RenderViewHost* rvh) {
  RenderViewDevToolsAgentHost* result = FindAgentHost(rvh);
  if (!result)
    result = new RenderViewDevToolsAgentHost(rvh);
  return result;
}

// static
bool DevToolsAgentHost::HasFor(RenderViewHost* rvh) {
  return FindAgentHost(rvh) != NULL;
}

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  if (g_instances == NULL)
    return false;
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (!devtools_manager)
    return false;
  RenderViewHostDelegate* delegate =
      static_cast<WebContentsImpl*>(web_contents);
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    RenderViewHost* rvh = (*it)->render_view_host_;
    if (rvh && rvh->GetDelegate() != delegate)
      continue;
    if ((*it)->IsAttached())
      return true;
  }
  return false;
}

//static
std::vector<RenderViewHost*> DevToolsAgentHost::GetValidRenderViewHosts() {
  std::vector<RenderViewHost*> result;
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed contents.
    if (!widget->GetProcess()->HasConnection())
      continue;
    if (!widget->IsRenderView())
      continue;

    RenderViewHost* rvh = RenderViewHost::From(widget);
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (!web_contents)
      continue;

    // Don't report a RenderViewHost if it is not the current RenderViewHost
    // for some WebContents (this filters out pre-render RVHs and similar).
    // However report a RenderViewHost created for an out of process iframe.
    // TODO (kaznacheev): Revisit this when it is clear how OOP iframes
    // interact with pre-rendering.
    // TODO (kaznacheev): GetMainFrame() call is a temporary hack. Iterate over
    // all RenderFrameHost instances when multiple OOP frames are supported.
    if (rvh != web_contents->GetRenderViewHost() &&
        !rvh->GetMainFrame()->IsCrossProcessSubframe()) {
      continue;
    }

    result.push_back(rvh);
  }
  return result;
}

// static
void RenderViewDevToolsAgentHost::OnCancelPendingNavigation(
    RenderViewHost* pending,
    RenderViewHost* current) {
  RenderViewDevToolsAgentHost* agent_host = FindAgentHost(pending);
  if (!agent_host)
    return;
  agent_host->DisconnectRenderViewHost();
  agent_host->ConnectRenderViewHost(current);
}

// static
bool RenderViewDevToolsAgentHost::DispatchIPCMessage(
    RenderViewHost* source,
    const IPC::Message& message) {
  RenderViewDevToolsAgentHost* agent_host = FindAgentHost(source);
  return agent_host && agent_host->DispatchIPCMessage(message);
}

RenderViewDevToolsAgentHost::RenderViewDevToolsAgentHost(RenderViewHost* rvh)
    : render_view_host_(NULL),
      overrides_handler_(new RendererOverridesHandler(this)),
      tracing_handler_(
          new DevToolsTracingHandler(DevToolsTracingHandler::Renderer)),
      power_handler_(new DevToolsPowerHandler()),
      reattaching_(false) {
  SetRenderViewHost(rvh);
  DevToolsProtocol::Notifier notifier(base::Bind(
      &RenderViewDevToolsAgentHost::OnDispatchOnInspectorFrontend,
      base::Unretained(this)));
  overrides_handler_->SetNotifier(notifier);
  tracing_handler_->SetNotifier(notifier);
  power_handler_->SetNotifier(notifier);
  g_instances.Get().push_back(this);
  AddRef();  // Balanced in RenderViewHostDestroyed.
}

RenderViewHost* RenderViewDevToolsAgentHost::GetRenderViewHost() {
  return render_view_host_;
}

void RenderViewDevToolsAgentHost::DispatchOnInspectorBackend(
    const std::string& message) {
  std::string error_message;

  scoped_ptr<base::DictionaryValue> message_dict(
      DevToolsProtocol::ParseMessage(message, &error_message));
  scoped_refptr<DevToolsProtocol::Command> command =
      DevToolsProtocol::ParseCommand(message_dict.get(), &error_message);

  if (command) {
    scoped_refptr<DevToolsProtocol::Response> overridden_response;

    DevToolsManagerDelegate* delegate =
        DevToolsManagerImpl::GetInstance()->delegate();
    if (delegate) {
      scoped_ptr<base::DictionaryValue> overridden_response_value(
          delegate->HandleCommand(this, message_dict.get()));
      if (overridden_response_value)
        overridden_response = DevToolsProtocol::ParseResponse(
            overridden_response_value.get());
    }
    if (!overridden_response)
      overridden_response = overrides_handler_->HandleCommand(command);
    if (!overridden_response)
      overridden_response = tracing_handler_->HandleCommand(command);
    if (!overridden_response)
      overridden_response = power_handler_->HandleCommand(command);
    if (overridden_response) {
      if (!overridden_response->is_async_promise())
        OnDispatchOnInspectorFrontend(overridden_response->Serialize());
      return;
    }
  }

  IPCDevToolsAgentHost::DispatchOnInspectorBackend(message);
}

void RenderViewDevToolsAgentHost::SendMessageToAgent(IPC::Message* msg) {
  if (!render_view_host_)
    return;
  msg->set_routing_id(render_view_host_->GetRoutingID());
  render_view_host_->Send(msg);
}

void RenderViewDevToolsAgentHost::OnClientAttached() {
  if (!render_view_host_)
    return;

  InnerOnClientAttached();

  // TODO(kaznacheev): Move this call back to DevToolsManagerImpl when
  // extensions::ProcessManager no longer relies on this notification.
  if (!reattaching_)
    DevToolsManagerImpl::GetInstance()->NotifyObservers(this, true);
}

void RenderViewDevToolsAgentHost::InnerOnClientAttached() {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
      render_view_host_->GetProcess()->GetID());

#if defined(OS_ANDROID)
  power_save_blocker_.reset(
      static_cast<PowerSaveBlockerImpl*>(
          PowerSaveBlocker::Create(
              PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
              "DevTools").release()));
  if (render_view_host_->GetView()) {
    power_save_blocker_.get()->
        InitDisplaySleepBlocker(render_view_host_->GetView()->GetNativeView());
  }
#endif
}

void RenderViewDevToolsAgentHost::OnClientDetached() {
#if defined(OS_ANDROID)
  power_save_blocker_.reset();
#endif
  overrides_handler_->OnClientDetached();
  tracing_handler_->OnClientDetached();
  ClientDetachedFromRenderer();
}

void RenderViewDevToolsAgentHost::ClientDetachedFromRenderer() {
  if (!render_view_host_)
    return;

  InnerClientDetachedFromRenderer();

  // TODO(kaznacheev): Move this call back to DevToolsManagerImpl when
  // extensions::ProcessManager no longer relies on this notification.
  if (!reattaching_)
    DevToolsManagerImpl::GetInstance()->NotifyObservers(this, false);
}

void RenderViewDevToolsAgentHost::InnerClientDetachedFromRenderer() {
  bool process_has_agents = false;
  RenderProcessHost* render_process_host = render_view_host_->GetProcess();
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if (*it == this || !(*it)->IsAttached())
      continue;
    RenderViewHost* rvh = (*it)->render_view_host();
    if (rvh && rvh->GetProcess() == render_process_host)
      process_has_agents = true;
  }

  // We are the last to disconnect from the renderer -> revoke permissions.
  if (!process_has_agents) {
    ChildProcessSecurityPolicyImpl::GetInstance()->RevokeReadRawCookies(
        render_process_host->GetID());
  }
}

RenderViewDevToolsAgentHost::~RenderViewDevToolsAgentHost() {
  Instances::iterator it = std::find(g_instances.Get().begin(),
                                     g_instances.Get().end(),
                                     this);
  if (it != g_instances.Get().end())
    g_instances.Get().erase(it);
}

void RenderViewDevToolsAgentHost::AboutToNavigateRenderView(
    RenderViewHost* dest_rvh) {
  if (!render_view_host_)
    return;

  if (render_view_host_ == dest_rvh && static_cast<RenderViewHostImpl*>(
          render_view_host_)->render_view_termination_status() ==
              base::TERMINATION_STATUS_STILL_RUNNING)
    return;
  ReattachToRenderViewHost(dest_rvh);
}

void RenderViewDevToolsAgentHost::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  if (new_host != render_view_host_) {
    // AboutToNavigateRenderView was not called for renderer-initiated
    // navigation.
    ReattachToRenderViewHost(new_host);
  }
}

void
RenderViewDevToolsAgentHost::ReattachToRenderViewHost(RenderViewHost* rvh) {
  DCHECK(!reattaching_);
  reattaching_ = true;
  DisconnectRenderViewHost();
  ConnectRenderViewHost(rvh);
  reattaching_ = false;
}

void RenderViewDevToolsAgentHost::RenderViewDeleted(RenderViewHost* rvh) {
  if (rvh != render_view_host_)
    return;

  DCHECK(render_view_host_);
  scoped_refptr<RenderViewDevToolsAgentHost> protect(this);
  NotifyCloseListener();
  ClearRenderViewHost();
  Release();
}

void RenderViewDevToolsAgentHost::RenderProcessGone(
    base::TerminationStatus status) {
  switch(status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
      RenderViewCrashed();
      break;
    default:
      break;
  }
}

void RenderViewDevToolsAgentHost::DidAttachInterstitialPage() {
  if (!render_view_host_)
    return;
  // The rvh set in AboutToNavigateRenderView turned out to be interstitial.
  // Connect back to the real one.
  WebContents* web_contents =
    WebContents::FromRenderViewHost(render_view_host_);
  if (!web_contents)
    return;
  DisconnectRenderViewHost();
  ConnectRenderViewHost(web_contents->GetRenderViewHost());
}

void RenderViewDevToolsAgentHost::Observe(int type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED) {
    bool visible = *Details<bool>(details).ptr();
    overrides_handler_->OnVisibilityChanged(visible);
  }
}

void RenderViewDevToolsAgentHost::SetRenderViewHost(RenderViewHost* rvh) {
  DCHECK(!render_view_host_);
  render_view_host_ = rvh;

  WebContentsObserver::Observe(WebContents::FromRenderViewHost(rvh));

  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(render_view_host_));
}

void RenderViewDevToolsAgentHost::ClearRenderViewHost() {
  DCHECK(render_view_host_);
  registrar_.Remove(
      this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(render_view_host_));
  render_view_host_ = NULL;
}

void RenderViewDevToolsAgentHost::ConnectRenderViewHost(RenderViewHost* rvh) {
  SetRenderViewHost(rvh);
  if (IsAttached())
    Reattach(state_);
}

void RenderViewDevToolsAgentHost::DisconnectRenderViewHost() {
  ClientDetachedFromRenderer();
  ClearRenderViewHost();
}

void RenderViewDevToolsAgentHost::RenderViewCrashed() {
  scoped_refptr<DevToolsProtocol::Notification> notification =
      DevToolsProtocol::CreateNotification(
          devtools::Inspector::targetCrashed::kName, NULL);
  DevToolsManagerImpl::GetInstance()->
      DispatchOnInspectorFrontend(this, notification->Serialize());
}

bool RenderViewDevToolsAgentHost::DispatchIPCMessage(
    const IPC::Message& msg) {
  if (!render_view_host_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewDevToolsAgentHost, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAgentRuntimeState,
                        OnSaveAgentRuntimeState)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_SwapCompositorFrame,
                                handled = false; OnSwapCompositorFrame(msg))
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderViewDevToolsAgentHost::OnSwapCompositorFrame(
    const IPC::Message& message) {
  ViewHostMsg_SwapCompositorFrame::Param param;
  if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
    return;
  overrides_handler_->OnSwapCompositorFrame(param.b.metadata);
}

void RenderViewDevToolsAgentHost::SynchronousSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!render_view_host_)
    return;
  overrides_handler_->OnSwapCompositorFrame(frame_metadata);
}

void RenderViewDevToolsAgentHost::OnSaveAgentRuntimeState(
    const std::string& state) {
  if (!render_view_host_)
    return;
  state_ = state;
}

void RenderViewDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const std::string& message) {
  if (!render_view_host_)
    return;
  DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(
      this, message);
}

}  // namespace content
