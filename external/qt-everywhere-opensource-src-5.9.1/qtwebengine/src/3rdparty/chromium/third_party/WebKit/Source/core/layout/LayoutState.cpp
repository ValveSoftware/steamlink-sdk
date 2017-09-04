/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutState.h"

#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"

namespace blink {

LayoutState::LayoutState(LayoutUnit pageLogicalHeight,
                         LayoutView& view)
    : m_isPaginated(pageLogicalHeight),
      m_containingBlockLogicalWidthChanged(false),
      m_paginationStateChanged(false),
      m_flowThread(nullptr),
      m_next(nullptr),
      m_pageLogicalHeight(pageLogicalHeight),
      m_layoutObject(view) {
  ASSERT(!view.layoutState());
  view.pushLayoutState(*this);
}

LayoutState::LayoutState(LayoutBox& layoutObject,
                         LayoutUnit pageLogicalHeight,
                         bool containingBlockLogicalWidthChanged)
    : m_containingBlockLogicalWidthChanged(containingBlockLogicalWidthChanged),
      m_next(layoutObject.view()->layoutState()),
      m_layoutObject(layoutObject) {
  if (layoutObject.isLayoutFlowThread())
    m_flowThread = toLayoutFlowThread(&layoutObject);
  else if (!layoutObject.isOutOfFlowPositioned())
    m_flowThread = m_next->flowThread();
  else
    m_flowThread = nullptr;
  m_paginationStateChanged = m_next->m_paginationStateChanged;
  layoutObject.view()->pushLayoutState(*this);
  m_heightOffsetForTableHeaders = m_next->heightOffsetForTableHeaders();

  if (pageLogicalHeight || layoutObject.isLayoutFlowThread()) {
    // Entering a new pagination context.
    m_pageLogicalHeight = pageLogicalHeight;
    m_paginationOffset = LayoutSize();
    m_isPaginated = true;
    return;
  }

  // Disable pagination for objects we don't support. For now this includes
  // overflow:scroll/auto, inline blocks and writing mode roots. Additionally,
  // pagination inside SVG is not allowed.
  if (layoutObject.getPaginationBreakability() == LayoutBox::ForbidBreaks ||
      (m_layoutObject.isSVG() && !m_layoutObject.isSVGRoot())) {
    m_flowThread = nullptr;
    m_pageLogicalHeight = LayoutUnit();
    m_isPaginated = false;
    return;
  }

  // Propagate the old page height and offset down.
  m_pageLogicalHeight = m_next->m_pageLogicalHeight;

  m_isPaginated = m_pageLogicalHeight || m_flowThread;
  if (!m_isPaginated)
    return;

  // Now adjust the pagination offset, so that we can easily figure out how far
  // away we are from the start of the pagination context.
  m_paginationOffset = m_next->m_paginationOffset;
  bool fixed = layoutObject.isOutOfFlowPositioned() &&
               layoutObject.style()->position() == FixedPosition;
  if (fixed)
    return;
  m_paginationOffset =
      m_next->m_paginationOffset + layoutObject.locationOffset();
  if (!layoutObject.isOutOfFlowPositioned())
    return;
  if (LayoutObject* container = layoutObject.container()) {
    if (container->style()->hasInFlowPosition() &&
        container->isLayoutInline()) {
      m_paginationOffset +=
          toLayoutInline(container)->offsetForInFlowPositionedInline(
              layoutObject);
    }
  }

  // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if
  // present.
}

LayoutState::LayoutState(LayoutObject& root)
    : m_isPaginated(false),
      m_containingBlockLogicalWidthChanged(false),
      m_paginationStateChanged(false),
      m_flowThread(nullptr),
      m_next(root.view()->layoutState()),
      m_layoutObject(root) {
  ASSERT(!m_next);
  // We'll end up pushing in LayoutView itself, so don't bother adding it.
  if (root.isLayoutView())
    return;

  root.view()->pushLayoutState(*this);
}

LayoutState::~LayoutState() {
  if (m_layoutObject.view()->layoutState()) {
    ASSERT(m_layoutObject.view()->layoutState() == this);
    m_layoutObject.view()->popLayoutState();
  }
}

LayoutUnit LayoutState::pageLogicalOffset(
    const LayoutBox& child,
    const LayoutUnit& childLogicalOffset) const {
  if (child.isHorizontalWritingMode())
    return m_paginationOffset.height() + childLogicalOffset;
  return m_paginationOffset.width() + childLogicalOffset;
}

}  // namespace blink
