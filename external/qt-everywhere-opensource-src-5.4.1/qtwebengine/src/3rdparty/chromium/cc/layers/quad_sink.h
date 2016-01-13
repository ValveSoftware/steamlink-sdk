// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_QUAD_SINK_H_
#define CC_LAYERS_QUAD_SINK_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"

namespace gfx {
class Rect;
class Transform;
}

namespace cc {

class DrawQuad;
class LayerImpl;
class RenderPass;
class RenderSurfaceImpl;
class SharedQuadState;
template <typename LayerType>
class OcclusionTracker;

class CC_EXPORT QuadSink {
 public:
  QuadSink(RenderPass* render_pass,
           const OcclusionTracker<LayerImpl>* occlusion_tracker);
  ~QuadSink() {}

  // Call this to add a SharedQuadState before appending quads that refer to it.
  // Returns a pointer to the given SharedQuadState, that can be set on the
  // quads to append.
  virtual SharedQuadState* CreateSharedQuadState();

  virtual gfx::Rect UnoccludedContentRect(const gfx::Rect& content_rect,
                                          const gfx::Transform& draw_transform);

  virtual gfx::Rect UnoccludedContributingSurfaceContentRect(
      const gfx::Rect& content_rect,
      const gfx::Transform& draw_transform);

  virtual void Append(scoped_ptr<DrawQuad> draw_quad);

  const RenderPass* render_pass() const { return render_pass_; }

 protected:
  RenderPass* render_pass_;

  SharedQuadState* current_shared_quad_state_;

 private:
  const OcclusionTracker<LayerImpl>* occlusion_tracker_;

  DISALLOW_COPY_AND_ASSIGN(QuadSink);
};

}  // namespace cc

#endif  // CC_LAYERS_QUAD_SINK_H_
