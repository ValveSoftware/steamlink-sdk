// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMMatrix.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/style/ComputedStyle.h"

namespace blink {

DOMMatrix* DOMMatrix::create(ExceptionState& exceptionState) {
  return new DOMMatrix(TransformationMatrix());
}

DOMMatrix* DOMMatrix::create(DOMMatrixReadOnly* other,
                             ExceptionState& exceptionState) {
  return new DOMMatrix(other->matrix(), other->is2D());
}

DOMMatrix* DOMMatrix::create(const SkMatrix44& matrix,
                             ExceptionState& exceptionState) {
  TransformationMatrix transformationMatrix(matrix);
  return new DOMMatrix(transformationMatrix, transformationMatrix.isAffine());
}

DOMMatrix* DOMMatrix::create(Vector<double> sequence,
                             ExceptionState& exceptionState) {
  if (sequence.size() != 6 && sequence.size() != 16) {
    exceptionState.throwTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrix(sequence, sequence.size());
}

DOMMatrix* DOMMatrix::fromFloat32Array(DOMFloat32Array* float32Array,
                                       ExceptionState& exceptionState) {
  if (float32Array->length() != 6 && float32Array->length() != 16) {
    exceptionState.throwTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrix(float32Array->data(), float32Array->length());
}

DOMMatrix* DOMMatrix::fromFloat64Array(DOMFloat64Array* float64Array,
                                       ExceptionState& exceptionState) {
  if (float64Array->length() != 6 && float64Array->length() != 16) {
    exceptionState.throwTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrix(float64Array->data(), float64Array->length());
}

template <typename T>
DOMMatrix::DOMMatrix(T sequence, int size)
    : DOMMatrixReadOnly(sequence, size) {}

DOMMatrix::DOMMatrix(const TransformationMatrix& matrix, bool is2D) {
  m_matrix = TransformationMatrix::create(matrix);
  m_is2D = is2D;
}

DOMMatrix* DOMMatrix::fromMatrix(DOMMatrixInit& other,
                                 ExceptionState& exceptionState) {
  validateAndFixup(other, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  if (other.is2D()) {
    return new DOMMatrix({other.m11(), other.m12(), other.m21(), other.m22(),
                          other.m41(), other.m42()},
                         other.is2D());
  }

  return new DOMMatrix({other.m11(), other.m12(), other.m13(), other.m14(),
                        other.m21(), other.m22(), other.m23(), other.m24(),
                        other.m31(), other.m32(), other.m33(), other.m34(),
                        other.m41(), other.m42(), other.m43(), other.m44()},
                       other.is2D());
}

void DOMMatrix::setIs2D(bool value) {
  if (m_is2D)
    m_is2D = value;
}

void DOMMatrix::setNAN() {
  m_matrix->setM11(NAN);
  m_matrix->setM12(NAN);
  m_matrix->setM13(NAN);
  m_matrix->setM14(NAN);
  m_matrix->setM21(NAN);
  m_matrix->setM22(NAN);
  m_matrix->setM23(NAN);
  m_matrix->setM24(NAN);
  m_matrix->setM31(NAN);
  m_matrix->setM32(NAN);
  m_matrix->setM33(NAN);
  m_matrix->setM34(NAN);
  m_matrix->setM41(NAN);
  m_matrix->setM42(NAN);
  m_matrix->setM43(NAN);
  m_matrix->setM44(NAN);
}

DOMMatrix* DOMMatrix::multiplySelf(DOMMatrixInit& other,
                                   ExceptionState& exceptionState) {
  DOMMatrix* otherMatrix = DOMMatrix::fromMatrix(other, exceptionState);
  if (!otherMatrix->is2D())
    m_is2D = false;

  *m_matrix *= otherMatrix->matrix();

  return this;
}

DOMMatrix* DOMMatrix::preMultiplySelf(DOMMatrixInit& other,
                                      ExceptionState& exceptionState) {
  DOMMatrix* otherMatrix = DOMMatrix::fromMatrix(other, exceptionState);
  if (!otherMatrix->is2D())
    m_is2D = false;

  TransformationMatrix& matrix = *m_matrix;
  *m_matrix = otherMatrix->matrix() * matrix;

  return this;
}

DOMMatrix* DOMMatrix::translateSelf(double tx, double ty, double tz) {
  if (!tx && !ty && !tz)
    return this;

  if (tz)
    m_is2D = false;

  if (m_is2D)
    m_matrix->translate(tx, ty);
  else
    m_matrix->translate3d(tx, ty, tz);

  return this;
}

DOMMatrix* DOMMatrix::scaleSelf(double sx) {
  return scaleSelf(sx, sx);
}

DOMMatrix* DOMMatrix::scaleSelf(double sx,
                                double sy,
                                double sz,
                                double ox,
                                double oy,
                                double oz) {
  if (sz != 1 || oz)
    m_is2D = false;

  if (sx == 1 && sy == 1 && sz == 1)
    return this;

  bool hasTranslation = (ox || oy || oz);

  if (hasTranslation)
    translateSelf(ox, oy, oz);

  if (m_is2D)
    m_matrix->scaleNonUniform(sx, sy);
  else
    m_matrix->scale3d(sx, sy, sz);

  if (hasTranslation)
    translateSelf(-ox, -oy, -oz);

  return this;
}

DOMMatrix* DOMMatrix::scale3dSelf(double scale,
                                  double ox,
                                  double oy,
                                  double oz) {
  return scaleSelf(scale, scale, scale, ox, oy, oz);
}

DOMMatrix* DOMMatrix::rotateSelf(double rotX) {
  return rotateSelf(0, 0, rotX);
}

DOMMatrix* DOMMatrix::rotateSelf(double rotX, double rotY) {
  return rotateSelf(rotX, rotY, 0);
}

DOMMatrix* DOMMatrix::rotateSelf(double rotX, double rotY, double rotZ) {
  if (rotZ)
    m_matrix->rotate3d(0, 0, 1, rotZ);

  if (rotY) {
    m_matrix->rotate3d(0, 1, 0, rotY);
    m_is2D = false;
  }

  if (rotX) {
    m_matrix->rotate3d(1, 0, 0, rotX);
    m_is2D = false;
  }

  return this;
}

DOMMatrix* DOMMatrix::rotateFromVectorSelf(double x, double y) {
  m_matrix->rotate(rad2deg(atan2(y, x)));
  return this;
}

DOMMatrix* DOMMatrix::rotateAxisAngleSelf(double x,
                                          double y,
                                          double z,
                                          double angle) {
  m_matrix->rotate3d(x, y, z, angle);

  if (x != 0 || y != 0)
    m_is2D = false;

  return this;
}

DOMMatrix* DOMMatrix::skewXSelf(double sx) {
  m_matrix->skewX(sx);
  return this;
}

DOMMatrix* DOMMatrix::skewYSelf(double sy) {
  m_matrix->skewY(sy);
  return this;
}

DOMMatrix* DOMMatrix::invertSelf() {
  if (m_matrix->isInvertible()) {
    m_matrix = TransformationMatrix::create(m_matrix->inverse());
  } else {
    setNAN();
    setIs2D(false);
  }
  return this;
}

DOMMatrix* DOMMatrix::setMatrixValue(const String& inputString,
                                     ExceptionState& exceptionState) {
  DEFINE_STATIC_LOCAL(String, identityMatrix2D, ("matrix(1, 0, 0, 1, 0, 0)"));
  String string = inputString;
  if (string.isEmpty())
    string = identityMatrix2D;

  const CSSValue* value =
      CSSParser::parseSingleValue(CSSPropertyTransform, string);

  if (!value || value->isCSSWideKeyword()) {
    exceptionState.throwDOMException(SyntaxError,
                                     "Failed to parse '" + inputString + "'.");
    return nullptr;
  }

  if (value->isIdentifierValue()) {
    DCHECK(toCSSIdentifierValue(value)->getValueID() == CSSValueNone);
    m_matrix->makeIdentity();
    m_is2D = true;
    return this;
  }

  if (TransformBuilder::hasRelativeLengths(toCSSValueList(*value))) {
    exceptionState.throwDOMException(SyntaxError,
                                     "Relative lengths not supported.");
    return nullptr;
  }

  const ComputedStyle& initialStyle = ComputedStyle::initialStyle();
  TransformOperations operations = TransformBuilder::createTransformOperations(
      *value, CSSToLengthConversionData(&initialStyle, &initialStyle,
                                        LayoutViewItem(nullptr), 1.0f));

  if (operations.dependsOnBoxSize()) {
    exceptionState.throwDOMException(SyntaxError,
                                     "The transformation depends on the box "
                                     "size, which is not supported.");
    return nullptr;
  }

  m_matrix->makeIdentity();
  operations.apply(FloatSize(0, 0), *m_matrix);

  m_is2D = !operations.has3DOperation();

  return this;
}

}  // namespace blink
