// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ScrollPaintPropertyNode.h"

namespace blink {

ScrollPaintPropertyNode* ScrollPaintPropertyNode::root() {
  DEFINE_STATIC_REF(ScrollPaintPropertyNode, root,
                    (ScrollPaintPropertyNode::create(
                        nullptr, TransformPaintPropertyNode::root(), IntSize(),
                        IntSize(), false, false)));
  return root;
}

}  // namespace blink
