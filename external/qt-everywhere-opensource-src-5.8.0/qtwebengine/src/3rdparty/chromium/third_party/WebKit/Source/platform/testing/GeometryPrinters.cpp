// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/GeometryPrinters.h"

#include "platform/geometry/FloatBox.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/LayoutRect.h"
#include <ostream> // NOLINT

namespace blink {

namespace {

class ScopedFloatFlags {
public:
    ScopedFloatFlags(std::ostream& out) : m_out(out), m_flags(out.flags())
    {
        out.unsetf(std::ios::floatfield);
        m_precision = out.precision(4);
    }

    ~ScopedFloatFlags()
    {
        m_out.flags(m_flags);
        m_out.precision(m_precision);
    }

private:
    std::ostream& m_out;
    std::ios::fmtflags m_flags;
    std::streamsize m_precision;
};

} // namespace

void PrintTo(const FloatBox& box, std::ostream* os)
{
    ScopedFloatFlags scope(*os);
    *os << "FloatBox("
        << box.x() << ", "
        << box.y() << ", "
        << box.z() << ", "
        << box.width() << ", "
        << box.height() << ", "
        << box.depth() << ")";
}

void PrintTo(const FloatPoint& point, std::ostream* os)
{
    ScopedFloatFlags scope(*os);
    *os << "FloatPoint("
        << point.x() << ", "
        << point.y() << ")";
}

void PrintTo(const FloatPoint3D& point, std::ostream* os)
{
    ScopedFloatFlags scope(*os);
    *os << "FloatPoint3D("
        << point.x() << ", "
        << point.y() << ", "
        << point.z() << ")";
}

void PrintTo(const FloatQuad& quad, std::ostream* os)
{
    ScopedFloatFlags scope(*os);
    *os << "FloatQuad("
        << "(" << quad.p1().x() << ", " << quad.p1().y() << "), "
        << "(" << quad.p2().x() << ", " << quad.p2().y() << "), "
        << "(" << quad.p3().x() << ", " << quad.p3().y() << "), "
        << "(" << quad.p4().x() << ", " << quad.p4().y() << "))";
}

void PrintTo(const FloatRect& rect, std::ostream* os)
{
    ScopedFloatFlags scope(*os);
    *os << "FloatRect("
        << rect.x() << ", "
        << rect.y() << ", "
        << rect.width() << ", "
        << rect.height() << ")";
}

void PrintTo(const FloatRoundedRect& roundedRect, std::ostream* os)
{
    *os << "FloatRoundedRect(";
    PrintTo(roundedRect.rect(), os);
    *os << ", ";
    PrintTo(roundedRect.getRadii(), os);
    *os << ")";
}

void PrintTo(const FloatRoundedRect::Radii& radii, std::ostream* os)
{
    *os << "FloatRoundedRect::Radii(";
    PrintTo(radii.topLeft(), os);
    *os << ", ";
    PrintTo(radii.topRight(), os);
    *os << ", ";
    PrintTo(radii.bottomLeft(), os);
    *os << ", ";
    PrintTo(radii.bottomRight(), os);
    *os << ")";
}

void PrintTo(const FloatSize& size, std::ostream* os)
{
    ScopedFloatFlags scope(*os);
    *os << "FloatSize("
        << size.width() << ", "
        << size.height() << ")";
}

void PrintTo(const IntRect& rect, std::ostream* os)
{
    *os << "IntRect("
        << rect.x() << ", "
        << rect.y() << ", "
        << rect.width() << ", "
        << rect.height() << ")";
}

void PrintTo(const LayoutRect& rect, std::ostream* os)
{
    ScopedFloatFlags scope(*os);
    *os << "LayoutRect("
        << rect.x().toFloat() << ", "
        << rect.y().toFloat() << ", "
        << rect.width().toFloat() << ", "
        << rect.height().toFloat() << ")";
}

} // namespace blink
