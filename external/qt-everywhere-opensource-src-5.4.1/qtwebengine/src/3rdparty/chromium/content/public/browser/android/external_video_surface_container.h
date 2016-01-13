// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_EXTERNAL_VIDEO_SURFACE_CONTAINER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_EXTERNAL_VIDEO_SURFACE_CONTAINER_H_

#include <jni.h>

#include "base/callback.h"
#include "content/common/content_export.h"

namespace gfx {
class RectF;
}

namespace content {
class WebContents;

// An interface used for managing the video surface for the hole punching.
class CONTENT_EXPORT ExternalVideoSurfaceContainer {
 public:
  typedef base::Callback<void(int, jobject)> SurfaceCreatedCB;
  typedef base::Callback<void(int)> SurfaceDestroyedCB;

  // Called when a media player wants to request an external video surface.
  // Whenever the surface is created and visible, |surface_created_cb| will be
  // called.  And whenever it is destroyed or invisible, |surface_destroyed_cb|
  // will be called.
  virtual void RequestExternalVideoSurface(
      int player_id,
      const SurfaceCreatedCB& surface_created_cb,
      const SurfaceDestroyedCB& surface_destroyed_cb) = 0;

  // Called when a media player wants to release an external video surface.
  virtual void ReleaseExternalVideoSurface(int player_id) = 0;

  // Called when the position and size of the video element which uses
  // the external video surface is changed.
  // |rect| contains the new position and size in css pixels.
  virtual void OnExternalVideoSurfacePositionChanged(
      int player_id, const gfx::RectF& rect) = 0;

  // Called when the page that contains the video element is scrolled or zoomed.
  virtual void OnFrameInfoUpdated() = 0;

  virtual ~ExternalVideoSurfaceContainer() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_EXTERNAL_VIDEO_SURFACE_CONTAINER_H_
