// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockPaintInvalidator_h
#define BlockPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

struct PaintInvalidatorContext;
class LayoutBlock;

class BlockPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BlockPaintInvalidator(const LayoutBlock& block,
                        const PaintInvalidatorContext& context)
      : m_block(block), m_context(context) {}

  PaintInvalidationReason invalidatePaintIfNeeded();

 private:
  const LayoutBlock& m_block;
  const PaintInvalidatorContext& m_context;
};

}  // namespace blink

#endif
