// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_SCOPED_INPUT_DEVICE_H_
#define UI_EVENTS_OZONE_EVDEV_SCOPED_INPUT_DEVICE_H_

#include "base/scoped_generic.h"

namespace ui {
namespace internal {

struct ScopedInputDeviceCloseTraits {
  static int InvalidValue() { return -1; }
  static void Free(int fd);
};

}  // namespace internal

typedef base::ScopedGeneric<int, internal::ScopedInputDeviceCloseTraits>
    ScopedInputDevice;

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_SCOPED_INPUT_DEVICE_H_
