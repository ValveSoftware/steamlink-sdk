// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_CARDBOARD_VR_DEVICE_H
#define DEVICE_VR_CARDBOARD_VR_DEVICE_H

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "device/vr/vr_device.h"

namespace device {

class CardboardVRDevice : public VRDevice {
 public:
  static bool RegisterCardboardVRDevice(JNIEnv* env);

  explicit CardboardVRDevice(VRDeviceProvider* provider);
  ~CardboardVRDevice() override;

  VRDisplayPtr GetVRDevice() override;
  VRPosePtr GetPose() override;
  void ResetPose() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_cardboard_device_;
  base::android::ScopedJavaGlobalRef<jfloatArray> j_head_matrix_;

  DISALLOW_COPY_AND_ASSIGN(CardboardVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_CARDBOARD_VR_DEVICE_H
