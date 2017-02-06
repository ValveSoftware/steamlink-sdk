// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <stddef.h>

#include <algorithm>
#include <deque>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/context_provider.h"
#include "cc/output/latency_info_swap_promise.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"

namespace {

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 200.0;

bool IsRunningInMojoShell(base::CommandLine* command_line) {
  const char kMojoShellFlag[] = "mojo-platform-channel-handle";
  return command_line->HasSwitch(kMojoShellFlag);
}

}  // namespace

namespace ui {

CompositorLock::CompositorLock(Compositor* compositor)
    : compositor_(compositor) {
  if (compositor_->locks_will_time_out_) {
    compositor_->task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CompositorLock::CancelLock, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
  }
}

CompositorLock::~CompositorLock() {
  CancelLock();
}

void CompositorLock::CancelLock() {
  if (!compositor_)
    return;
  compositor_->UnlockCompositor();
  compositor_ = NULL;
}

Compositor::Compositor(ui::ContextFactory* context_factory,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_factory_(context_factory),
      root_layer_(NULL),
      widget_(gfx::kNullAcceleratedWidget),
      widget_valid_(false),
      output_surface_requested_(false),
      surface_id_allocator_(context_factory->CreateSurfaceIdAllocator()),
      task_runner_(task_runner),
      vsync_manager_(new CompositorVSyncManager()),
      device_scale_factor_(0.0f),
      locks_will_time_out_(true),
      compositor_lock_(NULL),
      layer_animator_collection_(this),
      weak_ptr_factory_(this) {
  root_web_layer_ = cc::Layer::Create();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // This will ensure PictureLayers always can have LCD text, to match the
  // previous behaviour with ContentLayers, where LCD-not-allowed notifications
  // were ignored.
  settings.layers_always_allowed_lcd_text = true;
  // Use occlusion to allow more overlapping windows to take less memory.
  settings.use_occlusion_for_tile_prioritization = true;
  settings.renderer_settings.refresh_rate =
      context_factory_->DoesCreateTestContexts() ? kTestRefreshRate
                                                 : kDefaultRefreshRate;
  settings.main_frame_before_activation_enabled = false;
  if (command_line->HasSwitch(switches::kDisableGpuVsync)) {
    std::string display_vsync_string =
        command_line->GetSwitchValueASCII(switches::kDisableGpuVsync);
    if (display_vsync_string == "gpu") {
      settings.renderer_settings.disable_display_vsync = true;
    } else if (display_vsync_string == "beginframe") {
      settings.wait_for_beginframe_interval = false;
    } else {
      settings.renderer_settings.disable_display_vsync = true;
      settings.wait_for_beginframe_interval = false;
    }
  }
  settings.renderer_settings.partial_swap_enabled =
      !command_line->HasSwitch(switches::kUIDisablePartialSwap);
#if defined(OS_WIN)
  settings.renderer_settings.finish_rendering_on_resize = true;
#elif defined(OS_MACOSX)
  settings.renderer_settings.release_overlay_resources_after_gpu_query = true;
#endif

  // These flags should be mirrored by renderer versions in content/renderer/.
  settings.initial_debug_state.show_debug_borders =
      command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders);
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      command_line->HasSwitch(cc::switches::kUIShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      command_line->HasSwitch(switches::kUIShowPaintRects);
  settings.initial_debug_state.show_property_changed_rects =
      command_line->HasSwitch(cc::switches::kUIShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      command_line->HasSwitch(cc::switches::kUIShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      command_line->HasSwitch(cc::switches::kUIShowScreenSpaceRects);
  settings.initial_debug_state.show_replica_screen_space_rects =
      command_line->HasSwitch(cc::switches::kUIShowReplicaScreenSpaceRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));

  settings.use_zero_copy = IsUIZeroCopyEnabled();

  if (command_line->HasSwitch(switches::kUIEnableRGBA4444Textures))
    settings.renderer_settings.preferred_tile_format = cc::RGBA_4444;

  settings.use_layer_lists =
      command_line->HasSwitch(cc::switches::kUIEnableLayerLists);

  // UI compositor always uses partial raster if not using zero-copy. Zero copy
  // doesn't currently support partial raster.
  settings.use_partial_raster = !settings.use_zero_copy;

  // Use CPU_READ_WRITE_PERSISTENT memory buffers to support partial tile
  // raster if needed.
  gfx::BufferUsage usage =
      settings.use_partial_raster
          ? gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT
          : gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;

  for (size_t format = 0;
      format < static_cast<size_t>(gfx::BufferFormat::LAST) + 1; format++) {
    DCHECK_GT(settings.use_image_texture_targets.size(), format);
    settings.use_image_texture_targets[format] =
        context_factory_->GetImageTextureTarget(
            static_cast<gfx::BufferFormat>(format), usage);
  }

  // Note: Only enable image decode tasks if we have more than one worker
  // thread.
  settings.image_decode_tasks_enabled = false;

  // TODO(crbug.com/603600): This should always be turned on once mus tells its
  // clients about BeginFrame.
  settings.use_output_surface_begin_frame_source =
      !IsRunningInMojoShell(command_line);

#if !defined(OS_ANDROID)
  // TODO(sohanjg): Revisit this memory usage in tile manager.
  cc::ManagedMemoryPolicy policy(
      512 * 1024 * 1024, gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
      settings.memory_policy_.num_resources_limit);
  settings.memory_policy_ = policy;
#endif

  base::TimeTicks before_create = base::TimeTicks::Now();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.shared_bitmap_manager = context_factory_->GetSharedBitmapManager();
  params.gpu_memory_buffer_manager =
      context_factory_->GetGpuMemoryBufferManager();
  params.task_graph_runner = context_factory_->GetTaskGraphRunner();
  params.settings = &settings;
  params.main_task_runner = task_runner_;
  params.animation_host = cc::AnimationHost::CreateMainInstance();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  UMA_HISTOGRAM_TIMES("GPU.CreateBrowserCompositor",
                      base::TimeTicks::Now() - before_create);

  animation_timeline_ =
      cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId());
  host_->animation_host()->AddAnimationTimeline(animation_timeline_.get());

  host_->SetRootLayer(root_web_layer_);
  host_->set_surface_id_namespace(surface_id_allocator_->id_namespace());
  host_->SetVisible(true);
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown", "Compositor::destructor");

  CancelCompositorLock();
  DCHECK(!compositor_lock_);

  FOR_EACH_OBSERVER(CompositorObserver, observer_list_,
                    OnCompositingShuttingDown(this));

  FOR_EACH_OBSERVER(CompositorAnimationObserver, animation_observer_list_,
                    OnCompositingShuttingDown(this));

  if (root_layer_)
    root_layer_->ResetCompositor();

  if (animation_timeline_)
    host_->animation_host()->RemoveAnimationTimeline(animation_timeline_.get());

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  context_factory_->RemoveCompositor(this);
}

void Compositor::SetOutputSurface(
    std::unique_ptr<cc::OutputSurface> output_surface) {
  output_surface_requested_ = false;
  host_->SetOutputSurface(std::move(output_surface));
}

void Compositor::ScheduleDraw() {
  host_->SetNeedsCommit();
}

void Compositor::SetRootLayer(Layer* root_layer) {
  if (root_layer_ == root_layer)
    return;
  if (root_layer_)
    root_layer_->ResetCompositor();
  root_layer_ = root_layer;
  root_web_layer_->RemoveAllChildren();
  if (root_layer_)
    root_layer_->SetCompositor(this, root_web_layer_);
}

cc::AnimationTimeline* Compositor::GetAnimationTimeline() const {
  return animation_timeline_.get();
}

void Compositor::SetHostHasTransparentBackground(
    bool host_has_transparent_background) {
  host_->set_has_transparent_background(host_has_transparent_background);
}

void Compositor::ScheduleFullRedraw() {
  // TODO(enne): Some callers (mac) call this function expecting that it
  // will also commit.  This should probably just redraw the screen
  // from damage and not commit.  ScheduleDraw/ScheduleRedraw need
  // better names.
  host_->SetNeedsRedraw();
  host_->SetNeedsCommit();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  // TODO(enne): Make this not commit.  See ScheduleFullRedraw.
  host_->SetNeedsRedrawRect(damage_rect);
  host_->SetNeedsCommit();
}

void Compositor::FinishAllRendering() {
  host_->FinishAllRendering();
}

void Compositor::DisableSwapUntilResize() {
  host_->FinishAllRendering();
  context_factory_->ResizeDisplay(this, gfx::Size());
}

void Compositor::SetLatencyInfo(const ui::LatencyInfo& latency_info) {
  std::unique_ptr<cc::SwapPromise> swap_promise(
      new cc::LatencyInfoSwapPromise(latency_info));
  host_->QueueSwapPromise(std::move(swap_promise));
}

void Compositor::SetScaleAndSize(float scale, const gfx::Size& size_in_pixel) {
  DCHECK_GT(scale, 0);
  if (!size_in_pixel.IsEmpty()) {
    size_ = size_in_pixel;
    host_->SetViewportSize(size_in_pixel);
    root_web_layer_->SetBounds(size_in_pixel);
    context_factory_->ResizeDisplay(this, size_in_pixel);
  }
  if (device_scale_factor_ != scale) {
    device_scale_factor_ = scale;
    host_->SetDeviceScaleFactor(scale);
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetDisplayColorSpace(const gfx::ColorSpace& color_space) {
  context_factory_->SetDisplayColorSpace(this, color_space);
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->set_background_color(color);
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
}

bool Compositor::IsVisible() {
  return host_->visible();
}

void Compositor::SetAuthoritativeVSyncInterval(
    const base::TimeDelta& interval) {
  context_factory_->SetAuthoritativeVSyncInterval(this, interval);
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
         cc::switches::kEnableBeginFrameScheduling)) {
    vsync_manager_->SetAuthoritativeVSyncInterval(interval);
  }
}

void Compositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  // This function should only get called once.
  DCHECK(!widget_valid_);
  widget_ = widget;
  widget_valid_ = true;
  if (output_surface_requested_)
    context_factory_->CreateOutputSurface(weak_ptr_factory_.GetWeakPtr());
}

gfx::AcceleratedWidget Compositor::ReleaseAcceleratedWidget() {
  DCHECK(!IsVisible());
  if (!host_->output_surface_lost())
    host_->ReleaseOutputSurface();
  context_factory_->RemoveCompositor(this);
  widget_valid_ = false;
  gfx::AcceleratedWidget widget = widget_;
  widget_ = gfx::kNullAcceleratedWidget;
  return widget;
}

gfx::AcceleratedWidget Compositor::widget() const {
  DCHECK(widget_valid_);
  return widget_;
}

scoped_refptr<CompositorVSyncManager> Compositor::vsync_manager() const {
  return vsync_manager_;
}

void Compositor::AddObserver(CompositorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool Compositor::HasObserver(const CompositorObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Compositor::AddAnimationObserver(CompositorAnimationObserver* observer) {
  animation_observer_list_.AddObserver(observer);
  host_->SetNeedsAnimate();
}

void Compositor::RemoveAnimationObserver(
    CompositorAnimationObserver* observer) {
  animation_observer_list_.RemoveObserver(observer);
}

bool Compositor::HasAnimationObserver(
    const CompositorAnimationObserver* observer) const {
  return animation_observer_list_.HasObserver(observer);
}

void Compositor::BeginMainFrame(const cc::BeginFrameArgs& args) {
  FOR_EACH_OBSERVER(CompositorAnimationObserver,
                    animation_observer_list_,
                    OnAnimationStep(args.frame_time));
  if (animation_observer_list_.might_have_observers())
    host_->SetNeedsAnimate();
}

void Compositor::BeginMainFrameNotExpectedSoon() {
}

static void SendDamagedRectsRecursive(ui::Layer* layer) {
  layer->SendDamagedRects();
  for (auto* child : layer->children())
    SendDamagedRectsRecursive(child);
}

void Compositor::UpdateLayerTreeHost() {
  if (!root_layer())
    return;
  SendDamagedRectsRecursive(root_layer());
}

void Compositor::RequestNewOutputSurface() {
  DCHECK(!output_surface_requested_);
  output_surface_requested_ = true;
  if (widget_valid_)
    context_factory_->CreateOutputSurface(weak_ptr_factory_.GetWeakPtr());
}

void Compositor::DidInitializeOutputSurface() {
}

void Compositor::DidFailToInitializeOutputSurface() {
  // The OutputSurface should already be bound/initialized before being given to
  // the Compositor.
  NOTREACHED();
}

void Compositor::DidCommit() {
  DCHECK(!IsLocked());
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingDidCommit(this));
}

void Compositor::DidCommitAndDrawFrame() {
}

void Compositor::DidCompleteSwapBuffers() {
  FOR_EACH_OBSERVER(CompositorObserver, observer_list_,
                    OnCompositingEnded(this));
}

void Compositor::DidPostSwapBuffers() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  FOR_EACH_OBSERVER(CompositorObserver, observer_list_,
                    OnCompositingStarted(this, start_time));
}

void Compositor::DidAbortSwapBuffers() {
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingAborted(this));
}

void Compositor::SetOutputIsSecure(bool output_is_secure) {
  context_factory_->SetOutputIsSecure(this, output_is_secure);
}

const cc::LayerTreeDebugState& Compositor::GetLayerTreeDebugState() const {
  return host_->debug_state();
}

void Compositor::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& debug_state) {
  host_->SetDebugState(debug_state);
}

const cc::RendererSettings& Compositor::GetRendererSettings() const {
  return host_->settings().renderer_settings;
}

scoped_refptr<CompositorLock> Compositor::GetCompositorLock() {
  if (!compositor_lock_) {
    compositor_lock_ = new CompositorLock(this);
    host_->SetDeferCommits(true);
    FOR_EACH_OBSERVER(CompositorObserver,
                      observer_list_,
                      OnCompositingLockStateChanged(this));
  }
  return compositor_lock_;
}

void Compositor::UnlockCompositor() {
  DCHECK(compositor_lock_);
  compositor_lock_ = NULL;
  host_->SetDeferCommits(false);
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingLockStateChanged(this));
}

void Compositor::CancelCompositorLock() {
  if (compositor_lock_)
    compositor_lock_->CancelLock();
}

}  // namespace ui
