// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayerHitTestRects_h
#define LayerHitTestRects_h

#include "platform/geometry/LayoutRect.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

class PaintLayer;

typedef WTF::HashMap<const PaintLayer*, Vector<LayoutRect>> LayerHitTestRects;

}  // namespace blink

#endif  // LayerHitTestRects_h
