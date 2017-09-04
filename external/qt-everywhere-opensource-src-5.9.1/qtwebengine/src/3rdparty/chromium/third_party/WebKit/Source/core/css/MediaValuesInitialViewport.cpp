// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesInitialViewport.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/api/LayoutViewItem.h"

namespace blink {

MediaValuesInitialViewport* MediaValuesInitialViewport::create(
    LocalFrame& frame) {
  return new MediaValuesInitialViewport(frame);
}

MediaValuesInitialViewport::MediaValuesInitialViewport(LocalFrame& frame)
    : MediaValuesDynamic(&frame) {}

double MediaValuesInitialViewport::viewportWidth() const {
  DCHECK(m_frame->view());
  return m_frame->view()->initialViewportWidth();
}

double MediaValuesInitialViewport::viewportHeight() const {
  DCHECK(m_frame->view());
  return m_frame->view()->initialViewportHeight();
}

}  // namespace blink
