// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutSize.h"

#include "wtf/text/WTFString.h"

namespace blink {

#ifndef NDEBUG
String LayoutSize::toString() const
{
    return String::format("%fx%f", width().toDouble(), height().toDouble());
}
#endif

} // namespace blink
