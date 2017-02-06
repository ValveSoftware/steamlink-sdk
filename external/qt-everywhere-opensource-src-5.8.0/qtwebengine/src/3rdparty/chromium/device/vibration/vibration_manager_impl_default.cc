// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "device/vibration/vibration_manager_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

namespace {

class VibrationManagerEmptyImpl : public VibrationManager {
 public:
  void Vibrate(int64_t milliseconds, const VibrateCallback& callback) override {
    callback.Run();
  }

  void Cancel(const CancelCallback& callback) override { callback.Run(); }

 private:
  friend VibrationManagerImpl;

  explicit VibrationManagerEmptyImpl(
      mojo::InterfaceRequest<VibrationManager> request)
      : binding_(this, std::move(request)) {}
  ~VibrationManagerEmptyImpl() override {}

  // The binding between this object and the other end of the pipe.
  mojo::StrongBinding<VibrationManager> binding_;
};

}  // namespace

// static
void VibrationManagerImpl::Create(
    mojo::InterfaceRequest<VibrationManager> request) {
  new VibrationManagerEmptyImpl(std::move(request));
}

}  // namespace device
