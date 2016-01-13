// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_

#if defined(OS_MACOSX)
#import <Cocoa/Cocoa.h>
#endif

#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
@protocol RenderWidgetHostViewMacDelegate;
#endif

namespace gfx {
class Size;
}

namespace ui {
class FocusStoreGtk;
}

namespace content {
class RenderFrameHost;
class RenderWidgetHost;
class WebDragDestDelegate;
struct ContextMenuParams;

// This interface allows a client to extend the functionality of the
// WebContentsView implementation.
class CONTENT_EXPORT WebContentsViewDelegate {
 public:
  virtual ~WebContentsViewDelegate() {}

  // Returns a delegate to process drags not handled by content.
  virtual WebDragDestDelegate* GetDragDestDelegate() = 0;

  // Shows a context menu.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) = 0;

#if defined(USE_AURA)
  // These methods allow the embedder to intercept WebContentsViewWin's
  // implementation of these WebContentsView methods. See the WebContentsView
  // interface documentation for more information about these methods.
  virtual void StoreFocus() = 0;
  virtual void RestoreFocus() = 0;
  virtual bool Focus() = 0;
  virtual void TakeFocus(bool reverse) = 0;
  virtual void SizeChanged(const gfx::Size& size) = 0;
#elif defined(OS_MACOSX)
  // Returns a newly-created delegate for the RenderWidgetHostViewMac, to handle
  // events on the responder chain.
  virtual NSObject<RenderWidgetHostViewMacDelegate>*
      CreateRenderWidgetHostViewDelegate(
          RenderWidgetHost* render_widget_host) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_
