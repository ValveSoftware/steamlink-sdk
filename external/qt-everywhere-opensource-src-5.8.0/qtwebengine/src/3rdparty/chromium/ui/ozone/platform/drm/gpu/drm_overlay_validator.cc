// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_validator.h"

#include <drm_fourcc.h>

#include "ui/gfx/geometry/size_conversions.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

namespace {

const size_t kMaxCacheSize = 200;

bool NeedsAlphaComposition(uint32_t format) {
  switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_UYVY:
      return false;
    default:
      return true;
  }
}

scoped_refptr<ScanoutBuffer> GetBufferForPageFlipTest(
    const scoped_refptr<DrmDevice>& drm_device,
    const gfx::Size& size,
    uint32_t format,
    ScanoutBufferGenerator* buffer_generator,
    std::vector<scoped_refptr<ScanoutBuffer>>* reusable_buffers) {
  // Check if we can re-use existing buffers.
  for (const auto& buffer : *reusable_buffers) {
    if (buffer->GetFramebufferPixelFormat() == format &&
        buffer->GetSize() == size) {
      return buffer;
    }
  }

  gfx::BufferFormat buffer_format = GetBufferFormatFromFourCCFormat(format);
  scoped_refptr<ScanoutBuffer> scanout_buffer =
      buffer_generator->Create(drm_device, buffer_format, size);
  reusable_buffers->push_back(scanout_buffer);

  return scanout_buffer;
}

gfx::Size GetScaledSize(const gfx::Size original_size,
                        const gfx::Rect display_rect,
                        const gfx::RectF crop_rect) {
  if (!crop_rect.IsEmpty()) {
    return gfx::ToCeiledSize(
        gfx::SizeF(display_rect.width() / crop_rect.width(),
                   display_rect.height() / crop_rect.height()));
  }

  return original_size;
}

uint32_t FindOptimalBufferFormat(uint32_t original_format,
                                 uint32_t plane_z_order,
                                 const gfx::Rect& plane_bounds,
                                 const gfx::Rect& window_bounds,
                                 HardwareDisplayController* controller) {
  bool force_primary_format = false;
  uint32_t z_order = plane_z_order;
  // If Overlay completely covers primary and isn't transparent, try to find
  // optimal format w.r.t primary plane. This guarantees that optimal format
  // would not fail page flip when plane manager/CC collapses planes.
  if (plane_bounds == window_bounds &&
      !NeedsAlphaComposition(original_format)) {
    z_order = 0;
#if !defined(USE_DRM_ATOMIC)
    // Page flip can fail when trying to flip a buffer of format other than
    // what was used during Modeset on non atomic kernels. There is no
    // definitive way to query this.
    force_primary_format = true;
#endif
  }

  if (force_primary_format)
    return DRM_FORMAT_XRGB8888;

  // YUV is preferable format if supported.
  if (controller->IsFormatSupported(DRM_FORMAT_UYVY, z_order)) {
    return DRM_FORMAT_UYVY;
  } else if (controller->IsFormatSupported(DRM_FORMAT_XRGB8888, z_order)) {
    return DRM_FORMAT_XRGB8888;
  }

  return original_format;
}

}  // namespace

DrmOverlayValidator::OverlayHints::OverlayHints(uint32_t format,
                                                bool scale_buffer)
    : optimal_format(format), handle_scaling(scale_buffer) {}

DrmOverlayValidator::OverlayHints::~OverlayHints() {}

DrmOverlayValidator::DrmOverlayValidator(
    DrmWindow* window,
    ScanoutBufferGenerator* buffer_generator)
    : window_(window),
      buffer_generator_(buffer_generator),
      overlay_hints_cache_(kMaxCacheSize) {}

DrmOverlayValidator::~DrmOverlayValidator() {}

std::vector<OverlayCheck_Params> DrmOverlayValidator::TestPageFlip(
    const std::vector<OverlayCheck_Params>& params,
    const OverlayPlaneList& last_used_planes) {
  std::vector<OverlayCheck_Params> validated_params = params;
  HardwareDisplayController* controller = window_->GetController();
  if (!controller) {
    // Nothing much we can do here.
    for (auto& overlay : validated_params)
      overlay.is_overlay_candidate = false;

    return validated_params;
  }

  OverlayPlaneList test_list;
  std::vector<scoped_refptr<ScanoutBuffer>> reusable_buffers;
  scoped_refptr<DrmDevice> drm = controller->GetAllocationDrmDevice();

  for (const auto& plane : last_used_planes)
    reusable_buffers.push_back(plane.buffer);

  for (auto& overlay : validated_params) {
    if (!overlay.is_overlay_candidate)
      continue;

    gfx::Size scaled_buffer_size = GetScaledSize(
        overlay.buffer_size, overlay.display_rect, overlay.crop_rect);

    uint32_t original_format = GetFourCCFormatForFramebuffer(overlay.format);
    scoped_refptr<ScanoutBuffer> buffer =
        GetBufferForPageFlipTest(drm, scaled_buffer_size, original_format,
                                 buffer_generator_, &reusable_buffers);
    DCHECK(buffer);

    OverlayPlane plane(buffer, overlay.plane_z_order, overlay.transform,
                       overlay.display_rect, overlay.crop_rect);
    test_list.push_back(plane);

    if (controller->TestPageFlip(test_list)) {
      overlay.is_overlay_candidate = true;

      // If size scaling is needed, find an optimal format.
      if (overlay.plane_z_order && scaled_buffer_size != overlay.buffer_size) {
        uint32_t optimal_format = FindOptimalBufferFormat(
            original_format, overlay.plane_z_order, overlay.display_rect,
            window_->bounds(), controller);

        if (original_format != optimal_format) {
          OverlayPlane original_plain = test_list.back();
          test_list.pop_back();
          scoped_refptr<ScanoutBuffer> optimal_buffer =
              GetBufferForPageFlipTest(drm, scaled_buffer_size, optimal_format,
                                       buffer_generator_, &reusable_buffers);
          DCHECK(optimal_buffer);

          OverlayPlane optimal_plane(optimal_buffer, overlay.plane_z_order,
                                     overlay.transform, overlay.display_rect,
                                     overlay.crop_rect);
          test_list.push_back(optimal_plane);

          // If test failed here, it means even though optimal_format is
          // supported, platform cannot support it with current combination of
          // layers. This is usually the case when optimal_format needs certain
          // capabilites (i.e. conversion, scaling etc) and needed hardware
          // resources might be already in use. Fall back to original format.
          if (!controller->TestPageFlip(test_list)) {
            test_list.pop_back();
            test_list.push_back(original_plain);
          }
        }
      }
    } else {
      // If test failed here, platform cannot support this configuration
      // with current combination of layers. This is usually the case when this
      // plane has requested post processing capability which needs additional
      // hardware resources and they might be already in use by other planes.
      // For example this plane has requested scaling capabilities and all
      // available scalars are already in use by other planes.
      DCHECK(test_list.size() > 1);
      overlay.is_overlay_candidate = false;
      test_list.pop_back();
    }
  }

  UpdateOverlayHintsCache(test_list);

  return validated_params;
}

OverlayPlaneList DrmOverlayValidator::PrepareBuffersForPageFlip(
    const OverlayPlaneList& planes) {
  if (planes.size() <= 1)
    return planes;

  HardwareDisplayController* controller = window_->GetController();
  if (!controller)
    return planes;

  OverlayPlaneList pending_planes = planes;
  const auto& overlay_hints = overlay_hints_cache_.Get(planes);

  size_t size = planes.size();
  bool use_hints = overlay_hints != overlay_hints_cache_.end();

  for (size_t i = 0; i < size; i++) {
    auto& plane = pending_planes.at(i);
    if (plane.processing_callback.is_null())
      continue;

    uint32_t original_format = plane.buffer->GetFramebufferPixelFormat();
    uint32_t target_format = original_format;

    const gfx::Size& original_size = plane.buffer->GetSize();
    gfx::Size target_size =
        GetScaledSize(original_size, plane.display_bounds, plane.crop_rect);

    if (use_hints) {
      DCHECK(size == overlay_hints->second.size());
      const OverlayHints& hints = overlay_hints->second.at(i);
      target_format = hints.optimal_format;

      // We can handle plane scaling, avoid scaling buffer here.
      if (!hints.handle_scaling)
        target_size = original_size;
    }

    // The size scaling piggybacks the format conversion.
    if (original_size != target_size) {
      scoped_refptr<ScanoutBuffer> processed_buffer =
          plane.processing_callback.Run(target_size, target_format);

      if (processed_buffer)
        plane.buffer = processed_buffer;
    }
  }

  return pending_planes;
}

void DrmOverlayValidator::ClearCache() {
  overlay_hints_cache_.Clear();
}

void DrmOverlayValidator::UpdateOverlayHintsCache(
    const OverlayPlaneList& plane_list) {
  const auto& iter = overlay_hints_cache_.Get(plane_list);
  if (iter != overlay_hints_cache_.end())
    return;

  OverlayPlaneList hints_plane_list = plane_list;
  OverlayHintsList overlay_hints;
  for (auto& plane : hints_plane_list) {
    uint32_t format = plane.buffer->GetFramebufferPixelFormat();
    // TODO(kalyank): We always request scaling to be done by 3D engine, VPP
    // etc. We should use them only if downscaling is needed and let display
    // controller handle up-scaling on platforms which support it.
    overlay_hints.push_back(OverlayHints(format, true /* scaling */));

    // Make sure we dont hold reference to buffer when caching this plane list.
    plane.buffer = nullptr;
  }

  DCHECK(hints_plane_list.size() == overlay_hints.size());
  overlay_hints_cache_.Put(hints_plane_list, overlay_hints);
}

}  // namespace ui
