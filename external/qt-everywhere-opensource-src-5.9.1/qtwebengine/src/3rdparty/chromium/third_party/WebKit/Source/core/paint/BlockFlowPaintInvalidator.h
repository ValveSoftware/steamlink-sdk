// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockFlowPaintInvalidator_h
#define BlockFlowPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutBlockFlow;

class BlockFlowPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BlockFlowPaintInvalidator(const LayoutBlockFlow& blockFlow)
      : m_blockFlow(blockFlow) {}

  void invalidatePaintForOverhangingFloats() {
    invalidatePaintForOverhangingFloatsInternal(InvalidateDescendants);
  }

  void invalidateDisplayItemClients(PaintInvalidationReason);

 private:
  enum InvalidateDescendantMode {
    DontInvalidateDescendants,
    InvalidateDescendants
  };
  void invalidatePaintForOverhangingFloatsInternal(InvalidateDescendantMode);

  const LayoutBlockFlow& m_blockFlow;
};

}  // namespace blink

#endif
