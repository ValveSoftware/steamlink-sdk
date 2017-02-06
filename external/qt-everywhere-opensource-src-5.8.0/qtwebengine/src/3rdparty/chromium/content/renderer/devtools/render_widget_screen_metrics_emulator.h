// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_RENDER_WIDGET_SCREEN_METRICS_EMULATOR_H_
#define CONTENT_RENDERER_DEVTOOLS_RENDER_WIDGET_SCREEN_METRICS_EMULATOR_H_

#include <memory>

#include "content/common/resize_params.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"

namespace gfx {
class PointF;
class Rect;
}

namespace content {

class RenderWidgetScreenMetricsEmulatorDelegate;
struct ContextMenuParams;

// RenderWidgetScreenMetricsEmulator class manages screen emulation inside a
// RenderWidget. This includes resizing, placing view on the screen at desired
// position, changing device scale factor, and scaling down the whole
// widget if required to fit into the browser window.
class RenderWidgetScreenMetricsEmulator {
 public:
  RenderWidgetScreenMetricsEmulator(
      RenderWidgetScreenMetricsEmulatorDelegate* delegate,
      const blink::WebDeviceEmulationParams& params,
      const ResizeParams& resize_params,
      const gfx::Rect& view_screen_rect,
      const gfx::Rect& window_screen_rect);
  virtual ~RenderWidgetScreenMetricsEmulator();

  // Scale and offset used to convert between host coordinates
  // and webwidget coordinates.
  const gfx::Size& original_size() const {
    return original_resize_params_.new_size;
  }

  float scale() const { return scale_; }
  const gfx::PointF& offset() const { return offset_; }
  const gfx::Rect& applied_widget_rect() const { return applied_widget_rect_; }
  const blink::WebScreenInfo& original_screen_info() const {
    return original_resize_params_.screen_info;
  }
  const gfx::Rect& original_screen_rect() const {
    return original_view_screen_rect_;
  }

  void ChangeEmulationParams(const blink::WebDeviceEmulationParams& params);

  // The following methods alter handlers' behavior for messages related to
  // widget size and position.
  void OnResize(const ResizeParams& params);
  void OnUpdateWindowScreenRect(const gfx::Rect& window_screen_rect);
  void OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                           const gfx::Rect& window_screen_rect);
  void OnShowContextMenu(ContextMenuParams* params);
  gfx::Rect AdjustValidationMessageAnchor(const gfx::Rect& anchor);

  // Apply parameters to the render widget.
  void Apply();

 private:
  RenderWidgetScreenMetricsEmulatorDelegate* const delegate_;

  // Parameters as passed by RenderWidget::EnableScreenMetricsEmulation.
  blink::WebDeviceEmulationParams emulation_params_;

  // The computed scale and offset used to fit widget into browser window.
  float scale_;
  gfx::PointF offset_;

  // Widget rect as passed to webwidget.
  gfx::Rect applied_widget_rect_;

  // Original values to restore back after emulation ends.
  ResizeParams original_resize_params_;
  gfx::Rect original_view_screen_rect_;
  gfx::Rect original_window_screen_rect_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetScreenMetricsEmulator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_RENDER_WIDGET_SCREEN_METRICS_EMULATOR_H_
