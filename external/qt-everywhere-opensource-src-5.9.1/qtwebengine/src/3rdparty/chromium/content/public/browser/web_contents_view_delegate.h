// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_

#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(__OBJC__)
@protocol RenderWidgetHostViewMacDelegate;
#endif

namespace gfx {
class Size;
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
  virtual ~WebContentsViewDelegate();

  // Returns the native window containing the WebContents, or nullptr if the
  // WebContents is not in any window.
  virtual gfx::NativeWindow GetNativeWindow();

  // Returns a delegate to process drags not handled by content.
  virtual WebDragDestDelegate* GetDragDestDelegate();

  // Shows a context menu.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params);

  // These methods allow the embedder to intercept a WebContentsView's
  // implementation of these methods. See the WebContentsView interface
  // documentation for more information about these methods.
  virtual void StoreFocus();
  virtual void RestoreFocus();
  virtual bool Focus();
  virtual void TakeFocus(bool reverse);
  virtual void SizeChanged(const gfx::Size& size);

  // Returns a newly-created delegate for the RenderWidgetHostViewMac, to handle
  // events on the responder chain.
#if defined(__OBJC__)
  virtual NSObject<RenderWidgetHostViewMacDelegate>*
      CreateRenderWidgetHostViewDelegate(
          RenderWidgetHost* render_widget_host);
#else
  virtual void* CreateRenderWidgetHostViewDelegate(
      RenderWidgetHost* render_widget_host);
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_H_
