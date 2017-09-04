// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMMatrixReadOnly.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/DOMMatrix.h"
#include "core/dom/DOMMatrixInit.h"
#include "core/dom/DOMPoint.h"
#include "core/dom/DOMPointInit.h"

namespace blink {
namespace {

void setDictionaryMembers(DOMMatrixInit& other) {
  if (!other.hasM11())
    other.setM11(other.hasA() ? other.a() : 1);

  if (!other.hasM12())
    other.setM12(other.hasB() ? other.b() : 0);

  if (!other.hasM21())
    other.setM21(other.hasC() ? other.c() : 0);

  if (!other.hasM22())
    other.setM22(other.hasD() ? other.d() : 1);

  if (!other.hasM41())
    other.setM41(other.hasE() ? other.e() : 0);

  if (!other.hasM42())
    other.setM42(other.hasF() ? other.f() : 0);
}

String getErrorMessage(const char* a, const char* b) {
  return String::format("The '%s' property should eqaul the '%s' property.", a,
                        b);
}

}  // namespace

bool DOMMatrixReadOnly::validateAndFixup(DOMMatrixInit& other,
                                         ExceptionState& exceptionState) {
  if (other.hasA() && other.hasM11() && other.a() != other.m11()) {
    exceptionState.throwTypeError(getErrorMessage("a", "m11"));
    return false;
  }
  if (other.hasB() && other.hasM12() && other.b() != other.m12()) {
    exceptionState.throwTypeError(getErrorMessage("b", "m12"));
    return false;
  }
  if (other.hasC() && other.hasM21() && other.c() != other.m21()) {
    exceptionState.throwTypeError(getErrorMessage("c", "m21"));
    return false;
  }
  if (other.hasD() && other.hasM22() && other.d() != other.m22()) {
    exceptionState.throwTypeError(getErrorMessage("d", "m22"));
    return false;
  }
  if (other.hasE() && other.hasM41() && other.e() != other.m41()) {
    exceptionState.throwTypeError(getErrorMessage("e", "m41"));
    return false;
  }
  if (other.hasF() && other.hasM42() && other.f() != other.m42()) {
    exceptionState.throwTypeError(getErrorMessage("f", "m42"));
    return false;
  }
  if (other.hasIs2D() && other.is2D() &&
      (other.m31() || other.m32() || other.m13() || other.m23() ||
       other.m43() || other.m14() || other.m24() || other.m34() ||
       other.m33() != 1 || other.m44() != 1)) {
    exceptionState.throwTypeError(
        "The is2D member is set to true but the input matrix is 3d matrix.");
    return false;
  }

  setDictionaryMembers(other);
  if (!other.hasIs2D()) {
    bool is2D = !(other.m31() || other.m32() || other.m13() || other.m23() ||
                  other.m43() || other.m14() || other.m24() || other.m34() ||
                  other.m33() != 1 || other.m44() != 1);
    other.setIs2D(is2D);
  }
  return true;
}

DOMMatrixReadOnly* DOMMatrixReadOnly::create(Vector<double> sequence,
                                             ExceptionState& exceptionState) {
  if (sequence.size() != 6 && sequence.size() != 16) {
    exceptionState.throwTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrixReadOnly(sequence, sequence.size());
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromFloat32Array(
    DOMFloat32Array* float32Array,
    ExceptionState& exceptionState) {
  if (float32Array->length() != 6 && float32Array->length() != 16) {
    exceptionState.throwTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements a "
        "for 3D matrix.");
    return nullptr;
  }
  return new DOMMatrixReadOnly(float32Array->data(), float32Array->length());
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromFloat64Array(
    DOMFloat64Array* float64Array,
    ExceptionState& exceptionState) {
  if (float64Array->length() != 6 && float64Array->length() != 16) {
    exceptionState.throwTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrixReadOnly(float64Array->data(), float64Array->length());
}

DOMMatrixReadOnly* DOMMatrixReadOnly::fromMatrix(
    DOMMatrixInit& other,
    ExceptionState& exceptionState) {
  if (!validateAndFixup(other, exceptionState))
    return nullptr;

  if (other.is2D()) {
    double args[] = {other.m11(), other.m12(), other.m21(),
                     other.m22(), other.m41(), other.m42()};
    return new DOMMatrixReadOnly(args, 6);
  }

  double args[] = {other.m11(), other.m12(), other.m13(), other.m14(),
                   other.m21(), other.m22(), other.m23(), other.m24(),
                   other.m31(), other.m32(), other.m33(), other.m34(),
                   other.m41(), other.m42(), other.m43(), other.m44()};
  return new DOMMatrixReadOnly(args, 16);
}

DOMMatrixReadOnly::~DOMMatrixReadOnly() {}

bool DOMMatrixReadOnly::is2D() const {
  return m_is2D;
}

bool DOMMatrixReadOnly::isIdentity() const {
  return m_matrix->isIdentity();
}

DOMMatrix* DOMMatrixReadOnly::multiply(DOMMatrixInit& other,
                                       ExceptionState& exceptionState) {
  return DOMMatrix::create(this)->multiplySelf(other, exceptionState);
}

DOMMatrix* DOMMatrixReadOnly::translate(double tx, double ty, double tz) {
  return DOMMatrix::create(this)->translateSelf(tx, ty, tz);
}

DOMMatrix* DOMMatrixReadOnly::scale(double sx) {
  return scale(sx, sx);
}

DOMMatrix* DOMMatrixReadOnly::scale(double sx,
                                    double sy,
                                    double sz,
                                    double ox,
                                    double oy,
                                    double oz) {
  return DOMMatrix::create(this)->scaleSelf(sx, sy, sz, ox, oy, oz);
}

DOMMatrix* DOMMatrixReadOnly::scale3d(double scale,
                                      double ox,
                                      double oy,
                                      double oz) {
  return DOMMatrix::create(this)->scale3dSelf(scale, ox, oy, oz);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rotX) {
  return DOMMatrix::create(this)->rotateSelf(rotX);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rotX, double rotY) {
  return DOMMatrix::create(this)->rotateSelf(rotX, rotY);
}

DOMMatrix* DOMMatrixReadOnly::rotate(double rotX, double rotY, double rotZ) {
  return DOMMatrix::create(this)->rotateSelf(rotX, rotY, rotZ);
}

DOMMatrix* DOMMatrixReadOnly::rotateFromVector(double x, double y) {
  return DOMMatrix::create(this)->rotateFromVectorSelf(x, y);
}

DOMMatrix* DOMMatrixReadOnly::rotateAxisAngle(double x,
                                              double y,
                                              double z,
                                              double angle) {
  return DOMMatrix::create(this)->rotateAxisAngleSelf(x, y, z, angle);
}

DOMMatrix* DOMMatrixReadOnly::skewX(double sx) {
  return DOMMatrix::create(this)->skewXSelf(sx);
}

DOMMatrix* DOMMatrixReadOnly::skewY(double sy) {
  return DOMMatrix::create(this)->skewYSelf(sy);
}

DOMMatrix* DOMMatrixReadOnly::flipX() {
  DOMMatrix* flipX = DOMMatrix::create(this);
  flipX->setM11(-this->m11());
  flipX->setM12(-this->m12());
  flipX->setM13(-this->m13());
  flipX->setM14(-this->m14());
  return flipX;
}

DOMMatrix* DOMMatrixReadOnly::flipY() {
  DOMMatrix* flipY = DOMMatrix::create(this);
  flipY->setM21(-this->m21());
  flipY->setM22(-this->m22());
  flipY->setM23(-this->m23());
  flipY->setM24(-this->m24());
  return flipY;
}

DOMMatrix* DOMMatrixReadOnly::inverse() {
  return DOMMatrix::create(this)->invertSelf();
}

DOMPoint* DOMMatrixReadOnly::transformPoint(const DOMPointInit& point) {
  if (is2D() && point.z() == 0 && point.w() == 1) {
    double x = point.x() * m11() + point.y() * m12() + m41();
    double y = point.x() * m12() + point.y() * m22() + m42();
    return DOMPoint::create(x, y, 0, 1);
  }

  double x = point.x() * m11() + point.y() * m21() + point.z() * m31() +
             point.w() * m41();
  double y = point.x() * m12() + point.y() * m22() + point.z() * m32() +
             point.w() * m42();
  double z = point.x() * m13() + point.y() * m23() + point.z() * m33() +
             point.w() * m43();
  double w = point.x() * m14() + point.y() * m24() + point.z() * m34() +
             point.w() * m44();
  return DOMPoint::create(x, y, z, w);
}

DOMFloat32Array* DOMMatrixReadOnly::toFloat32Array() const {
  float array[] = {
      static_cast<float>(m_matrix->m11()), static_cast<float>(m_matrix->m12()),
      static_cast<float>(m_matrix->m13()), static_cast<float>(m_matrix->m14()),
      static_cast<float>(m_matrix->m21()), static_cast<float>(m_matrix->m22()),
      static_cast<float>(m_matrix->m23()), static_cast<float>(m_matrix->m24()),
      static_cast<float>(m_matrix->m31()), static_cast<float>(m_matrix->m32()),
      static_cast<float>(m_matrix->m33()), static_cast<float>(m_matrix->m34()),
      static_cast<float>(m_matrix->m41()), static_cast<float>(m_matrix->m42()),
      static_cast<float>(m_matrix->m43()), static_cast<float>(m_matrix->m44())};

  return DOMFloat32Array::create(array, 16);
}

DOMFloat64Array* DOMMatrixReadOnly::toFloat64Array() const {
  double array[] = {
      m_matrix->m11(), m_matrix->m12(), m_matrix->m13(), m_matrix->m14(),
      m_matrix->m21(), m_matrix->m22(), m_matrix->m23(), m_matrix->m24(),
      m_matrix->m31(), m_matrix->m32(), m_matrix->m33(), m_matrix->m34(),
      m_matrix->m41(), m_matrix->m42(), m_matrix->m43(), m_matrix->m44()};

  return DOMFloat64Array::create(array, 16);
}

const String DOMMatrixReadOnly::toString() const {
  std::stringstream stream;
  if (is2D()) {
    stream << "matrix(" << a() << ", " << b() << ", " << c() << ", " << d()
           << ", " << e() << ", " << f();
  } else {
    stream << "matrix3d(" << m11() << ", " << m12() << ", " << m13() << ", "
           << m14() << ", " << m21() << ", " << m22() << ", " << m23() << ", "
           << m24() << ", " << m31() << ", " << m32() << ", " << m33() << ", "
           << m34() << ", " << m41() << ", " << m42() << ", " << m43() << ", "
           << m44();
  }
  stream << ")";

  return String(stream.str().c_str());
}

ScriptValue DOMMatrixReadOnly::toJSONForBinding(
    ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  result.addNumber("a", a());
  result.addNumber("b", b());
  result.addNumber("c", c());
  result.addNumber("d", d());
  result.addNumber("e", e());
  result.addNumber("f", f());
  result.addNumber("m11", m11());
  result.addNumber("m12", m12());
  result.addNumber("m13", m13());
  result.addNumber("m14", m14());
  result.addNumber("m21", m21());
  result.addNumber("m22", m22());
  result.addNumber("m23", m23());
  result.addNumber("m24", m24());
  result.addNumber("m31", m31());
  result.addNumber("m32", m32());
  result.addNumber("m33", m33());
  result.addNumber("m34", m34());
  result.addNumber("m41", m41());
  result.addNumber("m42", m42());
  result.addNumber("m43", m43());
  result.addNumber("m44", m44());
  result.addBoolean("is2D", is2D());
  result.addBoolean("isIdentity", isIdentity());
  return result.scriptValue();
}

}  // namespace blink
