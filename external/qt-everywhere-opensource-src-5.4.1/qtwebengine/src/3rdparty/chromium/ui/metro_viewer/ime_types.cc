// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/metro_viewer/ime_types.h"

namespace metro_viewer {

UnderlineInfo::UnderlineInfo()
    : start_offset(0),
      end_offset(0),
      thick(false) {
}

Composition::Composition()
    : selection_start(0),
      selection_end(0) {
}

Composition::~Composition() {
}

CharacterBounds::CharacterBounds()
    : left(0),
      top(0),
      right(0),
      bottom(0) {
}

}  // namespace metro_viewer
