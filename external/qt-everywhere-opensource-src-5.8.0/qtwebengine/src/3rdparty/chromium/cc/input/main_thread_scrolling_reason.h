// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
#define CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

namespace cc {

// Ensure this stays in sync with MainThreadScrollingReason in histograms.xml.
// When adding a new MainThreadScrollingReason, make sure the corresponding
// [MainThread/Compositor]CanSetScrollReasons function is also updated.
struct MainThreadScrollingReason {
  // Non-transient scrolling reasons.
  enum : uint32_t { kNotScrollingOnMain = 0 };
  enum : uint32_t { kHasBackgroundAttachmentFixedObjects = 1 << 0 };
  enum : uint32_t { kHasNonLayerViewportConstrainedObjects = 1 << 1 };
  enum : uint32_t { kThreadedScrollingDisabled = 1 << 2 };
  enum : uint32_t { kScrollbarScrolling = 1 << 3 };
  enum : uint32_t { kPageOverlay = 1 << 4 };
  enum : uint32_t { kAnimatingScrollOnMainThread = 1 << 13 };
  enum : uint32_t { kHasStickyPositionObjects = 1 << 14 };
  enum : uint32_t { kCustomScrollbarScrolling = 1 << 15 };

  // Transient scrolling reasons. These are computed for each scroll begin.
  enum : uint32_t { kNonFastScrollableRegion = 1 << 5 };
  enum : uint32_t { kEventHandlers = 1 << 6 };
  enum : uint32_t { kFailedHitTest = 1 << 7 };
  enum : uint32_t { kNoScrollingLayer = 1 << 8 };
  enum : uint32_t { kNotScrollable = 1 << 9 };
  enum : uint32_t { kContinuingMainThreadScroll = 1 << 10 };
  enum : uint32_t { kNonInvertibleTransform = 1 << 11 };
  enum : uint32_t { kPageBasedScrolling = 1 << 12 };

  // The number of flags in this struct (excluding itself).
  enum : uint32_t { kMainThreadScrollingReasonCount = 17 };

  // Returns true if the given MainThreadScrollingReason can be set by the main
  // thread.
  static bool MainThreadCanSetScrollReasons(uint32_t reasons) {
    uint32_t reasons_set_by_main_thread =
        kNotScrollingOnMain | kHasBackgroundAttachmentFixedObjects |
        kHasNonLayerViewportConstrainedObjects | kThreadedScrollingDisabled |
        kScrollbarScrolling | kPageOverlay | kAnimatingScrollOnMainThread |
        kHasStickyPositionObjects | kCustomScrollbarScrolling;
    return (reasons & reasons_set_by_main_thread) == reasons;
  }

  // Returns true if the given MainThreadScrollingReason can be set by the
  // compositor.
  static bool CompositorCanSetScrollReasons(uint32_t reasons) {
    uint32_t reasons_set_by_compositor =
        kNonFastScrollableRegion | kEventHandlers | kFailedHitTest |
        kNoScrollingLayer | kNotScrollable | kContinuingMainThreadScroll |
        kNonInvertibleTransform | kPageBasedScrolling;
    return (reasons & reasons_set_by_compositor) == reasons;
  }
};

}  // namespace cc

#endif  // CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
