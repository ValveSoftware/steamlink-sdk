// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/overlay_plane.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class CrtcController;
class DrmDevice;

// This contains the list of planes controlled by one HDC on a given DRM fd.
// It is owned by the HDC and filled by the CrtcController.
struct HardwareDisplayPlaneList {
  HardwareDisplayPlaneList();
  ~HardwareDisplayPlaneList();

  // This is the list of planes to be committed this time.
  std::vector<HardwareDisplayPlane*> plane_list;
  // This is the list of planes that was committed last time.
  std::vector<HardwareDisplayPlane*> old_plane_list;

  struct PageFlipInfo {
    PageFlipInfo(uint32_t crtc_id, uint32_t framebuffer, CrtcController* crtc);
    PageFlipInfo(const PageFlipInfo& other);
    ~PageFlipInfo();

    uint32_t crtc_id;
    uint32_t framebuffer;
    CrtcController* crtc;

    struct Plane {
      Plane(int plane,
            int framebuffer,
            const gfx::Rect& bounds,
            const gfx::Rect& src_rect);
      ~Plane();
      int plane;
      int framebuffer;
      gfx::Rect bounds;
      gfx::Rect src_rect;
    };
    std::vector<Plane> planes;
  };
  // In the case of non-atomic operation, this info will be used for
  // pageflipping.
  std::vector<PageFlipInfo> legacy_page_flips;

#if defined(USE_DRM_ATOMIC)
  ScopedDrmAtomicReqPtr atomic_property_set;
#endif  // defined(USE_DRM_ATOMIC)
};

class HardwareDisplayPlaneManager {
 public:
  HardwareDisplayPlaneManager();
  virtual ~HardwareDisplayPlaneManager();

  // This parses information from the drm driver, adding any new planes
  // or crtcs found.
  bool Initialize(DrmDevice* drm);

  // Clears old frame state out. Must be called before any AssignOverlayPlanes
  // calls.
  void BeginFrame(HardwareDisplayPlaneList* plane_list);

  // Assign hardware planes from the |planes_| list to |overlay_list| entries,
  // recording the plane IDs in the |plane_list|. Only planes compatible with
  // |crtc_id| will be used. |overlay_list| must be sorted bottom-to-top.
  virtual bool AssignOverlayPlanes(HardwareDisplayPlaneList* plane_list,
                                   const OverlayPlaneList& overlay_list,
                                   uint32_t crtc_id,
                                   CrtcController* crtc);

  // Commit the plane states in |plane_list|.
  virtual bool Commit(HardwareDisplayPlaneList* plane_list,
                      bool test_only) = 0;

  const std::vector<std::unique_ptr<HardwareDisplayPlane>>& planes() {
    return planes_;
  }

  // Returns all formats which can be scanned out by this PlaneManager. Use
  // IsFormatSupported to find if a given format is supported on a particular
  // plane for a given crtc.
  const std::vector<uint32_t>& GetSupportedFormats() const;
  bool IsFormatSupported(uint32_t fourcc_format,
                         uint32_t z_order,
                         uint32_t crtc_id) const;

 protected:
  virtual bool SetPlaneData(HardwareDisplayPlaneList* plane_list,
                            HardwareDisplayPlane* hw_plane,
                            const OverlayPlane& overlay,
                            uint32_t crtc_id,
                            const gfx::Rect& src_rect,
                            CrtcController* crtc) = 0;

  virtual std::unique_ptr<HardwareDisplayPlane> CreatePlane(
      uint32_t plane_id,
      uint32_t possible_crtcs);

  // Finds the plane located at or after |*index| that is not in use and can
  // be used with |crtc_index|.
  HardwareDisplayPlane* FindNextUnusedPlane(size_t* index,
                                            uint32_t crtc_index,
                                            const OverlayPlane& overlay) const;

  // Convert |crtc_id| into an index, returning -1 if the ID couldn't be found.
  int LookupCrtcIndex(uint32_t crtc_id) const;

  // Returns true if |plane| can support |overlay| and compatible with
  // |crtc_index|.
  bool IsCompatible(HardwareDisplayPlane* plane,
                    const OverlayPlane& overlay,
                    uint32_t crtc_index) const;

  void ResetCurrentPlaneList(HardwareDisplayPlaneList* plane_list) const;

  // Populates scanout formats supported by all planes.
  void PopulateSupportedFormats();

  // Object containing the connection to the graphics device and wraps the API
  // calls to control it. Not owned.
  DrmDevice* drm_;

  std::vector<std::unique_ptr<HardwareDisplayPlane>> planes_;
  std::vector<uint32_t> crtcs_;
  std::vector<uint32_t> supported_formats_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_H_
