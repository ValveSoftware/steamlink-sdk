// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_UTILS_H_
#define PRINTING_PRINTING_UTILS_H_

#include "base/strings/string16.h"
#include "printing/printing_export.h"

namespace printing {

// Simplify title to resolve issue with some drivers.
PRINTING_EXPORT base::string16 SimplifyDocumentTitle(
    const base::string16& title);

}  // namespace printing

#endif  // PRINTING_PRINTING_UTILS_H_
