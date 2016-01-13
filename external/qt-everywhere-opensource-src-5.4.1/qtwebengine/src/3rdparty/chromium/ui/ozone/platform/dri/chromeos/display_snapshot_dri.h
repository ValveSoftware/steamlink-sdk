// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_SNAPSHOT_DRI_H_
#define UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_SNAPSHOT_DRI_H_

#include "ui/display/types/chromeos/display_snapshot.h"

typedef struct _drmModeConnector drmModeConnector;
typedef struct _drmModeCrtc drmModeCrtc;
typedef struct _drmModeProperty drmModePropertyRes;

namespace ui {

class DriWrapper;

class DisplaySnapshotDri : public DisplaySnapshot {
 public:
  DisplaySnapshotDri(DriWrapper* drm,
                     drmModeConnector* connector,
                     drmModeCrtc* crtc,
                     uint32_t index);
  virtual ~DisplaySnapshotDri();

  // Native properties of a display used by the DRI implementation in
  // configuring this display.
  uint32_t connector() const { return connector_; }
  uint32_t crtc() const { return crtc_; }
  drmModePropertyRes* dpms_property() const { return dpms_property_; }

  // DisplaySnapshot overrides:
  virtual std::string ToString() const OVERRIDE;

 private:
  uint32_t connector_;
  uint32_t crtc_;
  drmModePropertyRes* dpms_property_;
  std::string name_;
  bool overscan_flag_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySnapshotDri);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_SNAPSHOT_DRI_H_
