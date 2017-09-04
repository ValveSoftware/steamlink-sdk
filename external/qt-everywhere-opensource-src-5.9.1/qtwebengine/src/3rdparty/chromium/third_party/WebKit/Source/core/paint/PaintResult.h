// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintResult_h
#define PaintResult_h

namespace blink {

// Used as the return type of some paint methods.
enum PaintResult {
  // The layer/object is fully painted. This includes cases that nothing needs
  // painting regardless of the paint rect.
  FullyPainted,
  // Some part of the layer/object is out of the paint rect and may be not fully
  // painted.  The results cannot be cached because they may change when paint
  // rect changes.
  MayBeClippedByPaintDirtyRect,

  MaxPaintResult = MayBeClippedByPaintDirtyRect,
};

}  // namespace blink

#endif  // PaintResult.h
