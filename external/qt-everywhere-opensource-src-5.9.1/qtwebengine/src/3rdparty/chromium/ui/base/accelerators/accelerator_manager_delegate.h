// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_DELEGATE_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_DELEGATE_H_

#include "ui/base/ui_base_export.h"

namespace ui {

class Accelerator;

class UI_BASE_EXPORT AcceleratorManagerDelegate {
 public:
  // Called the first time a target is registered for |accelerator|. This is
  // only called the first time a target is registered for a unique accelerator.
  // For example, if Register() is called twice with the same accelerator
  // this is called only for the first call.
  virtual void OnAcceleratorRegistered(const Accelerator& accelerator) = 0;

  // Called when there no more targets are registered for |accelerator|.
  virtual void OnAcceleratorUnregistered(const Accelerator& accelerator) = 0;

 protected:
  virtual ~AcceleratorManagerDelegate() {}
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_DELEGATE_H_
