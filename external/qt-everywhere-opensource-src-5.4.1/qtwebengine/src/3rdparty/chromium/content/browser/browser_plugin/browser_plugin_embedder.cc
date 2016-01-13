// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/values.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

BrowserPluginEmbedder::BrowserPluginEmbedder(WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      weak_ptr_factory_(this) {
}

BrowserPluginEmbedder::~BrowserPluginEmbedder() {
}

// static
BrowserPluginEmbedder* BrowserPluginEmbedder::Create(
    WebContentsImpl* web_contents) {
  return new BrowserPluginEmbedder(web_contents);
}

void BrowserPluginEmbedder::DragEnteredGuest(BrowserPluginGuest* guest) {
  guest_dragging_over_ = guest->AsWeakPtr();
}

void BrowserPluginEmbedder::DragLeftGuest(BrowserPluginGuest* guest) {
  // Avoid race conditions in switching between guests being hovered over by
  // only un-setting if the caller is marked as the guest being dragged over.
  if (guest_dragging_over_.get() == guest) {
    guest_dragging_over_.reset();
  }
}

void BrowserPluginEmbedder::StartDrag(BrowserPluginGuest* guest) {
  guest_started_drag_ = guest->AsWeakPtr();
}

WebContentsImpl* BrowserPluginEmbedder::GetWebContents() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

BrowserPluginGuestManager*
BrowserPluginEmbedder::GetBrowserPluginGuestManager() const {
  return GetWebContents()->GetBrowserContext()->GetGuestManager();
}

bool BrowserPluginEmbedder::DidSendScreenRectsCallback(
   WebContents* guest_web_contents) {
  static_cast<RenderViewHostImpl*>(
      guest_web_contents->GetRenderViewHost())->SendScreenRects();
  // Not handled => Iterate over all guests.
  return false;
}

void BrowserPluginEmbedder::DidSendScreenRects() {
  GetBrowserPluginGuestManager()->ForEachGuest(
          GetWebContents(), base::Bind(
              &BrowserPluginEmbedder::DidSendScreenRectsCallback,
              base::Unretained(this)));
}

bool BrowserPluginEmbedder::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginEmbedder, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER_GENERIC(DragHostMsg_UpdateDragCursor,
                                OnUpdateDragCursor(&handled));
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedder::DragSourceEndedAt(int client_x, int client_y,
    int screen_x, int screen_y, blink::WebDragOperation operation) {
  if (guest_started_drag_.get()) {
    gfx::Point guest_offset =
        guest_started_drag_->GetScreenCoordinates(gfx::Point());
    guest_started_drag_->DragSourceEndedAt(client_x - guest_offset.x(),
        client_y - guest_offset.y(), screen_x, screen_y, operation);
  }
}

void BrowserPluginEmbedder::SystemDragEnded() {
  // When the embedder's drag/drop operation ends, we need to pass the message
  // to the guest that initiated the drag/drop operation. This will ensure that
  // the guest's RVH state is reset properly.
  if (guest_started_drag_.get())
    guest_started_drag_->EndSystemDrag();
  guest_started_drag_.reset();
  guest_dragging_over_.reset();
}

void BrowserPluginEmbedder::OnUpdateDragCursor(bool* handled) {
  *handled = (guest_dragging_over_.get() != NULL);
}

void BrowserPluginEmbedder::OnGuestCallback(
    int instance_id,
    const BrowserPluginHostMsg_Attach_Params& params,
    const base::DictionaryValue* extra_params,
    WebContents* guest_web_contents) {
  BrowserPluginGuest* guest = guest_web_contents ?
      static_cast<WebContentsImpl*>(guest_web_contents)->
          GetBrowserPluginGuest() : NULL;
  if (!guest) {
    scoped_ptr<base::DictionaryValue> copy_extra_params(
        extra_params->DeepCopy());
    guest_web_contents = GetBrowserPluginGuestManager()->CreateGuest(
        GetWebContents()->GetSiteInstance(),
        instance_id,
        copy_extra_params.Pass());
    guest = guest_web_contents
                ? static_cast<WebContentsImpl*>(guest_web_contents)
                      ->GetBrowserPluginGuest()
                : NULL;
  }

  if (guest)
    guest->Attach(GetWebContents(), params, *extra_params);
}

void BrowserPluginEmbedder::OnAttach(
    int instance_id,
    const BrowserPluginHostMsg_Attach_Params& params,
    const base::DictionaryValue& extra_params) {
  GetBrowserPluginGuestManager()->MaybeGetGuestByInstanceIDOrKill(
      instance_id, GetWebContents()->GetRenderProcessHost()->GetID(),
      base::Bind(&BrowserPluginEmbedder::OnGuestCallback,
                 base::Unretained(this),
                 instance_id,
                 params,
                 &extra_params));
}

}  // namespace content
