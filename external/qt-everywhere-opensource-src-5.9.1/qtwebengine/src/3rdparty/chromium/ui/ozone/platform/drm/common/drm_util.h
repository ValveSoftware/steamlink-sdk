// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {
class Point;
}

namespace ui {

// Representation of the information required to initialize and configure a
// native display. |index| is the position of the connection and will be
// used to generate a unique identifier for the display.
class HardwareDisplayControllerInfo {
 public:
  HardwareDisplayControllerInfo(ScopedDrmConnectorPtr connector,
                                ScopedDrmCrtcPtr crtc,
                                size_t index);
  ~HardwareDisplayControllerInfo();

  drmModeConnector* connector() const { return connector_.get(); }
  drmModeCrtc* crtc() const { return crtc_.get(); }
  size_t index() const { return index_; }

 private:
  ScopedDrmConnectorPtr connector_;
  ScopedDrmCrtcPtr crtc_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerInfo);
};

// Looks-up and parses the native display configurations returning all available
// displays.
ScopedVector<HardwareDisplayControllerInfo> GetAvailableDisplayControllerInfos(
    int fd);

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs);

DisplayMode_Params CreateDisplayModeParams(const drmModeModeInfo& mode);

// |info| provides the DRM information related to the display, |fd| is the
// connection to the DRM device.
DisplaySnapshot_Params CreateDisplaySnapshotParams(
    HardwareDisplayControllerInfo* info,
    int fd,
    const base::FilePath& sys_path,
    size_t device_index,
    const gfx::Point& origin);

int GetFourCCFormatFromBufferFormat(gfx::BufferFormat format);
gfx::BufferFormat GetBufferFormatFromFourCCFormat(int format);

int GetFourCCFormatForFramebuffer(gfx::BufferFormat format);

gfx::Size GetMaximumCursorSize(int fd);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_
