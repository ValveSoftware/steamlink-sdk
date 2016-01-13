// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/views_delegate.h"

namespace views {

// static
const char WebView::kViewClassName[] = "WebView";

////////////////////////////////////////////////////////////////////////////////
// WebView, public:

WebView::WebView(content::BrowserContext* browser_context)
    : holder_(new NativeViewHost()),
      embed_fullscreen_widget_mode_enabled_(false),
      is_embedding_fullscreen_widget_(false),
      browser_context_(browser_context),
      allow_accelerators_(false) {
  AddChildView(holder_);  // Takes ownership of |holder_|.
  NativeViewAccessibility::RegisterWebView(this);
}

WebView::~WebView() {
  SetWebContents(NULL);  // Make sure all necessary tear-down takes place.
  NativeViewAccessibility::UnregisterWebView(this);
}

content::WebContents* WebView::GetWebContents() {
  if (!web_contents()) {
    wc_owner_.reset(CreateWebContents(browser_context_));
    wc_owner_->SetDelegate(this);
    SetWebContents(wc_owner_.get());
  }
  return web_contents();
}

void WebView::SetWebContents(content::WebContents* replacement) {
  if (replacement == web_contents())
    return;
  DetachWebContents();
  WebContentsObserver::Observe(replacement);
  // web_contents() now returns |replacement| from here onwards.
  if (wc_owner_ != replacement)
    wc_owner_.reset();
  if (embed_fullscreen_widget_mode_enabled_) {
    is_embedding_fullscreen_widget_ =
        web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  } else {
    DCHECK(!is_embedding_fullscreen_widget_);
  }
  AttachWebContents();
  NotifyMaybeTextInputClientChanged();
}

void WebView::SetEmbedFullscreenWidgetMode(bool enable) {
  DCHECK(!web_contents())
      << "Cannot change mode while a WebContents is attached.";
  embed_fullscreen_widget_mode_enabled_ = enable;
}

void WebView::LoadInitialURL(const GURL& url) {
  GetWebContents()->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

void WebView::SetFastResize(bool fast_resize) {
  holder_->set_fast_resize(fast_resize);
}

void WebView::OnWebContentsFocused(content::WebContents* web_contents) {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(this);
}

void WebView::SetPreferredSize(const gfx::Size& preferred_size) {
  preferred_size_ = preferred_size;
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// WebView, View overrides:

const char* WebView::GetClassName() const {
  return kViewClassName;
}

ui::TextInputClient* WebView::GetTextInputClient() {
  // This function delegates the text input handling to the underlying
  // content::RenderWidgetHostView.  So when the underlying RWHV is destroyed or
  // replaced with another one, we have to notify the FocusManager through
  // FocusManager::OnTextInputClientChanged() that the focused TextInputClient
  // needs to be updated.
  if (switches::IsTextInputFocusManagerEnabled() &&
      web_contents() && !web_contents()->IsBeingDestroyed()) {
    content::RenderWidgetHostView* host_view =
        is_embedding_fullscreen_widget_ ?
        web_contents()->GetFullscreenRenderWidgetHostView() :
        web_contents()->GetRenderWidgetHostView();
    if (host_view)
      return host_view->GetTextInputClient();
  }
  return NULL;
}

void WebView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // In most cases, the holder is simply sized to fill this WebView's bounds.
  // Only WebContentses that are in fullscreen mode and being screen-captured
  // will engage the special layout/sizing behavior.
  gfx::Rect holder_bounds(bounds().size());
  if (!embed_fullscreen_widget_mode_enabled_ ||
      !web_contents() ||
      web_contents()->GetCapturerCount() == 0 ||
      web_contents()->GetPreferredSize().IsEmpty() ||
      !(is_embedding_fullscreen_widget_ ||
        (web_contents()->GetDelegate() &&
         web_contents()->GetDelegate()->
             IsFullscreenForTabOrPending(web_contents())))) {
    holder_->SetBoundsRect(holder_bounds);
    return;
  }

  // Size the holder to the capture video resolution and center it.  If this
  // WebView is not large enough to contain the holder at the preferred size,
  // scale down to fit (preserving aspect ratio).
  const gfx::Size capture_size = web_contents()->GetPreferredSize();
  if (capture_size.width() <= holder_bounds.width() &&
      capture_size.height() <= holder_bounds.height()) {
    // No scaling, just centering.
    holder_bounds.ClampToCenteredSize(capture_size);
  } else {
    // Scale down, preserving aspect ratio, and center.
    // TODO(miu): This is basically media::ComputeLetterboxRegion(), and it
    // looks like others have written this code elsewhere.  Let's considate
    // into a shared function ui/gfx/geometry or around there.
    const int64 x = static_cast<int64>(capture_size.width()) *
        holder_bounds.height();
    const int64 y = static_cast<int64>(capture_size.height()) *
        holder_bounds.width();
    if (y < x) {
      holder_bounds.ClampToCenteredSize(gfx::Size(
          holder_bounds.width(), static_cast<int>(y / capture_size.width())));
    } else {
      holder_bounds.ClampToCenteredSize(gfx::Size(
          static_cast<int>(x / capture_size.height()), holder_bounds.height()));
    }
  }

  holder_->SetBoundsRect(holder_bounds);
}

void WebView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add)
    AttachWebContents();
}

bool WebView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  if (allow_accelerators_)
    return FocusManager::IsTabTraversalKeyEvent(event);

  // Don't look-up accelerators or tab-traversal if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return web_contents() && !web_contents()->IsCrashed();
}

bool WebView::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  return !!web_contents();
}

void WebView::OnFocus() {
  if (!web_contents())
    return;
  if (is_embedding_fullscreen_widget_) {
    content::RenderWidgetHostView* const current_fs_view =
        web_contents()->GetFullscreenRenderWidgetHostView();
    if (current_fs_view)
      current_fs_view->Focus();
  } else {
    web_contents()->Focus();
  }
}

void WebView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (web_contents())
    web_contents()->FocusThroughTabTraversal(reverse);
}

void WebView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
}

gfx::NativeViewAccessible WebView::GetNativeViewAccessible() {
  if (web_contents()) {
    content::RenderWidgetHostView* host_view =
        web_contents()->GetRenderWidgetHostView();
    if (host_view)
      return host_view->GetNativeViewAccessible();
  }
  return View::GetNativeViewAccessible();
}

gfx::Size WebView::GetPreferredSize() const {
  if (preferred_size_ == gfx::Size())
    return View::GetPreferredSize();
  else
    return preferred_size_;
}

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsDelegate implementation:

void WebView::WebContentsFocused(content::WebContents* web_contents) {
  DCHECK(wc_owner_.get());
  // The WebView is only the delegate of WebContentses it creates itself.
  OnWebContentsFocused(wc_owner_.get());
}

bool WebView::EmbedsFullscreenWidget() const {
  DCHECK(wc_owner_.get());
  return embed_fullscreen_widget_mode_enabled_;
}

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsObserver implementation:

void WebView::RenderViewDeleted(content::RenderViewHost* render_view_host) {
  NotifyMaybeTextInputClientChanged();
}

void WebView::RenderViewHostChanged(content::RenderViewHost* old_host,
                                    content::RenderViewHost* new_host) {
  FocusManager* const focus_manager = GetFocusManager();
  if (focus_manager && focus_manager->GetFocusedView() == this)
    OnFocus();
  NotifyMaybeTextInputClientChanged();
}

void WebView::DidShowFullscreenWidget(int routing_id) {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(true);
}

void WebView::DidDestroyFullscreenWidget(int routing_id) {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(false);
}

void WebView::DidToggleFullscreenModeForTab(bool entered_fullscreen) {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(entered_fullscreen);
}

////////////////////////////////////////////////////////////////////////////////
// WebView, private:

void WebView::AttachWebContents() {
  // Prevents attachment if the WebView isn't already in a Widget, or it's
  // already attached.
  if (!GetWidget() || !web_contents())
    return;

  const gfx::NativeView view_to_attach = is_embedding_fullscreen_widget_ ?
      web_contents()->GetFullscreenRenderWidgetHostView()->GetNativeView() :
      web_contents()->GetNativeView();
  OnBoundsChanged(bounds());
  if (holder_->native_view() == view_to_attach)
    return;
  holder_->Attach(view_to_attach);

  // The view will not be focused automatically when it is attached, so we need
  // to pass on focus to it if the FocusManager thinks the view is focused. Note
  // that not every Widget has a focus manager.
  FocusManager* const focus_manager = GetFocusManager();
  if (focus_manager && focus_manager->GetFocusedView() == this)
    OnFocus();

#if defined(OS_WIN)
  if (!is_embedding_fullscreen_widget_) {
    web_contents()->SetParentNativeViewAccessible(
        parent()->GetNativeViewAccessible());
  }
#endif
}

void WebView::DetachWebContents() {
  if (web_contents()) {
    holder_->Detach();
#if defined(OS_WIN)
    if (!is_embedding_fullscreen_widget_)
      web_contents()->SetParentNativeViewAccessible(NULL);
#endif
  }
}

void WebView::ReattachForFullscreenChange(bool enter_fullscreen) {
  DCHECK(embed_fullscreen_widget_mode_enabled_);
  const bool web_contents_has_separate_fs_widget =
      web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  if (is_embedding_fullscreen_widget_ || web_contents_has_separate_fs_widget) {
    // Shutting down or starting up the embedding of the separate fullscreen
    // widget.  Need to detach and re-attach to a different native view.
    DetachWebContents();
    is_embedding_fullscreen_widget_ =
        enter_fullscreen && web_contents_has_separate_fs_widget;
    AttachWebContents();
  } else {
    // Entering or exiting "non-Flash" fullscreen mode, where the native view is
    // the same.  So, do not change attachment.
    OnBoundsChanged(bounds());
  }
  NotifyMaybeTextInputClientChanged();
}

void WebView::NotifyMaybeTextInputClientChanged() {
  // Update the TextInputClient as needed; see GetTextInputClient().
  FocusManager* const focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->OnTextInputClientChanged(this);
}

content::WebContents* WebView::CreateWebContents(
      content::BrowserContext* browser_context) {
  content::WebContents* contents = NULL;
  if (ViewsDelegate::views_delegate) {
    contents = ViewsDelegate::views_delegate->CreateWebContents(
        browser_context, NULL);
  }

  if (!contents) {
    content::WebContents::CreateParams create_params(
        browser_context, NULL);
    return content::WebContents::Create(create_params);
  }

  return contents;
}

}  // namespace views
