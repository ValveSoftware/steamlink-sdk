// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockPaintInvalidator.h"

#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutBlock.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

PaintInvalidationReason BlockPaintInvalidator::invalidatePaintIfNeeded() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(m_block, m_context).invalidatePaintIfNeeded();

  if (reason != PaintInvalidationNone && m_block.hasCaret()) {
    FrameSelection& selection = m_block.frame()->selection();
    selection.setCaretRectNeedsUpdate();
    selection.invalidateCaretRect(true);
  }
  return reason;
}
}
