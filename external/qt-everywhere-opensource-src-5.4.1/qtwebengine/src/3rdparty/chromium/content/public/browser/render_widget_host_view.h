// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class TextInputClient;
}

namespace content {

class RenderWidgetHost;
class RenderWidgetHostViewFrameSubscriber;

// RenderWidgetHostView is an interface implemented by an object that acts as
// the "View" portion of a RenderWidgetHost. The RenderWidgetHost and its
// associated RenderProcessHost own the "Model" in this case which is the
// child renderer process. The View is responsible for receiving events from
// the surrounding environment and passing them to the RenderWidgetHost, and
// for actually displaying the content of the RenderWidgetHost when it
// changes.
//
// RenderWidgetHostView Class Hierarchy:
//   RenderWidgetHostView - Public interface.
//   RenderWidgetHostViewBase - Common implementation between platforms.
//   RenderWidgetHostViewAura, ... - Platform specific implementations.
class CONTENT_EXPORT RenderWidgetHostView {
 public:
  virtual ~RenderWidgetHostView() {}

  // Initialize this object for use as a drawing area.  |parent_view| may be
  // left as NULL on platforms where a parent view is not required to initialize
  // a child window.
  virtual void InitAsChild(gfx::NativeView parent_view) = 0;

  // Returns the associated RenderWidgetHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() const = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Tells the View to size and move itself to the specified size and point in
  // screen space.
  virtual void SetBounds(const gfx::Rect& rect) = 0;

  // Retrieves the native view used to contain plugins and identify the
  // renderer in IPC messages.
  virtual gfx::NativeView GetNativeView() const = 0;
  virtual gfx::NativeViewId GetNativeViewId() const = 0;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // Returns a ui::TextInputClient to support text input or NULL if this RWHV
  // doesn't support text input.
  // Note: Not all the platforms use ui::InputMethod and ui::TextInputClient for
  // text input.  Some platforms (Mac and Android for example) use their own
  // text input system.
  virtual ui::TextInputClient* GetTextInputClient() = 0;

  // Set focus to the associated View component.
  virtual void Focus() = 0;
  // Returns true if the View currently has the focus.
  virtual bool HasFocus() const = 0;
  // Returns true is the current display surface is available.
  virtual bool IsSurfaceAvailableForCopy() const = 0;

  // Shows/hides the view.  These must always be called together in pairs.
  // It is not legal to call Hide() multiple times in a row.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Whether the view is showing.
  virtual bool IsShowing() = 0;

  // Retrieve the bounds of the View, in screen coordinates.
  virtual gfx::Rect GetViewBounds() const = 0;

  // Returns true if the View's context menu is showing.
  virtual bool IsShowingContextMenu() const = 0;

  // Tells the View whether the context menu is showing.
  virtual void SetShowingContextMenu(bool showing) = 0;

  // Returns the currently selected text.
  virtual base::string16 GetSelectedText() const = 0;

  // Subclasses should override this method to do what is appropriate to set
  // the background to be transparent or opaque.
  virtual void SetBackgroundOpaque(bool opaque) = 0;
  virtual bool GetBackgroundOpaque() = 0;

  // Return value indicates whether the mouse is locked successfully or not.
  virtual bool LockMouse() = 0;
  virtual void UnlockMouse() = 0;
  // Returns true if the mouse pointer is currently locked.
  virtual bool IsMouseLocked() = 0;

  // Retrives the size of the viewport for the visible region. May be smaller
  // than the view size if a portion of the view is obstructed (e.g. by a
  // virtual keyboard).
  virtual gfx::Size GetVisibleViewportSize() const = 0;

  // Set insets for the visible region of the root window. Used to compute the
  // visible viewport.
  virtual void SetInsets(const gfx::Insets& insets) = 0;

  // Begin subscribing for presentation events and captured frames.
  // |subscriber| is now owned by this object, it will be called only on the
  // UI thread.
  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) = 0;

  // End subscribing for frame presentation events. FrameSubscriber will be
  // deleted after this call.
  virtual void EndFrameSubscription() = 0;

#if defined(OS_MACOSX)
  // Set the view's active state (i.e., tint state of controls).
  virtual void SetActive(bool active) = 0;

  // Tells the view whether or not to accept first responder status.  If |flag|
  // is true, the view does not accept first responder status and instead
  // manually becomes first responder when it receives a mouse down event.  If
  // |flag| is false, the view participates in the key-view chain as normal.
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) = 0;

  // Notifies the view that its enclosing window has changed visibility
  // (minimized/unminimized, app hidden/unhidden, etc).
  // TODO(stuartmorgan): This is a temporary plugin-specific workaround for
  // <http://crbug.com/34266>. Once that is fixed, this (and the corresponding
  // message and renderer-side handling) can be removed in favor of using
  // WasHidden/WasShown.
  virtual void SetWindowVisibility(bool visible) = 0;

  // Informs the view that its containing window's frame changed.
  virtual void WindowFrameChanged() = 0;

  // Brings up the dictionary showing a definition for the selected text.
  virtual void ShowDefinitionForSelection() = 0;

  // Returns |true| if Mac OS X text to speech is supported.
  virtual bool SupportsSpeech() const = 0;
  // Tells the view to speak the currently selected text.
  virtual void SpeakSelection() = 0;
  // Returns |true| if text is currently being spoken by Mac OS X.
  virtual bool IsSpeaking() const = 0;
  // Stops speaking, if it is currently in progress.
  virtual void StopSpeaking() = 0;
#endif  // defined(OS_MACOSX)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
