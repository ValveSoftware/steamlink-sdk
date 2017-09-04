// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelocatablePosition_h
#define RelocatablePosition_h

#include "core/CoreExport.h"
#include "core/dom/Range.h"

namespace blink {

// |RelocatablePosition| is a helper class for keeping track of a |Position| in
// a document upon DOM tree changes even if the given |Position|'s original
// anchor node is moved out of document. The class is implemented by using a
// temporary |Range| object to keep track of the |Position|, and disposing the
// |Range| when out of scope.
class CORE_EXPORT RelocatablePosition final {
  STACK_ALLOCATED();

 public:
  explicit RelocatablePosition(const Position&);
  ~RelocatablePosition();

  Position position() const;

 private:
  const Member<Range> m_range;

  DISALLOW_COPY_AND_ASSIGN(RelocatablePosition);
};

}  // namespace blink

#endif  // RelocatablePosition_h
