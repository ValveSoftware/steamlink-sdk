// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/GeometryPrinters.h"

#include "platform/geometry/DoublePoint.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/FloatBox.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/LayoutSize.h"
#include <ostream>  // NOLINT

namespace blink {

void PrintTo(const DoublePoint& point, std::ostream* os) {
  *os << point.toString();
}

void PrintTo(const DoubleRect& rect, std::ostream* os) {
  *os << rect.toString();
}

void PrintTo(const DoubleSize& size, std::ostream* os) {
  *os << size.toString();
}

void PrintTo(const FloatBox& box, std::ostream* os) {
  *os << box.toString();
}

void PrintTo(const FloatPoint& point, std::ostream* os) {
  *os << point.toString();
}

void PrintTo(const FloatPoint3D& point, std::ostream* os) {
  *os << point.toString();
}

void PrintTo(const FloatQuad& quad, std::ostream* os) {
  *os << quad.toString();
}

void PrintTo(const FloatRect& rect, std::ostream* os) {
  *os << rect.toString();
}

void PrintTo(const FloatRoundedRect& roundedRect, std::ostream* os) {
  *os << roundedRect.toString();
}

void PrintTo(const FloatRoundedRect::Radii& radii, std::ostream* os) {
  *os << radii.toString();
}

void PrintTo(const FloatSize& size, std::ostream* os) {
  *os << size.toString();
}

void PrintTo(const IntPoint& point, std::ostream* os) {
  *os << point.toString();
}

void PrintTo(const IntRect& rect, std::ostream* os) {
  *os << rect.toString();
}

void PrintTo(const IntSize& size, std::ostream* os) {
  *os << size.toString();
}

void PrintTo(const LayoutPoint& point, std::ostream* os) {
  *os << point.toString();
}

void PrintTo(const LayoutRect& rect, std::ostream* os) {
  *os << rect.toString();
}

void PrintTo(const LayoutSize& size, std::ostream* os) {
  *os << size.toString();
}

}  // namespace blink
