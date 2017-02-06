// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeState_h
#define PropertyTreeState_h

#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace blink {

class TransformPaintPropertyNode;
class ClipPaintPropertyNode;
class EffectPaintPropertyNode;

// Represents the combination of transform, clip and effect nodes for a particular coordinate space.
// See GeometryMapper.
struct PropertyTreeState {
    PropertyTreeState(
        TransformPaintPropertyNode* transform,
        ClipPaintPropertyNode* clip,
        EffectPaintPropertyNode* effect)
    : transform(transform), clip(clip), effect(effect) {}

    TransformPaintPropertyNode* transform;
    ClipPaintPropertyNode* clip;
    EffectPaintPropertyNode* effect;
};

} // namespace blink

#endif // PropertyTreeState_h
