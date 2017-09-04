// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include <utility>

#include "base/android/application_status_listener.h"
#include "base/android/build_info.h"
#include "base/android/context_utils.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/latency_info_swap_promise.h"
#include "cc/resources/single_release_callback.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/composited_touch_handle_drawable.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/overscroll_controller_android.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_metadata_util.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu_host_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "skia/ext/image_operations.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/android/delegated_frame_host_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

namespace {

const int kUndefinedCompositorFrameSinkId = -1;

static const char kAsyncReadBackString[] = "Compositing.CopyFromSurfaceTime";

class PendingReadbackLock;

PendingReadbackLock* g_pending_readback_lock = nullptr;

class PendingReadbackLock : public base::RefCounted<PendingReadbackLock> {
 public:
  PendingReadbackLock() {
    DCHECK_EQ(g_pending_readback_lock, nullptr);
    g_pending_readback_lock = this;
  }

 private:
  friend class base::RefCounted<PendingReadbackLock>;

  ~PendingReadbackLock() {
    DCHECK_EQ(g_pending_readback_lock, this);
    g_pending_readback_lock = nullptr;
  }
};

using base::android::ApplicationState;
using base::android::ApplicationStatusListener;

class GLHelperHolder {
 public:
  static GLHelperHolder* Create();

  display_compositor::GLHelper* gl_helper() { return gl_helper_.get(); }
  bool IsLost() {
    if (!gl_helper_)
      return true;
    return provider_->ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
  }

  void ReleaseIfPossible();

 private:
  GLHelperHolder();
  void Initialize();
  void OnContextLost();
  void OnApplicationStatusChanged(ApplicationState new_state);

  scoped_refptr<ContextProviderCommandBuffer> provider_;
  std::unique_ptr<display_compositor::GLHelper> gl_helper_;

  // Set to |false| if there are only stopped activities (or none).
  bool has_running_or_paused_activities_;

  std::unique_ptr<ApplicationStatusListener> app_status_listener_;

  DISALLOW_COPY_AND_ASSIGN(GLHelperHolder);
};

GLHelperHolder::GLHelperHolder()
    : has_running_or_paused_activities_(
          (ApplicationStatusListener::GetState() ==
           base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) ||
          (ApplicationStatusListener::GetState() ==
           base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES)) {}

GLHelperHolder* GLHelperHolder::Create() {
  GLHelperHolder* holder = new GLHelperHolder;
  holder->Initialize();
  return holder;
}

void GLHelperHolder::Initialize() {
  auto* factory = BrowserGpuChannelHostFactory::instance();
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(factory->GetGpuChannel());

  // The Browser Compositor is in charge of reestablishing the channel if its
  // missing.
  if (!gpu_channel_host)
    return;

  // This is for an offscreen context, so we don't need the default framebuffer
  // to have alpha, stencil, depth, antialiasing.
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.stencil_size = 0;
  attributes.depth_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;

  constexpr size_t kBytesPerPixel = 4;
  size_t full_screen_texture_size_in_bytes =
      gfx::DeviceDisplayInfo().GetDisplayHeight() *
      gfx::DeviceDisplayInfo().GetDisplayWidth() * kBytesPerPixel;

  gpu::SharedMemoryLimits limits;
  // The GLHelper context doesn't do a lot of stuff, so we don't expect it to
  // need a lot of space for commands.
  limits.command_buffer_size = 1024;
  // The transfer buffer is used for shaders among other things, so give some
  // reasonable but small limit.
  limits.start_transfer_buffer_size = 4 * 1024;
  limits.min_transfer_buffer_size = 4 * 1024;
  limits.max_transfer_buffer_size = full_screen_texture_size_in_bytes;
  // This context is used for doing async readbacks, so allow at least a full
  // screen readback to be mapped.
  limits.mapped_memory_reclaim_limit = full_screen_texture_size_in_bytes;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  const GURL url("chrome://gpu/RenderWidgetHostViewAndroid");

  provider_ = new ContextProviderCommandBuffer(
      std::move(gpu_channel_host), gpu::GPU_STREAM_DEFAULT,
      gpu::GpuStreamPriority::NORMAL, gpu::kNullSurfaceHandle, url,
      automatic_flushes, support_locking, limits, attributes, nullptr,
      command_buffer_metrics::BROWSER_OFFSCREEN_MAINTHREAD_CONTEXT);
  if (!provider_->BindToCurrentThread())
    return;
  provider_->ContextGL()->TraceBeginCHROMIUM(
      "gpu_toplevel",
      base::StringPrintf("CmdBufferImageTransportFactory-%p", provider_.get())
          .c_str());
  provider_->SetLostContextCallback(
      base::Bind(&GLHelperHolder::OnContextLost, base::Unretained(this)));
  gl_helper_.reset(new display_compositor::GLHelper(
      provider_->ContextGL(), provider_->ContextSupport()));

  // Unretained() is safe because |this| owns the following two callbacks.
  app_status_listener_.reset(new ApplicationStatusListener(base::Bind(
      &GLHelperHolder::OnApplicationStatusChanged, base::Unretained(this))));
}

void GLHelperHolder::ReleaseIfPossible() {
  if (!has_running_or_paused_activities_ && !g_pending_readback_lock) {
    gl_helper_.reset();
    provider_ = nullptr;
    // Make sure this will get recreated on next use.
    DCHECK(IsLost());
  }
}

void GLHelperHolder::OnContextLost() {
  app_status_listener_.reset();
  gl_helper_.reset();
  // Need to post a task because the command buffer client cannot be deleted
  // from within this callback.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderWidgetHostViewAndroid::OnContextLost));
}

void GLHelperHolder::OnApplicationStatusChanged(ApplicationState new_state) {
  has_running_or_paused_activities_ =
      new_state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
      new_state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;
  ReleaseIfPossible();
}

GLHelperHolder* GetPostReadbackGLHelperHolder(bool create_if_necessary) {
  static GLHelperHolder* g_readback_helper_holder = nullptr;

  if (!create_if_necessary && !g_readback_helper_holder)
    return nullptr;

  if (g_readback_helper_holder && g_readback_helper_holder->IsLost()) {
    delete g_readback_helper_holder;
    g_readback_helper_holder = nullptr;
  }

  if (!g_readback_helper_holder)
    g_readback_helper_holder = GLHelperHolder::Create();

  return g_readback_helper_holder;
}

display_compositor::GLHelper* GetPostReadbackGLHelper() {
  bool create_if_necessary = true;
  return GetPostReadbackGLHelperHolder(create_if_necessary)->gl_helper();
}

void ReleaseGLHelper() {
  bool create_if_necessary = false;
  GLHelperHolder* holder = GetPostReadbackGLHelperHolder(create_if_necessary);

  if (holder) {
    holder->ReleaseIfPossible();
  }
}

void CopyFromCompositingSurfaceFinished(
    const ReadbackRequestCallback& callback,
    std::unique_ptr<cc::SingleReleaseCallback> release_callback,
    std::unique_ptr<SkBitmap> bitmap,
    const base::TimeTicks& start_time,
    std::unique_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    scoped_refptr<PendingReadbackLock> readback_lock,
    bool result) {
  TRACE_EVENT0(
      "cc", "RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceFinished");
  bitmap_pixels_lock.reset();
  gpu::SyncToken sync_token;
  if (result) {
    display_compositor::GLHelper* gl_helper = GetPostReadbackGLHelper();
    if (gl_helper)
      gl_helper->GenerateSyncToken(&sync_token);
  }

  // PostTask() to make sure the |readback_lock| is released. Also do this
  // synchronous GPU operation in a clean callstack.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&ReleaseGLHelper));

  const bool lost_resource = !sync_token.HasData();
  release_callback->Run(sync_token, lost_resource);
  UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                      base::TimeTicks::Now() - start_time);
  ReadbackResponse response = result ? READBACK_SUCCESS : READBACK_FAILED;
  callback.Run(*bitmap, response);
}

std::unique_ptr<ui::TouchSelectionController> CreateSelectionController(
    ui::TouchSelectionControllerClient* client,
    ContentViewCore* content_view_core) {
  DCHECK(client);
  DCHECK(content_view_core);
  ui::TouchSelectionController::Config config;
  config.max_tap_duration = base::TimeDelta::FromMilliseconds(
      gfx::ViewConfiguration::GetLongPressTimeoutInMs());
  config.tap_slop = gfx::ViewConfiguration::GetTouchSlopInDips();
  config.enable_adaptive_handle_orientation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAdaptiveSelectionHandleOrientation);
  config.enable_longpress_drag_selection =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLongpressDragSelection);
  return base::MakeUnique<ui::TouchSelectionController>(client, config);
}

std::unique_ptr<OverscrollControllerAndroid> CreateOverscrollController(
    ContentViewCoreImpl* content_view_core,
    float dpi_scale) {
  return base::MakeUnique<OverscrollControllerAndroid>(content_view_core,
                                                       dpi_scale);
}

gfx::RectF GetSelectionRect(const ui::TouchSelectionController& controller) {
  gfx::RectF rect = controller.GetRectBetweenBounds();
  if (rect.IsEmpty())
    return rect;

  rect.Union(controller.GetStartHandleRect());
  rect.Union(controller.GetEndHandleRect());
  return rect;
}

// TODO(wjmaclean): There is significant overlap between
// PrepareTextureCopyOutputResult and CopyFromCompositingSurfaceFinished in
// this file, and the versions in surface_utils.cc. They should
// be merged. See https://crbug.com/582955
void PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    SkColorType color_type,
    const base::TimeTicks& start_time,
    const ReadbackRequestCallback& callback,
    scoped_refptr<PendingReadbackLock> readback_lock,
    std::unique_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, SkBitmap(), READBACK_FAILED));
  TRACE_EVENT0("cc",
               "RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult");
  if (!result->HasTexture() || result->IsEmpty() || result->size().IsEmpty())
    return;
  cc::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;
  display_compositor::GLHelper* gl_helper = GetPostReadbackGLHelper();
  if (!gl_helper)
    return;
  if (!gl_helper->IsReadbackConfigSupported(color_type))
    color_type = kRGBA_8888_SkColorType;

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = result->size();
  else
    output_size_in_pixel = dst_size_in_pixel;

  std::unique_ptr<SkBitmap> bitmap(new SkBitmap);
  if (!bitmap->tryAllocPixels(SkImageInfo::Make(output_size_in_pixel.width(),
                                                output_size_in_pixel.height(),
                                                color_type,
                                                kOpaque_SkAlphaType))) {
    scoped_callback_runner.ReplaceClosure(
        base::Bind(callback, SkBitmap(), READBACK_BITMAP_ALLOCATION_FAILURE));
    return;
  }

  std::unique_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8_t* pixels = static_cast<uint8_t*>(bitmap->getPixels());

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(), texture_mailbox.sync_token(), result->size(),
      gfx::Rect(result->size()), output_size_in_pixel, pixels, color_type,
      base::Bind(&CopyFromCompositingSurfaceFinished, callback,
                 base::Passed(&release_callback), base::Passed(&bitmap),
                 start_time, base::Passed(&bitmap_pixels_lock), readback_lock),
      display_compositor::GLHelper::SCALER_QUALITY_GOOD);
}

}  // namespace

RenderWidgetHostViewAndroid::LastFrameInfo::LastFrameInfo(
    uint32_t compositor_frame_sink_id,
    cc::CompositorFrame output_frame)
    : compositor_frame_sink_id(compositor_frame_sink_id),
      frame(std::move(output_frame)) {}

RenderWidgetHostViewAndroid::LastFrameInfo::~LastFrameInfo() {}

void RenderWidgetHostViewAndroid::OnContextLost() {
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (widget->GetView()) {
      static_cast<RenderWidgetHostViewAndroid*>(widget->GetView())
          ->OnLostResources();
    }
  }
}

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    ContentViewCoreImpl* content_view_core)
    : host_(widget_host),
      outstanding_vsync_requests_(0),
      is_showing_(!widget_host->is_hidden()),
      is_window_visible_(true),
      is_window_activity_started_(true),
      content_view_core_(nullptr),
      ime_adapter_android_(this),
      cached_background_color_(SK_ColorWHITE),
      last_compositor_frame_sink_id_(kUndefinedCompositorFrameSinkId),
      gesture_provider_(ui::GetGestureProviderConfig(
                            ui::GestureProviderConfigType::CURRENT_PLATFORM),
                        this),
      stylus_text_selector_(this),
      using_browser_compositor_(CompositorImpl::IsInitialized()),
      synchronous_compositor_client_(nullptr),
      frame_evictor_(new DelegatedFrameEvictor(this)),
      locks_on_frame_count_(0),
      observing_root_window_(false),
      weak_ptr_factory_(this) {
  // Set the layer which will hold the content layer for this view. The content
  // layer is managed by the DelegatedFrameHost.
  view_.SetLayer(cc::Layer::Create());
  if (using_browser_compositor_) {
    delegated_frame_host_.reset(new ui::DelegatedFrameHostAndroid(
        &view_, cached_background_color_,
        base::Bind(&RenderWidgetHostViewAndroid::ReturnResources,
                   weak_ptr_factory_.GetWeakPtr())));
  }

  host_->SetView(this);
  SetContentViewCore(content_view_core);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  if (content_view_core_)
    content_view_core_->RemoveObserver(this);
  SetContentViewCore(NULL);
  DCHECK(ack_callbacks_.empty());
  DCHECK(!delegated_frame_host_);
}

void RenderWidgetHostViewAndroid::Blur() {
  host_->Blur();
  if (overscroll_controller_)
    overscroll_controller_->Disable();
}

bool RenderWidgetHostViewAndroid::OnMessageReceived(
    const IPC::Message& message) {
  if (IPC_MESSAGE_ID_CLASS(message.type()) == SyncCompositorMsgStart) {
    return SyncCompositorOnMessageReceived(message);
  }
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAndroid, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartContentIntent, OnStartContentIntent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SmartClipDataExtracted,
                        OnSmartClipDataExtracted)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowUnhandledTapUIIfNeeded,
                        OnShowUnhandledTapUIIfNeeded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidgetHostViewAndroid::SyncCompositorOnMessageReceived(
    const IPC::Message& message) {
  DCHECK(!content_view_core_ || sync_compositor_) << !!content_view_core_;
  return sync_compositor_ && sync_compositor_->OnMessageReceived(message);
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

RenderWidgetHost*
RenderWidgetHostViewAndroid::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAndroid::WasResized() {
  if (delegated_frame_host_ && content_view_core_)
    delegated_frame_host_->UpdateContainerSizeinDIP(
        content_view_core_->GetViewportSizeDip());
  host_->WasResized();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  default_bounds_ = gfx::Rect(default_bounds_.origin(), size);
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  default_bounds_ = rect;
}

void RenderWidgetHostViewAndroid::GetScaledContentBitmap(
    float scale,
    SkColorType preferred_color_type,
    gfx::Rect src_subrect,
    const ReadbackRequestCallback& result_callback) {
  if (!host_ || host_->is_hidden() || !IsSurfaceAvailableForCopy()) {
    result_callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
    return;
  }
  gfx::Size bounds = current_surface_size_;
  if (src_subrect.IsEmpty())
    src_subrect = gfx::Rect(bounds);
  DCHECK_LE(src_subrect.width() + src_subrect.x(), bounds.width());
  DCHECK_LE(src_subrect.height() + src_subrect.y(), bounds.height());
  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  DCHECK_GT(device_scale_factor, 0);
  gfx::Size dst_size(
      gfx::ScaleToCeiledSize(src_subrect.size(), scale / device_scale_factor));
  src_subrect = gfx::ConvertRectToDIP(device_scale_factor, src_subrect);

  CopyFromCompositingSurface(src_subrect, dst_size, result_callback,
                             preferred_color_type);
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  if (!content_view_core_)
    return false;

  if (current_surface_size_.IsEmpty())
    return false;
  // This tell us whether a valid frame has arrived or not.
  if (!frame_evictor_->HasFrame())
    return false;

  DCHECK(!delegated_frame_host_ ||
         delegated_frame_host_->HasDelegatedContent());
  return true;
}

gfx::Vector2dF RenderWidgetHostViewAndroid::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() const {
  return &view_;
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::Focus() {
  host_->Focus();
  if (overscroll_controller_)
    overscroll_controller_->Enable();
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  if (!content_view_core_)
    return false;  // ContentViewCore not created yet.

  return content_view_core_->HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  return !using_browser_compositor_ || HasValidFrame();
}

void RenderWidgetHostViewAndroid::Show() {
  if (is_showing_)
    return;

  is_showing_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  is_showing_ = false;
  HideInternal();
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // ContentViewCoreImpl represents the native side of the Java
  // ContentViewCore.  It being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_showing_ && content_view_core_;
}

void RenderWidgetHostViewAndroid::LockCompositingSurface() {
  DCHECK(HasValidFrame());
  DCHECK(host_);
  DCHECK(frame_evictor_->HasFrame());
  frame_evictor_->LockFrame();
  locks_on_frame_count_++;
}

void RenderWidgetHostViewAndroid::UnlockCompositingSurface() {
  if (!frame_evictor_->HasFrame()) {
    DCHECK_EQ(locks_on_frame_count_, 0u);
    return;
  }

  DCHECK_GT(locks_on_frame_count_, 0u);
  locks_on_frame_count_--;
  frame_evictor_->UnlockFrame();

  if (locks_on_frame_count_ == 0) {
    if (last_frame_info_) {
      InternalSwapCompositorFrame(last_frame_info_->compositor_frame_sink_id,
                                  std::move(last_frame_info_->frame));
      last_frame_info_.reset();
    }

    view_.GetLayer()->SetHideLayerAndSubtree(!is_showing_);
  }
}

void RenderWidgetHostViewAndroid::OnShowUnhandledTapUIIfNeeded(int x_dip,
                                                               int y_dip) {
  if (!content_view_core_)
    return;
  // Validate the coordinates are within the viewport.
  gfx::Size viewport_size = content_view_core_->GetViewportSizeDip();
  if (x_dip < 0 || x_dip > viewport_size.width() ||
      y_dip < 0 || y_dip > viewport_size.height())
    return;
  content_view_core_->OnShowUnhandledTapUIIfNeeded(x_dip, y_dip);
}

void RenderWidgetHostViewAndroid::ReleaseLocksOnSurface() {
  if (!frame_evictor_->HasFrame()) {
    DCHECK_EQ(locks_on_frame_count_, 0u);
    return;
  }
  while (locks_on_frame_count_ > 0) {
    UnlockCompositingSurface();
  }
  RunAckCallbacks();
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  if (!content_view_core_)
    return default_bounds_;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableOSKOverscroll))
    return gfx::Rect(content_view_core_->GetViewSizeWithOSKHidden());

  return gfx::Rect(content_view_core_->GetViewSize());
}

gfx::Size RenderWidgetHostViewAndroid::GetVisibleViewportSize() const {
  if (!content_view_core_)
    return default_bounds_.size();

  return content_view_core_->GetViewSize();
}

gfx::Size RenderWidgetHostViewAndroid::GetPhysicalBackingSize() const {
  if (!content_view_core_) {
    if (default_bounds_.IsEmpty()) return gfx::Size();

    return gfx::Size(default_bounds_.right()
        * ui::GetScaleFactorForNativeView(GetNativeView()),
        default_bounds_.bottom()
        * ui::GetScaleFactorForNativeView(GetNativeView()));
  }

  return content_view_core_->GetPhysicalBackingSize();
}

bool RenderWidgetHostViewAndroid::DoBrowserControlsShrinkBlinkSize() const {
  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar.
  return content_view_core_ &&
         content_view_core_->DoBrowserControlsShrinkBlinkSize();
}

float RenderWidgetHostViewAndroid::GetTopControlsHeight() const {
  if (!content_view_core_)
    return default_bounds_.x();

  // The height of the browser controls.
  return content_view_core_->GetTopControlsHeightDip();
}

float RenderWidgetHostViewAndroid::GetBottomControlsHeight() const {
  if (!content_view_core_)
    return 0.f;

  // The height of the browser controls.
  return content_view_core_->GetBottomControlsHeightDip();
}

void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& cursor) {
  // There are no cursors on Android.
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

long RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<intptr_t>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::TextInputStateChanged(
    const TextInputState& params) {
  if (params.is_non_ime_change) {
    // Sends an acknowledgement to the renderer of a processed IME event.
    host_->Send(new InputMsg_ImeEventAck(host_->GetRoutingID()));
  }

  if (!content_view_core_)
    return;

  content_view_core_->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type), params.flags,
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed, params.is_non_ime_change);
}

void RenderWidgetHostViewAndroid::UpdateBackgroundColor(SkColor color) {
  if (cached_background_color_ == color)
    return;

  cached_background_color_ = color;

  if (delegated_frame_host_)
    delegated_frame_host_->UpdateBackgroundColor(color);

  if (content_view_core_)
    content_view_core_->OnBackgroundColorChanged(color);
}

void RenderWidgetHostViewAndroid::SetNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::SetNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  if (needs_begin_frames)
    RequestVSyncUpdate(PERSISTENT_BEGIN_FRAME);
  else
    outstanding_vsync_requests_ &= ~PERSISTENT_BEGIN_FRAME;
}

void RenderWidgetHostViewAndroid::OnStartContentIntent(
    const GURL& content_url, bool is_main_frame) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url, is_main_frame);
}

void RenderWidgetHostViewAndroid::OnSmartClipDataExtracted(
    const base::string16& text,
    const base::string16& html,
    const gfx::Rect rect) {
  if (content_view_core_)
    content_view_core_->OnSmartClipDataExtracted(text, html, rect);
}

bool RenderWidgetHostViewAndroid::OnTouchEvent(
    const ui::MotionEvent& event) {
  if (!host_)
    return false;

  ComputeEventLatencyOSTouchHistograms(event);

  // If a browser-based widget consumes the touch event, it's critical that
  // touch event interception be disabled. This avoids issues with
  // double-handling for embedder-detected gestures like side swipe.
  if (selection_controller_ &&
      selection_controller_->WillHandleTouchEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  if (stylus_text_selector_.OnTouchEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
      event, result.moved_beyond_slop_region);
  ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
  host_->ForwardTouchEventWithLatencyInfo(web_event, latency_info);

  // Send a proactive BeginFrame for this vsync to reduce scroll latency for
  // scroll-inducing touch events. Note that Android's Choreographer ensures
  // that BeginFrame requests made during ACTION_MOVE dispatch will be honored
  // in the same vsync phase.
  if (observing_root_window_ && result.moved_beyond_slop_region)
    RequestVSyncUpdate(BEGIN_FRAME);

  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchHandleEvent(
    const ui::MotionEvent& event) {
  return selection_controller_ &&
         selection_controller_->WillHandleTouchEvent(event);
}

void RenderWidgetHostViewAndroid::ResetGestureDetection() {
  const ui::MotionEvent* current_down_event =
      gesture_provider_.GetCurrentDownEvent();
  if (!current_down_event) {
    // A hard reset ensures prevention of any timer-based events that might fire
    // after a touch sequence has ended.
    gesture_provider_.ResetDetection();
    return;
  }

  std::unique_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
  if (gesture_provider_.OnTouchEvent(*cancel_event).succeeded) {
    bool causes_scrolling = false;
    ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
    latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
    host_->ForwardTouchEventWithLatencyInfo(
        ui::CreateWebTouchEventFromMotionEvent(*cancel_event, causes_scrolling),
        latency_info);
  }
}

void RenderWidgetHostViewAndroid::OnDidNavigateMainFrameToNewPage() {
  ResetGestureDetection();
}

void RenderWidgetHostViewAndroid::SetDoubleTapSupportEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void RenderWidgetHostViewAndroid::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}

void RenderWidgetHostViewAndroid::ImeCancelComposition() {
  ime_adapter_android_.CancelComposition();
}

void RenderWidgetHostViewAndroid::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  std::vector<gfx::RectF> character_bounds_float;
  for (const gfx::Rect& rect : character_bounds) {
    character_bounds_float.emplace_back(rect);
  }
  ime_adapter_android_.SetCharacterBounds(character_bounds_float);
}

void RenderWidgetHostViewAndroid::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  ime_adapter_android_.FocusedNodeChanged(is_editable_node);
}

void RenderWidgetHostViewAndroid::RenderProcessGone(
    base::TerminationStatus status, int error_code) {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  host_->ViewDestroyed();
  SetContentViewCore(NULL);
  delegated_frame_host_.reset();

  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;

  delete this;
}

void RenderWidgetHostViewAndroid::SetTooltipText(
    const base::string16& tooltip_text) {
  // Tooltips don't makes sense on Android.
}

void RenderWidgetHostViewAndroid::SelectionChanged(const base::string16& text,
                                                   size_t offset,
                                                   const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (!content_view_core_)
    return;
  if (range.is_empty()) {
    content_view_core_->OnSelectionChanged("");
    return;
  }

  DCHECK(!text.empty());
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  std::string utf8_selection = base::UTF16ToUTF8(text.substr(pos, n));

  content_view_core_->OnSelectionChanged(utf8_selection);
}

void RenderWidgetHostViewAndroid::SetBackgroundColor(SkColor color) {
  RenderWidgetHostViewBase::SetBackgroundColor(color);
  host_->SetBackgroundOpaque(GetBackgroundOpaque());
  UpdateBackgroundColor(color);
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::CopyFromCompositingSurface");
  if (!host_ || !IsSurfaceAvailableForCopy()) {
    callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
    return;
  }
  if (!(view_.GetWindowAndroid())) {
    callback.Run(SkBitmap(), READBACK_FAILED);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  gfx::Size dst_size_in_pixel =
      gfx::ConvertRectToPixel(device_scale_factor, gfx::Rect(dst_size)).size();
  gfx::Rect src_subrect_in_pixel =
      gfx::ConvertRectToPixel(device_scale_factor, src_subrect);

  if (!using_browser_compositor_) {
    SynchronousCopyContents(src_subrect_in_pixel, dst_size_in_pixel, callback,
                            preferred_color_type);
    UMA_HISTOGRAM_TIMES("Compositing.CopyFromSurfaceTimeSynchronous",
                        base::TimeTicks::Now() - start_time);
    return;
  }

  ui::WindowAndroidCompositor* compositor =
      view_.GetWindowAndroid()->GetCompositor();
  DCHECK(compositor);
  DCHECK(delegated_frame_host_);
  scoped_refptr<PendingReadbackLock> readback_lock(
      g_pending_readback_lock ? g_pending_readback_lock
                              : new PendingReadbackLock);
  delegated_frame_host_->RequestCopyOfSurface(
      compositor, src_subrect_in_pixel,
      base::Bind(&PrepareTextureCopyOutputResult, dst_size_in_pixel,
                 preferred_color_type, start_time, callback, readback_lock));
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(gfx::Rect(), false);
}

bool RenderWidgetHostViewAndroid::CanCopyToVideoFrame() const {
  return false;
}

void RenderWidgetHostViewAndroid::ShowDisambiguationPopup(
    const gfx::Rect& rect_pixels, const SkBitmap& zoomed_bitmap) {
  if (!content_view_core_)
    return;

  content_view_core_->ShowDisambiguationPopup(rect_pixels, zoomed_bitmap);
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAndroid::CreateSyntheticGestureTarget() {
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetAndroid(
          host_, content_view_core_->CreateMotionEventSynthesizer()));
}

void RenderWidgetHostViewAndroid::SendReclaimCompositorResources(
    uint32_t compositor_frame_sink_id,
    bool is_swap_ack) {
  DCHECK(host_);
  host_->Send(new ViewMsg_ReclaimCompositorResources(
      host_->GetRoutingID(), compositor_frame_sink_id, is_swap_ack,
      surface_returned_resources_));
  surface_returned_resources_.clear();
}

void RenderWidgetHostViewAndroid::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;
  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
  if (ack_callbacks_.empty())
    SendReclaimCompositorResources(last_compositor_frame_sink_id_,
                                   false /* is_swap_ack */);
}

void RenderWidgetHostViewAndroid::CheckCompositorFrameSinkChanged(
    uint32_t compositor_frame_sink_id) {
  if (compositor_frame_sink_id == last_compositor_frame_sink_id_)
    return;

  delegated_frame_host_->CompositorFrameSinkChanged();

  if (!surface_returned_resources_.empty())
    SendReclaimCompositorResources(last_compositor_frame_sink_id_,
                                   false /* is_swap_ack */);

  last_compositor_frame_sink_id_ = compositor_frame_sink_id;
}

void RenderWidgetHostViewAndroid::InternalSwapCompositorFrame(
    uint32_t compositor_frame_sink_id,
    cc::CompositorFrame frame) {
  last_scroll_offset_ = frame.metadata.root_scroll_offset;
  DCHECK(frame.delegated_frame_data);
  DCHECK(delegated_frame_host_);

  if (locks_on_frame_count_ > 0) {
    DCHECK(HasValidFrame());
    RetainFrame(compositor_frame_sink_id, std::move(frame));
    return;
  }

  DCHECK(!frame.delegated_frame_data->render_pass_list.empty());

  cc::RenderPass* root_pass =
      frame.delegated_frame_data->render_pass_list.back().get();
  current_surface_size_ = root_pass->output_rect.size();
  bool is_transparent = root_pass->has_transparent_background;

  cc::CompositorFrameMetadata metadata = frame.metadata.Clone();

  CheckCompositorFrameSinkChanged(compositor_frame_sink_id);
  bool has_content = !current_surface_size_.IsEmpty();

  base::Closure ack_callback =
      base::Bind(&RenderWidgetHostViewAndroid::SendReclaimCompositorResources,
                 weak_ptr_factory_.GetWeakPtr(), compositor_frame_sink_id,
                 true /* is_swap_ack */);

  ack_callbacks_.push(ack_callback);

  if (!has_content) {
    DestroyDelegatedContent();
  } else {
    cc::SurfaceFactory::DrawCallback ack_callback =
        base::Bind(&RenderWidgetHostViewAndroid::RunAckCallbacks,
                   weak_ptr_factory_.GetWeakPtr());
    delegated_frame_host_->SubmitCompositorFrame(std::move(frame),
                                                 ack_callback);
    frame_evictor_->SwappedFrame(!host_->is_hidden());
  }

  if (host_->is_hidden())
    RunAckCallbacks();

  // As the metadata update may trigger view invalidation, always call it after
  // any potential compositor scheduling.
  OnFrameMetadataUpdated(std::move(metadata), is_transparent);
}

void RenderWidgetHostViewAndroid::DestroyDelegatedContent() {
  DCHECK(!delegated_frame_host_ ||
         delegated_frame_host_->HasDelegatedContent() ==
             frame_evictor_->HasFrame());
  DCHECK_EQ(locks_on_frame_count_, 0u);

  if (!delegated_frame_host_)
    return;

  if (!delegated_frame_host_->HasDelegatedContent())
    return;

  frame_evictor_->DiscardedFrame();
  delegated_frame_host_->DestroyDelegatedContent();
}

void RenderWidgetHostViewAndroid::OnSwapCompositorFrame(
    uint32_t compositor_frame_sink_id,
    cc::CompositorFrame frame) {
  InternalSwapCompositorFrame(compositor_frame_sink_id, std::move(frame));
}

void RenderWidgetHostViewAndroid::ClearCompositorFrame() {
  DestroyDelegatedContent();
}

void RenderWidgetHostViewAndroid::RetainFrame(uint32_t compositor_frame_sink_id,
                                              cc::CompositorFrame frame) {
  DCHECK(locks_on_frame_count_);

  // Store the incoming frame so that it can be swapped when all the locks have
  // been released. If there is already a stored frame, then replace and skip
  // the previous one but make sure we still eventually send the ACK. Holding
  // the ACK also blocks the renderer when its max_frames_pending is reached.
  if (last_frame_info_) {
    base::Closure ack_callback = base::Bind(
        &RenderWidgetHostViewAndroid::SendReclaimCompositorResources,
        weak_ptr_factory_.GetWeakPtr(),
        last_frame_info_->compositor_frame_sink_id, true /* is_swap_ack */);

    ack_callbacks_.push(ack_callback);
  }

  last_frame_info_.reset(
      new LastFrameInfo(compositor_frame_sink_id, std::move(frame)));
}

void RenderWidgetHostViewAndroid::SynchronousFrameMetadata(
    cc::CompositorFrameMetadata frame_metadata) {
  if (!content_view_core_)
    return;

  bool is_mobile_optimized = IsMobileOptimizedFrame(frame_metadata);

  if (host_ && host_->input_router()) {
    host_->input_router()->NotifySiteIsMobileOptimized(
        is_mobile_optimized);
  }

  // This is a subset of OnSwapCompositorFrame() used in the synchronous
  // compositor flow.
  OnFrameMetadataUpdated(frame_metadata.Clone(), false);

  // DevTools ScreenCast support for Android WebView.
  RenderFrameHost* frame_host = RenderViewHost::From(host_)->GetMainFrame();
  if (frame_host) {
    RenderFrameDevToolsAgentHost::SignalSynchronousSwapCompositorFrame(
        frame_host,
        std::move(frame_metadata));
  }
}

void RenderWidgetHostViewAndroid::SetSynchronousCompositorClient(
      SynchronousCompositorClient* client) {
  synchronous_compositor_client_ = client;
  if (!sync_compositor_ && synchronous_compositor_client_) {
    sync_compositor_ = SynchronousCompositorHost::Create(this);
  }
}

bool RenderWidgetHostViewAndroid::SupportsAnimation() const {
  // The synchronous (WebView) compositor does not have a proper browser
  // compositor with which to drive animations.
  return using_browser_compositor_;
}

void RenderWidgetHostViewAndroid::SetNeedsAnimate() {
  DCHECK(view_.GetWindowAndroid());
  DCHECK(using_browser_compositor_);
  view_.GetWindowAndroid()->SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::PointF& position) {
  MoveCaret(gfx::Point(position.x(), position.y()));
}

void RenderWidgetHostViewAndroid::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  DCHECK(content_view_core_);
  content_view_core_->MoveRangeSelectionExtent(extent);
}

void RenderWidgetHostViewAndroid::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  DCHECK(content_view_core_);
  content_view_core_->SelectBetweenCoordinates(base, extent);
}

void RenderWidgetHostViewAndroid::OnSelectionEvent(
    ui::SelectionEventType event) {
  DCHECK(content_view_core_);
  DCHECK(selection_controller_);
  // If a selection drag has started, it has taken over the active touch
  // sequence. Immediately cancel gesture detection and any downstream touch
  // listeners (e.g., web content) to communicate this transfer.
  if (event == ui::SELECTION_HANDLES_SHOWN &&
      gesture_provider_.GetCurrentDownEvent()) {
    ResetGestureDetection();
  }
  content_view_core_->OnSelectionEvent(
      event, selection_controller_->GetStartPosition(),
      GetSelectionRect(*selection_controller_));
}

std::unique_ptr<ui::TouchHandleDrawable>
RenderWidgetHostViewAndroid::CreateDrawable() {
  DCHECK(content_view_core_);
  if (!using_browser_compositor_) {
    if (!sync_compositor_)
      return nullptr;
    return std::unique_ptr<ui::TouchHandleDrawable>(
        sync_compositor_->client()->CreateDrawable());
  }
  base::android::ScopedJavaLocalRef<jobject> activityContext =
      content_view_core_->GetContext();
  return std::unique_ptr<ui::TouchHandleDrawable>(
      new CompositedTouchHandleDrawable(
          content_view_core_->GetViewAndroid()->GetLayer(),
          ui::GetScaleFactorForNativeView(GetNativeView()),
          // Use the activity context where possible (instead of the application
          // context) to ensure proper handle theming.
          activityContext.is_null() ? base::android::GetApplicationContext()
                                    : activityContext));
}

void RenderWidgetHostViewAndroid::SynchronousCopyContents(
    const gfx::Rect& src_subrect_in_pixel,
    const gfx::Size& dst_size_in_pixel,
    const ReadbackRequestCallback& callback,
    const SkColorType color_type) {
  gfx::Size input_size_in_pixel;
  if (src_subrect_in_pixel.IsEmpty())
    input_size_in_pixel = current_surface_size_;
  else
    input_size_in_pixel = src_subrect_in_pixel.size();

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = input_size_in_pixel;
  else
    output_size_in_pixel = dst_size_in_pixel;
  int output_width = output_size_in_pixel.width();
  int output_height = output_size_in_pixel.height();

  if (!sync_compositor_) {
    callback.Run(SkBitmap(), READBACK_FAILED);
    return;
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(output_width,
                                       output_height,
                                       color_type,
                                       kPremul_SkAlphaType));
  SkCanvas canvas(bitmap);
  canvas.scale(
      (float)output_width / (float)input_size_in_pixel.width(),
      (float)output_height / (float)input_size_in_pixel.height());
  sync_compositor_->DemandDrawSw(&canvas);
  callback.Run(bitmap, READBACK_SUCCESS);
}

void RenderWidgetHostViewAndroid::OnFrameMetadataUpdated(
    const cc::CompositorFrameMetadata& frame_metadata,
    bool is_transparent) {
  bool is_mobile_optimized = IsMobileOptimizedFrame(frame_metadata);
  gesture_provider_.SetDoubleTapSupportForPageEnabled(!is_mobile_optimized);

  if (!content_view_core_)
    return;

  if (overscroll_controller_)
    overscroll_controller_->OnFrameMetadataUpdated(frame_metadata);

  if (selection_controller_) {
    selection_controller_->OnSelectionEditable(
        frame_metadata.selection.is_editable);
    selection_controller_->OnSelectionEmpty(
        frame_metadata.selection.is_empty_text_form_control);
    selection_controller_->OnSelectionBoundsChanged(
        frame_metadata.selection.start, frame_metadata.selection.end);

    // Set parameters for adaptive handle orientation.
    gfx::SizeF viewport_size(frame_metadata.scrollable_viewport_size);
    viewport_size.Scale(frame_metadata.page_scale_factor);
    gfx::RectF viewport_rect(0.0f, frame_metadata.top_controls_height *
                                       frame_metadata.top_controls_shown_ratio,
                             viewport_size.width(), viewport_size.height());
    selection_controller_->OnViewportChanged(viewport_rect);
  }

  UpdateBackgroundColor(is_transparent ? SK_ColorTRANSPARENT
                                       : frame_metadata.root_background_color);

  // All offsets and sizes are in CSS pixels.
  content_view_core_->UpdateFrameInfo(
      frame_metadata.root_scroll_offset,
      frame_metadata.page_scale_factor,
      gfx::Vector2dF(frame_metadata.min_page_scale_factor,
                     frame_metadata.max_page_scale_factor),
      frame_metadata.root_layer_size,
      frame_metadata.scrollable_viewport_size,
      frame_metadata.top_controls_height,
      frame_metadata.top_controls_shown_ratio,
      frame_metadata.bottom_controls_height,
      frame_metadata.bottom_controls_shown_ratio,
      is_mobile_optimized,
      frame_metadata.selection.start);
}

void RenderWidgetHostViewAndroid::ShowInternal() {
  bool show = is_showing_ && is_window_activity_started_ && is_window_visible_;
  if (!show)
    return;

  if (!host_ || !host_->is_hidden())
    return;

  view_.GetLayer()->SetHideLayerAndSubtree(false);

  frame_evictor_->SetVisible(true);

  if (overscroll_controller_)
    overscroll_controller_->Enable();

  host_->WasShown(ui::LatencyInfo());

  if (content_view_core_) {
    StartObservingRootWindow();
    RequestVSyncUpdate(BEGIN_FRAME);
  }
}

void RenderWidgetHostViewAndroid::HideInternal() {
  DCHECK(!is_showing_ || !is_window_activity_started_ || !is_window_visible_)
      << "Hide called when the widget should be shown.";

  // Only preserve the frontbuffer if the activity was stopped while the
  // window is still visible. This avoids visual artificts when transitioning
  // between activities.
  bool hide_frontbuffer = is_window_activity_started_ || !is_window_visible_;

  // Only stop observing the root window if the widget has been explicitly
  // hidden and the frontbuffer is being cleared. This allows window visibility
  // notifications to eventually clear the frontbuffer.
  bool stop_observing_root_window = !is_showing_ && hide_frontbuffer;

  if (hide_frontbuffer) {
    if (locks_on_frame_count_ == 0)
      view_.GetLayer()->SetHideLayerAndSubtree(true);

    frame_evictor_->SetVisible(false);
  }

  if (stop_observing_root_window) {
    DCHECK(!is_showing_);
    StopObservingRootWindow();
  }

  if (!host_ || host_->is_hidden())
    return;

  if (overscroll_controller_)
    overscroll_controller_->Disable();

  RunAckCallbacks();

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();
}

void RenderWidgetHostViewAndroid::RequestVSyncUpdate(uint32_t requests) {
  bool should_request_vsync = !outstanding_vsync_requests_ && requests;
  outstanding_vsync_requests_ |= requests;

  // Note that if we're not currently observing the root window, outstanding
  // vsync requests will be pushed if/when we resume observing in
  // |StartObservingRootWindow()|.
  if (observing_root_window_ && should_request_vsync) {
    ui::WindowAndroid* windowAndroid = view_.GetWindowAndroid();
    DCHECK(windowAndroid);
    // TODO(boliu): This check should be redundant with
    // |observing_root_window_| check above. However we are receiving trickle
    // of crash reports (crbug.com/639868) with no root cause. Should
    // investigate more when time allows what corner case is missed.
    if (windowAndroid)
      windowAndroid->RequestVSyncUpdate();
  }
}

void RenderWidgetHostViewAndroid::StartObservingRootWindow() {
  DCHECK(content_view_core_);
  // TODO(yusufo): This will need to have a better fallback for cases where
  // setContentViewCore is called with a valid ContentViewCore without a window.
  DCHECK(view_.GetWindowAndroid());
  DCHECK(is_showing_);
  if (observing_root_window_)
    return;

  observing_root_window_ = true;
  if (host_)
    host_->Send(new ViewMsg_SetBeginFramePaused(host_->GetRoutingID(), false));
  view_.GetWindowAndroid()->AddObserver(this);

  // Clear existing vsync requests to allow a request to the new window.
  uint32_t outstanding_vsync_requests = outstanding_vsync_requests_;
  outstanding_vsync_requests_ = 0;
  RequestVSyncUpdate(outstanding_vsync_requests);

  ui::WindowAndroidCompositor* compositor =
      view_.GetWindowAndroid()->GetCompositor();
  if (compositor) {
    delegated_frame_host_->RegisterFrameSinkHierarchy(
        compositor->GetFrameSinkId());
  }
}

void RenderWidgetHostViewAndroid::StopObservingRootWindow() {
  if (!(view_.GetWindowAndroid())) {
    DCHECK(!observing_root_window_);
    return;
  }

  if (!observing_root_window_)
    return;

  // Reset window state variables to their defaults.
  is_window_activity_started_ = true;
  is_window_visible_ = true;
  observing_root_window_ = false;
  if (host_)
    host_->Send(new ViewMsg_SetBeginFramePaused(host_->GetRoutingID(), true));
  view_.GetWindowAndroid()->RemoveObserver(this);
  // If the DFH has already been destroyed, it will have cleaned itself up.
  // This happens in some WebView cases.
  if (delegated_frame_host_)
    delegated_frame_host_->UnregisterFrameSinkHierarchy();
}

void RenderWidgetHostViewAndroid::SendBeginFrame(base::TimeTicks frame_time,
                                                 base::TimeDelta vsync_period) {
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::SendBeginFrame",
               "frame_time_us", frame_time.ToInternalValue());

  // Synchronous compositor does not use deadline-based scheduling.
  // TODO(brianderson): Replace this hardcoded deadline after Android
  // switches to Surfaces and the Browser's commit isn't in the critcal path.
  base::TimeTicks deadline =
      sync_compositor_ ? base::TimeTicks() : frame_time + (vsync_period * 0.6);
  host_->Send(new ViewMsg_BeginFrame(
      host_->GetRoutingID(),
      cc::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, frame_time, deadline,
                                 vsync_period, cc::BeginFrameArgs::NORMAL)));
  if (sync_compositor_)
    sync_compositor_->DidSendBeginFrame(view_.GetWindowAndroid());
}

bool RenderWidgetHostViewAndroid::Animate(base::TimeTicks frame_time) {
  bool needs_animate = false;
  if (overscroll_controller_) {
    needs_animate |= overscroll_controller_->Animate(
        frame_time, content_view_core_->GetViewAndroid()->GetLayer());
  }
  if (selection_controller_)
    needs_animate |= selection_controller_->Animate(frame_time);
  return needs_animate;
}

void RenderWidgetHostViewAndroid::RequestDisallowInterceptTouchEvent() {
  if (content_view_core_)
    content_view_core_->RequestDisallowInterceptTouchEvent();
}

void RenderWidgetHostViewAndroid::EvictDelegatedFrame() {
  DestroyDelegatedContent();
}

bool RenderWidgetHostViewAndroid::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  NOTREACHED();
  return false;
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  const bool event_consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  gesture_provider_.OnTouchEventAck(touch.event.uniqueTouchEventId,
                                    event_consumed);
}

void RenderWidgetHostViewAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (overscroll_controller_)
    overscroll_controller_->OnGestureEventAck(event, ack_result);

  if (content_view_core_)
    content_view_core_->OnGestureEventAck(event, ack_result);
}

InputEventAckState RenderWidgetHostViewAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (selection_controller_ &&
      blink::WebInputEvent::isGestureEventType(input_event.type)) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    switch (gesture_event.type) {
      case blink::WebInputEvent::GestureLongPress:
        if (selection_controller_->WillHandleLongPressEvent(
                base::TimeTicks() +
                    base::TimeDelta::FromSecondsD(input_event.timeStampSeconds),
                gfx::PointF(gesture_event.x, gesture_event.y))) {
          return INPUT_EVENT_ACK_STATE_CONSUMED;
        }
        break;

      case blink::WebInputEvent::GestureTap:
        if (selection_controller_->WillHandleTapEvent(
                gfx::PointF(gesture_event.x, gesture_event.y),
                gesture_event.data.tap.tapCount)) {
          return INPUT_EVENT_ACK_STATE_CONSUMED;
        }
        break;

      case blink::WebInputEvent::GestureScrollBegin:
        selection_controller_->OnScrollBeginEvent();
        break;

      default:
        break;
    }
  }

  if (overscroll_controller_ &&
      blink::WebInputEvent::isGestureEventType(input_event.type) &&
      overscroll_controller_->WillHandleGestureEvent(
          static_cast<const blink::WebGestureEvent&>(input_event))) {
    return INPUT_EVENT_ACK_STATE_CONSUMED;
  }

  if (content_view_core_ && content_view_core_->FilterInputEvent(input_event))
    return INPUT_EVENT_ACK_STATE_CONSUMED;

  if (!host_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  if (input_event.type == blink::WebInputEvent::GestureTapDown ||
      input_event.type == blink::WebInputEvent::TouchStart) {
    GpuDataManagerImpl* gpu_data = GpuDataManagerImpl::GetInstance();
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim && gpu_data &&
        gpu_data->IsDriverBugWorkaroundActive(gpu::WAKE_UP_GPU_BEFORE_DRAWING))
      shim->Send(new GpuMsg_WakeUpGpu);
  }

  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostViewAndroid::OnSetNeedsFlushInput() {
  TRACE_EVENT0("input", "RenderWidgetHostViewAndroid::OnSetNeedsFlushInput");
  RequestVSyncUpdate(FLUSH_INPUT);
}

BrowserAccessibilityManager*
    RenderWidgetHostViewAndroid::CreateBrowserAccessibilityManager(
        BrowserAccessibilityDelegate* delegate, bool for_root_frame) {
  base::android::ScopedJavaLocalRef<jobject> content_view_core_obj;
  if (for_root_frame && host_ && content_view_core_)
    content_view_core_obj = content_view_core_->GetJavaObject();
  return new BrowserAccessibilityManagerAndroid(
      content_view_core_obj,
      BrowserAccessibilityManagerAndroid::GetEmptyDocument(),
      delegate);
}

bool RenderWidgetHostViewAndroid::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::UnlockMouse() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host_)
    return;

  RenderWidgetHostImpl* target_host = host_;

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (host_->delegate())
    target_host = host_->delegate()->GetFocusedRenderWidgetHost(host_);
  if (!target_host)
    return;

  target_host->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewAndroid::SendMouseEvent(
    const ui::MotionEventAndroid& motion_event,
    int changed_button) {
  blink::WebInputEvent::Type webMouseEventType =
      ui::ToWebMouseEventType(motion_event.GetAction());

  blink::WebMouseEvent mouse_event = WebMouseEventBuilder::Build(
      webMouseEventType,
      ui::EventTimeStampToSeconds(motion_event.GetEventTime()),
      motion_event.GetX(0),
      motion_event.GetY(0),
      motion_event.GetFlags(),
      motion_event.GetButtonState() ? 1 : 0 /* click count */,
      motion_event.GetPointerId(0),
      motion_event.GetPressure(0),
      motion_event.GetOrientation(0),
      motion_event.GetTilt(0),
      changed_button,
      motion_event.GetToolType(0));

  if (host_)
    host_->ForwardMouseEvent(mouse_event);
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (host_) {
    ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
    latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
    host_->ForwardWheelEventWithLatencyInfo(event, latency_info);
  }
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_controller_)
    overscroll_controller_->Enable();

  if (host_) {
    ui::LatencyInfo latency_info =
        ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event);
    host_->ForwardGestureEventWithLatencyInfo(event, latency_info);
  }
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host_)
    host_->MoveCaret(point);
}

void RenderWidgetHostViewAndroid::DismissTextHandles() {
  if (selection_controller_)
    selection_controller_->HideAndDisallowShowingAutomatically();
}

void RenderWidgetHostViewAndroid::SetTextHandlesTemporarilyHidden(bool hidden) {
  if (selection_controller_)
    selection_controller_->SetTemporarilyHidden(hidden);
}

SkColor RenderWidgetHostViewAndroid::GetCachedBackgroundColor() const {
  return cached_background_color_;
}

void RenderWidgetHostViewAndroid::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (sync_compositor_)
    sync_compositor_->DidOverscroll(params);

  if (!content_view_core_ || !is_showing_)
    return;

  if (overscroll_controller_)
    overscroll_controller_->OnOverscrolled(params);
}

void RenderWidgetHostViewAndroid::DidStopFlinging() {
  if (content_view_core_)
    content_view_core_->DidStopFlinging();
}

cc::FrameSinkId RenderWidgetHostViewAndroid::GetFrameSinkId() {
  if (!delegated_frame_host_)
    return cc::FrameSinkId();

  return delegated_frame_host_->GetFrameSinkId();
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  DCHECK(!content_view_core || !content_view_core_ ||
         (content_view_core_ == content_view_core));
  StopObservingRootWindow();

  bool resize = false;
  if (content_view_core != content_view_core_) {
    overscroll_controller_.reset();
    selection_controller_.reset();
    ReleaseLocksOnSurface();
    // TODO(yusufo) : Get rid of the below conditions and have a better handling
    // for resizing after crbug.com/628302 is handled.
    bool is_size_initialized = !content_view_core
        || content_view_core->GetViewportSizeDip().width() != 0
        || content_view_core->GetViewportSizeDip().height() != 0;
    if (content_view_core_ || is_size_initialized)
      resize = true;
    if (content_view_core_) {
      content_view_core_->RemoveObserver(this);
      view_.RemoveFromParent();
      view_.GetLayer()->RemoveFromParent();
    }
    if (content_view_core) {
      content_view_core->AddObserver(this);
      ui::ViewAndroid* parent_view = content_view_core->GetViewAndroid();
      parent_view->AddChild(&view_);
      parent_view->GetLayer()->AddChild(view_.GetLayer());
    }
    content_view_core_ = content_view_core;
  }

  BrowserAccessibilityManager* manager = NULL;
  if (host_)
    manager = host_->GetRootBrowserAccessibilityManager();
  if (manager) {
    base::android::ScopedJavaLocalRef<jobject> obj;
    if (content_view_core_)
      obj = content_view_core_->GetJavaObject();
    manager->ToBrowserAccessibilityManagerAndroid()->SetContentViewCore(obj);
  }

  if (!content_view_core_) {
    sync_compositor_.reset();
    return;
  }

  if (is_showing_)
    StartObservingRootWindow();

  if (resize)
    WasResized();

  if (!selection_controller_)
    selection_controller_ = CreateSelectionController(this, content_view_core_);

  if (!overscroll_controller_ &&
      view_.GetWindowAndroid()->GetCompositor()) {
    overscroll_controller_ = CreateOverscrollController(
        content_view_core_, ui::GetScaleFactorForNativeView(GetNativeView()));
  }
}

void RenderWidgetHostViewAndroid::RunAckCallbacks() {
  while (!ack_callbacks_.empty()) {
    ack_callbacks_.front().Run();
    ack_callbacks_.pop();
  }
}

void RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(jdduke): Remove this workaround after Android fixes UiAutomator to
  // stop providing shift meta values to synthetic MotionEvents. This prevents
  // unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.type == blink::WebInputEvent::GestureTap &&
      web_gesture.modifiers == blink::WebInputEvent::ShiftKey) {
    web_gesture.modifiers = 0;
  }
  SendGestureEvent(web_gesture);
}

void RenderWidgetHostViewAndroid::OnContentViewCoreDestroyed() {
  SetContentViewCore(NULL);
}

void RenderWidgetHostViewAndroid::OnCompositingDidCommit() {
  RunAckCallbacks();
}

void RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged(bool visible) {
  TRACE_EVENT1("browser",
               "RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged",
               "visible", visible);
  DCHECK(observing_root_window_);
  if (is_window_visible_ == visible)
    return;

  is_window_visible_ = visible;

  if (visible)
    ShowInternal();
  else
    HideInternal();
}

void RenderWidgetHostViewAndroid::OnAttachedToWindow() {
  if (is_showing_)
    StartObservingRootWindow();
  DCHECK(view_.GetWindowAndroid());
  if (view_.GetWindowAndroid()->GetCompositor())
    OnAttachCompositor();
}

void RenderWidgetHostViewAndroid::OnDetachedFromWindow() {
  StopObservingRootWindow();
  OnDetachCompositor();
}

void RenderWidgetHostViewAndroid::OnAttachCompositor() {
  DCHECK(content_view_core_);
  if (!overscroll_controller_)
    overscroll_controller_ = CreateOverscrollController(
        content_view_core_, ui::GetScaleFactorForNativeView(GetNativeView()));
  ui::WindowAndroidCompositor* compositor =
      view_.GetWindowAndroid()->GetCompositor();
  delegated_frame_host_->RegisterFrameSinkHierarchy(
      compositor->GetFrameSinkId());
}

void RenderWidgetHostViewAndroid::OnDetachCompositor() {
  DCHECK(content_view_core_);
  DCHECK(using_browser_compositor_);
  RunAckCallbacks();
  overscroll_controller_.reset();
  delegated_frame_host_->UnregisterFrameSinkHierarchy();
}

void RenderWidgetHostViewAndroid::OnVSync(base::TimeTicks frame_time,
                                          base::TimeDelta vsync_period) {
  TRACE_EVENT0("cc,benchmark", "RenderWidgetHostViewAndroid::OnVSync");
  if (!host_)
    return;

  if (outstanding_vsync_requests_ & FLUSH_INPUT) {
    outstanding_vsync_requests_ &= ~FLUSH_INPUT;
    host_->FlushInput();
  }

  if (outstanding_vsync_requests_ & BEGIN_FRAME ||
      outstanding_vsync_requests_ & PERSISTENT_BEGIN_FRAME) {
    outstanding_vsync_requests_ &= ~BEGIN_FRAME;
    SendBeginFrame(frame_time, vsync_period);
  }

  // This allows for SendBeginFrame and FlushInput to modify
  // outstanding_vsync_requests.
  uint32_t outstanding_vsync_requests = outstanding_vsync_requests_;
  outstanding_vsync_requests_ = 0;
  RequestVSyncUpdate(outstanding_vsync_requests);
}

void RenderWidgetHostViewAndroid::OnAnimate(base::TimeTicks begin_frame_time) {
  if (Animate(begin_frame_time))
    SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::OnActivityStopped() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStopped");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = false;
  HideInternal();
}

void RenderWidgetHostViewAndroid::OnActivityStarted() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStarted");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::OnLostResources() {
  ReleaseLocksOnSurface();
  DestroyDelegatedContent();
  DCHECK(ack_callbacks_.empty());
}

void RenderWidgetHostViewAndroid::OnStylusSelectBegin(float x0,
                                                      float y0,
                                                      float x1,
                                                      float y1) {
  SelectBetweenCoordinates(gfx::PointF(x0, y0), gfx::PointF(x1, y1));
}

void RenderWidgetHostViewAndroid::OnStylusSelectUpdate(float x, float y) {
  MoveRangeSelectionExtent(gfx::PointF(x, y));
}

void RenderWidgetHostViewAndroid::OnStylusSelectEnd() {
  if (selection_controller_)
    selection_controller_->AllowShowingFromCurrentSelection();
}

void RenderWidgetHostViewAndroid::OnStylusSelectTap(base::TimeTicks time,
                                                    float x,
                                                    float y) {
  // Treat the stylus tap as a long press, activating either a word selection or
  // context menu depending on the targetted content.
  blink::WebGestureEvent long_press = WebGestureEventBuilder::Build(
      blink::WebInputEvent::GestureLongPress,
      (time - base::TimeTicks()).InSecondsF(), x, y);
  SendGestureEvent(long_press);
}

void RenderWidgetHostViewAndroid::ComputeEventLatencyOSTouchHistograms(
      const ui::MotionEvent& event) {
  base::TimeTicks event_time = event.GetEventTime();
  base::TimeDelta delta = base::TimeTicks::Now() - event_time;
  switch (event.GetAction()) {
    case ui::MotionEvent::ACTION_DOWN:
    case ui::MotionEvent::ACTION_POINTER_DOWN:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_PRESSED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    case ui::MotionEvent::ACTION_MOVE:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_MOVED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    case ui::MotionEvent::ACTION_UP:
    case ui::MotionEvent::ACTION_POINTER_UP:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_RELEASED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
    default:
      return;
  }
}

}  // namespace content
