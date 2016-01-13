// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"

#include <string>
#include <OpenGL/gl.h>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"

namespace content {

namespace {

// Convenience macro allowing GLSL programs to be specified inline, and to be
// automatically converted into string form by the C preprocessor.
#define GLSL_PROGRAM_AS_STRING(shader_code) #shader_code

// As required by the spec, add the version directive to the beginning of each
// program to activate the expected syntax and built-in features.  GLSL version
// 1.2 is the latest version supported by MacOS 10.6.
const char kVersionDirective[] = "#version 120\n";

// Allow switchable output swizzling from RGBToYV12 fragment shaders (needed for
// workaround; see comments in CompositingIOSurfaceShaderPrograms ctor).
const char kOutputSwizzleMacroNormal[] = "#define OUTPUT_PIXEL_ORDERING bgra\n";
const char kOutputSwizzleMacroSwapRB[] = "#define OUTPUT_PIXEL_ORDERING rgba\n";

// Only the bare-bones calculations here for speed.
const char kvsBlit[] = GLSL_PROGRAM_AS_STRING(
    varying vec2 texture_coord;
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
      texture_coord = gl_MultiTexCoord0.xy;
    }
);

// Just samples the texture.
const char kfsBlit[] = GLSL_PROGRAM_AS_STRING(
    uniform sampler2DRect texture_;
    varying vec2 texture_coord;
    void main() {
      gl_FragColor = vec4(texture2DRect(texture_, texture_coord).rgb, 1.0);
    }
);


// Only calculates position.
const char kvsSolidWhite[] = GLSL_PROGRAM_AS_STRING(
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    }
);

// Always white.
const char kfsSolidWhite[] = GLSL_PROGRAM_AS_STRING(
    void main() {
      gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
    }
);


///////////////////////////////////////////////////////////////////////
// RGB24 to YV12 in two passes; writing two 8888 targets each pass.
//
// YV12 is full-resolution luma and half-resolution blue/red chroma.
//
//                  (original)
//    XRGB XRGB XRGB XRGB XRGB XRGB XRGB XRGB
//    XRGB XRGB XRGB XRGB XRGB XRGB XRGB XRGB
//    XRGB XRGB XRGB XRGB XRGB XRGB XRGB XRGB
//    XRGB XRGB XRGB XRGB XRGB XRGB XRGB XRGB
//    XRGB XRGB XRGB XRGB XRGB XRGB XRGB XRGB
//    XRGB XRGB XRGB XRGB XRGB XRGB XRGB XRGB
//      |
//      |      (y plane)    (temporary)
//      |      YYYY YYYY     UUVV UUVV
//      +--> { YYYY YYYY  +  UUVV UUVV }
//             YYYY YYYY     UUVV UUVV
//   First     YYYY YYYY     UUVV UUVV
//    pass     YYYY YYYY     UUVV UUVV
//             YYYY YYYY     UUVV UUVV
//                              |
//                              |  (u plane) (v plane)
//   Second                     |      UUUU   VVVV
//     pass                     +--> { UUUU + VVVV }
//                                     UUUU   VVVV
//
///////////////////////////////////////////////////////////////////////

// Phase one of RGB24->YV12 conversion: vsFetch4Pixels/fsConvertRGBtoY8UV44
//
// Writes four source pixels at a time to a full-size Y plane and a half-width
// interleaved UV plane.  After execution, the Y plane is complete but the UV
// planes still need to be de-interleaved and vertically scaled.
const char kRGBtoYV12_vsFetch4Pixels[] = GLSL_PROGRAM_AS_STRING(
    uniform float texel_scale_x_;
    varying vec2 texture_coord0;
    varying vec2 texture_coord1;
    varying vec2 texture_coord2;
    varying vec2 texture_coord3;
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

      vec2 texcoord_base = gl_MultiTexCoord0.xy;
      vec2 one_texel_x = vec2(texel_scale_x_, 0.0);
      texture_coord0 = texcoord_base - 1.5 * one_texel_x;
      texture_coord1 = texcoord_base - 0.5 * one_texel_x;
      texture_coord2 = texcoord_base + 0.5 * one_texel_x;
      texture_coord3 = texcoord_base + 1.5 * one_texel_x;
    }
);

const char kRGBtoYV12_fsConvertRGBtoY8UV44[] = GLSL_PROGRAM_AS_STRING(
    const vec3 rgb_to_y = vec3(0.257, 0.504, 0.098);
    const vec3 rgb_to_u = vec3(-0.148, -0.291, 0.439);
    const vec3 rgb_to_v = vec3(0.439, -0.368, -0.071);
    const float y_bias = 0.0625;
    const float uv_bias = 0.5;
    uniform sampler2DRect texture_;
    varying vec2 texture_coord0;
    varying vec2 texture_coord1;
    varying vec2 texture_coord2;
    varying vec2 texture_coord3;
    void main() {
      // Load the four texture samples.
      vec3 pixel0 = texture2DRect(texture_, texture_coord0).rgb;
      vec3 pixel1 = texture2DRect(texture_, texture_coord1).rgb;
      vec3 pixel2 = texture2DRect(texture_, texture_coord2).rgb;
      vec3 pixel3 = texture2DRect(texture_, texture_coord3).rgb;

      // RGB -> Y conversion (x4).
      vec4 yyyy = vec4(dot(pixel0, rgb_to_y),
                       dot(pixel1, rgb_to_y),
                       dot(pixel2, rgb_to_y),
                       dot(pixel3, rgb_to_y)) + y_bias;

      // Average adjacent texture samples while converting RGB->UV.  This is the
      // same as color converting then averaging, but slightly less math.  These
      // values will be in the range [-0.439f, +0.439f] and still need to have
      // the bias term applied.
      vec3 blended_pixel0 = pixel0 + pixel1;
      vec3 blended_pixel1 = pixel2 + pixel3;
      vec2 uu = vec2(dot(blended_pixel0, rgb_to_u),
                     dot(blended_pixel1, rgb_to_u)) / 2.0;
      vec2 vv = vec2(dot(blended_pixel0, rgb_to_v),
                     dot(blended_pixel1, rgb_to_v)) / 2.0;

      gl_FragData[0] = yyyy.OUTPUT_PIXEL_ORDERING;
      gl_FragData[1] = vec4(uu, vv) + uv_bias;
    }
);

// Phase two of RGB24->YV12 conversion: vsFetch2Pixels/fsConvertUV44toU2V2
//
// Deals with UV only.  Input is two UUVV quads.  The pixels have already been
// scaled horizontally prior to this point, and vertical scaling will now happen
// via bilinear interpolation during texture sampling.  Output is two color
// planes U and V, packed four pixels to a "RGBA" quad.
const char kRGBtoYV12_vsFetch2Pixels[] = GLSL_PROGRAM_AS_STRING(
    varying vec2 texture_coord0;
    varying vec2 texture_coord1;
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

      vec2 texcoord_base = gl_MultiTexCoord0.xy;
      texture_coord0 = texcoord_base - vec2(0.5, 0.0);
      texture_coord1 = texcoord_base + vec2(0.5, 0.0);
    }
);

const char kRGBtoYV12_fsConvertUV44toU2V2[] = GLSL_PROGRAM_AS_STRING(
    uniform sampler2DRect texture_;
    varying vec2 texture_coord0;
    varying vec2 texture_coord1;
    void main() {
      // We're just sampling two pixels and unswizzling them.  There's no need
      // to do vertical scaling with math, since bilinear interpolation in the
      // sampler takes care of that.
      vec4 lo_uuvv = texture2DRect(texture_, texture_coord0);
      vec4 hi_uuvv = texture2DRect(texture_, texture_coord1);
      gl_FragData[0] = vec4(lo_uuvv.rg, hi_uuvv.rg).OUTPUT_PIXEL_ORDERING;
      gl_FragData[1] = vec4(lo_uuvv.ba, hi_uuvv.ba).OUTPUT_PIXEL_ORDERING;
    }
);


enum ShaderProgram {
  SHADER_PROGRAM_BLIT = 0,
  SHADER_PROGRAM_SOLID_WHITE,
  SHADER_PROGRAM_RGB_TO_YV12__1_OF_2,
  SHADER_PROGRAM_RGB_TO_YV12__2_OF_2,
  NUM_SHADER_PROGRAMS
};

// The code snippets that together make up an entire vertex shader program.
const char* kVertexShaderSourceCodeMap[] = {
  // SHADER_PROGRAM_BLIT
  kvsBlit,
  // SHADER_PROGRAM_SOLID_WHITE
  kvsSolidWhite,

  // SHADER_PROGRAM_RGB_TO_YV12__1_OF_2
  kRGBtoYV12_vsFetch4Pixels,
  // SHADER_PROGRAM_RGB_TO_YV12__2_OF_2
  kRGBtoYV12_vsFetch2Pixels,
};

// The code snippets that together make up an entire fragment shader program.
const char* kFragmentShaderSourceCodeMap[] = {
  // SHADER_PROGRAM_BLIT
  kfsBlit,
  // SHADER_PROGRAM_SOLID_WHITE
  kfsSolidWhite,

  // SHADER_PROGRAM_RGB_TO_YV12__1_OF_2
  kRGBtoYV12_fsConvertRGBtoY8UV44,
  // SHADER_PROGRAM_RGB_TO_YV12__2_OF_2
  kRGBtoYV12_fsConvertUV44toU2V2,
};

GLuint CompileShaderGLSL(ShaderProgram shader_program, GLenum shader_type,
                         bool output_swap_rb) {
  TRACE_EVENT2("gpu", "CompileShaderGLSL",
               "program", shader_program,
               "type", shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment");

  DCHECK_GE(shader_program, 0);
  DCHECK_LT(shader_program, NUM_SHADER_PROGRAMS);

  const GLuint shader = glCreateShader(shader_type);
  DCHECK_NE(shader, 0u);

  // Select and compile the shader program source code.
  if (shader_type == GL_VERTEX_SHADER) {
    const GLchar* source_snippets[] = {
      kVersionDirective,
      kVertexShaderSourceCodeMap[shader_program],
    };
    glShaderSource(shader, arraysize(source_snippets), source_snippets, NULL);
  } else {
    DCHECK(shader_type == GL_FRAGMENT_SHADER);
    const GLchar* source_snippets[] = {
      kVersionDirective,
      output_swap_rb ? kOutputSwizzleMacroSwapRB : kOutputSwizzleMacroNormal,
      kFragmentShaderSourceCodeMap[shader_program],
    };
    glShaderSource(shader, arraysize(source_snippets), source_snippets, NULL);
  }
  glCompileShader(shader);

  // Check for successful compilation.  On error in debug builds, pull the info
  // log and emit the compiler messages.
  GLint error;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &error);
  if (error != GL_TRUE) {
#ifndef NDEBUG
    static const int kMaxInfoLogLength = 8192;
    scoped_ptr<char[]> buffer(new char[kMaxInfoLogLength]);
    GLsizei length_returned = 0;
    glGetShaderInfoLog(shader, kMaxInfoLogLength - 1, &length_returned,
                       buffer.get());
    buffer[kMaxInfoLogLength - 1] = '\0';
    DLOG(ERROR) << "Failed to compile "
                << (shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                << " shader for program " << shader_program << ":\n"
                << buffer.get()
                << (length_returned >= kMaxInfoLogLength ?
                        "\n*** TRUNCATED! ***" : "");
#endif
    glDeleteShader(shader);
    return 0;
  }

  // Success!
  return shader;
}

GLuint CompileAndLinkProgram(ShaderProgram which, bool output_swap_rb) {
  TRACE_EVENT1("gpu", "CompileAndLinkProgram", "program", which);

  // Compile and link a new shader program.
  const GLuint vertex_shader =
      CompileShaderGLSL(which, GL_VERTEX_SHADER, false);
  const GLuint fragment_shader =
      CompileShaderGLSL(which, GL_FRAGMENT_SHADER, output_swap_rb);
  const GLuint program = glCreateProgram();
  DCHECK_NE(program, 0u);
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  // Flag shaders for deletion so that they will be deleted automatically when
  // the program is later deleted.
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  // Check that the program successfully linked.
  GLint error = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &error);
  if (error != GL_TRUE) {
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

}  // namespace


CompositingIOSurfaceShaderPrograms::CompositingIOSurfaceShaderPrograms()
    : rgb_to_yv12_output_format_(GL_BGRA) {
  COMPILE_ASSERT(kNumShaderPrograms == NUM_SHADER_PROGRAMS,
                 header_constant_disagrees_with_enum);
  COMPILE_ASSERT(arraysize(kVertexShaderSourceCodeMap) == NUM_SHADER_PROGRAMS,
                 vertex_shader_source_code_map_incorrect_size);
  COMPILE_ASSERT(arraysize(kFragmentShaderSourceCodeMap) == NUM_SHADER_PROGRAMS,
                 fragment_shader_source_code_map_incorrect_size);

  memset(shader_programs_, 0, sizeof(shader_programs_));
  for (size_t i = 0; i < arraysize(texture_var_locations_); ++i)
    texture_var_locations_[i] = -1;
  for (size_t i = 0; i < arraysize(texel_scale_x_var_locations_); ++i)
    texel_scale_x_var_locations_[i] = -1;

  // Look for the swizzle_rgba_for_async_readpixels driver bug workaround and
  // modify rgb_to_yv12_output_format_ if necessary.
  // See: http://crbug.com/265115
  GpuDataManagerImpl* const manager = GpuDataManagerImpl::GetInstance();
  if (manager) {
    base::ListValue workarounds;
    manager->GetDriverBugWorkarounds(&workarounds);
    base::ListValue::const_iterator it = workarounds.Find(
        base::StringValue(gpu::GpuDriverBugWorkaroundTypeToString(
            gpu::SWIZZLE_RGBA_FOR_ASYNC_READPIXELS)));
    if (it != workarounds.end())
      rgb_to_yv12_output_format_ = GL_RGBA;
  }
  DVLOG(1) << "Using RGBToYV12 fragment shader output format: "
           << (rgb_to_yv12_output_format_ == GL_BGRA ? "BGRA" : "RGBA");
}

CompositingIOSurfaceShaderPrograms::~CompositingIOSurfaceShaderPrograms() {
#ifndef NDEBUG
  for (size_t i = 0; i < arraysize(shader_programs_); ++i)
    DCHECK_EQ(shader_programs_[i], 0u) << "Failed to call Reset().";
#endif
}

void CompositingIOSurfaceShaderPrograms::Reset() {
  for (size_t i = 0; i < arraysize(shader_programs_); ++i) {
    if (shader_programs_[i] != 0u) {
      glDeleteProgram(shader_programs_[i]);
      shader_programs_[i] = 0u;
    }
  }
  for (size_t i = 0; i < arraysize(texture_var_locations_); ++i)
    texture_var_locations_[i] = -1;
  for (size_t i = 0; i < arraysize(texel_scale_x_var_locations_); ++i)
    texel_scale_x_var_locations_[i] = -1;
}

bool CompositingIOSurfaceShaderPrograms::UseBlitProgram() {
  const GLuint program = GetShaderProgram(SHADER_PROGRAM_BLIT);
  if (program == 0u)
    return false;
  glUseProgram(program);
  BindUniformTextureVariable(SHADER_PROGRAM_BLIT, 0);
  return true;
}

bool CompositingIOSurfaceShaderPrograms::UseSolidWhiteProgram() {
  const GLuint program = GetShaderProgram(SHADER_PROGRAM_SOLID_WHITE);
  if (program == 0u)
    return false;
  glUseProgram(program);
  return true;
}

bool CompositingIOSurfaceShaderPrograms::UseRGBToYV12Program(
    int pass_number, float texel_scale_x) {
  const int which = SHADER_PROGRAM_RGB_TO_YV12__1_OF_2 + pass_number - 1;
  DCHECK_GE(which, SHADER_PROGRAM_RGB_TO_YV12__1_OF_2);
  DCHECK_LE(which, SHADER_PROGRAM_RGB_TO_YV12__2_OF_2);

  const GLuint program = GetShaderProgram(which);
  if (program == 0u)
    return false;
  glUseProgram(program);
  BindUniformTextureVariable(which, 0);
  if (which == SHADER_PROGRAM_RGB_TO_YV12__1_OF_2) {
    BindUniformTexelScaleXVariable(which, texel_scale_x);
  } else {
    // The second pass doesn't have a texel_scale_x uniform variable since it's
    // never supposed to be doing any scaling (i.e., outside of the usual
    // 2x2-->1x1 that's already built into the process).
    DCHECK_EQ(texel_scale_x, 1.0f);
  }
  return true;
}

void CompositingIOSurfaceShaderPrograms::SetOutputFormatForTesting(
    GLenum format) {
  rgb_to_yv12_output_format_ = format;
  Reset();
}

GLuint CompositingIOSurfaceShaderPrograms::GetShaderProgram(int which) {
  if (shader_programs_[which] == 0u) {
    shader_programs_[which] =
        CompileAndLinkProgram(static_cast<ShaderProgram>(which),
                              rgb_to_yv12_output_format_ == GL_RGBA);
    DCHECK_NE(shader_programs_[which], 0u)
        << "Failed to create ShaderProgram " << which;
  }
  return shader_programs_[which];
}

void CompositingIOSurfaceShaderPrograms::BindUniformTextureVariable(
    int which, int texture_unit_offset) {
  if (texture_var_locations_[which] == -1) {
    texture_var_locations_[which] =
        glGetUniformLocation(GetShaderProgram(which), "texture_");
    DCHECK_NE(texture_var_locations_[which], -1)
        << "Failed to find location of uniform variable: texture_";
  }
  glUniform1i(texture_var_locations_[which], texture_unit_offset);
}

void CompositingIOSurfaceShaderPrograms::BindUniformTexelScaleXVariable(
    int which, float texel_scale_x) {
  if (texel_scale_x_var_locations_[which] == -1) {
    texel_scale_x_var_locations_[which] =
        glGetUniformLocation(GetShaderProgram(which), "texel_scale_x_");
    DCHECK_NE(texel_scale_x_var_locations_[which], -1)
        << "Failed to find location of uniform variable: texel_scale_x_";
  }
  glUniform1f(texel_scale_x_var_locations_[which], texel_scale_x);
}

}  // namespace content
