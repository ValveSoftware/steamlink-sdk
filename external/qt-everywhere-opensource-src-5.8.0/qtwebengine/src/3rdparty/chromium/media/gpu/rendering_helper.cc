// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/rendering_helper.h"

#include <string.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringize_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif

#if defined(ARCH_CPU_X86_FAMILY) && defined(USE_X11)
#include "ui/gl/gl_surface_glx.h"
#define GL_VARIANT_GLX 1
#else
#include "ui/gl/gl_surface_egl.h"
#define GL_VARIANT_EGL 1
#endif

#if defined(USE_OZONE)
#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/types/native_display_delegate.h"
#endif  // defined(OS_CHROMEOS)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#endif  // defined(USE_OZONE)

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

namespace media {
namespace {

void WaitForSwapAck(const base::Closure& callback, gfx::SwapResult result) {
  callback.Run();
}

}  // namespace

#if defined(USE_OZONE)

class DisplayConfiguratorObserver : public ui::DisplayConfigurator::Observer {
 public:
  explicit DisplayConfiguratorObserver(base::RunLoop* loop) : loop_(loop) {}
  ~DisplayConfiguratorObserver() override {}

 private:
  // ui::DisplayConfigurator::Observer overrides:
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) override {
    if (!loop_)
      return;
    loop_->Quit();
    loop_ = nullptr;
  }
  void OnDisplayModeChangeFailed(
      const ui::DisplayConfigurator::DisplayStateList& outputs,
      ui::MultipleDisplayState failed_new_state) override {
    LOG(FATAL) << "Could not configure display";
  }

  base::RunLoop* loop_;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfiguratorObserver);
};

class RenderingHelper::StubOzoneDelegate : public ui::PlatformWindowDelegate {
 public:
  StubOzoneDelegate() : accelerated_widget_(gfx::kNullAcceleratedWidget) {
    platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, gfx::Rect());
  }
  ~StubOzoneDelegate() override {}

  void OnBoundsChanged(const gfx::Rect& new_bounds) override {}

  void OnDamageRect(const gfx::Rect& damaged_region) override {}

  void DispatchEvent(ui::Event* event) override {}

  void OnCloseRequest() override {}
  void OnClosed() override {}

  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}

  void OnLostCapture() override{};

  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override {
    accelerated_widget_ = widget;
  }

  void OnAcceleratedWidgetDestroyed() override { NOTREACHED(); }

  void OnActivationChanged(bool active) override{};

  gfx::AcceleratedWidget accelerated_widget() const {
    return accelerated_widget_;
  }

  gfx::Size GetSize() { return platform_window_->GetBounds().size(); }

  ui::PlatformWindow* platform_window() const { return platform_window_.get(); }

 private:
  std::unique_ptr<ui::PlatformWindow> platform_window_;
  gfx::AcceleratedWidget accelerated_widget_;

  DISALLOW_COPY_AND_ASSIGN(StubOzoneDelegate);
};

#endif  // defined(USE_OZONE)

RenderingHelperParams::RenderingHelperParams()
    : rendering_fps(0), warm_up_iterations(0), render_as_thumbnails(false) {}

RenderingHelperParams::RenderingHelperParams(
    const RenderingHelperParams& other) = default;

RenderingHelperParams::~RenderingHelperParams() {}

VideoFrameTexture::VideoFrameTexture(uint32_t texture_target,
                                     uint32_t texture_id,
                                     const base::Closure& no_longer_needed_cb)
    : texture_target_(texture_target),
      texture_id_(texture_id),
      no_longer_needed_cb_(no_longer_needed_cb) {
  DCHECK(!no_longer_needed_cb_.is_null());
}

VideoFrameTexture::~VideoFrameTexture() {
  base::ResetAndReturn(&no_longer_needed_cb_).Run();
}

RenderingHelper::RenderedVideo::RenderedVideo()
    : is_flushing(false), frames_to_drop(0) {}

RenderingHelper::RenderedVideo::RenderedVideo(const RenderedVideo& other) =
    default;

RenderingHelper::RenderedVideo::~RenderedVideo() {}

// static
void RenderingHelper::InitializeOneOff(base::WaitableEvent* done) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
#if GL_VARIANT_GLX
  cmd_line->AppendSwitchASCII(switches::kUseGL,
                              gl::kGLImplementationDesktopName);
#else
  cmd_line->AppendSwitchASCII(switches::kUseGL, gl::kGLImplementationEGLName);
#endif

  if (!gl::init::InitializeGLOneOff())
    LOG(FATAL) << "Could not initialize GL";
  done->Signal();
}

RenderingHelper::RenderingHelper() : ignore_vsync_(false) {
  window_ = gfx::kNullAcceleratedWidget;
  Clear();
}

RenderingHelper::~RenderingHelper() {
  CHECK_EQ(videos_.size(), 0U) << "Must call UnInitialize before dtor.";
  Clear();
}

void RenderingHelper::Setup() {
#if defined(OS_WIN)
  window_ = CreateWindowEx(0,
                           L"Static",
                           L"VideoDecodeAcceleratorTest",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           0, 0,
                           GetSystemMetrics(SM_CXSCREEN),
                           GetSystemMetrics(SM_CYSCREEN),
                           NULL, NULL, NULL, NULL);
#elif defined(USE_X11)
  Display* display = gfx::GetXDisplay();
  Screen* screen = DefaultScreenOfDisplay(display);

  CHECK(display);

  XSetWindowAttributes window_attributes;
  memset(&window_attributes, 0, sizeof(window_attributes));
  window_attributes.background_pixel =
      BlackPixel(display, DefaultScreen(display));
  window_attributes.override_redirect = true;
  int depth = DefaultDepth(display, DefaultScreen(display));

  window_ = XCreateWindow(display,
                          DefaultRootWindow(display),
                          0, 0,
                          XWidthOfScreen(screen),
                          XHeightOfScreen(screen),
                          0 /* border width */,
                          depth,
                          CopyFromParent /* class */,
                          CopyFromParent /* visual */,
                          (CWBackPixel | CWOverrideRedirect),
                          &window_attributes);
  XStoreName(display, window_, "VideoDecodeAcceleratorTest");
  XSelectInput(display, window_, ExposureMask);
  XMapWindow(display, window_);
#elif defined(USE_OZONE)
  base::MessageLoop::ScopedNestableTaskAllower nest_loop(
      base::MessageLoop::current());
  base::RunLoop wait_window_resize;

  platform_window_delegate_.reset(new RenderingHelper::StubOzoneDelegate());
  window_ = platform_window_delegate_->accelerated_widget();
  gfx::Size window_size(800, 600);
  // Ignore the vsync provider by default. On ChromeOS this will be set
  // accordingly based on the display configuration.
  ignore_vsync_ = true;
#if defined(OS_CHROMEOS)
  // We hold onto the main loop here to wait for the DisplayController
  // to give us the size of the display so we can create a window of
  // the same size.
  base::RunLoop wait_display_setup;
  DisplayConfiguratorObserver display_setup_observer(&wait_display_setup);
  display_configurator_.reset(new ui::DisplayConfigurator());
  display_configurator_->SetDelegateForTesting(0);
  display_configurator_->AddObserver(&display_setup_observer);
  display_configurator_->Init(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(), true);
  display_configurator_->ForceInitialConfigure(0);
  // Make sure all the display configuration is applied.
  wait_display_setup.Run();
  display_configurator_->RemoveObserver(&display_setup_observer);

  gfx::Size framebuffer_size = display_configurator_->framebuffer_size();
  if (!framebuffer_size.IsEmpty()) {
    window_size = framebuffer_size;
    ignore_vsync_ = false;
  }
#endif
  if (ignore_vsync_)
    DVLOG(1) << "Ignoring vsync provider";

  platform_window_delegate_->platform_window()->SetBounds(
      gfx::Rect(window_size));

  // On Ozone/DRI, platform windows are associated with the physical
  // outputs. Association is achieved by matching the bounds of the
  // window with the origin & modeset of the display output. Until a
  // window is associated with a display output, we cannot get vsync
  // events, because there is no hardware to get events from. Here we
  // wait for the window to resized and therefore associated with
  // display output to be sure that we will get such events.
  wait_window_resize.RunUntilIdle();
#else
#error unknown platform
#endif
  CHECK(window_ != gfx::kNullAcceleratedWidget);
}

void RenderingHelper::TearDown() {
#if defined(OS_WIN)
  if (window_)
    DestroyWindow(window_);
#elif defined(USE_X11)
  // Destroy resources acquired in Initialize, in reverse-acquisition order.
  if (window_) {
    CHECK(XUnmapWindow(gfx::GetXDisplay(), window_));
    CHECK(XDestroyWindow(gfx::GetXDisplay(), window_));
  }
#elif defined(USE_OZONE)
  platform_window_delegate_.reset();
#if defined(OS_CHROMEOS)
  display_configurator_->PrepareForExit();
  display_configurator_.reset();
#endif
#endif
  window_ = gfx::kNullAcceleratedWidget;
}

void RenderingHelper::Initialize(const RenderingHelperParams& params,
                                 base::WaitableEvent* done) {
  // Use videos_.size() != 0 as a proxy for the class having already been
  // Initialize()'d, and UnInitialize() before continuing.
  if (videos_.size()) {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    UnInitialize(&done);
    done.Wait();
  }

  render_task_.Reset(
      base::Bind(&RenderingHelper::RenderContent, base::Unretained(this)));

  frame_duration_ = params.rendering_fps > 0
                        ? base::TimeDelta::FromSeconds(1) / params.rendering_fps
                        : base::TimeDelta();

  render_as_thumbnails_ = params.render_as_thumbnails;
  message_loop_ = base::MessageLoop::current();

  gl_surface_ = gl::init::CreateViewGLSurface(window_);
#if defined(USE_OZONE)
  gl_surface_->Resize(platform_window_delegate_->GetSize(), 1.f, true);
#endif  // defined(USE_OZONE)
  screen_size_ = gl_surface_->GetSize();

  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::PreferIntegratedGpu);
  CHECK(gl_context_->MakeCurrent(gl_surface_.get()));

  CHECK_GT(params.window_sizes.size(), 0U);
  videos_.resize(params.window_sizes.size());
  LayoutRenderingAreas(params.window_sizes);

  if (render_as_thumbnails_) {
    CHECK_EQ(videos_.size(), 1U);

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
                 thumbnails_fbo_size_.width(), thumbnails_fbo_size_.height(),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_SHORT_5_6_5,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, thumbnails_texture_id_, 0);

    GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    CHECK(fb_status == GL_FRAMEBUFFER_COMPLETE) << fb_status;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebufferEXT(GL_FRAMEBUFFER,
                         gl_surface_->GetBackingFrameBufferObject());
  }

  // These vertices and texture coords. map (0,0) in the texture to the
  // bottom left of the viewport.  Since we get the video frames with the
  // the top left at (0,0) we need to flip the texture y coordinate
  // in the vertex shader for this to be rendered the right way up.
  // In the case of thumbnail rendering we use the same vertex shader
  // to render the FBO the screen, where we do not want this flipping.
  static const float kVertices[] = {
      -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f,
  };
  static const float kTextureCoords[] = {
      0, 1, 0, 0, 1, 1, 1, 0,
  };
  static const char kVertexShader[] =
      STRINGIZE(varying vec2 interp_tc; attribute vec4 in_pos;
                attribute vec2 in_tc; uniform bool tex_flip; void main() {
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
  static const char kFragmentShader[] =
      STRINGIZE(varying vec2 interp_tc;
                uniform sampler2D tex;
                void main() {
                  gl_FragColor = texture2D(tex, interp_tc);
                });
#endif
  program_ = glCreateProgram();
  CreateShader(program_, GL_VERTEX_SHADER, kVertexShader,
               arraysize(kVertexShader));
  CreateShader(program_, GL_FRAGMENT_SHADER, kFragmentShader,
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

  if (!frame_duration_.is_zero()) {
    int warm_up_iterations = params.warm_up_iterations;
#if defined(USE_OZONE)
    // On Ozone the VSyncProvider can't provide a vsync interval until
    // we render at least a frame, so we warm up with at least one
    // frame.
    // On top of this, we want to make sure all the buffers backing
    // the GL surface are cleared, otherwise we can see the previous
    // test's last frames, so we set warm up iterations to 2, to clear
    // the front and back buffers.
    warm_up_iterations = std::max(2, warm_up_iterations);
#endif
    WarmUpRendering(warm_up_iterations);
  }

  // It's safe to use Unretained here since |rendering_thread_| will be stopped
  // in VideoDecodeAcceleratorTest.TearDown(), while the |rendering_helper_| is
  // a member of that class. (See video_decode_accelerator_unittest.cc.)
  gfx::VSyncProvider* vsync_provider = gl_surface_->GetVSyncProvider();

  // VSync providers rely on the underlying CRTC to get the timing. In headless
  // mode the surface isn't associated with a CRTC so the vsync provider can not
  // get the timing, meaning it will not call UpdateVsyncParameters() ever.
  if (!ignore_vsync_ && vsync_provider && !frame_duration_.is_zero()) {
    vsync_provider->GetVSyncParameters(base::Bind(
        &RenderingHelper::UpdateVSyncParameters, base::Unretained(this), done));
  } else {
    done->Signal();
  }
}

// The rendering for the first few frames is slow (e.g., 100ms on Peach Pit).
// This affects the numbers measured in the performance test. We try to render
// several frames here to warm up the rendering.
void RenderingHelper::WarmUpRendering(int warm_up_iterations) {
  unsigned int texture_id;
  std::unique_ptr<GLubyte[]> emptyData(
      new GLubyte[screen_size_.GetArea() * 2]());
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGB,
               screen_size_.width(), screen_size_.height(),
               0,
               GL_RGB,
               GL_UNSIGNED_SHORT_5_6_5,
               emptyData.get());
  for (int i = 0; i < warm_up_iterations; ++i) {
    RenderTexture(GL_TEXTURE_2D, texture_id);

    // Need to allow nestable tasks since WarmUpRendering() is called from
    // within another task on the renderer thread.
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    base::RunLoop wait_for_swap_ack;
    if (gl_surface_->SupportsAsyncSwap()) {
      gl_surface_->SwapBuffersAsync(
          base::Bind(&WaitForSwapAck, wait_for_swap_ack.QuitClosure()));
      wait_for_swap_ack.Run();
    } else {
      gl_surface_->SwapBuffers();
    }
  }
  glDeleteTextures(1, &texture_id);
}

void RenderingHelper::UnInitialize(base::WaitableEvent* done) {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);

  render_task_.Cancel();

  if (render_as_thumbnails_) {
    glDeleteTextures(1, &thumbnails_texture_id_);
    glDeleteFramebuffersEXT(1, &thumbnails_fbo_id_);
  }

  gl_context_->ReleaseCurrent(gl_surface_.get());
  gl_context_ = NULL;
  gl_surface_ = NULL;

  Clear();
  done->Signal();
}

void RenderingHelper::CreateTexture(uint32_t texture_target,
                                    uint32_t* texture_id,
                                    const gfx::Size& size,
                                    base::WaitableEvent* done) {
  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RenderingHelper::CreateTexture, base::Unretained(this),
                   texture_target, texture_id, size, done));
    return;
  }
  glGenTextures(1, texture_id);
  glBindTexture(texture_target, *texture_id);
  if (texture_target == GL_TEXTURE_2D) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 size.width(), size.height(),
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

void RenderingHelper::RenderThumbnail(uint32_t texture_target,
                                      uint32_t texture_id) {
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
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFrameBufferObject());

  // Need to flush the GL commands before we return the tnumbnail texture to
  // the decoder.
  glFlush();
  ++frame_count_;
}

void RenderingHelper::QueueVideoFrame(
    size_t window_id,
    scoped_refptr<VideoFrameTexture> video_frame) {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);
  RenderedVideo* video = &videos_[window_id];
  DCHECK(!video->is_flushing);

  video->pending_frames.push(video_frame);

  if (video->frames_to_drop > 0 && video->pending_frames.size() > 1) {
    --video->frames_to_drop;
    video->pending_frames.pop();
  }

  // Schedules the first RenderContent() if need.
  if (scheduled_render_time_.is_null()) {
    scheduled_render_time_ = base::TimeTicks::Now();
    message_loop_->PostTask(FROM_HERE, render_task_.callback());
  }
}

void RenderingHelper::RenderTexture(uint32_t texture_target,
                                    uint32_t texture_id) {
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

void RenderingHelper::DeleteTexture(uint32_t texture_id) {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

gl::GLContext* RenderingHelper::GetGLContext() {
  return gl_context_.get();
}

void* RenderingHelper::GetGLDisplay() {
  return gl_surface_->GetDisplay();
}

void RenderingHelper::Clear() {
  videos_.clear();
  message_loop_ = NULL;
  gl_context_ = NULL;
  gl_surface_ = NULL;

  render_as_thumbnails_ = false;
  frame_count_ = 0;
  thumbnails_fbo_id_ = 0;
  thumbnails_texture_id_ = 0;
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
  glReadPixels(0, 0,
               thumbnails_fbo_size_.width(), thumbnails_fbo_size_.height(),
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &rgba[0]);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFrameBufferObject());
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

void RenderingHelper::Flush(size_t window_id) {
  videos_[window_id].is_flushing = true;
}

void RenderingHelper::RenderContent() {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);

  // Update the VSync params.
  //
  // It's safe to use Unretained here since |rendering_thread_| will be stopped
  // in VideoDecodeAcceleratorTest.TearDown(), while the |rendering_helper_| is
  // a member of that class. (See video_decode_accelerator_unittest.cc.)
  gfx::VSyncProvider* vsync_provider = gl_surface_->GetVSyncProvider();
  if (vsync_provider && !ignore_vsync_) {
    vsync_provider->GetVSyncParameters(base::Bind(
        &RenderingHelper::UpdateVSyncParameters, base::Unretained(this),
        static_cast<base::WaitableEvent*>(NULL)));
  }

  int tex_flip = 1;
#if defined(USE_OZONE)
  // Ozone surfaceless renders flipped from normal GL, so there's no need to
  // do an extra flip.
  tex_flip = 0;
#endif  // defined(USE_OZONE)
  glUniform1i(glGetUniformLocation(program_, "tex_flip"), tex_flip);

  // Frames that will be returned to the client (via the no_longer_needed_cb)
  // after this vector falls out of scope at the end of this method. We need
  // to keep references to them until after SwapBuffers() call below.
  std::vector<scoped_refptr<VideoFrameTexture>> frames_to_be_returned;
  bool need_swap_buffer = false;
  if (render_as_thumbnails_) {
    // In render_as_thumbnails_ mode, we render the FBO content on the
    // screen instead of the decoded textures.
    GLSetViewPort(videos_[0].render_area);
    RenderTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
    need_swap_buffer = true;
  } else {
    for (RenderedVideo& video : videos_) {
      if (video.pending_frames.empty())
        continue;
      need_swap_buffer = true;
      scoped_refptr<VideoFrameTexture> frame = video.pending_frames.front();
      GLSetViewPort(video.render_area);
      RenderTexture(frame->texture_target(), frame->texture_id());

      if (video.pending_frames.size() > 1 || video.is_flushing) {
        frames_to_be_returned.push_back(video.pending_frames.front());
        video.pending_frames.pop();
      } else {
        ++video.frames_to_drop;
      }
    }
  }

  base::Closure schedule_frame = base::Bind(
      &RenderingHelper::ScheduleNextRenderContent, base::Unretained(this));
  if (!need_swap_buffer) {
    schedule_frame.Run();
    return;
  }

  if (gl_surface_->SupportsAsyncSwap()) {
    gl_surface_->SwapBuffersAsync(base::Bind(&WaitForSwapAck, schedule_frame));
  } else {
    gl_surface_->SwapBuffers();
    ScheduleNextRenderContent();
  }
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

void RenderingHelper::LayoutRenderingAreas(
    const std::vector<gfx::Size>& window_sizes) {
  // Find the number of colums and rows.
  // The smallest n * n or n * (n + 1) > number of windows.
  size_t cols = sqrt(videos_.size() - 1) + 1;
  size_t rows = (videos_.size() + cols - 1) / cols;

  // Find the widths and heights of the grid.
  std::vector<int> widths(cols);
  std::vector<int> heights(rows);
  std::vector<int> offset_x(cols);
  std::vector<int> offset_y(rows);

  for (size_t i = 0; i < window_sizes.size(); ++i) {
    const gfx::Size& size = window_sizes[i];
    widths[i % cols] = std::max(widths[i % cols], size.width());
    heights[i / cols] = std::max(heights[i / cols], size.height());
  }

  ScaleAndCalculateOffsets(&widths, &offset_x, screen_size_.width());
  ScaleAndCalculateOffsets(&heights, &offset_y, screen_size_.height());

  // Put each render_area_ in the center of each cell.
  for (size_t i = 0; i < window_sizes.size(); ++i) {
    const gfx::Size& size = window_sizes[i];
    float scale =
        std::min(static_cast<float>(widths[i % cols]) / size.width(),
                 static_cast<float>(heights[i / cols]) / size.height());

    // Don't scale up the texture.
    scale = std::min(1.0f, scale);

    size_t w = scale * size.width();
    size_t h = scale * size.height();
    size_t x = offset_x[i % cols] + (widths[i % cols] - w) / 2;
    size_t y = offset_y[i / cols] + (heights[i / cols] - h) / 2;
    videos_[i].render_area = gfx::Rect(x, y, w, h);
  }
}

void RenderingHelper::UpdateVSyncParameters(base::WaitableEvent* done,
                                            const base::TimeTicks timebase,
                                            const base::TimeDelta interval) {
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;

  if (done)
    done->Signal();
}

void RenderingHelper::DropOneFrameForAllVideos() {
  for (RenderedVideo& video : videos_) {
    if (video.pending_frames.empty())
      continue;

    if (video.pending_frames.size() > 1 || video.is_flushing) {
      video.pending_frames.pop();
    } else {
      ++video.frames_to_drop;
    }
  }
}

void RenderingHelper::ScheduleNextRenderContent() {
  scheduled_render_time_ += frame_duration_;
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks target;

  if (vsync_interval_.is_zero()) {
    target = std::max(now, scheduled_render_time_);
  } else {
    // Schedules the next RenderContent() at latest VSYNC before the
    // |scheduled_render_time_|.
    target = std::max(now + vsync_interval_, scheduled_render_time_);

    int64_t intervals = (target - vsync_timebase_) / vsync_interval_;
    target = vsync_timebase_ + intervals * vsync_interval_;
  }

  // When the rendering falls behind, drops frames.
  while (scheduled_render_time_ < target) {
    scheduled_render_time_ += frame_duration_;
    DropOneFrameForAllVideos();
  }

  message_loop_->PostDelayedTask(FROM_HERE, render_task_.callback(),
                                 target - now);
}
}  // namespace media
