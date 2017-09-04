// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_glx.h"

extern "C" {
#include <X11/Xlib.h>
}
#include <memory>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_connection.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_visual_picker_glx.h"
#include "ui/gl/sync_control_vsync_provider.h"

namespace gl {

namespace {

Display* g_display = nullptr;
bool g_glx_context_create = false;
bool g_glx_create_context_robustness_supported = false;
bool g_glx_create_context_profile_supported = false;
bool g_glx_create_context_profile_es2_supported = false;
bool g_glx_texture_from_pixmap_supported = false;
bool g_glx_oml_sync_control_supported = false;

// Track support of glXGetMscRateOML separately from GLX_OML_sync_control as a
// whole since on some platforms (e.g. crosbug.com/34585), glXGetMscRateOML
// always fails even though GLX_OML_sync_control is reported as being supported.
bool g_glx_get_msc_rate_oml_supported = false;

bool g_glx_sgi_video_sync_supported = false;

// A 24-bit RGB visual and colormap to use when creating offscreen surfaces.
Visual* g_visual = nullptr;
int g_depth = CopyFromParent;
Colormap g_colormap = CopyFromParent;

GLXFBConfig GetConfigForWindow(Display* display,
                               gfx::AcceleratedWidget window) {
  DCHECK(window != 0);

  // This code path is expensive, but we only take it when
  // attempting to use GLX_ARB_create_context_robustness, in which
  // case we need a GLXFBConfig for the window in order to create a
  // context for it.
  //
  // TODO(kbr): this is not a reliable code path. On platforms which
  // support it, we should use glXChooseFBConfig in the browser
  // process to choose the FBConfig and from there the X Visual to
  // use when creating the window in the first place. Then we can
  // pass that FBConfig down rather than attempting to reconstitute
  // it.

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(display, window, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window << ".";
    return nullptr;
  }

  int visual_id = XVisualIDFromVisual(attributes.visual);

  int num_elements = 0;
  gfx::XScopedPtr<GLXFBConfig> configs(
      glXGetFBConfigs(display, DefaultScreen(display), &num_elements));
  if (!configs.get()) {
    LOG(ERROR) << "glXGetFBConfigs failed.";
    return nullptr;
  }
  if (!num_elements) {
    LOG(ERROR) << "glXGetFBConfigs returned 0 elements.";
    return nullptr;
  }
  bool found = false;
  int i;
  for (i = 0; i < num_elements; ++i) {
    int value;
    if (glXGetFBConfigAttrib(display, configs.get()[i], GLX_VISUAL_ID,
                             &value)) {
      LOG(ERROR) << "glXGetFBConfigAttrib failed.";
      return nullptr;
    }
    if (value == visual_id) {
      found = true;
      break;
    }
  }
  if (found) {
    return configs.get()[i];
  }
  return nullptr;
}

bool CreateDummyWindow(Display* display) {
  DCHECK(display);
  gfx::AcceleratedWidget parent_window =
      RootWindow(display, DefaultScreen(display));
  gfx::AcceleratedWidget window =
      XCreateWindow(display, parent_window, 0, 0, 1, 1, 0, CopyFromParent,
                    InputOutput, CopyFromParent, 0, nullptr);
  if (!window) {
    LOG(ERROR) << "XCreateWindow failed";
    return false;
  }
  GLXFBConfig config = GetConfigForWindow(display, window);
  GLXWindow glx_window = glXCreateWindow(display, config, window, nullptr);
  if (!glx_window) {
    LOG(ERROR) << "glXCreateWindow failed";
    XDestroyWindow(display, window);
    return false;
  }
  glXDestroyWindow(display, glx_window);
  XDestroyWindow(display, window);
  return true;
}

class OMLSyncControlVSyncProvider : public SyncControlVSyncProvider {
 public:
  explicit OMLSyncControlVSyncProvider(GLXWindow glx_window)
      : SyncControlVSyncProvider(), glx_window_(glx_window) {}

  ~OMLSyncControlVSyncProvider() override {}

 protected:
  bool GetSyncValues(int64_t* system_time,
                     int64_t* media_stream_counter,
                     int64_t* swap_buffer_counter) override {
    return glXGetSyncValuesOML(g_display, glx_window_, system_time,
                               media_stream_counter, swap_buffer_counter);
  }

  bool GetMscRate(int32_t* numerator, int32_t* denominator) override {
    if (!g_glx_get_msc_rate_oml_supported)
      return false;

    if (!glXGetMscRateOML(g_display, glx_window_, numerator, denominator)) {
      // Once glXGetMscRateOML has been found to fail, don't try again,
      // since each failing call may spew an error message.
      g_glx_get_msc_rate_oml_supported = false;
      return false;
    }

    return true;
  }

 private:
  GLXWindow glx_window_;

  DISALLOW_COPY_AND_ASSIGN(OMLSyncControlVSyncProvider);
};

class SGIVideoSyncThread : public base::Thread,
                           public base::NonThreadSafe,
                           public base::RefCounted<SGIVideoSyncThread> {
 public:
  static scoped_refptr<SGIVideoSyncThread> Create() {
    if (!g_video_sync_thread) {
      g_video_sync_thread = new SGIVideoSyncThread();
      g_video_sync_thread->Start();
    }
    return g_video_sync_thread;
  }

 private:
  friend class base::RefCounted<SGIVideoSyncThread>;

  SGIVideoSyncThread() : base::Thread("SGI_video_sync") {
    DCHECK(CalledOnValidThread());
  }

  ~SGIVideoSyncThread() override {
    DCHECK(CalledOnValidThread());
    g_video_sync_thread = nullptr;
    Stop();
  }

  static SGIVideoSyncThread* g_video_sync_thread;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncThread);
};

class SGIVideoSyncProviderThreadShim {
 public:
  explicit SGIVideoSyncProviderThreadShim(gfx::AcceleratedWidget parent_window)
      : parent_window_(parent_window),
        window_(0),
        glx_window_(0),
        task_runner_(base::ThreadTaskRunnerHandle::Get()),
        cancel_vsync_flag_(),
        vsync_lock_() {
    // This ensures that creation of |parent_window_| has occured when this shim
    // is executing in the same thread as the call to create |parent_window_|.
    XSync(g_display, False);
  }

  virtual ~SGIVideoSyncProviderThreadShim() {
    if (glx_window_)
      glXDestroyWindow(display_, glx_window_);

    if (window_)
      XDestroyWindow(display_, window_);
  }

  base::CancellationFlag* cancel_vsync_flag() { return &cancel_vsync_flag_; }

  base::Lock* vsync_lock() { return &vsync_lock_; }

  void Initialize() {
    DCHECK(display_);

    window_ =
        XCreateWindow(display_, parent_window_, 0, 0, 1, 1, 0, CopyFromParent,
                      InputOutput, CopyFromParent, 0, nullptr);
    if (!window_) {
      LOG(ERROR) << "video_sync: XCreateWindow failed";
      return;
    }

    GLXFBConfig config = GetConfigForWindow(display_, window_);
    DCHECK(config);

    glx_window_ = glXCreateWindow(display_, config, window_, nullptr);
    if (!glx_window_) {
      LOG(ERROR) << "video_sync: glXCreateWindow failed";
      return;
    }

    // Create the context only once for all vsync providers.
    if (!context_) {
      context_ =
        glXCreateNewContext(display_, config, GLX_RGBA_TYPE, nullptr, True);
      if (!context_)
        LOG(ERROR) << "video_sync: glXCreateNewContext failed";
    }
  }

  void GetVSyncParameters(
      const gfx::VSyncProvider::UpdateVSyncCallback& callback) {
    base::TimeTicks now;
    {
      // Don't allow |window_| destruction while we're probing vsync.
      base::AutoLock locked(vsync_lock_);

      if (!context_ || cancel_vsync_flag_.IsSet())
        return;

      glXMakeContextCurrent(display_, glx_window_, glx_window_, context_);

      unsigned int retrace_count = 0;
      if (glXWaitVideoSyncSGI(1, 0, &retrace_count) != 0)
        return;

      TRACE_EVENT_INSTANT0("gpu", "vblank", TRACE_EVENT_SCOPE_THREAD);
      now = base::TimeTicks::Now();

      glXMakeContextCurrent(display_, 0, 0, nullptr);
    }

    const base::TimeDelta kDefaultInterval =
        base::TimeDelta::FromSeconds(1) / 60;

    task_runner_->PostTask(FROM_HERE,
                           base::Bind(callback, now, kDefaultInterval));
  }

 private:
  // For initialization of display_ in GLSurface::InitializeOneOff before
  // the sandbox goes up.
  friend class gl::GLSurfaceGLX;

  // We only need one Display and GLXContext because we only use one thread for
  // SGI_video_sync. The display is created in GLSurfaceGLX::InitializeOneOff
  // and the context is created the first time a vsync provider is initialized.
  static Display* display_;
  static GLXContext context_;

  gfx::AcceleratedWidget parent_window_;

  gfx::AcceleratedWidget window_;
  GLXWindow glx_window_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::CancellationFlag cancel_vsync_flag_;
  base::Lock vsync_lock_;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncProviderThreadShim);
};

class SGIVideoSyncVSyncProvider
    : public gfx::VSyncProvider,
      public base::SupportsWeakPtr<SGIVideoSyncVSyncProvider> {
 public:
  explicit SGIVideoSyncVSyncProvider(gfx::AcceleratedWidget parent_window)
      : vsync_thread_(SGIVideoSyncThread::Create()),
        shim_(new SGIVideoSyncProviderThreadShim(parent_window)),
        cancel_vsync_flag_(shim_->cancel_vsync_flag()),
        vsync_lock_(shim_->vsync_lock()) {
    vsync_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&SGIVideoSyncProviderThreadShim::Initialize,
                              base::Unretained(shim_.get())));
  }

  ~SGIVideoSyncVSyncProvider() override {
    {
      base::AutoLock locked(*vsync_lock_);
      cancel_vsync_flag_->Set();
    }

    // Hand-off |shim_| to be deleted on the |vsync_thread_|.
    vsync_thread_->task_runner()->DeleteSoon(FROM_HERE, shim_.release());
  }

  void GetVSyncParameters(
      const gfx::VSyncProvider::UpdateVSyncCallback& callback) override {
    // Only one outstanding request per surface.
    if (!pending_callback_) {
      pending_callback_.reset(
          new gfx::VSyncProvider::UpdateVSyncCallback(callback));
      vsync_thread_->task_runner()->PostTask(
          FROM_HERE,
          base::Bind(
              &SGIVideoSyncProviderThreadShim::GetVSyncParameters,
              base::Unretained(shim_.get()),
              base::Bind(&SGIVideoSyncVSyncProvider::PendingCallbackRunner,
                         AsWeakPtr())));
    }
  }

 private:
  void PendingCallbackRunner(const base::TimeTicks timebase,
                             const base::TimeDelta interval) {
    DCHECK(pending_callback_);
    pending_callback_->Run(timebase, interval);
    pending_callback_.reset();
  }

  scoped_refptr<SGIVideoSyncThread> vsync_thread_;

  // Thread shim through which the sync provider is accessed on |vsync_thread_|.
  std::unique_ptr<SGIVideoSyncProviderThreadShim> shim_;

  std::unique_ptr<gfx::VSyncProvider::UpdateVSyncCallback> pending_callback_;

  // Raw pointers to sync primitives owned by the shim_.
  // These will only be referenced before we post a task to destroy
  // the shim_, so they are safe to access.
  base::CancellationFlag* cancel_vsync_flag_;
  base::Lock* vsync_lock_;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncVSyncProvider);
};

SGIVideoSyncThread* SGIVideoSyncThread::g_video_sync_thread = nullptr;

// In order to take advantage of GLX_SGI_video_sync, we need a display
// for use on a separate thread. We must allocate this before the sandbox
// goes up (rather than on-demand when we start the thread).
Display* SGIVideoSyncProviderThreadShim::display_ = nullptr;
GLXContext SGIVideoSyncProviderThreadShim::context_ = 0;

}  // namespace

GLSurfaceGLX::GLSurfaceGLX() {}

bool GLSurfaceGLX::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  // http://crbug.com/245466
  setenv("force_s3tc_enable", "true", 1);

  // SGIVideoSyncProviderShim (if instantiated) will issue X commands on
  // it's own thread.
  gfx::InitializeThreadedX11();
  g_display = gfx::GetXDisplay();

  if (!g_display) {
    LOG(ERROR) << "XOpenDisplay failed.";
    return false;
  }

  int major, minor;
  if (!glXQueryVersion(g_display, &major, &minor)) {
    LOG(ERROR) << "glxQueryVersion failed";
    return false;
  }

  if (major == 1 && minor < 3) {
    LOG(ERROR) << "GLX 1.3 or later is required.";
    return false;
  }

  g_glx_context_create = HasGLXExtension("GLX_ARB_create_context");
  g_glx_create_context_robustness_supported =
      HasGLXExtension("GLX_ARB_create_context_robustness");
  g_glx_create_context_profile_supported =
      HasGLXExtension("GLX_ARB_create_context_profile");
  g_glx_create_context_profile_es2_supported =
      HasGLXExtension("GLX_ARB_create_context_es2_profile");
  g_glx_texture_from_pixmap_supported =
      HasGLXExtension("GLX_EXT_texture_from_pixmap");
  g_glx_oml_sync_control_supported = HasGLXExtension("GLX_OML_sync_control");
  g_glx_get_msc_rate_oml_supported = g_glx_oml_sync_control_supported;
  g_glx_sgi_video_sync_supported = HasGLXExtension("GLX_SGI_video_sync");

  const XVisualInfo& visual_info =
      gl::GLVisualPickerGLX::GetInstance()->system_visual();
  g_visual = visual_info.visual;
  g_depth = visual_info.depth;
  g_colormap = XCreateColormap(g_display, DefaultRootWindow(g_display),
                               visual_info.visual, AllocNone);

  // We create a dummy unmapped window for both the main Display and the video
  // sync Display so that the Nvidia driver can initialize itself before the
  // sandbox is set up.
  // Unfortunately some fds e.g. /dev/nvidia0 are cached per thread and because
  // we can't start threads before the sandbox is set up, these are accessed
  // through the broker process. See GpuProcessPolicy::InitGpuBrokerProcess.
  if (!CreateDummyWindow(g_display)) {
    LOG(ERROR) << "CreateDummyWindow(g_display) failed";
    return false;
  }

  if (!g_glx_get_msc_rate_oml_supported && g_glx_sgi_video_sync_supported) {
    Display* video_sync_display = gfx::OpenNewXDisplay();
    if (!CreateDummyWindow(video_sync_display)) {
      LOG(ERROR) << "CreateDummyWindow(video_sync_display) failed";
      return false;
    }
    SGIVideoSyncProviderThreadShim::display_ = video_sync_display;
  }

  initialized = true;
  return true;
}

// static
const char* GLSurfaceGLX::GetGLXExtensions() {
  return glXQueryExtensionsString(g_display, 0);
}

// static
bool GLSurfaceGLX::HasGLXExtension(const char* name) {
  return ExtensionsContain(GetGLXExtensions(), name);
}

// static
bool GLSurfaceGLX::IsCreateContextSupported() {
  return g_glx_context_create;
}

// static
bool GLSurfaceGLX::IsCreateContextRobustnessSupported() {
  return g_glx_create_context_robustness_supported;
}

// static
bool GLSurfaceGLX::IsCreateContextProfileSupported() {
  return g_glx_create_context_profile_supported;
}

// static
bool GLSurfaceGLX::IsCreateContextES2ProfileSupported() {
  return g_glx_create_context_profile_es2_supported;
}

// static
bool GLSurfaceGLX::IsTextureFromPixmapSupported() {
  return g_glx_texture_from_pixmap_supported;
}

// static
bool GLSurfaceGLX::IsOMLSyncControlSupported() {
  return g_glx_oml_sync_control_supported;
}

void* GLSurfaceGLX::GetDisplay() {
  return g_display;
}

GLSurfaceGLX::~GLSurfaceGLX() {}

NativeViewGLSurfaceGLX::NativeViewGLSurfaceGLX(gfx::AcceleratedWidget window)
    : parent_window_(window),
      window_(0),
      glx_window_(0),
      config_(nullptr),
      visual_id_(CopyFromParent) {}

GLXDrawable NativeViewGLSurfaceGLX::GetDrawableHandle() const {
  return glx_window_;
}

bool NativeViewGLSurfaceGLX::Initialize(GLSurface::Format format) {
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(g_display, parent_window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << parent_window_
               << ".";
    return false;
  }
  size_ = gfx::Size(attributes.width, attributes.height);
  visual_id_ = XVisualIDFromVisual(attributes.visual);
  // Create a child window, with a CopyFromParent visual (to avoid inducing
  // extra blits in the driver), that we can resize exactly in Resize(),
  // correctly ordered with GL, so that we don't have invalid transient states.
  // See https://crbug.com/326995.
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = 0;
  swa.bit_gravity = NorthWestGravity;
  window_ = XCreateWindow(g_display, parent_window_, 0, 0, size_.width(),
                          size_.height(), 0, CopyFromParent, InputOutput,
                          CopyFromParent, CWBackPixmap | CWBitGravity, &swa);
  XMapWindow(g_display, window_);

  RegisterEvents();
  XFlush(g_display);

  GetConfig();
  DCHECK(config_);
  glx_window_ = glXCreateWindow(g_display, config_, window_, NULL);

  if (g_glx_oml_sync_control_supported) {
    vsync_provider_.reset(new OMLSyncControlVSyncProvider(glx_window_));
  } else if (g_glx_sgi_video_sync_supported) {
    vsync_provider_.reset(new SGIVideoSyncVSyncProvider(parent_window_));
  } else {
    // Assume a refresh rate of 59.9 Hz, which will cause us to skip
    // 1 frame every 10 seconds on a 60Hz monitor, but will prevent us
    // from blocking the GPU service due to back pressure. This would still
    // encounter backpressure on a <60Hz monitor, but hopefully that is
    // not common.
    const base::TimeTicks kDefaultTimebase;
    const base::TimeDelta kDefaultInterval =
        base::TimeDelta::FromSeconds(1) / 59.9;
    vsync_provider_.reset(
        new gfx::FixedVSyncProvider(kDefaultTimebase, kDefaultInterval));
  }

  return true;
}

void NativeViewGLSurfaceGLX::Destroy() {
  vsync_provider_.reset();
  if (glx_window_) {
    glXDestroyWindow(g_display, glx_window_);
    glx_window_ = 0;
  }
  if (window_) {
    UnregisterEvents();
    XDestroyWindow(g_display, window_);
    window_ = 0;
    XFlush(g_display);
  }
}

bool NativeViewGLSurfaceGLX::Resize(const gfx::Size& size,
                                    float scale_factor,
                                    bool has_alpha) {
  size_ = size;
  glXWaitGL();
  XResizeWindow(g_display, window_, size.width(), size.height());
  glXWaitX();
  return true;
}

bool NativeViewGLSurfaceGLX::IsOffscreen() {
  return false;
}

gfx::SwapResult NativeViewGLSurfaceGLX::SwapBuffers() {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceGLX:RealSwapBuffers", "width",
               GetSize().width(), "height", GetSize().height());

  glXSwapBuffers(g_display, GetDrawableHandle());
  return gfx::SwapResult::SWAP_ACK;
}

gfx::Size NativeViewGLSurfaceGLX::GetSize() {
  return size_;
}

void* NativeViewGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(GetDrawableHandle());
}

bool NativeViewGLSurfaceGLX::SupportsPostSubBuffer() {
  return g_driver_glx.ext.b_GLX_MESA_copy_sub_buffer;
}

void* NativeViewGLSurfaceGLX::GetConfig() {
  if (!config_)
    config_ = GetConfigForWindow(g_display, window_);
  return config_;
}

unsigned long NativeViewGLSurfaceGLX::GetCompatibilityKey() {
  return visual_id_;
}

gfx::SwapResult NativeViewGLSurfaceGLX::PostSubBuffer(int x,
                                                      int y,
                                                      int width,
                                                      int height) {
  DCHECK(g_driver_glx.ext.b_GLX_MESA_copy_sub_buffer);
  glXCopySubBufferMESA(g_display, GetDrawableHandle(), x, y, width, height);
  return gfx::SwapResult::SWAP_ACK;
}

gfx::VSyncProvider* NativeViewGLSurfaceGLX::GetVSyncProvider() {
  return vsync_provider_.get();
}

NativeViewGLSurfaceGLX::~NativeViewGLSurfaceGLX() {
  Destroy();
}

void NativeViewGLSurfaceGLX::ForwardExposeEvent(XEvent* event) {
  XEvent forwarded_event = *event;
  forwarded_event.xexpose.window = parent_window_;
  XSendEvent(g_display, parent_window_, False, ExposureMask, &forwarded_event);
  XFlush(g_display);
}

bool NativeViewGLSurfaceGLX::CanHandleEvent(XEvent* event) {
  return event->type == Expose &&
         event->xexpose.window == static_cast<Window>(window_);
}

UnmappedNativeViewGLSurfaceGLX::UnmappedNativeViewGLSurfaceGLX(
    const gfx::Size& size)
    : size_(size), config_(nullptr), window_(0), glx_window_(0) {
  // Ensure that we don't create a window with zero size.
  if (size_.GetArea() == 0)
    size_.SetSize(1, 1);
}

bool UnmappedNativeViewGLSurfaceGLX::Initialize(GLSurface::Format format) {
  DCHECK(!window_);

  gfx::AcceleratedWidget parent_window = DefaultRootWindow(g_display);

  XSetWindowAttributes attrs;
  attrs.border_pixel = 0;
  attrs.colormap = g_colormap;
  window_ = XCreateWindow(g_display, parent_window, 0, 0, size_.width(),
                          size_.height(), 0, g_depth, InputOutput, g_visual,
                          CWBorderPixel | CWColormap, &attrs);
  GetConfig();
  DCHECK(config_);
  glx_window_ = glXCreateWindow(g_display, config_, window_, NULL);
  return window_ != 0;
}

void UnmappedNativeViewGLSurfaceGLX::Destroy() {
  config_ = nullptr;
  if (glx_window_) {
    glXDestroyWindow(g_display, glx_window_);
    glx_window_ = 0;
  }
  if (window_) {
    XDestroyWindow(g_display, window_);
    window_ = 0;
  }
}

bool UnmappedNativeViewGLSurfaceGLX::IsOffscreen() {
  return true;
}

gfx::SwapResult UnmappedNativeViewGLSurfaceGLX::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on an unmapped window.";
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size UnmappedNativeViewGLSurfaceGLX::GetSize() {
  return size_;
}

void* UnmappedNativeViewGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(glx_window_);
}

void* UnmappedNativeViewGLSurfaceGLX::GetConfig() {
  if (!config_)
    config_ = GetConfigForWindow(g_display, window_);
  return config_;
}

unsigned long UnmappedNativeViewGLSurfaceGLX::GetCompatibilityKey() {
  return XVisualIDFromVisual(g_visual);
}

UnmappedNativeViewGLSurfaceGLX::~UnmappedNativeViewGLSurfaceGLX() {
  Destroy();
}

}  // namespace gl
