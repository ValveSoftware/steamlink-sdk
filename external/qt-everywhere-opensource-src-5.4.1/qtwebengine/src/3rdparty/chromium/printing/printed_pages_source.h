// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTED_PAGES_SOURCE_H_
#define PRINTING_PRINTED_PAGES_SOURCE_H_

#include "base/strings/string16.h"

namespace printing {

// Source of printed pages.
class PrintedPagesSource {
 public:
  // Returns the document title.
  virtual base::string16 RenderSourceName() = 0;

 protected:
  virtual ~PrintedPagesSource() {}
};

}  // namespace printing

#endif  // PRINTING_PRINTED_PAGES_SOURCE_H_
