// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_AURA_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_AURA_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "content/browser/media/capture/cursor_renderer.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// Setting to control cursor display based on either mouse movement or always
// forced to be enabled.
enum CursorDisplaySetting {
  kCursorAlwaysEnabled,
  kCursorEnabledOnMouseMovement
};

// Tracks state for making decisions on cursor display on a captured video
// frame.
class CONTENT_EXPORT CursorRendererAura : public CursorRenderer,
                                          public ui::EventHandler,
                                          public aura::WindowObserver {
 public:
  explicit CursorRendererAura(aura::Window* window,
                              CursorDisplaySetting cursor_display);
  ~CursorRendererAura() final;

  // CursorRender implementation.
  void Clear() final;
  bool SnapshotCursorState(const gfx::Rect& region_in_frame) final;
  void RenderOnVideoFrame(
      const scoped_refptr<media::VideoFrame>& target) const final;
  base::WeakPtr<CursorRenderer> GetWeakPtr() final;

  // ui::EventHandler overrides.
  void OnMouseEvent(ui::MouseEvent* event) final;

  // aura::WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) final;

 private:
  friend class CursorRendererAuraTest;

  aura::Window* window_;

  // Snapshot of cursor, source size, position, and cursor bitmap; as of the
  // last call to SnapshotCursorState.
  ui::Cursor last_cursor_;
  gfx::Size window_size_when_cursor_last_updated_;
  gfx::Point cursor_position_in_frame_;
  SkBitmap scaled_cursor_bitmap_;

  // Updated in mouse event listener and used to make a decision on
  // when the cursor is rendered.
  base::TimeTicks last_mouse_movement_timestamp_;
  float last_mouse_position_x_;
  float last_mouse_position_y_;
  bool cursor_displayed_;

  // Controls whether cursor is displayed based on active mouse movement.
  const CursorDisplaySetting cursor_display_setting_;

  // Allows tests to replace the clock.
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* tick_clock_;

  base::WeakPtrFactory<CursorRendererAura> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CursorRendererAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_AURA_H_
