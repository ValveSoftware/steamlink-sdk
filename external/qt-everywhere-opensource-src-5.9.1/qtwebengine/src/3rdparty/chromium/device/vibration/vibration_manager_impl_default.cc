// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/memory/ptr_util.h"
#include "device/vibration/vibration_manager_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

namespace {

class VibrationManagerEmptyImpl : public VibrationManager {
 public:
  VibrationManagerEmptyImpl() {}
  ~VibrationManagerEmptyImpl() override {}

  void Vibrate(int64_t milliseconds, const VibrateCallback& callback) override {
    callback.Run();
  }

  void Cancel(const CancelCallback& callback) override { callback.Run(); }
};

}  // namespace

// static
void VibrationManagerImpl::Create(VibrationManagerRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<VibrationManagerEmptyImpl>(),
                          std::move(request));
}

}  // namespace device
