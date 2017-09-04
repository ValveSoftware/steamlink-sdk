// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipPaintPropertyNode.h"

#include "platform/geometry/LayoutRect.h"

namespace blink {

ClipPaintPropertyNode* ClipPaintPropertyNode::root() {
  DEFINE_STATIC_REF(ClipPaintPropertyNode, root,
                    (ClipPaintPropertyNode::create(
                        nullptr, TransformPaintPropertyNode::root(),
                        FloatRoundedRect(LayoutRect::infiniteIntRect()))));
  return root;
}

}  // namespace blink
