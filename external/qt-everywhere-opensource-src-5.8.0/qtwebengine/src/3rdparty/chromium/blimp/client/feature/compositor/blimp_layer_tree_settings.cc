// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_layer_tree_settings.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_info.h"
#include "cc/base/switches.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_switches.h"

namespace blimp {
namespace client {

// TODO(dtrainor): This is temporary to get the compositor up and running.
// Much of this will either have to be pulled from the server or refactored to
// share the settings from render_widget_compositor.cc.
void PopulateCommonLayerTreeSettings(cc::LayerTreeSettings* settings) {
  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings->layer_transforms_should_scale_layer_contents = true;

  settings->main_frame_before_activation_enabled = false;
  settings->default_tile_size = gfx::Size(256, 256);
  settings->gpu_rasterization_msaa_sample_count = 0;
  settings->gpu_rasterization_forced = false;
  settings->gpu_rasterization_enabled = false;
  settings->can_use_lcd_text = false;
  settings->use_distance_field_text = false;
#if defined(OS_MACOSX)
  settings->use_zero_copy = true;
#else
  settings->use_zero_copy = false;
#endif
  settings->enable_elastic_overscroll = false;
  settings->image_decode_tasks_enabled = false;
  settings->single_thread_proxy_scheduler = false;
  settings->initial_debug_state.show_debug_borders = false;
  settings->initial_debug_state.show_fps_counter = false;
  settings->initial_debug_state.show_layer_animation_bounds_rects = false;
  settings->initial_debug_state.show_paint_rects = false;
  settings->initial_debug_state.show_property_changed_rects = false;
  settings->initial_debug_state.show_surface_damage_rects = false;
  settings->initial_debug_state.show_screen_space_rects = false;
  settings->initial_debug_state.show_replica_screen_space_rects = false;
  settings->initial_debug_state.SetRecordRenderingStats(false);

#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice())
    settings->gpu_rasterization_enabled = false;
  settings->using_synchronous_renderer_compositor = false;
  settings->scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
  settings->scrollbar_fade_delay_ms = 300;
  settings->scrollbar_fade_resize_delay_ms = 2000;
  settings->scrollbar_fade_duration_ms = 300;
  settings->solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  settings->renderer_settings.highp_threshold_min = 2048;
  settings->ignore_root_layer_flings = false;
  bool use_low_memory_policy = base::SysInfo::IsLowEndDevice();
  if (use_low_memory_policy) {
    // On low-end we want to be very carefull about killing other
    // apps. So initially we use 50% more memory to avoid flickering
    // or raster-on-demand.
    settings->max_memory_for_prepaint_percentage = 67;

    settings->renderer_settings.preferred_tile_format = cc::RGBA_4444;
  } else {
    // On other devices we have increased memory excessively to avoid
    // raster-on-demand already, so now we reserve 50% _only_ to avoid
    // raster-on-demand, and use 50% of the memory otherwise.
    settings->max_memory_for_prepaint_percentage = 50;
  }
  settings->renderer_settings.should_clear_root_render_pass = true;

  // TODO(danakj): Only do this on low end devices.
  settings->create_low_res_tiling = true;

// TODO(dtrainor): Investigate whether or not we want to use an external
// source here.
// settings->use_external_begin_frame_source = true;

#elif !defined(OS_MACOSX)
  settings->scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
  settings->solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  settings->scrollbar_fade_delay_ms = 500;
  settings->scrollbar_fade_resize_delay_ms = 500;
  settings->scrollbar_fade_duration_ms = 300;

  // When pinching in, only show the pinch-viewport overlay scrollbars if the
  // page scale is at least some threshold away from the minimum. i.e. don't
  // show the pinch scrollbars when at minimum scale.
  // TODO(dtrainor): Update this since https://crrev.com/1267603004 landed.
  // settings->scrollbar_show_scale_threshold = 1.05f;
#endif

  // Set the GpuMemoryPolicy.
  cc::ManagedMemoryPolicy memory_policy = settings->memory_policy_;
  memory_policy.bytes_limit_when_visible = 0;

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
  if (memory_policy.bytes_limit_when_visible == 0) {
    // NOTE: Non-low-end devices use only 50% of these limits,
    // except during 'emergencies' where 100% can be used.
    if (!base::SysInfo::IsLowEndDevice()) {
      if (physical_memory_mb >= 1536)
        memory_policy.bytes_limit_when_visible =
            physical_memory_mb / 8;  // >192MB
      else if (physical_memory_mb >= 1152)
        memory_policy.bytes_limit_when_visible =
            physical_memory_mb / 8;  // >144MB
      else if (physical_memory_mb >= 768)
        memory_policy.bytes_limit_when_visible =
            physical_memory_mb / 10;  // >76MB
      else
        memory_policy.bytes_limit_when_visible =
            physical_memory_mb / 12;  // <64MB
    } else {
      // Low-end devices have 512MB or less memory by definition
      // so we hard code the limit rather than relying on the heuristics
      // above. Low-end devices use 4444 textures so we can use a lower limit.
      memory_policy.bytes_limit_when_visible = 8;
    }
    memory_policy.bytes_limit_when_visible =
        memory_policy.bytes_limit_when_visible * 1024 * 1024;
    // Clamp the observed value to a specific range on Android.
    memory_policy.bytes_limit_when_visible = std::max(
        memory_policy.bytes_limit_when_visible,
        static_cast<size_t>(8 * 1024 * 1024));
    memory_policy.bytes_limit_when_visible =
        std::min(memory_policy.bytes_limit_when_visible,
                 static_cast<size_t>(256 * 1024 * 1024));
  }
  memory_policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
#else
  // Ignore what the system said and give all clients the same maximum
  // allocation on desktop platforms.
  memory_policy.bytes_limit_when_visible = 512 * 1024 * 1024;
  memory_policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
#endif
}

}  // namespace client
}  // namespace blimp
