// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/cursor_renderer_aura.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "skia/ext/image_operations.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_utils.h"
#include "ui/wm/public/activation_client.h"

namespace content {

namespace {

inline int clip_byte(int x) {
  return std::max(0, std::min(x, 255));
}

inline int alpha_blend(int alpha, int src, int dst) {
  return (src * alpha + dst * (255 - alpha)) / 255;
}

}  // namespace

// static
std::unique_ptr<CursorRenderer> CursorRenderer::Create(
    gfx::NativeWindow window) {
  return std::unique_ptr<CursorRenderer>(
      new CursorRendererAura(window, kCursorEnabledOnMouseMovement));
}

CursorRendererAura::CursorRendererAura(
    aura::Window* window,
    CursorDisplaySetting cursor_display_setting)
    : window_(window),
      cursor_display_setting_(cursor_display_setting),
      tick_clock_(&default_tick_clock_),
      weak_factory_(this) {
  if (window_) {
    window_->AddObserver(this);
    if (cursor_display_setting == kCursorEnabledOnMouseMovement)
      window_->AddPreTargetHandler(this);
  }
  Clear();
}

CursorRendererAura::~CursorRendererAura() {
  if (window_) {
    window_->RemoveObserver(this);
    if (cursor_display_setting_ == kCursorEnabledOnMouseMovement)
      window_->RemovePreTargetHandler(this);
  }
}

base::WeakPtr<CursorRenderer> CursorRendererAura::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void CursorRendererAura::Clear() {
  last_cursor_ = ui::Cursor();
  window_size_when_cursor_last_updated_ = gfx::Size();
  scaled_cursor_bitmap_.reset();
  last_mouse_position_x_ = 0;
  last_mouse_position_y_ = 0;
  if (cursor_display_setting_ == kCursorEnabledOnMouseMovement) {
    cursor_displayed_ = false;
  } else {
    cursor_displayed_ = true;
  }
}

bool CursorRendererAura::SnapshotCursorState(const gfx::Rect& region_in_frame) {
  if (!window_) {
    DVLOG(2) << "Skipping update with no window being tracked";
    return false;
  }
  const gfx::Rect window_bounds = window_->GetBoundsInScreen();
  gfx::Point cursor_position = aura::Env::GetInstance()->last_mouse_location();
  if (!window_bounds.Contains(cursor_position)) {
    // Return early if there is no need to draw the cursor.
    DVLOG(2) << "Skipping update with cursor outside the window";
    Clear();
    return false;
  }

  // If we are sharing the root window, or namely the whole screen, we will
  // render the mouse cursor. For ordinary window, we only render the mouse
  // cursor when the window is active.
  if (!window_->IsRootWindow()) {
    aura::client::ActivationClient* activation_client =
        aura::client::GetActivationClient(window_->GetRootWindow());
    if (!activation_client) {
      DVLOG(2) << "Assume window inactive with invalid activation_client";
      Clear();
      return false;
    }
    aura::Window* active_window = activation_client->GetActiveWindow();
    if (!active_window->Contains(window_)) {
      // Return early if the target window is not active.
      DVLOG(2) << "Skipping update on an inactive window";
      Clear();
      return false;
    }
  }

  if (cursor_display_setting_ == kCursorEnabledOnMouseMovement) {
    if (cursor_displayed_) {
      // Stop displaying cursor if there has been no mouse movement
      base::TimeTicks now = tick_clock_->NowTicks();
      if ((now - last_mouse_movement_timestamp_) >
          base::TimeDelta::FromSeconds(MAX_IDLE_TIME_SECONDS)) {
        cursor_displayed_ = false;
        DVLOG(2) << "Turning off cursor display after idle time";
      }
    }
    if (!cursor_displayed_)
      return false;
  }

  gfx::NativeCursor cursor = window_->GetHost()->last_cursor();
  gfx::Point cursor_hot_point;
  if (last_cursor_ != cursor ||
      window_size_when_cursor_last_updated_ != window_bounds.size()) {
    SkBitmap cursor_bitmap;
    if (ui::GetCursorBitmap(cursor, &cursor_bitmap, &cursor_hot_point)) {
      const int scaled_width = cursor_bitmap.width() * region_in_frame.width() /
                               window_bounds.width();
      const int scaled_height = cursor_bitmap.height() *
                                region_in_frame.height() /
                                window_bounds.height();
      if (scaled_width <= 0 || scaled_height <= 0) {
        DVLOG(2) << "scaled_width <= 0";
        Clear();
        return false;
      }
      scaled_cursor_bitmap_ = skia::ImageOperations::Resize(
          cursor_bitmap, skia::ImageOperations::RESIZE_BEST, scaled_width,
          scaled_height);
      last_cursor_ = cursor;
      window_size_when_cursor_last_updated_ = window_bounds.size();
    } else {
      // Clear cursor state if ui::GetCursorBitmap failed so that we do not
      // render cursor on the captured frame.
      Clear();
    }
  }

  cursor_position.Offset(-window_bounds.x() - cursor_hot_point.x(),
                         -window_bounds.y() - cursor_hot_point.y());
  cursor_position_in_frame_ = gfx::Point(
      region_in_frame.x() +
          cursor_position.x() * region_in_frame.width() / window_bounds.width(),
      region_in_frame.y() +
          cursor_position.y() * region_in_frame.height() /
              window_bounds.height());

  return true;
}

// Helper function to composite a cursor bitmap on a YUV420 video frame.
void CursorRendererAura::RenderOnVideoFrame(
    const scoped_refptr<media::VideoFrame>& target) const {
  if (scaled_cursor_bitmap_.isNull())
    return;

  DCHECK(target);

  gfx::Rect rect = gfx::IntersectRects(
      gfx::Rect(scaled_cursor_bitmap_.width(), scaled_cursor_bitmap_.height()) +
          gfx::Vector2d(cursor_position_in_frame_.x(),
                        cursor_position_in_frame_.y()),
      target->visible_rect());

  scaled_cursor_bitmap_.lockPixels();
  for (int y = rect.y(); y < rect.bottom(); ++y) {
    int cursor_y = y - cursor_position_in_frame_.y();
    uint8_t* yplane = target->data(media::VideoFrame::kYPlane) +
                      y * target->row_bytes(media::VideoFrame::kYPlane);
    uint8_t* uplane = target->data(media::VideoFrame::kUPlane) +
                      (y / 2) * target->row_bytes(media::VideoFrame::kUPlane);
    uint8_t* vplane = target->data(media::VideoFrame::kVPlane) +
                      (y / 2) * target->row_bytes(media::VideoFrame::kVPlane);
    for (int x = rect.x(); x < rect.right(); ++x) {
      int cursor_x = x - cursor_position_in_frame_.x();
      SkColor color = scaled_cursor_bitmap_.getColor(cursor_x, cursor_y);
      int alpha = SkColorGetA(color);
      int color_r = SkColorGetR(color);
      int color_g = SkColorGetG(color);
      int color_b = SkColorGetB(color);
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
  scaled_cursor_bitmap_.unlockPixels();
}

void CursorRendererAura::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      if (!cursor_displayed_) {
        if (std::abs(event->x() - last_mouse_position_x_) >
                MIN_MOVEMENT_PIXELS ||
            std::abs(event->y() - last_mouse_position_y_) > MIN_MOVEMENT_PIXELS)
          cursor_displayed_ = true;
      }
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSEWHEEL:
      cursor_displayed_ = true;
      break;
    default:
      return;
  }
  if (cursor_displayed_) {
    last_mouse_movement_timestamp_ = event->time_stamp();
    last_mouse_position_x_ = event->x();
    last_mouse_position_y_ = event->y();
  }
}

void CursorRendererAura::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  if (cursor_display_setting_ == kCursorEnabledOnMouseMovement)
    window_->RemovePreTargetHandler(this);
  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace content
