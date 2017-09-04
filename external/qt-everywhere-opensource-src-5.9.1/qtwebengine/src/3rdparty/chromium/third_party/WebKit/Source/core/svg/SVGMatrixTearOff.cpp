/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/SVGMatrixTearOff.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGTransformTearOff.h"

namespace blink {

SVGMatrixTearOff::SVGMatrixTearOff(const AffineTransform& staticValue)
    : m_staticValue(staticValue), m_contextTransform(this, nullptr) {}

SVGMatrixTearOff::SVGMatrixTearOff(SVGTransformTearOff* transform)
    : m_contextTransform(this, transform) {
  ASSERT(transform);
}

DEFINE_TRACE(SVGMatrixTearOff) {
  visitor->trace(m_contextTransform);
}

DEFINE_TRACE_WRAPPERS(SVGMatrixTearOff) {
  visitor->traceWrappers(m_contextTransform);
}

const AffineTransform& SVGMatrixTearOff::value() const {
  return m_contextTransform ? m_contextTransform->target()->matrix()
                            : m_staticValue;
}

AffineTransform* SVGMatrixTearOff::mutableValue() {
  return m_contextTransform ? m_contextTransform->target()->mutableMatrix()
                            : &m_staticValue;
}

void SVGMatrixTearOff::commitChange() {
  if (!m_contextTransform)
    return;

  m_contextTransform->target()->onMatrixChange();
  m_contextTransform->commitChange();
}

#define DEFINE_SETTER(ATTRIBUTE)                                          \
  void SVGMatrixTearOff::set##ATTRIBUTE(double f,                         \
                                        ExceptionState& exceptionState) { \
    if (m_contextTransform && m_contextTransform->isImmutable()) {        \
      SVGPropertyTearOffBase::throwReadOnly(exceptionState);              \
      return;                                                             \
    }                                                                     \
    mutableValue()->set##ATTRIBUTE(f);                                    \
    commitChange();                                                       \
  }

DEFINE_SETTER(A);
DEFINE_SETTER(B);
DEFINE_SETTER(C);
DEFINE_SETTER(D);
DEFINE_SETTER(E);
DEFINE_SETTER(F);

#undef DEFINE_SETTER

SVGMatrixTearOff* SVGMatrixTearOff::translate(double tx, double ty) {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->translate(tx, ty);
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::scale(double s) {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->scale(s, s);
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::scaleNonUniform(double sx, double sy) {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->scale(sx, sy);
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::rotate(double d) {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->rotate(d);
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::flipX() {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->flipX();
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::flipY() {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->flipY();
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::skewX(double angle) {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->skewX(angle);
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::skewY(double angle) {
  SVGMatrixTearOff* matrix = create(value());
  matrix->mutableValue()->skewY(angle);
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::multiply(SVGMatrixTearOff* other) {
  SVGMatrixTearOff* matrix = create(value());
  *matrix->mutableValue() *= other->value();
  return matrix;
}

SVGMatrixTearOff* SVGMatrixTearOff::inverse(ExceptionState& exceptionState) {
  if (!value().isInvertible()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The matrix is not invertible.");
    return nullptr;
  }
  return create(value().inverse());
}

SVGMatrixTearOff* SVGMatrixTearOff::rotateFromVector(
    double x,
    double y,
    ExceptionState& exceptionState) {
  if (!x || !y) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "Arguments cannot be zero.");
    return nullptr;
  }
  AffineTransform copy = value();
  copy.rotateFromVector(x, y);
  return create(copy);
}

}  // namespace blink
