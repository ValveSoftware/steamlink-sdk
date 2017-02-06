// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_mus.h"

#include "build/build_config.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mus.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "third_party/WebKit/public/platform/WebDragOperation.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {

WebContentsViewMus::WebContentsViewMus(
    mus::Window* parent_window,
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** delegate_view)
    : web_contents_(web_contents), delegate_(delegate) {
  DCHECK(parent_window);
  *delegate_view = this;
  mus::Window* window = parent_window->window_tree()->NewWindow();
  window->SetVisible(true);
  window->SetBounds(gfx::Rect(300, 300));
  parent_window->AddChild(window);
  mus_window_.reset(new mus::ScopedWindowPtr(window));
}

WebContentsViewMus::~WebContentsViewMus() {}

void WebContentsViewMus::SizeChangedCommon(const gfx::Size& size) {
  if (web_contents_->GetInterstitialPage())
    web_contents_->GetInterstitialPage()->SetSize(size);
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
  mus_window_->window()->SetBounds(gfx::Rect(size));
}

gfx::NativeView WebContentsViewMus::GetNativeView() const {
  return aura_window_.get();
}

gfx::NativeView WebContentsViewMus::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : nullptr;
}

gfx::NativeWindow WebContentsViewMus::GetTopLevelNativeWindow() const {
  gfx::NativeWindow window = aura_window_->GetToplevelWindow();
  return window ? window : delegate_->GetNativeWindow();
}

void WebContentsViewMus::GetContainerBounds(gfx::Rect* out) const {
  *out = aura_window_->GetBoundsInScreen();
}

void WebContentsViewMus::SizeContents(const gfx::Size& size) {
  gfx::Rect bounds = aura_window_->bounds();
  if (bounds.size() != size) {
    bounds.set_size(size);
    aura_window_->SetBounds(bounds);
    SizeChangedCommon(size);
  } else {
    // Our size matches what we want but the renderers size may not match.
    // Pretend we were resized so that the renderers size is updated too.
    SizeChangedCommon(size);
  }
}

void WebContentsViewMus::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
}

gfx::Rect WebContentsViewMus::GetViewBounds() const {
  return aura_window_->GetBoundsInScreen();
}

#if defined(OS_MACOSX)
void WebContentsViewMus::SetAllowOtherViews(bool allow) {
}

bool WebContentsViewMus::GetAllowOtherViews() const {
  return false;
}
#endif

void WebContentsViewMus::CreateView(const gfx::Size& initial_size,
                                    gfx::NativeView context) {
  // We install a WindowDelegate so that the mus_window_ can track the size
  // of the |aura_window_|.
  aura_window_.reset(new aura::Window(this));
  aura_window_->set_owned_by_parent(false);
  aura_window_->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  aura_window_->Init(ui::LAYER_NOT_DRAWN);
  aura::Window* root_window = context ? context->GetRootWindow() : nullptr;
  if (root_window) {
    // There are places where there is no context currently because object
    // hierarchies are built before they're attached to a Widget. (See
    // views::WebView as an example; GetWidget() returns nullptr at the point
    // where we are created.)
    //
    // It should be OK to not set a default parent since such users will
    // explicitly add this WebContentsViewMus to their tree after they create
    // us.
    aura::client::ParentWindowWithContext(aura_window_.get(), root_window,
                                          root_window->GetBoundsInScreen());
  }
  aura_window_->layer()->SetMasksToBounds(true);
  aura_window_->SetName("WebContentsViewMus");
}

RenderWidgetHostViewBase* WebContentsViewMus::CreateViewForWidget(
    RenderWidgetHost* render_widget_host,
    bool is_guest_view_hack) {
  RenderWidgetHostViewBase* view = new RenderWidgetHostViewMus(
      mus_window_->window(), RenderWidgetHostImpl::From(render_widget_host));
  view->InitAsChild(GetNativeView());
  return view;
}

RenderWidgetHostViewBase* WebContentsViewMus::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return new RenderWidgetHostViewMus(
      mus_window_->window(), RenderWidgetHostImpl::From(render_widget_host));
}

void WebContentsViewMus::SetPageTitle(const base::string16& title) {}

void WebContentsViewMus::RenderViewCreated(RenderViewHost* host) {
}

void WebContentsViewMus::RenderViewSwappedIn(RenderViewHost* host) {
}

void WebContentsViewMus::SetOverscrollControllerEnabled(bool enabled) {
  // This should never override the setting of the embedder view.
}

#if defined(OS_MACOSX)
bool WebContentsViewMus::IsEventTracking() const {
  return false;
}

void WebContentsViewMus::CloseTabAfterEventTracking() {}
#endif

WebContents* WebContentsViewMus::web_contents() {
  return web_contents_;
}

void WebContentsViewMus::RestoreFocus() {
  // Focus is managed by mus, not the browser.
  NOTIMPLEMENTED();
}

void WebContentsViewMus::Focus() {
  // Focus is managed by mus, not the browser.
  NOTIMPLEMENTED();
}

void WebContentsViewMus::StoreFocus() {
  // Focus is managed by mus, not the browser.
  NOTIMPLEMENTED();
}

DropData* WebContentsViewMus::GetDropData() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebContentsViewMus::UpdateDragCursor(WebDragOperation operation) {
  // TODO(fsamuel): Implement cursor in Mus.
}

void WebContentsViewMus::GotFocus() {
  // Focus is managed by mus, not the browser.
  NOTIMPLEMENTED();
}

void WebContentsViewMus::TakeFocus(bool reverse) {
  // Focus is managed by mus, not the browser.
  NOTIMPLEMENTED();
}

void WebContentsViewMus::ShowContextMenu(RenderFrameHost* render_frame_host,
                                         const ContextMenuParams& params) {
}

void WebContentsViewMus::StartDragging(const DropData& drop_data,
                                       WebDragOperationsMask ops,
                                       const gfx::ImageSkia& image,
                                       const gfx::Vector2d& image_offset,
                                       const DragEventSourceInfo& event_info) {
  // TODO(fsamuel): Implement drag and drop.
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewMus, aura::WindowDelegate implementation:

gfx::Size WebContentsViewMus::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size WebContentsViewMus::GetMaximumSize() const {
  return gfx::Size();
}

void WebContentsViewMus::OnBoundsChanged(const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  SizeChangedCommon(new_bounds.size());
  if (delegate_)
    delegate_->SizeChanged(new_bounds.size());
}

gfx::NativeCursor WebContentsViewMus::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int WebContentsViewMus::GetNonClientComponent(const gfx::Point& point) const {
  return HTCLIENT;
}

bool WebContentsViewMus::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool WebContentsViewMus::CanFocus() {
  return false;
}

void WebContentsViewMus::OnCaptureLost() {}

void WebContentsViewMus::OnPaint(const ui::PaintContext& context) {}

void WebContentsViewMus::OnDeviceScaleFactorChanged(float device_scale_factor) {
}

void WebContentsViewMus::OnWindowDestroying(aura::Window* window) {}

void WebContentsViewMus::OnWindowDestroyed(aura::Window* window) {}

void WebContentsViewMus::OnWindowTargetVisibilityChanged(bool visible) {}

bool WebContentsViewMus::HasHitTestMask() const {
  return false;
}

void WebContentsViewMus::GetHitTestMask(gfx::Path* mask) const {}

}  // namespace content
