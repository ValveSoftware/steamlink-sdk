// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DELEGATE_H
#define DEVICE_VR_ANDROID_GVR_DELEGATE_H

#include "base/memory/weak_ptr.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_export.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_types.h"

namespace gvr {
class GvrApi;
}  // namespace gvr

namespace device {

constexpr gvr::Sizei kInvalidRenderTargetSize = {0, 0};

class DEVICE_VR_EXPORT GvrDelegate {
 public:
  virtual void SetWebVRSecureOrigin(bool secure_origin) = 0;
  virtual void SubmitWebVRFrame() = 0;
  virtual void UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                        const gvr::Rectf& right_bounds) = 0;

  virtual void SetGvrPoseForWebVr(const gvr::Mat4f& pose,
                                  uint32_t pose_index) = 0;
  virtual gvr::Sizei GetWebVRCompositorSurfaceSize() = 0;
  virtual void SetWebVRRenderSurfaceSize(int width, int height) = 0;
  virtual gvr::GvrApi* gvr_api() = 0;
};

class DEVICE_VR_EXPORT GvrDelegateProvider {
 public:
  static void SetInstance(GvrDelegateProvider* delegate_provider);
  static GvrDelegateProvider* GetInstance();

  virtual void SetDeviceProvider(
      base::WeakPtr<GvrDeviceProvider> device_provider) = 0;
  virtual void RequestWebVRPresent(
      const base::Callback<void(bool)>& callback) = 0;
  virtual void ExitWebVRPresent() = 0;
  virtual base::WeakPtr<GvrDelegate> GetNonPresentingDelegate() = 0;
  virtual void DestroyNonPresentingDelegate() = 0;
  virtual void SetListeningForActivate(bool listening) = 0;

 private:
  static GvrDelegateProvider* delegate_provider_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DELEGATE_H
