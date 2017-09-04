// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_COLOR_PICKER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_COLOR_PICKER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {
class WebMouseEvent;
}

namespace content {

class RenderWidgetHostImpl;

namespace devtools {
namespace page {

class ColorPicker {
 public:
  typedef base::Callback<void(int, int, int, int)> ColorPickedCallback;

  explicit ColorPicker(ColorPickedCallback callback);
  virtual ~ColorPicker();

  void SetRenderWidgetHost(RenderWidgetHostImpl* host);
  void SetEnabled(bool enabled);
  void OnSwapCompositorFrame();

 private:
  void UpdateFrame();
  void ResetFrame();
  void FrameUpdated(const SkBitmap&, ReadbackResponse);
  bool HandleMouseEvent(const blink::WebMouseEvent& event);
  void UpdateCursor();

  ColorPickedCallback callback_;
  bool enabled_;
  SkBitmap frame_;
  int last_cursor_x_;
  int last_cursor_y_;
  RenderWidgetHost::MouseEventCallback mouse_event_callback_;
  RenderWidgetHostImpl* host_;
  base::WeakPtrFactory<ColorPicker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ColorPicker);
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_COLOR_PICKER_H_
