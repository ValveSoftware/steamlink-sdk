// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutPoint.h"

#include "wtf/text/WTFString.h"

namespace blink {

#ifndef NDEBUG
String LayoutPoint::toString() const
{
    return String::format("%f,%f", x().toDouble(), y().toDouble());
}
#endif

} // namespace blink
