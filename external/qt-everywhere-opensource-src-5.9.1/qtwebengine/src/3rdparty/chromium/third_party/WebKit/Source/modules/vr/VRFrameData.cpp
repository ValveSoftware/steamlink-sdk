// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRFrameData.h"

#include "modules/vr/VREyeParameters.h"
#include "modules/vr/VRPose.h"

#include <cmath>

namespace blink {

// TODO(bajones): All of the matrix math here is temporary. It will be removed
// once the VRService has been updated to allow the underlying VR APIs to
// provide the projection and view matrices directly.

// Build a projection matrix from a field of view and near/far planes.
void projectionFromFieldOfView(DOMFloat32Array* outArray,
                               VRFieldOfView* fov,
                               float depthNear,
                               float depthFar) {
  float upTan = tanf(fov->upDegrees() * M_PI / 180.0);
  float downTan = tanf(fov->downDegrees() * M_PI / 180.0);
  float leftTan = tanf(fov->leftDegrees() * M_PI / 180.0);
  float rightTan = tanf(fov->rightDegrees() * M_PI / 180.0);
  float xScale = 2.0f / (leftTan + rightTan);
  float yScale = 2.0f / (upTan + downTan);

  float* out = outArray->data();
  out[0] = xScale;
  out[1] = 0.0f;
  out[2] = 0.0f;
  out[3] = 0.0f;
  out[4] = 0.0f;
  out[5] = yScale;
  out[6] = 0.0f;
  out[7] = 0.0f;
  out[8] = -((leftTan - rightTan) * xScale * 0.5);
  out[9] = ((upTan - downTan) * yScale * 0.5);
  out[10] = (depthNear + depthFar) / (depthNear - depthFar);
  out[11] = -1.0f;
  out[12] = 0.0f;
  out[13] = 0.0f;
  out[14] = (2 * depthFar * depthNear) / (depthNear - depthFar);
  out[15] = 0.0f;
}

// Create a matrix from a rotation and translation.
void matrixfromRotationTranslation(
    DOMFloat32Array* outArray,
    const WTF::Optional<WTF::Vector<float>>& rotation,
    const WTF::Optional<WTF::Vector<float>>& translation) {
  // Quaternion math
  float x = !rotation ? 0.0f : rotation.value()[0];
  float y = !rotation ? 0.0f : rotation.value()[1];
  float z = !rotation ? 0.0f : rotation.value()[2];
  float w = !rotation ? 1.0f : rotation.value()[3];
  float x2 = x + x;
  float y2 = y + y;
  float z2 = z + z;

  float xx = x * x2;
  float xy = x * y2;
  float xz = x * z2;
  float yy = y * y2;
  float yz = y * z2;
  float zz = z * z2;
  float wx = w * x2;
  float wy = w * y2;
  float wz = w * z2;

  float* out = outArray->data();
  out[0] = 1 - (yy + zz);
  out[1] = xy + wz;
  out[2] = xz - wy;
  out[3] = 0;
  out[4] = xy - wz;
  out[5] = 1 - (xx + zz);
  out[6] = yz + wx;
  out[7] = 0;
  out[8] = xz + wy;
  out[9] = yz - wx;
  out[10] = 1 - (xx + yy);
  out[11] = 0;
  out[12] = !translation ? 0.0f : translation.value()[0];
  out[13] = !translation ? 0.0f : translation.value()[1];
  out[14] = !translation ? 0.0f : translation.value()[2];
  out[15] = 1;
}

// Translate a matrix
void matrixTranslate(DOMFloat32Array* outArray,
                     const DOMFloat32Array* translation) {
  if (!translation)
    return;

  float x = translation->data()[0];
  float y = translation->data()[1];
  float z = translation->data()[2];

  float* out = outArray->data();
  out[12] = out[0] * x + out[4] * y + out[8] * z + out[12];
  out[13] = out[1] * x + out[5] * y + out[9] * z + out[13];
  out[14] = out[2] * x + out[6] * y + out[10] * z + out[14];
  out[15] = out[3] * x + out[7] * y + out[11] * z + out[15];
}

bool matrixInvert(DOMFloat32Array* outArray) {
  float* out = outArray->data();
  float a00 = out[0];
  float a01 = out[1];
  float a02 = out[2];
  float a03 = out[3];
  float a10 = out[4];
  float a11 = out[5];
  float a12 = out[6];
  float a13 = out[7];
  float a20 = out[8];
  float a21 = out[9];
  float a22 = out[10];
  float a23 = out[11];
  float a30 = out[12];
  float a31 = out[13];
  float a32 = out[14];
  float a33 = out[15];

  float b00 = a00 * a11 - a01 * a10;
  float b01 = a00 * a12 - a02 * a10;
  float b02 = a00 * a13 - a03 * a10;
  float b03 = a01 * a12 - a02 * a11;
  float b04 = a01 * a13 - a03 * a11;
  float b05 = a02 * a13 - a03 * a12;
  float b06 = a20 * a31 - a21 * a30;
  float b07 = a20 * a32 - a22 * a30;
  float b08 = a20 * a33 - a23 * a30;
  float b09 = a21 * a32 - a22 * a31;
  float b10 = a21 * a33 - a23 * a31;
  float b11 = a22 * a33 - a23 * a32;

  // Calculate the determinant
  float det =
      b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

  if (!det)
    return false;

  det = 1.0 / det;

  out[0] = (a11 * b11 - a12 * b10 + a13 * b09) * det;
  out[1] = (a02 * b10 - a01 * b11 - a03 * b09) * det;
  out[2] = (a31 * b05 - a32 * b04 + a33 * b03) * det;
  out[3] = (a22 * b04 - a21 * b05 - a23 * b03) * det;
  out[4] = (a12 * b08 - a10 * b11 - a13 * b07) * det;
  out[5] = (a00 * b11 - a02 * b08 + a03 * b07) * det;
  out[6] = (a32 * b02 - a30 * b05 - a33 * b01) * det;
  out[7] = (a20 * b05 - a22 * b02 + a23 * b01) * det;
  out[8] = (a10 * b10 - a11 * b08 + a13 * b06) * det;
  out[9] = (a01 * b08 - a00 * b10 - a03 * b06) * det;
  out[10] = (a30 * b04 - a31 * b02 + a33 * b00) * det;
  out[11] = (a21 * b02 - a20 * b04 - a23 * b00) * det;
  out[12] = (a11 * b07 - a10 * b09 - a12 * b06) * det;
  out[13] = (a00 * b09 - a01 * b07 + a02 * b06) * det;
  out[14] = (a31 * b01 - a30 * b03 - a32 * b00) * det;
  out[15] = (a20 * b03 - a21 * b01 + a22 * b00) * det;

  return true;
};

VRFrameData::VRFrameData() : m_timestamp(0.0) {
  m_leftProjectionMatrix = DOMFloat32Array::create(16);
  m_leftViewMatrix = DOMFloat32Array::create(16);
  m_rightProjectionMatrix = DOMFloat32Array::create(16);
  m_rightViewMatrix = DOMFloat32Array::create(16);
  m_pose = VRPose::create();
}

bool VRFrameData::update(const device::mojom::blink::VRPosePtr& pose,
                         VREyeParameters* leftEye,
                         VREyeParameters* rightEye,
                         float depthNear,
                         float depthFar) {
  if (!pose)
    return false;

  m_timestamp = pose->timestamp;

  // Build the projection matrices
  projectionFromFieldOfView(m_leftProjectionMatrix, leftEye->fieldOfView(),
                            depthNear, depthFar);
  projectionFromFieldOfView(m_rightProjectionMatrix, rightEye->fieldOfView(),
                            depthNear, depthFar);

  // Build the view matrices
  matrixfromRotationTranslation(m_leftViewMatrix, pose->orientation,
                                pose->position);
  matrixTranslate(m_leftViewMatrix, leftEye->offset());
  if (!matrixInvert(m_leftViewMatrix))
    return false;

  matrixfromRotationTranslation(m_rightViewMatrix, pose->orientation,
                                pose->position);
  matrixTranslate(m_rightViewMatrix, rightEye->offset());
  if (!matrixInvert(m_rightViewMatrix))
    return false;

  // Set the pose
  m_pose->setPose(pose);

  return true;
}

DEFINE_TRACE(VRFrameData) {
  visitor->trace(m_leftProjectionMatrix);
  visitor->trace(m_leftViewMatrix);
  visitor->trace(m_rightProjectionMatrix);
  visitor->trace(m_rightViewMatrix);
  visitor->trace(m_pose);
}

}  // namespace blink
