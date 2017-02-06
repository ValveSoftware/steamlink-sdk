// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/LayoutSize.h"

#include "wtf/text/WTFString.h"

#include <limits>
#include <math.h>

namespace blink {

DoubleSize::DoubleSize(const LayoutSize& size)
    : m_width(size.width().toDouble())
    , m_height(size.height().toDouble())
{
}

bool DoubleSize::isZero() const
{
    return fabs(m_width) < std::numeric_limits<double>::epsilon() && fabs(m_height) < std::numeric_limits<double>::epsilon();
}

#ifndef NDEBUG
String DoubleSize::toString() const
{
    return String::format("%fx%f", width(), height());
}
#endif

} // namespace blink
