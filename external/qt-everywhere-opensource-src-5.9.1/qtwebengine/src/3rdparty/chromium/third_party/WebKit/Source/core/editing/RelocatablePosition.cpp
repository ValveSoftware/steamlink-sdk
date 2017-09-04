// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/RelocatablePosition.h"

namespace blink {

RelocatablePosition::RelocatablePosition(const Position& position)
    : m_range(position.isNotNull()
                  ? Range::create(*position.document(), position, position)
                  : nullptr) {}

RelocatablePosition::~RelocatablePosition() {
  if (!m_range)
    return;
  m_range->dispose();
}

Position RelocatablePosition::position() const {
  if (!m_range)
    return Position();
  DCHECK(m_range->collapsed());
  return m_range->startPosition();
}

}  // namespace blink
