// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_manager.h"

namespace ui {

InputDeviceManager* InputDeviceManager::instance_ = nullptr;

// static
InputDeviceManager* InputDeviceManager::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

// static
bool InputDeviceManager::HasInstance() {
  return instance_ != nullptr;
}

// static
void InputDeviceManager::SetInstance(InputDeviceManager* instance) {
  DCHECK(!instance_);
  instance_ = instance;
}

// static
void InputDeviceManager::ClearInstance() {
  instance_ = nullptr;
}

}  // namespace ui
