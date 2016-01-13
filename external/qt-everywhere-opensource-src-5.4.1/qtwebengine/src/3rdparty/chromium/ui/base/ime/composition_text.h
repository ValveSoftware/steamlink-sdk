// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_COMPOSITION_TEXT_H_
#define UI_BASE_IME_COMPOSITION_TEXT_H_

#include "base/strings/string16.h"
#include "ui/base/ime/composition_underline.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/range/range.h"

namespace ui {

// A struct represents the status of an ongoing composition text.
struct UI_BASE_EXPORT CompositionText {
  CompositionText();
  ~CompositionText();

  bool operator==(const CompositionText& rhs) const {
    if ((this->text != rhs.text) ||
        (this->selection != rhs.selection) ||
        (this->underlines.size() != rhs.underlines.size()))
      return false;
    for (size_t i = 0; i < this->underlines.size(); ++i) {
      if (this->underlines[i] != rhs.underlines[i])
        return false;
    }
    return true;
  }

  bool operator!=(const CompositionText& rhs) const {
    return !(*this == rhs);
  }

  void Clear();

  // Content of the composition text.
  base::string16 text;

  // Underline information of the composition text.
  // They must be sorted in ascending order by their start_offset and cannot be
  // overlapped with each other.
  CompositionUnderlines underlines;

  // Selection range in the composition text. It represents the caret position
  // if the range length is zero. Usually it's used for representing the target
  // clause (on Windows). Gtk doesn't have such concept, so background color is
  // usually used instead.
  gfx::Range selection;
};

}  // namespace ui

#endif  // UI_BASE_IME_COMPOSITION_TEXT_H_
