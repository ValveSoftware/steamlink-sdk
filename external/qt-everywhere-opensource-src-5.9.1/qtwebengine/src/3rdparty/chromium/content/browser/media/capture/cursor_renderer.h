// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// CursorRenderer is an interface that can be implememented to do cursor
// rendering on a video frame.
//
// In order to track the cursor, the platform-specific implementation
// will listen to mouse events.
class CONTENT_EXPORT CursorRenderer {
 public:
  static std::unique_ptr<CursorRenderer> Create(gfx::NativeView view);

  virtual ~CursorRenderer() {}

  // Clears the cursor state being tracked. Called when there is a need to
  // reset the state.
  virtual void Clear() = 0;

  // Takes a snapshot of the current cursor state and determines whether
  // the cursor will be rendered, which cursor image to render, and at what
  // location within |region_in_frame| to render it. Returns true if cursor
  // needs to be rendered.
  virtual bool SnapshotCursorState(const gfx::Rect& region_in_frame) = 0;

  // Renders cursor on the |target| video frame.
  virtual void RenderOnVideoFrame(
      const scoped_refptr<media::VideoFrame>& target) const = 0;

  // Returns a weak pointer.
  virtual base::WeakPtr<CursorRenderer> GetWeakPtr() = 0;

 protected:
  enum {
    // Minium movement before cursor is rendered on frame.
    MIN_MOVEMENT_PIXELS = 15,
    // Maximum idle time allowed before we stop rendering the cursor
    // on frame.
    MAX_IDLE_TIME_SECONDS = 2
  };
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_H_
