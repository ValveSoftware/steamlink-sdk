// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/rendering_helper.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "base/bind.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringize_macros.h"
#include "base/synchronization/waitable_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif

#ifdef GL_VARIANT_GLX
struct XFreeDeleter {
  void operator()(void* x) const { ::XFree(x); }
};
#endif

// Helper for Shader creation.
static void CreateShader(GLuint program,
                         GLenum type,
                         const char* source,
                         int size) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(shader, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glAttachShader(program, shader);
  glDeleteShader(shader);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

namespace content {

RenderingHelperParams::RenderingHelperParams() {}

RenderingHelperParams::~RenderingHelperParams() {}

static const gfx::GLImplementation kGLImplementation =
#if defined(GL_VARIANT_GLX)
    gfx::kGLImplementationDesktopGL;
#elif defined(GL_VARIANT_EGL)
    gfx::kGLImplementationEGLGLES2;
#else
    -1;
#error "Unknown GL implementation."
#endif

RenderingHelper::RenderingHelper() {
#if defined(GL_VARIANT_EGL)
  gl_surface_ = EGL_NO_SURFACE;
#endif

#if defined(OS_WIN)
  window_ = NULL;
#else
  x_window_ = (Window)0;
#endif

  Clear();
}

RenderingHelper::~RenderingHelper() {
  CHECK_EQ(clients_.size(), 0U) << "Must call UnInitialize before dtor.";
  Clear();
}

void RenderingHelper::Initialize(const RenderingHelperParams& params,
                                 base::WaitableEvent* done) {
  // Use cients_.size() != 0 as a proxy for the class having already been
  // Initialize()'d, and UnInitialize() before continuing.
  if (clients_.size()) {
    base::WaitableEvent done(false, false);
    UnInitialize(&done);
    done.Wait();
  }

  frame_duration_ = params.rendering_fps > 0
                        ? base::TimeDelta::FromSeconds(1) / params.rendering_fps
                        : base::TimeDelta();

  gfx::InitializeStaticGLBindings(kGLImplementation);
  scoped_refptr<gfx::GLContextStubWithExtensions> stub_context(
      new gfx::GLContextStubWithExtensions());

  render_as_thumbnails_ = params.render_as_thumbnails;
  message_loop_ = base::MessageLoop::current();

#if GL_VARIANT_GLX
  x_display_ = gfx::GetXDisplay();
  CHECK(x_display_);
  CHECK(glXQueryVersion(x_display_, NULL, NULL));
  const int fbconfig_attr[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
    GLX_BIND_TO_TEXTURE_RGB_EXT, GL_TRUE,
    GLX_DOUBLEBUFFER, True,
    GL_NONE,
  };
  int num_fbconfigs;
  scoped_ptr<GLXFBConfig, XFreeDeleter> glx_fb_configs(
      glXChooseFBConfig(x_display_, DefaultScreen(x_display_), fbconfig_attr,
                        &num_fbconfigs));
  CHECK(glx_fb_configs.get());
  CHECK_GT(num_fbconfigs, 0);
  x_visual_ = glXGetVisualFromFBConfig(x_display_, glx_fb_configs.get()[0]);
  CHECK(x_visual_);
  gl_context_ = glXCreateContext(x_display_, x_visual_, 0, true);
  CHECK(gl_context_);
  stub_context->AddExtensionsString(
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
  stub_context->SetGLVersionString(
      reinterpret_cast<const char*>(glGetString(GL_VERSION)));

  Screen* screen = DefaultScreenOfDisplay(x_display_);
  screen_size_ = gfx::Size(XWidthOfScreen(screen), XHeightOfScreen(screen));
#else // EGL
  EGLNativeDisplayType native_display;

#if defined(OS_WIN)
  native_display = EGL_DEFAULT_DISPLAY;
  screen_size_ =
      gfx::Size(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
#else
  x_display_ = gfx::GetXDisplay();
  CHECK(x_display_);
  native_display = x_display_;

  Screen* screen = DefaultScreenOfDisplay(x_display_);
  screen_size_ = gfx::Size(XWidthOfScreen(screen), XHeightOfScreen(screen));
#endif
  gl_display_ = eglGetDisplay(native_display);
  CHECK(gl_display_);
  CHECK(eglInitialize(gl_display_, NULL, NULL)) << eglGetError();

  static EGLint rgba8888[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE,
  };
  EGLConfig egl_config;
  int num_configs;
  CHECK(eglChooseConfig(gl_display_, rgba8888, &egl_config, 1, &num_configs))
      << eglGetError();
  CHECK_GE(num_configs, 1);
  static EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  gl_context_ = eglCreateContext(
      gl_display_, egl_config, EGL_NO_CONTEXT, context_attribs);
  CHECK_NE(gl_context_, EGL_NO_CONTEXT) << eglGetError();
  stub_context->AddExtensionsString(
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
  stub_context->AddExtensionsString(
      eglQueryString(gl_display_, EGL_EXTENSIONS));
  stub_context->SetGLVersionString(
      reinterpret_cast<const char*>(glGetString(GL_VERSION)));
#endif
  clients_ = params.clients;
  CHECK_GT(clients_.size(), 0U);
  LayoutRenderingAreas();

#if defined(OS_WIN)
  window_ = CreateWindowEx(0,
                           L"Static",
                           L"VideoDecodeAcceleratorTest",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           0,
                           0,
                           screen_size_.width(),
                           screen_size_.height(),
                           NULL,
                           NULL,
                           NULL,
                           NULL);
  CHECK(window_ != NULL);
#else
  int depth = DefaultDepth(x_display_, DefaultScreen(x_display_));

#if defined(GL_VARIANT_GLX)
  CHECK_EQ(depth, x_visual_->depth);
#endif

  XSetWindowAttributes window_attributes;
  window_attributes.background_pixel =
      BlackPixel(x_display_, DefaultScreen(x_display_));
  window_attributes.override_redirect = true;

  x_window_ = XCreateWindow(x_display_,
                            DefaultRootWindow(x_display_),
                            0,
                            0,
                            screen_size_.width(),
                            screen_size_.height(),
                            0 /* border width */,
                            depth,
                            CopyFromParent /* class */,
                            CopyFromParent /* visual */,
                            (CWBackPixel | CWOverrideRedirect),
                            &window_attributes);
  XStoreName(x_display_, x_window_, "VideoDecodeAcceleratorTest");
  XSelectInput(x_display_, x_window_, ExposureMask);
  XMapWindow(x_display_, x_window_);
#endif

#if GL_VARIANT_EGL
#if defined(OS_WIN)
  gl_surface_ =
      eglCreateWindowSurface(gl_display_, egl_config, window_, NULL);
#else
  gl_surface_ =
      eglCreateWindowSurface(gl_display_, egl_config, x_window_, NULL);
#endif
  CHECK_NE(gl_surface_, EGL_NO_SURFACE);
#endif

#if GL_VARIANT_GLX
  CHECK(glXMakeContextCurrent(x_display_, x_window_, x_window_, gl_context_));
#else  // EGL
  CHECK(eglMakeCurrent(gl_display_, gl_surface_, gl_surface_, gl_context_))
      << eglGetError();
#endif

  // Must be done after a context is made current.
  gfx::InitializeDynamicGLBindings(kGLImplementation, stub_context.get());

  if (render_as_thumbnails_) {
    CHECK_EQ(clients_.size(), 1U);

    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    CHECK_GE(max_texture_size, params.thumbnails_page_size.width());
    CHECK_GE(max_texture_size, params.thumbnails_page_size.height());

    thumbnails_fbo_size_ = params.thumbnails_page_size;
    thumbnail_size_ = params.thumbnail_size;

    glGenFramebuffersEXT(1, &thumbnails_fbo_id_);
    glGenTextures(1, &thumbnails_texture_id_);
    glBindTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 thumbnails_fbo_size_.width(),
                 thumbnails_fbo_size_.height(),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_SHORT_5_6_5,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              thumbnails_texture_id_,
                              0);

    GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    CHECK(fb_status == GL_FRAMEBUFFER_COMPLETE) << fb_status;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  }

  // These vertices and texture coords. map (0,0) in the texture to the
  // bottom left of the viewport.  Since we get the video frames with the
  // the top left at (0,0) we need to flip the texture y coordinate
  // in the vertex shader for this to be rendered the right way up.
  // In the case of thumbnail rendering we use the same vertex shader
  // to render the FBO the screen, where we do not want this flipping.
  static const float kVertices[] =
      { -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f, };
  static const float kTextureCoords[] = { 0, 1, 0, 0, 1, 1, 1, 0, };
  static const char kVertexShader[] = STRINGIZE(
      varying vec2 interp_tc;
      attribute vec4 in_pos;
      attribute vec2 in_tc;
      uniform bool tex_flip;
      void main() {
        if (tex_flip)
          interp_tc = vec2(in_tc.x, 1.0 - in_tc.y);
        else
          interp_tc = in_tc;
        gl_Position = in_pos;
      });

#if GL_VARIANT_EGL
  static const char kFragmentShader[] =
      "#extension GL_OES_EGL_image_external : enable\n"
      "precision mediump float;\n"
      "varying vec2 interp_tc;\n"
      "uniform sampler2D tex;\n"
      "#ifdef GL_OES_EGL_image_external\n"
      "uniform samplerExternalOES tex_external;\n"
      "#endif\n"
      "void main() {\n"
      "  vec4 color = texture2D(tex, interp_tc);\n"
      "#ifdef GL_OES_EGL_image_external\n"
      "  color += texture2D(tex_external, interp_tc);\n"
      "#endif\n"
      "  gl_FragColor = color;\n"
      "}\n";
#else
  static const char kFragmentShader[] = STRINGIZE(
      varying vec2 interp_tc;
      uniform sampler2D tex;
      void main() {
        gl_FragColor = texture2D(tex, interp_tc);
      });
#endif
  program_ = glCreateProgram();
  CreateShader(
      program_, GL_VERTEX_SHADER, kVertexShader, arraysize(kVertexShader));
  CreateShader(program_,
               GL_FRAGMENT_SHADER,
               kFragmentShader,
               arraysize(kFragmentShader));
  glLinkProgram(program_);
  int result = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(program_, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glUseProgram(program_);
  glDeleteProgram(program_);

  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  glUniform1i(glGetUniformLocation(program_, "tex"), 0);
  GLint tex_external = glGetUniformLocation(program_, "tex_external");
  if (tex_external != -1) {
    glUniform1i(tex_external, 1);
  }
  int pos_location = glGetAttribLocation(program_, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
  int tc_location = glGetAttribLocation(program_, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoords);

  if (frame_duration_ != base::TimeDelta()) {
    render_timer_.Start(
        FROM_HERE, frame_duration_, this, &RenderingHelper::RenderContent);
  }
  done->Signal();
}

void RenderingHelper::UnInitialize(base::WaitableEvent* done) {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);
  render_timer_.Stop();
  if (render_as_thumbnails_) {
    glDeleteTextures(1, &thumbnails_texture_id_);
    glDeleteFramebuffersEXT(1, &thumbnails_fbo_id_);
  }
#if GL_VARIANT_GLX

  glXDestroyContext(x_display_, gl_context_);
#else // EGL
  CHECK(eglMakeCurrent(
      gl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
      << eglGetError();
  CHECK(eglDestroyContext(gl_display_, gl_context_));
  CHECK(eglDestroySurface(gl_display_, gl_surface_));
  CHECK(eglTerminate(gl_display_));
#endif
  gfx::ClearGLBindings();
  Clear();
  done->Signal();
}

void RenderingHelper::CreateTexture(uint32 texture_target,
                                    uint32* texture_id,
                                    const gfx::Size& size,
                                    base::WaitableEvent* done) {
  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RenderingHelper::CreateTexture,
                                       base::Unretained(this),
                                       texture_target,
                                       texture_id,
                                       size,
                                       done));
    return;
  }
  glGenTextures(1, texture_id);
  glBindTexture(texture_target, *texture_id);
  if (texture_target == GL_TEXTURE_2D) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 size.width(),
                 size.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);
  }
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // OpenGLES2.0.25 section 3.8.2 requires CLAMP_TO_EDGE for NPOT textures.
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  done->Signal();
}

// Helper function to set GL viewport.
static inline void GLSetViewPort(const gfx::Rect& area) {
  glViewport(area.x(), area.y(), area.width(), area.height());
  glScissor(area.x(), area.y(), area.width(), area.height());
}

void RenderingHelper::RenderThumbnail(uint32 texture_target,
                                      uint32 texture_id) {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);
  const int width = thumbnail_size_.width();
  const int height = thumbnail_size_.height();
  const int thumbnails_in_row = thumbnails_fbo_size_.width() / width;
  const int thumbnails_in_column = thumbnails_fbo_size_.height() / height;
  const int row = (frame_count_ / thumbnails_in_row) % thumbnails_in_column;
  const int col = frame_count_ % thumbnails_in_row;

  gfx::Rect area(col * width, row * height, width, height);

  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  GLSetViewPort(area);
  RenderTexture(texture_target, texture_id);
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  ++frame_count_;
}

void RenderingHelper::RenderTexture(uint32 texture_target, uint32 texture_id) {
  // The ExternalOES sampler is bound to GL_TEXTURE1 and the Texture2D sampler
  // is bound to GL_TEXTURE0.
  if (texture_target == GL_TEXTURE_2D) {
    glActiveTexture(GL_TEXTURE0 + 0);
  } else if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    glActiveTexture(GL_TEXTURE0 + 1);
  }
  glBindTexture(texture_target, texture_id);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindTexture(texture_target, 0);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

void RenderingHelper::DeleteTexture(uint32 texture_id) {
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

NativeContextType RenderingHelper::GetGLContext() { return gl_context_; }

void* RenderingHelper::GetGLDisplay() {
#if GL_VARIANT_GLX
  return x_display_;
#else  // EGL
  return gl_display_;
#endif
}

void RenderingHelper::Clear() {
  clients_.clear();
  message_loop_ = NULL;
  gl_context_ = NULL;
#if GL_VARIANT_EGL
  gl_display_ = EGL_NO_DISPLAY;
  gl_surface_ = EGL_NO_SURFACE;
#endif
  render_as_thumbnails_ = false;
  frame_count_ = 0;
  thumbnails_fbo_id_ = 0;
  thumbnails_texture_id_ = 0;

#if defined(OS_WIN)
  if (window_) {
    DestroyWindow(window_);
    window_ = NULL;
  }
#else
  // Destroy resources acquired in Initialize, in reverse-acquisition order.
  if (x_window_) {
    CHECK(XUnmapWindow(x_display_, x_window_));
    CHECK(XDestroyWindow(x_display_, x_window_));
    x_window_ = (Window)0;
  }
  // Mimic newly created object.
  x_display_ = NULL;
#endif
}

void RenderingHelper::GetThumbnailsAsRGB(std::vector<unsigned char>* rgb,
                                         bool* alpha_solid,
                                         base::WaitableEvent* done) {
  CHECK(render_as_thumbnails_);

  const size_t num_pixels = thumbnails_fbo_size_.GetArea();
  std::vector<unsigned char> rgba;
  rgba.resize(num_pixels * 4);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  // We can only count on GL_RGBA/GL_UNSIGNED_BYTE support.
  glReadPixels(0,
               0,
               thumbnails_fbo_size_.width(),
               thumbnails_fbo_size_.height(),
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &rgba[0]);
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  rgb->resize(num_pixels * 3);
  // Drop the alpha channel, but check as we go that it is all 0xff.
  bool solid = true;
  unsigned char* rgb_ptr = &((*rgb)[0]);
  unsigned char* rgba_ptr = &rgba[0];
  for (size_t i = 0; i < num_pixels; ++i) {
    *rgb_ptr++ = *rgba_ptr++;
    *rgb_ptr++ = *rgba_ptr++;
    *rgb_ptr++ = *rgba_ptr++;
    solid = solid && (*rgba_ptr == 0xff);
    rgba_ptr++;
  }
  *alpha_solid = solid;

  done->Signal();
}

void RenderingHelper::RenderContent() {
  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 1);

  if (render_as_thumbnails_) {
    // In render_as_thumbnails_ mode, we render the FBO content on the
    // screen instead of the decoded textures.
    GLSetViewPort(render_areas_[0]);
    RenderTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
  } else {
    for (size_t i = 0; i < clients_.size(); ++i) {
      if (clients_[i]) {
        GLSetViewPort(render_areas_[i]);
        clients_[i]->RenderContent(this);
      }
    }
  }

#if GL_VARIANT_GLX
  glXSwapBuffers(x_display_, x_window_);
#else  // EGL
  eglSwapBuffers(gl_display_, gl_surface_);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
#endif
}

// Helper function for the LayoutRenderingAreas(). The |lengths| are the
// heights(widths) of the rows(columns). It scales the elements in
// |lengths| proportionally so that the sum of them equal to |total_length|.
// It also outputs the coordinates of the rows(columns) to |offsets|.
static void ScaleAndCalculateOffsets(std::vector<int>* lengths,
                                     std::vector<int>* offsets,
                                     int total_length) {
  int sum = std::accumulate(lengths->begin(), lengths->end(), 0);
  for (size_t i = 0; i < lengths->size(); ++i) {
    lengths->at(i) = lengths->at(i) * total_length / sum;
    offsets->at(i) = (i == 0) ? 0 : offsets->at(i - 1) + lengths->at(i - 1);
  }
}

void RenderingHelper::LayoutRenderingAreas() {
  // Find the number of colums and rows.
  // The smallest n * n or n * (n + 1) > number of clients.
  size_t cols = sqrt(clients_.size() - 1) + 1;
  size_t rows = (clients_.size() + cols - 1) / cols;

  // Find the widths and heights of the grid.
  std::vector<int> widths(cols);
  std::vector<int> heights(rows);
  std::vector<int> offset_x(cols);
  std::vector<int> offset_y(rows);

  for (size_t i = 0; i < clients_.size(); ++i) {
    const gfx::Size& window_size = clients_[i]->GetWindowSize();
    widths[i % cols] = std::max(widths[i % cols], window_size.width());
    heights[i / cols] = std::max(heights[i / cols], window_size.height());
  }

  ScaleAndCalculateOffsets(&widths, &offset_x, screen_size_.width());
  ScaleAndCalculateOffsets(&heights, &offset_y, screen_size_.height());

  // Put each render_area_ in the center of each cell.
  render_areas_.clear();
  for (size_t i = 0; i < clients_.size(); ++i) {
    const gfx::Size& window_size = clients_[i]->GetWindowSize();
    float scale =
        std::min(static_cast<float>(widths[i % cols]) / window_size.width(),
                 static_cast<float>(heights[i / cols]) / window_size.height());

    // Don't scale up the texture.
    scale = std::min(1.0f, scale);

    size_t w = scale * window_size.width();
    size_t h = scale * window_size.height();
    size_t x = offset_x[i % cols] + (widths[i % cols] - w) / 2;
    size_t y = offset_y[i / cols] + (heights[i / cols] - h) / 2;
    render_areas_.push_back(gfx::Rect(x, y, w, h));
  }
}
}  // namespace content
