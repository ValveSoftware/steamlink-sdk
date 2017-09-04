// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxReflectionUtils_h
#define BoxReflectionUtils_h

namespace blink {

class BoxReflection;
class ComputedStyle;
class PaintLayer;

// Utilities for manipulating box reflections in terms of core concepts, like
// PaintLayer.
BoxReflection boxReflectionForPaintLayer(const PaintLayer&,
                                         const ComputedStyle&);

}  // namespace blink

#endif  // BoxReflectionUtils_h
