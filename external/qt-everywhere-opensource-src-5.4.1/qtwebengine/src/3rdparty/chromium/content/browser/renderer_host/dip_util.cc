// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dip_util.h"

#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/layout.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_conversions.h"

namespace content {

float GetScaleFactorForView(const RenderWidgetHostView* view) {
  return ui::GetScaleFactorForNativeView(view ? view->GetNativeView() : NULL);
}

gfx::Point ConvertViewPointToDIP(const RenderWidgetHostView* view,
                                 const gfx::Point& point_in_pixel) {
  return gfx::ToFlooredPoint(
      gfx::ScalePoint(point_in_pixel, 1.0f / GetScaleFactorForView(view)));
}

gfx::Size ConvertViewSizeToPixel(const RenderWidgetHostView* view,
                                 const gfx::Size& size_in_dip) {
  return ConvertSizeToPixel(GetScaleFactorForView(view), size_in_dip);
}

gfx::Rect ConvertViewRectToPixel(const RenderWidgetHostView* view,
                                 const gfx::Rect& rect_in_dip) {
  return ConvertRectToPixel(GetScaleFactorForView(view), rect_in_dip);
}

gfx::Size ConvertSizeToDIP(float scale_factor,
                           const gfx::Size& size_in_pixel) {
  return gfx::ToFlooredSize(
      gfx::ScaleSize(size_in_pixel, 1.0f / scale_factor));
}

gfx::Rect ConvertRectToDIP(float scale_factor,
                           const gfx::Rect& rect_in_pixel) {
  return gfx::ToFlooredRectDeprecated(
      gfx::ScaleRect(rect_in_pixel, 1.0f / scale_factor));
}

gfx::Size ConvertSizeToPixel(float scale_factor,
                             const gfx::Size& size_in_dip) {
  return gfx::ToFlooredSize(gfx::ScaleSize(size_in_dip, scale_factor));
}

gfx::Rect ConvertRectToPixel(float scale_factor,
                             const gfx::Rect& rect_in_dip) {
    return gfx::ToFlooredRectDeprecated(
        gfx::ScaleRect(rect_in_dip, scale_factor));
}

}  // namespace content
