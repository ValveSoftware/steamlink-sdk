// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMMatrixReadOnly_h
#define DOMMatrixReadOnly_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTypedArray.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include <memory>

namespace blink {

class DOMMatrix;
class DOMMatrixInit;
class DOMPoint;
class DOMPointInit;

class CORE_EXPORT DOMMatrixReadOnly
    : public GarbageCollectedFinalized<DOMMatrixReadOnly>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMMatrixReadOnly* create(Vector<double>, ExceptionState&);
  static DOMMatrixReadOnly* fromFloat32Array(DOMFloat32Array*, ExceptionState&);
  static DOMMatrixReadOnly* fromFloat64Array(DOMFloat64Array*, ExceptionState&);
  static DOMMatrixReadOnly* fromMatrix(DOMMatrixInit&, ExceptionState&);
  virtual ~DOMMatrixReadOnly();

  double a() const { return m_matrix->m11(); }
  double b() const { return m_matrix->m12(); }
  double c() const { return m_matrix->m21(); }
  double d() const { return m_matrix->m22(); }
  double e() const { return m_matrix->m41(); }
  double f() const { return m_matrix->m42(); }

  double m11() const { return m_matrix->m11(); }
  double m12() const { return m_matrix->m12(); }
  double m13() const { return m_matrix->m13(); }
  double m14() const { return m_matrix->m14(); }
  double m21() const { return m_matrix->m21(); }
  double m22() const { return m_matrix->m22(); }
  double m23() const { return m_matrix->m23(); }
  double m24() const { return m_matrix->m24(); }
  double m31() const { return m_matrix->m31(); }
  double m32() const { return m_matrix->m32(); }
  double m33() const { return m_matrix->m33(); }
  double m34() const { return m_matrix->m34(); }
  double m41() const { return m_matrix->m41(); }
  double m42() const { return m_matrix->m42(); }
  double m43() const { return m_matrix->m43(); }
  double m44() const { return m_matrix->m44(); }

  bool is2D() const;
  bool isIdentity() const;

  DOMMatrix* multiply(DOMMatrixInit&, ExceptionState&);
  DOMMatrix* translate(double tx = 0, double ty = 0, double tz = 0);
  DOMMatrix* scale(double sx = 1);
  DOMMatrix* scale(double sx,
                   double sy,
                   double sz = 1,
                   double ox = 0,
                   double oy = 0,
                   double oz = 0);
  DOMMatrix* scale3d(double scale = 1,
                     double ox = 0,
                     double oy = 0,
                     double oz = 0);
  DOMMatrix* rotate(double rotX);
  DOMMatrix* rotate(double rotX, double rotY);
  DOMMatrix* rotate(double rotX, double rotY, double rotZ);
  DOMMatrix* rotateFromVector(double x, double y);
  DOMMatrix* rotateAxisAngle(double x = 0,
                             double y = 0,
                             double z = 0,
                             double angle = 0);
  DOMMatrix* skewX(double sx);
  DOMMatrix* skewY(double sy);
  DOMMatrix* flipX();
  DOMMatrix* flipY();
  DOMMatrix* inverse();

  DOMPoint* transformPoint(const DOMPointInit&);

  DOMFloat32Array* toFloat32Array() const;
  DOMFloat64Array* toFloat64Array() const;

  const String toString() const;

  ScriptValue toJSONForBinding(ScriptState*) const;

  const TransformationMatrix& matrix() const { return *m_matrix; }

  DEFINE_INLINE_TRACE() {}

 protected:
  DOMMatrixReadOnly() {}

  template <typename T>
  DOMMatrixReadOnly(T sequence, int size) {
    if (size == 6) {
      m_matrix =
          TransformationMatrix::create(sequence[0], sequence[1], sequence[2],
                                       sequence[3], sequence[4], sequence[5]);
      m_is2D = true;
    } else if (size == 16) {
      m_matrix = TransformationMatrix::create(
          sequence[0], sequence[1], sequence[2], sequence[3], sequence[4],
          sequence[5], sequence[6], sequence[7], sequence[8], sequence[9],
          sequence[10], sequence[11], sequence[12], sequence[13], sequence[14],
          sequence[15]);
      m_is2D = false;
    } else {
      NOTREACHED();
    }
  }

  static bool validateAndFixup(DOMMatrixInit&, ExceptionState&);
  // TransformationMatrix needs to be 16-byte aligned. PartitionAlloc
  // supports 16-byte alignment but Oilpan doesn't. So we use an std::unique_ptr
  // to allocate TransformationMatrix on PartitionAlloc.
  // TODO(oilpan): Oilpan should support 16-byte aligned allocations.
  std::unique_ptr<TransformationMatrix> m_matrix;
  bool m_is2D;
};

}  // namespace blink

#endif
