// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_MAC_H_

#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/capture/cursor_renderer.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"

namespace content {

// Tracks state for making decisions on cursor display on a captured video
// frame.
class CONTENT_EXPORT CursorRendererMac : public CursorRenderer {
 public:
  explicit CursorRendererMac(NSView* view);
  ~CursorRendererMac() final;

  // CursorRender implementation.
  void Clear() final;
  bool SnapshotCursorState(const gfx::Rect& region_in_frame) final;
  void RenderOnVideoFrame(
      const scoped_refptr<media::VideoFrame>& target) const final;
  base::WeakPtr<CursorRenderer> GetWeakPtr() final;

 private:
  NSView* const view_;
  base::ScopedCFTypeRef<CFDataRef> last_cursor_data_;
  int last_cursor_width_;
  int last_cursor_height_;
  gfx::Point cursor_position_in_frame_;

  base::TimeTicks last_mouse_movement_timestamp_;
  float last_mouse_location_x_;
  float last_mouse_location_y_;

  base::WeakPtrFactory<CursorRendererMac> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(CursorRendererMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_MAC_H_
