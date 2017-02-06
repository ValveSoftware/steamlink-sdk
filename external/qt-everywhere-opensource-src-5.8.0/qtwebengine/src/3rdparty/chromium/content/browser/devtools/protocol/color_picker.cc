// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/color_picker.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {
namespace devtools {
namespace page {

ColorPicker::ColorPicker(ColorPickedCallback callback)
    : callback_(callback),
      enabled_(false),
      last_cursor_x_(-1),
      last_cursor_y_(-1),
      host_(nullptr),
      weak_factory_(this) {
  mouse_event_callback_ = base::Bind(
      &ColorPicker::HandleMouseEvent,
      base::Unretained(this));
}

ColorPicker::~ColorPicker() {
}

void ColorPicker::SetRenderWidgetHost(RenderWidgetHostImpl* host) {
  if (host_ == host)
    return;

  if (enabled_ && host_)
    host_->RemoveMouseEventCallback(mouse_event_callback_);
  ResetFrame();
  host_ = host;
  if (enabled_ && host)
    host->AddMouseEventCallback(mouse_event_callback_);
}

void ColorPicker::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;
  if (!host_)
    return;

  if (enabled) {
    host_->AddMouseEventCallback(mouse_event_callback_);
    UpdateFrame();
  } else {
    host_->RemoveMouseEventCallback(mouse_event_callback_);
    ResetFrame();

    WebCursor pointer_cursor;
    WebCursor::CursorInfo cursor_info;
    cursor_info.type = blink::WebCursorInfo::TypePointer;
    pointer_cursor.InitFromCursorInfo(cursor_info);
    host_->SetCursor(pointer_cursor);
  }
}

void ColorPicker::OnSwapCompositorFrame() {
  if (enabled_)
    UpdateFrame();
}

void ColorPicker::UpdateFrame() {
  if (!host_)
    return;
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(host_->GetView());
  if (!view)
    return;

  gfx::Size size = view->GetViewBounds().size();
  view->CopyFromCompositingSurface(
      gfx::Rect(size), size,
      base::Bind(&ColorPicker::FrameUpdated,
                 weak_factory_.GetWeakPtr()),
      kN32_SkColorType);
}

void ColorPicker::ResetFrame() {
  frame_.reset();
  last_cursor_x_ = -1;
  last_cursor_y_ = -1;
}

void ColorPicker::FrameUpdated(const SkBitmap& bitmap,
                               ReadbackResponse response) {
  if (!enabled_)
    return;

  if (response == READBACK_SUCCESS) {
    frame_ = bitmap;
    UpdateCursor();
  }
}

bool ColorPicker::HandleMouseEvent(const blink::WebMouseEvent& event) {
  last_cursor_x_ = event.x;
  last_cursor_y_ = event.y;
  if (frame_.drawsNothing())
    return true;

  if (event.button == blink::WebMouseEvent::ButtonLeft &&
      event.type == blink::WebInputEvent::MouseDown) {
    if (last_cursor_x_ < 0 || last_cursor_x_ >= frame_.width() ||
        last_cursor_y_ < 0 || last_cursor_y_ >= frame_.height()) {
      return true;
    }

    SkAutoLockPixels lock_image(frame_);
    SkColor sk_color = frame_.getColor(last_cursor_x_, last_cursor_y_);
    callback_.Run(SkColorGetR(sk_color), SkColorGetG(sk_color),
                  SkColorGetB(sk_color), SkColorGetA(sk_color));
  }
  UpdateCursor();
  return true;
}

void ColorPicker::UpdateCursor() {
  if (!host_ || frame_.drawsNothing())
    return;

  if (last_cursor_x_ < 0 || last_cursor_x_ >= frame_.width() ||
      last_cursor_y_ < 0 || last_cursor_y_ >= frame_.height()) {
    return;
  }

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  if (!view)
    return;

  // Due to platform limitations, we are using two different cursors
  // depending on the platform. Mac and Win have large cursors with two circles
  // for original spot and its magnified projection; Linux gets smaller (64 px)
  // magnified projection only with centered hotspot.
  // Mac Retina requires cursor to be > 120px in order to render smoothly.

#if defined(OS_LINUX)
  const float kCursorSize = 63;
  const float kDiameter = 63;
  const float kHotspotOffset = 32;
  const float kHotspotRadius = 0;
  const float kPixelSize = 9;
#else
  const float kCursorSize = 150;
  const float kDiameter = 110;
  const float kHotspotOffset = 25;
  const float kHotspotRadius = 5;
  const float kPixelSize = 10;
#endif

  blink::WebScreenInfo screen_info;
  view->GetScreenInfo(&screen_info);
  double device_scale_factor = screen_info.deviceScaleFactor;

  SkBitmap result;
  result.allocN32Pixels(kCursorSize * device_scale_factor,
                        kCursorSize * device_scale_factor);
  result.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(result);
  canvas.scale(device_scale_factor, device_scale_factor);
  canvas.translate(0.5f, 0.5f);

  SkPaint paint;

  // Paint original spot with cross.
  if (kHotspotRadius > 0) {
    paint.setStrokeWidth(1);
    paint.setAntiAlias(false);
    paint.setColor(SK_ColorDKGRAY);
    paint.setStyle(SkPaint::kStroke_Style);

    canvas.drawLine(kHotspotOffset, kHotspotOffset - 2 * kHotspotRadius,
                    kHotspotOffset, kHotspotOffset - kHotspotRadius,
                    paint);
    canvas.drawLine(kHotspotOffset, kHotspotOffset + kHotspotRadius,
                    kHotspotOffset, kHotspotOffset + 2 * kHotspotRadius,
                    paint);
    canvas.drawLine(kHotspotOffset - 2 * kHotspotRadius, kHotspotOffset,
                    kHotspotOffset - kHotspotRadius, kHotspotOffset,
                    paint);
    canvas.drawLine(kHotspotOffset + kHotspotRadius, kHotspotOffset,
                    kHotspotOffset + 2 * kHotspotRadius, kHotspotOffset,
                    paint);

    paint.setStrokeWidth(2);
    paint.setAntiAlias(true);
    canvas.drawCircle(kHotspotOffset, kHotspotOffset, kHotspotRadius, paint);
  }

  // Clip circle for magnified projection.
  float padding = (kCursorSize - kDiameter) / 2;
  SkPath clip_path;
  clip_path.addOval(SkRect::MakeXYWH(padding, padding, kDiameter, kDiameter));
  clip_path.close();
  canvas.clipPath(clip_path, SkRegion::kIntersect_Op, true);

  // Project pixels.
  int pixel_count = kDiameter / kPixelSize;
  SkRect src_rect = SkRect::MakeXYWH(last_cursor_x_ - pixel_count / 2,
                                     last_cursor_y_ - pixel_count / 2,
                                     pixel_count, pixel_count);
  SkRect dst_rect = SkRect::MakeXYWH(padding, padding, kDiameter, kDiameter);
  canvas.drawBitmapRect(frame_, src_rect, dst_rect, NULL);

  // Paint grid.
  paint.setStrokeWidth(1);
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorGRAY);
  for (int i = 0; i < pixel_count; ++i) {
    canvas.drawLine(padding + i * kPixelSize, padding,
                    padding + i * kPixelSize, kCursorSize - padding, paint);
    canvas.drawLine(padding, padding + i * kPixelSize,
                    kCursorSize - padding, padding + i * kPixelSize, paint);
  }

  // Paint central pixel in red.
  SkRect pixel = SkRect::MakeXYWH((kCursorSize - kPixelSize) / 2,
                                  (kCursorSize - kPixelSize) / 2,
                                  kPixelSize, kPixelSize);
  paint.setColor(SK_ColorRED);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas.drawRect(pixel, paint);

  // Paint outline.
  paint.setStrokeWidth(2);
  paint.setColor(SK_ColorDKGRAY);
  paint.setAntiAlias(true);
  canvas.drawCircle(kCursorSize / 2, kCursorSize / 2, kDiameter / 2, paint);

  WebCursor cursor;
  WebCursor::CursorInfo cursor_info;
  cursor_info.type = blink::WebCursorInfo::TypeCustom;
  cursor_info.image_scale_factor = device_scale_factor;
  cursor_info.custom_image = result;
  cursor_info.hotspot =
      gfx::Point(kHotspotOffset * device_scale_factor,
                 kHotspotOffset * device_scale_factor);

  cursor.InitFromCursorInfo(cursor_info);
  DCHECK(host_);
  host_->SetCursor(cursor);
}

}  // namespace page
}  // namespace devtools
}  // namespace content
