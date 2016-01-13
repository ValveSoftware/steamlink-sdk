// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_TEXT_STORE_DELEGATE_H_
#define WIN8_METRO_DRIVER_IME_TEXT_STORE_DELEGATE_H_

#include <vector>

#include <windows.h>

#include "base/basictypes.h"
#include "base/strings/string16.h"

namespace metro_viewer {
struct UnderlineInfo;
}

namespace metro_driver {

// A delegate which works together with virtual text stores.
// Objects that implement this delegate will receive notifications from a
// virtual text store whenever an IME updates the composition or commits text.
// Objects that implement this delegate are also responsible for calculating
// the character position of composition and caret position upon request.
class TextStoreDelegate {
 public:
  virtual ~TextStoreDelegate() {}

  // Called when on-going composition is updated. An empty |text| represents
  // that the composition is canceled.
  virtual void OnCompositionChanged(
      const base::string16& text,
      int32 selection_start,
      int32 selection_end,
      const std::vector<metro_viewer::UnderlineInfo>& underlines) = 0;

  // Called when |text| is committed.
  virtual void OnTextCommitted(const base::string16& text) = 0;

  // Called when an IME requests the caret position. Objects that implement
  // this method must return the caret position in screen coordinates.
  virtual RECT GetCaretBounds() = 0;

  // Called when an IME requests the bounding box of an character whose
  // index is |index| in the on-going composition. position. Objects that
  // implement this method must return true and fill the character bounds into
  // |rect| in screen coordinates.
  // Should return false if |index| is invalid.
  virtual bool GetCompositionCharacterBounds(uint32 index, RECT* rect) = 0;
};

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_TEXT_STORE_DELEGATE_H_
