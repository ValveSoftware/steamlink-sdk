// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_macros_h
#define ng_macros_h

#include "base/macros.h"

namespace blink {

// Macro that can be used to annotate cases where it's OK to ignore writing
// mode. This is because changing the writing mode establishes a new formatting
// context and can be ignored while accessing some properties that are only
// stored in physical constraint space. Example: MarginStrut.
#ifndef WRITING_MODE_IGNORED
#define WRITING_MODE_IGNORED(message)
#endif

}  // namespace blink

#endif  // ng_macros_h
