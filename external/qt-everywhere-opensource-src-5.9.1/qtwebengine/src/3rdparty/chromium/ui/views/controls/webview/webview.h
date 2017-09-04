// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/views/view.h"

namespace views {

class NativeViewHost;

// Provides a view of a WebContents instance.  WebView can be used standalone,
// creating and displaying an internally-owned WebContents; or within a full
// browser where the browser swaps its own WebContents instances in/out (e.g.,
// for browser tabs).
//
// WebView creates and owns a single child view, a NativeViewHost, which will
// hold and display the native view provided by a WebContents.
//
// EmbedFullscreenWidgetMode: When enabled, WebView will observe for WebContents
// fullscreen changes and automatically swap the normal native view with the
// fullscreen native view (if different).  In addition, if the WebContents is
// being screen-captured, the view will be centered within WebView, sized to
// the aspect ratio of the capture video resolution, and scaling will be avoided
// whenever possible.
class WEBVIEW_EXPORT WebView : public View,
                               public content::RenderProcessHostObserver,
                               public content::WebContentsDelegate,
                               public content::WebContentsObserver {
 public:
  static const char kViewClassName[];

  explicit WebView(content::BrowserContext* browser_context);
  ~WebView() override;

  // This creates a WebContents if none is yet associated with this WebView. The
  // WebView owns this implicitly created WebContents.
  content::WebContents* GetWebContents();

  // WebView does not assume ownership of WebContents set via this method, only
  // those it implicitly creates via GetWebContents() above.
  void SetWebContents(content::WebContents* web_contents);

  // If |mode| is true, WebView will register itself with WebContents as a
  // WebContentsObserver, monitor for the showing/destruction of fullscreen
  // render widgets, and alter its child view hierarchy to embed the fullscreen
  // widget or restore the normal WebContentsView.
  void SetEmbedFullscreenWidgetMode(bool mode);

  content::BrowserContext* browser_context() { return browser_context_; }

  // Loads the initial URL to display in the attached WebContents. Creates the
  // WebContents if none is attached yet. Note that this is intended as a
  // convenience for loading the initial URL, and so URLs are navigated with
  // PAGE_TRANSITION_AUTO_TOPLEVEL, so this is not intended as a general purpose
  // navigation method - use WebContents' API directly.
  void LoadInitialURL(const GURL& url);

  // Controls how the attached WebContents is resized.
  // false = WebContents' views' bounds are updated continuously as the
  //         WebView's bounds change (default).
  // true  = WebContents' views' position is updated continuously but its size
  //         is not (which may result in some clipping or under-painting) until
  //         a continuous size operation completes. This allows for smoother
  //         resizing performance during interactive resizes and animations.
  void SetFastResize(bool fast_resize);

  // Set the background color to use while resizing with a clip. This is white
  // by default.
  void SetResizeBackgroundColor(SkColor resize_background_color);

  // When used to host UI, we need to explicitly allow accelerators to be
  // processed. Default is false.
  void set_allow_accelerators(bool allow_accelerators) {
    allow_accelerators_ = allow_accelerators;
  }

  // Sets the preferred size. If empty, View's implementation of
  // GetPreferredSize() is used.
  void SetPreferredSize(const gfx::Size& preferred_size);

  // Overridden from View:
  const char* GetClassName() const override;

  NativeViewHost* holder() { return holder_; }

 protected:
  // Swaps the owned WebContents |wc_owner_| with |new_web_contents|. Returns
  // the previously owned WebContents.
  std::unique_ptr<content::WebContents> SwapWebContents(
      std::unique_ptr<content::WebContents> new_web_contents);

  // Called when the web contents is successfully attached.
  virtual void OnWebContentsAttached() {}

  // Overridden from View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::Size GetPreferredSize() const override;

  // Overridden from content::RenderProcessHostObserver:
  void RenderProcessExited(content::RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Overridden from content::WebContentsDelegate:
  bool EmbedsFullscreenWidget() const override;

  // Overridden from content::WebContentsObserver:
  void RenderViewReady() override;
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;
  void DidShowFullscreenWidget() override;
  void DidDestroyFullscreenWidget() override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  // Workaround for MSVC++ linker bug/feature that requires
  // instantiation of the inline IPC::Listener methods in all translation units.
  void OnChannelConnected(int32_t peer_id) override {}
  void OnChannelError() override {}
  void OnBadMessageReceived(const IPC::Message& message) override {}
  void OnWebContentsFocused() override;

 private:
  friend class WebViewUnitTest;

  void AttachWebContents();
  void DetachWebContents();
  void ReattachForFullscreenChange(bool enter_fullscreen);
  void NotifyAccessibilityWebContentsChanged();

  // Create a regular or test web contents (based on whether we're running
  // in a unit test or not).
  content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context);

  NativeViewHost* const holder_;
  // Non-NULL if |web_contents()| was created and is owned by this WebView.
  std::unique_ptr<content::WebContents> wc_owner_;
  // The RenderProcessHost to which this RenderProcessHostObserver is added.
  // Since WebView::GetTextInputClient is relying on RWHV::GetTextInputClient,
  // we have to observe the lifecycle of the underlying RWHV through
  // RenderProcessHostObserver.
  content::RenderProcessHost* observing_render_process_host_;
  // When true, WebView auto-embeds fullscreen widgets as a child view.
  bool embed_fullscreen_widget_mode_enabled_;
  // Set to true while WebView is embedding a fullscreen widget view as a child
  // view instead of the normal WebContentsView render view. Note: This will be
  // false in the case of non-Flash fullscreen.
  bool is_embedding_fullscreen_widget_;
  content::BrowserContext* browser_context_;
  bool allow_accelerators_;
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(WebView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
