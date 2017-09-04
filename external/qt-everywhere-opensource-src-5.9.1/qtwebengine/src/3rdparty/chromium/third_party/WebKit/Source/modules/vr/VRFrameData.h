// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRFrameData_h
#define VRFrameData_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/dom/DOMTypedArray.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class VREyeParameters;
class VRPose;

class VRFrameData final : public GarbageCollected<VRFrameData>,
                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRFrameData* create() { return new VRFrameData(); }

  VRFrameData();

  DOMHighResTimeStamp timestamp() const { return m_timestamp; }
  DOMFloat32Array* leftProjectionMatrix() const {
    return m_leftProjectionMatrix;
  }
  DOMFloat32Array* leftViewMatrix() const { return m_leftViewMatrix; }
  DOMFloat32Array* rightProjectionMatrix() const {
    return m_rightProjectionMatrix;
  }
  DOMFloat32Array* rightViewMatrix() const { return m_rightViewMatrix; }
  VRPose* pose() const { return m_pose; }

  // Populate a the VRFrameData with a pose and the necessary eye parameters.
  // TODO(bajones): The full frame data should be provided by the VRService,
  // not computed here.
  bool update(const device::mojom::blink::VRPosePtr&,
              VREyeParameters* leftEye,
              VREyeParameters* rightEye,
              float depthNear,
              float depthFar);

  DECLARE_VIRTUAL_TRACE()

 private:
  DOMHighResTimeStamp m_timestamp;
  Member<DOMFloat32Array> m_leftProjectionMatrix;
  Member<DOMFloat32Array> m_leftViewMatrix;
  Member<DOMFloat32Array> m_rightProjectionMatrix;
  Member<DOMFloat32Array> m_rightViewMatrix;
  Member<VRPose> m_pose;
};

}  // namespace blink

#endif  // VRStageParameters_h
