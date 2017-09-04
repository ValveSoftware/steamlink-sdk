// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorTransformOperations.h"

#include "ui/gfx/transform.h"

namespace blink {

const cc::TransformOperations&
CompositorTransformOperations::asCcTransformOperations() const {
  return m_transformOperations;
}

cc::TransformOperations
CompositorTransformOperations::releaseCcTransformOperations() {
  return std::move(m_transformOperations);
}

bool CompositorTransformOperations::canBlendWith(
    const blink::CompositorTransformOperations& other) const {
  return m_transformOperations.CanBlendWith(other.m_transformOperations);
}

void CompositorTransformOperations::appendTranslate(double x,
                                                    double y,
                                                    double z) {
  m_transformOperations.AppendTranslate(x, y, z);
}

void CompositorTransformOperations::appendRotate(double x,
                                                 double y,
                                                 double z,
                                                 double degrees) {
  m_transformOperations.AppendRotate(x, y, z, degrees);
}

void CompositorTransformOperations::appendScale(double x, double y, double z) {
  m_transformOperations.AppendScale(x, y, z);
}

void CompositorTransformOperations::appendSkew(double x, double y) {
  m_transformOperations.AppendSkew(x, y);
}

void CompositorTransformOperations::appendPerspective(double depth) {
  m_transformOperations.AppendPerspective(depth);
}

void CompositorTransformOperations::appendMatrix(const SkMatrix44& matrix) {
  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = matrix;
  m_transformOperations.AppendMatrix(transform);
}

void CompositorTransformOperations::appendIdentity() {
  m_transformOperations.AppendIdentity();
}

bool CompositorTransformOperations::isIdentity() const {
  return m_transformOperations.IsIdentity();
}

}  // namespace blink
