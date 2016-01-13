// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_UTIL_H_
#define UI_OZONE_PLATFORM_DRI_DRI_UTIL_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"

typedef struct _drmModeConnector drmModeConnector;
typedef struct _drmModeCrtc drmModeCrtc;
typedef struct _drmModeModeInfo drmModeModeInfo;
typedef struct _drmModeRes drmModeRes;

namespace ui {

// Representation of the information required to initialize and configure a
// native display.
class HardwareDisplayControllerInfo {
 public:
  HardwareDisplayControllerInfo(drmModeConnector* connector, drmModeCrtc* crtc);
  ~HardwareDisplayControllerInfo();

  drmModeConnector* connector() const { return connector_; }
  drmModeCrtc* crtc() const { return crtc_; }

 private:
  drmModeConnector* connector_;
  drmModeCrtc* crtc_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerInfo);
};

// Looks-up and parses the native display configurations returning all available
// displays.
ScopedVector<HardwareDisplayControllerInfo>
GetAvailableDisplayControllerInfos(int fd, drmModeRes* resources);

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_UTIL_H_
