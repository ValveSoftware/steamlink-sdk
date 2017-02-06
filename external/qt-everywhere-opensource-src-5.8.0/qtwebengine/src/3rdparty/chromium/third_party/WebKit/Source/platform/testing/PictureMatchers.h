// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PictureMatchers_h
#define PictureMatchers_h

#include "platform/graphics/Color.h"
#include "testing/gmock/include/gmock/gmock.h"

class SkPicture;

namespace blink {

class FloatRect;

// Matches if the picture draws exactly one rectangle, which (after accounting
// for the total transformation matrix) matches the rect provided, and whose
// paint has the color requested.
::testing::Matcher<const SkPicture&> drawsRectangle(const FloatRect&, Color);

} // namespace blink

#endif // PictureMatchers_h
