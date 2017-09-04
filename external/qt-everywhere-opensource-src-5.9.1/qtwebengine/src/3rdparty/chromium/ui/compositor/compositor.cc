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
#include "base/metrics/histogram_macros.h"
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
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host_in_process.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/gl/gl_switches.h"

namespace {

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 200.0;

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
#if defined(USE_AURA)
      window_(nullptr),
#endif
      widget_valid_(false),
      compositor_frame_sink_requested_(false),
      frame_sink_id_(context_factory->AllocateFrameSinkId()),
      task_runner_(task_runner),
      vsync_manager_(new CompositorVSyncManager()),
      device_scale_factor_(0.0f),
      locks_will_time_out_(true),
      compositor_lock_(NULL),
      layer_animator_collection_(this),
      weak_ptr_factory_(this) {
  context_factory->GetSurfaceManager()->RegisterFrameSinkId(frame_sink_id_);
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
    // See comments in gl_switches about this flag.  The browser compositor
    // is only unthrottled when "gpu" or no switch value is passed, as it
    // is driven directly by the display compositor.
    if (display_vsync_string != "beginframe") {
      settings.renderer_settings.disable_display_vsync = true;
    }
  }
  settings.renderer_settings.partial_swap_enabled =
      !command_line->HasSwitch(switches::kUIDisablePartialSwap);
#if defined(OS_WIN)
  settings.renderer_settings.finish_rendering_on_resize = true;
#elif defined(OS_MACOSX)
  settings.renderer_settings.release_overlay_resources_after_gpu_query = true;
#endif
  settings.renderer_settings.gl_composited_texture_quad_border =
      command_line->HasSwitch(cc::switches::kGlCompositedTextureQuadBorder);

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

  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));

  settings.use_zero_copy = IsUIZeroCopyEnabled();

  if (command_line->HasSwitch(switches::kUIEnableRGBA4444Textures))
    settings.renderer_settings.preferred_tile_format = cc::RGBA_4444;

  settings.use_layer_lists =
      command_line->HasSwitch(cc::switches::kUIEnableLayerLists);

  settings.enable_color_correct_rendering =
      command_line->HasSwitch(cc::switches::kEnableColorCorrectRendering);

  // UI compositor always uses partial raster if not using zero-copy. Zero copy
  // doesn't currently support partial raster.
  settings.use_partial_raster = !settings.use_zero_copy;

  // Populate buffer_to_texture_target_map for all buffer usage/formats.
  for (int usage_idx = 0; usage_idx <= static_cast<int>(gfx::BufferUsage::LAST);
       ++usage_idx) {
    gfx::BufferUsage usage = static_cast<gfx::BufferUsage>(usage_idx);
    for (int format_idx = 0;
         format_idx <= static_cast<int>(gfx::BufferFormat::LAST);
         ++format_idx) {
      gfx::BufferFormat format = static_cast<gfx::BufferFormat>(format_idx);
      uint32_t target = context_factory_->GetImageTextureTarget(format, usage);
      settings.renderer_settings.buffer_to_texture_target_map.insert(
          cc::BufferToTextureTargetMap::value_type(
              cc::BufferToTextureTargetKey(usage, format), target));
    }
  }

  // Note: Only enable image decode tasks if we have more than one worker
  // thread.
  settings.image_decode_tasks_enabled = false;

  settings.gpu_memory_policy.bytes_limit_when_visible = 512 * 1024 * 1024;
  settings.gpu_memory_policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;

  base::TimeTicks before_create = base::TimeTicks::Now();

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHostInProcess::InitParams params;
  params.client = this;
  params.task_graph_runner = context_factory_->GetTaskGraphRunner();
  params.settings = &settings;
  params.main_task_runner = task_runner_;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHostInProcess::CreateSingleThreaded(this, &params);
  UMA_HISTOGRAM_TIMES("GPU.CreateBrowserCompositor",
                      base::TimeTicks::Now() - before_create);

  animation_timeline_ =
      cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId());
  animation_host_->AddAnimationTimeline(animation_timeline_.get());

  host_->GetLayerTree()->SetRootLayer(root_web_layer_);
  host_->SetFrameSinkId(frame_sink_id_);
  host_->SetVisible(true);
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown", "Compositor::destructor");

  CancelCompositorLock();
  DCHECK(!compositor_lock_);

  for (auto& observer : observer_list_)
    observer.OnCompositingShuttingDown(this);

  for (auto& observer : animation_observer_list_)
    observer.OnCompositingShuttingDown(this);

  if (root_layer_)
    root_layer_->ResetCompositor();

  if (animation_timeline_)
    animation_host_->RemoveAnimationTimeline(animation_timeline_.get());

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  context_factory_->RemoveCompositor(this);
  auto* manager = context_factory_->GetSurfaceManager();
  for (auto& client : child_frame_sinks_) {
    DCHECK(client.is_valid());
    manager->UnregisterFrameSinkHierarchy(frame_sink_id_, client);
  }
  manager->InvalidateFrameSinkId(frame_sink_id_);
}

void Compositor::AddFrameSink(const cc::FrameSinkId& frame_sink_id) {
  context_factory_->GetSurfaceManager()->RegisterFrameSinkHierarchy(
      frame_sink_id_, frame_sink_id);
  child_frame_sinks_.insert(frame_sink_id);
}

void Compositor::RemoveFrameSink(const cc::FrameSinkId& frame_sink_id) {
  auto it = child_frame_sinks_.find(frame_sink_id);
  DCHECK(it != child_frame_sinks_.end());
  DCHECK(it->is_valid());
  context_factory_->GetSurfaceManager()->UnregisterFrameSinkHierarchy(
      frame_sink_id_, *it);
  child_frame_sinks_.erase(it);
}

void Compositor::SetCompositorFrameSink(
    std::unique_ptr<cc::CompositorFrameSink> compositor_frame_sink) {
  compositor_frame_sink_requested_ = false;
  host_->SetCompositorFrameSink(std::move(compositor_frame_sink));
  // Display properties are reset when the output surface is lost, so update it
  // to match the Compositor's.
  context_factory_->SetDisplayVisible(this, host_->IsVisible());
  context_factory_->SetDisplayColorSpace(this, color_space_);
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
  host_->GetLayerTree()->set_has_transparent_background(
      host_has_transparent_background);
}

void Compositor::ScheduleFullRedraw() {
  // TODO(enne): Some callers (mac) call this function expecting that it
  // will also commit.  This should probably just redraw the screen
  // from damage and not commit.  ScheduleDraw/ScheduleRedraw need
  // better names.
  host_->SetNeedsRedrawRect(
      gfx::Rect(host_->GetLayerTree()->device_viewport_size()));
  host_->SetNeedsCommit();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  // TODO(enne): Make this not commit.  See ScheduleFullRedraw.
  host_->SetNeedsRedrawRect(damage_rect);
  host_->SetNeedsCommit();
}

void Compositor::DisableSwapUntilResize() {
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
    host_->GetLayerTree()->SetViewportSize(size_in_pixel);
    root_web_layer_->SetBounds(size_in_pixel);
    context_factory_->ResizeDisplay(this, size_in_pixel);
  }
  if (device_scale_factor_ != scale) {
    device_scale_factor_ = scale;
    host_->GetLayerTree()->SetDeviceScaleFactor(scale);
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetDisplayColorSpace(const gfx::ColorSpace& color_space) {
  host_->GetLayerTree()->SetDeviceColorSpace(color_space);
  color_space_ = color_space;
  // Color space is reset when the output surface is lost, so this must also be
  // updated then.
  context_factory_->SetDisplayColorSpace(this, color_space_);
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->GetLayerTree()->set_background_color(color);
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
  // Visibility is reset when the output surface is lost, so this must also be
  // updated then.
  context_factory_->SetDisplayVisible(this, visible);
}

bool Compositor::IsVisible() {
  return host_->IsVisible();
}

bool Compositor::ScrollLayerTo(int layer_id, const gfx::ScrollOffset& offset) {
  return host_->GetInputHandler()->ScrollLayerTo(layer_id, offset);
}

bool Compositor::GetScrollOffsetForLayer(int layer_id,
                                         gfx::ScrollOffset* offset) const {
  return host_->GetInputHandler()->GetScrollOffsetForLayer(layer_id, offset);
}

void Compositor::SetAuthoritativeVSyncInterval(
    const base::TimeDelta& interval) {
  context_factory_->SetAuthoritativeVSyncInterval(this, interval);
  vsync_manager_->SetAuthoritativeVSyncInterval(interval);
}

void Compositor::SetDisplayVSyncParameters(base::TimeTicks timebase,
                                           base::TimeDelta interval) {
  context_factory_->SetDisplayVSyncParameters(this, timebase, interval);
  vsync_manager_->UpdateVSyncParameters(timebase, interval);
}

void Compositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  // This function should only get called once.
  DCHECK(!widget_valid_);
  widget_ = widget;
  widget_valid_ = true;
  if (compositor_frame_sink_requested_)
    context_factory_->CreateCompositorFrameSink(weak_ptr_factory_.GetWeakPtr());
}

gfx::AcceleratedWidget Compositor::ReleaseAcceleratedWidget() {
  DCHECK(!IsVisible());
  host_->ReleaseCompositorFrameSink();
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

#if defined(USE_AURA)
void Compositor::SetWindow(ui::Window* window) {
  window_ = window;
}

ui::Window* Compositor::window() const {
  return window_;
}
#endif

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
  for (auto& observer : animation_observer_list_)
    observer.OnAnimationStep(args.frame_time);
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

void Compositor::RequestNewCompositorFrameSink() {
  DCHECK(!compositor_frame_sink_requested_);
  compositor_frame_sink_requested_ = true;
  if (widget_valid_)
    context_factory_->CreateCompositorFrameSink(weak_ptr_factory_.GetWeakPtr());
}

void Compositor::DidFailToInitializeCompositorFrameSink() {
  // The CompositorFrameSink should already be bound/initialized before being
  // given to
  // the Compositor.
  NOTREACHED();
}

void Compositor::DidCommit() {
  DCHECK(!IsLocked());
  for (auto& observer : observer_list_)
    observer.OnCompositingDidCommit(this);
}

void Compositor::DidReceiveCompositorFrameAck() {
  for (auto& observer : observer_list_)
    observer.OnCompositingEnded(this);
}

void Compositor::DidSubmitCompositorFrame() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  for (auto& observer : observer_list_)
    observer.OnCompositingStarted(this, start_time);
}

void Compositor::SetOutputIsSecure(bool output_is_secure) {
  context_factory_->SetOutputIsSecure(this, output_is_secure);
}

const cc::LayerTreeDebugState& Compositor::GetLayerTreeDebugState() const {
  return host_->GetDebugState();
}

void Compositor::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& debug_state) {
  host_->SetDebugState(debug_state);
}

const cc::RendererSettings& Compositor::GetRendererSettings() const {
  return host_->GetSettings().renderer_settings;
}

scoped_refptr<CompositorLock> Compositor::GetCompositorLock() {
  if (!compositor_lock_) {
    compositor_lock_ = new CompositorLock(this);
    host_->SetDeferCommits(true);
    for (auto& observer : observer_list_)
      observer.OnCompositingLockStateChanged(this);
  }
  return compositor_lock_;
}

void Compositor::UnlockCompositor() {
  DCHECK(compositor_lock_);
  compositor_lock_ = NULL;
  host_->SetDeferCommits(false);
  for (auto& observer : observer_list_)
    observer.OnCompositingLockStateChanged(this);
}

void Compositor::CancelCompositorLock() {
  if (compositor_lock_)
    compositor_lock_->CancelLock();
}

}  // namespace ui
