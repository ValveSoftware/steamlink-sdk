// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasPaintInvalidator_h
#define HTMLCanvasPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutHTMLCanvas;
struct PaintInvalidatorContext;

class HTMLCanvasPaintInvalidator {
  STACK_ALLOCATED();

 public:
  HTMLCanvasPaintInvalidator(const LayoutHTMLCanvas& htmlCanvas,
                             const PaintInvalidatorContext& context)
      : m_htmlCanvas(htmlCanvas), m_context(context) {}

  PaintInvalidationReason invalidatePaintIfNeeded();

 private:
  const LayoutHTMLCanvas& m_htmlCanvas;
  const PaintInvalidatorContext& m_context;
};

}  // namespace blink

#endif
