// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/EffectPaintPropertyNode.h"

namespace blink {

EffectPaintPropertyNode* EffectPaintPropertyNode::root() {
  DEFINE_STATIC_REF(
      EffectPaintPropertyNode, root,
      (EffectPaintPropertyNode::create(
          nullptr, TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), CompositorFilterOperations(), 1.0)));
  return root;
}

cc::Layer* EffectPaintPropertyNode::ensureDummyLayer() const {
  if (m_dummyLayer)
    return m_dummyLayer.get();
  m_dummyLayer = cc::Layer::Create();
  return m_dummyLayer.get();
}

}  // namespace blink
