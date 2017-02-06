// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/ResizeViewportAnchor.h"

#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

ResizeViewportAnchor::ResizeViewportAnchor(FrameView& rootFrameView, VisualViewport& visualViewport)
    : ViewportAnchor(rootFrameView, visualViewport)
    , m_visualViewportInDocument(rootFrameView.getScrollableArea()->scrollPositionDouble())
{
}

ResizeViewportAnchor::~ResizeViewportAnchor()
{
    // TODO(bokan): Don't use RootFrameViewport::setScrollPosition since it
    // assumes we can just set a sub-pixel precision offset on the FrameView.
    // While we "can" do this, the offset that will be shipped to CC will be the
    // truncated number and this class is used to handle TopControl movement
    // which needs the two threads to match exactly pixel-for-pixel. We can
    // replace this with RFV::setScrollPosition once Blink is sub-pixel scroll
    // offset aware. crbug.com/414283.

    ScrollableArea* rootViewport = m_rootFrameView->getScrollableArea();
    ScrollableArea* layoutViewport =
        m_rootFrameView->layoutViewportScrollableArea();

    // Clamp the scroll offset of each viewport now so that we force any invalid
    // offsets to become valid so we can compute the correct deltas.
    m_visualViewport->clampToBoundaries();
    layoutViewport->setScrollPosition(
        layoutViewport->scrollPositionDouble(), ProgrammaticScroll);

    DoubleSize delta = m_visualViewportInDocument
        - rootViewport->scrollPositionDouble();

    m_visualViewport->move(toFloatSize(delta));

    delta = m_visualViewportInDocument
        - rootViewport->scrollPositionDouble();

    // Since the main thread FrameView has integer scroll offsets, scroll it to
    // the next pixel and then we'll scroll the visual viewport again to
    // compensate for the sub-pixel offset. We need this "overscroll" to ensure
    // the pixel of which we want to be partially in appears fully inside the
    // FrameView since the VisualViewport is bounded by the FrameView.
    IntSize layoutDelta = IntSize(
        delta.width() < 0 ? floor(delta.width()) : ceil(delta.width()),
        delta.height() < 0 ? floor(delta.height()) : ceil(delta.height()));

    layoutViewport->setScrollPosition(
        layoutViewport->scrollPosition() + layoutDelta,
        ProgrammaticScroll);

    delta = m_visualViewportInDocument
        - rootViewport->scrollPositionDouble();
    m_visualViewport->move(toFloatSize(delta));
}

} // namespace blink
