// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMMatrix_h
#define DOMMatrix_h

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/DOMMatrixInit.h"
#include "core/dom/DOMMatrixReadOnly.h"

namespace blink {

class CORE_EXPORT DOMMatrix : public DOMMatrixReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMMatrix* create(ExceptionState&);
  static DOMMatrix* create(DOMMatrixReadOnly*,
                           ExceptionState& = ASSERT_NO_EXCEPTION);
  static DOMMatrix* create(const SkMatrix44&, ExceptionState&);
  static DOMMatrix* create(Vector<double>, ExceptionState&);
  static DOMMatrix* fromFloat32Array(DOMFloat32Array*, ExceptionState&);
  static DOMMatrix* fromFloat64Array(DOMFloat64Array*, ExceptionState&);
  static DOMMatrix* fromMatrix(DOMMatrixInit&, ExceptionState&);

  void setA(double value) { m_matrix->setM11(value); }
  void setB(double value) { m_matrix->setM12(value); }
  void setC(double value) { m_matrix->setM21(value); }
  void setD(double value) { m_matrix->setM22(value); }
  void setE(double value) { m_matrix->setM41(value); }
  void setF(double value) { m_matrix->setM42(value); }

  void setM11(double value) { m_matrix->setM11(value); }
  void setM12(double value) { m_matrix->setM12(value); }
  void setM13(double value) {
    m_matrix->setM13(value);
    setIs2D(!value);
  }
  void setM14(double value) {
    m_matrix->setM14(value);
    setIs2D(!value);
  }
  void setM21(double value) { m_matrix->setM21(value); }
  void setM22(double value) { m_matrix->setM22(value); }
  void setM23(double value) {
    m_matrix->setM23(value);
    setIs2D(!value);
  }
  void setM24(double value) {
    m_matrix->setM24(value);
    setIs2D(!value);
  }
  void setM31(double value) {
    m_matrix->setM31(value);
    setIs2D(!value);
  }
  void setM32(double value) {
    m_matrix->setM32(value);
    setIs2D(!value);
  }
  void setM33(double value) {
    m_matrix->setM33(value);
    setIs2D(value != 1);
  }
  void setM34(double value) {
    m_matrix->setM34(value);
    setIs2D(!value);
  }
  void setM41(double value) { m_matrix->setM41(value); }
  void setM42(double value) { m_matrix->setM42(value); }
  void setM43(double value) {
    m_matrix->setM43(value);
    setIs2D(!value);
  }
  void setM44(double value) {
    m_matrix->setM44(value);
    setIs2D(value != 1);
  }

  DOMMatrix* multiplySelf(DOMMatrixInit&, ExceptionState&);
  DOMMatrix* preMultiplySelf(DOMMatrixInit&, ExceptionState&);
  DOMMatrix* translateSelf(double tx = 0, double ty = 0, double tz = 0);
  DOMMatrix* scaleSelf(double sx = 1);
  DOMMatrix* scaleSelf(double sx,
                       double sy,
                       double sz = 1,
                       double ox = 0,
                       double oy = 0,
                       double oz = 0);
  DOMMatrix* scale3dSelf(double scale = 1,
                         double ox = 0,
                         double oy = 0,
                         double oz = 0);
  DOMMatrix* rotateSelf(double rotX);
  DOMMatrix* rotateSelf(double rotX, double rotY);
  DOMMatrix* rotateSelf(double rotX, double rotY, double rotZ);
  DOMMatrix* rotateFromVectorSelf(double x, double y);
  DOMMatrix* rotateAxisAngleSelf(double x = 0,
                                 double y = 0,
                                 double z = 0,
                                 double angle = 0);
  DOMMatrix* skewXSelf(double sx = 0);
  DOMMatrix* skewYSelf(double sy = 0);
  DOMMatrix* invertSelf();

  DOMMatrix* setMatrixValue(const String&, ExceptionState&);

 private:
  DOMMatrix(const TransformationMatrix&, bool is2D = true);
  template <typename T>
  DOMMatrix(T sequence, int size);

  void setIs2D(bool value);
  void setNAN();
};

}  // namespace blink

#endif
