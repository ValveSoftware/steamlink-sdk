// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreePrinter_h
#define PaintPropertyTreePrinter_h

#ifndef NDEBUG
namespace blink {

class FrameView;

void showTransformPropertyTree(const FrameView& rootFrame);
void showClipPropertyTree(const FrameView& rootFrame);
void showEffectPropertyTree(const FrameView& rootFrame);

} // namespace blink
#endif

#endif // PaintPropertyTreePrinter_h
