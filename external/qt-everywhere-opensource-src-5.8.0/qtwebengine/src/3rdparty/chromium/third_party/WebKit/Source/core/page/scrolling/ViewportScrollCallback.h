// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewportScrollCallback_h
#define ViewportScrollCallback_h

#include "core/page/scrolling/ScrollStateCallback.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class FloatSize;
class FrameHost;
class Element;
class ScrollableArea;
class ScrollState;
class TopControls;
class OverscrollController;

class ViewportScrollCallback : public ScrollStateCallback {
public:
    // ViewportScrollCallback does not assume ownership of TopControls or of
    // OverscrollController.
    ViewportScrollCallback(TopControls*, OverscrollController*);
    ~ViewportScrollCallback();

    void handleEvent(ScrollState*) override;

    void setScroller(ScrollableArea*);

    DECLARE_VIRTUAL_TRACE();

private:
    bool shouldScrollTopControls(const FloatSize&, ScrollGranularity) const;

    WeakMember<TopControls> m_topControls;
    WeakMember<OverscrollController> m_overscrollController;
    WeakMember<ScrollableArea> m_scroller;
};

} // namespace blink

#endif // ViewportScrollCallback_h
