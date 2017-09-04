// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/gl_helper_scaling.h"

#include <stddef.h>

#include <deque>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using gpu::gles2::GLES2Interface;

namespace display_compositor {

GLHelperScaling::GLHelperScaling(GLES2Interface* gl, GLHelper* helper)
    : gl_(gl), helper_(helper), vertex_attributes_buffer_(gl_) {
  InitBuffer();
}

GLHelperScaling::~GLHelperScaling() {}

// Used to keep track of a generated shader program. The program
// is passed in as text through Setup and is used by calling
// UseProgram() with the right parameters. Note that |gl_|
// and |helper_| are assumed to live longer than this program.
class ShaderProgram : public base::RefCounted<ShaderProgram> {
 public:
  ShaderProgram(GLES2Interface* gl, GLHelper* helper)
      : gl_(gl),
        helper_(helper),
        program_(gl_->CreateProgram()),
        position_location_(-1),
        texcoord_location_(-1),
        src_subrect_location_(-1),
        src_pixelsize_location_(-1),
        dst_pixelsize_location_(-1),
        scaling_vector_location_(-1),
        color_weights_location_(-1) {}

  // Compile shader program.
  void Setup(const GLchar* vertex_shader_text,
             const GLchar* fragment_shader_text);

  // UseProgram must be called with GL_TEXTURE_2D bound to the
  // source texture and GL_ARRAY_BUFFER bound to a vertex
  // attribute buffer.
  void UseProgram(const gfx::Size& src_size,
                  const gfx::Rect& src_subrect,
                  const gfx::Size& dst_size,
                  bool scale_x,
                  bool flip_y,
                  GLfloat color_weights[4]);

  bool Initialized() const { return position_location_ != -1; }

 private:
  friend class base::RefCounted<ShaderProgram>;
  ~ShaderProgram() { gl_->DeleteProgram(program_); }

  GLES2Interface* gl_;
  GLHelper* helper_;

  // A program for copying a source texture into a destination texture.
  GLuint program_;

  // The location of the position in the program.
  GLint position_location_;
  // The location of the texture coordinate in the program.
  GLint texcoord_location_;
  // The location of the source texture in the program.
  GLint texture_location_;
  // The location of the texture coordinate of
  // the sub-rectangle in the program.
  GLint src_subrect_location_;
  // Location of size of source image in pixels.
  GLint src_pixelsize_location_;
  // Location of size of destination image in pixels.
  GLint dst_pixelsize_location_;
  // Location of vector for scaling direction.
  GLint scaling_vector_location_;
  // Location of color weights.
  GLint color_weights_location_;

  DISALLOW_COPY_AND_ASSIGN(ShaderProgram);
};

// Implementation of a single stage in a scaler pipeline. If the pipeline has
// multiple stages, it calls Scale() on the subscaler, then further scales the
// output. Caches textures and framebuffers to avoid allocating/deleting
// them once per frame, which can be expensive on some drivers.
class ScalerImpl : public GLHelper::ScalerInterface,
                   public GLHelperScaling::ShaderInterface {
 public:
  // |gl| and |copy_impl| are expected to live longer than this object.
  // |src_size| is the size of the input texture in pixels.
  // |dst_size| is the size of the output texutre in pixels.
  // |src_subrect| is the portion of the src to copy to the output texture.
  // If |scale_x| is true, we are scaling along the X axis, otherwise Y.
  // If we are scaling in both X and Y, |scale_x| is ignored.
  // If |vertically_flip_texture| is true, output will be upside-down.
  // If |swizzle| is true, RGBA will be transformed into BGRA.
  // |color_weights| are only used together with SHADER_PLANAR to specify
  //   how to convert RGB colors into a single value.
  ScalerImpl(GLES2Interface* gl,
             GLHelperScaling* scaler_helper,
             const GLHelperScaling::ScalerStage& scaler_stage,
             ScalerImpl* subscaler,
             const float* color_weights)
      : gl_(gl),
        scaler_helper_(scaler_helper),
        spec_(scaler_stage),
        intermediate_texture_(0),
        dst_framebuffer_(gl),
        subscaler_(subscaler) {
    if (color_weights) {
      color_weights_[0] = color_weights[0];
      color_weights_[1] = color_weights[1];
      color_weights_[2] = color_weights[2];
      color_weights_[3] = color_weights[3];
    } else {
      color_weights_[0] = 0.0;
      color_weights_[1] = 0.0;
      color_weights_[2] = 0.0;
      color_weights_[3] = 0.0;
    }
    shader_program_ =
        scaler_helper_->GetShaderProgram(spec_.shader, spec_.swizzle);

    if (subscaler_) {
      intermediate_texture_ = 0u;
      gl_->GenTextures(1, &intermediate_texture_);
      ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_,
                                                        intermediate_texture_);
      gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, spec_.src_size.width(),
                      spec_.src_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      NULL);
    }
  }

  ~ScalerImpl() override {
    if (intermediate_texture_) {
      gl_->DeleteTextures(1, &intermediate_texture_);
    }
  }

  // GLHelperShader::ShaderInterface implementation.
  void Execute(GLuint source_texture,
               const std::vector<GLuint>& dest_textures) override {
    if (subscaler_) {
      subscaler_->Scale(source_texture, intermediate_texture_);
      source_texture = intermediate_texture_;
    }

    ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(
        gl_, dst_framebuffer_);
    DCHECK_GT(dest_textures.size(), 0U);
    std::unique_ptr<GLenum[]> buffers(new GLenum[dest_textures.size()]);
    for (size_t t = 0; t < dest_textures.size(); t++) {
      ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, dest_textures[t]);
      gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + t,
                                GL_TEXTURE_2D, dest_textures[t], 0);
      buffers[t] = GL_COLOR_ATTACHMENT0 + t;
    }
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, source_texture);

    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(
        gl_, scaler_helper_->vertex_attributes_buffer_);
    shader_program_->UseProgram(spec_.src_size, spec_.src_subrect,
                                spec_.dst_size, spec_.scale_x,
                                spec_.vertically_flip_texture, color_weights_);
    gl_->Viewport(0, 0, spec_.dst_size.width(), spec_.dst_size.height());

    if (dest_textures.size() > 1) {
      DCHECK_LE(static_cast<int>(dest_textures.size()),
                scaler_helper_->helper_->MaxDrawBuffers());
      gl_->DrawBuffersEXT(dest_textures.size(), buffers.get());
    }
    // Conduct texture mapping by drawing a quad composed of two triangles.
    gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (dest_textures.size() > 1) {
      // Set the draw buffers back to not confuse others.
      gl_->DrawBuffersEXT(1, &buffers[0]);
    }
  }

  // GLHelper::ScalerInterface implementation.
  void Scale(GLuint source_texture, GLuint dest_texture) override {
    std::vector<GLuint> tmp(1);
    tmp[0] = dest_texture;
    Execute(source_texture, tmp);
  }

  const gfx::Size& SrcSize() override {
    if (subscaler_) {
      return subscaler_->SrcSize();
    }
    return spec_.src_size;
  }
  const gfx::Rect& SrcSubrect() override {
    if (subscaler_) {
      return subscaler_->SrcSubrect();
    }
    return spec_.src_subrect;
  }
  const gfx::Size& DstSize() override { return spec_.dst_size; }

 private:
  GLES2Interface* gl_;
  GLHelperScaling* scaler_helper_;
  GLHelperScaling::ScalerStage spec_;
  GLfloat color_weights_[4];
  GLuint intermediate_texture_;
  scoped_refptr<ShaderProgram> shader_program_;
  ScopedFramebuffer dst_framebuffer_;
  std::unique_ptr<ScalerImpl> subscaler_;
};

GLHelperScaling::ScalerStage::ScalerStage(ShaderType shader_,
                                          gfx::Size src_size_,
                                          gfx::Rect src_subrect_,
                                          gfx::Size dst_size_,
                                          bool scale_x_,
                                          bool vertically_flip_texture_,
                                          bool swizzle_)
    : shader(shader_),
      src_size(src_size_),
      src_subrect(src_subrect_),
      dst_size(dst_size_),
      scale_x(scale_x_),
      vertically_flip_texture(vertically_flip_texture_),
      swizzle(swizzle_) {}

GLHelperScaling::ScalerStage::ScalerStage(const ScalerStage& other) = default;

// The important inputs for this function is |x_ops| and
// |y_ops|. They represent scaling operations to be done
// on an imag of size |src_size|. If |quality| is SCALER_QUALITY_BEST,
// then we will interpret these scale operations literally and we'll
// create one scaler stage for each ScaleOp.  However, if |quality|
// is SCALER_QUALITY_GOOD, then we can do a whole bunch of optimizations
// by combining two or more ScaleOps in to a single scaler stage.
// Normally we process ScaleOps from |y_ops| first and |x_ops| after
// all |y_ops| are processed, but sometimes we can combine one or more
// operation from both queues essentially for free. This is the reason
// why |x_ops| and |y_ops| aren't just one single queue.
void GLHelperScaling::ConvertScalerOpsToScalerStages(
    GLHelper::ScalerQuality quality,
    gfx::Size src_size,
    gfx::Rect src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle,
    std::deque<GLHelperScaling::ScaleOp>* x_ops,
    std::deque<GLHelperScaling::ScaleOp>* y_ops,
    std::vector<ScalerStage>* scaler_stages) {
  while (!x_ops->empty() || !y_ops->empty()) {
    gfx::Size intermediate_size = src_subrect.size();
    std::deque<ScaleOp>* current_queue = NULL;

    if (!y_ops->empty()) {
      current_queue = y_ops;
    } else {
      current_queue = x_ops;
    }

    ShaderType current_shader = SHADER_BILINEAR;
    switch (current_queue->front().scale_factor) {
      case 0:
        if (quality == GLHelper::SCALER_QUALITY_BEST) {
          current_shader = SHADER_BICUBIC_UPSCALE;
        }
        break;
      case 2:
        if (quality == GLHelper::SCALER_QUALITY_BEST) {
          current_shader = SHADER_BICUBIC_HALF_1D;
        }
        break;
      case 3:
        DCHECK(quality != GLHelper::SCALER_QUALITY_BEST);
        current_shader = SHADER_BILINEAR3;
        break;
      default:
        NOTREACHED();
    }
    bool scale_x = current_queue->front().scale_x;
    current_queue->front().UpdateSize(&intermediate_size);
    current_queue->pop_front();

    // Optimization: Sometimes we can combine 2-4 scaling operations into
    // one operation.
    if (quality == GLHelper::SCALER_QUALITY_GOOD) {
      if (!current_queue->empty() && current_shader == SHADER_BILINEAR) {
        // Combine two steps in the same dimension.
        current_queue->front().UpdateSize(&intermediate_size);
        current_queue->pop_front();
        current_shader = SHADER_BILINEAR2;
        if (!current_queue->empty()) {
          // Combine three steps in the same dimension.
          current_queue->front().UpdateSize(&intermediate_size);
          current_queue->pop_front();
          current_shader = SHADER_BILINEAR4;
        }
      }
      // Check if we can combine some steps in the other dimension as well.
      // Since all shaders currently use GL_LINEAR, we can easily scale up
      // or scale down by exactly 2x at the same time as we do another
      // operation. Currently, the following mergers are supported:
      // * 1 bilinear Y-pass with 1 bilinear X-pass (up or down)
      // * 2 bilinear Y-passes with 2 bilinear X-passes
      // * 1 bilinear Y-pass with N bilinear X-pass
      // * N bilinear Y-passes with 1 bilinear X-pass (down only)
      // Measurements indicate that generalizing this for 3x3 and 4x4
      // makes it slower on some platforms, such as the Pixel.
      if (!scale_x && x_ops->size() > 0 && x_ops->front().scale_factor <= 2) {
        int x_passes = 0;
        if (current_shader == SHADER_BILINEAR2 && x_ops->size() >= 2) {
          // 2y + 2x passes
          x_passes = 2;
          current_shader = SHADER_BILINEAR2X2;
        } else if (current_shader == SHADER_BILINEAR) {
          // 1y + Nx passes
          scale_x = true;
          switch (x_ops->size()) {
            case 0:
              NOTREACHED();
            case 1:
              if (x_ops->front().scale_factor == 3) {
                current_shader = SHADER_BILINEAR3;
              }
              x_passes = 1;
              break;
            case 2:
              x_passes = 2;
              current_shader = SHADER_BILINEAR2;
              break;
            default:
              x_passes = 3;
              current_shader = SHADER_BILINEAR4;
              break;
          }
        } else if (x_ops->front().scale_factor == 2) {
          // Ny + 1x-downscale
          x_passes = 1;
        }

        for (int i = 0; i < x_passes; i++) {
          x_ops->front().UpdateSize(&intermediate_size);
          x_ops->pop_front();
        }
      }
    }

    scaler_stages->push_back(ScalerStage(current_shader, src_size, src_subrect,
                                         intermediate_size, scale_x,
                                         vertically_flip_texture, swizzle));
    src_size = intermediate_size;
    src_subrect = gfx::Rect(intermediate_size);
    vertically_flip_texture = false;
    swizzle = false;
  }
}

void GLHelperScaling::ComputeScalerStages(
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle,
    std::vector<ScalerStage>* scaler_stages) {
  if (quality == GLHelper::SCALER_QUALITY_FAST ||
      src_subrect.size() == dst_size) {
    scaler_stages->push_back(ScalerStage(SHADER_BILINEAR, src_size, src_subrect,
                                         dst_size, false,
                                         vertically_flip_texture, swizzle));
    return;
  }

  std::deque<GLHelperScaling::ScaleOp> x_ops, y_ops;
  GLHelperScaling::ScaleOp::AddOps(src_subrect.width(), dst_size.width(), true,
                                   quality == GLHelper::SCALER_QUALITY_GOOD,
                                   &x_ops);
  GLHelperScaling::ScaleOp::AddOps(
      src_subrect.height(), dst_size.height(), false,
      quality == GLHelper::SCALER_QUALITY_GOOD, &y_ops);

  ConvertScalerOpsToScalerStages(quality, src_size, src_subrect, dst_size,
                                 vertically_flip_texture, swizzle, &x_ops,
                                 &y_ops, scaler_stages);
}

GLHelper::ScalerInterface* GLHelperScaling::CreateScaler(
    GLHelper::ScalerQuality quality,
    gfx::Size src_size,
    gfx::Rect src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle) {
  std::vector<ScalerStage> scaler_stages;
  ComputeScalerStages(quality, src_size, src_subrect, dst_size,
                      vertically_flip_texture, swizzle, &scaler_stages);

  ScalerImpl* ret = NULL;
  for (unsigned int i = 0; i < scaler_stages.size(); i++) {
    ret = new ScalerImpl(gl_, this, scaler_stages[i], ret, NULL);
  }
  return ret;
}

GLHelper::ScalerInterface* GLHelperScaling::CreatePlanarScaler(
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle,
    const float color_weights[4]) {
  ScalerStage stage(SHADER_PLANAR, src_size, src_subrect, dst_size, true,
                    vertically_flip_texture, swizzle);
  return new ScalerImpl(gl_, this, stage, NULL, color_weights);
}

GLHelperScaling::ShaderInterface* GLHelperScaling::CreateYuvMrtShader(
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle,
    ShaderType shader) {
  DCHECK(shader == SHADER_YUV_MRT_PASS1 || shader == SHADER_YUV_MRT_PASS2);
  ScalerStage stage(shader, src_size, src_subrect, dst_size, true,
                    vertically_flip_texture, swizzle);
  return new ScalerImpl(gl_, this, stage, NULL, NULL);
}

const GLfloat GLHelperScaling::kVertexAttributes[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,  // vertex 0
    1.0f,  -1.0f, 1.0f, 0.0f,  // vertex 1
    -1.0f, 1.0f,  0.0f, 1.0f,  // vertex 2
    1.0f,  1.0f,  1.0f, 1.0f,
};  // vertex 3

void GLHelperScaling::InitBuffer() {
  ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(gl_,
                                                    vertex_attributes_buffer_);
  gl_->BufferData(GL_ARRAY_BUFFER, sizeof(kVertexAttributes), kVertexAttributes,
                  GL_STATIC_DRAW);
}

scoped_refptr<ShaderProgram> GLHelperScaling::GetShaderProgram(ShaderType type,
                                                               bool swizzle) {
  ShaderProgramKeyType key(type, swizzle);
  scoped_refptr<ShaderProgram>& cache_entry(shader_programs_[key]);
  if (!cache_entry.get()) {
    cache_entry = new ShaderProgram(gl_, helper_);
    std::basic_string<GLchar> vertex_program;
    std::basic_string<GLchar> fragment_program;
    std::basic_string<GLchar> vertex_header;
    std::basic_string<GLchar> fragment_directives;
    std::basic_string<GLchar> fragment_header;
    std::basic_string<GLchar> shared_variables;

    vertex_header.append(
        "precision highp float;\n"
        "attribute vec2 a_position;\n"
        "attribute vec2 a_texcoord;\n"
        "uniform vec4 src_subrect;\n");

    fragment_header.append(
        "precision mediump float;\n"
        "uniform sampler2D s_texture;\n");

    vertex_program.append(
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "  vec2 texcoord = src_subrect.xy + a_texcoord * src_subrect.zw;\n");

    switch (type) {
      case SHADER_BILINEAR:
        shared_variables.append("varying vec2 v_texcoord;\n");
        vertex_program.append("  v_texcoord = texcoord;\n");
        fragment_program.append(
            "  gl_FragColor = texture2D(s_texture, v_texcoord);\n");
        break;

      case SHADER_BILINEAR2:
        // This is equivialent to two passes of the BILINEAR shader above.
        // It can be used to scale an image down 1.0x-2.0x in either dimension,
        // or exactly 4x.
        shared_variables.append(
            "varying vec4 v_texcoords;\n");  // 2 texcoords packed in one quad
        vertex_header.append(
            "uniform vec2 scaling_vector;\n"
            "uniform vec2 dst_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector * src_subrect.zw / dst_pixelsize;\n"
            "  step /= 4.0;\n"
            "  v_texcoords.xy = texcoord + step;\n"
            "  v_texcoords.zw = texcoord - step;\n");

        fragment_program.append(
            "  gl_FragColor = (texture2D(s_texture, v_texcoords.xy) +\n"
            "                  texture2D(s_texture, v_texcoords.zw)) / 2.0;\n");
        break;

      case SHADER_BILINEAR3:
        // This is kind of like doing 1.5 passes of the BILINEAR shader.
        // It can be used to scale an image down 1.5x-3.0x, or exactly 6x.
        shared_variables.append(
            "varying vec4 v_texcoords1;\n"  // 2 texcoords packed in one quad
            "varying vec2 v_texcoords2;\n");
        vertex_header.append(
            "uniform vec2 scaling_vector;\n"
            "uniform vec2 dst_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector * src_subrect.zw / dst_pixelsize;\n"
            "  step /= 3.0;\n"
            "  v_texcoords1.xy = texcoord + step;\n"
            "  v_texcoords1.zw = texcoord;\n"
            "  v_texcoords2 = texcoord - step;\n");
        fragment_program.append(
            "  gl_FragColor = (texture2D(s_texture, v_texcoords1.xy) +\n"
            "                  texture2D(s_texture, v_texcoords1.zw) +\n"
            "                  texture2D(s_texture, v_texcoords2)) / 3.0;\n");
        break;

      case SHADER_BILINEAR4:
        // This is equivialent to three passes of the BILINEAR shader above,
        // It can be used to scale an image down 2.0x-4.0x or exactly 8x.
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append(
            "uniform vec2 scaling_vector;\n"
            "uniform vec2 dst_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector * src_subrect.zw / dst_pixelsize;\n"
            "  step /= 8.0;\n"
            "  v_texcoords[0].xy = texcoord - step * 3.0;\n"
            "  v_texcoords[0].zw = texcoord - step;\n"
            "  v_texcoords[1].xy = texcoord + step;\n"
            "  v_texcoords[1].zw = texcoord + step * 3.0;\n");
        fragment_program.append(
            "  gl_FragColor = (\n"
            "      texture2D(s_texture, v_texcoords[0].xy) +\n"
            "      texture2D(s_texture, v_texcoords[0].zw) +\n"
            "      texture2D(s_texture, v_texcoords[1].xy) +\n"
            "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
        break;

      case SHADER_BILINEAR2X2:
        // This is equivialent to four passes of the BILINEAR shader above.
        // Two in each dimension. It can be used to scale an image down
        // 1.0x-2.0x in both X and Y directions. Or, it could be used to
        // scale an image down by exactly 4x in both dimensions.
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append("uniform vec2 dst_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = src_subrect.zw / 4.0 / dst_pixelsize;\n"
            "  v_texcoords[0].xy = texcoord + vec2(step.x, step.y);\n"
            "  v_texcoords[0].zw = texcoord + vec2(step.x, -step.y);\n"
            "  v_texcoords[1].xy = texcoord + vec2(-step.x, step.y);\n"
            "  v_texcoords[1].zw = texcoord + vec2(-step.x, -step.y);\n");
        fragment_program.append(
            "  gl_FragColor = (\n"
            "      texture2D(s_texture, v_texcoords[0].xy) +\n"
            "      texture2D(s_texture, v_texcoords[0].zw) +\n"
            "      texture2D(s_texture, v_texcoords[1].xy) +\n"
            "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
        break;

      case SHADER_BICUBIC_HALF_1D:
        // This scales down texture by exactly half in one dimension.
        // directions in one pass. We use bilinear lookup to reduce
        // the number of texture reads from 8 to 4
        shared_variables.append(
            "const float CenterDist = 99.0 / 140.0;\n"
            "const float LobeDist = 11.0 / 4.0;\n"
            "const float CenterWeight = 35.0 / 64.0;\n"
            "const float LobeWeight = -3.0 / 64.0;\n"
            "varying vec4 v_texcoords[2];\n");
        vertex_header.append(
            "uniform vec2 scaling_vector;\n"
            "uniform vec2 src_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = src_subrect.zw * scaling_vector / src_pixelsize;\n"
            "  v_texcoords[0].xy = texcoord - LobeDist * step;\n"
            "  v_texcoords[0].zw = texcoord - CenterDist * step;\n"
            "  v_texcoords[1].xy = texcoord + CenterDist * step;\n"
            "  v_texcoords[1].zw = texcoord + LobeDist * step;\n");
        fragment_program.append(
            "  gl_FragColor = \n"
            // Lobe pixels
            "      (texture2D(s_texture, v_texcoords[0].xy) +\n"
            "       texture2D(s_texture, v_texcoords[1].zw)) *\n"
            "          LobeWeight +\n"
            // Center pixels
            "      (texture2D(s_texture, v_texcoords[0].zw) +\n"
            "       texture2D(s_texture, v_texcoords[1].xy)) *\n"
            "          CenterWeight;\n");
        break;

      case SHADER_BICUBIC_UPSCALE:
        // When scaling up, we need 4 texture reads, but we can
        // save some instructions because will know in which range of
        // the bicubic function each call call to the bicubic function
        // will be in.
        // Also, when sampling the bicubic function like this, the sum
        // is always exactly one, so we can skip normalization as well.
        shared_variables.append("varying vec2 v_texcoord;\n");
        vertex_program.append("  v_texcoord = texcoord;\n");
        fragment_header.append(
            "uniform vec2 src_pixelsize;\n"
            "uniform vec2 scaling_vector;\n"
            "const float a = -0.5;\n"
            // This function is equivialent to calling the bicubic
            // function with x-1, x, 1-x and 2-x
            // (assuming 0 <= x < 1)
            "vec4 filt4(float x) {\n"
            "  return vec4(x * x * x, x * x, x, 1) *\n"
            "         mat4(       a,      -2.0 * a,   a, 0.0,\n"
            "               a + 2.0,      -a - 3.0, 0.0, 1.0,\n"
            "              -a - 2.0, 3.0 + 2.0 * a,  -a, 0.0,\n"
            "                    -a,             a, 0.0, 0.0);\n"
            "}\n"
            "mat4 pixels_x(vec2 pos, vec2 step) {\n"
            "  return mat4(\n"
            "      texture2D(s_texture, pos - step),\n"
            "      texture2D(s_texture, pos),\n"
            "      texture2D(s_texture, pos + step),\n"
            "      texture2D(s_texture, pos + step * 2.0));\n"
            "}\n");
        fragment_program.append(
            "  vec2 pixel_pos = v_texcoord * src_pixelsize - \n"
            "      scaling_vector / 2.0;\n"
            "  float frac = fract(dot(pixel_pos, scaling_vector));\n"
            "  vec2 base = (floor(pixel_pos) + vec2(0.5)) / src_pixelsize;\n"
            "  vec2 step = scaling_vector / src_pixelsize;\n"
            "  gl_FragColor = pixels_x(base, step) * filt4(frac);\n");
        break;

      case SHADER_PLANAR:
        // Converts four RGBA pixels into one pixel. Each RGBA
        // pixel will be dot-multiplied with the color weights and
        // then placed into a component of the output. This is used to
        // convert RGBA textures into Y, U and V textures. We do this
        // because single-component textures are not renderable on all
        // architectures.
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append(
            "uniform vec2 scaling_vector;\n"
            "uniform vec2 dst_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector * src_subrect.zw / dst_pixelsize;\n"
            "  step /= 4.0;\n"
            "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
            "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
            "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
            "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
        fragment_header.append("uniform vec4 color_weights;\n");
        fragment_program.append(
            "  gl_FragColor = color_weights * mat4(\n"
            "    vec4(texture2D(s_texture, v_texcoords[0].xy).rgb, 1.0),\n"
            "    vec4(texture2D(s_texture, v_texcoords[0].zw).rgb, 1.0),\n"
            "    vec4(texture2D(s_texture, v_texcoords[1].xy).rgb, 1.0),\n"
            "    vec4(texture2D(s_texture, v_texcoords[1].zw).rgb, 1.0));\n");
        break;

      case SHADER_YUV_MRT_PASS1:
        // RGB24 to YV12 in two passes; writing two 8888 targets each pass.
        //
        // YV12 is full-resolution luma and half-resolution blue/red chroma.
        //
        //                  (original)
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
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
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append(
            "uniform vec2 scaling_vector;\n"
            "uniform vec2 dst_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector * src_subrect.zw / dst_pixelsize;\n"
            "  step /= 4.0;\n"
            "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
            "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
            "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
            "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
        fragment_directives.append("#extension GL_EXT_draw_buffers : enable\n");
        fragment_header.append(
            "const vec3 kRGBtoY = vec3(0.257, 0.504, 0.098);\n"
            "const float kYBias = 0.0625;\n"
            // Divide U and V by two to compensate for averaging below.
            "const vec3 kRGBtoU = vec3(-0.148, -0.291, 0.439) / 2.0;\n"
            "const vec3 kRGBtoV = vec3(0.439, -0.368, -0.071) / 2.0;\n"
            "const float kUVBias = 0.5;\n");
        fragment_program.append(
            "  vec3 pixel1 = texture2D(s_texture, v_texcoords[0].xy).rgb;\n"
            "  vec3 pixel2 = texture2D(s_texture, v_texcoords[0].zw).rgb;\n"
            "  vec3 pixel3 = texture2D(s_texture, v_texcoords[1].xy).rgb;\n"
            "  vec3 pixel4 = texture2D(s_texture, v_texcoords[1].zw).rgb;\n"
            "  vec3 pixel12 = pixel1 + pixel2;\n"
            "  vec3 pixel34 = pixel3 + pixel4;\n"
            "  gl_FragData[0] = vec4(dot(pixel1, kRGBtoY),\n"
            "                        dot(pixel2, kRGBtoY),\n"
            "                        dot(pixel3, kRGBtoY),\n"
            "                        dot(pixel4, kRGBtoY)) + kYBias;\n"
            "  gl_FragData[1] = vec4(dot(pixel12, kRGBtoU),\n"
            "                        dot(pixel34, kRGBtoU),\n"
            "                        dot(pixel12, kRGBtoV),\n"
            "                        dot(pixel34, kRGBtoV)) + kUVBias;\n");
        break;

      case SHADER_YUV_MRT_PASS2:
        // We're just sampling two pixels and unswizzling them.  There's
        // no need to do vertical scaling with math, since bilinear
        // interpolation in the sampler takes care of that.
        shared_variables.append("varying vec4 v_texcoords;\n");
        vertex_header.append(
            "uniform vec2 scaling_vector;\n"
            "uniform vec2 dst_pixelsize;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector * src_subrect.zw / dst_pixelsize;\n"
            "  step /= 2.0;\n"
            "  v_texcoords.xy = texcoord - step * 0.5;\n"
            "  v_texcoords.zw = texcoord + step * 0.5;\n");
        fragment_directives.append("#extension GL_EXT_draw_buffers : enable\n");
        fragment_program.append(
            "  vec4 lo_uuvv = texture2D(s_texture, v_texcoords.xy);\n"
            "  vec4 hi_uuvv = texture2D(s_texture, v_texcoords.zw);\n"
            "  gl_FragData[0] = vec4(lo_uuvv.rg, hi_uuvv.rg);\n"
            "  gl_FragData[1] = vec4(lo_uuvv.ba, hi_uuvv.ba);\n");
        break;
    }
    if (swizzle) {
      switch (type) {
        case SHADER_YUV_MRT_PASS1:
          fragment_program.append("  gl_FragData[0] = gl_FragData[0].bgra;\n");
          break;
        case SHADER_YUV_MRT_PASS2:
          fragment_program.append("  gl_FragData[0] = gl_FragData[0].bgra;\n");
          fragment_program.append("  gl_FragData[1] = gl_FragData[1].bgra;\n");
          break;
        default:
          fragment_program.append("  gl_FragColor = gl_FragColor.bgra;\n");
          break;
      }
    }

    vertex_program = vertex_header + shared_variables + "void main() {\n" +
                     vertex_program + "}\n";

    fragment_program = fragment_directives + fragment_header +
                       shared_variables + "void main() {\n" + fragment_program +
                       "}\n";

    cache_entry->Setup(vertex_program.c_str(), fragment_program.c_str());
  }
  return cache_entry;
}

void ShaderProgram::Setup(const GLchar* vertex_shader_text,
                          const GLchar* fragment_shader_text) {
  // Shaders to map the source texture to |dst_texture_|.
  GLuint vertex_shader =
      helper_->CompileShaderFromSource(vertex_shader_text, GL_VERTEX_SHADER);
  if (vertex_shader == 0)
    return;

  gl_->AttachShader(program_, vertex_shader);
  gl_->DeleteShader(vertex_shader);

  GLuint fragment_shader = helper_->CompileShaderFromSource(
      fragment_shader_text, GL_FRAGMENT_SHADER);
  if (fragment_shader == 0)
    return;
  gl_->AttachShader(program_, fragment_shader);
  gl_->DeleteShader(fragment_shader);

  gl_->LinkProgram(program_);

  GLint link_status = 0;
  gl_->GetProgramiv(program_, GL_LINK_STATUS, &link_status);
  if (!link_status)
    return;

  position_location_ = gl_->GetAttribLocation(program_, "a_position");
  texcoord_location_ = gl_->GetAttribLocation(program_, "a_texcoord");
  texture_location_ = gl_->GetUniformLocation(program_, "s_texture");
  src_subrect_location_ = gl_->GetUniformLocation(program_, "src_subrect");
  src_pixelsize_location_ = gl_->GetUniformLocation(program_, "src_pixelsize");
  dst_pixelsize_location_ = gl_->GetUniformLocation(program_, "dst_pixelsize");
  scaling_vector_location_ =
      gl_->GetUniformLocation(program_, "scaling_vector");
  color_weights_location_ = gl_->GetUniformLocation(program_, "color_weights");
  // The only reason fetching these attribute locations should fail is
  // if the context was spontaneously lost (i.e., because the GPU
  // process crashed, perhaps deliberately for testing).
  DCHECK(Initialized() || gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
}

void ShaderProgram::UseProgram(const gfx::Size& src_size,
                               const gfx::Rect& src_subrect,
                               const gfx::Size& dst_size,
                               bool scale_x,
                               bool flip_y,
                               GLfloat color_weights[4]) {
  gl_->UseProgram(program_);

  // OpenGL defines the last parameter to VertexAttribPointer as type
  // "const GLvoid*" even though it is actually an offset into the buffer
  // object's data store and not a pointer to the client's address space.
  const void* offsets[2] = {0,
                            reinterpret_cast<const void*>(2 * sizeof(GLfloat))};

  gl_->VertexAttribPointer(position_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[0]);
  gl_->EnableVertexAttribArray(position_location_);

  gl_->VertexAttribPointer(texcoord_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[1]);
  gl_->EnableVertexAttribArray(texcoord_location_);

  gl_->Uniform1i(texture_location_, 0);

  // Convert |src_subrect| to texture coordinates.
  GLfloat src_subrect_texcoord[] = {
      static_cast<float>(src_subrect.x()) / src_size.width(),
      static_cast<float>(src_subrect.y()) / src_size.height(),
      static_cast<float>(src_subrect.width()) / src_size.width(),
      static_cast<float>(src_subrect.height()) / src_size.height(),
  };
  if (flip_y) {
    src_subrect_texcoord[1] += src_subrect_texcoord[3];
    src_subrect_texcoord[3] *= -1.0;
  }
  gl_->Uniform4fv(src_subrect_location_, 1, src_subrect_texcoord);

  gl_->Uniform2f(src_pixelsize_location_, src_size.width(), src_size.height());
  gl_->Uniform2f(dst_pixelsize_location_, static_cast<float>(dst_size.width()),
                 static_cast<float>(dst_size.height()));

  gl_->Uniform2f(scaling_vector_location_, scale_x ? 1.0 : 0.0,
                 scale_x ? 0.0 : 1.0);
  gl_->Uniform4fv(color_weights_location_, 1, color_weights);
}

}  // namespace display_compositor
