// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollEnums_h
#define ScrollEnums_h

namespace blink {

enum OverlayScrollbarClipBehavior {
    IgnoreOverlayScrollbarSize,
    ExcludeOverlayScrollbarSizeForHitTesting
};

enum ScrollOffsetClamping {
    ScrollOffsetUnclamped,
    ScrollOffsetClamped
};

} // namespace blink

#endif // ScrollEnums_h
