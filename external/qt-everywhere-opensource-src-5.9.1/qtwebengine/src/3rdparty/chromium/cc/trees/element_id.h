// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_ELEMENT_ID_H_
#define CC_TREES_ELEMENT_ID_H_

#include <stddef.h>

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>

#include "base/hash.h"
#include "cc/base/cc_export.h"

namespace base {
class Value;
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

// An "element" is really an animation target. It retains the name element to be
// symmetric with ElementAnimations and blink::ElementAnimations, but is not
// in fact tied to the notion of a blink element. It is also not associated with
// the notion of a Layer. Ultimately, these ids will be used to look up the
// property tree node associated with the given animation.
//
// These ids are chosen by cc's clients to permit the destruction and
// restoration of cc entities (when visuals are hidden and shown) but maintain
// stable identifiers. There will be a single layer for an ElementId, but
// not every layer will have an id.
struct CC_EXPORT ElementId {
  ElementId(int primaryId, int secondaryId)
      : primaryId(primaryId), secondaryId(secondaryId) {}
  ElementId() : ElementId(0, 0) {}

  bool operator==(const ElementId& o) const;
  bool operator!=(const ElementId& o) const;
  bool operator<(const ElementId& o) const;

  // An ElementId's conversion to a boolean value depends only on its primaryId.
  explicit operator bool() const;

  void AddToTracedValue(base::trace_event::TracedValue* res) const;
  std::unique_ptr<base::Value> AsValue() const;

  // The compositor treats this as an opaque handle and should not know how to
  // interpret these bits. Non-blink cc clients typically operate in terms of
  // layers and may set this value to match the client's layer id.
  int primaryId;
  int secondaryId;
};

CC_EXPORT ElementId LayerIdToElementIdForTesting(int layer_id);

struct CC_EXPORT ElementIdHash {
  size_t operator()(ElementId key) const;
};

// Stream operator so ElementId can be used in assertion statements.
CC_EXPORT std::ostream& operator<<(std::ostream& out, const ElementId& id);

}  // namespace cc

#endif  // CC_TREES_ELEMENT_ID_H_
