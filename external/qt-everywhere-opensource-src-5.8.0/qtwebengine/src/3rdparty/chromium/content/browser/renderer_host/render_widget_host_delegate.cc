// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

bool RenderWidgetHostDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

bool RenderWidgetHostDelegate::HandleWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  return false;
}

bool RenderWidgetHostDelegate::PreHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  return false;
}

BrowserAccessibilityManager*
    RenderWidgetHostDelegate::GetRootBrowserAccessibilityManager() {
  return NULL;
}

BrowserAccessibilityManager*
    RenderWidgetHostDelegate::GetOrCreateRootBrowserAccessibilityManager() {
  return NULL;
}

// If a delegate does not override this, the RenderWidgetHostView will
// assume it is the sole platform event consumer.
RenderWidgetHostInputEventRouter*
RenderWidgetHostDelegate::GetInputEventRouter() {
  return nullptr;
}

// If a delegate does not override this, the RenderWidgetHostView will
// assume its own RenderWidgetHost should consume keyboard events.
RenderWidgetHostImpl* RenderWidgetHostDelegate::GetFocusedRenderWidgetHost(
    RenderWidgetHostImpl* receiving_widget) {
  return receiving_widget;
}

gfx::Rect RenderWidgetHostDelegate::GetRootWindowResizerRect(
    RenderWidgetHostImpl* render_widget_host) const {
  return gfx::Rect();
};

bool RenderWidgetHostDelegate::IsFullscreenForCurrentTab() const {
  return false;
}

blink::WebDisplayMode RenderWidgetHostDelegate::GetDisplayMode(
    RenderWidgetHostImpl* render_widget_host) const {
  return blink::WebDisplayModeBrowser;
}

bool RenderWidgetHostDelegate::HasMouseLock(
    RenderWidgetHostImpl* render_widget_host) {
  return false;
}

TextInputManager* RenderWidgetHostDelegate::GetTextInputManager() {
  return nullptr;
}

}  // namespace content
