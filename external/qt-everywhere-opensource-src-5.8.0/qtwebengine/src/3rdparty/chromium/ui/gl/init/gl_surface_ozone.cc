// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_surface_ozone.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/worker_pool.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_overlay.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace gl {

namespace {

// Helper function for base::Bind to create callback to eglChooseConfig.
bool EglChooseConfig(EGLDisplay display,
                     const int32_t* attribs,
                     EGLConfig* configs,
                     int32_t config_size,
                     int32_t* num_configs) {
  return eglChooseConfig(display, attribs, configs, config_size, num_configs);
}

// Helper function for base::Bind to create callback to eglGetConfigAttrib.
bool EglGetConfigAttribute(EGLDisplay display,
                           EGLConfig config,
                           int32_t attribute,
                           int32_t* value) {
  return eglGetConfigAttrib(display, config, attribute, value);
}

// Populates EglConfigCallbacks with appropriate callbacks.
ui::EglConfigCallbacks GetEglConfigCallbacks(EGLDisplay display) {
  ui::EglConfigCallbacks callbacks;
  callbacks.choose_config = base::Bind(EglChooseConfig, display);
  callbacks.get_config_attribute = base::Bind(EglGetConfigAttribute, display);
  callbacks.get_last_error_string = base::Bind(&ui::GetLastEGLErrorString);
  return callbacks;
}

void WaitForFence(EGLDisplay display, EGLSyncKHR fence) {
  eglClientWaitSyncKHR(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                       EGL_FOREVER_KHR);
}

// A thin wrapper around GLSurfaceEGL that owns the EGLNativeWindow.
class GL_EXPORT GLSurfaceOzoneEGL : public NativeViewGLSurfaceEGL {
 public:
  GLSurfaceOzoneEGL(std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
                    gfx::AcceleratedWidget widget);

  // GLSurface:
  bool Initialize(GLSurface::Format format) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers() override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  EGLConfig GetConfig() override;

 private:
  using NativeViewGLSurfaceEGL::Initialize;

  ~GLSurfaceOzoneEGL() override;

  bool ReinitializeNativeSurface();

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneEGL);
};

GLSurfaceOzoneEGL::GLSurfaceOzoneEGL(
    std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
    gfx::AcceleratedWidget widget)
    : NativeViewGLSurfaceEGL(ozone_surface->GetNativeWindow()),
      ozone_surface_(std::move(ozone_surface)),
      widget_(widget) {}

bool GLSurfaceOzoneEGL::Initialize(GLSurface::Format format) {
  format_ = format;
  return Initialize(ozone_surface_->CreateVSyncProvider());
}

bool GLSurfaceOzoneEGL::Resize(const gfx::Size& size,
                               float scale_factor,
                               bool has_alpha) {
  if (!ozone_surface_->ResizeNativeWindow(size)) {
    if (!ReinitializeNativeSurface() ||
        !ozone_surface_->ResizeNativeWindow(size))
      return false;
  }

  return NativeViewGLSurfaceEGL::Resize(size, scale_factor, has_alpha);
}

gfx::SwapResult GLSurfaceOzoneEGL::SwapBuffers() {
  gfx::SwapResult result = NativeViewGLSurfaceEGL::SwapBuffers();
  if (result != gfx::SwapResult::SWAP_ACK)
    return result;

  return ozone_surface_->OnSwapBuffers() ? gfx::SwapResult::SWAP_ACK
                                         : gfx::SwapResult::SWAP_FAILED;
}

bool GLSurfaceOzoneEGL::ScheduleOverlayPlane(int z_order,
                                             gfx::OverlayTransform transform,
                                             GLImage* image,
                                             const gfx::Rect& bounds_rect,
                                             const gfx::RectF& crop_rect) {
  return image->ScheduleOverlayPlane(widget_, z_order, transform, bounds_rect,
                                     crop_rect);
}

EGLConfig GLSurfaceOzoneEGL::GetConfig() {
  if (!config_) {
    ui::EglConfigCallbacks callbacks = GetEglConfigCallbacks(GetDisplay());
    config_ = ozone_surface_->GetEGLSurfaceConfig(callbacks);
  }
  if (config_)
    return config_;
  return NativeViewGLSurfaceEGL::GetConfig();
}

GLSurfaceOzoneEGL::~GLSurfaceOzoneEGL() {
  Destroy();  // The EGL surface must be destroyed before SurfaceOzone.
}

bool GLSurfaceOzoneEGL::ReinitializeNativeSurface() {
  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  GLContext* current_context = GLContext::GetCurrent();
  bool was_current = current_context && current_context->IsCurrent(this);
  if (was_current) {
    scoped_make_current.reset(new ui::ScopedMakeCurrent(current_context, this));
  }

  Destroy();
  ozone_surface_ = ui::OzonePlatform::GetInstance()
                       ->GetSurfaceFactoryOzone()
                       ->CreateEGLSurfaceForWidget(widget_);
  if (!ozone_surface_) {
    LOG(ERROR) << "Failed to create native surface.";
    return false;
  }

  window_ = ozone_surface_->GetNativeWindow();
  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to initialize.";
    return false;
  }

  return true;
}

class GL_EXPORT GLSurfaceOzoneSurfaceless : public SurfacelessEGL {
 public:
  GLSurfaceOzoneSurfaceless(std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
                            gfx::AcceleratedWidget widget);

  // GLSurface:
  bool Initialize(GLSurface::Format format) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers() override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  bool IsOffscreen() override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  bool SupportsAsyncSwap() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  void SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          const SwapCompletionCallback& callback) override;
  EGLConfig GetConfig() override;

 protected:
  struct PendingFrame {
    PendingFrame();

    bool ScheduleOverlayPlanes(gfx::AcceleratedWidget widget);

    bool ready;
    std::vector<GLSurfaceOverlay> overlays;
    SwapCompletionCallback callback;
  };

  ~GLSurfaceOzoneSurfaceless() override;

  void SubmitFrame();

  EGLSyncKHR InsertFence();
  void FenceRetired(EGLSyncKHR fence, PendingFrame* frame);

  void SwapCompleted(const SwapCompletionCallback& callback,
                     gfx::SwapResult result);

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface_;
  gfx::AcceleratedWidget widget_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;
  ScopedVector<PendingFrame> unsubmitted_frames_;
  bool has_implicit_external_sync_;
  bool last_swap_buffers_result_;
  bool swap_buffers_pending_;

  base::WeakPtrFactory<GLSurfaceOzoneSurfaceless> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneSurfaceless);
};

GLSurfaceOzoneSurfaceless::PendingFrame::PendingFrame() : ready(false) {}

bool GLSurfaceOzoneSurfaceless::PendingFrame::ScheduleOverlayPlanes(
    gfx::AcceleratedWidget widget) {
  for (const auto& overlay : overlays)
    if (!overlay.ScheduleOverlayPlane(widget))
      return false;
  return true;
}

GLSurfaceOzoneSurfaceless::GLSurfaceOzoneSurfaceless(
    std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
    gfx::AcceleratedWidget widget)
    : SurfacelessEGL(gfx::Size()),
      ozone_surface_(std::move(ozone_surface)),
      widget_(widget),
      has_implicit_external_sync_(
          HasEGLExtension("EGL_ARM_implicit_external_sync")),
      last_swap_buffers_result_(true),
      swap_buffers_pending_(false),
      weak_factory_(this) {
  unsubmitted_frames_.push_back(new PendingFrame());
}

bool GLSurfaceOzoneSurfaceless::Initialize(GLSurface::Format format) {
  if (!SurfacelessEGL::Initialize(format))
    return false;
  vsync_provider_ = ozone_surface_->CreateVSyncProvider();
  if (!vsync_provider_)
    return false;
  return true;
}

bool GLSurfaceOzoneSurfaceless::Resize(const gfx::Size& size,
                                       float scale_factor,
                                       bool has_alpha) {
  if (!ozone_surface_->ResizeNativeWindow(size))
    return false;

  return SurfacelessEGL::Resize(size, scale_factor, has_alpha);
}

gfx::SwapResult GLSurfaceOzoneSurfaceless::SwapBuffers() {
  glFlush();
  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  if (has_implicit_external_sync_) {
    EGLSyncKHR fence = InsertFence();
    if (!fence)
      return gfx::SwapResult::SWAP_FAILED;

    EGLDisplay display = GetDisplay();
    WaitForFence(display, fence);
    eglDestroySyncKHR(display, fence);
  }

  unsubmitted_frames_.back()->ScheduleOverlayPlanes(widget_);
  unsubmitted_frames_.back()->overlays.clear();

  if (ozone_surface_->IsUniversalDisplayLinkDevice())
    glFinish();

  return ozone_surface_->OnSwapBuffers() ? gfx::SwapResult::SWAP_ACK
                                         : gfx::SwapResult::SWAP_FAILED;
}

bool GLSurfaceOzoneSurfaceless::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect) {
  unsubmitted_frames_.back()->overlays.push_back(
      GLSurfaceOverlay(z_order, transform, image, bounds_rect, crop_rect));
  return true;
}

bool GLSurfaceOzoneSurfaceless::IsOffscreen() {
  return false;
}

gfx::VSyncProvider* GLSurfaceOzoneSurfaceless::GetVSyncProvider() {
  return vsync_provider_.get();
}

bool GLSurfaceOzoneSurfaceless::SupportsAsyncSwap() {
  return true;
}

bool GLSurfaceOzoneSurfaceless::SupportsPostSubBuffer() {
  return true;
}

gfx::SwapResult GLSurfaceOzoneSurfaceless::PostSubBuffer(int x,
                                                         int y,
                                                         int width,
                                                         int height) {
  // The actual sub buffer handling is handled at higher layers.
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurfaceOzoneSurfaceless::SwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    callback.Run(gfx::SwapResult::SWAP_FAILED);
    return;
  }

  glFlush();

  SwapCompletionCallback surface_swap_callback =
      base::Bind(&GLSurfaceOzoneSurfaceless::SwapCompleted,
                 weak_factory_.GetWeakPtr(), callback);

  PendingFrame* frame = unsubmitted_frames_.back();
  frame->callback = surface_swap_callback;
  unsubmitted_frames_.push_back(new PendingFrame());

  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  if (has_implicit_external_sync_) {
    EGLSyncKHR fence = InsertFence();
    if (!fence) {
      callback.Run(gfx::SwapResult::SWAP_FAILED);
      return;
    }

    base::Closure fence_wait_task =
        base::Bind(&WaitForFence, GetDisplay(), fence);

    base::Closure fence_retired_callback =
        base::Bind(&GLSurfaceOzoneSurfaceless::FenceRetired,
                   weak_factory_.GetWeakPtr(), fence, frame);

    base::WorkerPool::PostTaskAndReply(FROM_HERE, fence_wait_task,
                                       fence_retired_callback, false);
    return;  // Defer frame submission until fence signals.
  }

  frame->ready = true;
  SubmitFrame();
}

void GLSurfaceOzoneSurfaceless::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const SwapCompletionCallback& callback) {
  // The actual sub buffer handling is handled at higher layers.
  SwapBuffersAsync(callback);
}

EGLConfig GLSurfaceOzoneSurfaceless::GetConfig() {
  if (!config_) {
    ui::EglConfigCallbacks callbacks = GetEglConfigCallbacks(GetDisplay());
    config_ = ozone_surface_->GetEGLSurfaceConfig(callbacks);
  }
  if (config_)
    return config_;
  return SurfacelessEGL::GetConfig();
}

GLSurfaceOzoneSurfaceless::~GLSurfaceOzoneSurfaceless() {
  Destroy();  // The EGL surface must be destroyed before SurfaceOzone.
}

void GLSurfaceOzoneSurfaceless::SubmitFrame() {
  DCHECK(!unsubmitted_frames_.empty());

  if (unsubmitted_frames_.front()->ready && !swap_buffers_pending_) {
    std::unique_ptr<PendingFrame> frame(unsubmitted_frames_.front());
    unsubmitted_frames_.weak_erase(unsubmitted_frames_.begin());
    swap_buffers_pending_ = true;

    if (!frame->ScheduleOverlayPlanes(widget_)) {
      // |callback| is a wrapper for SwapCompleted(). Call it to properly
      // propagate the failed state.
      frame->callback.Run(gfx::SwapResult::SWAP_FAILED);
      return;
    }

    if (ozone_surface_->IsUniversalDisplayLinkDevice())
      glFinish();

    ozone_surface_->OnSwapBuffersAsync(frame->callback);
  }
}

EGLSyncKHR GLSurfaceOzoneSurfaceless::InsertFence() {
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};
  return eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR, attrib_list);
}

void GLSurfaceOzoneSurfaceless::FenceRetired(EGLSyncKHR fence,
                                             PendingFrame* frame) {
  eglDestroySyncKHR(GetDisplay(), fence);
  frame->ready = true;
  SubmitFrame();
}

void GLSurfaceOzoneSurfaceless::SwapCompleted(
    const SwapCompletionCallback& callback,
    gfx::SwapResult result) {
  callback.Run(result);
  swap_buffers_pending_ = false;
  if (result == gfx::SwapResult::SWAP_FAILED) {
    last_swap_buffers_result_ = false;
    return;
  }

  SubmitFrame();
}

// This provides surface-like semantics implemented through surfaceless.
// A framebuffer is bound automatically.
class GL_EXPORT GLSurfaceOzoneSurfacelessSurfaceImpl
    : public GLSurfaceOzoneSurfaceless {
 public:
  GLSurfaceOzoneSurfacelessSurfaceImpl(
      std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
      gfx::AcceleratedWidget widget);

  // GLSurface:
  unsigned int GetBackingFrameBufferObject() override;
  bool OnMakeCurrent(GLContext* context) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult SwapBuffers() override;
  void SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  void Destroy() override;
  bool IsSurfaceless() const override;

 private:
  ~GLSurfaceOzoneSurfacelessSurfaceImpl() override;

  void BindFramebuffer();
  bool CreatePixmaps();

  scoped_refptr<GLContext> context_;
  GLuint fbo_;
  GLuint textures_[2];
  scoped_refptr<GLImage> images_[2];
  int current_surface_;
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneSurfacelessSurfaceImpl);
};

GLSurfaceOzoneSurfacelessSurfaceImpl::GLSurfaceOzoneSurfacelessSurfaceImpl(
    std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
    gfx::AcceleratedWidget widget)
    : GLSurfaceOzoneSurfaceless(std::move(ozone_surface), widget),
      context_(nullptr),
      fbo_(0),
      current_surface_(0) {
  for (auto& texture : textures_)
    texture = 0;
}

unsigned int
GLSurfaceOzoneSurfacelessSurfaceImpl::GetBackingFrameBufferObject() {
  return fbo_;
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::OnMakeCurrent(GLContext* context) {
  DCHECK(!context_ || context == context_);
  context_ = context;
  if (!fbo_) {
    glGenFramebuffersEXT(1, &fbo_);
    if (!fbo_)
      return false;
    glGenTextures(arraysize(textures_), textures_);
    if (!CreatePixmaps())
      return false;
  }
  BindFramebuffer();
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_);
  return SurfacelessEGL::OnMakeCurrent(context);
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::Resize(const gfx::Size& size,
                                                  float scale_factor,
                                                  bool has_alpha) {
  if (size == GetSize())
    return true;
  // Alpha value isn't actually used in allocating buffers yet, so always use
  // true instead.
  return GLSurfaceOzoneSurfaceless::Resize(size, scale_factor, true) &&
         CreatePixmaps();
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::SupportsPostSubBuffer() {
  return false;
}

gfx::SwapResult GLSurfaceOzoneSurfacelessSurfaceImpl::SwapBuffers() {
  if (!images_[current_surface_]->ScheduleOverlayPlane(
          widget_, 0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(GetSize()), gfx::RectF(1, 1)))
    return gfx::SwapResult::SWAP_FAILED;
  gfx::SwapResult result = GLSurfaceOzoneSurfaceless::SwapBuffers();
  if (result != gfx::SwapResult::SWAP_ACK)
    return result;
  current_surface_ ^= 1;
  BindFramebuffer();
  return gfx::SwapResult::SWAP_ACK;
}

void GLSurfaceOzoneSurfacelessSurfaceImpl::SwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  if (!images_[current_surface_]->ScheduleOverlayPlane(
          widget_, 0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(GetSize()), gfx::RectF(1, 1))) {
    callback.Run(gfx::SwapResult::SWAP_FAILED);
    return;
  }
  GLSurfaceOzoneSurfaceless::SwapBuffersAsync(callback);
  current_surface_ ^= 1;
  BindFramebuffer();
}

void GLSurfaceOzoneSurfacelessSurfaceImpl::Destroy() {
  if (!context_)
    return;
  scoped_refptr<GLContext> previous_context = GLContext::GetCurrent();
  scoped_refptr<GLSurface> previous_surface;

  bool was_current = previous_context && previous_context->IsCurrent(nullptr) &&
                     GLSurface::GetCurrent() == this;
  if (!was_current) {
    // Only take a reference to previous surface if it's not |this|
    // because otherwise we can take a self reference from our own dtor.
    previous_surface = GLSurface::GetCurrent();
    context_->MakeCurrent(this);
  }

  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  if (fbo_) {
    glDeleteTextures(arraysize(textures_), textures_);
    for (auto& texture : textures_)
      texture = 0;
    glDeleteFramebuffersEXT(1, &fbo_);
    fbo_ = 0;
  }
  for (auto image : images_) {
    if (image)
      image->Destroy(true);
  }

  if (!was_current) {
    if (previous_context) {
      previous_context->MakeCurrent(previous_surface.get());
    } else {
      context_->ReleaseCurrent(this);
    }
  }
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::IsSurfaceless() const {
  return false;
}

GLSurfaceOzoneSurfacelessSurfaceImpl::~GLSurfaceOzoneSurfacelessSurfaceImpl() {
  Destroy();
}

void GLSurfaceOzoneSurfacelessSurfaceImpl::BindFramebuffer() {
  ScopedFrameBufferBinder fb(fbo_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            textures_[current_surface_], 0);
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::CreatePixmaps() {
  if (!fbo_)
    return true;
  for (size_t i = 0; i < arraysize(textures_); i++) {
    scoped_refptr<ui::NativePixmap> pixmap =
        ui::OzonePlatform::GetInstance()
            ->GetSurfaceFactoryOzone()
            ->CreateNativePixmap(widget_, GetSize(),
                                 gfx::BufferFormat::BGRA_8888,
                                 gfx::BufferUsage::SCANOUT);
    if (!pixmap)
      return false;
    scoped_refptr<GLImageOzoneNativePixmap> image =
        new GLImageOzoneNativePixmap(GetSize(), GL_BGRA_EXT);
    if (!image->Initialize(pixmap.get(), gfx::BufferFormat::BGRA_8888))
      return false;
    // Image must have Destroy() called before destruction.
    if (images_[i])
      images_[i]->Destroy(true);
    images_[i] = image;
    // Bind image to texture.
    ScopedTextureBinder binder(GL_TEXTURE_2D, textures_[i]);
    if (!images_[i]->BindTexImage(GL_TEXTURE_2D))
      return false;
  }
  return true;
}

}  // namespace

scoped_refptr<GLSurface> CreateViewGLSurfaceOzone(
    gfx::AcceleratedWidget window) {
  std::unique_ptr<ui::SurfaceOzoneEGL> surface_ozone =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateEGLSurfaceForWidget(window);
  if (!surface_ozone)
    return nullptr;
  return InitializeGLSurface(
      new GLSurfaceOzoneEGL(std::move(surface_ozone), window));
}

scoped_refptr<GLSurface> CreateViewGLSurfaceOzoneSurfaceless(
    gfx::AcceleratedWidget window) {
  std::unique_ptr<ui::SurfaceOzoneEGL> surface_ozone =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateSurfacelessEGLSurfaceForWidget(window);
  if (!surface_ozone)
    return nullptr;
  return InitializeGLSurface(
      new GLSurfaceOzoneSurfaceless(std::move(surface_ozone), window));
}

scoped_refptr<GLSurface> CreateViewGLSurfaceOzoneSurfacelessSurfaceImpl(
    gfx::AcceleratedWidget window) {
  std::unique_ptr<ui::SurfaceOzoneEGL> surface_ozone =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateSurfacelessEGLSurfaceForWidget(window);
  if (!surface_ozone)
    return nullptr;
  return InitializeGLSurface(new GLSurfaceOzoneSurfacelessSurfaceImpl(
      std::move(surface_ozone), window));
}

}  // namespace gl
