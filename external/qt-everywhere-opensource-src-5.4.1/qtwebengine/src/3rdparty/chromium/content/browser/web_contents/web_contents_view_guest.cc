// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_guest.h"

#include "build/build_config.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/drag_messages.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {

WebContentsViewGuest::WebContentsViewGuest(
    WebContentsImpl* web_contents,
    BrowserPluginGuest* guest,
    scoped_ptr<WebContentsView> platform_view,
    RenderViewHostDelegateView* platform_view_delegate_view)
    : web_contents_(web_contents),
      guest_(guest),
      platform_view_(platform_view.Pass()),
      platform_view_delegate_view_(platform_view_delegate_view) {
}

WebContentsViewGuest::~WebContentsViewGuest() {
}

gfx::NativeView WebContentsViewGuest::GetNativeView() const {
  return platform_view_->GetNativeView();
}

gfx::NativeView WebContentsViewGuest::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewGuest::GetTopLevelNativeWindow() const {
  return guest_->embedder_web_contents()->GetTopLevelNativeWindow();
}

void WebContentsViewGuest::OnGuestInitialized(WebContentsView* parent_view) {
#if defined(USE_AURA)
  // In aura, ScreenPositionClient doesn't work properly if we do
  // not have the native view associated with this WebContentsViewGuest in the
  // view hierarchy. We add this view as embedder's child here.
  // This would go in WebContentsViewGuest::CreateView, but that is too early to
  // access embedder_web_contents(). Therefore, we do it here.
  parent_view->GetNativeView()->AddChild(platform_view_->GetNativeView());
#endif  // defined(USE_AURA)
}

ContextMenuParams WebContentsViewGuest::ConvertContextMenuParams(
    const ContextMenuParams& params) const {
#if defined(USE_AURA)
  // Context menu uses ScreenPositionClient::ConvertPointToScreen() in aura
  // to calculate popup position. Guest's native view
  // (platform_view_->GetNativeView()) is part of the embedder's view hierarchy,
  // but is placed at (0, 0) w.r.t. the embedder's position. Therefore, |offset|
  // is added to |params|.
  gfx::Rect embedder_bounds;
  guest_->embedder_web_contents()->GetView()->GetContainerBounds(
      &embedder_bounds);
  gfx::Rect guest_bounds;
  GetContainerBounds(&guest_bounds);

  gfx::Vector2d offset = guest_bounds.origin() - embedder_bounds.origin();
  ContextMenuParams params_in_embedder = params;
  params_in_embedder.x += offset.x();
  params_in_embedder.y += offset.y();
  return params_in_embedder;
#else
  return params;
#endif
}

void WebContentsViewGuest::GetContainerBounds(gfx::Rect* out) const {
  // We need embedder container's bounds to calculate our bounds.
  guest_->embedder_web_contents()->GetView()->GetContainerBounds(out);
  gfx::Point guest_coordinates = guest_->GetScreenCoordinates(gfx::Point());
  out->Offset(guest_coordinates.x(), guest_coordinates.y());
  out->set_size(size_);
}

void WebContentsViewGuest::SizeContents(const gfx::Size& size) {
  size_ = size;
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewGuest::SetInitialFocus() {
  platform_view_->SetInitialFocus();
}

gfx::Rect WebContentsViewGuest::GetViewBounds() const {
  return gfx::Rect(size_);
}

#if defined(OS_MACOSX)
void WebContentsViewGuest::SetAllowOverlappingViews(bool overlapping) {
  platform_view_->SetAllowOverlappingViews(overlapping);
}

bool WebContentsViewGuest::GetAllowOverlappingViews() const {
  return platform_view_->GetAllowOverlappingViews();
}

void WebContentsViewGuest::SetOverlayView(
    WebContentsView* overlay, const gfx::Point& offset) {
  platform_view_->SetOverlayView(overlay, offset);
}

void WebContentsViewGuest::RemoveOverlayView() {
  platform_view_->RemoveOverlayView();
}
#endif

void WebContentsViewGuest::CreateView(const gfx::Size& initial_size,
                                      gfx::NativeView context) {
  platform_view_->CreateView(initial_size, context);
  size_ = initial_size;
}

RenderWidgetHostViewBase* WebContentsViewGuest::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return static_cast<RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  RenderWidgetHostViewBase* platform_widget =
      platform_view_->CreateViewForWidget(render_widget_host);

  RenderWidgetHostViewBase* view = new RenderWidgetHostViewGuest(
      render_widget_host,
      guest_,
      platform_widget);

  return view;
}

RenderWidgetHostViewBase* WebContentsViewGuest::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return platform_view_->CreateViewForPopupWidget(render_widget_host);
}

void WebContentsViewGuest::SetPageTitle(const base::string16& title) {
}

void WebContentsViewGuest::RenderViewCreated(RenderViewHost* host) {
  platform_view_->RenderViewCreated(host);
}

void WebContentsViewGuest::RenderViewSwappedIn(RenderViewHost* host) {
  platform_view_->RenderViewSwappedIn(host);
}

void WebContentsViewGuest::SetOverscrollControllerEnabled(bool enabled) {
  // This should never override the setting of the embedder view.
}

#if defined(OS_MACOSX)
bool WebContentsViewGuest::IsEventTracking() const {
  return false;
}

void WebContentsViewGuest::CloseTabAfterEventTracking() {
}
#endif

WebContents* WebContentsViewGuest::web_contents() {
  return web_contents_;
}

void WebContentsViewGuest::RestoreFocus() {
  platform_view_->RestoreFocus();
}

void WebContentsViewGuest::Focus() {
  platform_view_->Focus();
}

void WebContentsViewGuest::StoreFocus() {
  platform_view_->StoreFocus();
}

DropData* WebContentsViewGuest::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewGuest::UpdateDragCursor(WebDragOperation operation) {
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          guest_->embedder_web_contents()->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view)
    view->UpdateDragCursor(operation);
}

void WebContentsViewGuest::GotFocus() {
}

void WebContentsViewGuest::TakeFocus(bool reverse) {
}

void WebContentsViewGuest::ShowContextMenu(RenderFrameHost* render_frame_host,
                                           const ContextMenuParams& params) {
  platform_view_delegate_view_->ShowContextMenu(
      render_frame_host, ConvertContextMenuParams(params));
}

void WebContentsViewGuest::StartDragging(
    const DropData& drop_data,
    WebDragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  WebContentsImpl* embedder_web_contents = guest_->embedder_web_contents();
  embedder_web_contents->GetBrowserPluginEmbedder()->StartDrag(guest_);
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          embedder_web_contents->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view) {
    RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.StartDrag"));
    view->StartDragging(drop_data, ops, image, image_offset, event_info);
  } else {
    embedder_web_contents->SystemDragEnded();
  }
}

}  // namespace content
