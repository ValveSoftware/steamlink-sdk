// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_GEOMETRY_BINDING_H_
#define CC_OUTPUT_GEOMETRY_BINDING_H_

#include "base/basictypes.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace gfx {
class RectF;
}
namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {

class GeometryBinding {
 public:
  GeometryBinding(gpu::gles2::GLES2Interface* gl,
                  const gfx::RectF& quad_vertex_rect);
  ~GeometryBinding();

  void PrepareForDraw();

  // All layer shaders share the same attribute locations for the vertex
  // positions and texture coordinates. This allows switching shaders without
  // rebinding attribute arrays.
  static int PositionAttribLocation() { return 0; }
  static int TexCoordAttribLocation() { return 1; }
  static int TriangleIndexAttribLocation() { return 2; }

 private:
  gpu::gles2::GLES2Interface* gl_;

  GLuint quad_vertices_vbo_;
  GLuint quad_elements_vbo_;

  DISALLOW_COPY_AND_ASSIGN(GeometryBinding);
};

}  // namespace cc

#endif  // CC_OUTPUT_GEOMETRY_BINDING_H_
