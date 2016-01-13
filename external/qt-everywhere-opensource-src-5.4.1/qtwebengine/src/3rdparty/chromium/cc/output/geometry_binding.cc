// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/geometry_binding.h"

#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/rect_f.h"

namespace cc {

GeometryBinding::GeometryBinding(gpu::gles2::GLES2Interface* gl,
                                 const gfx::RectF& quad_vertex_rect)
    : gl_(gl), quad_vertices_vbo_(0), quad_elements_vbo_(0) {
  struct Vertex {
    float a_position[3];
    float a_texCoord[2];
    // Index of the vertex, divide by 4 to have the matrix for this quad.
    float a_index;
  };
  struct Quad {
    Vertex v0, v1, v2, v3;
  };
  struct QuadIndex {
    uint16 data[6];
  };

  COMPILE_ASSERT(sizeof(Quad) == 24 * sizeof(float),  // NOLINT(runtime/sizeof)
                 struct_is_densely_packed);
  COMPILE_ASSERT(
      sizeof(QuadIndex) == 6 * sizeof(uint16_t),  // NOLINT(runtime/sizeof)
      struct_is_densely_packed);

  Quad quad_list[8];
  QuadIndex quad_index_list[8];
  for (int i = 0; i < 8; i++) {
    Vertex v0 = {{quad_vertex_rect.x(), quad_vertex_rect.bottom(), 0.0f, },
                 {0.0f, 1.0f, }, i * 4.0f + 0.0f};
    Vertex v1 = {{quad_vertex_rect.x(), quad_vertex_rect.y(), 0.0f, },
                 {0.0f, 0.0f, }, i * 4.0f + 1.0f};
    Vertex v2 = {{quad_vertex_rect.right(), quad_vertex_rect.y(), 0.0f, },
                 {1.0f, .0f, }, i * 4.0f + 2.0f};
    Vertex v3 = {{quad_vertex_rect.right(), quad_vertex_rect.bottom(), 0.0f, },
                 {1.0f, 1.0f, }, i * 4.0f + 3.0f};
    Quad x = {v0, v1, v2, v3};
    quad_list[i] = x;
    QuadIndex y = {
        {static_cast<uint16>(0 + 4 * i), static_cast<uint16>(1 + 4 * i),
         static_cast<uint16>(2 + 4 * i), static_cast<uint16>(3 + 4 * i),
         static_cast<uint16>(0 + 4 * i), static_cast<uint16>(2 + 4 * i)}};
    quad_index_list[i] = y;
  }

  gl_->GenBuffers(1, &quad_vertices_vbo_);
  gl_->GenBuffers(1, &quad_elements_vbo_);
  GLC(gl_, gl_->BindBuffer(GL_ARRAY_BUFFER, quad_vertices_vbo_));
  GLC(gl_,
      gl_->BufferData(
          GL_ARRAY_BUFFER, sizeof(quad_list), quad_list, GL_STATIC_DRAW));
  GLC(gl_, gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_elements_vbo_));
  GLC(gl_,
      gl_->BufferData(GL_ELEMENT_ARRAY_BUFFER,
                      sizeof(quad_index_list),
                      quad_index_list,
                      GL_STATIC_DRAW));
}

GeometryBinding::~GeometryBinding() {
  gl_->DeleteBuffers(1, &quad_vertices_vbo_);
  gl_->DeleteBuffers(1, &quad_elements_vbo_);
}

void GeometryBinding::PrepareForDraw() {
  GLC(gl_, gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_elements_vbo_));

  GLC(gl_, gl_->BindBuffer(GL_ARRAY_BUFFER, quad_vertices_vbo_));
  // OpenGL defines the last parameter to VertexAttribPointer as type
  // "const GLvoid*" even though it is actually an offset into the buffer
  // object's data store and not a pointer to the client's address space.
  const void* offsets[3] = {
      0, reinterpret_cast<const void*>(
             3 * sizeof(float)),  // NOLINT(runtime/sizeof)
      reinterpret_cast<const void*>(5 *
                                    sizeof(float)),  // NOLINT(runtime/sizeof)
  };

  GLC(gl_,
      gl_->VertexAttribPointer(PositionAttribLocation(),
                               3,
                               GL_FLOAT,
                               false,
                               6 * sizeof(float),  // NOLINT(runtime/sizeof)
                               offsets[0]));
  GLC(gl_,
      gl_->VertexAttribPointer(TexCoordAttribLocation(),
                               2,
                               GL_FLOAT,
                               false,
                               6 * sizeof(float),  // NOLINT(runtime/sizeof)
                               offsets[1]));
  GLC(gl_,
      gl_->VertexAttribPointer(TriangleIndexAttribLocation(),
                               1,
                               GL_FLOAT,
                               false,
                               6 * sizeof(float),  // NOLINT(runtime/sizeof)
                               offsets[2]));
  GLC(gl_, gl_->EnableVertexAttribArray(PositionAttribLocation()));
  GLC(gl_, gl_->EnableVertexAttribArray(TexCoordAttribLocation()));
  GLC(gl_, gl_->EnableVertexAttribArray(TriangleIndexAttribLocation()));
}

}  // namespace cc
