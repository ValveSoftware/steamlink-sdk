// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include <jni.h>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_controller.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_types.h"

using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace device {

GvrDeviceProvider::GvrDeviceProvider() : weak_ptr_factory_(this) {}

GvrDeviceProvider::~GvrDeviceProvider() {
  GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      GAMEPAD_SOURCE_GVR);

  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (delegate_provider) {
    delegate_provider->ExitWebVRPresent();
    delegate_provider->DestroyNonPresentingDelegate();
  }
}

void GvrDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  Initialize();

  if (vr_device_)
    devices->push_back(vr_device_.get());
}

void GvrDeviceProvider::Initialize() {
  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (!delegate_provider)
    return;
  delegate_provider->SetDeviceProvider(weak_ptr_factory_.GetWeakPtr());
  if (!vr_device_) {
    vr_device_.reset(
        new GvrDevice(this, delegate_provider->GetNonPresentingDelegate()));
  }
}

void GvrDeviceProvider::RequestPresent(
    const base::Callback<void(bool)>& callback) {
  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (!delegate_provider)
    return callback.Run(false);

  // RequestWebVRPresent is async as a render thread may be created.
  delegate_provider->RequestWebVRPresent(callback);
}

// VR presentation exit requested by the API.
void GvrDeviceProvider::ExitPresent() {
  SwitchToNonPresentingDelegate();
  // If we're presenting currently stop.
  GvrDelegateProvider* delegate_provider = GvrDelegateProvider::GetInstance();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();
}

void GvrDeviceProvider::OnGvrDelegateReady(
    const base::WeakPtr<GvrDelegate>& delegate) {
  if (!vr_device_)
    return;
  VLOG(1) << "Switching to presenting delegate";
  vr_device_->SetDelegate(delegate);
  GamepadDataFetcherManager::GetInstance()->AddFactory(
      new GvrGamepadDataFetcher::Factory(delegate, vr_device_->id()));
}

// VR presentation exit requested by the delegate (probably via UI).
void GvrDeviceProvider::OnGvrDelegateRemoved() {
  if (!vr_device_)
    return;

  SwitchToNonPresentingDelegate();
  vr_device_->OnExitPresent();
}

void GvrDeviceProvider::OnDisplayBlur() {
  if (!vr_device_)
    return;
  vr_device_->OnBlur();
}

void GvrDeviceProvider::OnDisplayFocus() {
  if (!vr_device_)
    return;
  vr_device_->OnFocus();
}

void GvrDeviceProvider::OnDisplayActivate() {
  if (!vr_device_)
    return;
  vr_device_->OnActivate(mojom::VRDisplayEventReason::MOUNTED);
}

void GvrDeviceProvider::SwitchToNonPresentingDelegate() {
  GvrDelegateProvider* delegate_provider = GvrDelegateProvider::GetInstance();
  if (!vr_device_ || !delegate_provider)
    return;

  VLOG(1) << "Switching to non-presenting delegate";
  vr_device_->SetDelegate(delegate_provider->GetNonPresentingDelegate());

  // Remove GVR gamepad polling.
  GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      GAMEPAD_SOURCE_GVR);
}

void GvrDeviceProvider::SetListeningForActivate(bool listening) {
  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (!delegate_provider)
    return;

  delegate_provider->SetListeningForActivate(listening);
}

}  // namespace device
