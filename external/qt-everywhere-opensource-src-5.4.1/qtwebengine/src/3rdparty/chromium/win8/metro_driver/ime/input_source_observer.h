// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_INPUT_SOURCE_OBSERVER_H_
#define WIN8_METRO_DRIVER_IME_INPUT_SOURCE_OBSERVER_H_

#include <Windows.h>

#include "base/basictypes.h"

namespace metro_driver {

// An observer interface implemented by objects that want to be informed
// when the active language profile is changed.
class InputSourceObserver {
 public:
  virtual ~InputSourceObserver() {}

  // Called when the active language profile is changed.
  virtual void OnInputSourceChanged() = 0;
};

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_INPUT_SOURCE_OBSERVER_H_
