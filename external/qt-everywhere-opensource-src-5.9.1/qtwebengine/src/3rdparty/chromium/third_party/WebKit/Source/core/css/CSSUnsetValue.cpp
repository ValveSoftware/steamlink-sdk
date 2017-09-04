// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSUnsetValue.h"

#include "core/css/CSSValuePool.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSUnsetValue* CSSUnsetValue::create() {
  return cssValuePool().unsetValue();
}

String CSSUnsetValue::customCSSText() const {
  return "unset";
}

}  // namespace blink
