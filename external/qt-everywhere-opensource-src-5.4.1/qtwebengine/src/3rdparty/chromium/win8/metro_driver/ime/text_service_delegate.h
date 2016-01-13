// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_TEXT_SERVICE_DELEGATE_H_
#define WIN8_METRO_DRIVER_IME_TEXT_SERVICE_DELEGATE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"

namespace metro_viewer {
struct UnderlineInfo;
}

namespace metro_driver {

// A delegate which works together with virtual text service.
// Objects that implement this delegate will receive notifications from a
// virtual text service whenever an IME updates the composition or commits text.
class TextServiceDelegate {
 public:
  virtual ~TextServiceDelegate() {}

  // Called when on-going composition is updated. An empty |text| represents
  // that the composition is canceled.
  virtual void OnCompositionChanged(
      const base::string16& text,
      int32 selection_start,
      int32 selection_end,
      const std::vector<metro_viewer::UnderlineInfo>& underlines) = 0;

  // Called when |text| is committed.
  virtual void OnTextCommitted(const base::string16& text) = 0;
};

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_TEXT_SERVICE_DELEGATE_H_
