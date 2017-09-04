// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSStyleDeclaration.h"

#include "core/css/CSSRule.h"

namespace blink {

DEFINE_TRACE_WRAPPERS(CSSStyleDeclaration) {
  visitor->traceWrappers(parentRule());
}

}  // namespace blink
