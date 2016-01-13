// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/shader.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"

#define SHADER0(Src) #Src
#define VERTEX_SHADER(Src) SetVertexTexCoordPrecision(SHADER0(Src))
#define FRAGMENT_SHADER(Src) SetFragmentTexCoordPrecision( \
    precision, SetFragmentSamplerType(sampler, SHADER0(Src)))

using gpu::gles2::GLES2Interface;

namespace cc {

namespace {

static void GetProgramUniformLocations(GLES2Interface* context,
                                       unsigned program,
                                       size_t count,
                                       const char** uniforms,
                                       int* locations,
                                       int* base_uniform_index) {
  for (size_t i = 0; i < count; i++) {
    locations[i] = (*base_uniform_index)++;
    context->BindUniformLocationCHROMIUM(program, locations[i], uniforms[i]);
  }
}

static std::string SetFragmentTexCoordPrecision(
    TexCoordPrecision requested_precision, std::string shader_string) {
  switch (requested_precision) {
    case TexCoordPrecisionHigh:
      DCHECK_NE(shader_string.find("TexCoordPrecision"), std::string::npos);
      return
          "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
          "  #define TexCoordPrecision highp\n"
          "#else\n"
          "  #define TexCoordPrecision mediump\n"
          "#endif\n" +
          shader_string;
    case TexCoordPrecisionMedium:
      DCHECK_NE(shader_string.find("TexCoordPrecision"), std::string::npos);
      return "#define TexCoordPrecision mediump\n" +
          shader_string;
    case TexCoordPrecisionNA:
      DCHECK_EQ(shader_string.find("TexCoordPrecision"), std::string::npos);
      DCHECK_EQ(shader_string.find("texture2D"), std::string::npos);
      DCHECK_EQ(shader_string.find("texture2DRect"), std::string::npos);
      return shader_string;
    default:
      NOTREACHED();
      break;
  }
  return shader_string;
}

static std::string SetVertexTexCoordPrecision(const char* shader_string) {
  // We unconditionally use highp in the vertex shader since
  // we are unlikely to be vertex shader bound when drawing large quads.
  // Also, some vertex shaders mutate the texture coordinate in such a
  // way that the effective precision might be lower than expected.
  return "#define TexCoordPrecision highp\n" +
      std::string(shader_string);
}

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int *highp_threshold_cache,
                                            int highp_threshold_min,
                                            int x, int y) {
  if (*highp_threshold_cache == 0) {
    // Initialize range and precision with minimum spec values for when
    // GetShaderPrecisionFormat is a test stub.
    // TODO(brianderson): Implement better stubs of GetShaderPrecisionFormat
    // everywhere.
    GLint range[2] = { 14, 14 };
    GLint precision = 10;
    GLC(context, context->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER,
                                                   GL_MEDIUM_FLOAT,
                                                   range, &precision));
    *highp_threshold_cache = 1 << precision;
  }

  int highp_threshold = std::max(*highp_threshold_cache, highp_threshold_min);
  if (x > highp_threshold || y > highp_threshold)
    return TexCoordPrecisionHigh;
  return TexCoordPrecisionMedium;
}

static std::string SetFragmentSamplerType(
    SamplerType requested_type, std::string shader_string) {
  switch (requested_type) {
    case SamplerType2D:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return
          "#define SamplerType sampler2D\n"
          "#define TextureLookup texture2D\n" +
          shader_string;
    case SamplerType2DRect:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return
          "#extension GL_ARB_texture_rectangle : require\n"
          "#define SamplerType sampler2DRect\n"
          "#define TextureLookup texture2DRect\n" +
          shader_string;
    case SamplerTypeExternalOES:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return
          "#extension GL_OES_EGL_image_external : require\n"
          "#define SamplerType samplerExternalOES\n"
          "#define TextureLookup texture2D\n" +
          shader_string;
    case SamplerTypeNA:
      DCHECK_EQ(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_EQ(shader_string.find("TextureLookup"), std::string::npos);
      return shader_string;
    default:
      NOTREACHED();
      break;
  }
  return shader_string;
}

}  // namespace

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int* highp_threshold_cache,
                                            int highp_threshold_min,
                                            const gfx::Point& max_coordinate) {
  return TexCoordPrecisionRequired(context,
                                   highp_threshold_cache, highp_threshold_min,
                                   max_coordinate.x(), max_coordinate.y());
}

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int *highp_threshold_cache,
                                            int highp_threshold_min,
                                            const gfx::Size& max_size) {
  return TexCoordPrecisionRequired(context,
                                   highp_threshold_cache, highp_threshold_min,
                                   max_size.width(), max_size.height());
}

VertexShaderPosTex::VertexShaderPosTex()
      : matrix_location_(-1) {}

void VertexShaderPosTex::Init(GLES2Interface* context,
                              unsigned program,
                              int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
}

std::string VertexShaderPosTex::GetShaderString() const {
  return VERTEX_SHADER(
    attribute vec4 a_position;
    attribute TexCoordPrecision vec2 a_texCoord;
    uniform mat4 matrix;
    varying TexCoordPrecision vec2 v_texCoord;
    void main() {
      gl_Position = matrix * a_position;
      v_texCoord = a_texCoord;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderPosTexYUVStretchOffset::VertexShaderPosTexYUVStretchOffset()
    : matrix_location_(-1), tex_scale_location_(-1), tex_offset_location_(-1) {}

void VertexShaderPosTexYUVStretchOffset::Init(GLES2Interface* context,
                                              unsigned program,
                                              int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "texScale",
    "texOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  tex_scale_location_ = locations[1];
  tex_offset_location_ = locations[2];
}

std::string VertexShaderPosTexYUVStretchOffset::GetShaderString() const {
  return VERTEX_SHADER(
    precision mediump float;
    attribute vec4 a_position;
    attribute TexCoordPrecision vec2 a_texCoord;
    uniform mat4 matrix;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform TexCoordPrecision vec2 texScale;
    uniform TexCoordPrecision vec2 texOffset;
    void main() {
        gl_Position = matrix * a_position;
        v_texCoord = a_texCoord * texScale + texOffset;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderPos::VertexShaderPos()
    : matrix_location_(-1) {}

void VertexShaderPos::Init(GLES2Interface* context,
                           unsigned program,
                           int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
}

std::string VertexShaderPos::GetShaderString() const {
  return VERTEX_SHADER(
    attribute vec4 a_position;
    uniform mat4 matrix;
    void main() {
        gl_Position = matrix * a_position;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderPosTexTransform::VertexShaderPosTexTransform()
    : matrix_location_(-1),
      tex_transform_location_(-1),
      vertex_opacity_location_(-1) {}

void VertexShaderPosTexTransform::Init(GLES2Interface* context,
                                       unsigned program,
                                       int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "texTransform",
    "opacity",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  tex_transform_location_ = locations[1];
  vertex_opacity_location_ = locations[2];
}

std::string VertexShaderPosTexTransform::GetShaderString() const {
  return VERTEX_SHADER(
    attribute vec4 a_position;
    attribute TexCoordPrecision vec2 a_texCoord;
    attribute float a_index;
    uniform mat4 matrix[8];
    uniform TexCoordPrecision vec4 texTransform[8];
    uniform float opacity[32];
    varying TexCoordPrecision vec2 v_texCoord;
    varying float v_alpha;
    void main() {
      int quad_index = int(a_index * 0.25);  // NOLINT
      gl_Position = matrix[quad_index] * a_position;
      TexCoordPrecision vec4 texTrans = texTransform[quad_index];
      v_texCoord = a_texCoord * texTrans.zw + texTrans.xy;
      v_alpha = opacity[int(a_index)]; // NOLINT
    }
  );  // NOLINT(whitespace/parens)
}

std::string VertexShaderPosTexIdentity::GetShaderString() const {
  return VERTEX_SHADER(
    attribute vec4 a_position;
    varying TexCoordPrecision vec2 v_texCoord;
    void main() {
      gl_Position = a_position;
      v_texCoord = (a_position.xy + vec2(1.0)) * 0.5;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderQuad::VertexShaderQuad()
    : matrix_location_(-1),
      quad_location_(-1) {}

void VertexShaderQuad::Init(GLES2Interface* context,
                            unsigned program,
                            int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "quad",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  quad_location_ = locations[1];
}

std::string VertexShaderQuad::GetShaderString() const {
#if defined(OS_ANDROID)
// TODO(epenner): Find the cause of this 'quad' uniform
// being missing if we don't add dummy variables.
// http://crbug.com/240602
  return VERTEX_SHADER(
    attribute TexCoordPrecision vec4 a_position;
    attribute float a_index;
    uniform mat4 matrix;
    uniform TexCoordPrecision vec2 quad[4];
    uniform TexCoordPrecision vec2 dummy_uniform;
    varying TexCoordPrecision vec2 dummy_varying;
    void main() {
      vec2 pos = quad[int(a_index)];  // NOLINT
      gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
      dummy_varying = dummy_uniform;
    }
  );  // NOLINT(whitespace/parens)
#else
  return VERTEX_SHADER(
    attribute TexCoordPrecision vec4 a_position;
    attribute float a_index;
    uniform mat4 matrix;
    uniform TexCoordPrecision vec2 quad[4];
    void main() {
      vec2 pos = quad[int(a_index)];  // NOLINT
      gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
    }
  );  // NOLINT(whitespace/parens)
#endif
}

VertexShaderQuadAA::VertexShaderQuadAA()
    : matrix_location_(-1),
      viewport_location_(-1),
      quad_location_(-1),
      edge_location_(-1) {}

void VertexShaderQuadAA::Init(GLES2Interface* context,
                            unsigned program,
                            int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "viewport",
    "quad",
    "edge",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  viewport_location_ = locations[1];
  quad_location_ = locations[2];
  edge_location_ = locations[3];
}

std::string VertexShaderQuadAA::GetShaderString() const {
  return VERTEX_SHADER(
    attribute TexCoordPrecision vec4 a_position;
    attribute float a_index;
    uniform mat4 matrix;
    uniform vec4 viewport;
    uniform TexCoordPrecision vec2 quad[4];
    uniform TexCoordPrecision vec3 edge[8];
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec2 pos = quad[int(a_index)];  // NOLINT
      gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
      vec2 ndc_pos = 0.5 * (1.0 + gl_Position.xy / gl_Position.w);
      vec3 screen_pos = vec3(viewport.xy + viewport.zw * ndc_pos, 1.0);
      edge_dist[0] = vec4(dot(edge[0], screen_pos),
                          dot(edge[1], screen_pos),
                          dot(edge[2], screen_pos),
                          dot(edge[3], screen_pos)) * gl_Position.w;
      edge_dist[1] = vec4(dot(edge[4], screen_pos),
                          dot(edge[5], screen_pos),
                          dot(edge[6], screen_pos),
                          dot(edge[7], screen_pos)) * gl_Position.w;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderQuadTexTransformAA::VertexShaderQuadTexTransformAA()
    : matrix_location_(-1),
      viewport_location_(-1),
      quad_location_(-1),
      edge_location_(-1),
      tex_transform_location_(-1) {}

void VertexShaderQuadTexTransformAA::Init(GLES2Interface* context,
                                        unsigned program,
                                        int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "viewport",
    "quad",
    "edge",
    "texTrans",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  viewport_location_ = locations[1];
  quad_location_ = locations[2];
  edge_location_ = locations[3];
  tex_transform_location_ = locations[4];
}

std::string VertexShaderQuadTexTransformAA::GetShaderString() const {
  return VERTEX_SHADER(
    attribute TexCoordPrecision vec4 a_position;
    attribute float a_index;
    uniform mat4 matrix;
    uniform vec4 viewport;
    uniform TexCoordPrecision vec2 quad[4];
    uniform TexCoordPrecision vec3 edge[8];
    uniform TexCoordPrecision vec4 texTrans;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec2 pos = quad[int(a_index)];  // NOLINT
      gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
      vec2 ndc_pos = 0.5 * (1.0 + gl_Position.xy / gl_Position.w);
      vec3 screen_pos = vec3(viewport.xy + viewport.zw * ndc_pos, 1.0);
      edge_dist[0] = vec4(dot(edge[0], screen_pos),
                          dot(edge[1], screen_pos),
                          dot(edge[2], screen_pos),
                          dot(edge[3], screen_pos)) * gl_Position.w;
      edge_dist[1] = vec4(dot(edge[4], screen_pos),
                          dot(edge[5], screen_pos),
                          dot(edge[6], screen_pos),
                          dot(edge[7], screen_pos)) * gl_Position.w;
      v_texCoord = (pos.xy + vec2(0.5)) * texTrans.zw + texTrans.xy;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderTile::VertexShaderTile()
    : matrix_location_(-1),
      quad_location_(-1),
      vertex_tex_transform_location_(-1) {}

void VertexShaderTile::Init(GLES2Interface* context,
                            unsigned program,
                            int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "quad",
    "vertexTexTransform",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  quad_location_ = locations[1];
  vertex_tex_transform_location_ = locations[2];
}

std::string VertexShaderTile::GetShaderString() const {
  return VERTEX_SHADER(
    attribute TexCoordPrecision vec4 a_position;
    attribute float a_index;
    uniform mat4 matrix;
    uniform TexCoordPrecision vec2 quad[4];
    uniform TexCoordPrecision vec4 vertexTexTransform;
    varying TexCoordPrecision vec2 v_texCoord;
    void main() {
      vec2 pos = quad[int(a_index)];  // NOLINT
      gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
      v_texCoord = pos.xy * vertexTexTransform.zw + vertexTexTransform.xy;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderTileAA::VertexShaderTileAA()
    : matrix_location_(-1),
      viewport_location_(-1),
      quad_location_(-1),
      edge_location_(-1),
      vertex_tex_transform_location_(-1) {}

void VertexShaderTileAA::Init(GLES2Interface* context,
                              unsigned program,
                              int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "viewport",
    "quad",
    "edge",
    "vertexTexTransform",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  viewport_location_ = locations[1];
  quad_location_ = locations[2];
  edge_location_ = locations[3];
  vertex_tex_transform_location_ = locations[4];
}

std::string VertexShaderTileAA::GetShaderString() const {
  return VERTEX_SHADER(
    attribute TexCoordPrecision vec4 a_position;
    attribute float a_index;
    uniform mat4 matrix;
    uniform vec4 viewport;
    uniform TexCoordPrecision vec2 quad[4];
    uniform TexCoordPrecision vec3 edge[8];
    uniform TexCoordPrecision vec4 vertexTexTransform;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec2 pos = quad[int(a_index)];  // NOLINT
      gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
      vec2 ndc_pos = 0.5 * (1.0 + gl_Position.xy / gl_Position.w);
      vec3 screen_pos = vec3(viewport.xy + viewport.zw * ndc_pos, 1.0);
      edge_dist[0] = vec4(dot(edge[0], screen_pos),
                          dot(edge[1], screen_pos),
                          dot(edge[2], screen_pos),
                          dot(edge[3], screen_pos)) * gl_Position.w;
      edge_dist[1] = vec4(dot(edge[4], screen_pos),
                          dot(edge[5], screen_pos),
                          dot(edge[6], screen_pos),
                          dot(edge[7], screen_pos)) * gl_Position.w;
      v_texCoord = pos.xy * vertexTexTransform.zw + vertexTexTransform.xy;
    }
  );  // NOLINT(whitespace/parens)
}

VertexShaderVideoTransform::VertexShaderVideoTransform()
    : matrix_location_(-1),
      tex_matrix_location_(-1) {}

void VertexShaderVideoTransform::Init(GLES2Interface* context,
                                      unsigned program,
                                      int* base_uniform_index) {
  static const char* uniforms[] = {
    "matrix",
    "texMatrix",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  tex_matrix_location_ = locations[1];
}

std::string VertexShaderVideoTransform::GetShaderString() const {
  return VERTEX_SHADER(
    attribute vec4 a_position;
    attribute TexCoordPrecision vec2 a_texCoord;
    uniform mat4 matrix;
    uniform TexCoordPrecision mat4 texMatrix;
    varying TexCoordPrecision vec2 v_texCoord;
    void main() {
        gl_Position = matrix * a_position;
        v_texCoord =
            vec2(texMatrix * vec4(a_texCoord.x, 1.0 - a_texCoord.y, 0.0, 1.0));
    }
  );  // NOLINT(whitespace/parens)
}

FragmentTexAlphaBinding::FragmentTexAlphaBinding()
    : sampler_location_(-1),
      alpha_location_(-1) {}

void FragmentTexAlphaBinding::Init(GLES2Interface* context,
                                   unsigned program,
                                   int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "alpha",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
}

FragmentTexColorMatrixAlphaBinding::FragmentTexColorMatrixAlphaBinding()
    : sampler_location_(-1),
      alpha_location_(-1),
      color_matrix_location_(-1),
      color_offset_location_(-1) {}

void FragmentTexColorMatrixAlphaBinding::Init(GLES2Interface* context,
                                              unsigned program,
                                              int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "alpha",
    "colorMatrix",
    "colorOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  color_matrix_location_ = locations[2];
  color_offset_location_ = locations[3];
}

FragmentTexOpaqueBinding::FragmentTexOpaqueBinding()
    : sampler_location_(-1) {}

void FragmentTexOpaqueBinding::Init(GLES2Interface* context,
                                    unsigned program,
                                    int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
}

std::string FragmentShaderRGBATexAlpha::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    uniform float alpha;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      gl_FragColor = texColor * alpha;
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATexColorMatrixAlpha::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    uniform float alpha;
    uniform mat4 colorMatrix;
    uniform vec4 colorOffset;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      float nonZeroAlpha = max(texColor.a, 0.00001);
      texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
      texColor = colorMatrix * texColor + colorOffset;
      texColor.rgb *= texColor.a;
      texColor = clamp(texColor, 0.0, 1.0);
      gl_FragColor = texColor * alpha;
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATexVaryingAlpha::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    varying float v_alpha;
    uniform SamplerType s_texture;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      gl_FragColor = texColor * v_alpha;
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATexPremultiplyAlpha::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    varying float v_alpha;
    uniform SamplerType s_texture;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      texColor.rgb *= texColor.a;
      gl_FragColor = texColor * v_alpha;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentTexBackgroundBinding::FragmentTexBackgroundBinding()
    : background_color_location_(-1),
      sampler_location_(-1) {
}

void FragmentTexBackgroundBinding::Init(GLES2Interface* context,
                                        unsigned program,
                                        int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "background_color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);

  sampler_location_ = locations[0];
  DCHECK_NE(sampler_location_, -1);

  background_color_location_ = locations[1];
  DCHECK_NE(background_color_location_, -1);
}

std::string FragmentShaderTexBackgroundVaryingAlpha::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    varying float v_alpha;
    uniform vec4 background_color;
    uniform SamplerType s_texture;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      texColor += background_color * (1.0 - texColor.a);
      gl_FragColor = texColor * v_alpha;
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderTexBackgroundPremultiplyAlpha::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    varying float v_alpha;
    uniform vec4 background_color;
    uniform SamplerType s_texture;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      texColor.rgb *= texColor.a;
      texColor += background_color * (1.0 - texColor.a);
      gl_FragColor = texColor * v_alpha;
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATexOpaque::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      gl_FragColor = vec4(texColor.rgb, 1.0);
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATex::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    void main() {
      gl_FragColor = TextureLookup(s_texture, v_texCoord);
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATexSwizzleAlpha::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    uniform float alpha;
    void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        gl_FragColor =
            vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha;
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATexSwizzleOpaque::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, 1.0);
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderRGBATexAlphaAA::FragmentShaderRGBATexAlphaAA()
    : sampler_location_(-1),
      alpha_location_(-1) {}

void FragmentShaderRGBATexAlphaAA::Init(GLES2Interface* context,
                                        unsigned program,
                                        int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "alpha",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
}

std::string FragmentShaderRGBATexAlphaAA::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform SamplerType s_texture;
    uniform float alpha;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      vec4 d4 = min(edge_dist[0], edge_dist[1]);
      vec2 d2 = min(d4.xz, d4.yw);
      float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
      gl_FragColor = texColor * alpha * aa;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentTexClampAlphaAABinding::FragmentTexClampAlphaAABinding()
    : sampler_location_(-1),
      alpha_location_(-1),
      fragment_tex_transform_location_(-1) {}

void FragmentTexClampAlphaAABinding::Init(GLES2Interface* context,
                                          unsigned program,
                                          int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "alpha",
    "fragmentTexTransform",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  fragment_tex_transform_location_ = locations[2];
}

std::string FragmentShaderRGBATexClampAlphaAA::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform SamplerType s_texture;
    uniform float alpha;
    uniform TexCoordPrecision vec4 fragmentTexTransform;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      TexCoordPrecision vec2 texCoord =
          clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw +
          fragmentTexTransform.xy;
      vec4 texColor = TextureLookup(s_texture, texCoord);
      vec4 d4 = min(edge_dist[0], edge_dist[1]);
      vec2 d2 = min(d4.xz, d4.yw);
      float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
      gl_FragColor = texColor * alpha * aa;
    }
  );  // NOLINT(whitespace/parens)
}

std::string FragmentShaderRGBATexClampSwizzleAlphaAA::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform SamplerType s_texture;
    uniform float alpha;
    uniform TexCoordPrecision vec4 fragmentTexTransform;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      TexCoordPrecision vec2 texCoord =
          clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw +
          fragmentTexTransform.xy;
      vec4 texColor = TextureLookup(s_texture, texCoord);
      vec4 d4 = min(edge_dist[0], edge_dist[1]);
      vec2 d2 = min(d4.xz, d4.yw);
      float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
      gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) *
          alpha * aa;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderRGBATexAlphaMask::FragmentShaderRGBATexAlphaMask()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      mask_tex_coord_scale_location_(-1) {}

void FragmentShaderRGBATexAlphaMask::Init(GLES2Interface* context,
                                          unsigned program,
                                          int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "s_mask",
    "alpha",
    "maskTexCoordScale",
    "maskTexCoordOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
}

std::string FragmentShaderRGBATexAlphaMask::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    uniform SamplerType s_mask;
    uniform TexCoordPrecision vec2 maskTexCoordScale;
    uniform TexCoordPrecision vec2 maskTexCoordOffset;
    uniform float alpha;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      TexCoordPrecision vec2 maskTexCoord =
          vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
               maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
      vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
      gl_FragColor = texColor * alpha * maskColor.w;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderRGBATexAlphaMaskAA::FragmentShaderRGBATexAlphaMaskAA()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      mask_tex_coord_scale_location_(-1),
      mask_tex_coord_offset_location_(-1) {}

void FragmentShaderRGBATexAlphaMaskAA::Init(GLES2Interface* context,
                                            unsigned program,
                                            int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "s_mask",
    "alpha",
    "maskTexCoordScale",
    "maskTexCoordOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
}

std::string FragmentShaderRGBATexAlphaMaskAA::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform SamplerType s_texture;
    uniform SamplerType s_mask;
    uniform TexCoordPrecision vec2 maskTexCoordScale;
    uniform TexCoordPrecision vec2 maskTexCoordOffset;
    uniform float alpha;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      TexCoordPrecision vec2 maskTexCoord =
          vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
               maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
      vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
      vec4 d4 = min(edge_dist[0], edge_dist[1]);
      vec2 d2 = min(d4.xz, d4.yw);
      float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
      gl_FragColor = texColor * alpha * maskColor.w * aa;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderRGBATexAlphaMaskColorMatrixAA::
    FragmentShaderRGBATexAlphaMaskColorMatrixAA()
        : sampler_location_(-1),
          mask_sampler_location_(-1),
          alpha_location_(-1),
          mask_tex_coord_scale_location_(-1),
          color_matrix_location_(-1),
          color_offset_location_(-1) {}

void FragmentShaderRGBATexAlphaMaskColorMatrixAA::Init(
    GLES2Interface* context,
    unsigned program,
    int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "s_mask",
    "alpha",
    "maskTexCoordScale",
    "maskTexCoordOffset",
    "colorMatrix",
    "colorOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
  color_matrix_location_ = locations[5];
  color_offset_location_ = locations[6];
}

std::string FragmentShaderRGBATexAlphaMaskColorMatrixAA::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform SamplerType s_texture;
    uniform SamplerType s_mask;
    uniform vec2 maskTexCoordScale;
    uniform vec2 maskTexCoordOffset;
    uniform mat4 colorMatrix;
    uniform vec4 colorOffset;
    uniform float alpha;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      float nonZeroAlpha = max(texColor.a, 0.00001);
      texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
      texColor = colorMatrix * texColor + colorOffset;
      texColor.rgb *= texColor.a;
      texColor = clamp(texColor, 0.0, 1.0);
      TexCoordPrecision vec2 maskTexCoord =
          vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
               maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
      vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
      vec4 d4 = min(edge_dist[0], edge_dist[1]);
      vec2 d2 = min(d4.xz, d4.yw);
      float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
      gl_FragColor = texColor * alpha * maskColor.w * aa;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderRGBATexAlphaColorMatrixAA::
    FragmentShaderRGBATexAlphaColorMatrixAA()
        : sampler_location_(-1),
          alpha_location_(-1),
          color_matrix_location_(-1),
          color_offset_location_(-1) {}

void FragmentShaderRGBATexAlphaColorMatrixAA::Init(
      GLES2Interface* context,
      unsigned program,
      int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "alpha",
    "colorMatrix",
    "colorOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  color_matrix_location_ = locations[2];
  color_offset_location_ = locations[3];
}

std::string FragmentShaderRGBATexAlphaColorMatrixAA::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform SamplerType s_texture;
    uniform float alpha;
    uniform mat4 colorMatrix;
    uniform vec4 colorOffset;
    varying TexCoordPrecision vec2 v_texCoord;
    varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      float nonZeroAlpha = max(texColor.a, 0.00001);
      texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
      texColor = colorMatrix * texColor + colorOffset;
      texColor.rgb *= texColor.a;
      texColor = clamp(texColor, 0.0, 1.0);
      vec4 d4 = min(edge_dist[0], edge_dist[1]);
      vec2 d2 = min(d4.xz, d4.yw);
      float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
      gl_FragColor = texColor * alpha * aa;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderRGBATexAlphaMaskColorMatrix::
    FragmentShaderRGBATexAlphaMaskColorMatrix()
        : sampler_location_(-1),
          mask_sampler_location_(-1),
          alpha_location_(-1),
          mask_tex_coord_scale_location_(-1) {}

void FragmentShaderRGBATexAlphaMaskColorMatrix::Init(
    GLES2Interface* context,
    unsigned program,
    int* base_uniform_index) {
  static const char* uniforms[] = {
    "s_texture",
    "s_mask",
    "alpha",
    "maskTexCoordScale",
    "maskTexCoordOffset",
    "colorMatrix",
    "colorOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
  color_matrix_location_ = locations[5];
  color_offset_location_ = locations[6];
}

std::string FragmentShaderRGBATexAlphaMaskColorMatrix::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType s_texture;
    uniform SamplerType s_mask;
    uniform vec2 maskTexCoordScale;
    uniform vec2 maskTexCoordOffset;
    uniform mat4 colorMatrix;
    uniform vec4 colorOffset;
    uniform float alpha;
    void main() {
      vec4 texColor = TextureLookup(s_texture, v_texCoord);
      float nonZeroAlpha = max(texColor.a, 0.00001);
      texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
      texColor = colorMatrix * texColor + colorOffset;
      texColor.rgb *= texColor.a;
      texColor = clamp(texColor, 0.0, 1.0);
      TexCoordPrecision vec2 maskTexCoord =
          vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
               maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
      vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
      gl_FragColor = texColor * alpha * maskColor.w;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderYUVVideo::FragmentShaderYUVVideo()
    : y_texture_location_(-1),
      u_texture_location_(-1),
      v_texture_location_(-1),
      alpha_location_(-1),
      yuv_matrix_location_(-1),
      yuv_adj_location_(-1) {}

void FragmentShaderYUVVideo::Init(GLES2Interface* context,
                                  unsigned program,
                                  int* base_uniform_index) {
  static const char* uniforms[] = {
    "y_texture",
    "u_texture",
    "v_texture",
    "alpha",
    "yuv_matrix",
    "yuv_adj",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  y_texture_location_ = locations[0];
  u_texture_location_ = locations[1];
  v_texture_location_ = locations[2];
  alpha_location_ = locations[3];
  yuv_matrix_location_ = locations[4];
  yuv_adj_location_ = locations[5];
}

std::string FragmentShaderYUVVideo::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    precision mediump int;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType y_texture;
    uniform SamplerType u_texture;
    uniform SamplerType v_texture;
    uniform float alpha;
    uniform vec3 yuv_adj;
    uniform mat3 yuv_matrix;
    void main() {
      float y_raw = TextureLookup(y_texture, v_texCoord).x;
      float u_unsigned = TextureLookup(u_texture, v_texCoord).x;
      float v_unsigned = TextureLookup(v_texture, v_texCoord).x;
      vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;
      vec3 rgb = yuv_matrix * yuv;
      gl_FragColor = vec4(rgb, 1.0) * alpha;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderYUVAVideo::FragmentShaderYUVAVideo()
    : y_texture_location_(-1),
      u_texture_location_(-1),
      v_texture_location_(-1),
      a_texture_location_(-1),
      alpha_location_(-1),
      yuv_matrix_location_(-1),
      yuv_adj_location_(-1) {
}

void FragmentShaderYUVAVideo::Init(GLES2Interface* context,
                                   unsigned program,
                                   int* base_uniform_index) {
  static const char* uniforms[] = {
      "y_texture",
      "u_texture",
      "v_texture",
      "a_texture",
      "alpha",
      "cc_matrix",
      "yuv_adj",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  y_texture_location_ = locations[0];
  u_texture_location_ = locations[1];
  v_texture_location_ = locations[2];
  a_texture_location_ = locations[3];
  alpha_location_ = locations[4];
  yuv_matrix_location_ = locations[5];
  yuv_adj_location_ = locations[6];
}

std::string FragmentShaderYUVAVideo::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    precision mediump int;
    varying TexCoordPrecision vec2 v_texCoord;
    uniform SamplerType y_texture;
    uniform SamplerType u_texture;
    uniform SamplerType v_texture;
    uniform SamplerType a_texture;
    uniform float alpha;
    uniform vec3 yuv_adj;
    uniform mat3 yuv_matrix;
    void main() {
      float y_raw = TextureLookup(y_texture, v_texCoord).x;
      float u_unsigned = TextureLookup(u_texture, v_texCoord).x;
      float v_unsigned = TextureLookup(v_texture, v_texCoord).x;
      float a_raw = TextureLookup(a_texture, v_texCoord).x;
      vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;
      vec3 rgb = yuv_matrix * yuv;
      gl_FragColor = vec4(rgb, 1.0) * (alpha * a_raw);
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderColor::FragmentShaderColor()
    : color_location_(-1) {}

void FragmentShaderColor::Init(GLES2Interface* context,
                               unsigned program,
                               int* base_uniform_index) {
  static const char* uniforms[] = {
    "color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  color_location_ = locations[0];
}

std::string FragmentShaderColor::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform vec4 color;
    void main() {
      gl_FragColor = color;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderColorAA::FragmentShaderColorAA()
    : color_location_(-1) {}

void FragmentShaderColorAA::Init(GLES2Interface* context,
                                 unsigned program,
                                 int* base_uniform_index) {
  static const char* uniforms[] = {
    "color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  color_location_ = locations[0];
}

std::string FragmentShaderColorAA::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  return FRAGMENT_SHADER(
    precision mediump float;
    uniform vec4 color;
    varying vec4 edge_dist[2];  // 8 edge distances.

    void main() {
      vec4 d4 = min(edge_dist[0], edge_dist[1]);
      vec2 d2 = min(d4.xz, d4.yw);
      float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
      gl_FragColor = color * aa;
    }
  );  // NOLINT(whitespace/parens)
}

FragmentShaderCheckerboard::FragmentShaderCheckerboard()
    : alpha_location_(-1),
      tex_transform_location_(-1),
      frequency_location_(-1) {}

void FragmentShaderCheckerboard::Init(GLES2Interface* context,
                                      unsigned program,
                                      int* base_uniform_index) {
  static const char* uniforms[] = {
    "alpha",
    "texTransform",
    "frequency",
    "color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  alpha_location_ = locations[0];
  tex_transform_location_ = locations[1];
  frequency_location_ = locations[2];
  color_location_ = locations[3];
}

std::string FragmentShaderCheckerboard::GetShaderString(
    TexCoordPrecision precision, SamplerType sampler) const {
  // Shader based on Example 13-17 of "OpenGL ES 2.0 Programming Guide"
  // by Munshi, Ginsburg, Shreiner.
  return FRAGMENT_SHADER(
    precision mediump float;
    precision mediump int;
    varying vec2 v_texCoord;
    uniform float alpha;
    uniform float frequency;
    uniform vec4 texTransform;
    uniform vec4 color;
    void main() {
      vec4 color1 = vec4(1.0, 1.0, 1.0, 1.0);
      vec4 color2 = color;
      vec2 texCoord =
          clamp(v_texCoord, 0.0, 1.0) * texTransform.zw + texTransform.xy;
      vec2 coord = mod(floor(texCoord * frequency * 2.0), 2.0);
      float picker = abs(coord.x - coord.y);  // NOLINT
      gl_FragColor = mix(color1, color2, picker) * alpha;
    }
  );  // NOLINT(whitespace/parens)
}

}  // namespace cc
