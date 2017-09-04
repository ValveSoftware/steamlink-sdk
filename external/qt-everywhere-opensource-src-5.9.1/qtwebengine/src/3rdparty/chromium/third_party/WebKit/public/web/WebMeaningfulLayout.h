// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMeaningfulLayout_h
#define WebMeaningfulLayout_h

namespace blink {

enum WebMeaningfulLayout {
  // Signifies that one of the following things were involved during the layout:
  // * > 200 text characters
  // * > 1024 image pixels
  // * a plugin
  // * a canvas
  // An approximation for first layout that resulted in pixels on screen.
  // Not the best heuristic, and we should replace it with something better.
  VisuallyNonEmpty,
  // First layout of a frame immediately after the parsing finished.
  FinishedParsing,
  // First layout of a frame immediately after the loading finished.
  FinishedLoading
};
}

#endif
