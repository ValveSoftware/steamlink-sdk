// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedLayoutObject_h
#define TracedLayoutObject_h

#include "platform/TracedValue.h"
#include <memory>

namespace blink {

class LayoutView;

class TracedLayoutObject {
public:
    static std::unique_ptr<TracedValue> create(const LayoutView&, bool traceGeometry = true);
};

} // namespace blink

#endif // TracedLayoutObject_h
