// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
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
                               public content::WebContentsDelegate,
                               public content::WebContentsObserver {
 public:
  static const char kViewClassName[];

  explicit WebView(content::BrowserContext* browser_context);
  virtual ~WebView();

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

  content::WebContents* web_contents() const {
    return content::WebContentsObserver::web_contents();
  }

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

  // Called when the WebContents is focused.
  // TODO(beng): This view should become a WebContentsViewObserver when a
  //             WebContents is attached, and not rely on the delegate to
  //             forward this notification.
  void OnWebContentsFocused(content::WebContents* web_contents);

  // When used to host UI, we need to explicitly allow accelerators to be
  // processed. Default is false.
  void set_allow_accelerators(bool allow_accelerators) {
    allow_accelerators_ = allow_accelerators;
  }

  // Sets the preferred size. If empty, View's implementation of
  // GetPreferredSize() is used.
  void SetPreferredSize(const gfx::Size& preferred_size);

  // Overridden from View:
  virtual const char* GetClassName() const OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;

 protected:
  // Overridden from View:
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(
      const ui::KeyEvent& event) OVERRIDE;
  virtual bool IsFocusable() const OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual void WebContentsFocused(content::WebContents* web_contents) OVERRIDE;
  virtual bool EmbedsFullscreenWidget() const OVERRIDE;

  // Overridden from content::WebContentsObserver:
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewHostChanged(
      content::RenderViewHost* old_host,
      content::RenderViewHost* new_host) OVERRIDE;
  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE;
  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE;
  virtual void DidToggleFullscreenModeForTab(bool entered_fullscreen) OVERRIDE;
  // Workaround for MSVC++ linker bug/feature that requires
  // instantiation of the inline IPC::Listener methods in all translation units.
  virtual void OnChannelConnected(int32 peer_id) OVERRIDE {}
  virtual void OnChannelError() OVERRIDE {}
  virtual void OnBadMessageReceived(const IPC::Message& message) OVERRIDE {}

 private:
  void AttachWebContents();
  void DetachWebContents();
  void ReattachForFullscreenChange(bool enter_fullscreen);
  void NotifyMaybeTextInputClientChanged();

  // Create a regular or test web contents (based on whether we're running
  // in a unit test or not).
  content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context);

  NativeViewHost* const holder_;
  // Non-NULL if |web_contents()| was created and is owned by this WebView.
  scoped_ptr<content::WebContents> wc_owner_;
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
