// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_utils.h"

#include <algorithm>

#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/text_elider.h"

namespace {
const int kMaxDocumentTitleLength = 50;
}

namespace printing {

base::string16 SimplifyDocumentTitle(const base::string16& title) {
  base::string16 no_controls(title);
  no_controls.erase(
      std::remove_if(no_controls.begin(), no_controls.end(), &u_iscntrl),
      no_controls.end());
  base::string16 result;
  gfx::ElideString(no_controls, kMaxDocumentTitleLength, &result);
  return result;
}

}  // namespace printing
