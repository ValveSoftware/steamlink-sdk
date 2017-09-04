// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TablePaintInvalidator_h
#define TablePaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutTable;
struct PaintInvalidatorContext;

class TablePaintInvalidator {
  STACK_ALLOCATED();

 public:
  TablePaintInvalidator(const LayoutTable& table,
                        const PaintInvalidatorContext& context)
      : m_table(table), m_context(context) {}

  PaintInvalidationReason invalidatePaintIfNeeded();

 private:
  const LayoutTable& m_table;
  const PaintInvalidatorContext& m_context;
};

}  // namespace blink

#endif
