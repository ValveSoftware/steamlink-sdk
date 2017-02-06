// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/cursor_renderer_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

#include <cmath>

#include "base/logging.h"

namespace content {

namespace {

// RGBA format on cursor bitmap
const int kBytesPerPixel = 4;

inline int clip_byte(int x) {
  return std::max(0, std::min(x, 255));
}

inline int alpha_blend(int alpha, int src, int dst) {
  return (src * alpha + dst * (255 - alpha)) / 255;
}

}  // namespace

// static
std::unique_ptr<CursorRenderer> CursorRenderer::Create(gfx::NativeView view) {
  return std::unique_ptr<CursorRenderer>(new CursorRendererMac(view));
}

CursorRendererMac::CursorRendererMac(NSView* view)
    : view_(view), weak_factory_(this) {
  Clear();
}

CursorRendererMac::~CursorRendererMac() {}

base::WeakPtr<CursorRenderer> CursorRendererMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void CursorRendererMac::Clear() {
  last_cursor_data_.reset();
  last_cursor_width_ = 0;
  last_cursor_height_ = 0;
  last_mouse_location_x_ = 0;
  last_mouse_location_y_ = 0;
  last_mouse_movement_timestamp_ = base::TimeTicks();
}

// Polls mouse cursor location and image and returns whether the mouse
// cursor should be rendered on the frame.
bool CursorRendererMac::SnapshotCursorState(const gfx::Rect& region_in_frame) {
  // Mouse location in window co-ordinates.
  NSPoint mouse_window_location =
      [view_ window].mouseLocationOutsideOfEventStream;

  // Mouse co-ordinates directly comparable against frame co-ordinates
  // after translation.
  if (mouse_window_location.x < 0 || mouse_window_location.y < 0 ||
      mouse_window_location.x > region_in_frame.width() ||
      mouse_window_location.y > region_in_frame.height()) {
    VLOG(2) << "Mouse outside content region";
    return false;
  }

  if (![[view_ window] isKeyWindow]) {
    VLOG(2) << "Window currently inactive";
    return false;
  }

  if ((base::TimeTicks::Now() - last_mouse_movement_timestamp_).InSeconds() >
          MAX_IDLE_TIME_SECONDS &&
      std::abs(mouse_window_location.x - last_mouse_location_x_) <
          MIN_MOVEMENT_PIXELS &&
      std::abs(mouse_window_location.y - last_mouse_location_y_) <
          MIN_MOVEMENT_PIXELS) {
    VLOG(2) << "No mouse movement in a while";
    return false;
  }

  // Mouse cursor position within the frame.
  cursor_position_in_frame_ =
      gfx::Point(region_in_frame.x() + mouse_window_location.x,
                 region_in_frame.y() + mouse_window_location.y);

  // Grab system cursor.
  NSCursor* nscursor = [NSCursor currentSystemCursor];
  NSPoint nshotspot = [nscursor hotSpot];
  NSImage* nsimage = [nscursor image];
  NSSize nssize = [nsimage size];

  // The cursor co-ordinates in the window and the video frame co-ordinates are
  // inverted along y-axis. We render the cursor inverse vertically on the
  // frame. Hence the inversion on hotspot offset here.
  cursor_position_in_frame_.Offset(-nshotspot.x,
                                   -(nssize.height - nshotspot.y));
  last_cursor_width_ = nssize.width;
  last_cursor_height_ = nssize.height;

  CGImageRef cg_image =
      [nsimage CGImageForProposedRect:NULL context:nil hints:nil];
  if (!cg_image)
    return false;

  if (CGImageGetBitsPerPixel(cg_image) != kBytesPerPixel * 8 ||
      CGImageGetBytesPerRow(cg_image) !=
          static_cast<size_t>(kBytesPerPixel * nssize.width) ||
      CGImageGetBitsPerComponent(cg_image) != 8) {
    return false;
  }

  CGDataProviderRef provider = CGImageGetDataProvider(cg_image);
  CFDataRef image_data_ref = CGDataProviderCopyData(provider);
  if (!image_data_ref)
    return false;
  last_cursor_data_.reset(image_data_ref, base::scoped_policy::ASSUME);

  if (std::abs(mouse_window_location.x - last_mouse_location_x_) >
          MIN_MOVEMENT_PIXELS ||
      std::abs(mouse_window_location.y - last_mouse_location_y_) >
          MIN_MOVEMENT_PIXELS) {
    last_mouse_movement_timestamp_ = base::TimeTicks::Now();
    last_mouse_location_x_ = mouse_window_location.x;
    last_mouse_location_y_ = mouse_window_location.y;
  }
  return true;
}

// Helper function to composite a RGBA cursor bitmap on a YUV420 video frame.
void CursorRendererMac::RenderOnVideoFrame(
    const scoped_refptr<media::VideoFrame>& target) const {
  DCHECK(target);
  DCHECK(last_cursor_data_);
  const uint8_t* cursor_data_ =
      reinterpret_cast<const uint8_t*>(CFDataGetBytePtr(last_cursor_data_));

  gfx::Rect visible_rect = target->visible_rect();
  gfx::Rect rect =
      gfx::IntersectRects(gfx::Rect(last_cursor_width_, last_cursor_height_) +
                              gfx::Vector2d(cursor_position_in_frame_.x(),
                                            cursor_position_in_frame_.y()),
                          visible_rect);

  for (int y = rect.y() + 1; y <= rect.bottom(); ++y) {
    int cursor_y = rect.bottom() - y;
    int inverted_y = visible_rect.bottom() - y;
    uint8_t* yplane =
        target->data(media::VideoFrame::kYPlane) +
        inverted_y * target->row_bytes(media::VideoFrame::kYPlane);
    uint8_t* uplane =
        target->data(media::VideoFrame::kUPlane) +
        (inverted_y / 2) * target->row_bytes(media::VideoFrame::kUPlane);
    uint8_t* vplane =
        target->data(media::VideoFrame::kVPlane) +
        (inverted_y / 2) * target->row_bytes(media::VideoFrame::kVPlane);
    for (int x = rect.x(); x < rect.right(); ++x) {
      int cursor_x = x - rect.x();
      int byte_pos = cursor_y * last_cursor_width_ * kBytesPerPixel +
                     cursor_x * kBytesPerPixel;
      int color_r = cursor_data_[byte_pos];
      int color_g = cursor_data_[byte_pos + 1];
      int color_b = cursor_data_[byte_pos + 2];
      int alpha = cursor_data_[byte_pos + 3];
      int color_y = clip_byte(
          ((color_r * 66 + color_g * 129 + color_b * 25 + 128) >> 8) + 16);
      yplane[x] = alpha_blend(alpha, color_y, yplane[x]);

      // Only sample U and V at even coordinates.
      if ((x % 2 == 0) && (y % 2 == 0)) {
        int color_u = clip_byte(
            ((color_r * -38 + color_g * -74 + color_b * 112 + 128) >> 8) + 128);
        int color_v = clip_byte(
            ((color_r * 112 + color_g * -94 + color_b * -18 + 128) >> 8) + 128);
        uplane[x / 2] = alpha_blend(alpha, color_u, uplane[x / 2]);
        vplane[x / 2] = alpha_blend(alpha, color_v, vplane[x / 2]);
      }
    }
  }
}

}  // namespace content
