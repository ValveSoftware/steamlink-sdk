// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPaintInvalidator.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

PaintInvalidationReason HTMLCanvasPaintInvalidator::invalidatePaintIfNeeded() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(m_htmlCanvas, m_context).invalidatePaintIfNeeded();

  HTMLCanvasElement* element = toHTMLCanvasElement(m_htmlCanvas.node());
  if (element->isDirty()) {
    element->doDeferredPaintInvalidation();
    if (reason < PaintInvalidationRectangle)
      reason = PaintInvalidationRectangle;
  }

  return reason;
}

}  // namespace blink
