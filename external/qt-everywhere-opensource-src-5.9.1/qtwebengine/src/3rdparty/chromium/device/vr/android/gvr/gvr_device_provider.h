// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"

namespace device {

class GvrDelegateProvider;
class GvrDelegate;
class GvrDevice;
class VRServiceImpl;

class DEVICE_VR_EXPORT GvrDeviceProvider : public VRDeviceProvider {
 public:
  GvrDeviceProvider();
  ~GvrDeviceProvider() override;

  void GetDevices(std::vector<VRDevice*>* devices) override;
  void Initialize() override;

  void SetListeningForActivate(bool listening) override;

  // Called from GvrDevice.
  void RequestPresent(const base::Callback<void(bool)>& callback);
  void ExitPresent();

  void OnGvrDelegateReady(const base::WeakPtr<GvrDelegate>& delegate);
  void OnGvrDelegateRemoved();
  void OnDisplayBlur();
  void OnDisplayFocus();
  void OnDisplayActivate();

 private:
  void SwitchToNonPresentingDelegate();

  std::unique_ptr<GvrDevice> vr_device_;

  base::WeakPtrFactory<GvrDeviceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GvrDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H
