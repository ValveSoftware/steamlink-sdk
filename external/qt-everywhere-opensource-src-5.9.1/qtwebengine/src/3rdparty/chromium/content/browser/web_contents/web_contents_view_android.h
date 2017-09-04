// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {
class ContentViewCoreImpl;
class RenderWidgetHostViewAndroid;
class SynchronousCompositorClient;
class WebContentsImpl;

// Android-specific implementation of the WebContentsView.
class WebContentsViewAndroid : public WebContentsView,
                               public RenderViewHostDelegateView {
 public:
  WebContentsViewAndroid(WebContentsImpl* web_contents,
                         WebContentsViewDelegate* delegate);
  ~WebContentsViewAndroid() override;

  // Sets the interface to the view system. ContentViewCoreImpl is owned
  // by its Java ContentViewCore counterpart, whose lifetime is managed
  // by the UI frontend.
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);

  void set_synchronous_compositor_client(SynchronousCompositorClient* client) {
    synchronous_compositor_client_ = client;
  }

  SynchronousCompositorClient* synchronous_compositor_client() const {
    return synchronous_compositor_client_;
  }

  // WebContentsView implementation --------------------------------------------
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  void GetScreenInfo(ScreenInfo* screen_info) const override;
  void GetContainerBounds(gfx::Rect* out) const override;
  void SizeContents(const gfx::Size& size) override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(const gfx::Size& initial_size,
                  gfx::NativeView context) override;
  RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host,
      bool is_guest_view_hack) override;
  RenderWidgetHostViewBase* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const base::string16& title) override;
  void RenderViewCreated(RenderViewHost* host) override;
  void RenderViewSwappedIn(RenderViewHost* host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;

  // Backend implementation of RenderViewHostDelegateView.
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;
  void ShowPopupMenu(RenderFrameHost* render_frame_host,
                     const gfx::Rect& bounds,
                     int item_height,
                     double item_font_size,
                     int selected_item,
                     const std::vector<MenuItem>& items,
                     bool right_aligned,
                     bool allow_multiple_selection) override;
  void HidePopupMenu() override;
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(blink::WebDragOperation operation) override;
  void GotFocus() override;
  void TakeFocus(bool reverse) override;

  void OnDragEntered(const std::vector<DropData::Metadata>& metadata,
                     const gfx::Point& location,
                     const gfx::Point& screen_location);
  void OnDragUpdated(const gfx::Point& location,
                     const gfx::Point& screen_location);
  void OnDragExited();
  void OnPerformDrop(DropData* drop_data,
                     const gfx::Point& location,
                     const gfx::Point& screen_location);
  void OnDragEnded();

 private:
  // The WebContents whose contents we display.
  WebContentsImpl* web_contents_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  // Interface for extensions to WebContentsView. Used to show the context menu.
  std::unique_ptr<WebContentsViewDelegate> delegate_;

  // The native view associated with the contents of the web.
  ui::ViewAndroid view_;

  // Interface used to get notified of events from the synchronous compositor.
  SynchronousCompositorClient* synchronous_compositor_client_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
