// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include <stddef.h>
#include <limits>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/layer_tree_mutator.h"
#include "cc/base/switches.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/latency_info_swap_promise.h"
#include "cc/output/swap_promise.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/resources/single_release_callback.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/remote_proto_channel.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/common/content_switches_internal.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/render_widget_compositor_delegate.h"
#include "content/renderer/input/input_handler_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebCompositorMutatorClient.h"
#include "third_party/WebKit/public/platform/WebLayoutAndPaintAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebSelection.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "ui/gfx/android/device_display_info.h"
#endif

namespace base {
class Value;
}

namespace cc {
class Layer;
}

using blink::WebFloatPoint;
using blink::WebRect;
using blink::WebSelection;
using blink::WebSize;
using blink::WebTopControlsState;

namespace content {
namespace {

bool GetSwitchValueAsInt(const base::CommandLine& command_line,
                         const std::string& switch_string,
                         int min_value,
                         int max_value,
                         int* result) {
  std::string string_value = command_line.GetSwitchValueASCII(switch_string);
  int int_value;
  if (base::StringToInt(string_value, &int_value) &&
      int_value >= min_value && int_value <= max_value) {
    *result = int_value;
    return true;
  } else {
    LOG(WARNING) << "Failed to parse switch " << switch_string  << ": " <<
        string_value;
    return false;
  }
}

cc::LayerSelectionBound ConvertWebSelectionBound(
    const WebSelection& web_selection,
    bool is_start) {
  cc::LayerSelectionBound cc_bound;
  if (web_selection.isNone())
    return cc_bound;

  const blink::WebSelectionBound& web_bound =
      is_start ? web_selection.start() : web_selection.end();
  DCHECK(web_bound.layerId);
  cc_bound.type = gfx::SelectionBound::CENTER;
  if (web_selection.isRange()) {
    if (is_start) {
      cc_bound.type = web_bound.isTextDirectionRTL ? gfx::SelectionBound::RIGHT
                                                   : gfx::SelectionBound::LEFT;
    } else {
      cc_bound.type = web_bound.isTextDirectionRTL ? gfx::SelectionBound::LEFT
                                                   : gfx::SelectionBound::RIGHT;
    }
  }
  cc_bound.layer_id = web_bound.layerId;
  cc_bound.edge_top = gfx::Point(web_bound.edgeTopInLayer);
  cc_bound.edge_bottom = gfx::Point(web_bound.edgeBottomInLayer);
  return cc_bound;
}

cc::LayerSelection ConvertWebSelection(const WebSelection& web_selection) {
  cc::LayerSelection cc_selection;
  cc_selection.start = ConvertWebSelectionBound(web_selection, true);
  cc_selection.end = ConvertWebSelectionBound(web_selection, false);
  cc_selection.is_editable = web_selection.isEditable();
  cc_selection.is_empty_text_form_control =
      web_selection.isEmptyTextFormControl();
  return cc_selection;
}

gfx::Size CalculateDefaultTileSize(float initial_device_scale_factor) {
  int default_tile_size = 256;
#if defined(OS_ANDROID)
  // TODO(epenner): unify this for all platforms if it
  // makes sense (http://crbug.com/159524)

  gfx::DeviceDisplayInfo info;
  bool real_size_supported = true;
  int display_width = info.GetPhysicalDisplayWidth();
  int display_height = info.GetPhysicalDisplayHeight();
  if (display_width == 0 || display_height == 0) {
    real_size_supported = false;
    display_width = info.GetDisplayWidth();
    display_height = info.GetDisplayHeight();
  }

  int portrait_width = std::min(display_width, display_height);
  int landscape_width = std::max(display_width, display_height);

  if (real_size_supported) {
    // Maximum HD dimensions should be 768x1280
    // Maximum FHD dimensions should be 1200x1920
    if (portrait_width > 768 || landscape_width > 1280)
      default_tile_size = 384;
    if (portrait_width > 1200 || landscape_width > 1920)
      default_tile_size = 512;

    // Adjust for some resolutions that barely straddle an extra
    // tile when in portrait mode. This helps worst case scroll/raster
    // by not needing a full extra tile for each row.
    if (default_tile_size == 256 && portrait_width == 768)
      default_tile_size += 32;
    if (default_tile_size == 384 && portrait_width == 1200)
      default_tile_size += 32;
  } else {
    // We don't know the exact resolution due to screen controls etc.
    // So this just estimates the values above using tile counts.
    int numTiles = (display_width * display_height) / (256 * 256);
    if (numTiles > 16)
      default_tile_size = 384;
    if (numTiles >= 40)
      default_tile_size = 512;
  }
#elif defined(OS_CHROMEOS) || defined(OS_MACOSX)
  // Use 512 for high DPI (dsf=2.0f) devices.
  if (initial_device_scale_factor >= 2.0f)
    default_tile_size = 512;
#endif

  return gfx::Size(default_tile_size, default_tile_size);
}

// Check cc::TopControlsState, and blink::WebTopControlsState
// are kept in sync.
static_assert(int(blink::WebTopControlsBoth) == int(cc::BOTH),
              "mismatching enums: BOTH");
static_assert(int(blink::WebTopControlsHidden) == int(cc::HIDDEN),
              "mismatching enums: HIDDEN");
static_assert(int(blink::WebTopControlsShown) == int(cc::SHOWN),
              "mismatching enums: SHOWN");

static cc::TopControlsState ConvertTopControlsState(
    WebTopControlsState state) {
  return static_cast<cc::TopControlsState>(state);
}

}  // namespace

// static
std::unique_ptr<RenderWidgetCompositor> RenderWidgetCompositor::Create(
    RenderWidgetCompositorDelegate* delegate,
    float device_scale_factor,
    CompositorDependencies* compositor_deps) {
  std::unique_ptr<RenderWidgetCompositor> compositor(
      new RenderWidgetCompositor(delegate, compositor_deps));
  compositor->Initialize(device_scale_factor);
  return compositor;
}

RenderWidgetCompositor::RenderWidgetCompositor(
    RenderWidgetCompositorDelegate* delegate,
    CompositorDependencies* compositor_deps)
    : num_failed_recreate_attempts_(0),
      delegate_(delegate),
      compositor_deps_(compositor_deps),
      threaded_(!!compositor_deps_->GetCompositorImplThreadTaskRunner()),
      never_visible_(false),
      layout_and_paint_async_callback_(nullptr),
      remote_proto_channel_receiver_(nullptr),
      weak_factory_(this) {}

void RenderWidgetCompositor::Initialize(float device_scale_factor) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  cc::LayerTreeSettings settings =
      GenerateLayerTreeSettings(*cmd, compositor_deps_, device_scale_factor);
  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.shared_bitmap_manager = compositor_deps_->GetSharedBitmapManager();
  params.gpu_memory_buffer_manager =
      compositor_deps_->GetGpuMemoryBufferManager();
  params.settings = &settings;
  params.task_graph_runner = compositor_deps_->GetTaskGraphRunner();
  params.main_task_runner =
      compositor_deps_->GetCompositorMainThreadTaskRunner();
  if (settings.use_external_begin_frame_source) {
    params.external_begin_frame_source =
        delegate_->CreateExternalBeginFrameSource();
  }
  params.animation_host = cc::AnimationHost::CreateMainInstance();

  if (cmd->HasSwitch(switches::kUseRemoteCompositing)) {
    DCHECK(!threaded_);
    params.image_serialization_processor =
        compositor_deps_->GetImageSerializationProcessor();
    layer_tree_host_ = cc::LayerTreeHost::CreateRemoteServer(this, &params);
  } else if (!threaded_) {
    // Single-threaded layout tests.
    layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  } else {
    layer_tree_host_ = cc::LayerTreeHost::CreateThreaded(
        compositor_deps_->GetCompositorImplThreadTaskRunner(), &params);
  }
  DCHECK(layer_tree_host_);
}

RenderWidgetCompositor::~RenderWidgetCompositor() = default;

// static
cc::LayerTreeSettings RenderWidgetCompositor::GenerateLayerTreeSettings(
    const base::CommandLine& cmd,
    CompositorDependencies* compositor_deps,
    float device_scale_factor) {
  cc::LayerTreeSettings settings;

  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;

  if (cmd.HasSwitch(switches::kDisableGpuVsync)) {
    std::string display_vsync_string =
        cmd.GetSwitchValueASCII(switches::kDisableGpuVsync);
    if (display_vsync_string == "gpu") {
      settings.renderer_settings.disable_display_vsync = true;
    } else if (display_vsync_string == "beginframe") {
      settings.wait_for_beginframe_interval = false;
    } else {
      settings.renderer_settings.disable_display_vsync = true;
      settings.wait_for_beginframe_interval = false;
    }
  }
  settings.main_frame_before_activation_enabled =
      cmd.HasSwitch(cc::switches::kEnableMainFrameBeforeActivation);

  // TODO(danakj): This should not be a setting O_O; it should change when the
  // device scale factor on LayerTreeHost changes.
  settings.default_tile_size = CalculateDefaultTileSize(device_scale_factor);
  if (cmd.HasSwitch(switches::kDefaultTileWidth)) {
    int tile_width = 0;
    GetSwitchValueAsInt(cmd, switches::kDefaultTileWidth, 1,
                        std::numeric_limits<int>::max(), &tile_width);
    settings.default_tile_size.set_width(tile_width);
  }
  if (cmd.HasSwitch(switches::kDefaultTileHeight)) {
    int tile_height = 0;
    GetSwitchValueAsInt(cmd, switches::kDefaultTileHeight, 1,
                        std::numeric_limits<int>::max(), &tile_height);
    settings.default_tile_size.set_height(tile_height);
  }

  int max_untiled_layer_width = settings.max_untiled_layer_size.width();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerWidth)) {
    GetSwitchValueAsInt(cmd, switches::kMaxUntiledLayerWidth, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_width);
  }
  int max_untiled_layer_height = settings.max_untiled_layer_size.height();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerHeight)) {
    GetSwitchValueAsInt(cmd, switches::kMaxUntiledLayerHeight, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_height);
  }

  settings.max_untiled_layer_size = gfx::Size(max_untiled_layer_width,
                                           max_untiled_layer_height);

  settings.gpu_rasterization_msaa_sample_count =
      compositor_deps->GetGpuRasterizationMSAASampleCount();
  settings.gpu_rasterization_forced =
      compositor_deps->IsGpuRasterizationForced();
  settings.gpu_rasterization_enabled =
      compositor_deps->IsGpuRasterizationEnabled();
  settings.async_worker_context_enabled =
      compositor_deps->IsAsyncWorkerContextEnabled();

  settings.can_use_lcd_text = compositor_deps->IsLcdTextEnabled();
  settings.use_distance_field_text =
      compositor_deps->IsDistanceFieldTextEnabled();
  settings.use_zero_copy = compositor_deps->IsZeroCopyEnabled();
  settings.use_partial_raster = compositor_deps->IsPartialRasterEnabled();
  settings.enable_elastic_overscroll =
      compositor_deps->IsElasticOverscrollEnabled();
  settings.renderer_settings.use_gpu_memory_buffer_resources =
      compositor_deps->IsGpuMemoryBufferCompositorResourcesEnabled();
  settings.use_image_texture_targets =
      compositor_deps->GetImageTextureTargets();
  settings.image_decode_tasks_enabled =
      compositor_deps->AreImageDecodeTasksEnabled();

  if (cmd.HasSwitch(cc::switches::kTopControlsShowThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kTopControlsShowThreshold);
    double show_threshold;
    if (base::StringToDouble(top_threshold_str, &show_threshold) &&
        show_threshold >= 0.f && show_threshold <= 1.f)
      settings.top_controls_show_threshold = show_threshold;
  }

  if (cmd.HasSwitch(cc::switches::kTopControlsHideThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kTopControlsHideThreshold);
    double hide_threshold;
    if (base::StringToDouble(top_threshold_str, &hide_threshold) &&
        hide_threshold >= 0.f && hide_threshold <= 1.f)
      settings.top_controls_hide_threshold = hide_threshold;
  }

  settings.use_layer_lists = cmd.HasSwitch(cc::switches::kEnableLayerLists);

  settings.renderer_settings.allow_antialiasing &=
      !cmd.HasSwitch(cc::switches::kDisableCompositedAntialiasing);
  // The means the renderer compositor has 2 possible modes:
  // - Threaded compositing with a scheduler.
  // - Single threaded compositing without a scheduler (for layout tests only).
  // Using the scheduler in layout tests introduces additional composite steps
  // that create flakiness.
  settings.single_thread_proxy_scheduler = false;

  // These flags should be mirrored by UI versions in ui/compositor/.
  settings.initial_debug_state.show_debug_borders =
      cmd.HasSwitch(cc::switches::kShowCompositedLayerBorders);
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      cmd.HasSwitch(cc::switches::kShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      cmd.HasSwitch(switches::kShowPaintRects);
  settings.initial_debug_state.show_property_changed_rects =
      cmd.HasSwitch(cc::switches::kShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      cmd.HasSwitch(cc::switches::kShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      cmd.HasSwitch(cc::switches::kShowScreenSpaceRects);
  settings.initial_debug_state.show_replica_screen_space_rects =
      cmd.HasSwitch(cc::switches::kShowReplicaScreenSpaceRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      cmd.HasSwitch(cc::switches::kEnableGpuBenchmarking));

  if (cmd.HasSwitch(cc::switches::kSlowDownRasterScaleFactor)) {
    const int kMinSlowDownScaleFactor = 0;
    const int kMaxSlowDownScaleFactor = INT_MAX;
    GetSwitchValueAsInt(
        cmd, cc::switches::kSlowDownRasterScaleFactor, kMinSlowDownScaleFactor,
        kMaxSlowDownScaleFactor,
        &settings.initial_debug_state.slow_down_raster_scale_factor);
  }

#if defined(OS_ANDROID)
  bool using_synchronous_compositor =
      GetContentClient()->UsingSynchronousCompositing();

  // We can't use GPU rasterization on low-end devices, because the Ganesh
  // cache would consume too much memory.
  if (base::SysInfo::IsLowEndDevice())
    settings.gpu_rasterization_enabled = false;
  settings.using_synchronous_renderer_compositor = using_synchronous_compositor;
  if (using_synchronous_compositor) {
    // Android WebView uses system scrollbars, so make ours invisible.
    settings.scrollbar_animator = cc::LayerTreeSettings::NO_ANIMATOR;
    settings.solid_color_scrollbar_color = SK_ColorTRANSPARENT;
  } else {
    settings.scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
    settings.scrollbar_fade_delay_ms = 300;
    settings.scrollbar_fade_resize_delay_ms = 2000;
    settings.scrollbar_fade_duration_ms = 300;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  }
  settings.renderer_settings.highp_threshold_min = 2048;
  // Android WebView handles root layer flings itself.
  settings.ignore_root_layer_flings = using_synchronous_compositor;
  // Memory policy on Android WebView does not depend on whether device is
  // low end, so always use default policy.
  bool use_low_memory_policy =
      base::SysInfo::IsLowEndDevice() && !using_synchronous_compositor;
  if (use_low_memory_policy) {
    // On low-end we want to be very carefull about killing other
    // apps. So initially we use 50% more memory to avoid flickering
    // or raster-on-demand.
    settings.max_memory_for_prepaint_percentage = 67;

    // RGBA_4444 textures are only enabled by default for low end devices
    // and are disabled for Android WebView as it doesn't support the format.
    if (!cmd.HasSwitch(switches::kDisableRGBA4444Textures))
      settings.renderer_settings.preferred_tile_format = cc::RGBA_4444;
  } else {
    // On other devices we have increased memory excessively to avoid
    // raster-on-demand already, so now we reserve 50% _only_ to avoid
    // raster-on-demand, and use 50% of the memory otherwise.
    settings.max_memory_for_prepaint_percentage = 50;
  }
  // Webview does not own the surface so should not clear it.
  settings.renderer_settings.should_clear_root_render_pass =
      !using_synchronous_compositor;

  // TODO(danakj): Only do this on low end devices.
  settings.create_low_res_tiling = true;

  settings.use_external_begin_frame_source = true;

#else  // defined(OS_ANDROID)
#if !defined(OS_MACOSX)
  if (ui::IsOverlayScrollbarEnabled()) {
    settings.scrollbar_animator = cc::LayerTreeSettings::THINNING;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  } else {
    settings.scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  }
  settings.scrollbar_fade_delay_ms = 500;
  settings.scrollbar_fade_resize_delay_ms = 500;
  settings.scrollbar_fade_duration_ms = 300;
#endif  // !defined(OS_MACOSX)

  // On desktop, if there's over 4GB of memory on the machine, increase the
  // image decode budget to 256MB for both gpu and software.
  const int kImageDecodeMemoryThresholdMB = 4 * 1024;
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >=
      kImageDecodeMemoryThresholdMB) {
    settings.gpu_decoded_image_budget_bytes = 256 * 1024 * 1024;
    settings.software_decoded_image_budget_bytes = 256 * 1024 * 1024;
  } else {
    // These are the defaults, but recorded here as well.
    settings.gpu_decoded_image_budget_bytes = 96 * 1024 * 1024;
    settings.software_decoded_image_budget_bytes = 128 * 1024 * 1024;
  }

#endif  // defined(OS_ANDROID)

  if (cmd.HasSwitch(switches::kEnableLowResTiling))
    settings.create_low_res_tiling = true;
  if (cmd.HasSwitch(switches::kDisableLowResTiling))
    settings.create_low_res_tiling = false;
  if (cmd.HasSwitch(cc::switches::kEnableBeginFrameScheduling))
    settings.use_external_begin_frame_source = true;

  if (cmd.HasSwitch(switches::kEnableRGBA4444Textures) &&
      !cmd.HasSwitch(switches::kDisableRGBA4444Textures)) {
    settings.renderer_settings.preferred_tile_format = cc::RGBA_4444;
  }

  if (cmd.HasSwitch(cc::switches::kEnableTileCompression)) {
    settings.renderer_settings.preferred_tile_format = cc::ETC1;
  }

  settings.max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;  // 32MB
  // Use 1/4th of staging buffers on low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    settings.max_staging_buffer_usage_in_bytes /= 4;

  cc::ManagedMemoryPolicy current = settings.memory_policy_;
  settings.memory_policy_ = GetGpuMemoryPolicy(current);

  settings.use_cached_picture_raster =
      !cmd.HasSwitch(cc::switches::kDisableCachedPictureRaster);

  if (cmd.HasSwitch(switches::kUseRemoteCompositing))
    settings.use_external_begin_frame_source = false;

  return settings;
}

// static
cc::ManagedMemoryPolicy RenderWidgetCompositor::GetGpuMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {
  cc::ManagedMemoryPolicy actual = policy;
  actual.bytes_limit_when_visible = 0;

  // If the value was overridden on the command line, use the specified value.
  static bool client_hard_limit_bytes_overridden =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceGpuMemAvailableMb);
  if (client_hard_limit_bytes_overridden) {
    if (base::StringToSizeT(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kForceGpuMemAvailableMb),
            &actual.bytes_limit_when_visible))
      actual.bytes_limit_when_visible *= 1024 * 1024;
    return actual;
  }

#if defined(OS_ANDROID)
  // We can't query available GPU memory from the system on Android.
  // Physical memory is also mis-reported sometimes (eg. Nexus 10 reports
  // 1262MB when it actually has 2GB, while Razr M has 1GB but only reports
  // 128MB java heap size). First we estimate physical memory using both.
  size_t dalvik_mb = base::SysInfo::DalvikHeapSizeMB();
  size_t physical_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  size_t physical_memory_mb = 0;
  if (dalvik_mb >= 256)
    physical_memory_mb = dalvik_mb * 4;
  else
    physical_memory_mb = std::max(dalvik_mb * 4, (physical_mb * 4) / 3);

  // Now we take a default of 1/8th of memory on high-memory devices,
  // and gradually scale that back for low-memory devices (to be nicer
  // to other apps so they don't get killed). Examples:
  // Nexus 4/10(2GB)    256MB (normally 128MB)
  // Droid Razr M(1GB)  114MB (normally 57MB)
  // Galaxy Nexus(1GB)  100MB (normally 50MB)
  // Xoom(1GB)          100MB (normally 50MB)
  // Nexus S(low-end)   8MB (normally 8MB)
  // Note that the compositor now uses only some of this memory for
  // pre-painting and uses the rest only for 'emergencies'.
  if (actual.bytes_limit_when_visible == 0) {
    // NOTE: Non-low-end devices use only 50% of these limits,
    // except during 'emergencies' where 100% can be used.
    if (!base::SysInfo::IsLowEndDevice()) {
      if (physical_memory_mb >= 1536)
        actual.bytes_limit_when_visible = physical_memory_mb / 8;  // >192MB
      else if (physical_memory_mb >= 1152)
        actual.bytes_limit_when_visible = physical_memory_mb / 8;  // >144MB
      else if (physical_memory_mb >= 768)
        actual.bytes_limit_when_visible = physical_memory_mb / 10;  // >76MB
      else
        actual.bytes_limit_when_visible = physical_memory_mb / 12;  // <64MB
    } else {
      // Low-end devices have 512MB or less memory by definition
      // so we hard code the limit rather than relying on the heuristics
      // above. Low-end devices use 4444 textures so we can use a lower limit.
      actual.bytes_limit_when_visible = 8;
    }
    actual.bytes_limit_when_visible =
        actual.bytes_limit_when_visible * 1024 * 1024;
    // Clamp the observed value to a specific range on Android.
    actual.bytes_limit_when_visible = std::max(
        actual.bytes_limit_when_visible, static_cast<size_t>(8 * 1024 * 1024));
    actual.bytes_limit_when_visible =
        std::min(actual.bytes_limit_when_visible,
                 static_cast<size_t>(256 * 1024 * 1024));
  }
  actual.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
#else
  // Ignore what the system said and give all clients the same maximum
  // allocation on desktop platforms.
  actual.bytes_limit_when_visible = 512 * 1024 * 1024;
  actual.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
#endif
  return actual;
}

void RenderWidgetCompositor::SetNeverVisible() {
  DCHECK(!layer_tree_host_->visible());
  never_visible_ = true;
}

const base::WeakPtr<cc::InputHandler>&
RenderWidgetCompositor::GetInputHandler() {
  return layer_tree_host_->GetInputHandler();
}

bool RenderWidgetCompositor::BeginMainFrameRequested() const {
  return layer_tree_host_->BeginMainFrameRequested();
}

void RenderWidgetCompositor::SetNeedsDisplayOnAllLayers() {
  layer_tree_host_->SetNeedsDisplayOnAllLayers();
}

void RenderWidgetCompositor::SetRasterizeOnlyVisibleContent() {
  cc::LayerTreeDebugState current = layer_tree_host_->debug_state();
  current.rasterize_only_visible_content = true;
  layer_tree_host_->SetDebugState(current);
}

void RenderWidgetCompositor::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

void RenderWidgetCompositor::SetNeedsForcedRedraw() {
  layer_tree_host_->SetNextCommitForcesRedraw();
  setNeedsAnimate();
}

std::unique_ptr<cc::SwapPromiseMonitor>
RenderWidgetCompositor::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return std::unique_ptr<cc::SwapPromiseMonitor>(
      new cc::LatencyInfoSwapPromiseMonitor(latency, layer_tree_host_.get(),
                                            NULL));
}

void RenderWidgetCompositor::QueueSwapPromise(
    std::unique_ptr<cc::SwapPromise> swap_promise) {
  layer_tree_host_->QueueSwapPromise(std::move(swap_promise));
}

int RenderWidgetCompositor::GetSourceFrameNumber() const {
  return layer_tree_host_->source_frame_number();
}

void RenderWidgetCompositor::SetNeedsUpdateLayers() {
  layer_tree_host_->SetNeedsUpdateLayers();
}

void RenderWidgetCompositor::SetNeedsCommit() {
  layer_tree_host_->SetNeedsCommit();
}

void RenderWidgetCompositor::NotifyInputThrottledUntilCommit() {
  layer_tree_host_->NotifyInputThrottledUntilCommit();
}

const cc::Layer* RenderWidgetCompositor::GetRootLayer() const {
  return layer_tree_host_->root_layer();
}

int RenderWidgetCompositor::ScheduleMicroBenchmark(
    const std::string& name,
    std::unique_ptr<base::Value> value,
    const base::Callback<void(std::unique_ptr<base::Value>)>& callback) {
  return layer_tree_host_->ScheduleMicroBenchmark(name, std::move(value),
                                                  callback);
}

bool RenderWidgetCompositor::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return layer_tree_host_->SendMessageToMicroBenchmark(id, std::move(value));
}

void RenderWidgetCompositor::setRootLayer(const blink::WebLayer& layer) {
  layer_tree_host_->SetRootLayer(
      static_cast<const cc_blink::WebLayerImpl*>(&layer)->layer());
}

void RenderWidgetCompositor::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void RenderWidgetCompositor::attachCompositorAnimationTimeline(
    cc::AnimationTimeline* compositor_timeline) {
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->AddAnimationTimeline(compositor_timeline);
}

void RenderWidgetCompositor::detachCompositorAnimationTimeline(
    cc::AnimationTimeline* compositor_timeline) {
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->RemoveAnimationTimeline(
      compositor_timeline);
}

void RenderWidgetCompositor::setViewportSize(
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

WebFloatPoint RenderWidgetCompositor::adjustEventPointForPinchZoom(
    const WebFloatPoint& point) const {
  return point;
}

void RenderWidgetCompositor::setDeviceScaleFactor(float device_scale) {
  layer_tree_host_->SetDeviceScaleFactor(device_scale);
}

void RenderWidgetCompositor::setBackgroundColor(blink::WebColor color) {
  layer_tree_host_->set_background_color(color);
}

void RenderWidgetCompositor::setHasTransparentBackground(bool transparent) {
  layer_tree_host_->set_has_transparent_background(transparent);
}

void RenderWidgetCompositor::setVisible(bool visible) {
  if (never_visible_)
    return;

  layer_tree_host_->SetVisible(visible);
}

void RenderWidgetCompositor::setPageScaleFactorAndLimits(
    float page_scale_factor, float minimum, float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void RenderWidgetCompositor::startPageScaleAnimation(
    const blink::WebPoint& destination,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {
  base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
      duration_sec * base::Time::kMicrosecondsPerSecond);
  layer_tree_host_->StartPageScaleAnimation(
      gfx::Vector2d(destination.x, destination.y),
      use_anchor,
      new_page_scale,
      duration);
}

bool RenderWidgetCompositor::hasPendingPageScaleAnimation() const {
  return layer_tree_host_->HasPendingPageScaleAnimation();
}

void RenderWidgetCompositor::heuristicsForGpuRasterizationUpdated(
    bool matches_heuristics) {
  layer_tree_host_->SetHasGpuRasterizationTrigger(matches_heuristics);
}

void RenderWidgetCompositor::setNeedsAnimate() {
  layer_tree_host_->SetNeedsAnimate();
  layer_tree_host_->SetNeedsUpdateLayers();
}

void RenderWidgetCompositor::setNeedsBeginFrame() {
  layer_tree_host_->SetNeedsAnimate();
}

void RenderWidgetCompositor::setNeedsCompositorUpdate() {
  layer_tree_host_->SetNeedsUpdateLayers();
}

void RenderWidgetCompositor::didStopFlinging() {
  layer_tree_host_->DidStopFlinging();
}

void RenderWidgetCompositor::registerViewportLayers(
    const blink::WebLayer* overscrollElasticityLayer,
    const blink::WebLayer* pageScaleLayer,
    const blink::WebLayer* innerViewportScrollLayer,
    const blink::WebLayer* outerViewportScrollLayer) {
  layer_tree_host_->RegisterViewportLayers(
      // TODO(bokan): This check can probably be removed now, but it looks
      // like overscroll elasticity may still be NULL until VisualViewport
      // registers its layers.
      // The scroll elasticity layer will only exist when using pinch virtual
      // viewports.
      overscrollElasticityLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(
                overscrollElasticityLayer)->layer()
          : NULL,
      static_cast<const cc_blink::WebLayerImpl*>(pageScaleLayer)->layer(),
      static_cast<const cc_blink::WebLayerImpl*>(innerViewportScrollLayer)
          ->layer(),
      // TODO(bokan): This check can probably be removed now, but it looks
      // like overscroll elasticity may still be NULL until VisualViewport
      // registers its layers.
      // The outer viewport layer will only exist when using pinch virtual
      // viewports.
      outerViewportScrollLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(outerViewportScrollLayer)
                ->layer()
          : NULL);
}

void RenderWidgetCompositor::clearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(
      scoped_refptr<cc::Layer>(), scoped_refptr<cc::Layer>(),
      scoped_refptr<cc::Layer>(), scoped_refptr<cc::Layer>());
}

void RenderWidgetCompositor::registerSelection(
    const blink::WebSelection& selection) {
  layer_tree_host_->RegisterSelection(ConvertWebSelection(selection));
}

void RenderWidgetCompositor::clearSelection() {
  cc::LayerSelection empty_selection;
  layer_tree_host_->RegisterSelection(empty_selection);
}

void RenderWidgetCompositor::setMutatorClient(
    std::unique_ptr<blink::WebCompositorMutatorClient> client) {
  TRACE_EVENT0("compositor-worker", "RenderWidgetCompositor::setMutatorClient");
  layer_tree_host_->SetLayerTreeMutator(std::move(client));
}

static_assert(static_cast<cc::EventListenerClass>(
                  blink::WebEventListenerClass::TouchStartOrMove) ==
                  cc::EventListenerClass::kTouchStartOrMove,
              "EventListenerClass and WebEventListenerClass enums must match");
static_assert(static_cast<cc::EventListenerClass>(
                  blink::WebEventListenerClass::MouseWheel) ==
                  cc::EventListenerClass::kMouseWheel,
              "EventListenerClass and WebEventListenerClass enums must match");

static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::Nothing) ==
                  cc::EventListenerProperties::kNone,
              "EventListener and WebEventListener enums must match");
static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::Passive) ==
                  cc::EventListenerProperties::kPassive,
              "EventListener and WebEventListener enums must match");
static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::Blocking) ==
                  cc::EventListenerProperties::kBlocking,
              "EventListener and WebEventListener enums must match");
static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::BlockingAndPassive) ==
                  cc::EventListenerProperties::kBlockingAndPassive,
              "EventListener and WebEventListener enums must match");

void RenderWidgetCompositor::setEventListenerProperties(
    blink::WebEventListenerClass eventClass,
    blink::WebEventListenerProperties properties) {
  layer_tree_host_->SetEventListenerProperties(
      static_cast<cc::EventListenerClass>(eventClass),
      static_cast<cc::EventListenerProperties>(properties));
}

blink::WebEventListenerProperties
RenderWidgetCompositor::eventListenerProperties(
    blink::WebEventListenerClass event_class) const {
  return static_cast<blink::WebEventListenerProperties>(
      layer_tree_host_->event_listener_properties(
          static_cast<cc::EventListenerClass>(event_class)));
}

void RenderWidgetCompositor::setHaveScrollEventHandlers(bool has_handlers) {
  layer_tree_host_->SetHaveScrollEventHandlers(has_handlers);
}

bool RenderWidgetCompositor::haveScrollEventHandlers() const {
  return layer_tree_host_->have_scroll_event_handlers();
}

void CompositeAndReadbackAsyncCallback(
    blink::WebCompositeAndReadbackAsyncCallback* callback,
    std::unique_ptr<cc::CopyOutputResult> result) {
  if (result->HasBitmap()) {
    std::unique_ptr<SkBitmap> result_bitmap = result->TakeBitmap();
    callback->didCompositeAndReadback(*result_bitmap);
  } else {
    callback->didCompositeAndReadback(SkBitmap());
  }
}

bool RenderWidgetCompositor::CompositeIsSynchronous() const {
  if (!threaded_) {
    DCHECK(!layer_tree_host_->settings().single_thread_proxy_scheduler);
    return true;
  }
  return false;
}

void RenderWidgetCompositor::layoutAndPaintAsync(
    blink::WebLayoutAndPaintAsyncCallback* callback) {
  DCHECK(!temporary_copy_output_request_ && !layout_and_paint_async_callback_);
  layout_and_paint_async_callback_ = callback;

  if (CompositeIsSynchronous()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RenderWidgetCompositor::LayoutAndUpdateLayers,
                              weak_factory_.GetWeakPtr()));
  } else {
    layer_tree_host_->SetNeedsCommit();
  }
}

void RenderWidgetCompositor::LayoutAndUpdateLayers() {
  DCHECK(CompositeIsSynchronous());
  layer_tree_host_->LayoutAndUpdateLayers();
  InvokeLayoutAndPaintCallback();
}

void RenderWidgetCompositor::InvokeLayoutAndPaintCallback() {
  if (!layout_and_paint_async_callback_)
    return;
  layout_and_paint_async_callback_->didLayoutAndPaint();
  layout_and_paint_async_callback_ = nullptr;
}

void RenderWidgetCompositor::compositeAndReadbackAsync(
    blink::WebCompositeAndReadbackAsyncCallback* callback) {
  DCHECK(!temporary_copy_output_request_ && !layout_and_paint_async_callback_);
  temporary_copy_output_request_ =
      cc::CopyOutputRequest::CreateBitmapRequest(
          base::Bind(&CompositeAndReadbackAsyncCallback, callback));

  // Force a commit to happen. The temporary copy output request will
  // be installed after layout which will happen as a part of the commit, for
  // widgets that delay the creation of their output surface.
  if (CompositeIsSynchronous()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RenderWidgetCompositor::SynchronouslyComposite,
                              weak_factory_.GetWeakPtr()));
  } else {
    layer_tree_host_->SetNeedsCommit();
  }
}

void RenderWidgetCompositor::SynchronouslyComposite() {
  DCHECK(CompositeIsSynchronous());
  layer_tree_host_->Composite(base::TimeTicks::Now());
}

void RenderWidgetCompositor::setDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

int RenderWidgetCompositor::layerTreeId() const {
  return layer_tree_host_->id();
}

void RenderWidgetCompositor::setShowFPSCounter(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_fps_counter = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowPaintRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_paint_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowDebugBorders(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_debug_borders = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowScrollBottleneckRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_touch_event_handler_rects = show;
  debug_state.show_wheel_event_handler_rects = show;
  debug_state.show_non_fast_scrollable_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::updateTopControlsState(
    WebTopControlsState constraints,
    WebTopControlsState current,
    bool animate) {
  layer_tree_host_->UpdateTopControlsState(ConvertTopControlsState(constraints),
                                           ConvertTopControlsState(current),
                                           animate);
}

void RenderWidgetCompositor::setTopControlsHeight(float height, bool shrink) {
    layer_tree_host_->SetTopControlsHeight(height, shrink);
}

void RenderWidgetCompositor::setTopControlsShownRatio(float ratio) {
  layer_tree_host_->SetTopControlsShownRatio(ratio);
}

void RenderWidgetCompositor::WillBeginMainFrame() {
  delegate_->WillBeginCompositorFrame();
}

void RenderWidgetCompositor::DidBeginMainFrame() {
}

void RenderWidgetCompositor::BeginMainFrame(const cc::BeginFrameArgs& args) {
  compositor_deps_->GetRendererScheduler()->WillBeginFrame(args);
  double frame_time_sec = (args.frame_time - base::TimeTicks()).InSecondsF();
  delegate_->BeginMainFrame(frame_time_sec);
}

void RenderWidgetCompositor::BeginMainFrameNotExpectedSoon() {
  compositor_deps_->GetRendererScheduler()->BeginFrameNotExpectedSoon();
}

void RenderWidgetCompositor::UpdateLayerTreeHost() {
  delegate_->UpdateVisualState();
  if (temporary_copy_output_request_) {
    // For WebViewImpl, this will always have a root layer.  For other widgets,
    // the widget may be closed before servicing this request, so ignore it.
    if (cc::Layer* root_layer = layer_tree_host_->root_layer()) {
      root_layer->RequestCopyOfOutput(
          std::move(temporary_copy_output_request_));
    } else {
      temporary_copy_output_request_->SendEmptyResult();
      temporary_copy_output_request_ = nullptr;
    }
  }
}

void RenderWidgetCompositor::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  delegate_->ApplyViewportDeltas(inner_delta, outer_delta,
                                 elastic_overscroll_delta, page_scale,
                                 top_controls_delta);
}

void RenderWidgetCompositor::RequestNewOutputSurface() {
  // If the host is closing, then no more compositing is possible.  This
  // prevents shutdown races between handling the close message and
  // the CreateOutputSurface task.
  if (delegate_->IsClosing())
    return;

  bool fallback =
      num_failed_recreate_attempts_ >= OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK;
  std::unique_ptr<cc::OutputSurface> surface(
      delegate_->CreateOutputSurface(fallback));

  if (!surface) {
    DidFailToInitializeOutputSurface();
    return;
  }

  DCHECK_EQ(surface->capabilities().max_frames_pending, 1);

  layer_tree_host_->SetOutputSurface(std::move(surface));
}

void RenderWidgetCompositor::DidInitializeOutputSurface() {
  num_failed_recreate_attempts_ = 0;
}

void RenderWidgetCompositor::DidFailToInitializeOutputSurface() {
  ++num_failed_recreate_attempts_;
  // Tolerate a certain number of recreation failures to work around races
  // in the output-surface-lost machinery.
  LOG_IF(FATAL, (num_failed_recreate_attempts_ >= MAX_OUTPUT_SURFACE_RETRIES))
      << "Failed to create a fallback OutputSurface.";

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderWidgetCompositor::RequestNewOutputSurface,
                            weak_factory_.GetWeakPtr()));
}

void RenderWidgetCompositor::WillCommit() {
  InvokeLayoutAndPaintCallback();
}

void RenderWidgetCompositor::DidCommit() {
  DCHECK(!temporary_copy_output_request_);
  delegate_->DidCommitCompositorFrame();
  compositor_deps_->GetRendererScheduler()->DidCommitFrameToCompositor();
}

void RenderWidgetCompositor::DidCommitAndDrawFrame() {
  delegate_->DidCommitAndDrawCompositorFrame();
}

void RenderWidgetCompositor::DidCompleteSwapBuffers() {
  delegate_->DidCompleteSwapBuffers();
  if (!threaded_)
    delegate_->OnSwapBuffersComplete();
}

void RenderWidgetCompositor::DidCompletePageScaleAnimation() {
  delegate_->DidCompletePageScaleAnimation();
}

void RenderWidgetCompositor::RequestScheduleAnimation() {
  delegate_->RequestScheduleAnimation();
}

void RenderWidgetCompositor::DidPostSwapBuffers() {
  delegate_->OnSwapBuffersPosted();
}

void RenderWidgetCompositor::DidAbortSwapBuffers() {
  delegate_->OnSwapBuffersAborted();
}

void RenderWidgetCompositor::SetProtoReceiver(ProtoReceiver* receiver) {
  remote_proto_channel_receiver_ = receiver;
}

void RenderWidgetCompositor::SendCompositorProto(
    const cc::proto::CompositorMessage& proto) {
  int signed_size = proto.ByteSize();
  size_t unsigned_size = base::checked_cast<size_t>(signed_size);
  std::vector<uint8_t> serialized(unsigned_size);
  proto.SerializeToArray(serialized.data(), signed_size);
  delegate_->ForwardCompositorProto(serialized);
}

void RenderWidgetCompositor::SetSurfaceIdNamespace(
    uint32_t surface_id_namespace) {
  layer_tree_host_->set_surface_id_namespace(surface_id_namespace);
}

void RenderWidgetCompositor::OnHandleCompositorProto(
    const std::vector<uint8_t>& proto) {
  DCHECK(remote_proto_channel_receiver_);

  std::unique_ptr<cc::proto::CompositorMessage> deserialized(
      new cc::proto::CompositorMessage);
  int signed_size = base::checked_cast<int>(proto.size());
  if (!deserialized->ParseFromArray(proto.data(), signed_size)) {
    LOG(ERROR) << "Unable to parse compositor proto.";
    return;
  }

  remote_proto_channel_receiver_->OnProtoReceived(std::move(deserialized));
}

void RenderWidgetCompositor::SetPaintedDeviceScaleFactor(
    float device_scale) {
  layer_tree_host_->SetPaintedDeviceScaleFactor(device_scale);
}

}  // namespace content
