// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_STREAM_VIDEO_DRAW_QUAD_H_
#define CC_QUADS_STREAM_VIDEO_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/quads/draw_quad.h"
#include "ui/gfx/transform.h"

namespace cc {

class CC_EXPORT StreamVideoDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<StreamVideoDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              unsigned resource_id,
              const gfx::Transform& matrix);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              unsigned resource_id,
              const gfx::Transform& matrix);

  unsigned resource_id;
  gfx::Transform matrix;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const StreamVideoDrawQuad* MaterialCast(const DrawQuad*);

 private:
  StreamVideoDrawQuad();
  virtual void ExtendValue(base::DictionaryValue* value) const OVERRIDE;
};

}  // namespace cc

#endif  // CC_QUADS_STREAM_VIDEO_DRAW_QUAD_H_
