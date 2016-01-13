// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/composition_text.h"

namespace ui {

CompositionText::CompositionText() {
}

CompositionText::~CompositionText() {
}

void CompositionText::Clear() {
  text.clear();
  underlines.clear();
  selection = gfx::Range();
}

}  // namespace ui
