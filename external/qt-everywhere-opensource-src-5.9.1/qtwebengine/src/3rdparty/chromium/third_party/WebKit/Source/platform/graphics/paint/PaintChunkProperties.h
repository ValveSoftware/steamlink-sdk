// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunkProperties_h
#define PaintChunkProperties_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <iosfwd>

namespace blink {

// The set of paint properties applying to a |PaintChunk|. These properties are
// not local-only paint style parameters such as color, but instead represent
// the hierarchy of transforms, clips, effects, etc, that apply to a contiguous
// chunk of display items. A single DisplayItemClient can generate multiple
// properties of the same type and this struct represents the total state of all
// properties for a given |PaintChunk|.
//
// This differs from |ObjectPaintProperties| because it only stores one property
// for each type (e.g., either transform or perspective, but not both).
struct PaintChunkProperties {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  PaintChunkProperties() : backfaceHidden(false) {}

  // TODO(pdr): Refactor these to use PropertyTreeState.
  RefPtr<const TransformPaintPropertyNode> transform;
  RefPtr<const ClipPaintPropertyNode> clip;
  RefPtr<const EffectPaintPropertyNode> effect;
  RefPtr<const ScrollPaintPropertyNode> scroll;
  bool backfaceHidden;
};

// Equality is based only on the pointers and is not 'deep' which would require
// crawling the entire property tree to compute.
inline bool operator==(const PaintChunkProperties& a,
                       const PaintChunkProperties& b) {
  return a.transform.get() == b.transform.get() &&
         a.clip.get() == b.clip.get() && a.effect.get() == b.effect.get() &&
         a.scroll.get() == b.scroll.get() &&
         a.backfaceHidden == b.backfaceHidden;
}

inline bool operator!=(const PaintChunkProperties& a,
                       const PaintChunkProperties& b) {
  return !(a == b);
}

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const PaintChunkProperties&, std::ostream*);

}  // namespace blink

#endif  // PaintChunkProperties_h
