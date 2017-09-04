// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/ResizeViewportAnchor.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/VisualViewport.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

void ResizeViewportAnchor::resizeFrameView(IntSize size) {
  FrameView* frameView = rootFrameView();
  DCHECK(frameView);

  ScrollableArea* rootViewport = frameView->getScrollableArea();
  ScrollOffset offset = rootViewport->scrollOffset();

  frameView->resize(size);
  m_drift += rootViewport->scrollOffset() - offset;
}

void ResizeViewportAnchor::endScope() {
  if (--m_scopeCount > 0)
    return;

  FrameView* frameView = rootFrameView();
  if (!frameView)
    return;

  ScrollOffset visualViewportInDocument =
      frameView->getScrollableArea()->scrollOffset() - m_drift;

  // TODO(bokan): Don't use RootFrameViewport::setScrollPosition since it
  // assumes we can just set a sub-pixel precision offset on the FrameView.
  // While we "can" do this, the offset that will be shipped to CC will be the
  // truncated number and this class is used to handle TopControl movement
  // which needs the two threads to match exactly pixel-for-pixel. We can
  // replace this with RFV::setScrollPosition once Blink is sub-pixel scroll
  // offset aware. crbug.com/414283.
  DCHECK(frameView->getRootFrameViewport());
  frameView->getRootFrameViewport()->restoreToAnchor(visualViewportInDocument);

  m_drift = ScrollOffset();
}

FrameView* ResizeViewportAnchor::rootFrameView() {
  if (Frame* frame = m_page->mainFrame()) {
    if (frame->isLocalFrame())
      return toLocalFrame(frame)->view();
  }
  return nullptr;
}

}  // namespace blink
