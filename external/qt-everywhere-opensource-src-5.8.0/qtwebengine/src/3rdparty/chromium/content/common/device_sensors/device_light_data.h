// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DEVICE_SENSORS_DEVICE_LIGHT_DATA_H_
#define CONTENT_COMMON_DEVICE_SENSORS_DEVICE_LIGHT_DATA_H_

#include "content/common/content_export.h"

namespace content {

// This struct is intentionally POD and fixed size so that it can be stored
// in shared memory between the browser and the renderer processes.
// POD class should be a struct, should have an inline cstor that uses
// initializer lists and no dstor.
struct DeviceLightData {
  DeviceLightData() : value(-1) {}
  double value;
};

}  // namespace content

#endif  // CONTENT_COMMON_DEVICE_SENSORS_DEVICE_LIGHT_DATA_H_
