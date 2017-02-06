// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_

#include "base/containers/mru_cache.h"
#include "ui/ozone/platform/drm/gpu/overlay_plane.h"

namespace ui {

class DrmDevice;
class DrmWindow;
class HardwareDisplayController;
class ScanoutBufferGenerator;
struct OverlayCheck_Params;

class DrmOverlayValidator {
 public:
  DrmOverlayValidator(DrmWindow* window,
                      ScanoutBufferGenerator* buffer_generator);
  ~DrmOverlayValidator();

  // Tests if configurations |params| are compatible with |window_| and finds
  // which of these configurations can be promoted to Overlay composition
  // without failing the page flip. It expects |params| to be sorted by z_order.
  std::vector<OverlayCheck_Params> TestPageFlip(
      const std::vector<OverlayCheck_Params>& params,
      const OverlayPlaneList& last_used_planes);

  // Checks if buffers bound to planes can be optimized (i.e. format, scaling)to
  // reduce Display Read bandwidth. This should be called before an actual page
  // flip. Expects |planes| to be sorted by Z order.
  OverlayPlaneList PrepareBuffersForPageFlip(const OverlayPlaneList& planes);

  // Clears internal cache of validated overlay configurations. This should be
  // usually called when |window_| size has changed or moved controller.
  void ClearCache();

 private:
  // Contains hints which can be used to reduce display read memory bandwidth,
  // for a given OverlayPlane. These are useful in case of video to determine
  // if converting the buffer storage format before composition could lead to
  // any potential bandwidth savings. Other useful hint is to determine if
  // scaling needs to be done before page flip or can be handled by plane.
  struct OverlayHints {
    OverlayHints(uint32_t optimal_format, bool handle_scaling);
    ~OverlayHints();
    // Optimal buffer storage format supported by hardware for a given
    // OverlayPlane. This hint can be ignored and still compositing an
    // OverlayPlane should not fail page flip or cause any visual artifacts.
    uint32_t optimal_format;
    // Hints if buffer scaling needs to be done before page flip as plane cannot
    // support it. Ignoring this hint may result in displaying buffer with wrong
    // resolution.
    bool handle_scaling;
  };

  using OverlayHintsList = std::vector<OverlayHints>;

  // Update hints cache.
  void UpdateOverlayHintsCache(const OverlayPlaneList& plane_list);

  DrmWindow* window_;  // Not owned.
  ScanoutBufferGenerator* buffer_generator_;  // Not owned.

  // List of all configurations which have been validated.
  base::MRUCache<OverlayPlaneList, OverlayHintsList> overlay_hints_cache_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayValidator);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_
