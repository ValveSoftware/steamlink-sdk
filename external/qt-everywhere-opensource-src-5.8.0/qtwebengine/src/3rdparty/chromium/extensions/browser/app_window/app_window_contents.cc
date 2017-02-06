// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_contents.h"

#include <string>
#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

AppWindowContentsImpl::AppWindowContentsImpl(AppWindow* host)
    : host_(host), is_blocking_requests_(false), is_window_ready_(false) {}

AppWindowContentsImpl::~AppWindowContentsImpl() {}

void AppWindowContentsImpl::Initialize(content::BrowserContext* context,
                                       content::RenderFrameHost* creator_frame,
                                       const GURL& url) {
  url_ = url;

  content::WebContents::CreateParams create_params(
      context, creator_frame->GetSiteInstance());
  create_params.opener_render_process_id = creator_frame->GetProcess()->GetID();
  create_params.opener_render_frame_id = creator_frame->GetRoutingID();
  web_contents_.reset(content::WebContents::Create(create_params));

  Observe(web_contents_.get());
  web_contents_->GetMutableRendererPrefs()->
      browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();
}

void AppWindowContentsImpl::LoadContents(int32_t creator_process_id) {
  // If the new view is in the same process as the creator, block the created
  // RVH from loading anything until the background page has had a chance to
  // do any initialization it wants. If it's a different process, the new RVH
  // shouldn't communicate with the background page anyway (e.g. sandboxed).
  if (web_contents_->GetMainFrame()->GetProcess()->GetID() ==
      creator_process_id) {
    SuspendRenderFrameHost(web_contents_->GetMainFrame());
  } else {
    VLOG(1) << "AppWindow created in new process ("
            << web_contents_->GetMainFrame()->GetProcess()->GetID()
            << ") != creator (" << creator_process_id << "). Routing disabled.";
  }

  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
}

void AppWindowContentsImpl::NativeWindowChanged(
    NativeAppWindow* native_app_window) {
  base::ListValue args;
  base::DictionaryValue* dictionary = new base::DictionaryValue();
  args.Append(dictionary);
  host_->GetSerializedState(dictionary);

  content::RenderFrameHost* rfh = web_contents_->GetMainFrame();
  rfh->Send(new ExtensionMsg_MessageInvoke(
      rfh->GetRoutingID(), host_->extension_id(), "app.window",
      "updateAppWindowProperties", args, false));
}

void AppWindowContentsImpl::NativeWindowClosed() {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AppWindowClosed(rvh->GetRoutingID()));
}

void AppWindowContentsImpl::DispatchWindowShownForTests() const {
  base::ListValue args;
  content::RenderFrameHost* rfh = web_contents_->GetMainFrame();
  rfh->Send(new ExtensionMsg_MessageInvoke(
      rfh->GetRoutingID(), host_->extension_id(), "app.window",
      "appWindowShownForTests", args, false));
}

void AppWindowContentsImpl::OnWindowReady() {
  is_window_ready_ = true;
  if (is_blocking_requests_) {
    is_blocking_requests_ = false;
    content::ResourceDispatcherHost::ResumeBlockedRequestsForFrameFromUI(
        web_contents_->GetMainFrame());
  }
}

content::WebContents* AppWindowContentsImpl::GetWebContents() const {
  return web_contents_.get();
}

WindowController* AppWindowContentsImpl::GetWindowController() const {
  return nullptr;
}

bool AppWindowContentsImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppWindowContentsImpl, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_UpdateDraggableRegions,
                        UpdateDraggableRegions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AppWindowContentsImpl::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  if (!is_window_ready_)
    host_->OnReadyToCommitFirstNavigation();
}

void AppWindowContentsImpl::UpdateDraggableRegions(
    const std::vector<DraggableRegion>& regions) {
  host_->UpdateDraggableRegions(regions);
}

void AppWindowContentsImpl::SuspendRenderFrameHost(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  // Don't bother blocking requests if the renderer side is already good to go.
  if (is_window_ready_)
    return;
  is_blocking_requests_ = true;
  content::ResourceDispatcherHost::BlockRequestsForFrameFromUI(rfh);
}

}  // namespace extensions
