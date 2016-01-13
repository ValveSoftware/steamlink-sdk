// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_METRO_VIEWER_IME_TYPES_H_
#define UI_METRO_VIEWER_IME_TYPES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"

namespace metro_viewer {

// An equivalent to ui::CompositionUnderline defined in
// "ui/base/ime/composition_underline.h". Redefined here to avoid dependency
// on ui.gyp from metro_driver.gyp.
struct UnderlineInfo {
  UnderlineInfo();
  int32 start_offset;
  int32 end_offset;
  bool thick;
};

// An equivalent to ui::CompositionText defined in
// "ui/base/ime/composition_text.h". Redefined here to avoid dependency
// on ui.gyp from metro_driver.gyp.
struct Composition {
  Composition();
  ~Composition();
  base::string16 text;
  int32 selection_start;
  int32 selection_end;
  std::vector<UnderlineInfo> underlines;
};

// An equivalent to Win32 RECT structure. This can be gfx::Rect but redefined
// here to avoid dependency on gfx.gyp from metro_driver.gyp.
struct CharacterBounds {
  CharacterBounds();
  int32 left;
  int32 top;
  int32 right;
  int32 bottom;
};

}  // namespace metro_viewer

#endif  // UI_METRO_VIEWER_IME_TYPES_H_
