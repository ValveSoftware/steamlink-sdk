// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BrowserControls_h
#define BrowserControls_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebBrowserControlsState.h"

namespace blink {
class FrameHost;
class FloatSize;

// This class encapsulate data and logic required to show/hide browser controls
// duplicating cc::BrowserControlsManager behaviour.  Browser controls'
// self-animation to completion is still handled by compositor and kicks in
// when scrolling is complete (i.e, upon ScrollEnd or FlingEnd).
class CORE_EXPORT BrowserControls final
    : public GarbageCollected<BrowserControls> {
 public:
  static BrowserControls* create(const FrameHost& host) {
    return new BrowserControls(host);
  }

  DECLARE_TRACE();

  // The amount that the viewport was shrunk by to accommodate the top
  // controls.
  float layoutHeight();
  // The amount that browser controls are currently shown.
  float contentOffset();

  float height() const { return m_height; }
  bool shrinkViewport() const { return m_shrinkViewport; }
  void setHeight(float height, bool shrinkViewport);

  float shownRatio() const { return m_shownRatio; }
  void setShownRatio(float);

  void updateConstraintsAndState(WebBrowserControlsState constraints,
                                 WebBrowserControlsState current,
                                 bool animate);

  void scrollBegin();

  // Scrolls browser controls vertically if possible and returns the remaining
  // scroll amount.
  FloatSize scrollBy(FloatSize scrollDelta);

  WebBrowserControlsState permittedState() const { return m_permittedState; }

 private:
  explicit BrowserControls(const FrameHost&);
  void resetBaseline();

  Member<const FrameHost> m_frameHost;

  // The browser controls height regardless of whether it is visible or not.
  float m_height;

  // The browser controls shown amount (normalized from 0 to 1) since the last
  // compositor commit. This value is updated from two sources:
  // (1) compositor (impl) thread at the beginning of frame if it has
  //     scrolled browser controls since last commit.
  // (2) blink (main) thread updates this value if it scrolls browser controls
  //     when responding to gesture scroll events.
  // This value is reflected in web layer tree and is synced with compositor
  // during the commit.
  float m_shownRatio;

  // Content offset when last re-baseline occurred.
  float m_baselineContentOffset;

  // Accumulated scroll delta since last re-baseline.
  float m_accumulatedScrollDelta;

  // If this is true, then the embedder shrunk the WebView size by the top
  // controls height.
  bool m_shrinkViewport;

  // Constraints on the browser controls state
  WebBrowserControlsState m_permittedState;
};
}  // namespace blink

#endif  // BrowserControls_h
